#include "string.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_fast_text_engine.h"
#include "esp_fast_text_engine_common.h"
#include "esp_fast_text_engine_common_atlas.h"
#include "esp_fast_text_engine_common_lru_atlas.h"

esp_err_t esp_fast_text_engine_new_text_engine_instance(
	esp_fast_text_engine_instance_t**				engine_instance_ret,
	esp_fast_text_engine_instance_configuration_t	engine_instance_configuration,
	esp_fast_text_engine_font_t						engine_instance_font
) {
	esp_err_t ret = ESP_OK;

	// We cannot proceed without a handle that receives the created text engine.
	ESP_RETURN_ON_FALSE(engine_instance_ret != NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_text_engine_instance_t handle provided to receive the result when creating text engine instance.");

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Reserving handles for text engine instance \"%s\".", engine_instance_configuration.name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Reserve handles for text engine instance.

	esp_fast_text_engine_instance_t*			engine_instance			= NULL;
	esp_fast_text_engine_instance_properties_t*	engine_properties		= NULL;
	esp_fast_text_engine_lru_atlas_t*			engine_l1_atlas			= NULL;
	esp_fast_text_engine_lru_atlas_t*			engine_l2_atlas			= NULL;
	esp_fast_text_engine_atlas_t*				engine_ascii_atlas		= NULL;
	uint16_t*									engine_swap_buffer		= NULL;
	char*										engine_string_buffer	= NULL;

	// Get the properties from the font and configuration of the text engine instance.
	const uint32_t	font_max_size_x			= engine_instance_font			.size_x_max;
	const uint32_t	font_size_y				= engine_instance_font			.size_y;
	const uint32_t	font_size_multiplier	= engine_instance_configuration	.font_size_scale;
	const uint32_t	font_outline_radius		= engine_instance_configuration	.font_outline_radius;
	const uint32_t	iram_atlas_slot_count	= engine_instance_configuration	.iram_atlas_slot_count;
	const uint32_t	psram_atlas_slot_count	= engine_instance_configuration	.psram_atlas_slot_count;
	const uint32_t	string_buffer_size		= engine_instance_configuration	.string_buffer_size;
	const uint32_t	atlas_flags				= engine_instance_configuration	.atlas_flags;
	const char*		name					= engine_instance_configuration	.name;

	// Evaluate the font and configuration of the text engine instance.
	ESP_GOTO_ON_FALSE(font_max_size_x			> 0, ESP_ERR_INVALID_ARG, error, ESP_FAST_TEXT_ENGINE_TAG, "The size_x_max of the font must be greater than 0.");
	ESP_GOTO_ON_FALSE(font_size_y				> 0, ESP_ERR_INVALID_ARG, error, ESP_FAST_TEXT_ENGINE_TAG, "The size_y of the font must be greater than 0.");
	ESP_GOTO_ON_FALSE(font_size_multiplier		> 0, ESP_ERR_INVALID_ARG, error, ESP_FAST_TEXT_ENGINE_TAG, "The font_size_multiplier of the configuration must be greater than 0.");
	ESP_GOTO_ON_FALSE(iram_atlas_slot_count		> 0, ESP_ERR_INVALID_ARG, error, ESP_FAST_TEXT_ENGINE_TAG, "The iram_atlas_slot_count of the configuration must be greater than 0.");
	ESP_GOTO_ON_FALSE(psram_atlas_slot_count	> 0, ESP_ERR_INVALID_ARG, error, ESP_FAST_TEXT_ENGINE_TAG, "The psram_atlas_slot_count of the configuration must be greater than 0.");
	ESP_GOTO_ON_FALSE(string_buffer_size		> 0, ESP_ERR_INVALID_ARG, error, ESP_FAST_TEXT_ENGINE_TAG, "The string_buffer_size of the configuration must be greater than 0.");

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Calculating properties for text engine instance \"%s\".", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Calculate the properties of the atlases.
	const uint32_t atlas_glyph_size_x	= font_size_multiplier	* font_max_size_x;
	const uint32_t atlas_glyph_size_y	= font_size_multiplier	* font_size_y;
	const uint32_t atlas_glyph_size		= atlas_glyph_size_x	* atlas_glyph_size_y;

	// Calculate the atlas slot size in pixels.
	uint32_t atlas_slot_size;

	// Include the baked outline ring glyph in the atlas slot if the outline presents.
	if (font_outline_radius != 0U) {
		// Calculate the expanded outline ring size range.
		const uint32_t atlas_outline_size_x = atlas_glyph_size_x + font_outline_radius * 2U;
		const uint32_t atlas_outline_size_y = atlas_glyph_size_y + font_outline_radius * 2U;

		// Calculate the count of pixels of outline ring glyph.
		const uint32_t atlas_outline_size = atlas_outline_size_x	* atlas_outline_size_y;

		atlas_slot_size = atlas_glyph_size + atlas_outline_size;
	} else {
		// No outline, we only need the regular baked glyph in the slot.
		atlas_slot_size = atlas_glyph_size;
	}

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Allocating handles for text engine instance \"%s\".", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Allocate properties and atlases of the engine instance.
	engine_string_buffer	= calloc			(string_buffer_size,	sizeof(char));
	engine_instance			= calloc			(1U,					sizeof(esp_fast_text_engine_instance_t));
	engine_properties		= calloc			(1U,					sizeof(esp_fast_text_engine_instance_properties_t));
	engine_swap_buffer		= heap_caps_calloc	(atlas_slot_size,		sizeof(uint16_t), L2_ATLAS_CAPS);

	// Check the allocations.
	ESP_GOTO_ON_FALSE(engine_instance		!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_TEXT_ENGINE_TAG, "Failed to create instance struct for text engine instance \"%s\".",			name);
	ESP_GOTO_ON_FALSE(engine_properties		!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_TEXT_ENGINE_TAG, "Failed to create properties for text engine instance \"%s\".",				name);
	ESP_GOTO_ON_FALSE(engine_swap_buffer	!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_TEXT_ENGINE_TAG, "Failed to create swap pixel buffer for text engine instance \"%s\".",		name);
	ESP_GOTO_ON_FALSE(engine_swap_buffer	!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_TEXT_ENGINE_TAG, "Failed to create formatting string buffer for text engine instance \"%s\".",	name);

	// Allocate and initialize the resident and LRU atlases.
	ESP_GOTO_ON_ERROR(private_new_lru_atlas	(&engine_l1_atlas,		"IRAM LRU",			iram_atlas_slot_count,	atlas_slot_size, L1_LRU_CAPS, L1_ATLAS_CAPS | atlas_flags), error, ESP_FAST_TEXT_ENGINE_TAG, "Failed to initialize IRAM LRU atlas for text engine instance \"%s\".",		name);
	ESP_GOTO_ON_ERROR(private_new_lru_atlas	(&engine_l2_atlas,		"PSRAM LRU",		psram_atlas_slot_count,	atlas_slot_size, L2_LRU_CAPS, L2_ATLAS_CAPS | atlas_flags), error, ESP_FAST_TEXT_ENGINE_TAG, "Failed to initialize PSRAM LRU atlas for text engine instance \"%s\".",		name);
	ESP_GOTO_ON_ERROR(private_new_atlas		(&engine_ascii_atlas,	"ASCII Resident",	128,					atlas_slot_size, L1_LRU_CAPS, L2_ATLAS_CAPS | atlas_flags), error, ESP_FAST_TEXT_ENGINE_TAG, "Failed to initialize ASCII Resident atlas for text engine instance \"%s\".",	name);

	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Filling properties for text engine instance \"%s\".", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Fill the properties
	engine_properties->font					= engine_instance_font;
	engine_properties->configuration		= engine_instance_configuration;
	engine_properties->atlas_glyph_size_x	= atlas_glyph_size_x;
	engine_properties->atlas_glyph_size_y	= atlas_glyph_size_y;
	engine_properties->atlas_glyph_size		= atlas_glyph_size;
	engine_properties->atlas_slot_size		= atlas_slot_size;

	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Finalizing text engine instance \"%s\".", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Fill the instance struct.
	engine_instance->properties			= engine_properties;
	engine_instance->iram_lru_atlas		= engine_l1_atlas;
	engine_instance->psram_lru_atlas	= engine_l2_atlas;
	engine_instance->ascii_atlas		= engine_ascii_atlas;
	engine_instance->swap_buffer		= engine_swap_buffer;
	engine_instance->string_buffer		= engine_string_buffer;

	// Bake ASCII characters into the resident ASCII
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Baking resident ASCII characters atlas for text engine instance \"%s\".", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Bake all ASCII characters.
	for (uint16_t codepoint = 0U; codepoint < 128U; codepoint ++) {
		// Bake the glyph into the resident ASCII atlas.
		private_bake_atlas_from_1bpp(
			/* context		= */ engine_instance,
			/* atlas_slot	= */ &engine_ascii_atlas->slots[codepoint],
			/* codepoint	= */ codepoint
		);
	}

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Text engine instance \"%s\" has been created.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Return the created text engine instance.
	*engine_instance_ret = engine_instance;

	return ret;

	// Resource cleanup when error occurred.
	error:

	// Log the error if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Error occurred while creating text engine instance \"%s\": %s", name, esp_err_to_name(ret));
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Cleaning up resources.");
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	if (engine_l2_atlas)		private_del_lru_atlas	(engine_l2_atlas);		// Cleanup L2 PSRAM LRU atlas.
	if (engine_l1_atlas)		private_del_lru_atlas	(engine_l1_atlas);		// Cleanup L1 IRAM LRU atlas.
	if (engine_swap_buffer)		free					(engine_swap_buffer);	// Free the swap pixel buffer.
	if (engine_ascii_atlas)		free					(engine_ascii_atlas);	// Cleanup resident ASCII atlas.
	if (engine_properties)		free					(engine_properties);	// Cleanup the properties.
	if (engine_instance)		free					(engine_instance);		// Cleanup the instance struct.
	if (engine_string_buffer)	free					(engine_string_buffer);	// Free the formatting string buffer.

	return ret;
}

esp_err_t esp_fast_text_engine_del_text_engine_instance(esp_fast_text_engine_instance_t* engine_instance_in) {
	// We cannot proceed without an allocated text engine.
	ESP_RETURN_ON_FALSE(engine_instance_in != NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_text_engine_instance_t handle provided when releasing text engine instance.");

	// Get the name of the LCD panel device to release.
	const char* name = engine_instance_in->properties->configuration.name;

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Releasing text engine instance \"%s\".", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Releasing the direct allocations of the handles in the text engine instance.
	free(engine_instance_in->properties);
	free(engine_instance_in->swap_buffer);
	free(engine_instance_in->string_buffer);

	// Release the resident and LRU atlas allocations.
	private_del_lru_atlas	(engine_instance_in->iram_lru_atlas);
	private_del_lru_atlas	(engine_instance_in->psram_lru_atlas);
	private_del_atlas		(engine_instance_in->ascii_atlas);

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Detaching all handles of text engine instance \"%s\".", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Detach all fields in the text engine.
	engine_instance_in->properties		= NULL;
	engine_instance_in->iram_lru_atlas	= NULL;
	engine_instance_in->psram_lru_atlas	= NULL;
	engine_instance_in->ascii_atlas		= NULL;
	engine_instance_in->swap_buffer		= NULL;
	engine_instance_in->string_buffer	= NULL;

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Releasing the engine instance struct of text engine instance \"%s\".", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Release the instance struct.
	free(engine_instance_in);

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Text engine instance \"%s\" has been released.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	return ESP_OK;
}