/*
 * 文件: src/app_ui/ui/comp/comp_update.h
 * 作用: 该文件属于公共组件模块，负责复用样式、通用控件、局部刷新逻辑或组件级显示封装。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时应重点检查布局尺寸、刷新频率与复用接口是否一致，避免局部样式或坐标调整影响同类页面的对齐和更新节奏。
 */

#ifndef COMP_UPDATE_H
#define COMP_UPDATE_H

#include "../core/ui_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 这一组接口负责把数据层数值同步进现有组件实例。 */
void comp_set_value(comp_id_t id, float value);
void comp_set_text(comp_id_t id, const char *text);
void comp_sync_data(comp_id_t w_id);

#ifdef __cplusplus
}
#endif

#endif /* COMP_UPDATE_H */
