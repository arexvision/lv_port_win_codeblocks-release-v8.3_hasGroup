#ifndef MENU_DEFS_H
#define MENU_DEFS_H

#include "../core/ui_engine.h"
#include "submenu_model.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MENU_MAX_ROWS 16U

extern const menu_item_cfg_t g_menu_info_card_items[SUBMENU_INFO_COUNT];
extern const menu_item_cfg_t g_menu_setup_card_items[SUBMENU_SETUP_COUNT];

typedef enum
{
    MENU_NONE = 0,
    MENU_INFO_LAST_DIVE,
    MENU_INFO_DIVE_PLAN,
    MENU_INFO_TISSUE_TOX,
    MENU_INFO_GAS_CALC,
    MENU_INFO_SENSOR_DEVICE,
    MENU_SETUP_GAS_SWITCH,
    MENU_SETUP_CONSERVATISM,
    MENU_SETUP_BRIGHTNESS,
    MENU_SETUP_COMPASS_CAL,
    MENU_SETUP_LIGHT_CONTROL,
    MENU_SETUP_SYSTEMS,
    MENU_MODE_SETUP,
    MENU_NITROX,
    MENU_THREE_GAS,
    MENU_OC_TECH,
    MENU_OC_TECH_EDIT,
    MENU_DIVE_SETUP,
    MENU_AI_SETUP,
    MENU_ALERTS_SETUP,
    MENU_DISPLAY,
    MENU_DATE_CLOCK,
    MENU_LIGHT_RED,
    MENU_LIGHT_GREEN,
    MENU_LIGHT_BLUE,
    MENU_LIGHT_WHITE,
} menu_id_t;

typedef enum
{
    MENU_ROW_NORMAL = 0,
    MENU_ROW_LIGHT_POWER,
    MENU_ROW_DIVE_PLAN,
    MENU_ROW_READONLY,
} menu_row_type_t;

