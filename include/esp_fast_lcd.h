#ifndef ESP_FAST_LCD_H
#define ESP_FAST_LCD_H

#include "stdint.h"
#include "stddef.h"
#include "stdatomic.h"
#include "freertos/FreeRTOS.h"
#include "esp_lcd_panel_dev.h"
#include "esp_fast_lcd_simd.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @brief The capabilities for allocating framebuffers and ring buffer slots.
 */
#define DMA_CAPS (MALLOC_CAP_DMA | MALLOC_CAP_8BIT)

/**
 * @brief The configuration struct of a LCD panel device.
 */
typedef struct {
			uint32_t	frame_size_x;			/*!< The width in pixels of the framebuffer. */
			uint32_t	frame_size_y;			/*!< The height in pixels of the framebuffer. */
			uint32_t	frame_tile_size_x;		/*!< The width in pixels of a frame tile. */
			uint32_t	frame_tile_size_y;		/*!< The height in pixels of a frame tile. */
			uint32_t	ring_buffer_slot_count;	/*!< The count or ring buffer slots, in frames. */
			uint32_t	buffer_flags;			/*!< The extra capability flags when allocating heap buffers (e.g. framebuffer, ring buffer slots). */
	const	char*		name;					/*!< The name of the LCD panel device. */
} esp_fast_lcd_panel_configuration_t;

/**
 * @brief The internal properties struct of the LCD panel device.
 */
typedef struct {
	esp_fast_lcd_panel_configuration_t	configuration;		/*!< The configuration of the LCD panel device. */
	uint32_t							frame_size;			/*!< The count of pixels of the framebuffer. */
	uint32_t							frame_tile_count_x;	/*!< The width of the framebuffer in tiles. */
	uint32_t							frame_tile_count_y;	/*!< The height of framebuffer in tiles. */
	uint32_t							frame_tile_count;	/*!< The count of the tiles in the framebuffer. */
} esp_fast_lcd_panel_properties_t;

/**
 * @brief The info struct of a transmission.
 */
typedef struct {
	uint32_t position_x;	/*!< The top-left origin position X of the region at the panel to be transmitted in pixels. */
	uint32_t position_y;	/*!< The top-left origin position Y of the region at the panel to be transmitted in pixels. */
	uint32_t size_x;		/*!< The width of the region to be transmitted in pixels. */
	uint32_t size_y;		/*!< The height of the region to be transmitted in pixels. */
	uint32_t buffer_offset;	/*!< The offset of the region in the ring buffer slot in bytes. */
} esp_fast_lcd_panel_transmit_t;

/**
 * @brief The struct of the asynchronous transfer queue of a LCD panel device.
 */
typedef struct {
	SemaphoreHandle_t				free_buffer_count;		/*!< The counting semaphore of remaining free ring buffer slots. */
	SemaphoreHandle_t				ring_buffer_lock;		/*!< The binary semaphore lock of the ring buffer. */
	esp_fast_lcd_panel_transmit_t*	pending_transmits;		/*!< The pending ESP_LCD transmits in the ring buffer slot to be sent by the next esp_fast_lcd_transmit() call. */
	atomic_uint*					ring_transmits;			/*!< The array of the count of in-flight transmits of each ring buffer slots. */
	uint16_t*						ring_buffer;			/*!< The SPI DMA transfer ring buffer, in frames. The count of slots of the ring buffer is ringBufferSlots. */
	uint16_t*						framebuffer;			/*!< The off-screen framebuffer to be drawn. */
	uint32_t*						dirty_tiles;			/*!< The 2D bitsets of dirty tiles to be committed. */
	uint32_t						pending_transmit_count;	/*!< The count of pending ESP_LCD transmits in the ring buffer slot to be sent. */
	uint64_t						transmit_index;			/*!< The incrementing ring index of which ring buffer slot is currently transmitting. */
	uint64_t						ring_index;				/*!< The incrementing ring index of which ring buffer slot to be used. */
	uint8_t							frame_dirty;			/*!< True if the frame has changes that are not been committed. */
} esp_fast_lcd_panel_transfer_queue_t;

/**
 * @brief The struct of a LCD panel device.
 */
