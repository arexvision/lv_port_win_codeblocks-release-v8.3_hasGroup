/*
 * 文件: src/app_ui/ui/screen/screen.h
 * 作用: 该文件属于屏幕或页面编排模块，负责整屏布局、分页切换、覆盖层、编辑态显示或页面注册管理。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef SCREEN_HDR
#define SCREEN_HDR

#include "lvgl/lvgl.h"
#include "../core/ui_defs.h"
#include "../core/ui_types.h"
#include "../views/submenu_types.h"
#include "../fonts/fonts.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/* =========================================
   Layout constants (legacy, kept for compatibility)
   ========================================= */
#define SCREEN_W   640
#define SCREEN_H   480
#define LEFT_W     160
#define RIGHT_W    (SCREEN_W - LEFT_W)
#define CARD_H     SCREEN_H

/* 页面切换动画开关：1=开动画，0=关动画 */
#ifndef TILE_ANIM_ENABLED
#define TILE_ANIM_ENABLED  0
#endif

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
/* 布局参数变更后，screen 层通过这组接口完成对象重排或整屏重建。 */
void screen_rebuild_layout(void);
void screen_rebuild_full(void);

/* 获取 Safe Zone 容器对象（供告警横幅使用） */
lv_obj_t *get_safe_zone(void);

/* =========================================
   Screen lifecycle
   ========================================= */
/* 创建根屏、左右面板、tileview 和附属视图对象。 */
void screen_create(void);

/* =========================================
   Tileview / card navigation
   ========================================= */
/* 这些接口负责右侧页面切换和 tileview 级别的重建。 */
void screen_scroll_to_page(uint8_t idx);

void screen_rebuild_tileview(void);
void screen_request_enter_card_home_after_layout_rebuild(void);

/* =========================================
   Left panel refresh (仅更新文字)
   ========================================= */

/* 统一全屏组件刷新接口：同时刷新左侧锚点和 5F 自定义网格
 * 内部调用 comp_sync_data() 路由分发器 */
void screen_refresh_all_widgets(void);

/* 兼容旧接口：仅刷新左侧面板（保留以避免外部引用断裂） */
void screen_refresh_left_panel(void);

/* =========================================
   Wall charge indicators
   ========================================= */
/* “墙提示”用于在边界连续旋钮时给出进入/退出菜单的蓄力反馈。 */
void screen_show_wall(wall_side_t side, uint8_t charge, const char *text);
void screen_hide_walls(void);
void screen_hide_walls_snap(void);

/* =========================================
   Menu list selection helpers
   ========================================= */
/* 统一控制 INFO/SETUP/子菜单列表的高亮项。 */
void    screen_set_info_selection(uint8_t idx);
uint8_t screen_info_item_count(void);

void    screen_set_setup_selection(uint8_t idx);
uint8_t screen_setup_item_count(void);

void screen_set_submenu_selection(uint8_t idx);

/* =========================================
   Gas menu
   ========================================= */
void screen_refresh_gas_menu(void);
void screen_refresh_setup_menu(void);

/* =========================================
   Sub-menu layer
   ========================================= */
/* 子菜单层负责二级菜单、嵌套菜单和潜水计划类特殊视图。 */
void screen_open_info_submenu(uint8_t item_idx);
void screen_open_setup_submenu(uint8_t item_idx);
void screen_handle_submenu_select(uint8_t item_idx);
void screen_close_submenu(void);
void screen_refresh_info_submenu_if_open(void);
bool screen_handle_dive_plan_rotate(int8_t dir);
bool screen_handle_logbook_rotate(int8_t dir);
bool screen_handle_logbook_back(void);
void screen_refresh_compass_cal_submenu_if_open(void);

void screen_open_nested_submenu(const char *title, const char **items, uint8_t count);

void screen_update_setup_badge(uint8_t item_idx, const char *value);

void screen_show_modal_act(const char *action_text);
void screen_show_modal_setup_confirm(const char *body);
void screen_confirm_submenu_setting(void);
void screen_cancel_submenu_setting(void);

void screen_begin_edit_value(uint8_t item_idx, const submenu_edit_spec_t *spec);

/* =========================================
   Modal dialogs
   ========================================= */
/* 模态框用于确认、动作提示和特定场景的弹窗交互。 */
void screen_show_modal_gas(void);
void screen_show_modal_compass(void);
void screen_pulse_modal(void);
void screen_hide_modal(void);

/* =========================================
   Inline value editor
   ========================================= */
/* 行内编辑器用于修改亮度、阈值、日期时间等数值型设置。 */
void screen_refresh_edit_value(void);
void screen_commit_edit_value(void);
void screen_cancel_edit_value(void);

/* =========================================
   Compass
   ========================================= */
/* 罗盘相关刷新目前主要集中在目标指示和航向显示同步。 */
void screen_refresh_compass_target(void);

/* =========================================
   Scroll dots indicator
   ========================================= */
void screen_update_scroll_dots(uint8_t active_idx, bool visible);

/* =========================================
   Card title helper
   ========================================= */
lv_obj_t *screen_make_card_title(lv_obj_t *parent, const char *text);

/* =========================================
   List registration (called by card_*.c)
   ========================================= */
void screen_register_info_list(lv_obj_t *list);
void screen_register_setup_list(lv_obj_t *list);
#ifdef __cplusplus
}
#endif

#endif /* SCREEN_HDR */
