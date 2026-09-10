#ifndef UNIFONT_17_0_05_H
#define UNIFONT_17_0_05_H

#include "stdint.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @brief The info struct of a glyph.
 */
typedef struct {
	uint32_t offset;	/*!< The offset in bytes of the first byte in the glyph data. */
	uint32_t size_x;	/*!< The size of X axis of the glyph in pixels/bits. */
} glyph_info_t;

/**
 * @brief The tightly packed 1bpp LSBit first font glyph data of the modified version of Unifont 17.0.05.
 */
extern const uint8_t unifont_17_0_05_1bpp_bits[];

/**
 * @brief Glyph block look-up table.
 */
extern const glyph_info_t* glyph_table[];

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // UNIFONT_17_0_05_H
