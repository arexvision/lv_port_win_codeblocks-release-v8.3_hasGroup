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
    AREX_SUBMENU_SETTING_LAST_DECO,
    AREX_SUBMENU_SETTING_ALTITUDE,
    AREX_SUBMENU_SETTING_DIVE_MODE,
    AREX_SUBMENU_SETTING_NITROX_O2,
    AREX_SUBMENU_SETTING_3GAS_O2,
    AREX_SUBMENU_SETTING_3GAS_COUNT,
    AREX_SUBMENU_SETTING_OC_TECH_GAS,
    AREX_SUBMENU_SETTING_AI_PAIR,
    AREX_SUBMENU_SETTING_AI_TANK_STATE,
    AREX_SUBMENU_SETTING_GTR_MODE,
    AREX_SUBMENU_SETTING_MOD_PPO2,
    AREX_SUBMENU_SETTING_DEPTH_ALARM,
    AREX_SUBMENU_SETTING_TIME_ALARM,
    AREX_SUBMENU_SETTING_NDL_ALARM,
    AREX_SUBMENU_SETTING_VIBRATION_TEST,
    AREX_SUBMENU_SETTING_UNITS,
    AREX_SUBMENU_SETTING_DATETIME_FIELD,
    AREX_SUBMENU_SETTING_DATETIME_ACTION,
    AREX_SUBMENU_SETTING_LOG_RATE,
    AREX_SUBMENU_SETTING_BLUETOOTH,
    AREX_SUBMENU_SETTING_RESET_DEFAULTS,
} arex_submenu_setting_kind_t;

typedef struct
{
    arex_submenu_setting_kind_t kind;
    uint8_t arg;
    uint16_t value;
    char body[48];
} arex_submenu_setting_confirm_t;

typedef struct
{
    arex_submenu_setting_kind_t kind;
    uint8_t arg;
    float value;
    float min;
    float max;
    float step;
    uint8_t decimals;
    char label[20];
} arex_submenu_edit_spec_t;

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
bool arex_submenu_direct_setting_from_selection(const char *current_title,
                                                uint8_t item_index,
                                                const char *item_text,
                                                arex_submenu_setting_confirm_t *out_setting);
bool arex_submenu_edit_spec_from_selection(const char *current_title,
                                           uint8_t item_index,
                                           const char *item_text,
                                           arex_submenu_edit_spec_t *out_spec);
void arex_submenu_apply_setting(arex_submenu_setting_kind_t kind, uint8_t arg, uint16_t value);
void arex_submenu_apply_edit_value(arex_submenu_setting_kind_t kind, uint8_t arg, float value);
uint8_t arex_submenu_safety_stop_depth_m(uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* AREX_SUBMENU_MODEL_H */
