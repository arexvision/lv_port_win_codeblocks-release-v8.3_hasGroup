#ifndef SUBMENU_TYPES_H
#define SUBMENU_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SUBMENU_INFO_COUNT   5
#define SUBMENU_SETUP_COUNT  6

typedef enum
{
    SUBMENU_SETTING_NONE = 0,
    SUBMENU_SETTING_SALINITY,
    SUBMENU_SETTING_SAFETY_STOP,
    SUBMENU_SETTING_LAST_DECO,
    SUBMENU_SETTING_ALTITUDE,
    SUBMENU_SETTING_DIVE_MODE,
    SUBMENU_SETTING_NITROX_O2,
    SUBMENU_SETTING_3GAS_O2,
    SUBMENU_SETTING_3GAS_COUNT,
    SUBMENU_SETTING_OC_TECH_GAS,
    SUBMENU_SETTING_OC_TECH_SAVE,
    SUBMENU_SETTING_AI_PAIR,
    SUBMENU_SETTING_AI_TANK_STATE,
    SUBMENU_SETTING_GTR_MODE,
    SUBMENU_SETTING_MOD_PPO2,
    SUBMENU_SETTING_DEPTH_ALARM,
    SUBMENU_SETTING_TIME_ALARM,
    SUBMENU_SETTING_NDL_ALARM,
    SUBMENU_SETTING_VIBRATION_TEST,
    SUBMENU_SETTING_UNITS,
    SUBMENU_SETTING_DATETIME_FIELD,
    SUBMENU_SETTING_DATETIME_ACTION,
    SUBMENU_SETTING_LOG_RATE,
    SUBMENU_SETTING_BLUETOOTH,
    SUBMENU_SETTING_RESET_DEFAULTS,
    SUBMENU_SETTING_PLAN_DEPTH,
    SUBMENU_SETTING_PLAN_TIME,
    SUBMENU_SETTING_PLAN_RMV,
} submenu_setting_kind_t;

typedef struct
{
    uint8_t value;
    const char *menu_label;
    const char *badge_label;
} setting_option_t;

typedef struct
{
    uint8_t value;
    const char *menu_label;
    const char *badge_label;
    uint8_t visible_opa;
} brightness_option_t;

typedef struct
{
    submenu_setting_kind_t kind;
    uint8_t arg;
    uint16_t value;
    char body[48];
} submenu_setting_confirm_t;

typedef struct
{
    submenu_setting_kind_t kind;
    uint8_t arg;
    float value;
    float min;
    float max;
    float step;
    uint8_t decimals;
    char label[20];
} submenu_edit_spec_t;

#ifdef __cplusplus
}
#endif

#endif /* SUBMENU_TYPES_H */
