/*
 * 文件: src/app_ui/ui/views/modal_view.h
 * 作用: 该文件属于视图表现模块，负责菜单、子菜单、模态框或特定视图状态的展示与交互联动。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef MODAL_VIEW_H
#define MODAL_VIEW_H

#include "lvgl/lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void modal_view_reset(void);
void modal_view_create(lv_obj_t *parent, uint16_t width, uint16_t height);

void screen_show_modal_act(const char *action_text);
void screen_show_modal_setup_confirm(const char *body);
void screen_show_modal_gas(void);
void screen_show_modal_compass(void);
void screen_pulse_modal(void);
void screen_hide_modal(void);

#ifdef __cplusplus
}
#endif

#endif /* MODAL_VIEW_H */
