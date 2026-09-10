#include "esp_log.h"
#include "esp_fast_lcd.h"
#include "esp_fast_lcd_common.h"
#include "esp_fast_lcd_common_rect.h"
#include "esp_fast_lcd_common_masked.h"

esp_err_t esp_fast_lcd_draw_native_rectangle_masked(
	const	esp_fast_lcd_panel_device_t*	context,
			int32_t							position_x,
			int32_t							position_y,
			uint32_t						size_x,
			uint32_t						size_y,
			uint32_t						bitmask_offset_x,
			uint32_t						bitmask_offset_y,
			uint32_t						bitmask_size_x,
			uint8_t							bitmask_flipped,
			uint16_t						color_rgb565,
	const	uint16_t*						bitmask_rgb565
) {
	// We cannot proceed without context.
	if (context == NULL) {
		// Log the error if LCD panel debug logging is enabled.
		#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
			ESP_LOGE(ESP_FAST_LCD_TAG, "No esp_fast_lcd_panel_device_t handle provided when performing drawing an opaque native masked rectangle.");
		#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		return ESP_ERR_INVALID_ARG;
	}

	// Skip the draw if the rect has no size:
	if (	size_x == 0U
		||	size_y == 0U
	) {
		return ESP_OK;
	}

	// Get the transfer queue and properties from the LCD panel device context.
	const	esp_fast_lcd_panel_properties_t*		properties		= context->properties;
			esp_fast_lcd_panel_transfer_queue_t*	transfer_queue	= context->transfer_queue;

	// Get the detailed properties of the device for calculating clipped range of the rectangle and dirty tiles range.
	const uint32_t frame_size_x			= properties->configuration.frame_size_x;
	const uint32_t frame_size_y			= properties->configuration.frame_size_y;
	const uint32_t frame_tile_size_x	= properties->configuration.frame_tile_size_x;
	const uint32_t frame_tile_size_y	= properties->configuration.frame_tile_size_y;

	// Get the framebuffer from the transfer queue.
	uint16_t* framebuffer = transfer_queue->framebuffer;

	// Skip if the position is out of bounds.
	if (	position_x >= ((int32_t) frame_size_x)
		||	position_y >= ((int32_t) frame_size_y)
	) {
		return ESP_OK;
	}

	// Get the ending bottom-right position of the rectangle to be drawn.
	const int32_t end_position_x_exclusive = position_x + ((int32_t) size_x);
	const int32_t end_position_y_exclusive = position_y + ((int32_t) size_y);

	// Skip if the range is out of bounds.
	if (	end_position_x_exclusive <= 0
		||	end_position_y_exclusive <= 0
	) {
		return ESP_OK;
	}

	// Get the clipped start top-left and end bottom-right position of the rectangle.
	const uint32_t clipped_start_position_x			= position_x				< 0				? 0U			: ((uint32_t) position_x);
	const uint32_t clipped_start_position_y			= position_y				< 0				? 0U			: ((uint32_t) position_y);
	const uint32_t clipped_end_position_x_exclusive	= end_position_x_exclusive	> frame_size_x	? frame_size_x	: ((uint32_t) end_position_x_exclusive);
	const uint32_t clipped_end_position_y_exclusive	= end_position_y_exclusive	> frame_size_y	? frame_size_y	: ((uint32_t) end_position_y_exclusive);

	// Calculate the clipped bitmask offset start position of the rectangle.
	const uint32_t clipped_mask_start_offset_x = position_x < 0 ? ((uint32_t) (-position_x)) : 0;
	const uint32_t clipped_mask_start_offset_y = position_y < 0 ? ((uint32_t) (-position_y)) : 0;

	// Calculate the clipped width and height at the current position.
	const uint32_t clipped_size_x = clipped_end_position_x_exclusive - clipped_start_position_x;
	const uint32_t clipped_size_y = clipped_end_position_y_exclusive - clipped_start_position_y;

	// Calculate the tile range of the rect based on the clipped position.
	const uint32_t tile_x_start	= clipped_start_position_x					/ frame_tile_size_x;
	const uint32_t tile_y_start	= clipped_start_position_y					/ frame_tile_size_y;
	const uint32_t tile_x_end	= (clipped_end_position_x_exclusive - 1U)	/ frame_tile_size_x; // use inclusive end position (exclusive - 1).
	const uint32_t tile_y_end	= (clipped_end_position_y_exclusive - 1U)	/ frame_tile_size_y; // use inclusive end position (exclusive - 1).

	// Mark the frame dirty.
	transfer_queue->frame_dirty = true;

	// Get the dirty tile bitsets from the transfer queue.
	uint32_t* dirty_tiles = transfer_queue->dirty_tiles;

	// Calculate the count of ones in the dirty tile bitmask.
	const uint32_t tile_count_x = tile_x_end - tile_x_start + 1U; // inclusive.

	// Mark all tiles that the rect covers dirty.
	for (uint32_t tile_y = tile_y_start; tile_y <= tile_y_end; tile_y ++) {
		// Set the bit of the tiles to 1.
		dirty_tiles[tile_y] |= (((1U << tile_count_x) - 1U) << tile_x_start);
	}

	// Flip the LSB and MSB to get correct transmission byte order.
	const uint16_t color_src_flipped =	((color_rgb565 >> 8U) & 0x00FFU)
	|									((color_rgb565 << 8U) & 0xFF00U);

	// Log the operation if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		// Get all color components of rgb565.
		const uint8_t r5_src = (uint8_t) ((color_rgb565 >> 11U)	& 0b011111U);
		const uint8_t g6_src = (uint8_t) ((color_rgb565 >> 5U)	& 0b111111U);
		const uint8_t b5_src = (uint8_t) ((color_rgb565 >> 0U)	& 0b011111U);

		ESP_LOGD(ESP_FAST_LCD_TAG, "LCD panel device \"%s\" is performing an opaque native masked rectangle draw at: positionX=%" PRId32 ", positionY=%" PRId32 ", sizeX=%" PRIu32 ", sizeY=%" PRIu32 ".",
			/* s		*/ properties->configuration.name,
			/* PRId32	*/ position_x,
			/* PRId32	*/ position_y,
			/* PRIu32	*/ size_x,
			/* PRIu32	*/ size_y
		);
		ESP_LOGD(ESP_FAST_LCD_TAG, "Native pixel color of the rectangle: r=0x%02" PRIX8 ", g=0x%02" PRIX8 ", b=0x%02" PRIX8 ", a=0x%02" PRIX8 ".",
			/* PRIX8 */ r5_src,
			/* PRIX8 */ g6_src,
			/* PRIX8 */ b5_src,
			/* PRIX8 */ 0xFF
		);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Check if we should manually flip the bitmask.
	uint8_t flip_mask = !bitmask_flipped;

	// Enumerate every line of the rectangle.
	for (uint32_t y = 0U; y < clipped_size_y; y ++) {
		// Extract the X cursor out of the loop for u32 fill optimization.
		uint32_t x = 0;

		// Calculate the index of the first pixel of the current line at the framebuffer.
		const uint32_t dst_index =	/* index_y	= */ (clipped_start_position_y + y) * frame_size_x +
									/* index_x	= */ (clipped_start_position_x + 0);

		// Calculate the offset of the first pixel of the current line at the framebuffer.
		uint16_t* dst_offset = &framebuffer[dst_index];

		// Calculate the offset of the first bitmask pixel of the line at bitmask.
		const uint16_t* bitmask_offset = &bitmask_rgb565[
			/* index_y = */ (clipped_mask_start_offset_y + bitmask_offset_y + y) * bitmask_size_x +
			/* index_x = */ (clipped_mask_start_offset_x + bitmask_offset_x + 0)
		];

		// It is not worthwhile to use SIMD if clipped_size_x is too short.
		if (clipped_size_x < 16U) {
			if (bitmask_flipped && is_same_align_4byte(dst_offset, bitmask_offset)) {
				// Combine 2 flipped RGB565 color together to speed up the first line fill.
				const uint32_t color_src_flipped_32 =	(((uint32_t) color_src_flipped) << 16U)
				|										(((uint32_t) color_src_flipped) << 0U);

				// Align the X to multiples of twos in order to fill the line using u32.
				if (dst_index % 2U != 0U) {
					// Get the flipped original color and the color bitmask of the current pixel.
					const uint16_t color_dst_flipped		= dst_offset		[x];
					const uint16_t color_bitmask_flipped	= bitmask_offset	[x];

					// Mix and write the masked flipped color to framebuffer.
					dst_offset[x] = private_mix_mask_color(
						/* color_dst		= */ color_dst_flipped,
						/* color_src		= */ color_src_flipped,
						/* color_bitmask	= */ color_bitmask_flipped
					);
					// Increment the cursor.
					x ++;
				}

				// Try to fill the rest of the line of the rectangle with the u32 batch color.
				for (; x + 2U <= clipped_size_x; x += 2U) {
					// Get the 32-bit form of the flipped original color and the color bitmask of two pixels.
					const uint32_t color_dst_flipped_32		= *((uint32_t*) (&dst_offset		[x]));
					const uint32_t color_bitmask_flipped_32	= *((uint32_t*) (&bitmask_offset	[x]));

					// Mix the colors with the flipped color bitmask.
					const uint32_t color_dst_flipped_masked_32 = color_dst_flipped_32 & ~	color_bitmask_flipped_32;
					const uint32_t color_src_flipped_masked_32 = color_src_flipped_32 &		color_bitmask_flipped_32;

					// Fill the reinterpreted framebuffer with u32 combined colors.
					*((uint32_t*) (&dst_offset[x])) = color_dst_flipped_masked_32 | color_src_flipped_masked_32;
				}

				// Fill the last pixel.
				if (x < clipped_size_x) {
					// Get the flipped original color and the color bitmask of the current pixel.
					const uint16_t color_dst_flipped		= dst_offset		[x];
					const uint16_t color_bitmask_flipped	= bitmask_offset	[x];

					// Mix and write the masked flipped color to framebuffer.
					dst_offset[x] = private_mix_mask_color(
						/* color_dst		= */ color_dst_flipped,
						/* color_src		= */ color_src_flipped,
						/* color_bitmask	= */ color_bitmask_flipped
					);
				}
			} else {
				// Mask and fill the pixels one-by-one manually.
				for (; x < clipped_size_x; x ++) {
					// Get the flipped original color and the color bitmask of the current pixel.
					const	uint16_t color_dst_flipped	= dst_offset		[x];
							uint16_t color_bitmask		= bitmask_offset	[x];

					// Flip the bitmask if the bitmask is not flipped.
					if (flip_mask) {
						color_bitmask =	((color_bitmask >> 8U) & 0x00FFU)
						|				((color_bitmask << 8U) & 0xFF00U);
					}

					// Mix and write the masked flipped color to framebuffer.
					dst_offset[x] = private_mix_mask_color(
						/* color_dst		= */ color_dst_flipped,
						/* color_src		= */ color_src_flipped,
						/* color_bitmask	= */ color_bitmask
					);
				}
			}
		} else {
			// Get the colors that we need to fill to reach the next 16-byte aligned address.
			// ">> 1U" means "divided 2" because a color is 2 bytes long as RGB565.
			const uint32_t padding = next_16byte_padding(dst_offset) >> 1U;

			// Fill the padding manually if needed.
			// Exit immediately when the padding or the size is reached.
			for (; x < clipped_size_x && x < padding; x ++) {
				// Get the flipped original color and the color bitmask of the current pixel.
				const	uint16_t color_dst_flipped	= dst_offset		[x];
						uint16_t color_bitmask		= bitmask_offset	[x];

				// Flip the bitmask if the bitmask is not flipped.
				if (flip_mask) {
					color_bitmask =	((color_bitmask >> 8U) & 0x00FFU)
					|				((color_bitmask << 8U) & 0xFF00U);
				}

				// Mix and write the masked flipped color to framebuffer.
				dst_offset[x] = private_mix_mask_color(
					/* color_dst		= */ color_dst_flipped,
					/* color_src		= */ color_src_flipped,
					/* color_bitmask	= */ color_bitmask
				);
			}

			// Broadcast the flipped RGB565 color to the SIMD vector register.
			asm volatile(vector_broadcast_16(q1, arg(0)) :: "a"(&color_src_flipped));

			// Note:
			// Now Q1 is the flipped RGB565 color of the rectangle.

			// Get the SIMD start offset of the framebuffer and the bitmask.
					uint16_t* simd_offset_dst	= &dst_offset		[x];
			const	uint16_t* simd_offset_mask	= &bitmask_offset	[x];

			// Evaluate the alignment of the bitmask offset.
			uint8_t bitmask_offset_aligned = is_same_align_16byte(simd_offset_dst, simd_offset_mask);

			// SIMD is 8 colors a batch (128-bit, 8 16-bit colors).
			for (; x + 8U <= clipped_size_x; x += 8U) {
				// Load the aligned framebuffer colors to the SIMD vector register.
				asm volatile(vector_load_128_aligned(q0, arg(0), 0) : "+a"(simd_offset_dst));

				// Load the color bitmasks of the bitmask to the SIMD vector register.
				if (bitmask_offset_aligned) {
					// Load the aligned color bitmasks to the SIMD vector register.
					asm volatile(vector_load_128_aligned(q2, arg(0), 16) : "+a"(simd_offset_mask));
				} else {
					// Load two parts of unaligned color bitmasks then combine them together in to the SIMD vector register.
					asm volatile(
						vector_load_128_usar			(q6, arg(0),	16)	// Load lower n-bits data from the unaligned bitmask address.
						vector_load_128_usar			(q7, arg(0),	0)	// Load the higher (128-n) bits data from the unaligned bitmask address.
						vector_shift_right_combined_256	(q2, q6,		q7)	// Combine two parts of the data together.
						: /* arg(0) = */ "+a"(simd_offset_mask)
					);
				}

				// Note:
				// Now Q0 is the original aligned flipped framebuffer colors.
				// Q1 is the flipped RGB565 color of the rectangle.

				// Flip the bitmasks if the bitmask is not flipped.
				if (flip_mask) {
					asm_vector_swap_16(
						/* src_register = */ q2,
						/* dst_register = */ q2,
						/* tmp_register = */ q7
					);
				}

				// Note:
				// Now Q2 is the flipped color bitmasks of 8 pixels.
				// Q0 is the original aligned flipped framebuffer colors.
				// Q1 is the flipped RGB565 color of the rectangle.

				// Mix the framebuffer color and the rectangle color using the bitmask.
				asm volatile(
					vector_bitwise_and_16	(q3, q1, q2)	// Remove invisible colors.
					vector_bitwise_not		(q2, q2)		// Bitwise-NOT the bitmask.
					vector_bitwise_and_16	(q0, q0, q2)	// Remove colors to be overwritten.
					vector_bitwise_or_16	(q0, q0, q3)	// Bitwise-OR to combine the old colors and the new colors.
				);

				// Store the masked colors to the memory.
				// VST.128.IP will increment the %0 operand register automatically by the third operand (16).
				asm volatile(vector_store_128_aligned(q0, arg(0), 16) : "+a"(simd_offset_dst));
			}

			// Manually fill the remaining padding colors until the size is reached.
			for (; x < clipped_size_x; x ++) {
				// Get the flipped original color and the color bitmask of the current pixel.
				const	uint16_t color_dst_flipped	= dst_offset		[x];
						uint16_t color_bitmask		= bitmask_offset	[x];

				// Flip the bitmask if the bitmask is not flipped.
				if (flip_mask) {
					color_bitmask =	((color_bitmask >> 8U) & 0x00FFU)
					|				((color_bitmask << 8U) & 0xFF00U);
				}

				// Mix and write the masked flipped color to framebuffer.
				dst_offset[x] = private_mix_mask_color(
					/* color_dst		= */ color_dst_flipped,
					/* color_src		= */ color_src_flipped,
					/* color_bitmask	= */ color_bitmask
				);
			}
		}
	}

	return ESP_OK;
}

