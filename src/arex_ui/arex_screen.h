#ifndef AREX_SCREEN_HDR
#define AREX_SCREEN_HDR

#include "lvgl/lvgl.h"
#include <stdint.h>
#include <stdbool.h>

/* =========================================
   Colours — AREX monochromatic green palette
   ========================================= */
#define AREX_GREEN   lv_color_make(0x00, 0xFF, 0x00)
#define AREX_LIGHT   lv_color_make(0x55, 0xFF, 0x55)
#define AREX_DARK    lv_color_make(0x00, 0x33, 0x00)
#define AREX_BLACK   lv_color_make(0x00, 0x00, 0x00)
#define AREX_BG      lv_color_make(0x05, 0x05, 0x05)

/* =========================================
   Layout constants
   ========================================= */
#define AREX_SCREEN_W   640
#define AREX_SCREEN_H   480
#define AREX_LEFT_W     180
#define AREX_RIGHT_W    (AREX_SCREEN_W - AREX_LEFT_W)  /* 460 */
#define AREX_CARD_H     AREX_SCREEN_H

/* =========================================
   Wall indicator side
   ========================================= */
typedef enum {
    WALL_TOP,
    WALL_BOTTOM,
} wall_side_t;

/* =========================================
   Screen lifecycle
   ========================================= */
void arex_screen_create(void);

/* =========================================
   Tileview / card navigation
   ========================================= */
void arex_screen_scroll_to_card(uint8_t idx);

/* =========================================
   Left panel refresh
   ========================================= */
void arex_screen_refresh_left_panel(void);

/* =========================================
   Wall charge indicators
   ========================================= */
void arex_screen_show_wall(wall_side_t side, uint8_t charge, const char *text);
void arex_screen_hide_walls(void);        /* smooth return to y=0 (wall-charge reversed) */
void arex_screen_hide_walls_snap(void);   /* instant y=0 (wall-charge threshold crossed) */

/* =========================================
   Menu list selection helpers
   ========================================= */
void    arex_screen_set_info_selection(uint8_t idx);
uint8_t arex_screen_info_item_count(void);

void    arex_screen_set_setup_selection(uint8_t idx);
uint8_t arex_screen_setup_item_count(void);

void arex_screen_set_submenu_selection(uint8_t idx);

/* =========================================
   Gas menu
   ========================================= */
void arex_screen_refresh_gas_menu(void);

/* =========================================
   Sub-menu layer (slides in from right)
   ========================================= */
void arex_screen_open_info_submenu(uint8_t item_idx);
void arex_screen_open_setup_submenu(uint8_t item_idx);
void arex_screen_handle_submenu_select(uint8_t item_idx);
void arex_screen_close_submenu(void);

/* Open a nested (3rd-level) sub-menu, pushing current onto history stack */
void arex_screen_open_nested_submenu(const char *title, const char **items, uint8_t count);

/* Update the value shown in a setup menu item's right-side badge */
void arex_screen_update_setup_badge(uint8_t item_idx, const char *value);

/* Show a generic action modal that auto-closes after 1 second */
void arex_screen_show_modal_act(const char *action_text);

/* Begin inline editing for a sub-menu item (MOD PO2 pattern) */
void arex_screen_begin_edit_value(uint8_t item_idx, float value,
                                   float min, float max, float step);

/* =========================================
   Modal dialogs
   ========================================= */
void arex_screen_show_modal_gas(void);
void arex_screen_show_modal_compass(void);
void arex_screen_pulse_modal(void);   /* shake on invalid confirm */
void arex_screen_hide_modal(void);

/* =========================================
   Inline value editor
   ========================================= */
void arex_screen_refresh_edit_value(void);
void arex_screen_commit_edit_value(void);
void arex_screen_cancel_edit_value(void);

/* =========================================
   Compass
   ========================================= */
void arex_screen_refresh_compass_target(void);

/* =========================================
   Scroll dots indicator
   ========================================= */
void arex_screen_update_scroll_dots(uint8_t active_idx, bool visible);

/* =========================================
   Card title helper
   Creates a LIGHT-colored title label (font TITLE) at pos (16,12)
   with a 2px DARK bottom border line underneath, matching .card-title in HTML.
   Returns the label object. The border line is placed at y=38.
   ========================================= */
lv_obj_t *arex_screen_make_card_title(lv_obj_t *parent, const char *text);

#endif /* AREX_SCREEN_HDR */