typedef enum
{
    MENU_ITEM_NONE = 0,
    MENU_ITEM_BACK,
    MENU_ITEM_READONLY,

    MENU_ITEM_INFO_LAST_DIVE,
    MENU_ITEM_INFO_DIVE_PLAN,
    MENU_ITEM_INFO_TISSUE_TOX,
    MENU_ITEM_INFO_GAS_CALC,
    MENU_ITEM_INFO_SENSOR_DEVICE,

    MENU_ITEM_SETUP_GAS_SWITCH,
    MENU_ITEM_SETUP_CONSERVATISM,
    MENU_ITEM_SETUP_BRIGHTNESS,
    MENU_ITEM_SETUP_COMPASS_CAL,
    MENU_ITEM_SETUP_LIGHT_CONTROL,
    MENU_ITEM_SETUP_SYSTEMS,

    MENU_ITEM_GAS_SLOT_0,
    MENU_ITEM_GAS_SLOT_1,
    MENU_ITEM_GAS_SLOT_2,
    MENU_ITEM_GAS_SLOT_3,
    MENU_ITEM_GAS_SLOT_4,

    MENU_ITEM_CONSERVATISM_LOW,
    MENU_ITEM_CONSERVATISM_MED,
    MENU_ITEM_CONSERVATISM_HIGH,
    MENU_ITEM_CONSERVATISM_CUSTOM,

    MENU_ITEM_BRIGHTNESS_ECO,
    MENU_ITEM_BRIGHTNESS_MED,
    MENU_ITEM_BRIGHTNESS_HIGH,
    MENU_ITEM_BRIGHTNESS_MAX,
    MENU_ITEM_BRIGHTNESS_SUN,

    MENU_ITEM_COMPASS_CAL_START,
    MENU_ITEM_COMPASS_CAL_RESET,

    MENU_ITEM_LIGHT_POWER,
    MENU_ITEM_LIGHT_RED,
    MENU_ITEM_LIGHT_GREEN,
    MENU_ITEM_LIGHT_BLUE,
    MENU_ITEM_LIGHT_WHITE,
    MENU_ITEM_LIGHT_LEVEL_10,
    MENU_ITEM_LIGHT_LEVEL_30,
    MENU_ITEM_LIGHT_LEVEL_50,
    MENU_ITEM_LIGHT_LEVEL_70,
    MENU_ITEM_LIGHT_LEVEL_100,

    MENU_ITEM_SYSTEM_VERSION,
    MENU_ITEM_SYSTEM_MODE_SETUP,
    MENU_ITEM_SYSTEM_DIVE_SETUP,
    MENU_ITEM_SYSTEM_AI_SETUP,
    MENU_ITEM_SYSTEM_ALERTS_SETUP,
    MENU_ITEM_SYSTEM_DISPLAY,

    MENU_ITEM_MODE_AIR,
    MENU_ITEM_MODE_NITROX,
    MENU_ITEM_MODE_THREE_GAS,
    MENU_ITEM_MODE_OC_TECH,

    MENU_ITEM_NITROX_O2,
    MENU_ITEM_NITROX_CONFIRM,
    MENU_ITEM_THREE_GAS_O2_0,
    MENU_ITEM_THREE_GAS_O2_1,
    MENU_ITEM_THREE_GAS_O2_2,
    MENU_ITEM_THREE_GAS_COUNT,
    MENU_ITEM_THREE_GAS_CONFIRM,
    MENU_ITEM_OC_TECH_SLOT_0,
    MENU_ITEM_OC_TECH_SLOT_1,
    MENU_ITEM_OC_TECH_SLOT_2,
    MENU_ITEM_OC_TECH_SLOT_3,
    MENU_ITEM_OC_TECH_SLOT_4,
    MENU_ITEM_OC_TECH_CONFIRM,
    MENU_ITEM_OC_TECH_EDIT_O2,
    MENU_ITEM_OC_TECH_EDIT_HE,
    MENU_ITEM_OC_TECH_EDIT_SAVE,

    MENU_ITEM_DIVE_SALINITY,
    MENU_ITEM_DIVE_MOD_PPO2,
    MENU_ITEM_DIVE_SAFETY_STOP,
    MENU_ITEM_DIVE_LAST_DECO,
    MENU_ITEM_DIVE_ALTITUDE,

    MENU_ITEM_AI_TANK_0,
    MENU_ITEM_AI_TANK_1,
    MENU_ITEM_AI_GTR,

    MENU_ITEM_ALERT_DEPTH,
    MENU_ITEM_ALERT_TIME,
    MENU_ITEM_ALERT_NDL,

    MENU_ITEM_DISPLAY_UNITS,
    MENU_ITEM_DISPLAY_DATE_CLOCK,
    MENU_ITEM_DISPLAY_LOG_RATE,
    MENU_ITEM_DISPLAY_BLUETOOTH,
    MENU_ITEM_DISPLAY_RESET,
    MENU_ITEM_DATE_YEAR,
    MENU_ITEM_DATE_MONTH,
    MENU_ITEM_DATE_DAY,
    MENU_ITEM_DATE_HOUR,
    MENU_ITEM_DATE_MINUTE,

    MENU_ITEM_DIVE_PLAN_EXIT,
    MENU_ITEM_DIVE_PLAN_PRIMARY,
} menu_item_id_t;

typedef enum
{
    MENU_ACTION_NONE = 0,
    MENU_ACTION_BACK,
    MENU_ACTION_OPEN_CHILD,
    MENU_ACTION_REFRESH,
    MENU_ACTION_CLOSE,
    MENU_ACTION_CLOSE_PARENT_TOO,
    MENU_ACTION_RETURN_DASH,
    MENU_ACTION_SHOW_CONFIRM,
    MENU_ACTION_BEGIN_EDIT,
    MENU_ACTION_SHOW_GAS_MODAL,
    MENU_ACTION_SHOW_TEXT_MODAL,
} menu_action_type_t;

typedef struct
{
    menu_item_id_t id;
    menu_row_type_t type;
    const char *label;
    const char *badge;
} menu_row_t;

typedef struct
{
    menu_action_type_t type;
    menu_id_t child_menu;
    uint8_t keep_index;
    submenu_edit_spec_t edit_spec;
    const char *modal_text;
} menu_action_t;

const menu_item_cfg_t *menu_defs_info_card_items(uint8_t *out_count);
const menu_item_cfg_t *menu_defs_setup_card_items(uint8_t *out_count);
menu_id_t menu_defs_info_menu_for_index(uint8_t index);
menu_id_t menu_defs_setup_menu_for_index(uint8_t index);
const char *menu_defs_title(menu_id_t id);
menu_item_id_t menu_defs_back_item(void);
menu_id_t menu_defs_child_menu_for_item(menu_item_id_t id);
bool menu_defs_is_info_menu(menu_id_t id);
bool menu_defs_is_readonly_menu(menu_id_t id);
bool menu_defs_is_light_color_menu(menu_id_t id);
const char *menu_defs_light_color_name(menu_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* MENU_DEFS_H */
