#ifndef AREX_SUBMENU_MODEL_H
#define AREX_SUBMENU_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AREX_SUBMENU_INFO_COUNT   5
#define AREX_SUBMENU_SETUP_COUNT  6

const char *arex_submenu_info_title(uint8_t index);
const char **arex_submenu_build_info_items(uint8_t index, uint8_t *out_count);

const char *arex_submenu_setup_title(uint8_t index);
int8_t arex_submenu_setup_index_for_title(const char *title);
const char **arex_submenu_build_setup_items(uint8_t index, uint8_t *out_count);

const char **arex_submenu_build_compass_cal_items(uint8_t *out_count);
const char **arex_submenu_nested_items_for(const char *title, uint8_t *out_count);

bool arex_submenu_is_readonly_info_title(const char *title);

#ifdef __cplusplus
}
#endif

#endif /* AREX_SUBMENU_MODEL_H */
