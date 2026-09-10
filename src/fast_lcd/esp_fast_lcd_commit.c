#include "esp_check.h"
#include "esp_log.h"
#include "string.h"
#include "esp_lcd_panel_ops.h"
#include "esp_fast_lcd.h"
#include "esp_fast_lcd_common.h"
#include "esp_fast_lcd_common_commit.h"

esp_err_t esp_fast_lcd_commit(const esp_fast_lcd_panel_device_t* context) {
	esp_err_t ret = ESP_OK;

	// We cannot proceed without context.
	if (context == NULL) {
		// Log the error if LCD panel debug logging is enabled.
		#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
			ESP_LOGE(ESP_FAST_LCD_TAG, "NO esp_fast_lcd_panel_device_t handle provided when performing a commit.");
		#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		return ESP_ERR_INVALID_ARG;
	}

	// Get the transfer queue and properties from the LCD panel device context.
	const	esp_fast_lcd_panel_properties_t*		properties		= context->properties;
			esp_fast_lcd_panel_transfer_queue_t*	transfer_queue	= context->transfer_queue;

	// Skip if the frame has no changes.
	if (transfer_queue->frame_dirty) {
		transfer_queue->frame_dirty = false;

		// Prefetch all necessary handles from the transfer queue.
				uint32_t*			dirtyTiles	= transfer_queue->dirty_tiles;
		const	uint16_t*			framebuffer	= transfer_queue->framebuffer;
		const	SemaphoreHandle_t	freeBuffer	= transfer_queue->free_buffer;

		// Get the detailed properties of the device for calculating clipped range of the rectangle and dirty tiles range.
		const uint32_t frame_size_x				= properties->configuration.frame_size_x;
		const uint32_t frame_tile_size_x		= properties->configuration.frame_tile_size_x;
		const uint32_t frame_tile_size_y		= properties->configuration.frame_tile_size_y;
		const uint32_t ring_buffer_slot_count	= properties->configuration.ring_buffer_slot_count;
		const uint32_t frame_size				= properties->frame_size;
		const uint32_t frame_tile_count_y		= properties->frame_tile_count_y;

		// Merge all dirty tiles into batches to prevent esp_lcd overhead.
		for (uint32_t tile_y = 0U; tile_y < frame_tile_count_y; tile_y ++) {
			// The bitset is a line of tiles.
			uint32_t bitset = dirtyTiles[tile_y];
			uint32_t offset = 0U;

			// Iterate until no ones in the bitset.
			while (bitset != 0) {
				// Count the trailing zeros of the bitset.
				const uint32_t zeros = ctz(bitset);

				offset +=	zeros; // The x of the batch rectangle should skip the zeros.
				bitset >>=	zeros; // Remove zeros from the bitset.

				// The length of ones is the width of the rectangle.
				const uint32_t size_x = ctz(~bitset);

				// Count the height of the batch rectangle by testing following lines with the mask.
				const uint32_t mask = ((1U << size_x) - 1U) << offset;

				// Clear the mask of the current line.
				dirtyTiles[tile_y] &= ~mask;

				// Reserve height for the batch.
				uint32_t size_y	= 1U;

				// Check if following lines can be batched.
				for (uint32_t tile_y_2 = tile_y + 1U; tile_y_2 < frame_tile_count_y; tile_y_2 ++) {
					if ((dirtyTiles[tile_y_2] & mask) == mask) {
						// Update the height, remove the batchable ones from the line to avoid duplicated batch.
						dirtyTiles[tile_y_2] &= ~mask;
						size_y ++;
					} else {
						// Stop when the pattern ends.
						break;
					}
				}

				// Now we have a biggest(?) rectangle of tiles that can be batched in one esp_lcd_panel_draw_bitmap.
				// The width of the rectangle is sizeX, the height of the rectangle is sizeY.
				// The start X in tiles of the rectangle is offset. The start Y in tiles of the rectangle is tileY.
				const uint32_t batch_start_position_x	= frame_tile_size_x * offset;
				const uint32_t batch_start_position_y	= frame_tile_size_y * tile_y;
				const uint32_t batch_size_x				= frame_tile_size_x * size_x;
				const uint32_t batch_size_y				= frame_tile_size_y * size_y;

				// Wait for the next available ring buffer slot.
				xSemaphoreTake(freeBuffer, portMAX_DELAY);

				// Now we can take the ring buffer slot safely.
				uint16_t* dst_offset = &transfer_queue->ring_buffer[((transfer_queue->ring_index ++) % ring_buffer_slot_count) * frame_size];

				// Get the start offset of the framebuffer.
				const uint16_t* src_offset = &framebuffer[
					/* index_y	= */ batch_start_position_y * frame_size_x +
					/* index_x	= */ batch_start_position_x
				];

				// Copy lines of the framebuffer in the range of batch to the ring buffer slots.
				for (uint32_t y = 0U; y < batch_size_y; y ++) {
					memcpy(
						/* dst_buffer	= */ &dst_offset[y * batch_size_x],
						/* src_buffer	= */ &src_offset[y * frame_size_x],
						/* length		= */ batch_size_x * 2U
					);
				}

				// Commit the buffer to the esp_lcd SPI IO.
				ESP_GOTO_ON_ERROR(esp_lcd_panel_draw_bitmap(
					/* panel		= */ context->handle,
					/* x_start		= */ batch_start_position_x,
					/* y_start		= */ batch_start_position_y,
					/* x_end		= */ batch_start_position_x + batch_size_x,
					/* y_end		= */ batch_start_position_y + batch_size_y,
					/* color_data	= */ dst_offset
				), error, ESP_FAST_LCD_TAG, "Failed to commit batch of positionX=%" PRIu32 ", positionY=%" PRIu32 ", sizeX=%" PRIu32 ", sizeY=%" PRIu32 " to the LCD panel.",
					/* PRIu32 */ batch_start_position_x,
					/* PRIu32 */ batch_start_position_y,
					/* PRIu32 */ batch_size_x,
					/* PRIu32 */ batch_size_y
				);

				offset +=	size_x; // The x of the next batch rectangle should skip the ones of the current rectangle.
				bitset >>=	size_x; // Remove the ones of the current bitset.
			}
		}
	}

	return ret;

	// Resource cleanup when error occurred.
	error:

	// Release the buffer slot by giving the counting semaphore.
	xSemaphoreGive(transfer_queue->free_buffer);

	return ret;
}

IRAM_ATTR bool private_on_commit_done(
	esp_lcd_panel_io_handle_t		panel_io_handle,
	esp_lcd_panel_io_event_data_t*	panel_io_event,
	void*							user_handle
) {
	// Cast the user context to the LCD device context.
	const esp_fast_lcd_panel_device_t* context = (esp_fast_lcd_panel_device_t*) user_handle;

	// Check if there is a higher priority task.
	BaseType_t higher_priority_task_woken = pdFALSE;

	// Increment the counting semaphore from ISR.
	xSemaphoreGiveFromISR(context->transfer_queue->free_buffer, &higher_priority_task_woken);

	// Tell the driver if it should yield from ISR.
	return higher_priority_task_woken == pdTRUE;
}