#ifndef ESP_FAST_TEXT_ENGINE_H
#define ESP_FAST_TEXT_ENGINE_H

#include "stdint.h"
#include "stddef.h"
#include "unifont-17.0.05.h"
#include "esp_fast_lcd.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @brief The sentinel codepoint of an empty LRU node.
 */
#define EMPTY_CODEPOINT_SENTINEL 0xFFFFU

/**
 * @brief The capabilities for allocating IRAM LRU atlas linked list.
 */
#define L1_LRU_CAPS MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL

/**
 * @brief The capabilities for allocating PSRAM LRU atlas linked list.
 */
#define L2_LRU_CAPS MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM

/**
 * @brief The capabilities for allocating IRAM LRU atlas pixel buffer.
 */
#define L1_ATLAS_CAPS DMA_CAPS | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT

/**
 * @brief The capabilities for allocating PSRAM LRU atlas pixel buffer.
 */
#define L2_ATLAS_CAPS DMA_CAPS | MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT

/**
 * @brief The struct of a font.
 */
typedef struct {
			uint32_t		size_x_max;		/*!< The max width of a glyph could be in pixels. */
			uint32_t		size_y;			/*!< The height of a glyph in pixels. */
	const	uint8_t*		glyph_data;		/*!< The pointer to the packed 1bpp LSBit first font glyph data. */
	const	glyph_info_t**	glyph_table;	/*!< The pointer to the two-level glyph-block lookup table. */
} esp_fast_text_engine_font_t;

/**
 * @brief The configuration struct of a text engine instance.
 */
typedef struct {
			uint32_t	crlf_mode;				/*!< True if using \\r\\n for new lines. */
			uint32_t	font_size_scale;		/*!< The size multiplier of the font. A 16x16 pixels glyph will be 32x32 pixels in font size 2. */
			uint32_t	font_outline_radius;	/*!< The outline radius of the font in pixels. */
			uint32_t	iram_atlas_slot_count;	/*!< The count of atlas slots in internal RAM (IRAM) LRU atlas. */
			uint32_t	psram_atlas_slot_count;	/*!< The count of atlas slots in PSRAM LRU atlas. */
			uint32_t	string_buffer_size;		/*!< The size of the formatting string buffer of the text engine instance. It determines the longest formatted string that can be drawn. */
			uint32_t	atlas_flags;			/*!< The extra capability flags when allocating heap buffers (e.g. iram and psram atlases). */
	const	char*		name;					/*!< The name of the text engine instance. */
} esp_fast_text_engine_instance_configuration_t;

/**
 * @brief The internal properties struct of the text engine instance.
 */
typedef struct {
			esp_fast_text_engine_font_t						font;				/*!< The font used by the text engine instance. */
			esp_fast_text_engine_instance_configuration_t	configuration;		/*!< The configuration of the text engine instance */
			uint32_t										atlas_glyph_size_x;	/*!< The width of an atlas glyph slo in pixels. */
			uint32_t										atlas_glyph_size_y;	/*!< The height of an atlas glyph slot in pixels. */
			uint32_t										atlas_glyph_size;	/*!< The count of pixels of a regular baked glyph in a slot. */
			uint32_t										atlas_slot_size;	/*!< The count of pixels of an atlas glyph slot. */
} esp_fast_text_engine_instance_properties_t;

/**
 * @brief The info struct of a baked glyph atlas slot.
 */
typedef struct {
	uint16_t*	buffer;		/*!< The pointer to the buffer of the atlas slot. */
	uint32_t	size_x;		/*!< The width in pixels of the baked glyph in the atlas slot. */
	uint16_t	codepoint;	/*!< The codepoint of the baked glyph in the atlas slot. */
} esp_fast_text_engine_atlas_slot_t;

/**
 * @brief The common baked glyph atlas struct.
 */
typedef struct {
	const	char*								name;		/*!< The name of the atlas. */
			esp_fast_text_engine_atlas_slot_t*	slots;		/*!< The array of atlas slot infos in the atlas, in slot index order. */
			uint16_t*							buffer;		/*!< The pixel buffer of the atlas slots in the atlas. */
			uint32_t							slot_size;	/*!< The count of pixels of an atlas slot in the atlas. */
			uint32_t							slot_count;	/*!< The count of atlas slots in the atlas. */
} esp_fast_text_engine_atlas_t;

/**
 * @brief The linked list node of LRU cache in the LRU atlas.
 */
typedef struct esp_fast_text_engine_lru_node esp_fast_text_engine_lru_node_t;

/**
 * @brief The linked list node of LRU cache in the LRU atlas.
 */
