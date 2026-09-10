#include "string.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_fast_text_engine.h"
#include "esp_fast_text_engine_common.h"
#include "esp_fast_text_engine_common_atlas.h"

esp_err_t private_new_atlas(
			esp_fast_text_engine_atlas_t**	atlas_ret,
	const	char*							name,
			uint32_t						slot_count,
			uint32_t						slot_size,
			uint32_t						slot_buffer_flags,
			uint32_t						info_buffer_flags
) {
	esp_err_t ret = ESP_OK;

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Reserving handles for the %s atlas.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Reserve the handles for the atlas.
	esp_fast_text_engine_atlas_t*		atlas	= NULL;
	esp_fast_text_engine_atlas_slot_t*	slots	= NULL;
	uint16_t*							buffer	= NULL;

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Allocating handles for the %s atlas.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	atlas	= calloc			(1,							sizeof(esp_fast_text_engine_atlas_t));
	slots	= heap_caps_calloc	(slot_count,				sizeof(esp_fast_text_engine_atlas_slot_t),	info_buffer_flags);
	buffer	= heap_caps_calloc	(slot_count * slot_size,	sizeof(uint16_t),							slot_buffer_flags);

	// Check the allocations.
	ESP_GOTO_ON_FALSE(atlas		!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_TEXT_ENGINE_TAG, "Failed to create atlas struct for the %s atlas.",	name);
	ESP_GOTO_ON_FALSE(slots		!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_TEXT_ENGINE_TAG, "Failed to create slot info array for the %s atlas.",	name);
	ESP_GOTO_ON_FALSE(buffer	!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_TEXT_ENGINE_TAG, "Failed to create slot buffer for the %s atlas.",		name);

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Initializing the slots of the %s atlas.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Initialize all slots and bind their buffer pointers to the atlas buffer.
	for (uint32_t slot_index = 0U; slot_index < slot_count; slot_index ++) {
		slots[slot_index].buffer	= &buffer[slot_index * slot_size];
		slots[slot_index].size_x	= 0U;
		slots[slot_index].codepoint	= 0U;
	}

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Finalizing the %s atlas.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Fill the atlas struct.
	atlas->name			= name;
	atlas->slots		= slots;
	atlas->buffer		= buffer;
	atlas->slot_size	= slot_size;
	atlas->slot_count	= slot_count;

	// Return the allocated and initialized atlas.
	*atlas_ret = atlas;

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

	if (buffer)	free(buffer);	// Cleanup the slot buffer of the atlas.
	if (slots)	free(slots);	// Cleanup the slot info array of the atlas.
	if (atlas)	free(atlas);	// Cleanup the atlas struct of the atlas.

	return ret;
}

esp_err_t private_del_atlas(esp_fast_text_engine_atlas_t* atlas_in) {
	// We cannot proceed without an allocated atlas.
	ESP_RETURN_ON_FALSE(atlas_in != NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_text_engine_atlas_t handle provided when releasing atlas.");

	// Get the name of the LCD panel device to release.
	const char* name = atlas_in->name;

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Releasing the %s atlas.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Free the handles in the atlas.
	free(atlas_in->slots);
	free(atlas_in->buffer);

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_TEXT_ENGINE_TAG, "Detaching all handles of %s atlas.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Detaching all fields in atlas.
	atlas_in->name			= NULL;
	atlas_in->slots			= NULL;
	atlas_in->buffer		= NULL;
	atlas_in->slot_size		= 0U;
	atlas_in->slot_count	= 0U;

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
