#ifndef ESP_FAST_LCD_COMMON_RECT_H
#define ESP_FAST_LCD_COMMON_RECT_H

#include "stdint.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @brief						Private function of blending colors using fast floorDiv with pre-multiplied incoming RGB565
 *								color components and inverted 8-bit alpha component.
 * @attention					It is the same algorithm that SIMD path uses.
 * @param r5_src_pre_mul		The 8-bit red color component of the incoming color pre-multiplied with its alpha component.
 * @param g6_src_pre_mul		The 8-bit green color component of the incoming color pre-multiplied with its alpha component.
 * @param b5_src_pre_mul		The 8-bit blue color component of the incoming color pre-multiplied with its alpha component.
 * @param a8_src_inv			The inverted 8-bit alpha component of the color (225 - alpha).
 * @param color_dst_rgb565		The framebuffer color to be blended with the incoming pre-multiplied color.
 * @return						The blended color in RGB565 format (MSB first).
 */
static inline uint16_t private_blend_pre_mul_fast(
	const uint8_t	r5_src_pre_mul,
	const uint8_t	g6_src_pre_mul,
	const uint8_t	b5_src_pre_mul,
	const uint8_t	a8_src_inv,
	const uint16_t	color_dst_rgb565
) {
	// Return the destination color as the final color if the incoming color is transparent.
	if (a8_src_inv == 255U) {
		return color_dst_rgb565;
	}

	// Reserve the final output RGB565 color components.
	uint8_t r5_final;
	uint8_t g6_final;
	uint8_t b5_final;

	// Fast path for 255 alpha.
	if (a8_src_inv == 0U) {
		// Set the incoming color to the final color if the alpha of the incoming color is opaque.
		r5_final = r5_src_pre_mul;
		g6_final = g6_src_pre_mul;
		b5_final = b5_src_pre_mul;
	} else {
		// Get all color components of rgb565.
		const uint8_t r5_dst = (uint8_t) ((color_dst_rgb565 >> 11U)	& 0b011111U);
		const uint8_t g6_dst = (uint8_t) ((color_dst_rgb565 >> 5U)	& 0b111111U);
		const uint8_t b5_dst = (uint8_t) ((color_dst_rgb565 >> 0U)	& 0b011111U);

		// Mix the incoming color with the original color using painter's algorithm.
		const uint16_t r16 = ((uint16_t) r5_src_pre_mul) + ((((uint16_t) a8_src_inv) * ((uint16_t) (r5_dst))) / 255U);
		const uint16_t g16 = ((uint16_t) g6_src_pre_mul) + ((((uint16_t) a8_src_inv) * ((uint16_t) (g6_dst))) / 255U);
		const uint16_t b16 = ((uint16_t) b5_src_pre_mul) + ((((uint16_t) a8_src_inv) * ((uint16_t) (b5_dst))) / 255U);

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

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ESP_FAST_LCD_COMMON_RECT_H