typedef struct {
	esp_fast_lcd_panel_transfer_queue_t*	transfer_queue;	/*!< The asynchronous transfer queue of the LCD panel device. */
	esp_fast_lcd_panel_properties_t*		properties;		/*!< The internal properties of the LCD panel device */
	esp_lcd_panel_handle_t					handle;			/*!< The handle of the LCD panel device. */
} esp_fast_lcd_panel_device_t;

/**
 * @brief					Draw a pixel on the framebuffer of the given LCD panel device.
 * @param context			The device to be drawn to.
 * @param position_x		The position X of the pixel to be drawn.
 * @param position_y		The position Y of the pixel to be drawn.
 * @param color_rgba8888	The color of the pixel to be drawn in RGBA 8888 format (MSB first).
 * @return					The status of the draw.
 */
esp_err_t esp_fast_lcd_draw_pixel(
	const	esp_fast_lcd_panel_device_t*	context,
			int32_t							position_x,
			int32_t							position_y,
			uint32_t						color_rgba8888
);

/**
 * @brief					Draw a filled rectangle on the framebuffer of the given LCD panel device.
 * @param context			The device to be drawn to.
 * @param position_x		The top-left origin position X of the rectangle to be drawn.
 * @param position_y		The top-left origin position Y of the rectangle to be drawn.
 * @param size_x			The width of the rectangle, in pixels.
 * @param size_y			The height of the rectangle, in pixels.
 * @param color_rgba8888	The color of the rectangle to be drawn in RGBA 8888 format (MSB first).
 * @return					The status of the draw.
*/
esp_err_t esp_fast_lcd_draw_rectangle(
	const	esp_fast_lcd_panel_device_t*	context,
			int32_t							position_x,
			int32_t							position_y,
			uint32_t						size_x,
			uint32_t						size_y,
			uint32_t						color_rgba8888
);

/**
* @brief				Draw a filled rectangle on the framebuffer of the given LCD panel device.
* @param context		The device to be drawn to.
* @param position_x		The top-left origin position X of the rectangle to be drawn.
* @param position_y		The top-left origin position Y of the rectangle to be drawn.
* @param size_x			The width of the rectangle, in pixels.
* @param size_y			The height of the rectangle, in pixels.
* @param color_rgb565	The color of the rectangle to be drawn in RGB 565 format (MSB first).
* @return				The status of the draw.
*/
esp_err_t esp_fast_lcd_draw_native_rectangle(
	const	esp_fast_lcd_panel_device_t*	context,
			int32_t							position_x,
			int32_t							position_y,
			uint32_t						size_x,
			uint32_t						size_y,
			uint16_t						color_rgb565
);

/**
 * @brief					Draw a filled bit-masked rectangle on the framebuffer of the given LCD panel device.
 * @param context			The device to be drawn to.
 * @param position_x		The top-left origin position X of the rectangle to be drawn.
 * @param position_y		The top-left origin position Y of the rectangle to be drawn.
 * @param size_x			The width of the rectangle, in pixels.
 * @param size_y			The height of the rectangle, in pixels.
 * @param bitmask_offset_x	The top-left origin position X of the bitmask slice on the bitmask, in pixels.
 * @param bitmask_offset_y	The top-left origin position Y of the bitmask slice on the bitmask, in pixels.
 * @param bitmask_size_x	The width of the bitmask, in pixels.
 * @param bitmask_flipped	True if the bitmask is flipped. ("Flipped" means that the LSB byte and the MSB byte of the
 *							mask of a pixel has already been flipped to get correct transmission byte order.)
 * @param color_rgba8888	The color of the rectangle to be drawn in RGBA 8888 format (MSB first).
 * @param bitmask_rgb565	The bitmask data of the bitmask slice to be applied to be rectangle to be drawn, in 16 bits-per-pixel.
 * @return					The status of the draw.
*/
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
);

