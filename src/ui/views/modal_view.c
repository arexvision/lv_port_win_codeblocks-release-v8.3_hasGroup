/*
 * 文件: src/app_ui/ui/views/modal_view.c
 * 作用: 该文件属于视图表现模块，负责菜单、子菜单、模态框或特定视图状态的展示与交互联动。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "modal_view.h"

#include "../core/ui_state.h"
#include "../core/ui_runtime.h"
#include "../core/vm/ui_vm_menu.h"
#include "../core/vm/ui_vm_menu_types.h"
#include "../fonts/fonts.h"

#include <stdio.h>

static lv_obj_t *s_modal = NULL;
static lv_obj_t *s_modal_box = NULL;

static uint8_t modal_count_body_lines(const char *body)
{
    uint8_t lines = 1U;

    if (!body || body[0] == '\0')
    {
        return 0U;
    }

    while (*body)
    {
        if (*body == '\n')
        {
            lines++;
        }
        body++;
    }

    return lines;
}

void modal_view_reset(void)
{
    /* 布局重建后旧模态框对象会失效，因此只保留空引用。 */
    s_modal = NULL;
    s_modal_box = NULL;
}

void modal_view_create(lv_obj_t *parent, uint16_t width, uint16_t height)
{
    /* 模态层由全屏遮罩和中央弹框两部分组成。 */
    s_modal = lv_obj_create(parent);
    lv_obj_set_size(s_modal, width, height);
    lv_obj_set_pos(s_modal, 0, 0);
    lv_obj_set_style_bg_color(s_modal, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(s_modal, 242, 0);
    lv_obj_set_style_border_width(s_modal, 0, 0);
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);

    s_modal_box = lv_obj_create(s_modal);
    lv_obj_set_size(s_modal_box, 400, 260);
    lv_obj_center(s_modal_box);
    lv_obj_set_style_bg_color(s_modal_box, BLACK, 0);
    lv_obj_set_style_bg_opa(s_modal_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_modal_box, GREEN, 0);
    lv_obj_set_style_border_width(s_modal_box, 4, 0);
    lv_obj_set_style_radius(s_modal_box, 0, 0);
    lv_obj_set_style_pad_all(s_modal_box, 30, 0);
}

static void modal_act_timer_cb(lv_timer_t *t)
{
    /* 动作提示类弹窗到时后自动关闭，并把状态机恢复到上一级界面。 */
    (void)t;
    screen_hide_modal();
    if (ui_state_get_state() == UI_MODAL_ACT)
    {
        ui_state_set_state((ui_state_get_sub_item_count() > 0U) ? UI_SUB_MENU : UI_DASH);
    }
    lv_timer_del(t);
}

static void modal_set_content(const char *title, const char *body, const char *hint)
{
    /* 每次展示弹窗前都重新构建内容，避免残留旧文本。 */
    if (!s_modal_box)
    {
        return;
    }

    lv_obj_clean(s_modal_box);

    lv_obj_t *t = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(t, GREEN, 0);
    lv_obj_set_style_text_font(t, get_font(FONT_ID_TITLE), 0);
    lv_label_set_text(t, title);
    lv_obj_set_pos(t, 0, 0);

    lv_obj_t *b = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(b, GREEN, 0);
    lv_obj_set_style_text_font(b, get_font(FONT_ID_MEDIUM), 0);
    lv_obj_set_style_text_line_space(b, 6, 0);
    lv_label_set_text(b, body);
    lv_obj_set_pos(b, 0, 40);

    uint8_t body_lines = modal_count_body_lines(body);
    int16_t hint_y = (body_lines > 2U) ? (int16_t)(52 + body_lines * 38) : 100;

    lv_obj_t *h = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(h, LIGHT, 0);
    lv_obj_set_style_text_font(h, get_font(FONT_ID_SMALL), 0);
    lv_label_set_text(h, hint);
    lv_obj_set_pos(h, 0, hint_y);
}

void screen_show_modal_act(const char *action_text)
{
    /* ACTION 弹窗用于展示短时操作反馈，会自动消失。 */
    if (!s_modal)
    {
        return;
    }
    modal_set_content("ACTION", action_text ? action_text : "", "[ ESC TO BACK ]");
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
    ui_state_set_state(UI_MODAL_ACT);
    lv_timer_create(modal_act_timer_cb, 1000, NULL);
}

void screen_show_modal_setup_confirm(const char *body)
{
    /* 设置确认弹窗需要用户明确确认或取消。 */
    if (!s_modal)
    {
        return;
    }
    modal_set_content("CONFIRM SETTING", body ? body : "",
                      "[ ENTER CONFIRM ]  [ ESC CANCEL ]");
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void screen_show_modal_gas(void)
{
    /* 气体弹窗内容完全由 VM 提供，view 层只负责显示。 */
    ui_vm_modal_gas_t vm;

    if (!s_modal)
    {
        return;
    }

    ui_vm_modal_gas_update(&vm, ui_state_get_gas_cursor());

    if (vm.valid == 0U)
    {
        modal_set_content(vm.title, vm.body, vm.hint);
        lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    modal_set_content(vm.title, vm.body, vm.hint);
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void screen_show_modal_compass(void)
{
    /* 罗盘目标清除弹窗是一个固定文案确认框。 */
    if (!s_modal)
    {
        return;
    }
    modal_set_content("CLEAR TARGET?", "REMOVE HEADING MARKER?",
                      "[ ENTER CONFIRM ]  [ ESC CANCEL ]");
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void screen_pulse_modal(void)
{
    /* 轻微左右抖动用于提示当前弹窗需要用户注意。 */
    if (!s_modal_box)
    {
        return;
    }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_modal_box);
    lv_anim_set_values(&a, 0, 6);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 80);
    lv_anim_set_playback_time(&a, 80);
    lv_anim_set_repeat_count(&a, 2);
    lv_anim_start(&a);
}

void screen_hide_modal(void)
{
    /* 隐藏时只加 hidden 标志，保留对象以便下次复用。 */
    if (!s_modal)
    {
        return;
    }
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}
