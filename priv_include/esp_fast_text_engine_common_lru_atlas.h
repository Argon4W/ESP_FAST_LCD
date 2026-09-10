#ifndef ESP_FAST_TEXT_ENGINE_COMMON_LRU_ATLAS_H
#define ESP_FAST_TEXT_ENGINE_COMMON_LRU_ATLAS_H

#include "stdint.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @brief		Private function of moving a given LRU linked node to the head of its LRU atlas linked list.
 * @param atlas	The LRU atlas of the linked node.
 * @param node	The node to be moved to the head of its linked list.
 */
void private_move_lru_node_to_head(
	esp_fast_text_engine_lru_atlas_t*	atlas,
	esp_fast_text_engine_lru_node_t*	node
);

/**
 * @brief		Private function of moving a given LRU linked node to the tail of its LRU atlas linked list.
 * @param atlas	The LRU atlas of the linked node.
 * @param node	The node to be moved to the tail of its linked list.
 */
void private_move_lru_node_to_tail(
	esp_fast_text_engine_lru_atlas_t*	atlas,
	esp_fast_text_engine_lru_node_t*	node
);

/**
 * @brief					Private function of creating a new LRU atlas.
 * @param atlas_ret			The handle to receive created LRU atlas.
 * @param name				The name of the LRU atlas, used in debug logging.
 * @param slot_count		The count of atlas slot in the LRU atlas.
 * @param slot_size			The count of pixels of an LRU atlas slot.
 * @param slot_buffer_flags	The buffer flags when allocating heap slot buffers.
 * @param info_buffer_flags	The buffer flags when allocating heap slot infos.
 * @return					The status of the creation.
 */
esp_err_t private_new_lru_atlas(
			esp_fast_text_engine_lru_atlas_t**	atlas_ret,
	const	char*								name,
			uint32_t							slot_count,
			uint32_t							slot_size,
			uint32_t							slot_buffer_flags,
			uint32_t							info_buffer_flags
);

/**
 * @brief			Private function of releasing a given LRU atlas.
 * @param atlas_in	The LRU atlas handle to be released.
 * @return			The status of the releasing.
 */
esp_err_t private_del_lru_atlas(esp_fast_text_engine_lru_atlas_t* atlas_in);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ESP_FAST_TEXT_ENGINE_COMMON_LRU_ATLAS_H
