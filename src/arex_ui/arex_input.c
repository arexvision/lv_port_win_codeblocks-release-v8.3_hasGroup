#include "arex_screen.h"
#include "arex_data.h"
#include "arex_ui_state.h"
#include "lvgl/lvgl.h"
#include "lv_drivers/win32drv/win32drv.h"

/*
 * 键盘:   UP/DOWN        → rotate(-1/+1)
 *         ENTER          → click
 *         ESC/BACKSPACE  → back
 * 滚轮(encoder editing模式): LEFT → rotate(-1), RIGHT → rotate(+1)
 */

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

void arex_input_init(lv_obj_t *scr)
{
    lv_group_t *g = lv_group_create();
    lv_group_set_default(g);

    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr, key_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(g, scr);
    lv_group_focus_obj(scr);

    /* editing=true: encoder enc_diff → LV_KEY_LEFT/RIGHT → key_event_cb */
    lv_group_set_editing(g, true);

    lv_indev_set_group(lv_win32_keypad_device_object,  g);
    lv_indev_set_group(lv_win32_encoder_device_object, g);
}
