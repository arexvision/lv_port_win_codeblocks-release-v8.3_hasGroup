#include "ui/core/ui_engine.h"
#include "ui/core/ui_state.h"
#include "ui/screen/screen.h"
#include "ui/core/ui_dirty.h"
#include "lvgl/lvgl.h"
#include "ui_main.h"

#ifdef PC_SIMULATOR
#include "hal_sim/input_pc.h"
#include "hal_sim/sim_data.h"
#endif

static lv_timer_t *s_update_task_timer;  /* 50ms UI 消费定时器 */

static void ui_bootstrap_force_first_paint(void)
{
    /*
     * UI 基础可用性不能依赖传感器首帧。
     * 这里用旧 UI 总线的默认值主动完成首刷：硬件没起来时只影响数值真实性，
     * 不能影响对象树、页面切换、左侧 widget 和当前页的可见性。
     */
    bus_requeue_dirty(DIRTY_UI_LAYOUT);
    ui_update_task(NULL);

    bus_requeue_dirty(DIRTY_DATA_ALL);
    ui_update_task(NULL);
}

void UI_main(void)
{
    ui_init();
    screen_create();
    ui_state_init();
    ui_state_set_state(UI_DASH);
    ui_state_set_dash_page(PAGE_POS_DYNAMIC_FIRST);

    ui_bootstrap_force_first_paint();

    #ifdef PC_SIMULATOR
    lv_obj_t *scr = lv_scr_act();
    input_init(scr);
    #endif
    screen_scroll_to_page(PAGE_POS_DYNAMIC_FIRST);

    s_update_task_timer = lv_timer_create(ui_update_task, 50, NULL);
    #ifdef PC_SIMULATOR
    sim_data_start();
    #endif
}
