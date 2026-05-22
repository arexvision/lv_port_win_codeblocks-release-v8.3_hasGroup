#include "arex_ui/core/ui_engine.h"
#include "arex_ui/core/ui_state.h"
#include "arex_ui/screen/screen.h"
#include "arex_hal_sim/input_pc.h"
#include "arex_hal_sim/sim_data.h"
#include "lvgl/lvgl.h"
#include "ui_main.h"

static lv_timer_t *s_update_task_timer;  /* 50ms UI 消费定时器 */

void UI_main(void)
{
    arex_ui_init();
    arex_screen_create();

    #ifdef PC_SIMULATOR
    lv_obj_t *scr = lv_scr_act();
    arex_input_init(scr);
    #endif

    arex_ui_update_task(NULL);
    arex_ui_state_init();

#if AREX_ENABLE_INFO_MENU
    g_ui.state = UI_INFO;
    g_ui.dash_card = CARD_POS_INFO;
    g_ui.menu_info_idx = 0;
    arex_screen_scroll_to_card(CARD_POS_INFO);
    arex_screen_set_info_selection(0);
#else
    arex_screen_scroll_to_card(CARD_POS_DYNAMIC_FIRST);
#endif

    s_update_task_timer = lv_timer_create(arex_ui_update_task, 50, NULL);
    #ifdef PC_SIMULATOR
    arex_sim_data_start();
    #endif
}
