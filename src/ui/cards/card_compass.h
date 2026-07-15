/*
 * 文件: src/app_ui/ui/cards/card_compass.h
 * 作用: 该文件属于仪表卡片模块，负责某一类卡片页面的创建、布局、刷新或与页面注册表之间的装配。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时应重点检查布局尺寸、刷新频率与复用接口是否一致，避免局部样式或坐标调整影响同类页面的对齐和更新节奏。
 */

#ifndef CARD_COMPASS_H
#define CARD_COMPASS_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl/lvgl.h"
#include "../core/vm/ui_vm_dashboard_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void card_compass_create(lv_obj_t *parent);
void card_compass_update(void);
void card_compass_refresh_heading_vm(const ui_vm_compass_t *vm, bool force_refresh);
void card_compass_refresh_heading(bool force_refresh);
uint16_t card_compass_display_heading_deg(void);
bool card_compass_display_heading_available(void);

#ifdef __cplusplus
}
#endif

#endif /* CARD_COMPASS_H */
