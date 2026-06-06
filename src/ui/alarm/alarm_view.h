/*
 * 文件: src/app_ui/ui/alarm/alarm_view.h
 * 作用: 该文件属于闹钟界面模块，负责闹钟数据、视图构建、交互刷新或与上层 UI 状态之间的衔接。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时建议先理解该文件在 app_ui 流程中的位置，再修改注释所描述的职责边界，避免把数据准备、状态切换和视图渲染职责混杂到一起。
 */

#ifndef ALARM_VIEW_H
#define ALARM_VIEW_H

#include "../core/ui_engine.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    lv_obj_t *safe_zone;
    lv_obj_t *left_anchor;
    lv_obj_t **custom_cards;
    uint8_t custom_card_count;
    uint8_t max_custom_cards;
    uint8_t layout_order;
    bool vertical_split;
    uint16_t safe_zone_w;
    uint16_t left_anchor_w;
    uint16_t panel_gap_px;
    uint16_t content_x;
    uint16_t content_y;
    uint16_t content_w;
    uint16_t content_h;
    bool alarm_pending_click;
} alarm_view_context_t;

void alarm_view_tick(const alarm_view_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* ALARM_VIEW_H */
