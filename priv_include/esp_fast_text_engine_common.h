#ifndef ESP_FAST_TEXT_ENGINE_COMMON_H
#define ESP_FAST_TEXT_ENGINE_COMMON_H

#include "stdint.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// The log tag of esp_fast_text_engine.
static const char* ESP_FAST_TEXT_ENGINE_TAG = "esp_fast_text_engine";

/**
 * @brief				Private function of baking a glyph of given codepoint from the tightly-packed 1bpp LSbit first font
 *						glyph data to a given atlas slot.
 * @param context		The text engine instance to bake the glyph.
 * @param atlas_slot	The atlas slot to bake the glyph into.
 * @param codepoint		The codepoint of the glyph to bake into the atlas.
 * @return				The baking result.
 */
void private_bake_atlas_from_1bpp(
	const	esp_fast_text_engine_instance_t*	context,
			esp_fast_text_engine_atlas_slot_t*	atlas_slot,
			uint16_t							codepoint
);

/**
 * @brief				Private function of looking up a glyph in the cache of the given text engine or load it to the cache
 *						if it is not in the cache.
 * @param context		The text engine to look up or load the glyph.
 * @param codepoint		The codepoint of the glyph to look up or load.
 * @param pixel_buffer	The handle to receive the pixel buffer of the found or loaded glyph of given codepoint.
 * @param glyph_size_x	The handle to receive the width in pixels of the found or loaded glyph of given
 *						codepoint.
 * @return				The RGB565 MSB first bitmask buffer of the glyph of the given codepoint.
 */
void private_lookup_or_load_glyph(
	const	esp_fast_text_engine_instance_t*	context,
			uint16_t							codepoint,
			uint16_t**							pixel_buffer,
			uint32_t*							glyph_size_x
);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ESP_FAST_TEXT_ENGINE_COMMON_H
