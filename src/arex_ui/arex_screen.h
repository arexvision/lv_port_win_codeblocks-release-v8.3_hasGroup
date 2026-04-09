#ifndef AREX_SCREEN_H
#define AREX_SCREEN_H

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
void arex_screen_hide_walls(void);

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

#endif /* AREX_SCREEN_H */
