/*
 * 文件: src/app_ui/ui/core/ui_runtime.h
 * 作用: 该文件属于 UI 核心模块，负责状态机、数据桥接、事件分发、更新调度或 UI 运行时公共定义。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef UI_RUNTIME_H
#define UI_RUNTIME_H

#include "ui_defs.h"
#include "ui_types.h"
#include "../comp/comp_style_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_init(void);
void ui_apply_config(void);
void ui_update_data(void);
void ui_update_flush_pending_once(void);
void sys_config_defaults(sys_config_t *cfg);
lv_text_align_t align_to_lv(uint8_t align);
lv_align_t align_to_lv_align(uint8_t align);
const lv_font_t *get_font(uint8_t font_id);
bool alarm_mark_clear_requested(void);
const char *comp_get_name(comp_id_t id);
void ui_update_task(lv_timer_t *timer);

extern lv_obj_t *g_left_anchor_obj;
extern lv_obj_t *g_card_custom_objs[MAX_CUSTOM_CARDS];
extern uint8_t   g_card_custom_obj_count;

#ifdef __cplusplus
}
#endif

#endif /* UI_RUNTIME_H */