struct esp_fast_text_engine_lru_node {
	esp_fast_text_engine_lru_node_t*	prev; /*!< Pointer to the previous linked list node. */
	esp_fast_text_engine_lru_node_t*	next; /*!< Pointer to the next linked list node. */
	esp_fast_text_engine_atlas_slot_t*	slot; /*!< The internal atlas slot of the node. */
};

/**
 * @brief The LRU baked glyph atlas struct of the text engine instance.
 */
typedef struct {
			esp_fast_text_engine_lru_node_t*	head;	/*!< Pointer to the head node of the LRU linked list in the atlas. */
			esp_fast_text_engine_lru_node_t*	tail;	/*!< Pointer to the tail node of the LRU linked list in the atlas. */
			esp_fast_text_engine_lru_node_t*	nodes;	/*!< Array of all nodes of the LRU linked list in the atlas in slot index order. */
			esp_fast_text_engine_atlas_t*		atlas;	/*!< The internal baked glyph atlas of the LRU atlas. */
} esp_fast_text_engine_lru_atlas_t;

/**
 * @brief The context struct of a text engine instance.
 */
typedef struct {
	esp_fast_text_engine_instance_properties_t*	properties;			/*!< The internal properties of the text engine instance. */
	esp_fast_text_engine_lru_atlas_t*			iram_lru_atlas;		/*!< The L1 LRU atlas in internal RAM (IRAM). */
	esp_fast_text_engine_lru_atlas_t*			psram_lru_atlas;	/*!< The L2 LRU atlas in PSRAM. */
	esp_fast_text_engine_atlas_t*				ascii_atlas;		/*!< The resident atlas of ASCII characters in RGB565 bitmask, max_size_x * 128. */
	uint16_t*									swap_buffer;		/*!< The scratch pixel buffer of the atlas for swapping baked glyphs between L1 and L2 atlas. */
	char*										string_buffer;		/*!< The formatting string buffer of the text engine instance. */
} esp_fast_text_engine_instance_t;

