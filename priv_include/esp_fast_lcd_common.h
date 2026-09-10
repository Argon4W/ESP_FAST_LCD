#ifndef ESP_FAST_LCD_COMMON_H
#define ESP_FAST_LCD_COMMON_H

#include "stdint.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// The log tag of esp_fast_lcd.
static const char* ESP_FAST_LCD_TAG = "esp_fast_lcd";

/**
 * @brief	Multiply two unsigned normalized 8-bit numbers.
 * @param a	The multiplier unsigned normalized 8-bit number in uint8_t.
 * @param b	The multiplicand unsigned normalized 8-bit number in uint8_t.
 * @return	The result of the multiplied normalized 8-bit number in uint8_t.
 */
static inline uint8_t unorm8_mul_exact(
	const uint8_t a,
	const uint8_t b
) {
	// round div (+ 128U)
	const uint16_t temp = ((uint16_t) a) * ((uint16_t) b) + 128U;
	// Bitshift hack of divided by 255.
	return (uint8_t) (((temp >> 8U) + temp) >> 8U);
}

/**
 * @brief					Check if a RGBA8888 color is opaque (R=255).
 * @param color_rgba8888	The RGBA8888 color to be checked.
 * @return					true if the color is opaque.
 */
static inline uint8_t color_rgba8888_is_opaque(const uint32_t color_rgba8888) {
	return (color_rgba8888 & 0xFFU) == 0xFFU;
}

/**
 * @brief					Check if a RGBA8888 color is transparent (R=0).
 * @param color_rgba8888	The RGBA8888 color to be checked.
 * @return					true if the color is transparent.
 */
static inline uint8_t color_rgba8888_is_transparent(const uint32_t color_rgba8888) {
	return (color_rgba8888 & 0xFFU) == 0x00U;
}

/**
 * @brief		Get the count of trailing zeros of an uint32_t.
 * @param val	The vale to get trailing zeros.
 * @return		The count of trailing zeros of the val.
 */
static inline uint32_t ctz(const uint32_t val) {
	return val == 0U ? 32U : __builtin_ctz(val);
}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ESP_FAST_LCD_COMMON_H