/**
 * @brief					Draw a filled bit-masked rectangle on the framebuffer of the given LCD panel device.
 * @param context			The device to be drawn to.
 * @param position_x		The top-left origin position X of the rectangle to be drawn.
 * @param position_y		The top-left origin position Y of the rectangle to be drawn.
 * @param size_x			The width of the rectangle, in pixels.
 * @param size_y			The height of the rectangle, in pixels.
 * @param bitmask_offset_x	The top-left origin position X of the bitmask slice on the bitmask, in pixels.
 * @param bitmask_offset_y	The top-left origin position Y of the bitmask slice on the bitmask, in pixels.
 * @param bitmask_size_x	The width of the bitmask, in pixels.
 * @param bitmask_flipped	True if the bitmask is flipped. ("Flipped" means that the LSB byte and the MSB byte of the
 *							mask of a pixel has already been flipped to get correct transmission byte order.)
 * @param color_rgb565		The color of the rectangle to be drawn in RGB 565 format (MSB first).
 * @param bitmask_rgb565	The bitmask data of the bitmask slice to be applied to be rectangle to be drawn, in 16 bits-per-pixel.
 * @return					The status of the draw.
*/
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
);

/**
 * @brief						Draw a slice of bitmap on the framebuffer of the given LCD panel device.
 * @param context				The device to be drawn to.
 * @param position_x			The top-left origin position X of the bitmap slice to be drawn on the framebuffer.
 * @param position_y			The top-left origin position Y of the bitmap slice to be drawn on the framebuffer.
 * @param size_x				The width of the bitmap slice, in pixels.
 * @param size_y				The height of the bitmap slice, in pixels.
 * @param bitmap_offset_x		The top-left origin position X of the slice on the bitmap, in pixels.
 * @param bitmap_offset_y		The top-left origin position Y of the slice on the bitmap, in pixels.
 * @param bitmap_size_x			The width of the bitmap, in pixels.
 * @param bitmap_pre_multiplied	True if the bitmap is pre-multiplied. ("Pre-multiplied" means that the R, G, and B components
 *								of a pixel is already pre-multiplied with the alpha of the pixel, and the alpha component
 *								of the pixel is replaced by the 255 - alpha.)
 * @param bitmap_rgba8888		The color data of the bitmap of the slice to be drawn in RGBA 8888 format (MSB first,
 *								no strides between lines).
 * @return						The status of the draw.
*/
esp_err_t esp_fast_lcd_draw_bitmap(
	const	esp_fast_lcd_panel_device_t*	context,
			int32_t							position_x,
			int32_t							position_y,
			uint32_t						size_x,
			uint32_t						size_y,
			uint32_t						bitmap_offset_x,
			uint32_t						bitmap_offset_y,
			uint32_t						bitmap_size_x,
			uint8_t							bitmap_pre_multiplied,
	const	uint32_t*						bitmap_rgba8888
);

/**
 * @brief						Specialized function of drawing a slice of translucent bitmap on the framebuffer of the
 *								given LCD panel device using a pre-multiplied rgb565 bitmap and an inverted a8 bitmap.
 * @param context				The device to be drawn to.
 * @param position_x			The top-left origin position X of the bitmap slice to be drawn on the framebuffer.
 * @param position_y			The top-left origin position Y of the bitmap slice to be drawn on the framebuffer.
 * @param size_x				The width of the bitmap slice, in pixels.
 * @param size_y				The height of the bitmap slice, in pixels.
 * @param bitmap_offset_x		The top-left origin position X of the slice on the bitmap, in pixels.
 * @param bitmap_offset_y		The top-left origin position Y of the slice on the bitmap, in pixels.
 * @param bitmap_size_x			The width of the bitmap, in pixels.
 * @param bitmap_rgb565_pre_mul	The color data of the bitmap of the slice to be drawn in pre-multiplied RGB565 format
 *								(MSB first, no strides between lines).
 * @param bitmap_a8_inv			The alpha data of the bitmap of the slice to be drawn in inverted A8 format (255 - alpha,
 *								no strides between lines) in lower 8-bit (LSB) of the 16-bit pixel.
 * @return						The status of the draw.
*/
esp_err_t esp_fast_lcd_draw_bitmap_rgb565_pre_mul_a8_inv(
	const	esp_fast_lcd_panel_device_t*	context,
			int32_t							position_x,
			int32_t							position_y,
			uint32_t						size_x,
			uint32_t						size_y,
			uint32_t						bitmap_offset_x,
			uint32_t						bitmap_offset_y,
			uint32_t						bitmap_size_x,
	const	uint16_t*						bitmap_rgb565_pre_mul,
	const	uint16_t*						bitmap_a8_inv
);

