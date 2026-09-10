#include "string.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_fast_lcd.h"
#include "esp_fast_lcd_common.h"
#include "esp_fast_lcd_common_commit.h"

esp_err_t esp_fast_lcd_new_lcd_panel_device(
			esp_fast_lcd_panel_device_t**		panel_device_ret,
			esp_fast_lcd_panel_configuration_t	panel_device_configuration,
			esp_lcd_panel_handle_t				panel_handle,
			esp_lcd_panel_io_handle_t			panel_io
) {
	esp_err_t ret = ESP_OK;

	// We cannot proceed without a handle that receives the created panel device.
	ESP_RETURN_ON_FALSE(panel_device_ret != NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No esp_fast_lcd_panel_device_t handle provided to receive the result when creating LCD panel device.");

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "Reserving handles for panel device handle \"%s\".", panel_device_configuration.name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Reserve handles for the LCD panel device.
	esp_fast_lcd_panel_device_t*			panel_device						= NULL;
	esp_fast_lcd_panel_transfer_queue_t*	panel_transfer_queue				= NULL;
	esp_fast_lcd_panel_properties_t*		panel_properties					= NULL;
	esp_fast_lcd_panel_transmit_t*			transfer_queue_pending_transmits	= NULL;
	atomic_uint*							transfer_queue_ring_transmits		= NULL;
	uint16_t*								transfer_queue_ring_buffer			= NULL;
	uint16_t*								transfer_queue_framebuffer			= NULL;
	uint32_t*								transfer_queue_dirty_tiles			= NULL;
	SemaphoreHandle_t						transfer_queue_free_buffer_count	= NULL;
	SemaphoreHandle_t						transfer_queue_ring_buffer_lock		= NULL;

	// Get the properties from the configuration of the panel device.
	const uint32_t	frame_size_x			= panel_device_configuration.frame_size_x;
	const uint32_t	frame_size_y			= panel_device_configuration.frame_size_y;
	const uint32_t	frame_tile_size_x		= panel_device_configuration.frame_tile_size_x;
	const uint32_t	frame_tile_size_y		= panel_device_configuration.frame_tile_size_y;
	const uint32_t	ring_buffer_slot_count	= panel_device_configuration.ring_buffer_slot_count;
	const uint32_t	buffer_flags			= panel_device_configuration.buffer_flags;
	const char*		name					= panel_device_configuration.name;

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "Creating LCD panel device handle \"%s.\"", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "Calculating properties for LCD panel device handle \"%s.\"", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Calculate the tile count.
	const uint32_t frame_size			= frame_size_x * frame_size_y;
	const uint32_t frame_tile_count_x	= frame_size_x / frame_tile_size_x;
	const uint32_t frame_tile_count_y	= frame_size_y / frame_tile_size_y;
	const uint32_t frame_tile_count		= frame_tile_count_x * frame_tile_count_y;

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "Validating properties for LCD panel device handle \"%s.\"", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Check the alignment requirements.
	ESP_GOTO_ON_FALSE(frame_size	% 2					== 0,	ESP_ERR_INVALID_ARG, error, ESP_FAST_LCD_TAG, "frame_size_x * frame_size_y of the configuration must be even.");
	ESP_GOTO_ON_FALSE(frame_size_x	% frame_tile_size_x	== 0,	ESP_ERR_INVALID_ARG, error, ESP_FAST_LCD_TAG, "frame_size_x of the configuration must be a multiple of frame_tile_size_x.");
	ESP_GOTO_ON_FALSE(frame_size_y	% frame_tile_size_y	== 0,	ESP_ERR_INVALID_ARG, error, ESP_FAST_LCD_TAG, "frame_size_y of the configuration must be a multiple of frame_tile_size_y.");
	ESP_GOTO_ON_FALSE(frame_tile_count_x				<= 30,	ESP_ERR_INVALID_ARG, error, ESP_FAST_LCD_TAG, "frame_tile_count_x of the configuration must be less than or equal to 30.");
	ESP_GOTO_ON_FALSE(frame_tile_count_y				<= 30,	ESP_ERR_INVALID_ARG, error, ESP_FAST_LCD_TAG, "frame_tile_count_y of the configuration must be less than or equal to 30.");

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "Allocating handles for LCD panel device handle \"%s.\"", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Create the counting semaphore of the available ring buffer slots.
	transfer_queue_free_buffer_count	= xSemaphoreCreateCounting	(ring_buffer_slot_count, ring_buffer_slot_count);
	transfer_queue_ring_buffer_lock		= xSemaphoreCreateBinary	();

	// Allocate the handles of the panel device.
	panel_device			= calloc(1, sizeof(esp_fast_lcd_panel_device_t));
	panel_properties		= calloc(1, sizeof(esp_fast_lcd_panel_properties_t));
	panel_transfer_queue	= calloc(1, sizeof(esp_fast_lcd_panel_transfer_queue_t));

	// Create the handles of the transfer queue.
	transfer_queue_pending_transmits	= calloc			(frame_tile_count,						sizeof(esp_fast_lcd_panel_transmit_t));
	transfer_queue_ring_transmits		= calloc			(ring_buffer_slot_count,				sizeof(atomic_uint));
	transfer_queue_dirty_tiles			= calloc			(frame_tile_count_y,					sizeof(uint32_t));
	transfer_queue_ring_buffer			= heap_caps_calloc	(frame_size * ring_buffer_slot_count,	sizeof(uint16_t), DMA_CAPS | buffer_flags);
	transfer_queue_framebuffer			= heap_caps_calloc	(frame_size,							sizeof(uint16_t), DMA_CAPS | buffer_flags);

	// Check the allocations.
	ESP_GOTO_ON_FALSE(transfer_queue_free_buffer_count	!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_LCD_TAG, "Failed to create free buffer counter of the ring buffer for LCD panel device \"%s\".",	name);
	ESP_GOTO_ON_FALSE(transfer_queue_ring_buffer_lock	!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_LCD_TAG, "Failed to create lock of the ring buffer for LCD panel device \"%s\".",					name);
	ESP_GOTO_ON_FALSE(panel_device						!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_LCD_TAG, "Failed to create panel device struct for LCD panel device \"%s\".",						name);
	ESP_GOTO_ON_FALSE(panel_properties					!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_LCD_TAG, "Failed to create properties for LCD panel device \"%s\".",								name);
	ESP_GOTO_ON_FALSE(panel_transfer_queue				!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_LCD_TAG, "Failed to create transfer queue for LCD panel device \"%s\".",							name);
	ESP_GOTO_ON_FALSE(transfer_queue_pending_transmits	!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_LCD_TAG, "Failed to create pending transmission array for LCD panel device \"%s\".",				name);
	ESP_GOTO_ON_FALSE(transfer_queue_ring_transmits		!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_LCD_TAG, "Failed to create in-flight transmission array for LCD panel device \"%s\".",				name);
	ESP_GOTO_ON_FALSE(transfer_queue_dirty_tiles		!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_LCD_TAG, "Failed to create dirty tiles bitmap for LCD panel device \"%s\".",						name);
	ESP_GOTO_ON_FALSE(transfer_queue_ring_buffer		!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_LCD_TAG, "Failed to create ring buffer for LCD panel device \"%s\".",								name);
	ESP_GOTO_ON_FALSE(transfer_queue_framebuffer		!= NULL, ESP_ERR_NO_MEM, error, ESP_FAST_LCD_TAG, "Failed to create framebuffer for LCD panel device \"%s\".",								name);

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "Initializing locks for LCD panel device handle \"%s.\"", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Give the lock back by default.
	xSemaphoreGive(transfer_queue_ring_buffer_lock);

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "Registering transfer-done callback for LCD panel device handle \"%s.\"", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Initialize the transfer-done callback configuration for the esp_lcd panel IO handle.
	const esp_lcd_panel_io_callbacks_t esp_lcd_panel_io_callback = {
		.on_color_trans_done = private_on_commit_done,
	};

	// Register the transfer-done callback to the panel IO handle.
	ESP_GOTO_ON_ERROR(esp_lcd_panel_io_register_event_callbacks(
		/* io		= */ panel_io,
		/* cbs		= */ &esp_lcd_panel_io_callback,
		/* user_ctx	= */ panel_device
	), error, ESP_FAST_LCD_TAG, "Failed to register SPI callbacks of LCD panel device \"%s\".", name);

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "Finalizing LCD panel device handle \"%s.\"", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Fill the transfer queue of the LCD panel device handle with the allocated handles.
	panel_transfer_queue->free_buffer_count			= transfer_queue_free_buffer_count;
	panel_transfer_queue->ring_buffer_lock			= transfer_queue_ring_buffer_lock;
	panel_transfer_queue->pending_transmits			= transfer_queue_pending_transmits;
	panel_transfer_queue->ring_transmits			= transfer_queue_ring_transmits;
	panel_transfer_queue->ring_buffer				= transfer_queue_ring_buffer;
	panel_transfer_queue->framebuffer				= transfer_queue_framebuffer;
	panel_transfer_queue->dirty_tiles				= transfer_queue_dirty_tiles;
	panel_transfer_queue->pending_transmit_count	= 0U;
	panel_transfer_queue->transmit_index			= 0U;
	panel_transfer_queue->ring_index				= 0U;
	panel_transfer_queue->frame_dirty				= false;

	// Fill the properties of the LCD panel device handle with the calculated properties.
	panel_properties->configuration			= panel_device_configuration;
	panel_properties->frame_size			= frame_size;
	panel_properties->frame_tile_count_x	= frame_tile_count_x;
	panel_properties->frame_tile_count_y	= frame_tile_count_y;
	panel_properties->frame_tile_count		= frame_tile_count;

	// Fill LCD panel device handle with the transfer queue, properties and esp_lcd panel handle.
	panel_device->transfer_queue	= panel_transfer_queue;
	panel_device->properties		= panel_properties;
	panel_device->handle			= panel_handle;

	// Return the created panel device handle.
	*panel_device_ret = panel_device;

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "LCD panel device handle \"%s\" has been created.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	return ret;

	// Resource cleanup when error occurred.
	error:

	// Log the error if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "Error occurred while creating LCD panel device \"%s\": %s", name, esp_err_to_name(ret));
		ESP_LOGD(ESP_FAST_LCD_TAG, "Cleaning up resources.");
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	if (transfer_queue_free_buffer_count)	vSemaphoreDelete(transfer_queue_free_buffer_count);	// Cleanup the free buffer count counting semaphore.
	if (transfer_queue_ring_buffer_lock)	vSemaphoreDelete(transfer_queue_ring_buffer_lock);	// Cleanup the ring buffer lock binary semaphore.
	if (transfer_queue_framebuffer)			free			(transfer_queue_framebuffer);		// Cleanup the framebuffer.
	if (transfer_queue_ring_buffer)			free			(transfer_queue_ring_buffer);		// Cleanup the ring buffer.
	if (transfer_queue_dirty_tiles)			free			(transfer_queue_dirty_tiles);		// Cleanup the dirty tile bitmap.
	if (transfer_queue_ring_transmits)		free			(transfer_queue_ring_transmits);	// Cleanup the in-flight transmission array.
	if (transfer_queue_pending_transmits)	free			(transfer_queue_pending_transmits);	// Cleanup the pending transmission array.
	if (panel_transfer_queue)				free			(panel_transfer_queue);				// Cleanup the transfer queue.
	if (panel_properties)					free			(panel_properties);					// Cleanup the properties.
	if (panel_device)						free			(panel_device);						// Cleanup the panel device struct.

	return ret;
}

esp_err_t esp_fast_lcd_del_lcd_panel_device(esp_fast_lcd_panel_device_t* panel_device_in) {
	// We cannot proceed without a panel device.
	ESP_RETURN_ON_FALSE(panel_device_in != NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No esp_fast_lcd_panel_device_t handle provided when releasing lcd panel device.");

	// Get the name of the LCD panel device to release.
	const char* name = panel_device_in->properties->configuration.name;

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "Releasing panel LCD device \"%s\".", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Free all transfer queue related allocations.
	esp_fast_lcd_panel_transfer_queue_t* transfer_queue = panel_device_in->transfer_queue;

	// Free the semaphores of the queue.
	vSemaphoreDelete(transfer_queue->free_buffer_count);
	vSemaphoreDelete(transfer_queue->ring_buffer_lock);

	// Free all transfer queue related allocations and the queue itself.
	free(transfer_queue->pending_transmits);
	free(transfer_queue->ring_transmits);
	free(transfer_queue->ring_buffer);
	free(transfer_queue->framebuffer);
	free(transfer_queue->dirty_tiles);
	free(transfer_queue);

	// Free the properties of the LCD panel device.
	free(panel_device_in->properties);

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "Detaching all handles of LCD panel device \"%s\".", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Detaching all fields in LCD panel device handle.
	panel_device_in->transfer_queue	= NULL;
	panel_device_in->properties		= NULL;
	panel_device_in->handle			= NULL;

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "Release the panel device struct of LCD panel device \"%s\".", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Release the panel device struct.
	free(panel_device_in);

	// Log the progress if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "LCD panel device \"%s\" has been released.", name);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	return ESP_OK;
}