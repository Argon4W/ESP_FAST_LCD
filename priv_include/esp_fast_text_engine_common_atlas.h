#ifndef ESP_FAST_TEXT_ENGINE_COMMON_ATLAS_H
#define ESP_FAST_TEXT_ENGINE_COMMON_ATLAS_H

#include "stdint.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @brief					Private function of creating a new atlas.
 * @param atlas_ret			The handle to receive created atlas.
 * @param name				The name of the atlas, used in debug logging.
 * @param slot_count		The count of atlas slots in the atlas.
 * @param slot_size			The count of pixels of an atlas slot.
 * @param slot_buffer_flags	The buffer flags when allocating heap slot buffers.
 * @param info_buffer_flags	The buffer flags when allocating heap slot infos.
 * @return					The status of the creation.
 */
esp_err_t private_new_atlas(
			esp_fast_text_engine_atlas_t**	atlas_ret,
	const	char*							name,
			uint32_t						slot_count,
			uint32_t						slot_size,
			uint32_t						slot_buffer_flags,
			uint32_t						info_buffer_flags
);

/**
 * @brief			Private function of releasing a given atlas.
 * @param atlas_in	The atlas handle to be released.
 * @return			The status of the releasing.
 */
esp_err_t private_del_atlas(esp_fast_text_engine_atlas_t* atlas_in);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ESP_FAST_TEXT_ENGINE_COMMON_ATLAS_H
