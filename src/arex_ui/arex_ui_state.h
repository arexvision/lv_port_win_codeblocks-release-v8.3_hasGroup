#ifndef AREX_UI_STATE_H
#define AREX_UI_STATE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef AREX_ENABLE_INFO_MENU
#define AREX_ENABLE_INFO_MENU 0
#endif

/* =========================================
   UI State Machine — mirrors HTML STATE obj
   ========================================= */
typedef enum
{
    UI_DASH         = 0,  /* scrolling dashboard cards */
    UI_INFO         = 1,  /* INFO menu list active */
    UI_SETUP        = 2,  /* SETUP menu list active */
    UI_EDIT_GAS     = 3,  /* gas cursor moving on 3F */
    UI_MODAL_GAS    = 4,  /* confirm-gas modal open */
    UI_MODAL_COMPASS= 5,  /* clear-compass-target modal */
    UI_SUB_MENU     = 6,  /* sub-menu layer visible */
    UI_MODAL_ACT    = 7,  /* generic action modal */
    UI_EDIT_VALUE   = 8,  /* inline value editor (e.g. MOD PO2) */
    UI_MODAL_SETUP_CONFIRM = 9,  /* confirm setup item from sub-menu */
} arex_ui_state_t;

/* Sub-menu history entry */
typedef struct
{
    char    title[32];
    uint8_t idx;
} arex_sub_history_t;

#define AREX_SUB_HISTORY_MAX 4

/* =========================================
   气体切换命令队列（单向数据流：UI → Algorithm）
   ========================================= */
typedef struct
{
    bool pending;       /* 是否有待处理的命令 */
    uint8_t gas_idx;    /* 目标气体索引 0-3 */
} gas_switch_cmd_t;

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
    AREX_COMPASS_CAL_IDLE = 0,
    AREX_COMPASS_CAL_RUNNING,
    AREX_COMPASS_CAL_READY,
} arex_compass_cal_ui_state_t;

/* =========================================
   UI Context — everything the state machine
   needs to know to render and handle input
   ========================================= */
typedef struct
{
    arex_ui_state_t  state;

    uint8_t  dash_card;         /* 当前 DASH 所在 tile 位置：1~(CARD_POS_SETUP-1) */

    /* Menu cursors */
    uint8_t  menu_info_idx;
    uint8_t  menu_setup_idx;
    uint8_t  sub_menu_idx;
    uint8_t  gas_cursor;
    bool     gas_modal_from_submenu;  // HOTFIX: Route GAS modal exit based on context.

    /* Wall-charge: consecutive scroll presses at boundary */
    uint8_t  wall_charge;       /* 0~3; reach 3 → cross boundary */
    int8_t   wall_dir;          /* +1 bottom  -1 top */

    /* Sub-menu stack */
    arex_sub_history_t sub_history[AREX_SUB_HISTORY_MAX];
    uint8_t            sub_history_depth;

    /* Inline value edit context */
    struct
    {
        float   value;
        float   min;
        float   max;
        float   step;
        float   original;
        uint8_t item_index;     /* which sub-menu item is being edited */
        bool    active;
    } edit_ctx;

    /* Sub-menu content (current page) */
    const char *sub_title;
    const char *sub_items[8];
    uint8_t     sub_item_count;

    /* Parent state when sub-menu was opened */
    arex_ui_state_t sub_parent;

    /* 告警清除标志：触发后必须先 click/rotate 一次才可清除 */
    bool alarm_pending_click;

} arex_ui_ctx_t;

extern arex_ui_ctx_t g_ui;

/* =========================================
   Public API — called from arex_input.c
   ========================================= */
void arex_ui_state_init(void);

/* dir: +1 = scroll down/right,  -1 = scroll up/left */
void ui_handle_rotate(int8_t dir);

void ui_handle_click(void);
void ui_handle_back(void);

/* =========================================
   Internal helpers (used by cards too)
   ========================================= */
/* Notify all registered cards to refresh their widgets */
void arex_ui_refresh_all(void);

/* Scroll the tileview to the given card index (0-based, follows card_order) */
void arex_ui_go_to_card(uint8_t idx);

/* =========================================
   气体切换命令队列接口（UI 层调用）
   ========================================= */
/* 请求气体切换（不直接修改数据源，发送命令到队列） */
void arex_request_gas_switch(uint8_t gas_idx);

/* 检查是否有待处理的气体切换命令（buhlmann_task 调用） */
bool arex_has_pending_gas_switch(uint8_t *out_gas_idx);

/* 清除气体切换命令（buhlmann_task 处理后调用） */
void arex_clear_gas_switch_cmd(void);

/* 罗盘校准命令（UI -> 传感器任务） */
void arex_request_compass_calibration_start(void);
void arex_request_compass_calibration_reset(void);
bool arex_has_pending_compass_calibration(compass_cal_cmd_action_t *out_action);
void arex_clear_compass_calibration_cmd(void);
void arex_set_compass_calibration_ui_state(arex_compass_cal_ui_state_t state);
arex_compass_cal_ui_state_t arex_get_compass_calibration_ui_state(void);

#ifdef __cplusplus
}
#endif

#endif /* AREX_UI_STATE_H */
