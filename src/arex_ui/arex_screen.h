#ifndef AREX_SCREEN_HDR
#define AREX_SCREEN_HDR

#include "lvgl/lvgl.h"
#include "arex_ui_engine.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/* =========================================
   Layout constants (legacy, kept for compatibility)
   ========================================= */
#define AREX_SCREEN_W   640
#define AREX_SCREEN_H   480
#define AREX_LEFT_W     160
#define AREX_RIGHT_W    (AREX_SCREEN_W - AREX_LEFT_W)
#define AREX_CARD_H     AREX_SCREEN_H

/* 页面切换动画开关：1=开动画，0=关动画 */
#ifndef AREX_TILE_ANIM_ENABLED
#define AREX_TILE_ANIM_ENABLED  0
#endif

/* 向前兼容宏 */
#define AREX_FONT_HUGE   (&lv_font_courier_58)
#define AREX_FONT_MEDIUM  (&lv_font_courier_28)
#define AREX_FONT_SMALL   (&lv_font_courier_14)
#define AREX_FONT_TITLE   (&lv_font_courier_20)

/* =========================================
   注意：上述宏已废弃！
   正确做法：使用 arex_font_id_t 枚举 + arex_get_font(id)
   示例：lv_obj_set_style_text_font(obj, arex_get_font(AREX_FONT_ID_HUGE), 0);
   保留这些宏仅为兼容旧代码，新代码禁止使用！
   ========================================= */

/* =========================================
   Wall indicator side
   ========================================= */
typedef enum {
    WALL_TOP,
    WALL_BOTTOM,
} wall_side_t;

/* =========================================
   Safe Zone rebuild — called after config change
   ========================================= */
void arex_screen_rebuild_layout(void);

/* =========================================
   Screen lifecycle
   ========================================= */
void arex_screen_create(void);

/* =========================================
   Tileview / card navigation
   ========================================= */
void arex_screen_scroll_to_card(uint8_t idx);

void arex_screen_rebuild_tileview(void);

/* =========================================
   Left panel refresh (仅更新文字)
   ========================================= */
void arex_screen_refresh_left_panel(void);
void arex_screen_refresh_system_data(void);

/* =========================================
   Wall charge indicators
   ========================================= */
void arex_screen_show_wall(wall_side_t side, uint8_t charge, const char *text);
void arex_screen_hide_walls(void);
void arex_screen_hide_walls_snap(void);

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
void arex_screen_refresh_setup_menu(void);

/* =========================================
   Sub-menu layer
   ========================================= */
void arex_screen_open_info_submenu(uint8_t item_idx);
void arex_screen_open_setup_submenu(uint8_t item_idx);
void arex_screen_handle_submenu_select(uint8_t item_idx);
void arex_screen_close_submenu(void);

void arex_screen_open_nested_submenu(const char *title, const char **items, uint8_t count);

void arex_screen_update_setup_badge(uint8_t item_idx, const char *value);

void arex_screen_show_modal_act(const char *action_text);

void arex_screen_begin_edit_value(uint8_t item_idx, float value,
                                   float min, float max, float step);

/* =========================================
   Modal dialogs
   ========================================= */
void arex_screen_show_modal_gas(void);
void arex_screen_show_modal_compass(void);
void arex_screen_pulse_modal(void);
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
   ========================================= */
lv_obj_t *arex_screen_make_card_title(lv_obj_t *parent, const char *text);

/* =========================================
   List registration (called by card_*.c)
   ========================================= */
void arex_screen_register_info_list(lv_obj_t *list);
void arex_screen_register_setup_list(lv_obj_t *list);

/* =========================================
   Light control callbacks (供业务层对接)
   ========================================= */
extern bool g_light_power_state;        /* 灯光开关状态（共享） */
void arex_bus_set_light_power(bool on);           /* 开关灯光 */
void arex_ui_on_light_color_set(const char *color, const char *level);  /* 颜色亮度设置 */
void arex_set_brightness(uint8_t level);           /* 设置屏幕亮度 (0-3) */
void arex_ui_on_conservatism_set(uint8_t level);   /* 设置算法保守度 */
#ifdef __cplusplus
}
#endif

#endif /* AREX_SCREEN_HDR */
