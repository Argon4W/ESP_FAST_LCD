#include "string.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_fast_text_engine.h"
#include "esp_fast_text_engine_common.h"
#include "esp_fast_text_engine_common_atlas.h"
#include "esp_fast_text_engine_common_lru_atlas.h"

void private_move_lru_node_to_head(
	esp_fast_text_engine_lru_atlas_t*	atlas,
	esp_fast_text_engine_lru_node_t*	node
) {
	// Skip if the node is already the head node.
	if (node == atlas->head) {
		return;
	}

	if (node == atlas->tail) {
		// Update the tail node pointer.
		atlas->tail = node->prev;

		// Detach the current node from previous node.
		node->prev->next	= NULL;
		node->prev			= NULL;
	} else {
		// Attach the previous node to the next node.
		node->prev->next = node->next;
		node->next->prev = node->prev;

		// Detach current node from the previous node and next node.
		node->prev = NULL;
		node->next = NULL;
	}

	// The old head node of the linked list.
	esp_fast_text_engine_lru_node_t *head = atlas->head;

	// Attach current node to the head of the linked list.
	node->next = head;
	head->prev = node;

	// Update the head node pointer.
	atlas->head = node;
}

void private_move_lru_node_to_tail(
	esp_fast_text_engine_lru_atlas_t*	atlas,
	esp_fast_text_engine_lru_node_t*	node
) {
	// Skip if the node is already the tail node.
	if (node == atlas->tail) {
		return;
	}

	if (node == atlas->head) {
		// Update the head node pointer.
		atlas->head = node->next;

		// Detach the current node from next node.
		node->next->prev	= NULL;
		node->next			= NULL;
	} else {
		// Attach the previous node to the next node.
		node->prev->next = node->next;
		node->next->prev = node->prev;

		// Detach current node from the previous node and next node.
		node->prev = NULL;
		node->next = NULL;
	}

	// The old tail node of the linked list.
	esp_fast_text_engine_lru_node_t *tail = atlas->tail;

	// Append current node to the tail of the linked list.
	node->prev = tail;
	tail->next = node;

	// Update the tail node pointer.
	atlas->tail = node;
}

esp_err_t private_new_lru_atlas(
			esp_fast_text_engine_lru_atlas_t**	atlas_ret,
	const	char*								name,
	const	uint32_t							slot_count,
	const	uint32_t							slot_size,
	const	uint32_t							slot_buffer_flags,
	const	uint32_t							info_buffer_flags
) {
	// No atlas_ret check here, every call to private_new_lru_atlas is controlled by us.
	esp_err_t ret = ESP_OK;

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Reserving handles for the %s atlas.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Reserve the handles for the atlas.
	esp_fast_text_engine_lru_atlas_t*	lru		= NULL;
	esp_fast_text_engine_lru_node_t*	nodes	= NULL;
	esp_fast_text_engine_atlas_t*		atlas	= NULL;

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Allocating handles for the %s atlas.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Allocate the handles.
	lru		= calloc(1,				sizeof(esp_fast_text_engine_lru_atlas_t));
	nodes	= calloc(slot_count,	sizeof(esp_fast_text_engine_lru_node_t));

	// Check the allocations.
	ESP_GOTO_ON_FALSE(lru	!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_TEXT_ENGINE_TAG, "Failed to create atlas struct for the %s atlas.",	name);
	ESP_GOTO_ON_FALSE(nodes	!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_TEXT_ENGINE_TAG, "Failed to create LRU linked list for the %s atlas.",	name);

	// Create the atlas.
	ESP_GOTO_ON_ERROR(private_new_atlas(
		/* atlas_ret			= */ &atlas,
		/* name					= */ name,
		/* slot_count			= */ slot_count,
		/* slot_size			= */ slot_size,
		/* slot_buffer_flags	= */ slot_buffer_flags,
		/* info_buffer_flags	= */ info_buffer_flags
	), error, ESP_FAST_TEXT_ENGINE_TAG, "Failed to create the internal atlas for the %s atlas.", name);

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Initializing all nodes in the LRU linked list of the %s atlas.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	const uint32_t head_index = 0U;
	const uint32_t tail_index = slot_count - 1U;

	// Initialize all nodes.
	for (uint32_t slot_index = 0; slot_index < slot_count; slot_index ++) {
		// Get the node of the slot.
		esp_fast_text_engine_lru_node_t* node = &nodes[slot_index];

		// Link the node to the linked list.
		node->prev = (slot_index != head_index) ? &nodes[slot_index - 1U] : NULL;
		node->next = (slot_index != tail_index) ? &nodes[slot_index + 1U] : NULL;
		node->slot = &atlas->slots[slot_index];

		// Set all slots of the internal atlas to empty.
		atlas->slots[slot_index].codepoint	= EMPTY_CODEPOINT_SENTINEL;
		atlas->slots[slot_index].size_x		= EMPTY_CODEPOINT_SENTINEL;
	}

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Finalizing the %s atlas.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Fill the atlas struct.
	lru->head	= &nodes[0];
	lru->tail	= &nodes[slot_count - 1];
	lru->nodes	= nodes;
	lru->atlas	= atlas;

	// Return the allocated and initialized LRU atlas.
	*atlas_ret = lru;

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "%s atlas has been created.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	return ret;

	// Resource cleanup when error occurred.
	error:

	// Log the error if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Error occurred while creating %s atlas: %s", name, esp_err_to_name(ret));
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Cleaning up resources.");
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	if (atlas)	private_del_atlas	(atlas);	// Cleanup the internal baked glyph atlas of the LRU atlas.
	if (nodes)	free				(nodes);	// Cleanup the LRU node array of the LRU atlas.
	if (lru)	free				(lru);		// Cleanup the atlas struct of the LRU atlas.

	return ret;
}

esp_err_t private_del_lru_atlas(esp_fast_text_engine_lru_atlas_t* atlas_in) {
	// We cannot proceed without an allocated atlas.
	ESP_RETURN_ON_FALSE(atlas_in != NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_text_engine_lru_atlas_t handle provided when releasing LRU atlas.");

	// Get the name of the LCD panel device to release.
	const char* name = atlas_in->atlas->name;

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Releasing the %s atlas.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Free the handles.
	free				(atlas_in->nodes);
	private_del_atlas	(atlas_in->atlas);

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Detaching all handles of %s atlas.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Detaching all fields in atlas.
	atlas_in->head	= NULL;
	atlas_in->tail	= NULL;
	atlas_in->nodes	= NULL;
	atlas_in->atlas	= NULL;

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Releasing the atlas struct of %s atlas.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Release the atlas struct.
	free(atlas_in);

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "%s atlas has been released.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	return ESP_OK;
}