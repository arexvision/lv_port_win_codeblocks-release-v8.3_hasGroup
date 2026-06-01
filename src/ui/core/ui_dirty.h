/*
 * 文件: src/app_ui/ui/core/ui_dirty.h
 * 作用: 该文件属于 UI 核心模块，负责状态机、数据桥接、事件分发、更新调度或 UI 运行时公共定义。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef UI_DIRTY_H
#define UI_DIRTY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t dirty_mask_t;

typedef enum
{
    DIRTY_NONE         = 0,
    DIRTY_DIVE_PROFILE = (1U << 0),
    DIRTY_DECO_STATUS  = (1U << 1),
    DIRTY_TISSUE_TOX   = (1U << 2),
    DIRTY_GAS_SUPPLY   = (1U << 3),
    DIRTY_SYSTEM       = (1U << 4),
    DIRTY_COMPASS      = (1U << 5),
    DIRTY_SENSOR       = (1U << 6),
    DIRTY_PLAN         = (1U << 7),
    DIRTY_DIVE_CONFIG  = (1U << 8),
    DIRTY_ALARM        = (1U << 9),
    DIRTY_UI_LAYOUT    = (1U << 10),
} dirty_bit_t;

#define DIRTY_DATA_ALL  (DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS | DIRTY_TISSUE_TOX | \
                         DIRTY_GAS_SUPPLY | DIRTY_SYSTEM | DIRTY_COMPASS | \
                         DIRTY_SENSOR | DIRTY_PLAN | DIRTY_DIVE_CONFIG | DIRTY_ALARM)

#define DIRTY_INFO_REFRESH_MASK  (DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS | DIRTY_TISSUE_TOX | \
                                  DIRTY_GAS_SUPPLY | DIRTY_SYSTEM | DIRTY_COMPASS | \
                                  DIRTY_SENSOR | DIRTY_PLAN | DIRTY_DIVE_CONFIG)

#define DIRTY_WIDGET_REFRESH_MASK  (DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS | \
                                    DIRTY_TISSUE_TOX | DIRTY_GAS_SUPPLY | \
                                    DIRTY_SYSTEM | DIRTY_DIVE_CONFIG)

#ifdef __cplusplus
}
#endif

#endif /* UI_DIRTY_H */
