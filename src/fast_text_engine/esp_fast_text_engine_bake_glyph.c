#include "esp_fast_text_engine.h"
#include "esp_fast_text_engine_common.h"

/**
 * @brief					Internal function of baking a glyph into the destination buffer.
 * @param buffer			The buffer to bake the glyph into.
 * @param glyph_data		The data of the glyph to be baked into the buffer.
 * @param glyph_size_x		The width of the baked glyph in pixels.
 * @param glyph_size_y		The height of the baked glyph in pixels.
 * @param buffer_size_x		The width of the buffer in pixels.
 * @param unscaled_size_x	The unscaled width of the glyph in 1bpp data in bits.
 * @param font_size_scale	The size scale of the font.
 * @param offset_x			The offset in position X in the destination buffer when baking.
 * @param offset_y			The offset in position Y in the destination buffer when baking.
 */
static void internal_bake_glyph(
			uint16_t*	buffer,
	const	uint8_t*	glyph_data,
			uint32_t	glyph_size_x,
			uint32_t	glyph_size_y,
			uint32_t	buffer_size_x,
			uint32_t	unscaled_size_x,
			uint32_t	font_size_scale,
			uint32_t	offset_x,
			uint32_t	offset_y
);

void private_bake_atlas_from_1bpp(
	const	esp_fast_text_engine_instance_t*	context,
			esp_fast_text_engine_atlas_slot_t*	atlas_slot,
			uint16_t							codepoint
) {
	// Get the properties, configuration, and font of the text engine instance.
	const esp_fast_text_engine_instance_properties_t*		properties	= context		->properties;
	const esp_fast_text_engine_instance_configuration_t*	config		= &properties	->configuration;
	const esp_fast_text_engine_font_t*						font		= &properties	->font;

	// Extract necessary parameters for glyph baking.
	const uint32_t atlas_glyph_size		= properties->atlas_glyph_size;
	const uint32_t atlas_glyph_size_x	= properties->atlas_glyph_size_x;
	const uint32_t atlas_glyph_size_y	= properties->atlas_glyph_size_y;
	const uint32_t font_outline_radius	= config	->font_outline_radius;
	const uint32_t font_size_scale		= config	->font_size_scale;

	// Get the lookup index of the codepoint to bake.
	const uint32_t block = (codepoint >> 8U) & 0xFFU;
	const uint32_t index = (codepoint >> 0U) & 0xFFU;

	// Get the glyph data and the glyph info of the codepoint to bake.
	const uint8_t*		glyph_data = font->glyph_data;
	const glyph_info_t	glyph_info = font->glyph_table[block][index];

	// Get the properties of the glyph to bake.
	const uint32_t offset = glyph_info.offset;
	const uint32_t size_x = glyph_info.size_x;

	// Get the buffer of the 1bpp data of the glyph.
	const uint8_t* glyph_buffer = &glyph_data[offset];

	// Get the buffer pointer to bake the glyph into.
	uint16_t* slot_buffer_regular = &atlas_slot->buffer[0];
	uint16_t* slot_buffer_outline = &atlas_slot->buffer[atlas_glyph_size]; // Just obsessive, should be okay.

	// Update the slot info.
	atlas_slot->codepoint	= codepoint;
	atlas_slot->size_x		= size_x * font_size_scale;

	// Bake the regular glyph.
	internal_bake_glyph(
		/* buffer			= */ slot_buffer_regular,
		/* glyph_data		= */ glyph_buffer,
		/* glyph_size_x		= */ atlas_glyph_size_x,
		/* glyph_size_y		= */ atlas_glyph_size_y,
		/* buffer_size_x	= */ atlas_glyph_size_x,
		/* unscaled_size_x	= */ size_x,
		/* font_size_scale	= */ font_size_scale,
		/* offset_x			= */ 0,
		/* offset_y			= */ 0
	);

	// Skip the outline ring glyph baking if no outline presents.
	if (font_outline_radius == 0U) {
		return;
	}

	// Calculate the width of the outline buffer.
	const uint32_t outline_size_x = atlas_glyph_size_x + font_outline_radius * 2U;

	// Bake the outline ring glyph by drawing multiple times with different offsets.
	for		(uint32_t offset_y = 0U; offset_y <= font_outline_radius * 2U; offset_y ++) {
		for	(uint32_t offset_x = 0U; offset_x <= font_outline_radius * 2U; offset_x ++) {
			internal_bake_glyph(
				/* buffer			= */ slot_buffer_outline,
				/* glyph_data		= */ glyph_buffer,
				/* glyph_size_x		= */ atlas_glyph_size_x,
				/* glyph_size_y		= */ atlas_glyph_size_y,
				/* buffer_size_x	= */ outline_size_x,
				/* unscaled_size_x	= */ size_x,
				/* font_size_scale	= */ font_size_scale,
				/* offset_x			= */ offset_x,
				/* offset_y			= */ offset_y
			);
		}
	}
}

static void internal_bake_glyph(
			uint16_t*	buffer,
	const	uint8_t*	glyph_data,
	const	uint32_t	glyph_size_x,
	const	uint32_t	glyph_size_y,
	const	uint32_t	buffer_size_x,
	const	uint32_t	unscaled_size_x,
	const	uint32_t	font_size_scale,
	const	uint32_t	offset_x,
	const	uint32_t	offset_y
) {
	// Calculate bytes in a line of 1bpp glyph data.
	const uint32_t unscaled_bytes_x = (unscaled_size_x + 7U) / 8U;

	for		(uint32_t position_y = 0U; position_y < glyph_size_y; position_y ++) {
		for	(uint32_t position_x = 0U; position_x < glyph_size_x; position_x ++) {
			// Map the scaled pixel coordinate backed to unscaled 1bpp pixel coordinate.
			const uint32_t position_x_1bpp = position_x / font_size_scale;
			const uint32_t position_y_1bpp = position_y / font_size_scale;

			// All glyphs have the same height, but they have different widths.
			if (position_x_1bpp >= unscaled_size_x) {
				continue;
			}

			// Get the byte coordinate from the X of the unscaled 1bpp pixel coordinate.
			const uint32_t position_x_bits = position_x_1bpp % 8U;
			const uint32_t position_x_byte = position_x_1bpp / 8U;

			// Get the byte containing the 1bpp data of the unscaled pixel.
			const uint8_t pixel_byte = glyph_data[
				/* index_y	= */ position_y_1bpp * unscaled_bytes_x +
				/* index_x	= */ position_x_byte
			];

			// Get the final bitmask value from the pixel byte.
			const uint16_t bitmask = (pixel_byte & (1U << position_x_bits)) ? 0xFFFFU : 0x0000U;

			// Write the bitmask value to the atlas.
			// Use bitwise-or to avoid overwriting outline ring baking.
			buffer[
				/* index_y = */ (position_y + offset_y) * buffer_size_x +
				/* index_x = */ (position_x + offset_x)
			] |= bitmask;
		}
	}
}