/**
 * @brief					Draw a slice of native RGB565 bitmap on the framebuffer of the given LCD panel device.
 * @param context			The device to be drawn to.
 * @param position_x		The top-left origin position X of the bitmap slice to be drawn on the framebuffer.
 * @param position_y		The top-left origin position Y of the bitmap slice to be drawn on the framebuffer.
 * @param size_x			The width of the bitmap slice, in pixels.
 * @param size_y			The height of the bitmap slice, in pixels.
 * @param bitmap_offset_x	The top-left origin position X of the slice on the bitmap, in pixels.
 * @param bitmap_offset_y	The top-left origin position Y of the slice on the bitmap, in pixels.
 * @param bitmap_size_x		The width of the bitmap, in pixels.
 * @param bitmap_flipped	True if the bitmap is flipped. ("Flipped" means that the LSB byte and the MSB byte of the
 *							color of a pixel has already been flipped to get correct transmission byte order.)
 * @param bitmap_rgb565		The color data of the bitmap of the slice to be drawn in RGB 565 format (MSB first, no strides
 *							between lines).
 * @return					The status of the draw.
*/
esp_err_t esp_fast_lcd_draw_native_bitmap(
	const	esp_fast_lcd_panel_device_t*	context,
			int32_t							position_x,
			int32_t							position_y,
			uint32_t						size_x,
			uint32_t						size_y,
			uint32_t						bitmap_offset_x,
			uint32_t						bitmap_offset_y,
			uint32_t						bitmap_size_x,
			uint8_t							bitmap_flipped,
	const	uint16_t*						bitmap_rgb565
);

/**
 * @brief						Draw a slice of bit-masked bitmap on the framebuffer of the given LCD panel device.
 * @param context				The device to be drawn to.
 * @param position_x			The top-left origin position X of the bitmap slice to be drawn on the framebuffer.
 * @param position_y			The top-left origin position Y of the bitmap slice to be drawn on the framebuffer.
 * @param size_x				The width of the bitmap slice, in pixels.
 * @param size_y				The height of the bitmap slice, in pixels.
 * @param bitmap_offset_x		The top-left origin position X of the slice on the bitmap, in pixels.
 * @param bitmap_offset_y		The top-left origin position Y of the slice on the bitmap, in pixels.
 * @param bitmap_size_x			The width of the bitmap, in pixels.
 * @param bitmask_offset_x		The top-left origin position X of the bitmask slice on the bitmask, in pixels.
 * @param bitmask_offset_y		The top-left origin position Y of the bitmask slice on the bitmask, in pixels.
 * @param bitmask_size_x		The width of the bitmask, in pixels.
 * @param bitmap_pre_multiplied	True if the bitmap is pre-multiplied. ("Pre-multiplied" means that the R, G, and B components
 *								of a pixel is already pre-multiplied with the alpha of the pixel, and the alpha component
 *								of the pixel is replaced by the 255 - alpha.)
 * @param bitmask_flipped		True if the bitmask is flipped. ("Flipped" means that the LSB byte and the MSB byte of the
 *								bitmask of a pixel has already been flipped to get correct transmission byte order.)
 * @param bitmap_rgba8888		The color data of the bitmap of the slice to be drawn in RGBA 8888 format (MSB first,
 *								no strides between lines).
 * @param bitmask_rgb565		The bitmask data of the bitmask slice to be applied to be bitmap slice to be drawn, in 16
 *								bits-per-pixel.
 * @return						The status of the draw.
*/
esp_err_t esp_fast_lcd_draw_bitmap_masked(
	const	esp_fast_lcd_panel_device_t*	context,
			int32_t							position_x,
			int32_t							position_y,
			uint32_t						size_x,
			uint32_t						size_y,
			uint32_t						bitmap_offset_x,
			uint32_t						bitmap_offset_y,
			uint32_t						bitmap_size_x,
			uint32_t						bitmask_offset_x,
			uint32_t						bitmask_offset_y,
			uint32_t						bitmask_size_x,
			uint8_t							bitmap_pre_multiplied,
			uint8_t							bitmask_flipped,
	const	uint32_t*						bitmap_rgba8888,
	const	uint16_t*						bitmask_rgb565
);

