#include "esp_check.h"
#include "esp_log.h"
#include "esp_fast_lcd.h"
#include "esp_fast_lcd_common.h"

esp_err_t esp_fast_lcd_draw_pixel(
	const esp_fast_lcd_panel_device_t*	context,
	const int32_t						position_x,
	const int32_t						position_y,
	const uint32_t						color_rgba8888
) {
	// We cannot proceed without context.
	ESP_RETURN_ON_FALSE(context != NULL, ESP_ERR_INVALID_ARG, ESP_FAST_LCD_TAG, "No esp_fast_lcd_panel_device_t handle provided when performing drawing a pixel.");

	// Skip the draw if the color is transparent.
	if (color_rgba8888_is_transparent(color_rgba8888)) {
		return ESP_OK;
	}

	// Get the transfer queue, framebuffer and its properties.
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
	if (	position_x >=	frame_size_x
		||	position_y >=	frame_size_y
		||	position_x <	0
		||	position_y <	0
	) {
		return ESP_OK;
	}

	// Calculate the coordinate of the frame tile.
	const uint32_t tileX = ((uint32_t) position_x) / frame_tile_size_x;
	const uint32_t tileY = ((uint32_t) position_y) / frame_tile_size_y;

	// Mark the frame dirty.
	transfer_queue->frame_dirty = true;

	// Mark the tile dirty.
	transfer_queue->dirty_tiles[tileY] |= (1U << tileX);

	// Get the R/G/B components of the rgba8888.
	const uint8_t r8_src = (uint8_t) ((color_rgba8888 >> 24U)	& 0xFFU);
	const uint8_t g8_src = (uint8_t) ((color_rgba8888 >> 16U)	& 0xFFU);
	const uint8_t b8_src = (uint8_t) ((color_rgba8888 >> 8U)	& 0xFFU);
	const uint8_t a8_src = (uint8_t) ((color_rgba8888 >> 0U)	& 0xFFU);

	uint8_t r5_final;
	uint8_t g6_final;
	uint8_t b5_final;

	// Get the index on the framebuffer at the given coordinate.
	uint32_t pixel_index =	/* index_y = */ ((uint32_t) position_y) * frame_size_x +
							/* index_x = */ ((uint32_t) position_x);

	// Convert and write the RGBA8888 color directly if the color is opaque.
	if (color_rgba8888_is_opaque(color_rgba8888)) {
		// Log the operation if LCD panel debug logging is enabled.
		#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
			ESP_LOGD(ESP_FAST_LCD_TAG, "LCD panel device \"%s\" is performing a opaque pixel draw at: positionX=%" PRId32 ", positionY=%" PRId32 ".",
				/* s		*/ context->properties->configuration.name,
				/* PRIu32	*/ position_x,
				/* PRIu32	*/ position_y
			);
			ESP_LOGD(ESP_FAST_LCD_TAG, "Pixel color: r=0x%02" PRIX8 ", g=0x%02" PRIX8 ", b=0x%02" PRIX8 ".",
				/* PRIX8 */ r8_src,
				/* PRIX8 */ g8_src,
				/* PRIX8 */ b8_src
			);
		#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

		// Convert the RGBA8888 color components directly to RGB565.
		r5_final = r8_src >> 3U;
		g6_final = g8_src >> 2U;
		b5_final = b8_src >> 3U;
	} else {
		// get the inverted alpha component of the rgba8888.
		const uint8_t a8_inv = 255U - a8_src;

		// Get the flipped original color of the pixel from the framebuffer.
		const uint16_t color_dst_flipped = framebuffer[pixel_index];

		// Flip the LSB and MSB to get the correct RGB565 color.
		const uint32_t color_dst_rgb565 =	((color_dst_flipped >> 8U) & 0x00FFU)
		|									((color_dst_flipped << 8U) & 0xFF00U);

		// Get all color components of rgb565.
		const uint8_t r5_dst = (uint8_t) ((color_dst_rgb565 >> 11U)	& 0b011111U);
		const uint8_t g6_dst = (uint8_t) ((color_dst_rgb565 >> 5U)	& 0b111111U);
		const uint8_t b5_dst = (uint8_t) ((color_dst_rgb565 >> 0U)	& 0b011111U);

		// Map them to 0-255.
		const uint8_t r8_dst = (uint8_t) ((r5_dst << 3U) | (r5_dst >> 2U));
		const uint8_t g8_dst = (uint8_t) ((g6_dst << 2U) | (g6_dst >> 4U));
		const uint8_t b8_dst = (uint8_t) ((b5_dst << 3U) | (b5_dst >> 2U));

		// Mix the incoming color with the original color using painter's algorithm.
		const uint16_t r16 = ((uint16_t) unorm8_mul_exact(a8_src, r8_src)) + ((uint16_t) unorm8_mul_exact(a8_inv, r8_dst));
		const uint16_t g16 = ((uint16_t) unorm8_mul_exact(a8_src, g8_src)) + ((uint16_t) unorm8_mul_exact(a8_inv, g8_dst));
		const uint16_t b16 = ((uint16_t) unorm8_mul_exact(a8_src, b8_src)) + ((uint16_t) unorm8_mul_exact(a8_inv, b8_dst));

		const uint8_t r8_final = r16 > 255U ? 255U : ((uint8_t) r16);
		const uint8_t g8_final = g16 > 255U ? 255U : ((uint8_t) g16);
		const uint8_t b8_final = b16 > 255U ? 255U : ((uint8_t) b16);

		// Log the operation if LCD panel debug logging is enabled.
		#ifdef CONFIG_ESP_FAST_LCD_DEBUG_LOGGING
			ESP_LOGD(ESP_FAST_LCD_TAG, "LCD panel device \"%s\" is performing a translucent pixel draw at: positionX=%" PRId32 ", positionY=%" PRId32 ".",
				/* s		*/ context->properties->configuration.name,
				/* PRIu32	*/ position_x,
				/* PRIu32	*/ position_y
			);
			ESP_LOGD(ESP_FAST_LCD_TAG, "New pixel color: r=0x%02" PRIX8 ", g=0x%02" PRIX8 ", b=0x%02" PRIX8 ", a=0x%02" PRIX8 ".",
				/* PRIX8 */ r8_src,
				/* PRIX8 */ g8_src,
				/* PRIX8 */ b8_src,
				/* PRIX8 */ a8_src
			);
			ESP_LOGD(ESP_FAST_LCD_TAG, "Final blended pixel color: r=0x%02" PRIX8 ", g=0x%02" PRIX8 ", b=0x%02" PRIX8 ".",
				/* PRIX8 */ r8_final,
				/* PRIX8 */ g8_final,
				/* PRIX8 */ b8_final
			);
		#endif // CONFIG_ESP_FAST_LCD_DEBUG_LOGGING

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

	// Write back the flipped final RGB565 color to framebuffer.
	framebuffer[pixel_index] = color_final_flipped;

	return ESP_OK;
}