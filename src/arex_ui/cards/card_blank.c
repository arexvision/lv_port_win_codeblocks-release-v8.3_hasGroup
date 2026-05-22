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
