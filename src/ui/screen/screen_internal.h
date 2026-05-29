/*
 * 文件: src/app_ui/ui/screen/screen_internal.h
 * 作用: 该文件属于屏幕或页面编排模块，负责整屏布局、分页切换、覆盖层、编辑态显示或页面注册管理。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef SCREEN_INTERNAL_H
#define SCREEN_INTERNAL_H

#include "screen.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_obj_t *s_scr;
/* 下列 extern 是 screen.c 内部维护的全局对象句柄，供同模块拆分文件共享。 */
extern lv_obj_t *s_safe_zone;
extern lv_obj_t *s_left_anchor;
extern lv_obj_t *s_right_cont;
extern lv_obj_t *s_tileview;
extern lv_obj_t *s_tile_objs[PAGE_COUNT];
extern lv_obj_t *s_wall_top;
extern lv_obj_t *s_wall_bottom;
extern lv_obj_t *s_wall_text_top;
extern lv_obj_t *s_wall_blocks_top;
extern lv_obj_t *s_wall_text_bottom;
extern lv_obj_t *s_wall_blocks_bottom;
extern lv_obj_t *s_info_list;
extern lv_obj_t *s_setup_list;
extern lv_obj_t *s_dot_cont;
extern lv_obj_t *s_scroll_dots[DASH_PAGE_COUNT];
extern lv_obj_t *s_brightness_overlay;
extern lv_timer_t *s_edit_flash_timer;
extern lv_obj_t *s_edit_flash_badge;
extern lv_obj_t *s_edit_flash_val_lbl;
extern bool s_edit_flash_on;
extern bool s_software_brightness_enabled;
extern uint16_t s_cached_right_w;
extern lv_style_t s_style_screen;
extern lv_style_t s_style_panel;
extern lv_style_t s_style_anchor_bg;
extern lv_style_t s_style_label_huge;
extern lv_style_t s_style_label_med;
extern lv_style_t s_style_label_small;
extern lv_style_t s_style_title_zone;
extern lv_style_t s_style_val_zone;
extern lv_style_t s_style_menu_item;
extern lv_style_t s_style_menu_item_active;
extern lv_style_t s_style_sep_line;

/* 这些内部函数只给 screen 子模块使用，不建议外部业务层直接调用。 */
void reset_transient_ui_refs(void);
void restore_brightness_overlay_state(void);
void edit_flash_stop(void);
void wall_create(void);
void render_left_anchor_grid(lv_obj_t *left_anchor);
void render_5f_custom_grid(lv_obj_t *card_custom,
                           lv_obj_t *left_anchor_obj,
                           uint8_t custom_card_idx);
void grid_5f_rebuild_all(void);
uint8_t page_visible_dash_count(void);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_INTERNAL_H */
