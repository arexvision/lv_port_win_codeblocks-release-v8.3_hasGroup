#ifndef AREX_UI_STATE_H
#define AREX_UI_STATE_H

#include <stdint.h>
#include <stdbool.h>

/* =========================================
   UI State Machine — mirrors HTML STATE obj
   ========================================= */
typedef enum {
    UI_DASH         = 0,  /* scrolling dashboard cards */
    UI_INFO         = 1,  /* INFO menu list active */
    UI_SETUP        = 2,  /* SETUP menu list active */
    UI_EDIT_GAS     = 3,  /* gas cursor moving on 3F */
    UI_MODAL_GAS    = 4,  /* confirm-gas modal open */
    UI_MODAL_COMPASS= 5,  /* clear-compass-target modal */
    UI_SUB_MENU     = 6,  /* sub-menu layer visible */
    UI_MODAL_ACT    = 7,  /* generic action modal */
    UI_EDIT_VALUE   = 8,  /* inline value editor (e.g. MOD PO2) */
} arex_ui_state_t;

/* Sub-menu history entry */
typedef struct {
    char    title[32];
    uint8_t idx;
} arex_sub_history_t;

#define AREX_SUB_HISTORY_MAX 4

/* =========================================
   UI Context — everything the state machine
   needs to know to render and handle input
   ========================================= */
typedef struct {
    arex_ui_state_t  state;

    uint8_t  dash_card;         /* card_order[] 位置：0=INFO 1=COMPASS 2=DECO 3=GAS 4=PLAN 5=SETUP */

    /* Menu cursors */
    uint8_t  menu_info_idx;
    uint8_t  menu_setup_idx;
    uint8_t  sub_menu_idx;
    uint8_t  gas_cursor;

    /* Wall-charge: consecutive scroll presses at boundary */
    uint8_t  wall_charge;       /* 0~3; reach 3 → cross boundary */
    int8_t   wall_dir;          /* +1 bottom  -1 top */

    /* Sub-menu stack */
    arex_sub_history_t sub_history[AREX_SUB_HISTORY_MAX];
    uint8_t            sub_history_depth;

    /* Inline value edit context */
    struct {
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

#endif /* AREX_UI_STATE_H */