/**
 * @brief						Draw a glyph of given codepoint on a given LCD panel device.
 * @param text_engine_context	The text engine to draw the glyph.
 * @param panel_device_context	The LCD panel the glyph to be drawn to.
 * @param codepoint				The codepoint of the glyph to be drawn.
 * @param position_x			The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y			The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgba8888		The color of the glyph to be drawn in RGBA 8888 format (MSB first).
 * @param advance_x				Advance the position X in pixels after the glyph is drawn, can be NULL if no need.
 * @return						The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_glyph(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			uint16_t							codepoint,
			int32_t								position_x,
			int32_t								position_y,
			uint32_t							color_rgba8888,
			int32_t*							advance_x
);

/**
 * @brief						Draw a glyph of given codepoint on a given LCD panel device.
 * @param text_engine_context	The text engine to draw the glyph.
 * @param panel_device_context	The LCD panel the glyph to be drawn to.
 * @param codepoint				The codepoint of the glyph to be drawn.
 * @param position_x			The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y			The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgb565			The color of the glyph to be drawn in RGB 565 format (MSB first).
 * @param advance_x				Advance the position X in pixels after the glyph is drawn, can be NULL if no need.
 * @return						The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_native_glyph(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			uint16_t							codepoint,
			int32_t								position_x,
			int32_t								position_y,
			uint16_t							color_rgb565,
			int32_t*							advance_x
);

/**
 * @brief							Draw an outlined glyph of given codepoint on a given LCD panel device.
 * @param text_engine_context		The text engine to draw the glyph.
 * @param panel_device_context		The LCD panel the glyph to be drawn to.
 * @param codepoint					The codepoint of the glyph to be drawn.
 * @param position_x				The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y				The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgba8888			The color of the glyph to be drawn in RGBA 8888 format (MSB first).
 * @param color_outline_rgba8888	The outline color of the glyph to be drawn in RGBA 8888 format (MSB first).
 * @param advance_x					Advance the position X in pixels after the glyph is drawn, can be NULL if no need.
 * @return							The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_outlined_glyph(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			uint16_t							codepoint,
			int32_t								position_x,
			int32_t								position_y,
			uint32_t							color_rgba8888,
			uint32_t							color_outline_rgba8888,
			int32_t*							advance_x
);

/**
 * @brief						Draw an outlined glyph of given codepoint on a given LCD panel device.
 * @param text_engine_context	The text engine to draw the glyph.
 * @param panel_device_context	The LCD panel the glyph to be drawn to.
 * @param codepoint				The codepoint of the glyph to be drawn.
 * @param position_x			The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y			The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgb565			The color of the glyph to be drawn in RGB 565 format (MSB first).
 * @param color_outline_rgb565	The outline color of the glyph to be drawn in RGB 565 format (MSB first).
 * @param advance_x				Advance the position X in pixels after the glyph is drawn, can be NULL if no need.
 * @return						The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_native_outlined_glyph(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			uint16_t							codepoint,
			int32_t								position_x,
			int32_t								position_y,
			uint16_t							color_rgb565,
			uint16_t							color_outline_rgb565,
			int32_t*							advance_x
);

/**
 * @brief						Draw a string on a given LCD panel device.
 * @param text_engine_context	The text engine to draw the glyph.
 * @param panel_device_context	The LCD panel the glyph to be drawn to.
 * @param position_x			The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y			The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgba8888		The color of the string to be drawn in RGBA 8888 format (MSB first).
 * @param string				The string to be drawn
 * @return						The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_string(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			int32_t								position_x,
			int32_t								position_y,
			uint32_t							color_rgba8888,
			char*								string
);

/**
 * @brief						Draw a string on a given LCD panel device.
 * @param text_engine_context	The text engine to draw the glyph.
 * @param panel_device_context	The LCD panel the glyph to be drawn to.
 * @param position_x			The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y			The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgb565			The color of the string to be drawn in RGB 565 format (MSB first).
 * @param string				The string to be drawn.
 * @return						The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_native_string(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			int32_t								position_x,
			int32_t								position_y,
			uint16_t							color_rgb565,
			char*								string
);

/**
 * @brief							Draw an outlined string on a given LCD panel device.
 * @param text_engine_context		The text engine to draw the glyph.
 * @param panel_device_context		The LCD panel the glyph to be drawn to.
 * @param position_x				The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y				The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgba8888			The color of the string to be drawn in RGBA 8888 format (MSB first).
 * @param color_outline_rgba8888	The outline color of the glyph to be drawn in RGBA 8888 format (MSB first).
 * @param string					The string to be drawn.
 * @return							The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_outlined_string(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			int32_t								position_x,
			int32_t								position_y,
			uint32_t							color_rgba8888,
			uint32_t							color_outline_rgba8888,
			char*								string
);

/**
 * @brief						Draw an outlined string on a given LCD panel device.
 * @param text_engine_context	The text engine to draw the glyph.
 * @param panel_device_context	The LCD panel the glyph to be drawn to.
 * @param position_x			The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y			The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgb565			The color of the string to be drawn in RGB 565 format (MSB first).
 * @param color_outline_rgb565	The outline color of the string to be drawn in RGB 565 format (MSB first).
 * @param string				The string to be drawn.
 * @return						The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_native_outlined_string(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			int32_t								position_x,
			int32_t								position_y,
			uint16_t							color_rgb565,
			uint16_t							color_outline_rgb565,
			char*								string
);

/**
 * @brief						Draw a formatted string on a given LCD panel device.
 * @param text_engine_context	The text engine to draw the glyph.
 * @param panel_device_context	The LCD panel the glyph to be drawn to.
 * @param position_x			The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y			The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgba8888		The color of the string to be drawn in RGBA 8888 format (MSB first).
 * @param string				The string to be drawn
 * @param ...					Optional arguments to be formatted according to the format string.
 * @return						The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_string_fmt(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			int32_t								position_x,
			int32_t								position_y,
			uint32_t							color_rgba8888,
			char*								string,
			...
);

/**
 * @brief						Draw a formatted string on a given LCD panel device.
 * @param text_engine_context	The text engine to draw the glyph.
 * @param panel_device_context	The LCD panel the glyph to be drawn to.
 * @param position_x			The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y			The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgb565			The color of the string to be drawn in RGB 565 format (MSB first).
 * @param string				The string to be drawn.
 * @param ...					Optional arguments to be formatted according to the format string.
 * @return						The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_native_string_fmt(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			int32_t								position_x,
			int32_t								position_y,
			uint16_t							color_rgb565,
			char*								string,
			...
);

/**
 * @brief							Draw an outlined formatted string on a given LCD panel device.
 * @param text_engine_context		The text engine to draw the glyph.
 * @param panel_device_context		The LCD panel the glyph to be drawn to.
 * @param position_x				The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y				The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgba8888			The color of the string to be drawn in RGBA 8888 format (MSB first).
 * @param color_outline_rgba8888	The outline color of the glyph to be drawn in RGBA 8888 format (MSB first).
 * @param string					The string to be drawn.
 * @param ...						Optional arguments to be formatted according to the format string.
 * @return							The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_outlined_string_fmt(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			int32_t								position_x,
			int32_t								position_y,
			uint32_t							color_rgba8888,
			uint32_t							color_outline_rgba8888,
			char*								string,
			...
);

/**
 * @brief						Draw an outlined formatted string on a given LCD panel device.
 * @param text_engine_context	The text engine to draw the glyph.
 * @param panel_device_context	The LCD panel the glyph to be drawn to.
 * @param position_x			The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y			The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgb565			The color of the string to be drawn in RGB 565 format (MSB first).
 * @param color_outline_rgb565	The outline color of the string to be drawn in RGB 565 format (MSB first).
 * @param string				The string to be drawn.
 * @param ...					Optional arguments to be formatted according to the format string.
 * @return						The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_native_outlined_string_fmt(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			int32_t								position_x,
			int32_t								position_y,
			uint16_t							color_rgb565,
			uint16_t							color_outline_rgb565,
			char*								string,
			...
);

/**
 * @brief						Draw a formatted string on a given LCD panel device.
 * @param text_engine_context	The text engine to draw the glyph.
 * @param panel_device_context	The LCD panel the glyph to be drawn to.
 * @param position_x			The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y			The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgba8888		The color of the string to be drawn in RGBA 8888 format (MSB first).
 * @param string				The string to be drawn
 * @param args					Arguments to be formatted according to the format string in va_list.
 * @return						The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_string_vfmt(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			int32_t								position_x,
			int32_t								position_y,
			uint32_t							color_rgba8888,
			char*								string,
			va_list								args
);

/**
 * @brief						Draw a formatted string on a given LCD panel device.
 * @param text_engine_context	The text engine to draw the glyph.
 * @param panel_device_context	The LCD panel the glyph to be drawn to.
 * @param position_x			The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y			The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgb565			The color of the string to be drawn in RGB 565 format (MSB first).
 * @param string				The string to be drawn.
 * @param args					Arguments to be formatted according to the format string in va_list.
 * @return						The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_native_string_vfmt(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			int32_t								position_x,
			int32_t								position_y,
			uint16_t							color_rgb565,
			char*								string,
			va_list								args
);

/**
 * @brief							Draw an outlined formatted string on a given LCD panel device.
 * @param text_engine_context		The text engine to draw the glyph.
 * @param panel_device_context		The LCD panel the glyph to be drawn to.
 * @param position_x				The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y				The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgba8888			The color of the string to be drawn in RGBA 8888 format (MSB first).
 * @param color_outline_rgba8888	The outline color of the glyph to be drawn in RGBA 8888 format (MSB first).
 * @param string					The string to be drawn.
 * @param args						Arguments to be formatted according to the format string in va_list.
 * @return							The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_outlined_string_vfmt(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			int32_t								position_x,
			int32_t								position_y,
			uint32_t							color_rgba8888,
			uint32_t							color_outline_rgba8888,
			char*								string,
			va_list								args
);

/**
 * @brief						Draw an outlined formatted string on a given LCD panel device.
 * @param text_engine_context	The text engine to draw the glyph.
 * @param panel_device_context	The LCD panel the glyph to be drawn to.
 * @param position_x			The upper-left origin position X of the glyph to draw in pixels.
 * @param position_y			The upper-left origin position Y of the glyph to draw in pixels.
 * @param color_rgb565			The color of the string to be drawn in RGB 565 format (MSB first).
 * @param color_outline_rgb565	The outline color of the string to be drawn in RGB 565 format (MSB first).
 * @param string				The string to be drawn.
 * @param args					Arguments to be formatted according to the format string in va_list.
 * @return						The status of the draw.
 */