/**
 * @brief						Specialized function of drawing a slice of bit-masked translucent bitmap on the framebuffer
 *								of the given LCD panel device using a pre-multiplied rgb565 bitmap and an inverted
 *								a8 bitmap.
 * @param context				The device to be drawn to.
 * @param position_x			The top-left origin position X of the bitmap slice to be drawn on the framebuffer.
 * @param position_y			The top-left origin position Y of the bitmap slice to be drawn on the framebuffer.
 * @param size_x				The width of the bitmap slice, in pixels.
 * @param size_y				The height of the bitmap slice, in pixels.
 * @param bitmap_offset_x		The top-left origin position X of the slice on the bitmap, in pixels.
 * @param bitmap_offset_y		The top-left origin position Y of the slice on the bitmap, in pixels.
 * @param bitmap_size_x			The width of the bitmap, in pixels.
 * @param bitmask_offset_x		The top-left origin position X of the bitmask slice on the bitmask, in pixels.
 * @param bitmask_offset_y		The top-left origin position Y of the bitmask slice on the bitmask, in pixels.
 * @param bitmask_size_x		The width of the bitmask, in pixels.
 * @param bitmask_flipped		True if the bitmask is flipped. ("Flipped" means that the LSB byte and the MSB byte of the
 *								bitmask of a pixel has already been flipped to get correct transmission byte order.)
 * @param bitmap_rgb565_pre_mul	The color data of the bitmap of the slice to be drawn in pre-multiplied RGB565 format
 *								(MSB first, no strides between lines).
 * @param bitmap_a8_inv			The alpha data of the bitmap of the slice to be drawn in inverted A8 format (255 - alpha,
 *								no strides between lines) in lower 8-bit (LSB) of the 16-bit pixel.
 * @param bitmask_rgb565		The bitmask data of the bitmask slice to be applied to be bitmap slice to be drawn, in 16
 *								bits-per-pixel.
 * @return						The status of the draw.
*/
esp_err_t esp_fast_lcd_draw_bitmap_rgb565_pre_mul_a8_inv_masked(
	const	esp_fast_lcd_panel_device_t*	context,
			int32_t							position_x,
			int32_t							position_y,
			uint32_t						size_x,
			uint32_t						size_y,
			uint32_t						bitmap_offset_x,
			uint32_t						bitmap_offset_y,
			uint32_t						bitmap_size_x,
			uint32_t						bitmask_offset_x,
			uint32_t						bitmask_offset_y,
			uint32_t						bitmask_size_x,
			uint8_t							bitmask_flipped,
	const	uint16_t*						bitmap_rgb565_pre_mul,
	const	uint16_t*						bitmap_a8_inv,
	const	uint16_t*						bitmask_rgb565
);

/**
 * @brief					Draw a slice of bit-masked native RGB565 bitmap on the framebuffer of the given LCD panel device.
 * @param context			The device to be drawn to.
 * @param position_x		The top-left origin position X of the bitmap slice to be drawn on the framebuffer.
 * @param position_y		The top-left origin position Y of the bitmap slice to be drawn on the framebuffer.
 * @param size_x			The width of the bitmap slice, in pixels.
 * @param size_y			The height of the bitmap slice, in pixels.
 * @param bitmap_offset_x	The top-left origin position X of the slice on the bitmap, in pixels.
 * @param bitmap_offset_y	The top-left origin position Y of the slice on the bitmap, in pixels.
 * @param bitmap_size_x		The width of the bitmap, in pixels.
 * @param bitmask_offset_x	The top-left origin position X of the bitmask slice on the bitmask, in pixels.
 * @param bitmask_offset_y	The top-left origin position Y of the bitmask slice on the bitmask, in pixels.
 * @param bitmask_size_x	The width of the bitmask, in pixels.
 * @param bitmap_flipped	True if the bitmap is flipped. ("Flipped" means that the LSB byte and the MSB byte of the
 *							color of a pixel has already been flipped to get correct transmission byte order.)
 * @param bitmask_flipped	True if the bitmask is flipped. ("Flipped" means that the LSB byte and the MSB byte of the
 *							bitmask of a pixel has already been flipped to get correct transmission byte order.)
 * @param bitmap_rgb565		The color data of the bitmap of the slice to be drawn in RGB 565 format (MSB first, no strides
 *							between lines).
 * @param bitmask_rgb565	The bitmask data of the bitmask slice to be applied to be bitmap slice to be drawn, in 16
 *							bits-per-pixel.
 * @return					The status of the draw.
*/
esp_err_t esp_fast_lcd_draw_native_bitmap_masked(
	const	esp_fast_lcd_panel_device_t*	context,
			int32_t							position_x,
			int32_t							position_y,
			uint32_t						size_x,
			uint32_t						size_y,
			uint32_t						bitmap_offset_x,
			uint32_t						bitmap_offset_y,
			uint32_t						bitmap_size_x,
			uint32_t						bitmask_offset_x,
			uint32_t						bitmask_offset_y,
			uint32_t						bitmask_size_x,
			uint8_t							bitmap_flipped,
			uint8_t							bitmask_flipped,
	const	uint16_t*						bitmap_rgb565,
	const	uint16_t*						bitmask_rgb565
);

