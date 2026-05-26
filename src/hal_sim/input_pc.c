#include "../ui/screen/screen.h"
#include "../ui/core/data.h"
#include "../ui/core/ui_state.h"
#include "lvgl/lvgl.h"
#include "lv_drivers/win32drv/win32drv.h"

static void key_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    switch (key) {
        case LV_KEY_UP:
        case LV_KEY_LEFT:      ui_handle_rotate(-1); break;
        case LV_KEY_DOWN:
        case LV_KEY_RIGHT:     ui_handle_rotate(+1); break;
        case LV_KEY_ENTER:     ui_handle_click();    break;
        case LV_KEY_ESC:
        case LV_KEY_BACKSPACE: ui_handle_back();     break;
        default: break;
    }
}

static void enc_click_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) ui_handle_click();
}

void input_init(lv_obj_t *scr)
{
    /* 透明 btn 接收所有输入事件 */
    lv_obj_t *catcher = lv_btn_create(scr);
    lv_obj_set_size(catcher, 1, 1);
    lv_obj_set_pos(catcher, 0, 0);
    lv_obj_set_style_opa(catcher, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(catcher, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(catcher, 0, 0);
    lv_obj_set_style_shadow_width(catcher, 0, 0);
    lv_obj_set_style_outline_width(catcher, 0, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(catcher, key_event_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(catcher, enc_click_cb, LV_EVENT_CLICKED, NULL);

    /* group_kbd: keypad → KEY 事件 */
    lv_group_t *g_kbd = lv_group_create();
    lv_group_add_obj(g_kbd, catcher);
    lv_group_focus_obj(catcher);
    lv_group_set_default(g_kbd);
    lv_indev_set_group(lv_win32_keypad_device_object, g_kbd);

    /* group_enc: encoder，固定 editing=true → enc_diff → LEFT/RIGHT → key_event_cb */
    lv_group_t *g_enc = lv_group_create();
    lv_group_add_obj(g_enc, catcher);
    lv_group_set_editing(g_enc, true);
    lv_indev_set_group(lv_win32_encoder_device_object, g_enc);
}
