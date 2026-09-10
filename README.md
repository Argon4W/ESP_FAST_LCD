# ESP_FAST_LCD

Fast LCD rendering infrastructure specialized for ESP32-S3 using transmission-order framebuffer, ring buffers, dirty tiles, 
and asynchronous DMA transmission on top of the mature abstraction of [esp_lcd](https://github.com/espressif/esp-idf/tree/v6.1/components/esp_lcd).

## Advantage

This library makes full use of the SIMD/SWAR and precomputed bitmaps to accelerate the frontend framebuffer draw operations
while separating the backend transmission into an asynchronous DMA ring buffer transfer queue to allow transmission tasks 
to run independently of the main logic thread.

---

## Feature
- Batched commit with run-length merged dirty tiles transmission.
- Accelerated opaque RGB565 rectangle fill/masked-fill with SIMD/SWAR/memcpy.
- Accelerated translucent RGBA8888 bitmap alpha-blended draw/masked-draw with SIMD.
- Multiple bitmap preparation functions and corresponding draw functions for further accelerating bitmap draws.
- Accelerated text rendering engine using the masked rectangle fill providing 2-level cached fast text and outlined text rendering.
- Built-in modified version of Unifont 17.0.05.

## Usage

The following example shows how to use this library to draw rectangles and texts then commit changes to the esp_lcd.

```c++
#include "esp_fast_lcd.h"
#include "esp_fast_text_engine.h"

void example() {
    // Set up the configuration of the panel device.
    esp_fast_lcd_panel_configuration_t panel_device_config = {
        .frame_size_x           = 96U,                  // The width of the panel in pixels.
        .frame_size_y           = 54U,                  // The height of the panel in pixels.
        .frame_tile_size_x      = 16U,                  // The width of a dirty tile in pixels.
        .frame_tile_size_y      = 18U,                  // The height of a dirty tile in pixels.
        .ring_buffer_slot_count = 3U,                   // The count of ring buffer slots of the transfer queue.
        .name                   = "device",             // The name of the panel device used for debugging.
        .buffer_flags           = MALLOC_CAP_INTERNAL   // The extra capability flags used to allocate the framebuffer and ring buffers.
    };
    
    // Set up the configuration of the text engine instance.
    esp_fast_text_engine_instance_configuration_t text_engine_config = {
        .crlf_mode				= false,        // True if using CRLF for new lines when drawing texts.
        .font_size_scale		= 1,            // The size multiplier of the glyph.
        .font_outline_radius	= 1,            // The outline radius in pixels of the text when drawing outlined text.
        .iram_atlas_slot_count	= 16,           // The count of slots in L1 cache.
        .psram_atlas_slot_count	= 64,           // The count of slots in L2 cache.
        .string_buffer_size		= 32,           // The size of the formatted string buffer in bytes.
        .atlas_flags			= 0U,           // The extra capability flags used to allocate glyph atlases.
        .name					= "text_engine" // The name of the text engine used for debugging.
    };
    
    // Set up the font used by the text engine to the built-in modified Unifont.
    esp_fast_text_engine_font_t font = {
        .size_x_max     = 16,                           // The maximum width of the glyph in Unifont is 16.
        .size_y         = 16,                           // The height of the glyph in Unifont is 16.
        .glyph_data     = unifont_17_0_05_1bpp_bits,    // The built-in tightly packed 1bpp LSB-first modified Unifont data.
        .glyph_table    = glyph_table,                  // The built-in look-up table of the modified Unifont.
    };

    // Reserve the handles of the panel device and text engine instance.
    esp_fast_lcd_panel_device_t*        fast_lcd_panel_device;
    esp_fast_text_engine_instance_t*    text_engine_instance;
    
    // Create the panel device and text engine instance.
    esp_fast_lcd_new_lcd_panel_device(
        /* panel_device_ret             = */ &fast_lcd_panel_device,
        /* panel_device_configuration   = */ panel_device_config,
        /* panel_handle                 = */ ...,   // The esp_lcd panel handle of the panel device.
        /* panel_io                     = */ ...    // The esp_lcd panel IO handle of the panel device.
    );
	esp_fast_text_engine_new_text_engine_instance(
        /* engine_instance_ret              = */ &text_engine_instance, 
        /* engine_instance_configuration    = */ text_engine_config, 
        /* engine_instance_font             = */ font
    );
    
    // Draw a full-screen green rectangle on the framebuffer.
    esp_fast_lcd_draw_rectangle(
        /* context          = */ fast_lcd_panel_device,
        /* position_x       = */ 0,
        /* position_y       = */ 0,
        /* size_x           = */ 96U,
        /* size_y           = */ 54U,
        /* color_rgba8888   = */ 0x00FF00FFU
    );
    
    // Draw an black-outlined red "Hello World" on the framebuffer.
    esp_fast_text_engine_draw_outlined_string_fmt(
        /* text_engine_context      = */ text_engine_instance,
        /* panel_device_context     = */ fast_lcd_panel_device,
        /* position_x               = */ 0,
        /* position_y               = */ 0,
        /* color_rgba8888           = */ 0xFF0000FFU,
        /* color_outline_rgba8888   = */ 0x000000FFU,
        /* string                   = */ "Hello\n%s",
        "World"
    );
    
    // Commit changes to the esp_lcd.
    esp_fast_lcd_commit(fast_lcd_panel_device);
}
```

## License
- Source code is licensed under MIT License.
- `src/fast_text_engine/unifont-17.0.05.c` is licensed under SIL Open Font License (OFL) version 1.1.