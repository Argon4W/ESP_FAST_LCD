#include "string.h"
#include "esp_fast_text_engine.h"
#include "esp_fast_text_engine_common.h"
#include "esp_fast_text_engine_common_atlas.h"
#include "esp_fast_text_engine_common_lru_atlas.h"

void private_lookup_or_load_glyph(
	const	esp_fast_text_engine_instance_t*	context,
	const	uint16_t							codepoint,
			uint16_t**							glyph_buffer,
			uint32_t*							glyph_size_x
) {
	// Extract necessary properties for glyph lookup.
	const uint32_t atlas_slot_size = context->properties->atlas_slot_size;

	// ASCII characters are resident in memory.
	if (codepoint < 128U) {
		// Get the slot from the resident ASCII atlas.
		const esp_fast_text_engine_atlas_slot_t* slot = &context->ascii_atlas->slots[codepoint];

		// Return the glyph buffer and the width of the glyph from the slot of ASCII resident atlas.
		*glyph_buffer = slot->buffer;
		*glyph_size_x =	slot->size_x;

		return;
	}

	// Lookup the glyph in the L1 IRAM LRU atlas.
	esp_fast_text_engine_lru_atlas_t* l1_lru_atlas = context->iram_lru_atlas;

	for (uint32_t slot_index = 0U; slot_index < l1_lru_atlas->atlas->slot_count; slot_index ++) {
		// Get the node of the corresponding
		esp_fast_text_engine_lru_node_t* l1_node = &l1_lru_atlas->nodes[slot_index];

		if (l1_node->slot->codepoint == codepoint) {
			// Move the node to the head to reset its used state to recently used.
			private_move_lru_node_to_head(l1_lru_atlas, l1_node);

			// Return the glyph buffer and the width of the glyph from the node of L1 IRAM LRU atlas.
			*glyph_buffer = l1_node->slot->buffer;
			*glyph_size_x = l1_node->slot->size_x;

			return;
		}
	}

	// Lookup the glyph in the L2 PSRAM LRU atlas.
	esp_fast_text_engine_lru_atlas_t* l2_lru_atlas = context->psram_lru_atlas;

	for (uint32_t l2_slot_index = 0U; l2_slot_index < l2_lru_atlas->atlas->slot_count; l2_slot_index ++) {
		esp_fast_text_engine_lru_node_t* l2_node = &l2_lru_atlas->nodes[l2_slot_index];

		if (l2_node->slot->codepoint == codepoint) {
			// Find the empty or least used L1 node to replace, it should always be the tail node.
			esp_fast_text_engine_lru_node_t* l1_node = l1_lru_atlas->tail;

			// Get the atlas slot of the L1 and L2 nodes.
			esp_fast_text_engine_atlas_slot_t* l1_slot  = l1_node->slot;
			esp_fast_text_engine_atlas_slot_t* l2_slot  = l2_node->slot;

			// Get the buffer pointer of both L1 and L2 slots, get the width in L2 slot.
					uint16_t*	dst_buffer = l1_slot->buffer;
					uint16_t*	src_buffer = l2_slot->buffer;
			const	uint32_t	src_size_x = l2_slot->size_x;

			// Swap the content if the selected L1 node is not empty.
			if (l1_node->slot->codepoint != EMPTY_CODEPOINT_SENTINEL) {
				uint16_t* swp_buffer = context->swap_buffer;

				// Swap the data of the two nodes. (L1 <=> L2)
				memcpy(swp_buffer, dst_buffer, atlas_slot_size * 2U); // S = L1;
				memcpy(dst_buffer, src_buffer, atlas_slot_size * 2U); // L1 = L2;
				memcpy(src_buffer, swp_buffer, atlas_slot_size * 2U); // L2 = S;

				// Get the original infos of the L1 slot.
				const uint32_t l1_codepoint	= l1_slot->codepoint;
				const uint32_t l1_size_x	= l1_slot->size_x;

				// Swap the codepoints.
				l1_slot->codepoint = l2_slot->codepoint;
				l2_slot->codepoint = l1_codepoint;

				// Swap the widths
				l1_slot->size_x = l2_slot->size_x;
				l2_slot->size_x = l1_size_x;

				// Both nodes are not empty (not sentinel value), no need to set empty bitsets.
				// Move both nodes to the head of their linked list.
				private_move_lru_node_to_head(l1_lru_atlas, l1_node);
				private_move_lru_node_to_head(l2_lru_atlas, l2_node);
			} else {
				// Copy the baked glyph to the L1 slot buffer.
				memcpy(
					/* dst_buffer	= */ dst_buffer,
					/* src_buffer	= */ src_buffer,
					/* length		= */ atlas_slot_size * 2U
				);

				// Copy the infos to L1 slot.
				l1_slot->codepoint	= l2_slot->codepoint;
				l1_slot->size_x		= l2_slot->size_x;

				// L2 slot is empty now, set to empty sentinel value.
				l1_slot->codepoint	= EMPTY_CODEPOINT_SENTINEL;
				l1_slot->size_x		= EMPTY_CODEPOINT_SENTINEL;

				// Move the used L1 node to the head of its linked list, move the empty L2 node to the tail of its linled list..
				private_move_lru_node_to_head(l1_lru_atlas, l1_node);
				private_move_lru_node_to_tail(l2_lru_atlas, l2_node);
			}

			// Return the glyph buffer and the size x of the glyph from the hoisted node in the L1 IRAM LRU atlas.
			*glyph_buffer = dst_buffer;
			*glyph_size_x = src_size_x;

			return;
		}
	}

	// Bake from tightly packed 1bpp LSBit first font glyph data.
	esp_fast_text_engine_lru_node_t*	l1_lru_node = l1_lru_atlas	->tail;
	esp_fast_text_engine_atlas_slot_t*	l1_lru_slot = l1_lru_node	->slot;

	// Move the original baked glyph of the L1 node to an empty or least used L2 node.
	if (l1_lru_node->slot->codepoint != EMPTY_CODEPOINT_SENTINEL) {
		// Find the empty or least used L2 node to replace, it should always be the tail node.
		esp_fast_text_engine_lru_node_t*	l2_lru_node = l2_lru_atlas	->tail;
		esp_fast_text_engine_atlas_slot_t*	l2_lru_slot = l2_lru_node	->slot;

		// Move the baked bitmask data to the L2 node.
		memcpy(
			/* dst_buffer	= */ l2_lru_slot->buffer,
			/* src_buffer	= */ l1_lru_slot->buffer,
			/* length		= */ atlas_slot_size * 2U
		);

		// Update the metadata of the L2 node.
		l2_lru_slot->codepoint	= l1_lru_slot->codepoint;
		l2_lru_slot->size_x		= l1_lru_slot->size_x;

		// Move the L2 node to the head of its linked list.
		private_move_lru_node_to_head(l2_lru_atlas, l2_lru_node);
	}

	// Bake bitmask for the glyph of given codepoint.
	private_bake_atlas_from_1bpp(
		/* context		= */ context,
		/* atlas_slot	= */ l1_lru_slot,
		/* codepoint	= */ codepoint
	);

	// Move the L1 node to the head of its linked list.
	private_move_lru_node_to_head(l1_lru_atlas, l1_lru_node);

	// Return the glyph buffer and the size x of the glyph from the baked L1 node.
	*glyph_buffer = l1_lru_slot->buffer;
	*glyph_size_x = l1_lru_slot->size_x;
}