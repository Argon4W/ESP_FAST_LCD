#ifndef ESP_FAST_LCD_SIMD_H
#define ESP_FAST_LCD_SIMD_H

#include "stdint.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @brief The bitmask of the RGB565 color components for extracting each color components when using SIMD blend.
 * @{
 */

/**
 * @brief The bitmask of the 5-bit red color component of RGB565.
 */
static const uint16_t rgb565_r5_bitmask = 0b1111100000000000U;

/**
 * @brief The bitmask of the 6-bit green color component of RGB565.
 */
static const uint16_t rgb565_g6_bitmask = 0b0000011111100000U;

/**
 * @brief The bitmask of the 5-bit blue component of RGB565.
 */
static const uint16_t rgb565_b5_bitmask = 0b0000000000011111U;

/**
 *@}
 */

/**
 * @brief The masks of uint16_t for extracting lower 8-bits (LSB) and higher 8-bits (MSB).
 * @{
 */

/**
 * @brief The lower 8-bit (LSB) bitmask of uint16_t.
 */
static const uint16_t uint16_lsb_8_bitmask = 0b0000000011111111U;

/**
 * @brief The higher 8-bit (MSB) bitmask of uint16_t.
 */
static const uint16_t u1nt16_msb_8_bitmask = 0b1111111100000000U;

/**
 * @}
 */

/**
 * The addressable multiplier 256 that is equivalent to left-shifting the multiplicand by 8 bits.
 */
static const uint16_t left_shift_8 = 256U; // (1 << 8U);

/**
 * The addressable multiplier 8 that is equivalent to left-shifting the multiplicand by 3 bits.
 */
static const uint16_t left_shift_3 = 8U; // (1 << 8U);

/**
 * The addressable multiplier of right-shifting the multiplicand by vector multiplication and SAR.
 */
static const uint16_t value_one = 1U;

/**
 * The addressable constant of 255.
 */
static const uint16_t value_255 = 255U;

/**
 * @brief		Get the next first 16-byte aligned address after the ptr.
 * @param ptr	The pointer to find the next first 16-byte aligned address.
 */
#define next_16byte_padding(ptr) (((((uintptr_t) (ptr)) + ((uintptr_t) 15U)) & (~((uintptr_t) 15U))) - ((uintptr_t) (ptr)))

/**
 * @brief		True if two pointers have the exact same lower 2 bits, which means that they can both be 4-byte aligned
 *				after applying same offsets.
 * @param a_ptr	The first pointer to check.
 * @param b_ptr	The second pointer to check.
 */
#define is_same_align_4byte(a_ptr, b_ptr) (((((uintptr_t) (a_ptr)) ^ ((uintptr_t) (b_ptr))) ^ 0b11U) == 0U)

/**
 * @brief		True if two pointers have the exact same lower 4 bits, which means that they can both be 16-byte aligned
 *				after applying same offsets.
 * @param a_ptr	The first pointer to check.
 * @param b_ptr	The second pointer to check.
 */
#define is_same_align_16byte(a_ptr, b_ptr) (((((uintptr_t) (a_ptr)) ^ ((uintptr_t) (b_ptr))) ^ 0b1111U) == 0U)

/**
 * @brief					This instruction forces the lower 4 bits of the access address src_address to 0 and loads
 *							16-byte data from memory to register dst_register. After the access is completed, the
 *							src_address is incremented by address_increment bytes.
 * @param dst_register		The SIMD vector register that receives the data.
 * @param src_address		The address to be read aligned.
 * @param address_increment	The increment on src_address after the load.
 */
#define vector_load_128_aligned(dst_register, src_address, address_increment) "EE.VLD.128.IP " #dst_register ", " src_address ", " #address_increment "\n"

/**
 * @brief					This instruction forces the lower 4 bits of the access address src_address to 0 and loads
 *							16-byte data from memory to register dst_register. Meanwhile, it saves the value of the
 *							lower 4 bits in as to the special register SAR_BYTE. After the access is completed, the
 *							src_address is incremented by address_increment bytes.
 * @param dst_register		The SIMD vector register that receives the data.
 * @param src_address		The address to be read aligned.
 * @param address_increment	The increment on src_address after the load.
 */
#define vector_load_128_usar(dst_register, src_address, address_increment) "EE.LD.128.USAR.IP " #dst_register ", " src_address ", " #address_increment "\n"

	/**
	 * @brief					This instruction forces the lower 4 bits of the access address dst_address to 0 and stores
	 *							the 128-bits in register src_register to memory. After the access is completed, the
	 *							dst_address is incremented by address_increment bytes.
	 * @param src_register		The SIMD vector register that receives the data.
	 * @param dst_address		The address to be read aligned.
	 * @param address_increment	The increment on src_address after the load.
	 */
#define vector_store_128_aligned(src_register, dst_address, address_increment) "EE.VST.128.IP " #src_register ", " dst_address ", " #address_increment "\n"

/**
 * @brief				This instruction forces the lower 1 bit of the src_address as to 0, loads 16-bit data from memory
 *						and broadcasts it to the eight 16-bit data segments in register dst_register.
 * @param dst_register	The SIMD vector register to be broadcasted.
 * @param src_address	The address to read the 16-bit data.
 */
#define vector_broadcast_16(dst_register, src_address) "EE.VLDBC.16 " #dst_register ", " src_address "\n"

/**
 * @brief				This instruction forces the lower 2 bit of the src_address as to 0, loads 32-bit data from memory
 *						and broadcasts it to the four 32-bit data segments in register dst_register.
 * @param dst_register	The SIMD vector register to be broadcasted.
 * @param src_address	The address to read the 32-bit data.
 */
#define vector_broadcast_32(dst_register, src_address) "EE.VLDBC.32 " #dst_register ", " src_address "\n"

/**
 * @brief				This instruction performs a bitwise OR operation on registers a_register and b_register and
 *						writes the result of the logic operation to register dst_register.
 * @param dst_register	The SIMD vector register that receives the result.
 * @param a_register	THE SIMD vector register to perform the logical operation.
 * @param b_register	THE SIMD vector register to perform the logical operation.
 */
#define vector_bitwise_or_16(dst_register, a_register, b_register) "EE.ORQ " #dst_register ", " #a_register ", " #b_register "\n"

/**
 * @brief				This instruction performs a bitwise AND operation on registers a_register and b_register and
 *						writes the result of the logic operation to register dst_register.
 * @param dst_register	The SIMD vector register that receives the result.
 * @param a_register	THE SIMD vector register to perform the logical operation.
 * @param b_register	THE SIMD vector register to perform the logical operation.
 */
#define vector_bitwise_and_16(dst_register, a_register, b_register) "EE.ANDQ " #dst_register ", " #a_register ", " #b_register "\n"

/**
 * @brief				This instruction performs a bitwise NOT operation on registers src_register and writes the result
 *						of the logic operation to register dst_register.
 * @param dst_register	The SIMD vector register that receives the result.
 * @param a_register	THE SIMD vector register to perform the logical operation.
 */
#define vector_bitwise_not(dst_register, src_register) "EE.NOTQ " #dst_register ", " #src_register "\n"

/**
 * @brief				This instruction performs an unsigned vector multiplication on 16-bit data. Registers a_register
 *						and b_register are the multiplier and the multiplicand respectively. The eight 32-bit data
 *						results obtained from the calculation is logically right-shifted by the value in special register
 *						SAR. Then, the lower 16-bit data of the shift result is written into corresponding segment of
 *						register dst_register.
 * @param dst_register	The SIMD vector register that receives the result of 16-bit multiplication result after the right shifting.
 * @param a_register	The SIMD vector register of the multiplier.
 * @param b_register	The SIMD vector register of the multiplicand.
 */
#define vector_multiply_u16(dst_register, a_register, b_register) "EE.VMUL.U16 " #dst_register ", " #a_register ", " #b_register "\n"

/**
 * @brief				This instruction performs a vector addition on 16-bit data in the two registers a_register and
 *						b_register. Then, the 8 results obtained from the calculation are saturated, and the saturated
 *						results are written to register dst_register.
 * @param dst_register	The SIMD vector register that receives the result of addition.
 * @param a_register	SIMD vector register to perform the addition.
 * @param b_register	SIMD vector register to perform the addition.
 */
#define vector_add_s16(dst_register, a_register, b_register) "EE.VADDS.S16 " #dst_register ", " #a_register ", " #b_register "\n"

/**
 * @brief				This instruction performs a vector subtraction on 16-bit data in the two registers a_register and
 *						b_register. Then, the 8 results obtained from the calculation are saturated, and the saturated
 *						results are written to register dst_register.
 * @param dst_register	The SIMD vector register that receives the result of subtraction.
 * @param a_register	SIMD vector register of the subtrahend.
 * @param b_register	SIMD vector register to the minuend.
 */
#define vector_subtract_s16(dst_register, a_register, b_register) "EE.VSUBS.S16 " #dst_register ", " #a_register ", " #b_register "\n"

/**
 * @brief				This instruction performs a vector addition on 32-bit data in the two registers a_register and
 *						b_register. Then, the 4 results obtained from the calculation are saturated, and the saturated
 *						results are written to register dst_register.
 * @param dst_register	The SIMD vector register that receives the result of addition.
 * @param a_register	SIMD vector register to perform the addition.
 * @param b_register	SIMD vector register to perform the addition.
 */
#define vector_add_s32(dst_register, a_register, b_register) "EE.VADDS.S32 " #dst_register ", " #a_register ", " #b_register "\n"

/**
 * @brief				This instruction performs an arithmetic right shift on the 32-byte concatenation of registers
 *						lsb_Register and msb_register that hold the loaded data of two consecutive aligned addresses.
 *						By this way, you can obtain unaligned 16-byte data, which will be written to register dst_register.
 *						The right shift amount is SAR_BYTE multiplied by 8.
 * @param dst_register	The SIMD vector register that receives the result of the combined right shift.
 * @param lsb_register	The lower part 128-bit (LSB) of the combined 256-bit data to be right shifted.
 * @param msb_register	The higher part 128-bit (MSB) of the combined 256-bit data to be right shifted.
 */
#define vector_shift_right_combined_256(dst_register, lsb_register, msb_register) "EE.SRC.Q " #dst_register ", " #lsb_register ", " #msb_register "\n"

/**
 * @brief				This instruction clears the value in register src_register to 0.
 * @param src_register	The SIMD vector register to be cleared.
 */
#define vector_clear_zero(src_register) "EE.ZERO.Q " #src_register "\n"

/**
 * @brief				This instruction implements the zip algorithm on 8-bit vector data
 * @param a_register	The SIMD vector register that provides lower 8-bit data of shuffled 16-bit data.
 * @param b_register	The SIMD vector register that provides higher 8-bit data of shuffled 16-bit data.
 */
#define vector_zip_shuffle_8(a_register, b_register) "EE.VZIP.8 " #a_register ", " #b_register "\n"

	/**
	 * @brief				This instruction implements the unzip algorithm on 8-bit vector data
	 * @param a_register	The SIMD vector register that receives lower 8-bit data of unshuffled 16-bit data.
	 * @param b_register	The SIMD vector register that receives higher 8-bit data of unshuffled 16-bit data.
	 */
#define vector_unzip_unshuffle_8(a_register, b_register) "EE.VUNZIP.8 " #a_register ", " #b_register "\n"

/**
 * @brief				This instruction implements the zip algorithm on 16-bit vector data
 * @param a_register	The SIMD vector register that provides lower 16-bit data of shuffled 32-bit data.
 * @param b_register	The SIMD vector register that provides higher 16-bit data of shuffled 32-bit data.
 */
#define vector_zip_shuffle_16(a_register, b_register) "EE.VZIP.16 " #a_register ", " #b_register "\n"

/**
 * @brief				This instruction implements the unzip algorithm on 16-bit vector data
 * @param a_register	The SIMD vector register that receives lower 16-bit data of unshuffled 32-bit data.
 * @param b_register	The SIMD vector register that receives higher 16-bit data of unshuffled 32-bit data.
 */
#define vector_unzip_unshuffle_16(a_register, b_register) "EE.VUNZIP.16 " #a_register ", " #b_register "\n"

/**
 * @brief				This instruction moves the value from the source QR register src_register to the target QR
 *						register dst_register.
 * @param dst_register	The SIMD vector register that receives the moved/copied data from src_register.
 * @param src_register	The SIMD vector register that provides the data to be copied to dst_register.
 */
#define vector_copy(dst_register, src_register) "MV.QR " #dst_register ", " #src_register "\n"

/**
 * @brief		Set the value of the Shift Amount Register (SAR).
 * @param value	The value of the SAR.
 */
#define set_shift_amount(value) "WSR " value ", " "SAR" "\n"

/**
 * @brief		The mnemonic of the arguments in the assembly.
 * @param index	the index of the argument.
 */
#define arg(index) "%" #index

/**
 * @brief				Swap the LSB and MSB of every 16-bit values in a SIMD vector register, store the result into the destination
 *						SIMD vector register.
 * @attention			Inspired by https://github.com/bitbank2/bb_spi_lcd/blob/master/src/s3_simd_byteswap.S. (Licensed
 *						under Apache 2.0)
 * @param src_register	The SIMD vector register that contains the data to be swapped.
 * @param dst_register	The SIMD vector register that receives the result of the swapped data.
 * @param tmp_register	The SIMD vector register that holds temporary data.
 */
#define asm_vector_swap_16(	\
	src_register,			\
	dst_register,			\
	tmp_register			\
) asm volatile(				\
		vector_copy					(tmp_register, src_register)	/* Copy the data to temporary register to avoid modifying on the original SIMD vector register. */			\
		vector_unzip_unshuffle_8	(tmp_register, dst_register)	/* Split the lower 8-bit (LSB) and higher 8-bit (MSB） into two SIMD vector registers. */					\
		vector_zip_shuffle_8		(dst_register, tmp_register)	/* Combine lower 8-bit (LSB) and higher 8-bit (MSB) data together, but treat MSB as LSB and LSB as MSB. */	\
	);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ESP_FAST_LCD_SIMD_H
