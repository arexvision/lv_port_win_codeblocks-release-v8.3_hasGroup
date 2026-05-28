/*
 * 文件: src/app_ui/ui/screen/screen_edit.h
 * 作用: 该文件属于屏幕或页面编排模块，负责整屏布局、分页切换、覆盖层、编辑态显示或页面注册管理。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef SCREEN_EDIT_H
#define SCREEN_EDIT_H

#include "screen_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

void screen_refresh_edit_value(void);
void screen_begin_edit_value(uint8_t item_idx, const submenu_edit_spec_t *spec);
void screen_commit_edit_value(void);
void screen_cancel_edit_value(void);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_EDIT_H */