/**
 * @brief			Commit all uncommitted changes on the framebuffer to the ring buffer slot and wait for transmission.
 * @attention		Only one draw-commit task is allowed, Multiple commits before transmit will result in overwriting the
 *					same ring buffer slot.
 * @param context	The device to be committed.
 * @return			The status of the commit.
 */
esp_err_t esp_fast_lcd_commit(const esp_fast_lcd_panel_device_t* context);

/**
 * @brief			Transfer the pending transmission in the ring buffer slot to the ESP_LCD panel IO devices of the given LCD panel device.
 * @param context	The device to be transferred.
 * @return			The status of the transmission, if error occurred, it is NON-RECOVERABLE, recreate the panel device if needed.
 */
esp_err_t esp_fast_lcd_transmit(const esp_fast_lcd_panel_device_t* context);

/**
 * @brief								Prepare the given RGBA8888 color format bitmap. The R, G, and B color components
 *										of every pixel of the given bitmap will be pre-multiplied with the alpha component
 *										of the corresponding pixel. The alpha component of the pixel will be replaced by
 *										the inverted alpha component (255 - alpha).
 * @param bitmap_rgba8888_src			The bitmap to be prepared.
 * @param bitmap_rgba8888_pre_mul_dst	The bitmap that receives the pre-multiplied result.
 * @param bitmap_size_x					The width of the bitmap.
 * @param bitmap_size_y					The height of the bitmap.
 * @return							The status of the preparation.
 */
esp_err_t esp_fast_lcd_prepare_rgba8888_bitmap_to_rgba8888_pre_mul(
	const	uint32_t*	bitmap_rgba8888_src,
			uint32_t*	bitmap_rgba8888_pre_mul_dst,
			uint32_t	bitmap_size_x,
			uint32_t	bitmap_size_y
);

/**
 * @brief							Prepare the given RGBA8888 color format bitmap, assuming all pixels of the given
 *									bitmap are opaque. The color of the pixel will be converted into RGB565 format
 *									and the LSB byte and the MSB byte will be flipped before writing into output
 *									RGB565 bitmap to get correct transmission byte order.
 * @param bitmap_rgba8888_src		The bitmap to be prepared.
 * @param bitmap_rgb565_flipped_dst	The bitmap that receives the flipped result.
 * @param bitmap_size_x				The width of the bitmap.
 * @param bitmap_size_y				The height of the bitmap.
 * @return							The status of the preparation.
 */
esp_err_t esp_fast_lcd_prepare_rgba8888_bitmap_to_rgb565_flipped(
	const	uint32_t*	bitmap_rgba8888_src,
			uint16_t*	bitmap_rgb565_flipped_dst,
			uint32_t	bitmap_size_x,
			uint32_t	bitmap_size_y
);

/**
 * @brief								Prepare the given RGBA8888 color format bitmap, The color of the pixel will be
 *										converted into RGB565 format and the LSB byte and the MSB byte of the color and
 *										bitmask will be flipped before writing into output bitmask/bitmap to get correct
 *										transmission byte order. Any pixel with alpha below the threshold will be discarded
 *										by marking the corresponding pixel on the output prepared bitmask to zero.
 * @param bitmap_rgba8888_src			The bitmap to be prepared.
 * @param bitmap_rgb565_flipped_dst		The bitmap that receives the flipped result.
 * @param bitmap_bitmask_flipped_dst	The bitmask that receives the flipped mask result.
 * @param bitmap_size_x					The width of the bitmap.
 * @param bitmap_size_y					The height of the bitmap.
 * @param bitmap_alpha_threshold		The threshold of the alpha component to distinguish the visible and invisible pixels.
 * @return								The status of the preparation.
 */
