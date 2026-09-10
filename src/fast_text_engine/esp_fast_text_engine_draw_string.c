#include "string.h"
#include "esp_check.h"
#include "esp_log.h"
#include "utf8.h"
#include "esp_fast_text_engine.h"
#include "esp_fast_text_engine_common.h"

esp_err_t esp_fast_text_engine_draw_native_string(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
	const	int32_t								position_x,
	const	int32_t								position_y,
	const	uint16_t							color_rgb565,
			char*								string
) {
	// We cannot proceed without contexts.
	ESP_RETURN_ON_FALSE(text_engine_context		!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_text_engine_instance_t handle provided when performing drawing an opaque native string.");
	ESP_RETURN_ON_FALSE(panel_device_context	!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_lcd_panel_device_t handle provided when performing drawing an opaque native string.");

	// Get the width and height of the glyph in pixels.
	const esp_fast_text_engine_instance_properties_t*	properties			= text_engine_context	->properties;
	const uint32_t										atlas_glyph_size_x	= properties			->atlas_glyph_size_x;
	const uint32_t										atlas_glyph_size_y	= properties			->atlas_glyph_size_y;
	const uint8_t										crlf_mode			= properties			->configuration.crlf_mode;

	// Handles for receiving the lookup or load result and UTF-8 decode result.
	uint16_t*	glyph_buffer	= NULL;
	uint32_t	glyph_size_x	= 0U;
	int32_t		glyph_codepoint	= 0U;
	int32_t		glyph_advance_x	= 0U;
	int32_t		glyph_advance_y	= 0U;

	// Log the operation if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		// Get all color components of rgba8888.
		const uint8_t r5_src = (uint8_t) ((color_rgb565 >> 11U)	& 0b011111U);
		const uint8_t g6_src = (uint8_t) ((color_rgb565 >> 5U)	& 0b111111U);
		const uint8_t b5_src = (uint8_t) ((color_rgb565 >> 0U)	& 0b011111U);

		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Text engine instance \"%s\" is performing an opaque native string draw at: positionX=%" PRId32 ", positionY=%" PRId32 ".",
			/* s		*/ text_engine_context->properties->configuration.name,
			/* PRId32	*/ position_x,
			/* PRId32	*/ position_y
		);
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Pixel color of the string: r=0x%02" PRIX8 ", g=0x%02" PRIX8 ", b=0x%02" PRIX8 ".",
			/* PRIX8 */ r5_src,
			/* PRIX8 */ g6_src,
			/* PRIX8 */ b5_src
		);
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Content of the string: %s", string);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Decode all codepoints in the string until it reaches the \0.
	while (*string != '\0') {
		// Get a codepoint from the string, then move the pointer.
		string = utf8codepoint(string, &glyph_codepoint);

		// Font only contains 0x0000U ~ 0xFFFFU, route to undefined character codepoint if it is out of range.
		if (glyph_codepoint > 0xFFFFU) {
			glyph_codepoint = 0xFFFDU;
		}

		// Reset the advance in position X to 0 if the codepoint is CR.
		if (glyph_codepoint == 0x000DU) {
			glyph_advance_x = 0U;

			// SKip drawing the CR.
			continue;
		}

		// Advance the position Y to next line if the codepoint is LF.
		if (glyph_codepoint == 0x000AU) {
			glyph_advance_y += (int32_t) atlas_glyph_size_y;

			// Only reset the advance in position X to 0 if the text engine is not in CRLF mode.
			if (!crlf_mode) {
				glyph_advance_x = 0U;
			}

			// Skip drawing the LF.
			continue;
		}

		// Lookup/Load the glyph of the decoded codepoint from the text engine instance.
		private_lookup_or_load_glyph(
			/* context		= */ text_engine_context,
			/* codepoint	= */ (uint16_t) glyph_codepoint,
			/* pixel_buffer	= */ &glyph_buffer,
			/* glyph_size_x	= */ &glyph_size_x
		);

		// Draw the glyph by drawing a colored filled rectangle with the bitmask of the glyph.
		ESP_RETURN_ON_ERROR(esp_fast_lcd_draw_native_rectangle_masked(
			/* context			= */ panel_device_context,
			/* position_x		= */ position_x + glyph_advance_x,
			/* position_y		= */ position_y + glyph_advance_y,
			/* size_x			= */ glyph_size_x,
			/* size_y			= */ atlas_glyph_size_y,
			/* bitmask_offset_x	= */ 0,
			/* bitmask_offset_y	= */ 0,
			/* bitmask_size_x	= */ atlas_glyph_size_x,
			/* bitmask_flipped	= */ true,
			/* color_rgb565		= */ color_rgb565,
			/* bitmask_rgb565	= */ glyph_buffer
		), ESP_FAST_TEXT_ENGINE_TAG, "Failed to draw the glyph of the string.");

		// Advance the position X for the draw of next glyph.
		glyph_advance_x += (int32_t) glyph_size_x;
	}

	return ESP_OK;
}

