#ifndef AREX_SCREEN_HDR
#define AREX_SCREEN_HDR

#include "lvgl/lvgl.h"
#include "../core/arex_ui_engine.h"
#include "../views/arex_submenu_model.h"
#include "../fonts/arex_fonts.h"
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

/* 向前兼容宏 (已在 arex_fonts.h 中通过 AREX_FONT_HUGE 等定义) */

/* =========================================
   注意：上述宏已废弃！
   正确做法：使用 arex_font_id_t 枚举 + arex_get_font(id)
   示例：lv_obj_set_style_text_font(obj, arex_get_font(FONT_ID_HUGE), 0);
   保留这些宏仅为兼容旧代码，新代码禁止使用！
   ========================================= */

/* =========================================
   Wall indicator side
   ========================================= */
typedef enum
{
    WALL_TOP,
    WALL_BOTTOM,
} wall_side_t;

/* =========================================
   Safe Zone rebuild — called after config change
   ========================================= */
void arex_screen_rebuild_layout(void);
void arex_screen_rebuild_full(void);

/* 获取 Safe Zone 容器对象（供告警横幅使用） */
lv_obj_t *arex_get_safe_zone(void);

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

/* 统一全屏组件刷新接口：同时刷新左侧锚点和 5F 自定义网格
 * 内部调用 comp_sync_data() 路由分发器 */
void arex_screen_refresh_all_widgets(void);

/* 兼容旧接口：仅刷新左侧面板（保留以避免外部引用断裂） */
void arex_screen_refresh_left_panel(void);

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
void arex_screen_refresh_info_submenu_if_open(void);
void arex_screen_refresh_compass_cal_submenu_if_open(void);

void arex_screen_open_nested_submenu(const char *title, const char **items, uint8_t count);

void arex_screen_update_setup_badge(uint8_t item_idx, const char *value);

void arex_screen_show_modal_act(const char *action_text);
void arex_screen_show_modal_setup_confirm(const char *body);
void arex_screen_confirm_submenu_setting(void);
void arex_screen_cancel_submenu_setting(void);

void arex_screen_begin_edit_value(uint8_t item_idx, const arex_submenu_edit_spec_t *spec);

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
#ifdef __cplusplus
}
#endif

#endif /* AREX_SCREEN_HDR */
