/*
 * 文件: src/app_ui/ui/views/submenu_types.h
 * 作用: 该文件属于视图表现模块，负责菜单、子菜单、模态框或特定视图状态的展示与交互联动。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef SUBMENU_TYPES_H
#define SUBMENU_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SUBMENU_INFO_COUNT   6
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
    SUBMENU_SETTING_OC_TECH_GAS,
    SUBMENU_SETTING_GAS_EDIT_PPO2,
    SUBMENU_SETTING_GAS_EDIT_ACTIVE,
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
    SUBMENU_SETTING_TEMP_UNIT,
    SUBMENU_SETTING_DATETIME_FIELD,
    SUBMENU_SETTING_DATETIME_ACTION,
    SUBMENU_SETTING_TIME_24H,
    SUBMENU_SETTING_DATE_FORMAT,
    SUBMENU_SETTING_LOG_RATE,
    SUBMENU_SETTING_BLUETOOTH,
    SUBMENU_SETTING_RESET_DEFAULTS,
    SUBMENU_SETTING_PLAN_DEPTH,
    SUBMENU_SETTING_PLAN_TIME,
    SUBMENU_SETTING_PLAN_RMV,
    SUBMENU_SETTING_SURFACE_CONFIRM,
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
