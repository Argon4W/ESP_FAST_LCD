#ifndef ESP_FAST_LCD_COMMON_MASKED_H
#define ESP_FAST_LCD_COMMON_MASKED_H

#include "stdint.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @}
 */

/**
 * @brief				Mix the source color and the destination color with the bitmask.
 * @param color_dst		The flipped framebuffer color at the pixel (the destination color).
 * @param color_src		The flipped incoming color of the pixel (the source color).
 * @param color_bitmask	The mask of the color to select bits that are allowed to be overwritten.
 * @return				The mixed color.
 */
static inline uint16_t private_mix_mask_color(
	const uint16_t color_dst,
	const uint16_t color_src,
	const uint16_t color_bitmask
) {
	// Prepare the incoming color and the bottom color.
	const uint16_t color_dst_masked = color_dst & ~	color_bitmask; // Reset the masked color bits to be to 0 to receive incoming color data.
	const uint16_t color_src_masked = color_src &	color_bitmask; // Retain only the non-masked bits to prevent remaining data affecting masked bits on bottom color.

	// Mix the colors with the flipped mask value.
	return color_dst_masked | color_src_masked;
}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ESP_FAST_LCD_COMMON_MASKED_H
