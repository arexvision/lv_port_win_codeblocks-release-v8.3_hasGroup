/*
 * 文件: src/app_ui/ui/core/ui_state.h
 * 作用: 该文件属于 UI 核心模块，负责状态机、数据桥接、事件分发、更新调度或 UI 运行时公共定义。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef UI_STATE_H
#define UI_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "../views/submenu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ENABLE_INFO_MENU
#define ENABLE_INFO_MENU 1
#endif

/* =========================================
   UI State Machine — mirrors HTML STATE obj
   ========================================= */
/* UI 全局状态机入口，旋钮/点击/返回键都会先转成这里的状态流转。 */
typedef enum
{
    UI_DASH         = 0,  /* scrolling dashboard pages */
    UI_INFO         = 1,  /* INFO menu list active */
    UI_SETUP        = 2,  /* SETUP menu list active */
    UI_EDIT_GAS     = 3,  /* gas cursor moving on 3F */
    UI_MODAL_GAS    = 4,  /* confirm-gas modal open */
    UI_MODAL_COMPASS= 5,  /* clear-compass-target modal */
    UI_SUB_MENU     = 6,  /* sub-menu layer visible */
    UI_MODAL_ACT    = 7,  /* generic action modal */
    UI_EDIT_VALUE   = 8,  /* inline value editor (e.g. MOD PO2) */
    UI_MODAL_SETUP_CONFIRM = 9,  /* confirm setup item from sub-menu */
    UI_MENU_ENTRY   = 10, /* MENU floor internal option focus */
    UI_MODAL_END_DIVE = 11, /* confirm manual dive end from MENU HUB */
    UI_MODAL_TURN_OFF = 12, /* confirm device sleep/power off */
    UI_EDIT_LIGHT_COLOR = 13, /* live light color preview before confirm */
} ui_state_t;

/* Sub-menu history entry */
typedef struct
{
    /* 用于返回上一级子菜单时恢复标题和选中行。 */
    char    title[32];
    uint8_t idx;
} sub_history_t;

#define SUB_HISTORY_MAX 4

/* =========================================
   气体切换命令队列（单向数据流：UI → Algorithm）
   ========================================= */
typedef struct
{
    bool pending;       /* 是否有待处理的命令 */
    uint8_t gas_idx;    /* 目标气体索引 0-3 */
} gas_switch_cmd_t;

typedef struct
{
    bool pending;       /* 是否有待处理的忽略命令 */
    uint8_t gas_idx;    /* 需要从本次 runtime 计划中排除的气体索引 */
} gas_ignore_cmd_t;

typedef enum
{
    COMPASS_CAL_CMD_NONE = 0,
    COMPASS_CAL_CMD_START,
    COMPASS_CAL_CMD_RESET,
} compass_cal_cmd_action_t;

typedef struct
{
    bool pending;       /* 是否有待处理的罗盘校准命令 */
    compass_cal_cmd_action_t action;
} compass_cal_cmd_t;

typedef enum
{
    COMPASS_CAL_IDLE = 0,
    COMPASS_CAL_RUNNING,
    COMPASS_CAL_READY,
} compass_cal_ui_state_t;

/* =========================================
   UI Context — state store (private to ui_state.c)
   外部模块必须通过 getter/setter 访问，禁止直接读写
   ========================================= */
/* 这个上下文是 UI 层唯一的状态仓库，禁止外部直接修改字段。 */
typedef struct
{
    ui_state_t  state;

    uint8_t  dash_page;         /* 当前 DASH 所在 tile 位置：1~(PAGE_POS_SETUP-1) */

    /* Menu cursors */
    uint8_t  menu_info_idx;
    uint8_t  menu_setup_idx;
    uint8_t  menu_entry_idx;
    uint8_t  sub_menu_idx;
    uint8_t  gas_cursor;
    bool     gas_modal_from_submenu;  // HOTFIX: Route GAS modal exit based on context.

    /* Wall-charge: consecutive scroll presses at boundary */
    uint8_t  wall_charge;       /* 0~3; reach 3 → cross boundary */
    int8_t   wall_dir;          /* +1 bottom  -1 top */

    /* Sub-menu stack */
    sub_history_t sub_history[SUB_HISTORY_MAX];
    uint8_t            sub_history_depth;

    /* Inline value edit context */
    struct
    {
        float   value;
        float   min;
        float   max;
        float   step;
        float   original;
        submenu_setting_kind_t setting_kind;
        uint8_t setting_arg;
        uint8_t decimals;
        uint8_t item_index;     /* which sub-menu item is being edited */
        char    label[20];
        bool    active;
    } edit_ctx;

    /* Sub-menu content (current page) */
    const char *sub_title;
    const char *sub_items[8];
    uint8_t     sub_item_count;

    /* Parent state when sub-menu was opened */
    ui_state_t sub_parent;

    /* 告警清除标志：触发后必须先 click/rotate 一次才可清除 */
    bool alarm_pending_click;

} ui_ctx_t;

