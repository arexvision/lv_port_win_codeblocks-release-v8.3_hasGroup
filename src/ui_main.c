#include "ui/core/ui_engine.h"
#include "ui/core/ui_state.h"
#include "ui/screen/screen.h"
#include "lvgl/lvgl.h"
#include "ui_main.h"

#ifdef PC_SIMULATOR
#include "hal_sim/input_pc.h"
#include "hal_sim/sim_data.h"
#endif

static lv_timer_t *s_update_task_timer;  /* 50ms UI 消费定时器 */

void UI_main(void)
{
    ui_init();
    screen_create();

    #ifdef PC_SIMULATOR
    lv_obj_t *scr = lv_scr_act();
    input_init(scr);
    #endif

    ui_update_task(NULL);
    ui_state_init();

    ui_state_set_state(UI_DASH);
    ui_state_set_dash_page(PAGE_POS_DYNAMIC_FIRST);
    screen_scroll_to_page(PAGE_POS_DYNAMIC_FIRST);

    s_update_task_timer = lv_timer_create(ui_update_task, 50, NULL);
    #ifdef PC_SIMULATOR
    sim_data_start();
    #endif
}
