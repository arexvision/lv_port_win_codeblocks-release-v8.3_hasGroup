/*
 * 文件: src/app_ui/ui/core/ui_engine.h
 * 作用: 该文件属于 UI 核心模块，负责状态机、数据桥接、事件分发、更新调度或 UI 运行时公共定义。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef UI_ENGINE_H
#define UI_ENGINE_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#include "ui_runtime.h"
#include "data.h"
#include "ui_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PC_SIMULATOR
#define SYSTEM_VERSION  "20260101.01"
#else
#include "../../config/app_version_auto.h"
#define SYSTEM_VERSION  APP_VERSION_SEMVER
#endif

extern uint8_t g_sys_page_order(uint8_t pos);
#define g_sys_card_order g_sys_page_order

#ifdef __cplusplus
}
#endif

#endif /* UI_ENGINE_H */