esp_err_t esp_fast_lcd_draw_rectangle_masked(
	const	esp_fast_lcd_panel_device_t*	context,
			int32_t							position_x,
			int32_t							position_y,
			uint32_t						size_x,
			uint32_t						size_y,
			uint32_t						bitmask_offset_x,
			uint32_t						bitmask_offset_y,
			uint32_t						bitmask_size_x,
			uint8_t							bitmask_flipped,
			uint32_t						color_rgba8888,
	const	uint16_t*						bitmask_rgb565
) {
	// We cannot proceed without context.
	if (context == NULL) {
		// Log the error if LCD panel debug logging is enabled.
		#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
			ESP_LOGE(ESP_FAST_LCD_TAG, "No esp_fast_LCD_panel_device_t handle provided when performing drawing a translucent masked rectangle.");
		#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		return ESP_ERR_INVALID_ARG;
	}

	// Skip the draw if the rect:
	if (	size_x == 0U									// has no size.
		||	size_y == 0U									// has no size.
		||	color_rgba8888_is_transparent(color_rgba8888)	// is invisible.
	) {
		return ESP_OK;
	}

	// Get all color components of rgba8888.
	const uint8_t r8_src = (uint8_t) ((color_rgba8888 >> 24U)	& 0xFFU);
	const uint8_t g8_src = (uint8_t) ((color_rgba8888 >> 16U)	& 0xFFU);
	const uint8_t b8_src = (uint8_t) ((color_rgba8888 >> 8U)	& 0xFFU);
	const uint8_t a8_src = (uint8_t) ((color_rgba8888 >> 0U)	& 0xFFU);

	// Route to native RGB565 opaque rectangle draw function if the color of the rectangle is opaque.
	if (color_rgba8888_is_opaque(color_rgba8888)) {
		// Map the RGBA8888 color components to RGB565 color components.
		const uint8_t r5_src = r8_src >> 3U;
		const uint8_t g6_src = g8_src >> 2U;
		const uint8_t b5_src = b8_src >> 3U;

		// Pack the color components into the RGB565 format;
		const uint16_t color_src_rgb565 =	((((uint16_t) r5_src) & 0b011111U) << 11U)
		|									((((uint16_t) g6_src) & 0b111111U) << 5U)
		|									((((uint16_t) b5_src) & 0b011111U) << 0U);

		// Call the native opaque rectangle draw function with converted color.
		return esp_fast_lcd_draw_native_rectangle_masked(
			/* context			= */ context,
			/* position_x		= */ position_x,
			/* position_y		= */ position_y,
			/* size_x			= */ size_x,
			/* size_y			= */ size_y,
			/* bitmask_offset_x	= */ bitmask_offset_x,
			/* bitmask_offset_y	= */ bitmask_offset_y,
			/* bitmask_size_x	= */ bitmask_size_x,
			/* bitmask_flipped	= */ bitmask_flipped,
			/* color_rgb565		= */ color_src_rgb565,
			/* bitmask_rgb565	= */ bitmask_rgb565
		);
	}

	// Get the transfer queue and properties from the LCD panel device context.
	const	esp_fast_lcd_panel_properties_t*		properties		= context->properties;
			esp_fast_lcd_panel_transfer_queue_t*	transfer_queue	= context->transfer_queue;

	// Get the detailed properties of the device for calculating clipped range of the rectangle and dirty tiles range.
	const uint32_t frame_size_x			= properties->configuration.frame_size_x;
	const uint32_t frame_size_y			= properties->configuration.frame_size_y;
	const uint32_t frame_tile_size_x	= properties->configuration.frame_tile_size_x;
	const uint32_t frame_tile_size_y	= properties->configuration.frame_tile_size_y;

	// Get the framebuffer from the transfer queue.
	uint16_t* framebuffer = transfer_queue->framebuffer;

	// Skip if the position is out of bounds.
	if (	position_x >= ((int32_t) frame_size_x)
		||	position_y >= ((int32_t) frame_size_y)
	) {
		return ESP_OK;
	}

	// Get the ending bottom-right position of the rectangle to be drawn.
	const int32_t end_position_x_exclusive = position_x + ((int32_t) size_x);
	const int32_t end_position_y_exclusive = position_y + ((int32_t) size_y);

	// Skip if the range is out of bounds.
	if (	end_position_x_exclusive <= 0
		||	end_position_y_exclusive <= 0
	) {
		return ESP_OK;
	}

	// Get the clipped start top-left and end bottom-right position of the rectangle.
	const uint32_t clipped_start_position_x			= position_x				< 0				? 0U			: ((uint32_t) position_x);
	const uint32_t clipped_start_position_y			= position_y				< 0				? 0U			: ((uint32_t) position_y);
	const uint32_t clipped_end_position_x_exclusive	= end_position_x_exclusive	> frame_size_x	? frame_size_x	: ((uint32_t) end_position_x_exclusive);
	const uint32_t clipped_end_position_y_exclusive	= end_position_y_exclusive	> frame_size_y	? frame_size_y	: ((uint32_t) end_position_y_exclusive);

	// Calculate the clipped bitmask offset start position of the rectangle.
	const uint32_t clipped_mask_start_offset_x = position_x < 0 ? ((uint32_t) (-position_x)) : 0;
	const uint32_t clipped_mask_start_offset_y = position_y < 0 ? ((uint32_t) (-position_y)) : 0;

	// Calculate the clipped width and height at the current position.
	const uint32_t clipped_size_x = clipped_end_position_x_exclusive - clipped_start_position_x;
	const uint32_t clipped_size_y = clipped_end_position_y_exclusive - clipped_start_position_y;

	// Calculate the tile range of the rect based on the clipped position.
	const uint32_t tile_x_start	= clipped_start_position_x					/ frame_tile_size_x;
	const uint32_t tile_y_start	= clipped_start_position_y					/ frame_tile_size_y;
	const uint32_t tile_x_end	= (clipped_end_position_x_exclusive - 1U)	/ frame_tile_size_x; // use inclusive end position (exclusive - 1).
	const uint32_t tile_y_end	= (clipped_end_position_y_exclusive - 1U)	/ frame_tile_size_y; // use inclusive end position (exclusive - 1).

	// Mark the frame dirty.
	transfer_queue->frame_dirty = true;

	// Get the dirty tile bitsets from the transfer queue.
	uint32_t* dirty_tiles = transfer_queue->dirty_tiles;

	// Calculate the count of ones in the dirty tile bitmask.
	const uint32_t tile_count_x = tile_x_end - tile_x_start + 1U; // inclusive.

	// Mark all tiles that the rect covers dirty.
	for (uint32_t tile_y = tile_y_start; tile_y <= tile_y_end; tile_y ++) {
		// Set the bit of the tiles to 1.
		dirty_tiles[tile_y] |= (((1U << tile_count_x) - 1U) << tile_x_start);
	}

	// Invert the alpha component.
	const uint8_t a8_src_inv = 255U - a8_src;

	// Pre-multiply the color with its alpha.
	const uint8_t r8_src_pre_mul = unorm8_mul_exact(a8_src, r8_src);
	const uint8_t g8_src_pre_mul = unorm8_mul_exact(a8_src, g8_src);
	const uint8_t b8_src_pre_mul = unorm8_mul_exact(a8_src, b8_src);

	// Log the operation if LCD panel debug logging is enabled.
	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "LCD panel device \"%s\" is performing an translucent masked rectangle draw at: positionX=%" PRId32 ", positionY=%" PRId32 ", sizeX=%" PRIu32 ", sizeY=%" PRIu32 ".",
			/* s		*/ properties->configuration.name,
			/* PRId32	*/ position_x,
			/* PRId32	*/ position_y,
			/* PRIu32	*/ size_x,
			/* PRIu32	*/ size_y
		);
		ESP_LOGD(ESP_FAST_LCD_TAG, "Pixel color of the rectangle before the blending: r=0x%02" PRIX8 ", g=0x%02" PRIX8 ", b=0x%02" PRIX8 ", a=0x%02" PRIX8 ".",
			/* PRIX8 */ r8_src,
			/* PRIX8 */ g8_src,
			/* PRIX8 */ b8_src,
			/* PRIX8 */ a8_src
		);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Check if we should manually flip the bitmask.
	uint8_t flip_mask = !bitmask_flipped;

	// It is not worthwhile to use SIMD if clipped_size_x is too short.
	if (clipped_size_x < 16) {
		// Mix every pixel in the range with the rectangle color.
		for		(uint32_t y = 0U; y < clipped_size_y; y ++) {
			for	(uint32_t x = 0U; x < clipped_size_x; x ++) {
				uint32_t pixel_index =	/* index_y = */ (clipped_start_position_y + y) * frame_size_x +
										/* index_x = */ (clipped_start_position_x + x);

				// Get the flipped original color of the pixel from the framebuffer.
				uint16_t color_dst_flipped = framebuffer[pixel_index];

				// Get the color bitmask of the pixel from the bitmask.
				uint16_t color_src_bitmask = bitmask_rgb565[
					/* index_y = */ (clipped_mask_start_offset_y + bitmask_offset_y + y) * bitmask_size_x +
					/* index_x = */ (clipped_mask_start_offset_x + bitmask_offset_x + x)
				];

				// Flip the bitmask if the bitmask is not flipped.
				if (flip_mask) {
					color_src_bitmask =	((color_src_bitmask >> 8U) & 0x00FFU)
					|					((color_src_bitmask << 8U) & 0xFF00U);
				}

				// Flip the LSB and MSB to get the correct RGB565 color.
				const uint16_t color_dst_rgb565 =	((color_dst_flipped >> 8U) & 0x00FFU)
				|									((color_dst_flipped << 8U) & 0xFF00U);

				// Get all color components of rgb565.
				const uint8_t r5_dst = (uint8_t) ((color_dst_rgb565 >> 11U)	& 0b011111U);
				const uint8_t g6_dst = (uint8_t) ((color_dst_rgb565 >> 5U)	& 0b111111U);
				const uint8_t b5_dst = (uint8_t) ((color_dst_rgb565 >> 0U)	& 0b011111U);

				// Map the framebuffer RGB565 color to 0-255.
				const uint8_t r8_dst = (uint8_t) ((r5_dst << 3U) | (r5_dst >> 2U));
				const uint8_t g8_dst = (uint8_t) ((g6_dst << 2U) | (g6_dst >> 4U));
				const uint8_t b8_dst = (uint8_t) ((b5_dst << 3U) | (b5_dst >> 2U));

				// Mix the incoming color with the original color using painter's algorithm.
				const uint16_t r16 = ((uint16_t) r8_src_pre_mul) + ((uint16_t) unorm8_mul_exact(a8_src_inv, r8_dst));
				const uint16_t g16 = ((uint16_t) g8_src_pre_mul) + ((uint16_t) unorm8_mul_exact(a8_src_inv, g8_dst));
				const uint16_t b16 = ((uint16_t) b8_src_pre_mul) + ((uint16_t) unorm8_mul_exact(a8_src_inv, b8_dst));

				// Clamp the mixed possible 16-bit color back to 0-255.
				const uint8_t r8_final = r16 > 255U ? 255U : ((uint8_t) r16);
				const uint8_t g8_final = g16 > 255U ? 255U : ((uint8_t) g16);
				const uint8_t b8_final = b16 > 255U ? 255U : ((uint8_t) b16);

				// Map the RGB888 into RGB565.
				const uint8_t r5_final = r8_final >> 3U;
				const uint8_t g6_final = g8_final >> 2U;
				const uint8_t b5_final = b8_final >> 3U;

				// Pack them into RGB565 format;
				const uint16_t color_final_rgb565 =	((((uint16_t) r5_final)	& 0b011111U) << 11U)
				|									((((uint16_t) g6_final)	& 0b111111U) << 5U)
				|									((((uint16_t) b5_final)	& 0b011111U) << 0U);

				// Flip the LSB and MSB back to get correct transmission byte order.
				const uint16_t color_final_flipped =	((color_final_rgb565 >> 8U) & 0x00FFU)
				|										((color_final_rgb565 << 8U) & 0xFF00U);

				// Mix and write the masked flipped color to framebuffer.
				framebuffer[pixel_index] = private_mix_mask_color(
					/* color_dst		= */ color_dst_flipped,
					/* color_src		= */ color_final_flipped,
					/* color_bitmask	= */ color_src_bitmask
				);
			}
		}
	} else {
		// Prepare the RGB565 pre-multiplied color components of the incoming color.
		const uint8_t r5_src_pre_mul = r8_src_pre_mul >> 3U;
		const uint8_t g6_src_pre_mul = g8_src_pre_mul >> 2U;
		const uint8_t b5_src_pre_mul = b8_src_pre_mul >> 3U;

		// Shift the RGB565 pre-multiplied color components to the corresponding bit ranges.
		const uint16_t r5_src_pre_mul_shift = ((uint16_t) r5_src_pre_mul) << 11U;
		const uint16_t g6_src_pre_mul_shift = ((uint16_t) g6_src_pre_mul) << 5U;
		const uint16_t b5_src_pre_mul_shift = ((uint16_t) b5_src_pre_mul) << 0U;

		// Invert of alpha needs to be in 16-bit form instead of 8-bit form.
		const uint16_t a8_src_inv_16 = ((uint16_t) a8_src_inv) & 0x00FFU;

		// Red needs to be in 32-bit form for later zipped 32-bit vector addition.
		const uint32_t r5_src_pre_mul_shift_32 = ((uint32_t) r5_src_pre_mul_shift) & 0x0000FFFFU;

		// Use SIMD for every line of the rectangle.
		for (uint32_t y = 0U; y < clipped_size_y; y ++) {
			// Extract the X cursor out of the loop for u32 fill optimization.
			uint32_t x = 0;

			// Calculate the offset of the first pixel of the line at the framebuffer.
			uint16_t* dst_offset = &framebuffer[
				(clipped_start_position_y + y) * frame_size_x +
				(clipped_start_position_x + 0)
			];

			// Calculate the offset of the first bitmask pixel of the line at bitmask.
			const uint16_t* bitmask_offset = &bitmask_rgb565[
				/* index_y = */ (clipped_mask_start_offset_y + bitmask_offset_y + y) * bitmask_size_x +
				/* index_x = */ (clipped_mask_start_offset_x + bitmask_offset_x + 0)
			];

			// Get the colors that we need to fill to reach the next 16-byte aligned address.
			// ">> 1U" means "divided 2" because a color is 2 bytes long as RGB565.
			const uint32_t padding = next_16byte_padding(dst_offset) >> 1U;

			// Blend the padding colors manually if needed.
			// Exit immediately when the padding or the size is reached.
			for (; x < clipped_size_x && x < padding; x ++) {
				// Get the flipped original color and the color bitmask of the current pixel.
				const	uint16_t color_dst_flipped = dst_offset		[x];
						uint16_t color_src_bitmask = bitmask_offset	[x];

				// Flip the bitmask if the bitmap is not flipped.
				if (flip_mask) {
					color_src_bitmask =	((color_src_bitmask >> 8U) & 0x00FFU)
					|					((color_src_bitmask << 8U) & 0xFF00U);
				}

				// Flip the LSB and MSB to get the correct RGB565 color.
				const uint16_t color_dst_rgb565 =	((color_dst_flipped >> 8U) & 0x00FFU)
				|									((color_dst_flipped << 8U) & 0xFF00U);

				// Blend the framebuffer color with pre-multiplied incoming colors.
				const uint16_t color_final_rgb565 = private_blend_pre_mul_fast(
					/* r5_src_pre_mul	= */ r5_src_pre_mul,
					/* g6_src_pre_mul	= */ g6_src_pre_mul,
					/* b5_src_pre_mul	= */ b5_src_pre_mul,
					/* a8_src_inv		= */ a8_src_inv,
					/* color_dst_rgb565	= */ color_dst_rgb565
				);

				// Flip the LSB and MSB back to get correct transmission byte order.
				const uint16_t color_final_flipped =	((color_final_rgb565 >> 8U) & 0x00FFU)
				|										((color_final_rgb565 << 8U) & 0xFF00U);

				dst_offset[x] = private_mix_mask_color(
					/* color_dst		= */ color_dst_flipped,
					/* color_src		= */ color_final_flipped,
					/* color_bitmask	= */ color_src_bitmask
				);
			}

			// Broadcast the bitmasks of the RGB565 format to the SIMD vector registers.
			asm volatile(
				vector_broadcast_16(q4, arg(0)) // Broadcast the red5 component bitmask to the SIMD vector register.
				vector_broadcast_16(q5, arg(1)) // Broadcast the green6 component bitmask to the SIMD vector register.
				vector_broadcast_16(q6, arg(2)) // Broadcast the blue5 component bitmask to the SIMD vector register.
				:
				:	/* arg(0) = */ "a"(&rgb565_r5_bitmask),
					/* arg(1) = */ "a"(&rgb565_g6_bitmask),
					/* arg(2) = */ "a"(&rgb565_b5_bitmask)
			);

			// Note:
			// Q4, Q5, Q6 are bitmasks of color components of RGB565.
			// Q0, Q1, Q2, Q3, Q7 are free.

			// Get the SIMD start offset of the framebuffer and the bitmask.
					uint16_t* simd_offset_dst	= &dst_offset		[x];
			const	uint16_t* simd_offset_mask	= &bitmask_offset	[x];

			// Evaluate the alignment of the bitmask offset.
			uint8_t bitmask_offset_aligned = is_same_align_16byte(simd_offset_dst, simd_offset_mask);

			// SIMD is 8 colors a batch (128-bit, 8 16-bit colors).
			for (; x + 8 <= clipped_size_x; x += 8) {
				// Load 8 original colors from the framebuffer to the vector register.
				// VLD.128.IP will increment the %0 operand register automatically by the third operand (0).
				asm volatile(vector_load_128_aligned(q3, arg(0), 0) : "+a"(simd_offset_dst));

				// Swap the bytes of the U16 to get the correct RGB565 color order.
				asm_vector_swap_16(
					/* src_register = */ q3,
					/* dst_register = */ q3,
					/* tmp_register = */ q7
				);

				// Extract the color components from RGB565.
				asm volatile(
					vector_bitwise_and_16(q0, q3, q4) // Extract the red5 component from the RGB565.
					vector_bitwise_and_16(q1, q3, q5) // Extract the green6 component from the RGB565.
					vector_bitwise_and_16(q2, q3, q6) // Extract the blue5 component from the RGB565.
				);

				// Note:
				// Q0, Q1, Q2 is the original color now.
				// Q3, Q7 are free.

				// Set the shift amount register to 8 to shift the multiplication result 8 bits right.
				// aka. divided by 256. (to get approximately correct unorm8 value after multiplying the inverted alpha.)
				asm volatile(set_shift_amount(arg(0)) :: "a"(8));

				// Broadcast the inverted alpha to be multiplied to be original color to the SIMD vector register.
				asm volatile(vector_broadcast_16(q7, arg(0)) :: "a"(&a8_src_inv_16));

				// Note:
				// Q7 is inverted alpha now.
				// Q0, Q1, Q2 is the original color.
				// Q3 are free.

				// Multiply the original RGB 565 color components with the inverted alpha then divided by 256.
				asm volatile(
					vector_multiply_u16(q0, q0, q7) // Multiply the red5 with the inverted alpha then divided by 256.
					vector_multiply_u16(q1, q1, q7) // Multiply the green6 with the inverted alpha then divided by 256.
					vector_multiply_u16(q2, q2, q7) // Multiply the blue5 with the inverted alpha then divided by 256.
				);

				// Note:
				// Q0, Q1, Q2 are the pre-multiplied original color components now.
				// Q3, Q7 are free.

				// Now it's the tricky part.
				// The red part contains highest bit of the 16-bit, but we only have signed vector addition.
				// We need to shuffle it with a pure zero vector register to expand every red component to 32 bit to
				// avoid the incorrect signed saturation.

				// Fill a vector register with zeros for the shuffle.
				asm volatile(vector_clear_zero(q3));

				// Note:
				// Q3 is the zero vector now.
				// Q0, Q1, Q2 is the pre-multiplied original color .
				// Q7 are free.

				// Shuffle the red components with the zero vectors to expand the red5 components to 32-bit.
				asm volatile(vector_zip_shuffle_16(q0, q3));

				// Broadcast the pre-multiplied 32bit form of red5 components of the incoming color to the SIMD vector register.
				asm volatile(vector_broadcast_32(q7, arg(0)) :: "a"(&r5_src_pre_mul_shift_32));

				// Note:
				// Q0 is the lower part of the expanded 32-bit red5 components now.
				// Q3 is the higher part of the expanded 32-bit red5 components now.
				// Q7 is the pre-multiplied 32-bit form of red5 components of the incoming color.
				// Q1, Q2 is the pre-multiplied original color now.

				asm volatile(
					vector_add_s32				(q0, q0, q7)	// Add the pre-multiplied incoming colors to the lower part of the red5 components of the pre-multiplied original color.
					vector_add_s32				(q3, q3, q7)	// Add the pre-multiplied incoming colors to the higher part of the red5 components of the pre-multiplied original color.
					vector_unzip_unshuffle_16	(q0, q3)		// Un-shuffle the 32-bit blended red5 components, all data will go into Q0 and Q3 will theoretically be zeros.
				);

				// Note:
				// Q0 is the blended red5 components now.
				// Q1, Q2 is the pre-multiplied original green6 and blue 5 components now.
				// Q3, Q7 are free.

				// Blend the multiplied original green6/blue5 components with the incoming green6/blue5 components.
				asm volatile(
					vector_broadcast_16	(q7, arg(0)) // Broadcast the pre-multiplied green6 components of the incoming color to the SIMD vector register.
					vector_broadcast_16	(q3, arg(1)) // Broadcast the pre-multiplied blue5 components of the incoming color to the SIMD vector register.
					vector_add_s16		(q1, q1, q7) // Add the pre-multiplied incoming colors to green6 components of the pre-multiplied original color.
					vector_add_s16		(q2, q2, q3) // Add the pre-multiplied incoming colors to blue5 components of the pre-multiplied original color.
					:
					:	/* arg(0) = */ "a"(&g6_src_pre_mul_shift),
						/* arg(1) = */ "a"(&b5_src_pre_mul_shift)
				);

				// Note:
				// Q1, Q2 are the blended green6 and blue5 components now.
				// Q0 is the blended red5 components.
				// Q3, Q7 are free.

				// Ensure the blended color components only occupy their own range of bits.
				asm volatile(
					vector_bitwise_and_16(q0, q0, q4) // Bitwise-AND the blended red5 components with the bitmask to make sure they only occupy [15:11].
					vector_bitwise_and_16(q1, q1, q5) // Bitwise-AND the blended green6 components with the bitmask to make sure they only occupy [10:5].
					vector_bitwise_and_16(q2, q2, q6) // Bitwise-AND the blended blue5 components with the bitmask to make sure they only occupy [4:0].
				);

				// Note:
				// Q0, Q1, Q2 are the final masked and blended color rgb565 components now.
				// Q3, Q7 are free.

				// Combine the final masked and blended RGB565 components.
				asm volatile(
					vector_bitwise_or_16(q1, q1, q2) // Combine the final masked and blended green6 with blue5 components.
					vector_bitwise_or_16(q1, q1, q0) // Combine the final masked and blended green6_blue5 with red5 components.
				);

				// Note:
				// Q1 are the final masked and blended color rgb565 now.
				// Q0, Q2, Q3, Q7 are free.

				// Swap the bytes of the U16 to get the correct transmission byte order.
				asm_vector_swap_16(
					/* src_register = */ q1,
					/* dst_register = */ q1,
					/* tmp_register = */ q7
				);

				// Note:
				// Now Q1 is the final blended and flipped RGB565 color.
				// Q0, Q2, Q3, Q7 are free.

				// Load the color bitmasks of the bitmask to the SIMD vector register.
				if (bitmask_offset_aligned) {
					// Load the aligned color bitmasks to the SIMD vector register.
					asm volatile(vector_load_128_aligned(q2, arg(0), 16) : "+a"(simd_offset_mask));
				} else {
					// Load two parts of unaligned color bitmasks then combine them together in to the SIMD vector register.
					asm volatile(
						vector_load_128_usar			(q3, arg(0),	16)	// Load lower n-bits data from the unaligned bitmask address.
						vector_load_128_usar			(q7, arg(0),	0)	// Load the higher (128-n) bits data from the unaligned bitmask address.
						vector_shift_right_combined_256	(q2, q3,		q7)	// Combine two parts of the data together.
						: /* arg(0) = */ "+a"(simd_offset_mask)
					);
				}

				// Flip the bitmasks if the bitmask is not flipped.
				if (flip_mask) {
					asm_vector_swap_16(
						/* src_register = */ q2,
						/* dst_register = */ q2,
						/* tmp_register = */ q7
					);
				}

				// Note:
				// Now Q2 is the flipped color bitmasks of 8 pixels.
				// Q1 is the final blended and flipped RGB565 color.
				// Q0, Q3, Q7 are free.

				// Load the aligned framebuffer colors to the SIMD vector register again.
				asm volatile(vector_load_128_aligned(q0, arg(0), 0) : "+a"(simd_offset_dst));

				// Note:
				// Now Q0 is the original aligned flipped framebuffer colors.
				// Q1 is the final blended, and flipped RGB565 color.
				// Q2 is the flipped color bitmasks of 8 pixels.
				// Q3, Q7 are free.

				// Mix the framebuffer color and the rectangle color using the bitmask.
				asm volatile(
					vector_bitwise_and_16	(q1, q1, q2)	// Remove invisible colors.
					vector_bitwise_not		(q2, q2)		// Bitwise-NOT the bitmask.
					vector_bitwise_and_16	(q0, q0, q2)	// Remove colors to be overwritten.
					vector_bitwise_or_16	(q0, q0, q1)	// Bitwise-OR to combine the incoming colors and the framebuffer colors.
				);

				// Store the final blended rgb565 colors to the memory.
				// VST.128.IP will increment the %0 operand register automatically by the third operand (16).
				asm volatile(vector_store_128_aligned(q0, arg(0), 16) : "+a"(simd_offset_dst));
			}

			// Manually blending the remaining padding colors until the size is reached.
			for (; x < clipped_size_x; x ++) {
				// Get the flipped original color and the color bitmask of the current pixel.
				const	uint16_t color_dst_flipped = dst_offset		[x];
						uint16_t color_src_bitmask = bitmask_offset	[x];

				// Flip the bitmask if the bitmask is not flipped.
				if (flip_mask) {
					color_src_bitmask =	((color_src_bitmask >> 8U) & 0x00FFU)
					|					((color_src_bitmask << 8U) & 0xFF00U);
				}

				// Flip the LSB and MSB to get the correct RGB565 color.
				const uint16_t color_dst_rgb565 =	((color_dst_flipped >> 8U) & 0x00FFU)
				|									((color_dst_flipped << 8U) & 0xFF00U);

				// Blend the framebuffer color with pre-multiplied incoming colors.
				const uint16_t color_final_rgb565 = private_blend_pre_mul_fast(
					/* r5_src_pre_mul	= */ r5_src_pre_mul,
					/* g6_src_pre_mul	= */ g6_src_pre_mul,
					/* b5_src_pre_mul	= */ b5_src_pre_mul,
					/* a8_src_inv		= */ a8_src_inv,
					/* color_dst_rgb565	= */ color_dst_rgb565
				);

				// Flip the LSB and MSB back to get correct transmission byte order.
				const uint16_t color_final_flipped =	((color_final_rgb565 >> 8U) & 0x00FFU)
				|										((color_final_rgb565 << 8U) & 0xFF00U);

				dst_offset[x] = private_mix_mask_color(
					/* color_dst		= */ color_dst_flipped,
					/* color_src		= */ color_final_flipped,
					/* color_bitmask	= */ color_src_bitmask
				);
			}
		}
	}

	return ESP_OK;
}