esp_err_t esp_fast_lcd_prepare_rgba8888_bitmap_to_rgb565_masked(
	const	uint32_t*	bitmap_rgba8888_src,
			uint16_t*	bitmap_rgb565_flipped_dst,
			uint16_t*	bitmap_bitmask_flipped_dst,
			uint32_t	bitmap_size_x,
			uint32_t	bitmap_size_y,
			uint8_t		bitmap_alpha_threshold
);

/**
 * @brief							Prepare the given RGBA8888 color format bitmap, The color of the pixel will be
 *									pre-multiplied with the alpha component of the corresponding pixel and converted
 *									into RGB565 format. The LSB byte and the MSB byte will be flipped before writing
 *									the converted color into output RGB565 bitmap to get correct transmission byte
 *									order. The alpha component of the pixel will be inverted (255 - alpha) and extracted
 *									into the lower 8-bit (LSB) data of the 16-bit pixel in the inverted-alpha-only bitmap.
 * @param bitmap_rgba8888_src		The bitmap to be prepared.
 * @param bitmap_rgb565_pre_mul_dst	The bitmap that receives the pre-multiplied result.
 * @param bitmap_a8_inv_dst			The inverted-alpha-only bitmap that receives the inverted alpha result.
 * @param bitmap_size_x				The width of the bitmap.
 * @param bitmap_size_y				The height of the bitmap.
 * @return							The status of the preparation.
 */
esp_err_t esp_fast_lcd_prepare_rgba8888_bitmap_to_rgb565_pre_mul_a8_inv(
	const	uint32_t*	bitmap_rgba8888_src,
			uint16_t*	bitmap_rgb565_pre_mul_dst,
			uint16_t*	bitmap_a8_inv_dst,
			uint32_t	bitmap_size_x,
			uint32_t	bitmap_size_y
);

/**
 * @brief							Prepare the given RGB565 color format bitmap, the LSB byte and the MSB byte of
 *									the color of every pixel will be flipped before writing into output RGB565 bitmap
 *									to get correct transmission byte order.
 * @param bitmap_rgb565_src			The bitmap to be prepared.
 * @param bitmap_rgb565_flipped_dst	The bitmap that receives the flipped result.
 * @param bitmap_size_x				The width of the bitmap.
 * @param bitmap_size_y				The height of the bitmap.
 * @return							The status of the preparation.
 */
esp_err_t esp_fast_lcd_prepare_rgb565_bitmap_to_rgb565_flipped(
	const	uint16_t*	bitmap_rgb565_src,
			uint16_t*	bitmap_rgb565_flipped_dst,
			uint32_t	bitmap_size_x,
			uint32_t	bitmap_size_y
);

/**
 * @brief								Create the LCD panel device with the given LCD panel device and the
 *										LCD panel device configuration.
 * @param panel_device_ret				The handle to receive the created LCD panel device.
 *										initialized panel device data.
 * @param panel_device_configuration	The configuration for initializing the LCD panel device.
 * @param panel_handle					The ESP_LCD panel device handle of the LCD panel device.
 * @param panel_io						The ESP_LCD panel IO handle of the LCD panel device.
 * @return								The status of the creation.
 */
esp_err_t esp_fast_lcd_new_lcd_panel_device(
	esp_fast_lcd_panel_device_t**		panel_device_ret,
	esp_fast_lcd_panel_configuration_t	panel_device_configuration,
	esp_lcd_panel_handle_t				panel_handle,
	esp_lcd_panel_io_handle_t			panel_io
);

/**
 * @brief					Release the given LCD panel device.
 * @attention				Stop the transmission task and release the ESP_LCD panel and IO handles BEFORE calling this
 *							function.
 * @param panel_device_in	The LCD panel device handle to be released.
 * @return					The status of the releasing.
 */
esp_err_t esp_fast_lcd_del_lcd_panel_device(esp_fast_lcd_panel_device_t* panel_device_in);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ESP_FAST_LCD_H
