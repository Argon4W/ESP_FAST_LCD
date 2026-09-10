#include "esp_check.h"
#include "esp_log.h"
#include "esp_fast_lcd.h"
#include "esp_fast_lcd_common.h"

esp_err_t esp_fast_lcd_prepare_rgba8888_bitmap_to_rgba8888_pre_mul(
	const	uint32_t*	bitmap_rgba8888_src,
			uint32_t*	bitmap_rgba8888_pre_mul_dst,
	const	uint32_t	bitmap_size_x,
	const	uint32_t	bitmap_size_y
) {
	// We cannot proceed without the input and output bitmap pointer.
	ESP_RETURN_ON_FALSE(bitmap_rgba8888_src			!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No input bitmap handle provided when preparing bitmap.");
	ESP_RETURN_ON_FALSE(bitmap_rgba8888_pre_mul_dst	!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No output bitmap handle provided when preparing bitmap.");

	// Skip the draw if the bitmap has no size.
	if (	bitmap_size_x == 0
		||	bitmap_size_y == 0
	) {
		return ESP_OK;
	}

	// Prepare for all pixels.
	for		(uint32_t position_y = 0; position_y < bitmap_size_y; position_y ++) {
		for	(uint32_t position_x = 0; position_x < bitmap_size_x; position_x ++) {
			// Get the index of the given coordinate.
			const uint32_t pixel_index =	/* index_y = */ position_y * bitmap_size_x +
											/* index_x = */ position_x;

			// Get the RGBA8888 from input
			const uint32_t color_src_rgba8888 = bitmap_rgba8888_src[pixel_index];

			// Get the all color components of the rgba8888.
			const uint8_t r8_src = (uint8_t) ((color_src_rgba8888 >> 24U)	& 0xFFU);
			const uint8_t g8_src = (uint8_t) ((color_src_rgba8888 >> 16U)	& 0xFFU);
			const uint8_t b8_src = (uint8_t) ((color_src_rgba8888 >> 8U)	& 0xFFU);
			const uint8_t a8_src = (uint8_t) ((color_src_rgba8888 >> 0U)	& 0xFFU);

			// Pre-multiply the color components the RGBA8888 color with the alpha then invert the alpha.
			const uint8_t r8_src_pre_mul	= unorm8_mul_exact(a8_src, r8_src);
			const uint8_t g8_src_pre_mul	= unorm8_mul_exact(a8_src, g8_src);
			const uint8_t b8_src_pre_mul	= unorm8_mul_exact(a8_src, b8_src);
			const uint8_t a8_src_inv		= 255U - a8_src;

			// Pack them into pre-multiplied RGBA8888 format;
			const uint32_t color_final_rgba8888_pre_mul =	((((uint32_t) r8_src_pre_mul)	& 0xFFU) << 24U)
			|												((((uint32_t) g8_src_pre_mul)	& 0xFFU) << 16U)
			|												((((uint32_t) b8_src_pre_mul)	& 0xFFU) << 8U)
			|												((((uint32_t) a8_src_inv)		& 0xFFU) << 0U);

			// Write the pre-multiplied color to the pixel of the destination bitmap.
			bitmap_rgba8888_pre_mul_dst[pixel_index] = color_final_rgba8888_pre_mul;
		}
	}

	return ESP_OK;
}

esp_err_t esp_fast_lcd_prepare_rgba8888_bitmap_to_rgb565_flipped(
	const	uint32_t*	bitmap_rgba8888_src,
			uint16_t*	bitmap_rgb565_flipped_dst,
	const	uint32_t	bitmap_size_x,
	const	uint32_t	bitmap_size_y
) {
	// We cannot proceed without the input and output bitmap pointer.
	ESP_RETURN_ON_FALSE(bitmap_rgba8888_src			!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No input bitmap handle provided when preparing bitmap.");
	ESP_RETURN_ON_FALSE(bitmap_rgb565_flipped_dst	!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No output bitmap handle provided when preparing bitmap.");

	// Skip the draw if the bitmap has no size.
	if (	bitmap_size_x == 0
		||	bitmap_size_y == 0
	) {
		return ESP_OK;
	}

	// Prepare for all pixels.
	for		(uint32_t position_y = 0; position_y < bitmap_size_y; position_y ++) {
		for	(uint32_t position_x = 0; position_x < bitmap_size_x; position_x ++) {
			// Get the index of the given coordinate.
			const uint32_t pixel_index =	/* index_y = */ position_y * bitmap_size_x +
											/* index_x = */ position_x;

			// Get the RGBA8888 from input
			const uint32_t color_src_rgba8888 = bitmap_rgba8888_src[pixel_index];

			// Get the all color components of the rgba8888.
			const uint8_t r8_src = (uint8_t) ((color_src_rgba8888 >> 24U)	& 0xFFU);
			const uint8_t g8_src = (uint8_t) ((color_src_rgba8888 >> 16U)	& 0xFFU);
			const uint8_t b8_src = (uint8_t) ((color_src_rgba8888 >> 8U)	& 0xFFU);
			const uint8_t a8_src = (uint8_t) ((color_src_rgba8888 >> 0U)	& 0xFFU);

			// Blend the color with black.
			const uint8_t r8_final = unorm8_mul_exact(a8_src, r8_src);
			const uint8_t g8_final = unorm8_mul_exact(a8_src, g8_src);
			const uint8_t b8_final = unorm8_mul_exact(a8_src, b8_src);

			// Map the blended RGB888 color into RGB565.
			const uint8_t r5_final = r8_final >> 3U;
			const uint8_t g6_final = g8_final >> 2U;
			const uint8_t b5_final = b8_final >> 3U;

			// Pack them into RGB565 format;
			const uint16_t color_final_rgb565 =	((((uint16_t) r5_final) & 0b011111U) << 11U)
			|									((((uint16_t) g6_final) & 0b111111U) << 5U)
			|									((((uint16_t) b5_final) & 0b011111U) << 0U);

			// Flip the LSB and MSB back to get correct transmission byte order.
			const uint16_t color_final_flipped =	((color_final_rgb565 >> 8U) & 0x00FFU)
			|										((color_final_rgb565 << 8U) & 0xFF00U);

			// Write the blended color to the pixel of the destination bitmap.
			bitmap_rgb565_flipped_dst[pixel_index] = color_final_flipped;
		}
	}

	return ESP_OK;
}

esp_err_t esp_fast_lcd_prepare_rgba8888_bitmap_to_rgb565_masked(
	const	uint32_t*	bitmap_rgba8888_src,
			uint16_t*	bitmap_rgb565_flipped_dst,
			uint16_t*	bitmap_bitmask_flipped_dst,
	const	uint32_t	bitmap_size_x,
	const	uint32_t	bitmap_size_y,
	const	uint8_t		bitmap_alpha_threshold
) {
	// We cannot proceed without the input and output bitmap pointer.
	ESP_RETURN_ON_FALSE(bitmap_rgba8888_src			!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No input bitmap handle provided when preparing bitmap.");
	ESP_RETURN_ON_FALSE(bitmap_rgb565_flipped_dst	!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No output bitmap handle provided when preparing bitmap.");
	ESP_RETURN_ON_FALSE(bitmap_bitmask_flipped_dst	!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No output bitmask handle provided when preparing bitmap.");

	// Skip the draw if the bitmap has no size.
	if (	bitmap_size_x == 0
		||	bitmap_size_y == 0
	) {
		return ESP_OK;
	}

	// Prepare for all pixels.
	for		(uint32_t position_y = 0; position_y < bitmap_size_y; position_y ++) {
		for	(uint32_t position_x = 0; position_x < bitmap_size_x; position_x ++) {
			// Get the index of the given coordinate.
			const uint32_t pixel_index =	/* index_y = */ position_y * bitmap_size_x +
											/* index_x = */ position_x;

			// Get the RGBA8888 from input
			const uint32_t color_src_rgba8888 = bitmap_rgba8888_src[pixel_index];

			// Get the all color components of the rgba8888.
			const uint8_t r8_src = (uint8_t) ((color_src_rgba8888 >> 24U)	& 0xFFU);
			const uint8_t g8_src = (uint8_t) ((color_src_rgba8888 >> 16U)	& 0xFFU);
			const uint8_t b8_src = (uint8_t) ((color_src_rgba8888 >> 8U)	& 0xFFU);
			const uint8_t a8_src = (uint8_t) ((color_src_rgba8888 >> 0U)	& 0xFFU);

			// Write the pixel if the alpha of the pixel reaches the threshold.
			if (a8_src >= bitmap_alpha_threshold) {
				// Map the RGB888 color into RGB565.
				const uint8_t r5_final = r8_src >> 3U;
				const uint8_t g6_final = g8_src >> 2U;
				const uint8_t b5_final = b8_src >> 3U;

				// Pack them into RGB565 format;
				const uint16_t color_final_rgb565 =	((((uint16_t) r5_final) & 0b011111U) << 11U)
				|									((((uint16_t) g6_final) & 0b111111U) << 5U)
				|									((((uint16_t) b5_final) & 0b011111U) << 0U);

				// Flip the LSB and MSB back to get correct transmission byte order.
				const uint16_t color_final_flipped =	((color_final_rgb565 >> 8U) & 0x00FFU)
				|										((color_final_rgb565 << 8U) & 0xFF00U);

				// Write the flipped color and mark the pixel visible.
				bitmap_rgb565_flipped_dst	[pixel_index] = color_final_flipped;
				bitmap_bitmask_flipped_dst	[pixel_index] = 0xFFFFU;
			} else {
				// Write black and mark the pixel invisible.
				bitmap_rgb565_flipped_dst	[pixel_index] = 0x0000U;
				bitmap_bitmask_flipped_dst	[pixel_index] = 0x0000U;
			}
		}
	}

	return ESP_OK;
}

esp_err_t esp_fast_lcd_prepare_rgba8888_bitmap_to_rgb565_pre_mul_a8_inv(
	const	uint32_t*	bitmap_rgba8888_src,
			uint16_t*	bitmap_rgb565_pre_mul_dst,
			uint16_t*	bitmap_a8_inv_dst,
	const	uint32_t	bitmap_size_x,
	const	uint32_t	bitmap_size_y
) {
	// We cannot proceed without the input and output bitmap pointer.
	ESP_RETURN_ON_FALSE(bitmap_rgba8888_src			!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No input bitmap handle provided when preparing bitmap.");
	ESP_RETURN_ON_FALSE(bitmap_rgb565_pre_mul_dst	!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No output bitmap handle provided when preparing bitmap.");
	ESP_RETURN_ON_FALSE(bitmap_a8_inv_dst			!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No output alpha bitmap handle provided when preparing bitmap.");

	// Skip the draw if the bitmap has no size.
	if (	bitmap_size_x == 0
		||	bitmap_size_y == 0
	) {
		return ESP_OK;
	}

	// Prepare for all pixels.
	for		(uint32_t position_y = 0; position_y < bitmap_size_y; position_y ++) {
		for	(uint32_t position_x = 0; position_x < bitmap_size_x; position_x ++) {
			// Get the index of the given coordinate.
			const uint32_t pixel_index =	/* index_y = */ position_y * bitmap_size_x +
											/* index_x = */ position_x;

			// Get the RGBA8888 from input
			const uint32_t color_src_rgba8888 = bitmap_rgba8888_src[pixel_index];

			// Get the all color components of the rgba8888.
			const uint8_t r8_src = (uint8_t) ((color_src_rgba8888 >> 24U)	& 0xFFU);
			const uint8_t g8_src = (uint8_t) ((color_src_rgba8888 >> 16U)	& 0xFFU);
			const uint8_t b8_src = (uint8_t) ((color_src_rgba8888 >> 8U)	& 0xFFU);
			const uint8_t a8_src = (uint8_t) ((color_src_rgba8888 >> 0U)	& 0xFFU);

			// Pre-multiply the color components the RGBA8888 color with the alpha then invert the alpha.
			const uint8_t r8_src_pre_mul	= unorm8_mul_exact(a8_src, r8_src);
			const uint8_t g8_src_pre_mul	= unorm8_mul_exact(a8_src, g8_src);
			const uint8_t b8_src_pre_mul	= unorm8_mul_exact(a8_src, b8_src);
			const uint8_t a8_src_inv		= 255U - a8_src;

			// Map the pre-multiplied RGB888 color into RGB565.
			const uint8_t r5_final = r8_src_pre_mul >> 3U;
			const uint8_t g6_final = g8_src_pre_mul >> 2U;
			const uint8_t b5_final = b8_src_pre_mul >> 3U;

			// Pack them into RGB565 format;
			const uint16_t color_final_rgb565_pre_mul =	((((uint16_t) r5_final) & 0b011111U) << 11U)
			|											((((uint16_t) g6_final) & 0b111111U) << 5U)
			|											((((uint16_t) b5_final) & 0b011111U) << 0U);

			// Write the pre-multiplied color and inverted alpha to the pixel of the destination bitmap.
			bitmap_rgb565_pre_mul_dst	[pixel_index] = color_final_rgb565_pre_mul;
			bitmap_a8_inv_dst			[pixel_index] = a8_src_inv;
		}
	}

	return ESP_OK;
}

esp_err_t esp_fast_lcd_prepare_rgb565_bitmap_to_rgb565_flipped(
	const	uint16_t*	bitmap_rgb565_src,
			uint16_t*	bitmap_rgb565_flipped_dst,
	const	uint32_t	bitmap_size_x,
	const	uint32_t	bitmap_size_y
) {
	ESP_RETURN_ON_FALSE(bitmap_rgb565_src			!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No input bitmap handle provided when preparing bitmap.");
	ESP_RETURN_ON_FALSE(bitmap_rgb565_flipped_dst	!= NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No output bitmap handle provided when preparing bitmap.");

	// Skip the draw if the bitmap has no size.
	if (	bitmap_size_x == 0
		||	bitmap_size_y == 0
	) {
		return ESP_OK;
	}

	// Prepare for all pixels.
	for		(uint32_t position_y = 0; position_y < bitmap_size_y; position_y ++) {
		for	(uint32_t position_x = 0; position_x < bitmap_size_x; position_x ++) {
			// Get the index of the given coordinate.
			const uint32_t pixel_index =	/* index_y = */ position_y * bitmap_size_x +
											/* index_x = */ position_x;

			// Get the RGB565 from input
			const uint16_t color_src_rgb565 = bitmap_rgb565_src[pixel_index];

			// Flip the LSB and MSB back to get correct transmission byte order.
			const uint16_t color_final_flipped =	((color_src_rgb565 >> 8U) & 0x00FFU)
			|										((color_src_rgb565 << 8U) & 0xFF00U);

			// Write the flipped color to the pixel of the destination bitmap.
			bitmap_rgb565_flipped_dst[pixel_index] = color_final_flipped;
		}
	}

	return ESP_OK;
}