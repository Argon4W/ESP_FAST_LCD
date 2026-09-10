#include "stdatomic.h"
#include "string.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_fast_lcd.h"
#include "esp_fast_lcd_common.h"
#include "esp_fast_lcd_common_commit.h"

esp_err_t esp_fast_lcd_commit(const esp_fast_lcd_panel_device_t* context) {
	// We cannot proceed without context.
	ESP_RETURN_ON_FALSE(context != NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No esp_fast_lcd_panel_device_t handle provided when performing a commit.");

	// Get the transfer queue and properties from the LCD panel device context.
	const	esp_fast_lcd_panel_properties_t*		properties		= context->properties;
			esp_fast_lcd_panel_transfer_queue_t*	transfer_queue	= context->transfer_queue;

	// Skip if the frame has no changes.
	if (transfer_queue->frame_dirty) {
		transfer_queue->frame_dirty = false; // Only one draw-commit task is allowed.

		// Prefetch all necessary handles from the transfer queue.
				esp_fast_lcd_panel_transmit_t*	pending_transmits	= transfer_queue->pending_transmits;
				uint32_t*						dirty_tiles			= transfer_queue->dirty_tiles;
				uint16_t*						ring_buffer			= transfer_queue->ring_buffer;
		const	uint16_t*						framebuffer			= transfer_queue->framebuffer;
		const	SemaphoreHandle_t				free_buffer_count	= transfer_queue->free_buffer_count;
		const	SemaphoreHandle_t				ring_buffer_lock	= transfer_queue->ring_buffer_lock;

		// Get the detailed properties of the device for calculating clipped range of the rectangle and dirty tiles range.
		const uint32_t frame_size_x				= properties->configuration.frame_size_x;
		const uint32_t frame_tile_size_x		= properties->configuration.frame_tile_size_x;
		const uint32_t frame_tile_size_y		= properties->configuration.frame_tile_size_y;
		const uint32_t ring_buffer_slot_count	= properties->configuration.ring_buffer_slot_count;
		const uint32_t frame_size				= properties->frame_size;
		const uint32_t frame_tile_count_y		= properties->frame_tile_count_y;

		// Acquire the lock, enter critical.
		xSemaphoreTake(ring_buffer_lock, portMAX_DELAY);

		// Wait until there is at least one slot (the current slot) available.
		xQueuePeek(free_buffer_count, NULL, portMAX_DELAY);

		// NEVER get the ring index before entering the lock!
		const uint64_t ring_index = transfer_queue->ring_index;

		// Get the buffer pointer of the current ring buffer slot, multiple commits.
		uint16_t* ring_buffer_slot = &ring_buffer[(ring_index % ring_buffer_slot_count) * frame_size];

		// Initialize the index and the buffer offset of the pending transmissions.
		uint32_t pending_transmit_offset	= 0U;
		uint32_t pending_transmit_index		= 0U;

		// Merge all dirty tiles into batches to prevent esp_lcd overhead.
		for (uint32_t tile_y = 0U; tile_y < frame_tile_count_y; tile_y ++) {
			// The bitset is a line of tiles.
			uint32_t bitset = dirty_tiles[tile_y];
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
				dirty_tiles[tile_y] &= ~mask;

				// Reserve height for the batch.
				uint32_t size_y	= 1U;

				// Check if following lines can be batched.
				for (uint32_t tile_y_2 = tile_y + 1U; tile_y_2 < frame_tile_count_y; tile_y_2 ++) {
					if ((dirty_tiles[tile_y_2] & mask) == mask) {
						// Update the height, remove the batchable ones from the line to avoid duplicated batch.
						dirty_tiles[tile_y_2] &= ~mask;
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
				const uint32_t batch_size				= batch_size_x * batch_size_y;

				// Get the buffer pointer of this transmission.
				uint16_t* dst_offset = &ring_buffer_slot[pending_transmit_offset];

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

				esp_fast_lcd_panel_transmit_t* transmit = &pending_transmits[pending_transmit_index];

				transmit->position_x	= batch_start_position_x;
				transmit->position_y	= batch_start_position_y;
				transmit->size_x		= batch_size_x;
				transmit->size_y		= batch_size_y;
				transmit->buffer_offset	= pending_transmit_offset;

				// Increment the offset and index.
				pending_transmit_offset	+= batch_size;
				pending_transmit_index	++;

				offset +=	size_x; // The x of the next batch rectangle should skip the ones of the current rectangle.
				bitset >>=	size_x; // Remove the ones of the current bitset.
			}
		}

		// Update the count of pending transmissions.
		transfer_queue->pending_transmit_count = pending_transmit_index;

		// Release the lock, exit critical.
		xSemaphoreGive(ring_buffer_lock);
	}

	return ESP_OK;
}

esp_err_t esp_fast_lcd_transmit(const esp_fast_lcd_panel_device_t* context) {
	esp_fast_lcd_panel_transfer_queue_t* transfer_queue = context->transfer_queue;

	// Get the ring buffer lock and free buffer count semaphore from the transfer queue.
	const SemaphoreHandle_t free_buffer_count	= transfer_queue->free_buffer_count;
	const SemaphoreHandle_t ring_buffer_lock	= transfer_queue->ring_buffer_lock;

	// Acquire the lock, enter critical.
	xSemaphoreTake(ring_buffer_lock, portMAX_DELAY);

	// Only send the actual transmission if there are pending transmissions.
	if (transfer_queue->pending_transmit_count > 0U) {
		// Get necessary infos about this transmission.
		const esp_fast_lcd_panel_properties_t* properties = context->properties;

		// Acquire only necessary handles in the lock.
		const esp_fast_lcd_panel_transmit_t*	pending_transmits		= transfer_queue->pending_transmits;
		const uint32_t							pending_transmit_count	= transfer_queue->pending_transmit_count;
		const uint64_t							ring_index				= transfer_queue->ring_index;

		// Create the local ongoing transmission array.
		esp_fast_lcd_panel_transmit_t ongoing_transmits[pending_transmit_count];

		// Copy the pending transmits to be ongoing transmits, leaving pending transmits for the next commit.
		memcpy(
			/* dst_buffer	= */ ongoing_transmits,
			/* src_buffer	= */ pending_transmits,
			/* length		= */ sizeof(esp_fast_lcd_panel_transmit_t) * pending_transmit_count
		);

		// Mark this slot used/in-transmitting, by the time transmit is called, commit has already waited until there is
		// at least one slot available.
		xSemaphoreTake(free_buffer_count, portMAX_DELAY);

		// Clear the pending transmission array, increment the ring index for the next commit.
		transfer_queue->pending_transmit_count = 0U;
		transfer_queue->ring_index ++;

		// Release the lock, exit critical, we have take the ownership of the necessary data/info to finish this transmission.
		xSemaphoreGive(ring_buffer_lock);

				atomic_uint*	ring_transmits			= transfer_queue->ring_transmits;
		const	uint16_t*		ring_buffer				= transfer_queue->ring_buffer;
		const	uint32_t		frame_size				= properties	->frame_size;
		const	uint32_t		ring_buffer_slot_count	= properties	->configuration.ring_buffer_slot_count;

		// Calculate the ring buffer index using the old ring index, initialize the failed transmission count.
		uint32_t ring_buffer_index = ring_index % ring_buffer_slot_count;

		// Get the buffer pointer of the ring buffer slot of current transmission
		const uint16_t* ring_buffer_slot = &ring_buffer[ring_buffer_index * frame_size];

		// Fill the in-flight transmission count.
		atomic_fetch_add(&ring_transmits[ring_buffer_index], pending_transmit_count);

		// Send all pending transmissions.
		for (uint32_t index = 0U; index < pending_transmit_count; index ++) {
			// Get the current pending transmission to be sent.
			const esp_fast_lcd_panel_transmit_t* transmit = &ongoing_transmits[index];

			// Get the range of the current transmission on the panel
			const uint32_t position_x	= transmit->position_x;
			const uint32_t position_y	= transmit->position_y;
			const uint32_t size_x		= transmit->size_x;
			const uint32_t size_y		= transmit->size_y;

			// Get the pointer of the data of the current transmission at the ring buffer slot.
			const uint16_t* transmit_data = &ring_buffer_slot[transmit->buffer_offset];

			// Transmit the data to the LCD panel, error is non-recoverable.
			ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(
				/* panel		= */ context->handle,
				/* x_start		= */ position_x,
				/* y_start		= */ position_y,
				/* x_end		= */ position_x + size_x,
				/* y_end		= */ position_y + size_y,
				/* color_data	= */ transmit_data
			), ESP_FAST_LCD_TAG, "Failed to transmit range of positionX=%" PRIu32 ", positionY=%" PRIu32 ", sizeX=%" PRIu32 ", sizeY=%" PRIu32 " to the LCD panel.",
				/* PRIu32 */ position_x,
				/* PRIu32 */ position_y,
				/* PRIu32 */ size_x,
				/* PRIu32 */ size_y
			);
		}
	} else {
		// Release the lock, exit critical.
		xSemaphoreGive(ring_buffer_lock);
	}

	return ESP_OK;
}

IRAM_ATTR bool private_on_commit_done(
	esp_lcd_panel_io_handle_t		panel_io_handle,
	esp_lcd_panel_io_event_data_t*	panel_io_event,
	void*							user_handle
) {
	// Cast the user context to the LCD device panel device and get its transfer queue.
	const	esp_fast_lcd_panel_device_t*			context			= (esp_fast_lcd_panel_device_t*) user_handle;
	const	esp_fast_lcd_panel_properties_t*		properties		= context->properties;
			esp_fast_lcd_panel_transfer_queue_t*	transfer_queue	= context->transfer_queue;

	// Check if there is a higher priority task.
	BaseType_t higher_priority_task_woken = pdFALSE;

	// Decrease the in-flight transmission count of the current ring buffer slot at transmit index.
	// It's brittle here, make sure the in-flight transmission count is filled before the actual transmission is submitted.
	if (atomic_fetch_sub(&transfer_queue->ring_transmits[transfer_queue->transmit_index % properties->configuration.ring_buffer_slot_count], 1U) == 1U) {
		// Increment the counting semaphore from ISR then increment the transmit index if no in-flight transmit in the current slot.
		xSemaphoreGiveFromISR(context->transfer_queue->free_buffer_count, &higher_priority_task_woken);
		// Increment to the next slot transmitting/to be transmitted.
		transfer_queue->transmit_index ++;
	}

	// Tell the driver if it should yield from ISR.
	return higher_priority_task_woken == pdTRUE;
}