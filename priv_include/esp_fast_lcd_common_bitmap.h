#ifndef ESP_FAST_LCD_COMMON_BITMAP_H
#define ESP_FAST_LCD_COMMON_BITMAP_H

#include "stdint.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @brief							Private function of blending colors using fast floorDiv with either raw or pre-multiplied
 *									RGBA8888 colors.
 * @param color_src_rgba8888		The incoming 8-bit component color that can be either pre-multiplied or raw RGBA8888 color.
 * @param color_src_pre_multiplied	True if the incoming color is pre-multiplied RGBA8888 color.
 * @param color_dst_rgb565			The framebuffer color to be blended with the incoming color.
 * @return							The blended color in RGB565 format (MSB first).
 */
static inline uint16_t private_blend_color_fast_rgba8888(
	const uint32_t	color_src_rgba8888,
	const uint8_t	color_src_pre_multiplied,
	const uint16_t	color_dst_rgb565
) {
	// Reserve the all pre-multiplied color components of the RGBA8888 color.
	uint8_t r8_src_pre_mul;
	uint8_t g8_src_pre_mul;
	uint8_t b8_src_pre_mul;
	uint8_t a8_src_inv;

	// Skip the pre-multiplication of the incoming color if the bitmap is already pre-multiplied.
	if (color_src_pre_multiplied) {
		// Get the inverted alpha component of the pre-multiplied rgba8888.
		a8_src_inv = (uint8_t) ((color_src_rgba8888 >> 0U) & 0xFFU);

		// Skip if the pixel is transparent.
		if (a8_src_inv == 255U) {
			return color_dst_rgb565;
		}

		// Get the pre-multiplied RGBA8888 color components.
		r8_src_pre_mul = (uint8_t) ((color_src_rgba8888 >> 24U)	& 0xFFU);
		g8_src_pre_mul = (uint8_t) ((color_src_rgba8888 >> 16U)	& 0xFFU);
		b8_src_pre_mul = (uint8_t) ((color_src_rgba8888 >> 8U)	& 0xFFU);
	} else {
		// Get the alpha component of the rgba8888.
		const uint8_t a8 = (uint8_t) ((color_src_rgba8888 >> 0U) & 0xFFU);

		// Skip is the pixel is transparent.
		if (a8 == 0U) {
			return color_dst_rgb565;
		}

		// Get the R/G/B color components of the rgba8888.
		const uint8_t r8 = (uint8_t) ((color_src_rgba8888 >> 24U)	& 0xFFU);
		const uint8_t g8 = (uint8_t) ((color_src_rgba8888 >> 16U)	& 0xFFU);
		const uint8_t b8 = (uint8_t) ((color_src_rgba8888 >> 8U)	& 0xFFU);

		// Pre-multiply the color now.
		if (a8 != 255U) {
			// Pre-multiply the color components the RGBA8888 color with the alpha if the alpha is not opaque.
			r8_src_pre_mul	= (uint8_t) ((((uint16_t) a8) * ((uint16_t) r8)) / 256U);
			g8_src_pre_mul	= (uint8_t) ((((uint16_t) a8) * ((uint16_t) g8)) / 256U);
			b8_src_pre_mul	= (uint8_t) ((((uint16_t) a8) * ((uint16_t) b8)) / 256U);
			a8_src_inv		= 255U - a8;
		} else {
			// Set the color directly as the pre-multiplied color if the alpha is opaque.
			r8_src_pre_mul	= r8;
			g8_src_pre_mul	= g8;
			b8_src_pre_mul	= b8;
			a8_src_inv		= 0;
		}
	}

	// Map the pre-multiplied RGBA8888 color components into RGB565 color components.
	const uint8_t r5_src_pre_mul = r8_src_pre_mul >> 3U;
	const uint8_t g6_src_pre_mul = g8_src_pre_mul >> 2U;
	const uint8_t b5_src_pre_mul = b8_src_pre_mul >> 3U;

	uint8_t r5_final;
	uint8_t g6_final;
	uint8_t b5_final;

	// Write the color directly to the framebuffer if the bitmap is opaque.
	if (a8_src_inv == 0U) {
		// Map them into 5-6-5.
		r5_final = r5_src_pre_mul;
		g6_final = g6_src_pre_mul;
		b5_final = b5_src_pre_mul;
	} else {
		// Get all color components of rgb565.
		const uint8_t r5_dst = (uint8_t) ((color_dst_rgb565 >> 11U)	& 0b011111U);
		const uint8_t g6_dst = (uint8_t) ((color_dst_rgb565 >> 5U)	& 0b111111U);
		const uint8_t b5_dst = (uint8_t) ((color_dst_rgb565 >> 0U)	& 0b011111U);

		// Mix the incoming color with the framebuffer color using painter's algorithm.
		const uint16_t r16 = ((uint16_t) r5_src_pre_mul) + ((((uint16_t) a8_src_inv) * ((uint16_t) (r5_dst))) / 256U);
		const uint16_t g16 = ((uint16_t) g6_src_pre_mul) + ((((uint16_t) a8_src_inv) * ((uint16_t) (g6_dst))) / 256U);
		const uint16_t b16 = ((uint16_t) b5_src_pre_mul) + ((((uint16_t) a8_src_inv) * ((uint16_t) (b5_dst))) / 256U);

		// Saturate the blended RGB565 color components.
		r5_final = r16 > 0b011111U ? 0B011111U :((uint8_t) r16);
		g6_final = g16 > 0b111111U ? 0B111111U :((uint8_t) g16);
		b5_final = b16 > 0b011111U ? 0B011111U :((uint8_t) b16);
	}

	// Pack them into RGB565 format;
	const uint16_t color_rgb565_final =	((((uint16_t) r5_final) & 0b011111U) << 11U)
	|									((((uint16_t) g6_final) & 0b111111U) << 5U)
	|									((((uint16_t) b5_final) & 0b011111U) << 0U);

	return color_rgb565_final;
}

