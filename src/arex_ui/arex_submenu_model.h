#ifndef AREX_SUBMENU_MODEL_H
#define AREX_SUBMENU_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AREX_SUBMENU_INFO_COUNT   5
#define AREX_SUBMENU_SETUP_COUNT  6

typedef enum
{
    AREX_SUBMENU_SETTING_NONE = 0,
    AREX_SUBMENU_SETTING_SALINITY,
    AREX_SUBMENU_SETTING_SAFETY_STOP,
    AREX_SUBMENU_SETTING_ALTITUDE,
} arex_submenu_setting_kind_t;

typedef struct
{
    arex_submenu_setting_kind_t kind;
    uint8_t value;
    char body[48];
} arex_submenu_setting_confirm_t;

const char *arex_submenu_info_title(uint8_t index);
const char **arex_submenu_build_info_items(uint8_t index, uint8_t *out_count);

const char *arex_submenu_setup_title(uint8_t index);
int8_t arex_submenu_setup_index_for_title(const char *title);
const char **arex_submenu_build_setup_items(uint8_t index, uint8_t *out_count);

const char **arex_submenu_build_compass_cal_items(uint8_t *out_count);
const char **arex_submenu_nested_items_for(const char *title, uint8_t *out_count);
const char **arex_submenu_child_items_for(const char *current_title,
                                          uint8_t item_index,
                                          const char *item_text,
                                          char *out_title,
                                          uint8_t out_title_size,
                                          uint8_t *out_count);

bool arex_submenu_is_readonly_info_title(const char *title);
bool arex_submenu_setting_from_selection(const char *current_title,
                                         uint8_t item_index,
                                         const char *item_text,
                                         arex_submenu_setting_confirm_t *out_setting);
void arex_submenu_apply_setting(arex_submenu_setting_kind_t kind, uint8_t value);
uint8_t arex_submenu_safety_stop_depth_m(uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* AREX_SUBMENU_MODEL_H */
