#include "stdalign.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_fast_lcd.h"
#include "esp_fast_lcd_common.h"
#include "esp_fast_lcd_common_bitmap.h"

esp_err_t esp_fast_lcd_draw_bitmap(
	const esp_fast_lcd_panel_device_t*	context,
	const int32_t						position_x,
	const int32_t						position_y,
	const uint32_t						size_x,
	const uint32_t						size_y,
	const uint32_t						bitmap_offset_x,
	const uint32_t						bitmap_offset_y,
	const uint32_t						bitmap_size_x,
	const uint8_t						bitmap_pre_multiplied,
	const uint8_t						bitmap_a8_multiplier,
	const uint32_t*						bitmap_rgba8888
) {
	// We cannot proceed without context.
	ESP_RETURN_ON_FALSE(context != NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No esp_fast_lcd_panel_device_t handle provided when performing drawing a translucent bitmap.");

	// Skip the draw if the bitmap has no size.
	if (	size_x == 0
		||	size_y == 0
	) {
		return ESP_OK;
	}

	// Skip the draw if the bitmap is invisible.
	if (bitmap_a8_multiplier == 0U) {
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

	// Calculate the clipped bitmap offset start position of the rectangle.
	const uint32_t clipped_bitmap_start_offset_x = position_x < 0 ? ((uint32_t) (-position_x)) : 0;
	const uint32_t clipped_bitmap_start_offset_y = position_y < 0 ? ((uint32_t) (-position_y)) : 0;

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

	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "LCD panel device \"%s\" is performing an translucent bitmap draw at: positionX=%" PRId32 ", positionY=%" PRId32 ", sizeX=%" PRIu32 ", sizeY=%" PRIu32 ".",
			/* s		*/ properties->configuration.name,
			/* PRId32	*/ position_x,
			/* PRId32	*/ position_y,
			/* PRIu32	*/ size_x,
			/* PRIu32	*/ size_y
		);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	for (uint32_t y = 0U; y < clipped_size_y; y ++) {
		// It's not worthwhile to use SIMD if the clipped_size_x is too short.
		if (clipped_size_x < 16) {
			for	(uint32_t x = 0U; x < clipped_size_x; x ++) {
				// Get the index on the framebuffer at the given coordinate.
				uint32_t pixel_index =	/* index_y = */ (clipped_start_position_y + y) * frame_size_x +
										/* index_x = */ (clipped_start_position_x + x);

				// Get the color of the bitmap at given coordinate.
				const uint32_t color_src_rgba8888 = bitmap_rgba8888[
					(clipped_bitmap_start_offset_y + bitmap_offset_y + y) * bitmap_size_x +
					(clipped_bitmap_start_offset_x + bitmap_offset_x + x)
				];

				// Reserve the all pre-multiplied color components of the RGBA8888 color.
				uint8_t r8_src_pre_mul;
				uint8_t g8_src_pre_mul;
				uint8_t b8_src_pre_mul;
				uint8_t a8_src_inv;

				// Skip the pre-multiplication of the bitmap color if the bitmap is already pre-multiplied.
				if (bitmap_pre_multiplied) {
					// Get the inverted alpha component of the bitmap_pre_multiplied rgba8888.
					a8_src_inv = (uint8_t) ((color_src_rgba8888 >> 0U) & 0xFFU);

					// Skip if the pixel is transparent.
					if (a8_src_inv == 255U) {
						continue;
					}

					// Apply the multiplier to alpha first if it is not opaque.
					if (bitmap_a8_multiplier != 255U) {
						// Invert the a8_src_inv back, apply the multiplier, then invert the alpha again.
						a8_src_inv = 255U - unorm8_mul_exact(bitmap_a8_multiplier, 255U - a8_src_inv);
					}

					// Skip if the pixel is transparent after the multiplier is applied (edge case).
					if (a8_src_inv == 255U) {
						continue;
					}

					// Get the pre-multiplied RGBA8888 color components.
					r8_src_pre_mul = (uint8_t) ((color_src_rgba8888 >> 24U)	& 0xFFU);
					g8_src_pre_mul = (uint8_t) ((color_src_rgba8888 >> 16U)	& 0xFFU);
					b8_src_pre_mul = (uint8_t) ((color_src_rgba8888 >> 8U)	& 0xFFU);

					// Apply the multiplier if it is not opaque.
					if (bitmap_a8_multiplier != 255U) {
						// Apply the multiplier to R/G/B components.
						r8_src_pre_mul = unorm8_mul_exact(bitmap_a8_multiplier, r8_src_pre_mul);
						g8_src_pre_mul = unorm8_mul_exact(bitmap_a8_multiplier, g8_src_pre_mul);
						b8_src_pre_mul = unorm8_mul_exact(bitmap_a8_multiplier, b8_src_pre_mul);
					}
				} else {
					// Get the alpha component of the rgba8888.
					uint8_t a8_src = (uint8_t) ((color_src_rgba8888 >> 0U) & 0xFFU);

					// Skip is the pixel is transparent.
					if (a8_src == 0U) {
						continue;
					}

					// Apply the multiplier if it is not opaque.
					if (bitmap_a8_multiplier != 255U) {
						a8_src = unorm8_mul_exact(bitmap_a8_multiplier, a8_src);
					}

					// Skip is the pixel is transparent after the multiplier is applied (edge case).
					if (a8_src == 0U) {
						continue;
					}

					// Get the R/G/B color components of the rgba8888.
					const uint8_t r8_src = (uint8_t) ((color_src_rgba8888 >> 24U)	& 0xFFU);
					const uint8_t g8_src = (uint8_t) ((color_src_rgba8888 >> 16U)	& 0xFFU);
					const uint8_t b8_src = (uint8_t) ((color_src_rgba8888 >> 8U)	& 0xFFU);

					// Pre-multiply the color now.
					if (a8_src != 255U) {
						// Pre-multiply the color components the RGBA8888 color with the alpha if the alpha is not opaque.
						r8_src_pre_mul	= unorm8_mul_exact(a8_src, r8_src);
						g8_src_pre_mul	= unorm8_mul_exact(a8_src, g8_src);
						b8_src_pre_mul	= unorm8_mul_exact(a8_src, b8_src);
						a8_src_inv		= 255U - a8_src;
					} else {
						// Set the color directly as the pre-multiplied color if the alpha is opaque.
						r8_src_pre_mul	= r8_src;
						g8_src_pre_mul	= g8_src;
						b8_src_pre_mul	= b8_src;
						a8_src_inv		= 0;
					}
				}

				uint8_t r5_final;
				uint8_t g6_final;
				uint8_t b5_final;

				// Write the color directly to the framebuffer if the bitmap is opaque.
				if (a8_src_inv == 0U) {
					// Map them into 5-6-5.
					r5_final = r8_src_pre_mul >> 3U;
					g6_final = g8_src_pre_mul >> 2U;
					b5_final = b8_src_pre_mul >> 3U;
				} else {
					// Get the flipped original color of the pixel from the framebuffer.
					const uint16_t color_flipped = framebuffer[pixel_index];

					// Flip the LSB and MSB to get the correct RGB565 color order.
					const uint16_t color_dst_rgb565 =	((color_flipped >> 8U) & 0x00FFU)
					|									((color_flipped << 8U) & 0xFF00U);

					// Get all color components of rgb565.
					const uint8_t r5_dst = (uint8_t) ((color_dst_rgb565 >> 11U)	& 0b011111U);
					const uint8_t g6_dst = (uint8_t) ((color_dst_rgb565 >> 5U)	& 0b111111U);
					const uint8_t b5_dst = (uint8_t) ((color_dst_rgb565 >> 0U)	& 0b011111U);

					// Map them to 0-255.
					const uint8_t r8_dst = (uint8_t) ((r5_dst << 3U) | (r5_dst >> 2U));
					const uint8_t g8_dst = (uint8_t) ((g6_dst << 2U) | (g6_dst >> 4U));
					const uint8_t b8_dst = (uint8_t) ((b5_dst << 3U) | (b5_dst >> 2U));

					// Mix the incoming color with the original color using painter's algorithm.
					const uint16_t r16 = ((uint16_t) (r8_src_pre_mul)) + ((uint16_t) unorm8_mul_exact(a8_src_inv, r8_dst));
					const uint16_t g16 = ((uint16_t) (g8_src_pre_mul)) + ((uint16_t) unorm8_mul_exact(a8_src_inv, g8_dst));
					const uint16_t b16 = ((uint16_t) (b8_src_pre_mul)) + ((uint16_t) unorm8_mul_exact(a8_src_inv, b8_dst));

					// Clamp the mixed possible 16-bit color back to 0-255.
					const uint8_t r8_final = r16 > 255U ? 255U : ((uint8_t) r16);
					const uint8_t g8_final = g16 > 255U ? 255U : ((uint8_t) g16);
					const uint8_t b8_final = b16 > 255U ? 255U : ((uint8_t) b16);

					// Map the RGB888 into RGB565.
					r5_final = r8_final >> 3U;
					g6_final = g8_final >> 2U;
					b5_final = b8_final >> 3U;
				}

				// Pack them into RGB565 format;
				const uint16_t color_final_rgb565 =	((((uint16_t) r5_final) & 0b011111U) << 11U)
				|									((((uint16_t) g6_final) & 0b111111U) << 5U)
				|									((((uint16_t) b5_final) & 0b011111U) << 0U);

				// Flip the LSB and MSB back to get correct transmission byte order.
				const uint16_t color_final_flipped =	((color_final_rgb565 >> 8U) & 0x00FFU)
				|										((color_final_rgb565 << 8U) & 0xFF00U);

				// Write back the mixed and flipped color to framebuffer.
				framebuffer[pixel_index] = color_final_flipped;
			}
		} else {
			// Alpha multiplier needs to be in 16-bit form instead of 8-bit form.
			const uint16_t bitmap_a8_multiplier_16 = ((uint16_t) bitmap_a8_multiplier) & 0x00FFU;

			// Extract the X cursor out of the loop for SIMD blend optimization.
			uint32_t x = 0;

			// Calculate the offset of the first pixel of the line at the framebuffer.
			uint16_t* dst_offset = &framebuffer[
				(clipped_start_position_y + y) * frame_size_x +
				(clipped_start_position_x + 0)
			];

			// Calculate the offset of the first pixel of the line at bitmap.
			const uint32_t* src_offset = &bitmap_rgba8888[
				/* index_y = */ (clipped_bitmap_start_offset_y + bitmap_offset_y + y) * bitmap_size_x +
				/* index_x = */ (clipped_bitmap_start_offset_x + bitmap_offset_x + 0)
			];

			// Get the colors that we need to fill to reach the next 16-byte aligned address.
			// ">> 1U" means "divided 2" because a color is 2 bytes long as RGB565.
			const uint32_t padding = next_16byte_padding(dst_offset) >> 1U;

			// Blend the padding colors manually if needed.
			// Exit immediately when the padding or the size is reached.
			for (; x < clipped_size_x && x < padding; x ++) {
				// Get the bitmap color and the flipped original color of the pixel.
						uint16_t color_dst_flipped	= dst_offset[x];
				const	uint32_t color_src_rgba8888	= src_offset[x];

				// Flip the LSB and MSB to get the correct RGB565 color.
				const uint16_t color_dst_rgb565 =	((color_dst_flipped >> 8U) & 0x00FFU)
				|									((color_dst_flipped << 8U) & 0xFF00U);

				const uint16_t color_final_rgb565 = private_blend_color_fast_rgba8888(
					/* color_src_rgba8888		= */ color_src_rgba8888,
					/* color_src_a8_multiplier	= */ bitmap_a8_multiplier,
					/* color_src_pre_multiplied	= */ bitmap_pre_multiplied,
					/* color_dst_rgb565			= */ color_dst_rgb565
				);

				// Flip the LSB and MSB to get the correct transmission byte color.
				color_dst_flipped =	((color_final_rgb565 >> 8U) & 0x00FFU)
				|					((color_final_rgb565 << 8U) & 0xFF00U);

				dst_offset[x] = color_dst_flipped;
			}

			// Reserve the 16-byte aligned stack space for offloading the converted pre-multiplied RGB565 color and inverted alpha.
			alignas(16) uint32_t r5_pre_mul_32_low	[4];
			alignas(16) uint32_t r5_pre_mul_32_high	[4];
			alignas(16) uint16_t g6_pre_mul			[8];
			alignas(16) uint16_t b5_pre_mul			[8];
			alignas(16) uint16_t a8_inv_16			[8];

			// Get the SIMD start offset of the framebuffer and the bitmap.
					uint16_t* simd_offset_dst = &dst_offset[x];
			const	uint32_t* simd_offset_src = &src_offset[x];

			// Evaluate the alignment of the bitmap offset.
			uint8_t src_offset_aligned = is_same_align_16byte(simd_offset_dst, simd_offset_src);

			// SIMD is 8 colors a batch (128-bit, 8 16-bit colors).
			for (; x + 8 <= clipped_size_x; x += 8) {
				// Note:
				// First we need to convert the incoming 8 32-bit RGBA8888 colors into 8 RGB565 colors with 8 inverted 8-bit alpha.
				// Convert first:

				// Load the colors of the bitmap to the SIMD vector register.
				if (src_offset_aligned) {
					// Load 8 aligned 32-bit colors to the SIMD vector register.
					asm volatile(vector_load_128_aligned(q3, arg(0), 16) : "+a"(simd_offset_src)); // Load the first 4 32-bit colors from the aligned bitmap address.
					asm volatile(vector_load_128_aligned(q1, arg(0), 16) : "+a"(simd_offset_src)); // Load the second 4 32-bit colors from the aligned bitmap address.
				} else {
					// Load two parts of unaligned color then combine them together in to the SIMD vector register, repeat for two times to get complete 8 32-bit colors.
					asm volatile(
						vector_load_128_usar			(q5, arg(0),	16)	// Load lower n-bits data of the first 4 32-bit colors from the unaligned bitmap address.
						vector_load_128_usar			(q6, arg(0),	16)	// Load the higher (128-n)-bits data of the first 4 32-bit colors and the lower n-bits data of the second 4 32-bit colors from the unaligned bitmap address.
						vector_load_128_usar			(q7, arg(0),	0)	// Load the higher (128-n)-bits data of the second 4 32-bit colors from the unaligned bitmap address.
						vector_shift_right_combined_256	(q3, q5,		q6)	// Combine two parts of the first 4 32-bit color data together.
						vector_shift_right_combined_256	(q1, q6,		q7)	// Combine two parts of the second 4 32-bit color data together.
						: /* arg(0) = */ "+a"(simd_offset_src)
					);
				}

				// Extract 8 32-bit RGBA colors into RG and BA parts.
				asm volatile(vector_unzip_unshuffle_16(q3, q1));

				// Note:
				// Now Q3 is the blue-alpha part of the 8 32-bit RGBA colors.
				// Now Q1 is the red-green part of the 8 32-bit RGBA colors.
				// Q0, Q2, Q4, Q5, Q6, Q7 are free.

				// Extract the all color components of RGBA8888 separately from Q1 and Q3.
				asm volatile(
					set_shift_amount		(arg(0))		// Right shift on the higher 8-bit by 8 bits, set to 8.
					vector_broadcast_16		(q7, arg(1))	// Broadcast the ones to the SIMD vector register.
					vector_multiply_u16		(q0, q1, q7)	// Right shift the 8-bit red parts by 8 bits to extract 8-bit red components.
					vector_multiply_u16		(q2, q3, q7)	// Right shift the 8-bit blue part by 8 bits to extract 8-bit blue components.
					vector_broadcast_16		(q7, arg(2))	// Broadcast the lower 8-bit mask (LSB) of 16-bit data to clear remaining red and blue bits in Q1 and q3.
					vector_bitwise_and_16	(q3, q3, q7)	// Clear the remaining blue bits.
					vector_bitwise_and_16	(q1, q1, q7)	// Clear the remaining red bits.
					:
					:	/* arg(0) = */ "a"(8),						// The value 8 set to SAR to shift the red and blue parts 8-bits right
						/* arg(1) = */ "a"(&value_one),				// The value 1 broadcasted to the SIMD vector register to prevent left shifting.
						/* arg(2) = */ "a"(&uint16_lsb_8_bitmask)	// The address of the lower 8-bit (LSB) bitmask of uint16_6.
				);

				// Note:
				// Now Q0 is the red components of the 8 32-bit RGBA colors.
				// Now Q1 is the green components of the 8 32-bit RGBA colors.
				// Now Q2 is the blue components of the 8 32-bit RGBA colors.
				// Now Q3 is the alpha components of the 8 32-bit RGBA colors.
				// Q4, Q5, Q6, Q7 are free.

				// Pre-multiply the RGBA8888 color if the bitmap is not pre-multiplied.
				// The SAR is already 8 (divided by 256) now, no need to set the SAR again, results of the multiplication
				// will be automatically divided by 256.
				if (!bitmap_pre_multiplied) {
					// Only apply the multiplier when it is not opaque.
					if (bitmap_a8_multiplier != 255U) {
						// Apply the multiplier to only the alpha if the color is not pre-multiplied.
						asm volatile(
							vector_broadcast_16(q7, arg(0)) // Broadcast the multiplier to the SIMD vector register to apply the multiplier.
							vector_multiply_u16(q3, q3, q7) // Multiply the multiplier with alpha to apply the multiplier.
							:
							: /* arg(0) = */ "a"(&bitmap_a8_multiplier_16) // The multiplier broadcasted to the SIMD vector register.
						);
					}

					// Pre-multiply the 8 8-bit R/G/B components with its alpha.
					asm volatile(
						vector_multiply_u16(q0, q0, q3) // Pre-multiply the 8-bit red components with its alpha (q0 = (q0 * q3) / 256).
						vector_multiply_u16(q1, q1, q3) // Pre-multiply the 8-bit green components with its alpha (q1 = (q1 * q3) / 256).
						vector_multiply_u16(q2, q2, q3) // Pre-multiply the 8-bit blue components with its alpha (q2 = (q2 * q3) / 256).
						vector_broadcast_16(q7, arg(0)) // Broadcast the 255 to the SIMD vector register to invert the alpha.
						vector_subtract_s16(q3, q7, q3) // Invert the alpha component of the color.
						:
						: /* arg(0) = */ "a"(&value_255) // The value 255 broadcasted to the SIMD vector register to invert alpha component.
					);
				} else {
					// Only apply the multiplier when it is not opaque.
					if (bitmap_a8_multiplier != 255U) {
						// Apply the multiplier to the pre-multiplied R/G/B components and inverted alpha component.
						asm volatile(
							vector_broadcast_16(q7, arg(0)) // Broadcast the multiplier to the SIMD vector register to apply the multiplier.
							vector_multiply_u16(q0, q0, q7) // Apply the multiplier to 8-bit red components.
							vector_multiply_u16(q1, q1, q7) // Apply the multiplier to 8-bit green components.
							vector_multiply_u16(q2, q2, q7) // Apply the multiplier to 8-bit blue components.
							vector_broadcast_16(q6, arg(1)) // Broadcast the 255 to the SIMD vector register to invert the alpha.
							vector_subtract_s16(q3, q6, q3) // Invert the alpha component back to non-inverted alpha.
							vector_multiply_u16(q3, q3, q7) // Apply the multiplier to 8-bit alpha components.
							vector_subtract_s16(q3, q6, q3) // Invert the alpha component again to inverted alpha.
							:
							:	/* arg(0) = */ "a"(&bitmap_a8_multiplier_16),	// The multiplier broadcasted to the SIMD vector register.
								/* arg(1) = */ "a"(&value_255)					// The value 255 broadcasted to the SIMD vector register to invert alpha component.
						);
					}
				}

				// Note:
				// Now Q0 is the pre-multiplied red components of the 8 32-bit RGBA colors.
				// Now Q1 is the pre-multiplied green components of the 8 32-bit RGBA colors.
				// Now Q2 is the pre-multiplied blue components of the 8 32-bit RGBA colors.
				// Now Q3 is the inverted alpha components of the 8 32-bit RGBA colors.
				// Q4, Q5, Q6, Q7 are free.

				// Convert the RGBA8888 color components to RGB565 color formats.
				asm volatile(
					set_shift_amount	(arg(0))		// Set the shift amount to 0 to convert red and green components (prevent right shifting).
					vector_broadcast_16	(q6, arg(1))	// Broadcast the equivalent multiplier of shifting 8 bits left to the SIMD vector register.
					vector_broadcast_16	(q7, arg(2))	// Broadcast the equivalent multiplier of shifting 3 bits left to the SIMD vector register.
					vector_multiply_u16	(q0, q0, q6)	// Left shift the red components by 8 bits. Now the higher 5-bits of the red components is started from bit 11.
					vector_multiply_u16	(q1, q1, q7)	// Left shift the green components by 3 bits. Now the higher 6-bits of the green components is started from bit 5.
					vector_broadcast_16	(q7, arg(3))	// Broadcast the ones to the SIMD vector register.
					set_shift_amount	(arg(4))		// Set the shift amount to 3 to convert the blue components.
					vector_multiply_u16	(q2, q2, q7)	// Right shift the blue components by 3 bits. Now the remaining 5-bits of the blue components is started from bit 0.
					:
					:	/* arg(0) = */ "a"(0),				// The value 0 set to SAR to prevent right shifting.
						/* arg(1) = */ "a"(&left_shift_8),	// The value 256 broadcasted to the SIMD vector register to shift 8-bits left.
						/* arg(2) = */ "a"(&left_shift_3),	// The value 8 broadcasted to the SIMD vector register to shift 3-bits left.
						/* arg(3) = */ "a"(&value_one),		// The value 1 broadcasted to the SIMD vector register to prevent left shifting.
						/* arg(4) = */ "a"(3)				// The value 3 set to SAR to shift the higher 3-bits right.
				);

				// Load the RGB565 color components masks for clearing the out-of-range bits.
				// Load three masks to Q4, Q5, and Q6 for convenience, used by blending.
				asm volatile(
					vector_broadcast_16(q4, arg(0)) // Broadcast the red5 component mask to the SIMD vector register.
					vector_broadcast_16(q5, arg(1)) // Broadcast the green6 component mask to the SIMD vector register.
					vector_broadcast_16(q6, arg(2)) // Broadcast the blue5 component mask to the SIMD vector register.
					:
					:	/* arg(0) = */ "a"(&rgb565_r5_bitmask),
						/* arg(1) = */ "a"(&rgb565_g6_bitmask),
						/* arg(2) = */ "a"(&rgb565_b5_bitmask)
				);

				// Clearing the out-of-range bits of the converted RGB565 color components.
				asm volatile(
					vector_bitwise_and_16(q0, q0, q4) // Clearing the out-of-range bits of the 5-bit red components.
					vector_bitwise_and_16(q1, q1, q5) // Clearing the out-of-range bits of the 6-bit green components.
					vector_bitwise_and_16(q2, q2, q6) // (Maybe unnecessary?) Clearing the out-of range bits of the 5-bit blue components.
				);

				// Blending requires red components to be 32-bits long, expand the q0 to q0 (low) + q7 (high).
				asm volatile(
					vector_clear_zero		(q7)		// Fill a vector register with zeros for the shuffle.
					vector_zip_shuffle_16	(q0, q7)	// Shuffle the red components with the zero vectors to expand the red5 components to 32-bit.
				);

				// Offloading the converted pre-multiplied color and inverted alpha for blending.
				asm volatile(
					vector_store_128_aligned(q0, arg(0), 0) // Offloading the lower part of the 32-bit form of converted pre-multiplied 5-bit red components.
					vector_store_128_aligned(q7, arg(1), 0) // Offloading the higher part of the 32-bit form of converted pre-multiplied 5-bit red components.
					vector_store_128_aligned(q1, arg(2), 0) // Offloading the converted pre-multiplied 6-bit green components.
					vector_store_128_aligned(q2, arg(3), 0) // Offloading the converted pre-multiplied 5-bit blue components.
					vector_store_128_aligned(q3, arg(4), 0) // Offloading the inverted 8-bit inverted alpha components in 16-bit form.
					:
					:	/* arg(0) = */ "a"(r5_pre_mul_32_low),	// The offload stack address of lower part of the 32-bit form of 5-bit red components.
						/* arg(1) = */ "a"(r5_pre_mul_32_high),	// The offload stack address of higher part of the 32-bit form of 5-bit red components.
						/* arg(2) = */ "a"(g6_pre_mul),			// The offload stack address of 6-bit green components.
						/* arg(3) = */ "a"(b5_pre_mul),			// The offload stack address of 5-bit blue components.
						/* arg(4) = */ "a"(a8_inv_16)			// The offload stack address of 8-bit inverted alpha components in 16-bit form.
				);

				// Note:
				// We have finished the conversion, now we can blend the color together.
				// The converted result has been offloaded to stack, so:
				// Q4, Q5, Q6 are masks of color components of RGB565.
				// Q0, Q1, Q2, Q3, Q7 are free.

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

				// Load the inverted alpha to be multiplied to be original color to the SIMD vector register.
				asm volatile(vector_load_128_aligned(q7, arg(0), 0) :: "a"(a8_inv_16));

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

				// Note:
				// Q0 is the lower part of the expanded 32-bit red5 components now.
				// Q3 is the higher part of the expanded 32-bit red5 components now.
				// Q1, Q2 is the pre-multiplied original color now.
				// Q7 is free.

				asm volatile(
					vector_load_128_aligned		(q7, arg(0),	0)	// Load the lower part of the pre-multiplied 32bit form of red5 components of the incoming color to the SIMD vector register.
					vector_add_s32				(q0, q0,		q7)	// Add the lower part of the pre-multiplied incoming colors to the lower part of the red5 components of the pre-multiplied original color.
					vector_load_128_aligned		(q7, arg(1),	0)	// Load the higher part of the pre-multiplied 32bit form of red5 components of the incoming color to the SIMD vector register.
					vector_add_s32				(q3, q3,		q7)	// Add the higher part of the pre-multiplied incoming colors to the higher part of the red5 components of the pre-multiplied original color.
					vector_unzip_unshuffle_16	(q0, q3)			// Un-shuffle the 32-bit blended red5 components, all data will go into Q0 and Q3 will theoretically be zeros.
					:
					:	/* arg(0) = */ "a"(r5_pre_mul_32_low),
						/* arg(1) = */ "a"(r5_pre_mul_32_high)
				);

				// Note:
				// Q0 is the blended red5 components now.
				// Q1, Q2 is the pre-multiplied original green6 and blue 5 components now.
				// Q3, Q7 are free.

				// Blend the multiplied original green6/blue5 components with the incoming green6/blue5 components.
				asm volatile(
					vector_load_128_aligned	(q7, arg(0), 0)	// Load the pre-multiplied green6 components of the incoming color to the SIMD vector register.
					vector_load_128_aligned	(q3, arg(1), 0)	// Load the pre-multiplied blue5 components of the incoming color to the SIMD vector register.
					vector_add_s16			(q1, q1, q7)	// Add the pre-multiplied incoming colors to green6 components of the pre-multiplied original color.
					vector_add_s16			(q2, q2, q3)	// Add the pre-multiplied incoming colors to blue5 components of the pre-multiplied original color.
					:
					:	/* arg(0) = */ "a"(g6_pre_mul),
						/* arg(1) = */ "a"(b5_pre_mul)
				);

				// Note:
				// Q1, Q2 are the blended green6 and blue5 components now.
				// Q0 is the blended red5 components.
				// Q3, Q7 are free.

				// Ensure the blended color components only occupy their own range of bits.
				asm volatile(
					vector_bitwise_and_16(q0, q0, q4) // Bitwise-AND the blended red5 components with the mask to make sure they only occupy [15:11].
					vector_bitwise_and_16(q1, q1, q5) // Bitwise-AND the blended green6 components with the mask to make sure they only occupy [10:5].
					vector_bitwise_and_16(q2, q2, q6) // Bitwise-AND the blended blue5 components with the mask to make sure they only occupy [4:0].
				);

				// Note:
				// Q0, Q1, Q2 are the final masked and blended color rgb565 components now.
				// Q3, Q7 are free.

				// Combine the final masked and blended RGB565 components.
				asm volatile(
					vector_bitwise_or_16(q2, q2, q1) // Combine the final masked and blended green6 with blue5 components.
					vector_bitwise_or_16(q2, q2, q0) // Combine the final masked and blended green6+blue5 with red5 components.
				);

				asm_vector_swap_16(
					/* src_register = */ q2,
					/* dst_register = */ q3,
					/* tmp_register = */ q7
				);

				// Store the final blended rgb565 colors to the memory.
				// VST.128.IP will increment the %0 operand register automatically by the third operand (16).
				asm volatile(vector_store_128_aligned(q3, arg(0), 16) : "+a"(simd_offset_dst));
			}

			// Manually blending the remaining padding colors until the size is reached.
			for (; x < clipped_size_x; x ++) {
				// Get the bitmap color and the flipped original color of the pixel.
						uint16_t color_dst_flipped	= dst_offset[x];
				const	uint32_t color_src_rgba8888	= src_offset[x];

				// Flip the LSB and MSB to get the correct RGB565 color.
				const uint16_t color_dst_rgb565 =	((color_dst_flipped >> 8U) & 0x00FFU)
				|									((color_dst_flipped << 8U) & 0xFF00U);

				const uint16_t color_final_rgb565 = private_blend_color_fast_rgba8888(
					/* color_src_rgba8888		= */ color_src_rgba8888,
					/* color_src_a8_multiplier	= */ bitmap_a8_multiplier,
					/* color_src_pre_multiplied	= */ bitmap_pre_multiplied,
					/* color_dst_rgb565			= */ color_dst_rgb565
				);

				// Flip the LSB and MSB to get the correct transmission byte color.
				color_dst_flipped =	((color_final_rgb565 >> 8U) & 0x00FFU)
				|					((color_final_rgb565 << 8U) & 0xFF00U);

				dst_offset[x] = color_dst_flipped;
			}
		}
	}

	return ESP_OK;
}

esp_err_t esp_fast_lcd_draw_bitmap_rgb565_pre_mul_a8_inv(
	const esp_fast_lcd_panel_device_t*	context,
	const int32_t						position_x,
	const int32_t						position_y,
	const uint32_t						size_x,
	const uint32_t						size_y,
	const uint32_t						bitmap_offset_x,
	const uint32_t						bitmap_offset_y,
	const uint32_t						bitmap_size_x,
	const uint8_t						bitmap_flipped,
	const uint8_t						bitmap_a8_multiplier,
	const uint16_t*						bitmap_rgb565_pre_mul,
	const uint16_t*						bitmap_a8_inv
) {
	// We cannot proceed without context.
	ESP_RETURN_ON_FALSE(context != NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No esp_fast_lcd_panel_device_t handle provided when performing drawing a translucent native bitmap.");

	// Skip the draw if the bitmap has no size.
	if (	size_x == 0
		||	size_y == 0
	) {
		return ESP_OK;
	}

	// Skip the draw if the bitmap is invisible.
	if (bitmap_a8_multiplier == 0U) {
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

	// Calculate the clipped bitmap offset start position of the rectangle.
	const uint32_t clipped_bitmap_start_offset_x = position_x < 0 ? ((uint32_t) (-position_x)) : 0;
	const uint32_t clipped_bitmap_start_offset_y = position_y < 0 ? ((uint32_t) (-position_y)) : 0;

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

	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "LCD panel device \"%s\" is performing an translucent native bitmap draw at: positionX=%" PRId32 ", positionY=%" PRId32 ", sizeX=%" PRIu32 ", sizeY=%" PRIu32 ".",
			/* s		*/ properties->configuration.name,
			/* PRId32	*/ position_x,
			/* PRId32	*/ position_y,
			/* PRIu32	*/ size_x,
			/* PRIu32	*/ size_y
		);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	for (uint32_t y = 0U; y < clipped_size_y; y ++) {
		// It's not worthwhile to use SIMD if the clipped_size_x is too short.
		if (clipped_size_x < 16) {
			for	(uint32_t x = 0U; x < clipped_size_x; x ++) {
				// Get the index on the framebuffer at the given coordinate.
				uint32_t pixel_index =	/* index_y = */ (clipped_start_position_y + y) * frame_size_x +
										/* index_x = */ (clipped_start_position_x + x);

				// Get the index of the bitmap at given coordinate.
				const uint32_t bitmap_index =	/* index_y = */ (clipped_bitmap_start_offset_y + bitmap_offset_y + y) * bitmap_size_x +
												/* index_x = */ (clipped_bitmap_start_offset_x + bitmap_offset_x + x);

				// Get the color of the bitmap at given coordinate.
				uint16_t color_src_rgb565_pre_mul = bitmap_rgb565_pre_mul[bitmap_index];

				// Flip the color back to get correct color order if the bitmap is flipped.
				if (bitmap_flipped) {
					color_src_rgb565_pre_mul = 	((color_src_rgb565_pre_mul >> 8U) & 0x00FFU)
					|							((color_src_rgb565_pre_mul << 8U) & 0xFF00U);
				}

				// Get the inverted alpha of the bitmap at given coordinate or defaulted to 0.
				const uint16_t color_src_a8_inv = bitmap_a8_inv != NULL ? bitmap_a8_inv[bitmap_index] : 0u;

				// Skip if the pixel is transparent.
				if (color_src_a8_inv == 255U) {
					continue;
				}

				// Reserve the final blended RGB565 color.
				uint16_t color_final_rgb565;

				// Use the pre-multiplied color directly if the pixel is opaque.
				if (color_src_a8_inv == 0U && bitmap_a8_multiplier == 255U) {
					color_final_rgb565 = color_src_rgb565_pre_mul;
				} else {
					// Get the flipped original color of the pixel from the framebuffer.
					const uint16_t color_dst_flipped = framebuffer[pixel_index];

					// Flip the LSB and MSB to get the correct RGB565 color order.
					uint16_t color_dst_rgb565 =	((color_dst_flipped >> 8U) & 0x00FFU)
					|							((color_dst_flipped << 8U) & 0xFF00U);

					// Blend the framebuffer color with the pre-multiplied color and the inverted-alpha.
					color_final_rgb565 = private_blend_color_fast_rgb565_pre_mul(
						/* color_src_a8_inv			= */ color_src_a8_inv,
						/* color_src_a8_multiplier	= */ bitmap_a8_multiplier,
						/* color_src_rgb565_pre_mul	= */ color_src_rgb565_pre_mul,
						/* color_dst_rgb565			= */ color_dst_rgb565
					);
				}

				// Flip the LSB and MSB back to get correct transmission byte order.
				const uint16_t color_flipped =	((color_final_rgb565 >> 8U) & 0x00FFU)
				|								((color_final_rgb565 << 8U) & 0xFF00U);

				// Write back the mixed and flipped color to framebuffer.
				framebuffer[pixel_index] = color_flipped;
			}
		} else {
			// Alpha multiplier needs to be in 16-bit form instead of 8-bit form.
			const uint16_t bitmap_a8_multiplier_16 = ((uint16_t) bitmap_a8_multiplier) & 0x00FFU;

			// Extract the X cursor out of the loop for SIMD blend optimization.
			uint32_t x = 0;

			// Calculate the offset of the first pixel of the line at the framebuffer.
			uint16_t* dst_offset = &framebuffer[
				(clipped_start_position_y + y) * frame_size_x +
				(clipped_start_position_x + 0)
			];

			const uint32_t bitmap_index =	/* index_y = */ (clipped_bitmap_start_offset_y + bitmap_offset_y + y) * bitmap_size_x +
											/* index_x = */ (clipped_bitmap_start_offset_x + bitmap_offset_x + 0);

			// Calculate the offset of the first pixel of the line at color and alpha bitmap.
			const uint16_t* color_offset =							&bitmap_rgb565_pre_mul	[bitmap_index];
			const uint16_t* alpha_offset = bitmap_a8_inv != NULL ?	&bitmap_a8_inv			[bitmap_index] : NULL;

			// Get the colors that we need to fill to reach the next 16-byte aligned address.
			// ">> 1U" means "divided 2" because a color is 2 bytes long as RGB565.
			const uint32_t padding = next_16byte_padding(dst_offset) >> 1U;

			// Blend the padding colors manually if needed.
			// Exit immediately when the padding or the size is reached.
			for (; x < clipped_size_x && x < padding; x ++) {
				// Get the bitmap color and the flipped original color of the pixel.
						uint16_t color_dst_flipped			=							dst_offset	[x];
						uint16_t color_src_rgb565_pre_mul	=							color_offset[x];
				const	uint16_t color_src_alpha_inv		= alpha_offset != NULL ?	alpha_offset[x] : 0U;

				// Flip the color back to get correct color order if the bitmap is flipped.
				if (bitmap_flipped) {
					color_src_rgb565_pre_mul = 	((color_src_rgb565_pre_mul >> 8U) & 0x00FFU)
					|							((color_src_rgb565_pre_mul << 8U) & 0xFF00U);
				}

				// Flip the LSB and MSB to get the correct RGB565 color.
				const uint16_t color_dst_rgb565 =	((color_dst_flipped >> 8U) & 0x00FFU)
				|									((color_dst_flipped << 8U) & 0xFF00U);

				const uint16_t color_final_rgb565 = private_blend_color_fast_rgb565_pre_mul(
					/* color_src_a8_inv			= */ (uint8_t) color_src_alpha_inv,
					/* color_src_a8_multiplier	= */ bitmap_a8_multiplier,
					/* color_src_rgb565_pre_mul	= */ color_src_rgb565_pre_mul,
					/* color_dst_rgb565			= */ color_dst_rgb565
				);

				// Flip the LSB and MSB to get the correct transmission byte color.
				color_dst_flipped =	((color_final_rgb565 >> 8U) & 0x00FFU)
				|					((color_final_rgb565 << 8U) & 0xFF00U);

				dst_offset[x] = color_dst_flipped;
			}

			// Broadcast the bitmasks of the RGB565 format to the SIMD vector registers.
			asm volatile(
				vector_broadcast_16(q4, arg(0)) // Broadcast the red5 component mask to the SIMD vector register.
				vector_broadcast_16(q5, arg(1)) // Broadcast the green6 component mask to the SIMD vector register.
				vector_broadcast_16(q6, arg(2)) // Broadcast the blue5 component mask to the SIMD vector register.
				:
				:	/* arg(0) = */ "a"(&rgb565_r5_bitmask),
					/* arg(1) = */ "a"(&rgb565_g6_bitmask),
					/* arg(2) = */ "a"(&rgb565_b5_bitmask)
			);

			// Note:
			// Q4, Q5, Q6 are bitmasks of color components of RGB565.
			// Q0, Q1, Q2, Q3, Q7 are free.

			// Reserve the 16-byte aligned stack space for offloading the converted pre-multiplied RGB565 color.
			alignas(16) uint32_t r5_pre_mul_32_low	[4];
			alignas(16) uint32_t r5_pre_mul_32_high	[4];
			alignas(16) uint16_t g6_pre_mul			[8];
			alignas(16) uint16_t b5_pre_mul			[8];

			// Get the SIMD start offset of the framebuffer and the bitmap.
					uint16_t* simd_offset_dst	=							&dst_offset		[x];
			const	uint16_t* simd_offset_color	=							&color_offset	[x];
			const	uint16_t* simd_offset_alpha	= alpha_offset != NULL ?	&alpha_offset	[x] : NULL;

			// Evaluate the alignment of the bitmap offsets.
			uint8_t color_offset_aligned = is_same_align_16byte(simd_offset_dst, simd_offset_color);
			uint8_t alpha_offset_aligned = is_same_align_16byte(simd_offset_dst, simd_offset_alpha);

			// SIMD is 8 colors a batch (128-bit, 8 16-bit colors).
			for (; x + 8 <= clipped_size_x; x += 8) {
				// Note:
				// First we need to extract the incoming 8 pre-multiplied RGB565 into 8 red5<<11, green6<<5, blue5<<0.
				// Extract first:

				// Load the colors of the bitmap to the SIMD vector register.
				if (color_offset_aligned) {
					// Load 8 aligned 16-bit colors to the SIMD vector register.
					asm volatile(vector_load_128_aligned(q3, arg(0), 16) : "+a"(simd_offset_color));
				} else {
					// Load two parts of unaligned color then combine them together in to the SIMD vector register.
					asm volatile(
						vector_load_128_usar			(q1, arg(0),	16)	// Load lower n-bits data from the unaligned color address.
						vector_load_128_usar			(q2, arg(0),	0)	// Load the higher (128-n)-bits data from the unaligned color address.
						vector_shift_right_combined_256	(q3, q1,		q2)	// Combine two parts of the data together.
						: /* arg(0) = */ "+a"(simd_offset_color)
					);
				}

				// Flip the bitmap color back to get the correct RGB565 color order if the bitmap is flipped.
				if (bitmap_flipped) {
					asm_vector_swap_16(
						/* src_register = */ q3,
						/* dst_register = */ q3,
						/* tmp_register = */ q2
					);
				}

				// Note:
				// Now Q3 is the 8 bitmap pre-multiplied RGB565 colors.
				// Q0, Q1, Q2, Q7 are free.

				// Extract the color components from RGB565.
				asm volatile(
					vector_bitwise_and_16(q0, q3, q4) // Extract the red5 component from the RGB565.
					vector_bitwise_and_16(q1, q3, q5) // Extract the green6 component from the RGB565.
					vector_bitwise_and_16(q2, q3, q6) // Extract the blue5 component from the RGB565.
				);

				// Note:
				// Q0, Q1, Q2 is the extracted color components now.
				// Q3, Q7 are free.

				// Set the shift amount register to 8 to shift the multiplication result 8 bits right.
				// aka. divided by 256. (to get approximately correct unorm8 value after multiplying the inverted alpha.)
				asm volatile(set_shift_amount(arg(0)) :: "a"(8));

				// Only apply the multiplier when it is not opaque.
				if (bitmap_a8_multiplier != 255U) {
					// Apply the multiplier to the pre-multiplied R/G/B components.
					asm volatile(
						vector_broadcast_16	(q7, arg(0)) // Broadcast the multiplier to the SIMD vector register to apply the multiplier.
						vector_multiply_u16	(q0, q0, q7) // Apply the multiplier to 8-bit red components.
						vector_multiply_u16	(q1, q1, q7) // Apply the multiplier to 8-bit green components.
						vector_multiply_u16	(q2, q2, q7) // Apply the multiplier to 8-bit blue components.
						:
						:	/* arg(0) = */ "a"(&bitmap_a8_multiplier_16) // The multiplier broadcasted to the SIMD vector register.
					);
				}

				// Note:
				// Q0, Q1, Q2 is the extracted color components after the multiplier is applied now.
				// Q3, Q7 are free.

				// Blending requires red components to be 32-bits long, expand the q0 to q0 (low) + q7 (high).
				asm volatile(
					vector_clear_zero		(q7)		// Fill a vector register with zeros for the shuffle.
					vector_zip_shuffle_16	(q0, q7)	// Shuffle the red components with the zero vectors to expand the red5 components to 32-bit.
				);

				// Note:
				// Q0 and Q7 are the lower part and higher part of the red5 components in 32-bit form.
				// Q1, Q2 are the extracted green6 and blue5 color components.
				// Q3 is free.

				// Offloading the converted pre-multiplied color and inverted alpha for blending.
				asm volatile(
					vector_store_128_aligned(q0, arg(0), 0) // Offloading the lower part of the 32-bit form of converted pre-multiplied 5-bit red components.
					vector_store_128_aligned(q7, arg(1), 0) // Offloading the higher part of the 32-bit form of converted pre-multiplied 5-bit red components.
					vector_store_128_aligned(q1, arg(2), 0) // Offloading the converted pre-multiplied 6-bit green components.
					vector_store_128_aligned(q2, arg(3), 0) // Offloading the converted pre-multiplied 5-bit blue components.
					:
					:	/* arg(0) = */ "a"(r5_pre_mul_32_low),	// The offload stack address of lower part of the 32-bit form of 5-bit red components.
						/* arg(1) = */ "a"(r5_pre_mul_32_high),	// The offload stack address of higher part of the 32-bit form of 5-bit red components.
						/* arg(2) = */ "a"(g6_pre_mul),			// The offload stack address of 6-bit green components.
						/* arg(3) = */ "a"(b5_pre_mul)			// The offload stack address of 5-bit blue components.
				);

				// Note:
				// Q4, Q5, Q6 are masks of color components of RGB565.
				// Q0, Q1, Q2, Q3, Q7 are free.

				// Separate the multiplier apply of the alpha from the RGB because we have no registers left (Q4, Q5, Q6 are masks).
				// Load the inverted alpha to be multiplied to be original color to the SIMD vector register.
				if (simd_offset_alpha != NULL) {
					// Load from the alpha bitmap if it is not
					if (alpha_offset_aligned) {
						// Load 8 aligned 8-bit inverted alpha component in 16-bit form to the SIMD vector register.
						asm volatile(vector_load_128_aligned(q7, arg(0), 16) : "+a"(simd_offset_alpha));
					} else {
						// Load two parts of unaligned inverted alpha then combine them together in to the SIMD vector register.
						asm volatile(
							vector_load_128_usar			(q3, arg(0),	16)	// Load lower n-bits data from the unaligned inverted alpha address.
							vector_load_128_usar			(q7, arg(0),	0)	// Load the higher (128-n)-bits data from the unaligned inverted alpha address.
							vector_shift_right_combined_256	(q7, q3,		q7)	// Combine two parts of the data together.
							: /* arg(0) = */ "+a"(simd_offset_alpha)
						);
					}
				} else {
					// Fill the inverted alpha with 0 (equivalent to alpha 255) if the alpha bitmap does not exist.
					asm volatile(vector_clear_zero(q7));
				}

				// Note:
				// Q7 is the inverted alpha component now.
				// Q4, Q5, Q6 are masks of color components of RGB565.
				// Q0, Q1, Q2, Q3 are free.

				// Only apply the multiplier when it is not opaque.
				if (bitmap_a8_multiplier != 255U) {
					// Apply the multiplier to the pre-multiplied R/G/B components.
					// The SAR is already 8 (divided by 256) now, no need to set the SAR again, results of the multiplication
					// will be automatically divided by 256.
					asm volatile(
						vector_broadcast_16(q2, arg(0)) // Broadcast the multiplier to the SIMD vector register to apply the multiplier.
						vector_broadcast_16(q3, arg(1)) // Broadcast the 255 to the SIMD vector register to invert the alpha.
						vector_subtract_s16(q7, q3, q7) // Invert the alpha component back to non-inverted alpha.
						vector_multiply_u16(q7, q2, q7) // Apply the multiplier to 8-bit alpha components.
						vector_subtract_s16(q7, q3, q7) // Invert the alpha component again to inverted alpha.
						:
						:	/* arg(0) = */ "a"(&bitmap_a8_multiplier_16),	// The multiplier broadcasted to the SIMD vector register.
							/* arg(1) = */ "a"(&value_255)					// The value 255 broadcasted to the SIMD vector register to invert alpha component.
					);
				}

				// Note:
				// We have finished the extraction, now we can blend the color together.
				// The converted result has been offloaded to stack, so:
				// Q7 is the alpha after the multiplier is applied now.
				// Q4, Q5, Q6 are masks of color components of RGB565.
				// Q0, Q1, Q2, Q3 are free.

				// Load 8 original colors from the framebuffer to the vector register.
				// VLD.128.IP will increment the %0 operand register automatically by the third operand (0).
				asm volatile(vector_load_128_aligned(q3, arg(0), 0) : "+a"(simd_offset_dst));

				// Swap the bytes of the U16 to get the correct RGB565 color order.
				asm_vector_swap_16(
					/* src_register = */ q3,
					/* dst_register = */ q3,
					/* tmp_register = */ q2
				);

				// Extract the color components from RGB565.
				asm volatile(
					vector_bitwise_and_16(q0, q3, q4) // Extract the red5 component from the RGB565.
					vector_bitwise_and_16(q1, q3, q5) // Extract the green6 component from the RGB565.
					vector_bitwise_and_16(q2, q3, q6) // Extract the blue5 component from the RGB565.
				);

				// Note:
				// Q0, Q1, Q2 is the original color now.
				// Q7 is the alpha after the multiplier is applied.
				// Q3 is free.

				// Multiply the original RGB 565 color components with the inverted alpha then divided by 256.
				// The SAR is already 8 (divided by 256) now, no need to set the SAR again, results of the multiplication
				// will be automatically divided by 256.
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

				// Note:
				// Q0 is the lower part of the expanded 32-bit red5 components now.
				// Q3 is the higher part of the expanded 32-bit red5 components now.
				// Q1, Q2 is the pre-multiplied original color now.
				// Q7 is free.

				asm volatile(
					vector_load_128_aligned		(q7, arg(0),	0)	// Load the lower part of the pre-multiplied 32bit form of red5 components of the incoming color to the SIMD vector register.
					vector_add_s32				(q0, q0,		q7)	// Add the lower part of the pre-multiplied incoming colors to the lower part of the red5 components of the pre-multiplied original color.
					vector_load_128_aligned		(q7, arg(1),	0)	// Load the higher part of the pre-multiplied 32bit form of red5 components of the incoming color to the SIMD vector register.
					vector_add_s32				(q3, q3,		q7)	// Add the higher part of the pre-multiplied incoming colors to the higher part of the red5 components of the pre-multiplied original color.
					vector_unzip_unshuffle_16	(q0, q3)			// Un-shuffle the 32-bit blended red5 components, all data will go into Q0 and Q3 will theoretically be zeros.
					:
					:	/* arg(0) = */ "a"(r5_pre_mul_32_low),
						/* arg(1) = */ "a"(r5_pre_mul_32_high)
				);

				// Note:
				// Q0 is the blended red5 components now.
				// Q1, Q2 is the pre-multiplied original green6 and blue 5 components now.
				// Q3, Q7 are free.

				// Blend the multiplied original green6/blue5 components with the incoming green6/blue5 components.
				asm volatile(
					vector_load_128_aligned	(q7, arg(0), 0)	// Load the pre-multiplied green6 components of the incoming color to the SIMD vector register.
					vector_load_128_aligned	(q3, arg(1), 0)	// Load the pre-multiplied blue5 components of the incoming color to the SIMD vector register.
					vector_add_s16			(q1, q1, q7)	// Add the pre-multiplied incoming colors to green6 components of the pre-multiplied original color.
					vector_add_s16			(q2, q2, q3)	// Add the pre-multiplied incoming colors to blue5 components of the pre-multiplied original color.
					:
					:	/* arg(0) = */ "a"(g6_pre_mul),
						/* arg(1) = */ "a"(b5_pre_mul)
				);

				// Note:
				// Q1, Q2 are the blended green6 and blue5 components now.
				// Q0 is the blended red5 components.
				// Q3, Q7 are free.

				// Ensure the blended color components only occupy their own range of bits.
				asm volatile(
					vector_bitwise_and_16(q0, q0, q4) // Bitwise-AND the blended red5 components with the mask to make sure they only occupy [15:11].
					vector_bitwise_and_16(q1, q1, q5) // Bitwise-AND the blended green6 components with the mask to make sure they only occupy [10:5].
					vector_bitwise_and_16(q2, q2, q6) // Bitwise-AND the blended blue5 components with the mask to make sure they only occupy [4:0].
				);

				// Note:
				// Q0, Q1, Q2 are the final masked and blended color rgb565 components now.
				// Q3, Q7 are free.

				// Combine the final masked and blended RGB565 components.
				asm volatile(
					vector_bitwise_or_16(q2, q2, q1) // Combine the final masked and blended green6 with blue5 components.
					vector_bitwise_or_16(q2, q2, q0) // Combine the final masked and blended green6+blue5 with red5 components.
				);

				asm_vector_swap_16(
					/* src_register = */ q2,
					/* dst_register = */ q3,
					/* tmp_register = */ q7
				);

				// Store the final blended rgb565 colors to the memory.
				// VST.128.IP will increment the %0 operand register automatically by the third operand (16).
				asm volatile(vector_store_128_aligned(q3, arg(0), 16) : "+a"(simd_offset_dst));
			}

			// Manually blending the remaining padding colors until the size is reached.
			for (; x < clipped_size_x; x ++) {
				// Get the bitmap color and the flipped original color of the pixel.
						uint16_t color_dst_flipped			=							dst_offset	[x];
						uint16_t color_src_rgb565_pre_mul	=							color_offset[x];
				const	uint16_t color_src_alpha_inv		= alpha_offset != NULL ?	alpha_offset[x] : 0U;

				// Flip the color back to get correct color order if the bitmap is flipped.
				if (bitmap_flipped) {
					color_src_rgb565_pre_mul = 	((color_src_rgb565_pre_mul >> 8U) & 0x00FFU)
					|							((color_src_rgb565_pre_mul << 8U) & 0xFF00U);
				}

				// Flip the LSB and MSB to get the correct RGB565 color.
				const uint16_t color_dst_rgb565 =	((color_dst_flipped >> 8U) & 0x00FFU)
				|									((color_dst_flipped << 8U) & 0xFF00U);

				const uint16_t color_final_rgb565 = private_blend_color_fast_rgb565_pre_mul(
					/* color_src_a8_inv			= */ (uint8_t) color_src_alpha_inv,
					/* color_src_a8_multiplier	= */ bitmap_a8_multiplier,
					/* color_src_rgb565_pre_mul	= */ color_src_rgb565_pre_mul,
					/* color_dst_rgb565			= */ color_dst_rgb565
				);

				// Flip the LSB and MSB to get the correct transmission byte color.
				color_dst_flipped =	((color_final_rgb565 >> 8U) & 0x00FFU)
				|					((color_final_rgb565 << 8U) & 0xFF00U);

				dst_offset[x] = color_dst_flipped;
			}
		}
	}

	return ESP_OK;
}

esp_err_t esp_fast_lcd_draw_native_bitmap(
	const esp_fast_lcd_panel_device_t*	context,
	const int32_t						position_x,
	const int32_t						position_y,
	const uint32_t						size_x,
	const uint32_t						size_y,
	const uint32_t						bitmap_offset_x,
	const uint32_t						bitmap_offset_y,
	const uint32_t						bitmap_size_x,
	const uint8_t						bitmap_flipped,
	const uint8_t						bitmap_a8_multiplier,
	const uint16_t*						bitmap_rgb565
) {
	// We cannot proceed without context.
	ESP_RETURN_ON_FALSE(context != NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No esp_fast_lcd_panel_device_t handle provided when performing drawing an opaque native bitmap.");

	// Skip the draw if the bitmap has no size.
	if (	size_x == 0
		||	size_y == 0
	) {
		return ESP_OK;
	}

	// Skip the draw if the bitmap is invisible.
	if (bitmap_a8_multiplier == 0U) {
		return ESP_OK;
	}

	// Route to translucent native bitmap variant if the multiplier is not opaque.
	if (bitmap_a8_multiplier != 255U) {
		return esp_fast_lcd_draw_bitmap_rgb565_pre_mul_a8_inv(
			/* context					= */ context,
			/* position_x				= */ position_x,
			/* position_y				= */ position_y,
			/* size_x					= */ size_x,
			/* size_y					= */ size_y,
			/* bitmap_offset_x			= */ bitmap_offset_x,
			/* bitmap_offset_y			= */ bitmap_offset_y,
			/* bitmap_size_x			= */ bitmap_size_x,
			/* bitmap_flipped			= */ bitmap_flipped,
			/* bitmap_a8_multiplier		= */ bitmap_a8_multiplier,
			/* bitmap_rgb565_pre_mul	= */ bitmap_rgb565,
			/* bitmap_a8_inv			= */ NULL
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

	// Calculate the clipped bitmap offset start position of the rectangle.
	const uint32_t clipped_bitmap_start_offset_x = position_x < 0 ? ((uint32_t) (-position_x)) : 0;
	const uint32_t clipped_bitmap_start_offset_y = position_y < 0 ? ((uint32_t) (-position_y)) : 0;

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

	#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
		ESP_LOGD(ESP_FAST_LCD_TAG, "LCD panel device \"%s\" is performing an opaque native bitmap draw at: positionX=%" PRId32 ", positionY=%" PRId32 ", sizeX=%" PRIu32 ", sizeY=%" PRIu32 ".",
			/* s		*/ properties->configuration.name,
			/* PRId32	*/ position_x,
			/* PRId32	*/ position_y,
			/* PRIu32	*/ size_x,
			/* PRIu32	*/ size_y
		);
	#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

	// Write the bitmap to the framebuffer directly since the bitmap is opaque.
	for (uint32_t y = 0U; y < clipped_size_y; y ++) {
		// Extract the X cursor out of the loop for u32 copy optimization.
		uint32_t x = 0;

		// Calculate the index of the first pixel of the line at the framebuffer.
		const uint32_t dst_index =	/* index_y	= */ (clipped_start_position_y + y) * frame_size_x +
									/* index_x	= */ (clipped_start_position_x + 0);

		// Calculate the offset of the first pixel of the line at the framebuffer.
		uint16_t* dst_offset = &framebuffer[dst_index];

		// Calculate the offset of the first pixel of the current line at the bitmap.
		const uint16_t* src_offset = &bitmap_rgb565[
			(clipped_bitmap_start_offset_y + bitmap_offset_y + y) * bitmap_size_x +
			(clipped_bitmap_start_offset_x + bitmap_offset_x + 0)
		];

		// It's not worthwhile to use SIMD if the clipped_size_x is too short.
		if (clipped_size_x < 16U) {
			// If the source offset and destination address can both be 4-byte aligned, use the fast path.
			if (bitmap_flipped && is_same_align_4byte(dst_offset, src_offset)) {
				// Align the X to multiples of twos in order to copy the line using u32.
				if (dst_index % 2U != 0U) {
					// Get the flipped color from the bitmap and write the flipped color to the framebuffer..
					dst_offset[x] = src_offset[x];
					// Increment the cursor.
					x ++;
				}

				// Copy the current line of the bitmap using U32 batching.
				for (; x + 2U <= clipped_size_x; x += 2U) {
					// Write the reinterpreted bitmap u32 color to the reinterpreted framebuffer with u32 combined colors.
					*((uint32_t*) (&dst_offset[x])) = *((uint32_t*) (&src_offset[x]));
				}

				// Fill the last pixel.
				if (x < clipped_size_x) {
					// Get the flipped color from the bitmap and write the flipped color to the framebuffer.
					dst_offset[x] = src_offset[x];
				}
			} else {
				// Fill the pixels one-by-one manually.
				for (; x < clipped_size_x; x ++) {
					// Get the original 16-bit color flipped or un-flipped color from the bitmap.
					uint16_t color_src_rgb565 = src_offset[x];

					// If the bitmap is not flipped, we need to flip the color to get the correct transmission byte order.
					if (!bitmap_flipped) {
						color_src_rgb565 =	((color_src_rgb565 >> 8U) & 0x00FFU)
						|					((color_src_rgb565 << 8U) & 0xFF00U);
					}

					// Write the flipped color to the framebuffer.
					dst_offset[x] = color_src_rgb565;
				}
			}
		} else {
			// If the color is already flipped, we could just copy the lines instead of filling manually.
			if (bitmap_flipped) {
				memcpy(
					/* dst_buffer	= */ dst_offset,
					/* src_buffer	= */ src_offset,
					/* length		= */ clipped_size_x * 2U
				);
			} else {
				// Get the colors that we need to fill to reach the next 16-byte aligned address.
				// ">> 1U" means "divided 2" because a color is 2 bytes long as RGB565.
				const uint32_t padding = next_16byte_padding(dst_offset) >> 1U;

				// Flip and copy the padding manually if needed.
				// Exit immediately when the padding or the size is reached.
				for (; x < clipped_size_x && x < padding; x ++) {
					// Get the original 16-bit color flipped or un-flipped color from the bitmap.
					const uint16_t color_src_rgb565 = src_offset[x];

					// Flip the RGB565 color to get the correct transmission byte order.
					const uint16_t color_src_flipped =	((color_src_rgb565 >> 8U) & 0x00FFU)
					|									((color_src_rgb565 << 8U) & 0xFF00U);

					// Write the flipped color to the framebuffer.
					dst_offset[x] = color_src_flipped;
				}

				// Initialize the SIMD color memory cursors of the current line.
						uint16_t* simd_offset_dst = &dst_offset[x];
				const	uint16_t* simd_offset_src = &src_offset[x];

				// Check if aligned vector loading is supported.
				const uint8_t src_offset_aligned = is_same_align_16byte(simd_offset_dst, simd_offset_src);

				// SIMD is 8 colors a batch (128-bit amd, 8 16-bit colors).
				for (; x + 8U <= clipped_size_x; x += 8U) {
					// Load the flipped RGB565 color from the bitmap to the SIMD vector register.
					if (src_offset_aligned) {
						// Load the aligned color memory to the SIMD vector register.
						// VLD.128.IP will increment the %0 operand register automatically by the third operand (16).
						asm volatile(vector_load_128_aligned(q0, arg(0), 16) : "+a"(simd_offset_src));
					} else {
						// Load two parts of the unaligned color memory to the SIMD vector registers then construct the
						// complete sequence.
						asm volatile(
							vector_load_128_usar			(q6, arg(0),	16)	// Load the unaligned lower-part of the memory, increment the simdSrcOffset.
							vector_load_128_usar			(q7, arg(0),	0)	// Load the unaligned higher-part of the memory.
							vector_shift_right_combined_256	(q0, q6,		q7)	// Construct the complete sequence from 2 vector registers.
							: /* %0 = */ "+a"(simd_offset_src)
						);
					}

					// We need to flip the color to get the correct transmission byte order.
					asm_vector_swap_16(
						/* src_register = */ q0,
						/* dst_register = */ q0,
						/* tmp_register = */ q7
					);

					// Store the loaded and flipped colors to the memory.
					// VST.128.IP will increment the %0 operand register automatically by the third operand (16).
					asm volatile(vector_store_128_aligned(q0, arg(0), 16) : "+a"(simd_offset_dst));
				}

				// Manually flip and copy the remaining padding colors until the size is reached.
				for (; x < clipped_size_x; x ++) {
					// Get the original 16-bit color flipped or un-flipped color from the bitmap.
					const uint16_t color_src_rgb565 = src_offset[x];

					// Flip the RGB565 color to get the correct transmission byte order.
					const uint16_t color_src_flipped =	((color_src_rgb565 >> 8U) & 0x00FFU)
					|									((color_src_rgb565 << 8U) & 0xFF00U);

					// Write the flipped color to the framebuffer.
					dst_offset[x] = color_src_flipped;
				}
			}
		}
	}

	return ESP_OK;
}