esp_err_t esp_fast_text_engine_draw_string(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
	const	int32_t								position_x,
	const	int32_t								position_y,
	const	uint32_t							color_rgba8888,
			char*								string
) {
	// We cannot proceed without contexts.
	ESP_RETURN_ON_FALSE(text_engine_context		!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_text_engine_instance_t handle provided when performing drawing a translucent string.");
	ESP_RETURN_ON_FALSE(panel_device_context	!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_lcd_panel_device_t handle provided when performing drawing a translucent string.");

	// Get the width and height of the glyph in pixels.
	const esp_fast_text_engine_instance_properties_t*	properties			= text_engine_context	->properties;
	const uint32_t										atlas_glyph_size_x	= properties			->atlas_glyph_size_x;
	const uint32_t										atlas_glyph_size_y	= properties			->atlas_glyph_size_y;
	const uint8_t										crlf_mode			= properties			->configuration.crlf_mode;

	// Handles for receiving the lookup or load result and UTF-8 decode result.
	uint16_t*	glyph_buffer	= NULL;
	uint32_t	glyph_size_x	= 0U;
	int32_t		glyph_codepoint	= 0U;
	int32_t		glyph_advance_x	= 0U;
	int32_t		glyph_advance_y	= 0U;

	// Log the operation if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		// Get all color components of rgba8888.
		const uint8_t r8_src = (uint8_t) ((color_rgba8888 >> 24U)	& 0xFFU);
		const uint8_t g8_src = (uint8_t) ((color_rgba8888 >> 16U)	& 0xFFU);
		const uint8_t b8_src = (uint8_t) ((color_rgba8888 >> 8U)	& 0xFFU);
		const uint8_t a8_src = (uint8_t) ((color_rgba8888 >> 0U)	& 0xFFU);

		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Text engine instance \"%s\" is performing an translucent string draw at: positionX=%" PRId32 ", positionY=%" PRId32 ".",
			/* s		*/ text_engine_context->properties->configuration.name,
			/* PRId32	*/ position_x,
			/* PRId32	*/ position_y
		);
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Pixel color of the string before the blending: r=0x%02" PRIX8 ", g=0x%02" PRIX8 ", b=0x%02" PRIX8 ", a=0x%02" PRIX8 ".",
			/* PRIX8 */ r8_src,
			/* PRIX8 */ g8_src,
			/* PRIX8 */ b8_src,
			/* PRIX8 */ a8_src
		);
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Content of the string: %s", string);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Decode all codepoints in the string until it reaches the \0.
	while (*string != '\0') {
		// Get a codepoint from the string, then move the pointer.
		string = utf8codepoint(string, &glyph_codepoint);

		// Font only contains 0x0000U ~ 0xFFFFU, route to undefined character codepoint if it is out of range.
		if (glyph_codepoint > 0xFFFFU) {
			glyph_codepoint = 0xFFFDU;
		}

		// Reset the advance in position X to 0 if the codepoint is CR.
		if (glyph_codepoint == 0x000DU) {
			glyph_advance_x = 0U;

			// SKip drawing the CR.
			continue;
		}

		// Advance the position Y to next line if the codepoint is LF.
		if (glyph_codepoint == 0x000AU) {
			glyph_advance_y += (int32_t) atlas_glyph_size_y;

			// Only reset the advance in position X to 0 if the text engine is not in CRLF mode.
			if (!crlf_mode) {
				glyph_advance_x = 0U;
			}

			// Skip drawing the LF.
			continue;
		}

		// Lookup/Load the glyph of the decoded codepoint from the text engine instance.
		private_lookup_or_load_glyph(
			/* context		= */ text_engine_context,
			/* codepoint	= */ (uint16_t) glyph_codepoint,
			/* pixel_buffer	= */ &glyph_buffer,
			/* glyph_size_x	= */ &glyph_size_x
		);

		// Draw the glyph by drawing a colored filled rectangle with the bitmask of the glyph.
		ESP_RETURN_ON_ERROR(esp_fast_lcd_draw_rectangle_masked(
			/* context			= */ panel_device_context,
			/* position_x		= */ position_x + glyph_advance_x,
			/* position_y		= */ position_y + glyph_advance_y,
			/* size_x			= */ glyph_size_x,
			/* size_y			= */ atlas_glyph_size_y,
			/* bitmask_offset_x	= */ 0,
			/* bitmask_offset_y	= */ 0,
			/* bitmask_size_x	= */ atlas_glyph_size_x,
			/* bitmask_flipped	= */ true,
			/* color_rgba8888	= */ color_rgba8888,
			/* bitmask_rgb565	= */ glyph_buffer
		), ESP_FAST_TEXT_ENGINE_TAG, "Failed to draw the glyph of the string.");

		// Advance the position X for the draw of next glyph.
		glyph_advance_x += (int32_t) glyph_size_x;
	}

	return ESP_OK;
}