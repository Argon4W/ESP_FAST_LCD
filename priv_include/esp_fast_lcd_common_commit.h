#ifndef ESP_FAST_LCD_COMMON_BITMAP_H
#define ESP_FAST_LCD_COMMON_BITMAP_H

#include "stdint.h"
#include "stddef.h"
#include "esp_lcd_panel_dev.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @brief					Callback function invoked when panel IO finishes transferring color data. It tells the LCD
 *							panel device handle that a new ring buffer slot is available.
 * @param panel_io_handle	LCD panel IO handle, which is created by factory API like `esp_lcd_new_panel_io_spi()`.
 * @param panel_io_event	Panel IO event data, fed by driver.
 * @param user_handle		User data, passed from `esp_lcd_panel_io_xxx_config_t`.
 * @return					Whether a high priority task has been waken up by this function.
 */
IRAM_ATTR bool private_on_commit_done(
	esp_lcd_panel_io_handle_t		panel_io_handle,
	esp_lcd_panel_io_event_data_t*	panel_io_event,
	void*							user_handle
);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ESP_FAST_LCD_COMMON_BITMAP_H