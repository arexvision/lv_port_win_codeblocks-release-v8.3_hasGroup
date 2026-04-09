#include "arex_screen.h"
#include "arex_data.h"
#include "arex_ui_state.h"
#include "lvgl/lvgl.h"

/*
 * Input mapping (PC simulation via keyboard):
 *   UP arrow / Mouse wheel up   → rotate(-1)
 *   DOWN arrow / Mouse wheel dn → rotate(+1)
 *   ENTER                       → click()
 *   ESC / BACKSPACE             → back()
 *   LEFT / RIGHT                → compass style cycle (DASH only)
 */

static void key_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);

    switch (key) {
        case LV_KEY_UP:
            ui_handle_rotate(-1);
            break;
        case LV_KEY_DOWN:
            ui_handle_rotate(+1);
            break;
        case LV_KEY_ENTER:
            ui_handle_click();
            break;
        case LV_KEY_ESC:
        case LV_KEY_BACKSPACE:
            ui_handle_back();
            break;
        case LV_KEY_LEFT:
            if (g_ui.state == UI_DASH) {
                g_arex.compass.style = (g_arex.compass.style + 2) % 3;
                arex_screen_refresh_compass_target();
            }
            break;
        case LV_KEY_RIGHT:
            if (g_ui.state == UI_DASH) {
                g_arex.compass.style = (g_arex.compass.style + 1) % 3;
                arex_screen_refresh_compass_target();
            }
            break;
        default:
            break;
    }
}

void arex_input_init(lv_obj_t *scr)
{
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr, key_event_cb, LV_EVENT_KEY, NULL);

    /* Focus the screen so it receives key events */
    lv_group_t *g = lv_group_create();
    lv_group_add_obj(g, scr);
    lv_group_set_default(g);
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD ||
            lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER) {
            lv_indev_set_group(indev, g);
        }
        indev = lv_indev_get_next(indev);
    }
}
