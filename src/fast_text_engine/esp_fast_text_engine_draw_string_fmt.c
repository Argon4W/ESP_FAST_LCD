#include "string.h"
#include "stdarg.h"
#include "esp_check.h"
#include "esp_log.h"
#include "utf8.h"
#include "esp_fast_text_engine.h"
#include "esp_fast_text_engine_common.h"

esp_err_t esp_fast_text_engine_draw_string_fmt(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
	const	int32_t								position_x,
	const	int32_t								position_y,
	const	uint32_t							color_rgba8888,
			char*								string,
			...
) {
	// We cannot proceed without contexts.
	ESP_RETURN_ON_FALSE(text_engine_context		!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_text_engine_instance_t handle provided when performing drawing a translucent formatted string.");
	ESP_RETURN_ON_FALSE(panel_device_context	!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_lcd_panel_device_t handle provided when performing drawing a translucent formatted string.");

	// Set up the variable length arguments.
	va_list args;
	va_start(args, string);

	// Format the string.
	vsnprintf(
		/* buf = */ text_engine_context->string_buffer,
		/* len = */ text_engine_context->properties->configuration.string_buffer_size,
		/* fmt = */ string,
		/* arg = */ args
	);

	// Clean up the variable length arguments.
	va_end(args);

	// Draw the formatted string.
	ESP_RETURN_ON_ERROR(esp_fast_text_engine_draw_string(
		/* text_engine_context	= */ text_engine_context,
		/* panel_device_context	= */ panel_device_context,
		/* position_x			= */ position_x,
		/* position_y			= */ position_y,
		/* color_rgba8888		= */ color_rgba8888,
		/* string				= */ text_engine_context->string_buffer
	), ESP_FAST_TEXT_ENGINE_TAG, "Failed to draw the formatted string.");

	return ESP_OK;
}

esp_err_t esp_fast_text_engine_draw_native_string_fmt(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
	const	int32_t								position_x,
	const	int32_t								position_y,
	const	uint16_t							color_rgb565,
			char*								string,
			...
) {
	// We cannot proceed without contexts.
	ESP_RETURN_ON_FALSE(text_engine_context		!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_text_engine_instance_t handle provided when performing drawing an opaque native formatted string.");
	ESP_RETURN_ON_FALSE(panel_device_context	!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_lcd_panel_device_t handle provided when performing drawing an opaque native formatted string.");

	// Set up the variable length arguments.
	va_list args;
	va_start(args, string);

	// Format the string.
	vsnprintf(
		/* buf = */ text_engine_context->string_buffer,
		/* len = */ text_engine_context->properties->configuration.string_buffer_size,
		/* fmt = */ string,
		/* arg = */ args
	);

	// Clean up the variable length arguments.
	va_end(args);

	// Draw the formatted string.
	ESP_RETURN_ON_ERROR(esp_fast_text_engine_draw_native_string(
		/* text_engine_context	= */ text_engine_context,
		/* panel_device_context	= */ panel_device_context,
		/* position_x			= */ position_x,
		/* position_y			= */ position_y,
		/* color_rgb565			= */ color_rgb565,
		/* string				= */ text_engine_context->string_buffer
	), ESP_FAST_TEXT_ENGINE_TAG, "Failed to draw the formatted string.");

	return ESP_OK;
}

esp_err_t esp_fast_text_engine_draw_outlined_string_fmt(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
	const	int32_t								position_x,
	const	int32_t								position_y,
	const	uint32_t							color_rgba8888,
	const	uint32_t							color_outline_rgba8888,
			char*								string,
			...
) {
	// We cannot proceed without contexts.
	ESP_RETURN_ON_FALSE(text_engine_context		!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_text_engine_instance_t handle provided when performing drawing a translucent formatted outlined string.");
	ESP_RETURN_ON_FALSE(panel_device_context	!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_lcd_panel_device_t handle provided when performing drawing a translucent formatted outlined string.");

	// Set up the variable length arguments.
	va_list args;
	va_start(args, string);

	// Format the string.
	vsnprintf(
		/* buf = */ text_engine_context->string_buffer,
		/* len = */ text_engine_context->properties->configuration.string_buffer_size,
		/* fmt = */ string,
		/* arg = */ args
	);

	// Clean up the variable length arguments.
	va_end(args);

	// Draw the formatted string.
	ESP_RETURN_ON_ERROR(esp_fast_text_engine_draw_outlined_string(
		/* text_engine_context		= */ text_engine_context,
		/* panel_device_context		= */ panel_device_context,
		/* position_x				= */ position_x,
		/* position_y				= */ position_y,
		/* color_rgba8888			= */ color_rgba8888,
		/* color_outline_rgba8888	= */ color_outline_rgba8888,
		/* string					= */ text_engine_context->string_buffer
	), ESP_FAST_TEXT_ENGINE_TAG, "Failed to draw the formatted string.");

	return ESP_OK;
}

esp_err_t esp_fast_text_engine_draw_native_outlined_string_fmt(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
	const	int32_t								position_x,
	const	int32_t								position_y,
	const	uint16_t							color_rgb565,
	const	uint16_t							color_outline_rgb565,
			char*								string,
			...
) {
	// We cannot proceed without contexts.
	ESP_RETURN_ON_FALSE(text_engine_context		!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_text_engine_instance_t handle provided when performing drawing an opaque native formatted outlined string.");
	ESP_RETURN_ON_FALSE(panel_device_context	!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_TEXT_ENGINE_TAG, "No esp_fast_lcd_panel_device_t handle provided when performing drawing an opaque native formatted outlined string.");

	// Set up the variable length arguments.
	va_list args;
	va_start(args, string);

	// Format the string.
	vsnprintf(
		/* buf = */ text_engine_context->string_buffer,
		/* len = */ text_engine_context->properties->configuration.string_buffer_size,
		/* fmt = */ string,
		/* arg = */ args
	);

	// Clean up the variable length arguments.
	va_end(args);

	// Draw the formatted string.
	ESP_RETURN_ON_ERROR(esp_fast_text_engine_draw_native_outlined_string(
		/* text_engine_context	= */ text_engine_context,
		/* panel_device_context	= */ panel_device_context,
		/* position_x			= */ position_x,
		/* position_y			= */ position_y,
		/* color_rgb565			= */ color_rgb565,
		/* color_outline_rgb565	= */ color_outline_rgb565,
		/* string				= */ text_engine_context->string_buffer
	), ESP_FAST_TEXT_ENGINE_TAG, "Failed to draw the formatted string.");

	return ESP_OK;
}