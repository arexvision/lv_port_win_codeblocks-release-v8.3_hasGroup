/*
 * 文件: src/app_ui/ui/cards/card_blank.c
 * 作用: 该文件属于仪表卡片模块，负责某一类卡片页面的创建、布局、刷新或与页面注册表之间的装配。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时应重点检查布局尺寸、刷新频率与复用接口是否一致，避免局部样式或坐标调整影响同类页面的对齐和更新节奏。
 */

#include "../screen/screen.h"
#include "../core/data.h"
#include "../core/ui_engine.h"
#include "../core/ui_state.h"
#include "lvgl/lvgl.h"
#include "../fonts/fonts.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

/* =========================================================
 * card_blank.c — 空白卡片
 * 仅显示黑色背景，不渲染任何内容
 * ========================================================= */

void card_blank_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, BLACK, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
}

void card_blank_update(void)
{
    /* 空白卡片无需更新 */
}