/**
 * @brief							Private function of blending colors using fast floorDiv with pre-multiplied RGB565 color
 *									and separate inverted 8-bit alpha component.
 * @param color_src_a8_inv			The separate 8-bit inverted alpha (255 - alpha) component of the incoming color.
 * @param color_src_rgb565_pre_mul	The incoming pre-multiplied RGB565 color.
 * @param color_dst_rgb565			The framebuffer color to be blended with the incoming color.
 * @return							The blended color in RGB565 format (MSB first).
 */
static inline uint16_t private_blend_color_fast_rgb565_pre_mul(
	const uint8_t	color_src_a8_inv,
	const uint16_t	color_src_rgb565_pre_mul,
	const uint16_t	color_dst_rgb565
) {
	if (color_src_a8_inv == 255U) {
		return color_dst_rgb565;
	}

	if (color_src_a8_inv == 0U) {
		return color_src_rgb565_pre_mul;
	}

	// Get all color components of pre-multiplied RGB565 color components.
	const uint8_t r5_src_pre_mul = (uint8_t) ((color_src_rgb565_pre_mul >> 11U)	& 0b011111U);
	const uint8_t g6_src_pre_mul = (uint8_t) ((color_src_rgb565_pre_mul >> 5U)	& 0b111111U);
	const uint8_t b5_src_pre_mul = (uint8_t) ((color_src_rgb565_pre_mul >> 0U)	& 0b011111U);

	// Get all color components of the framebuffer RGB565 color.
	const uint8_t r5_dst = (uint8_t) ((color_dst_rgb565 >> 11U)	& 0b011111U);
	const uint8_t g6_dst = (uint8_t) ((color_dst_rgb565 >> 5U)	& 0b111111U);
	const uint8_t b5_dst = (uint8_t) ((color_dst_rgb565 >> 0U)	& 0b011111U);

	// Mix the incoming color with the framebuffer color using painter's algorithm.
	const uint16_t r16 = ((uint16_t) r5_src_pre_mul) + ((((uint16_t) color_src_a8_inv) * ((uint16_t) (r5_dst))) / 256U);
	const uint16_t g16 = ((uint16_t) g6_src_pre_mul) + ((((uint16_t) color_src_a8_inv) * ((uint16_t) (g6_dst))) / 256U);
	const uint16_t b16 = ((uint16_t) b5_src_pre_mul) + ((((uint16_t) color_src_a8_inv) * ((uint16_t) (b5_dst))) / 256U);

	// Saturate the blended RGB565 color components.
	const uint8_t r5_final = r16 > 0b011111U ? 0B011111U :((uint8_t) r16);
	const uint8_t g6_final = g16 > 0b111111U ? 0B111111U :((uint8_t) g16);
	const uint8_t b5_final = b16 > 0b011111U ? 0B011111U :((uint8_t) b16);

	// Pack them into RGB565 format;
	const uint16_t color_rgb565_final =	((((uint16_t) r5_final) & 0b011111U) << 11U)
	|									((((uint16_t) g6_final) & 0b111111U) << 5U)
	|									((((uint16_t) b5_final) & 0b011111U) << 0U);

	return color_rgb565_final;
}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ESP_FAST_LCD_COMMON_BITMAP_H