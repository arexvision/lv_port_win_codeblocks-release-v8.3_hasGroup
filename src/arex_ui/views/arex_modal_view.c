#include "arex_modal_view.h"

#include "arex_ui_engine.h"
#include "arex_ui_state.h"
#include "fonts/arex_fonts.h"

#include <stdio.h>

static lv_obj_t *s_modal = NULL;
static lv_obj_t *s_modal_box = NULL;

void arex_modal_view_reset(void)
{
    s_modal = NULL;
    s_modal_box = NULL;
}

void arex_modal_view_create(lv_obj_t *parent, uint16_t width, uint16_t height)
{
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
    (void)t;
    arex_screen_hide_modal();
    if (g_ui.state == UI_MODAL_ACT)
    {
        g_ui.state = (g_ui.sub_item_count > 0) ? UI_SUB_MENU : UI_DASH;
    }
    lv_timer_del(t);
}

static void modal_set_content(const char *title, const char *body, const char *hint)
{
    if (!s_modal_box)
    {
        return;
    }

    lv_obj_clean(s_modal_box);

    lv_obj_t *t = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(t, GREEN, 0);
    lv_obj_set_style_text_font(t, arex_get_font(FONT_ID_TITLE), 0);
    lv_label_set_text(t, title);
    lv_obj_set_pos(t, 0, 0);

    lv_obj_t *b = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(b, GREEN, 0);
    lv_obj_set_style_text_font(b, arex_get_font(FONT_ID_MEDIUM), 0);
    lv_label_set_text(b, body);
    lv_obj_set_pos(b, 0, 40);

    lv_obj_t *h = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(h, LIGHT, 0);
    lv_obj_set_style_text_font(h, arex_get_font(FONT_ID_SMALL), 0);
    lv_label_set_text(h, hint);
    lv_obj_set_pos(h, 0, 100);
}

void arex_screen_show_modal_act(const char *action_text)
{
    if (!s_modal)
    {
        return;
    }
    modal_set_content("ACTION", action_text ? action_text : "", "[ ESC TO BACK ]");
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
    g_ui.state = UI_MODAL_ACT;
    lv_timer_create(modal_act_timer_cb, 1000, NULL);
}

void arex_screen_show_modal_setup_confirm(const char *body)
{
    if (!s_modal)
    {
        return;
    }
    modal_set_content("CONFIRM SETTING", body ? body : "",
                      "[ ENTER CONFIRM ]  [ ESC CANCEL ]");
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void arex_screen_show_modal_gas(void)
{
    if (!s_modal)
    {
        return;
    }

    uint8_t gas_count = g_sensor_data.gas_slot_count;
    if (gas_count > GAS_COUNT)
    {
        gas_count = GAS_COUNT;
    }
    if (gas_count == 0U)
    {
        modal_set_content("CONFIRM GAS", "NO ACTIVE GAS", "[ ESC CANCEL ]");
        lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    uint8_t ci = (g_ui.gas_cursor < gas_count) ? g_ui.gas_cursor : 0U;
    char body[32];
    const char *gas_name = g_sensor_data.gas_slot_name[ci][0]
                           ? g_sensor_data.gas_slot_name[ci]
                           : GAS_NAMES[ci];
    float mod_m = g_sensor_data.gas_slot_mod_m[ci] > 0.0f
                  ? g_sensor_data.gas_slot_mod_m[ci]
                  : (float)GAS_MOD_M[ci];
    snprintf(body, sizeof(body), "%s\nMOD: %.0fm", gas_name, (double)mod_m);

    const char *hint = (g_sensor_data.depth > mod_m)
                       ? "[ FATAL: OVER MOD ]"
                       : "[ ENTER CONFIRM ]  [ ESC CANCEL ]";

    modal_set_content("CONFIRM GAS", body, hint);
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void arex_screen_show_modal_compass(void)
{
    if (!s_modal)
    {
        return;
    }
    modal_set_content("CLEAR TARGET?", "REMOVE HEADING MARKER?",
                      "[ ENTER CONFIRM ]  [ ESC CANCEL ]");
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void arex_screen_pulse_modal(void)
{
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

void arex_screen_hide_modal(void)
{
    if (!s_modal)
    {
        return;
    }
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}