/* =========================================
   Public API — called from input.c
   ========================================= */
/* 这些接口是输入层和 UI 状态机之间的唯一公共入口。 */
void ui_state_init(void);

/* dir: +1 = scroll down/right,  -1 = scroll up/left */
void ui_handle_rotate(int8_t dir);
bool ui_handle_rotate_steps_ex(int8_t steps);
void ui_handle_rotate_steps(int8_t steps);
bool ui_rotate_steps_can_coalesce(void);

void ui_handle_click(void);
void ui_handle_back(void);
void ui_state_poll_deferred_navigation(void);
bool ui_state_dash_navigation_pending(void);

ui_state_t ui_state_get_state(void);
void ui_state_set_state(ui_state_t state);
uint8_t ui_state_get_dash_page(void);
void ui_state_set_dash_page(uint8_t page);
uint8_t ui_state_get_menu_info_idx(void);
void ui_state_set_menu_info_idx(uint8_t idx);
uint8_t ui_state_get_menu_setup_idx(void);
void ui_state_set_menu_setup_idx(uint8_t idx);
uint8_t ui_state_get_gas_cursor(void);
void ui_state_set_gas_cursor(uint8_t cursor);
uint8_t ui_state_get_sub_menu_idx(void);
void ui_state_set_sub_menu_idx(uint8_t idx);
uint8_t ui_state_get_sub_item_count(void);
void ui_state_set_sub_item_count(uint8_t count);
ui_state_t ui_state_get_sub_parent(void);
void ui_state_set_sub_parent(ui_state_t state);
uint8_t ui_state_get_sub_history_depth(void);
void ui_state_set_sub_history_depth(uint8_t depth);
bool ui_state_get_gas_modal_from_submenu(void);
void ui_state_set_gas_modal_from_submenu(bool enabled);
bool ui_state_get_alarm_pending_click(void);
void ui_state_set_alarm_pending_click(bool pending);
bool ui_state_get_edit_active(void);
void ui_state_set_edit_active(bool active);
uint8_t ui_state_get_edit_item_index(void);
void ui_state_set_edit_item_index(uint8_t index);
bool ui_state_get_edit_value_active(void);
float ui_state_get_edit_value(void);
void ui_state_set_edit_value(float value);
float ui_state_get_edit_original(void);
void ui_state_set_edit_original(float value);
float ui_state_get_edit_min(void);
void ui_state_set_edit_min(float value);
float ui_state_get_edit_max(void);
void ui_state_set_edit_max(float value);
float ui_state_get_edit_step(void);
void ui_state_set_edit_step(float value);
submenu_setting_kind_t ui_state_get_edit_setting_kind(void);
void ui_state_set_edit_setting_kind(submenu_setting_kind_t kind);
uint8_t ui_state_get_edit_setting_arg(void);
void ui_state_set_edit_setting_arg(uint8_t arg);
uint8_t ui_state_get_edit_decimals(void);
void ui_state_set_edit_decimals(uint8_t decimals);
const char *ui_state_get_edit_label(void);
void ui_state_set_edit_label(const char *label);
bool ui_state_get_heading_lock_pending(void);
void ui_state_set_heading_lock_pending(bool pending);
bool ui_state_get_heading_lock_active(void);
void ui_state_set_heading_lock_active(bool active);

/* =========================================
   Internal helpers (used by cards too)
   ========================================= */
/* Notify all registered right-side pages to refresh their widgets */
void ui_refresh_all(void);

/* Scroll the tileview to the given page index (0-based, follows page registry order) */
void ui_go_to_page(uint8_t idx);

/* =========================================
   气体切换命令队列接口（UI 层调用）
   ========================================= */
/* 这一组接口只负责投递/消费命令，不直接改算法数据。 */
/* 请求气体切换（不直接修改数据源，发送命令到队列） */
void request_gas_switch(uint8_t gas_idx);

/* 检查是否有待处理的气体切换命令（算法适配层调用） */
bool has_pending_gas_switch(uint8_t *out_gas_idx);

/* 清除气体切换命令（算法适配层处理后调用） */
void clear_gas_switch_cmd(void);

/* 请求本次 runtime 忽略某路推荐气体（错过推荐深度后由告警层投递）。 */
void request_gas_ignore(uint8_t gas_idx);
bool has_pending_gas_ignore(uint8_t *out_gas_idx);
void clear_gas_ignore_cmd(void);

/* 罗盘校准命令（UI -> 传感器任务） */
void request_compass_calibration_start(void);
void request_compass_calibration_reset(void);
bool has_pending_compass_calibration(compass_cal_cmd_action_t *out_action);
void clear_compass_calibration_cmd(void);
void set_compass_calibration_ui_state(compass_cal_ui_state_t state);
compass_cal_ui_state_t get_compass_calibration_ui_state(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_STATE_H */