esp_err_t esp_fast_text_engine_draw_native_outlined_string_vfmt(
	const	esp_fast_text_engine_instance_t*	text_engine_context,
	const	esp_fast_lcd_panel_device_t*		panel_device_context,
			int32_t								position_x,
			int32_t								position_y,
			uint16_t							color_rgb565,
			uint16_t							color_outline_rgb565,
			char*								string,
			va_list								args
);

/**
 * @brief								Create a text engine instance with given text engine configuration and font.
 * @param engine_instance_ret			The handle to receive the created text engine instance.
 * @param engine_instance_configuration	The configuration for initializing the text engine instance.
 * @param engine_instance_font			The font of the text engine instance.
 * @return								The status of the creation.
 */
esp_err_t esp_fast_text_engine_new_text_engine_instance(
	esp_fast_text_engine_instance_t**				engine_instance_ret,
	esp_fast_text_engine_instance_configuration_t	engine_instance_configuration,
	esp_fast_text_engine_font_t						engine_instance_font
);

/**
 * @brief						Release the given text engine instance.
 * @param engine_instance_in	The text engine instance handle to be released.
 * @return						The status of the releasing.
 */
esp_err_t esp_fast_text_engine_del_text_engine_instance(esp_fast_text_engine_instance_t* engine_instance_in);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif //ESP_FAST_TEXT_ENGINE_H
