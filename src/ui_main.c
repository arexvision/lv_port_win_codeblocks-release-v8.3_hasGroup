#include "ui/core/ui_engine.h"
#include "ui/core/ui_state.h"
#include "ui/screen/screen.h"
#include "ui/core/ui_dirty.h"
#include "config/build/ui_build_flags.h"
#include "error_screen/error_screen.h"
#include "lvgl/lvgl.h"
#include "ui_test/ui_test.h"
#include "ui_main.h"

#ifdef PC_SIMULATOR
#include "hal_sim/input_pc.h"
#include "hal_sim/sim_data.h"
#else
#include "startup_gif.h"
#endif

static lv_timer_t *s_update_task_timer;  /* UI 数据消费定时器 */

static void ui_bootstrap_force_first_paint(void)
{
    /*
     * UI 基础可用性不能依赖传感器首帧。
     * 启动期布局已经在 UI_main() 之前恢复完毕，screen_create() 会直接按当前
     * g_sys_config 建树；这里不能再人为补一个 DIRTY_UI_LAYOUT，否则刚建好的
     * 对象树会立即再走一次整屏重建，放大半初始化窗口的重入风险。
     *
     * 首刷只补数据域，让左侧 widget、当前页文本和基础状态在没有传感器首帧时
     * 也先显示出来。
     */
    bus_requeue_dirty(DIRTY_DATA_ALL);
    ui_update_task(NULL);
}

void UI_main(void)
{
    ui_init();
    if (error_screen_try_start())
    {
#ifndef PC_SIMULATOR
        app_ui_startup_gif_disable();
#endif
        return;
    }
    if (ui_test_try_start())
    {
        return;
    }

#ifndef PC_SIMULATOR
    /* 设备端启动动画依赖 SF32 资源/GIF 解码；PC 模拟器不进入这条路径。 */
    app_ui_startup_gif_preload();
#endif

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

    s_update_task_timer = lv_timer_create(ui_update_task, APP_UI_UPDATE_TIMER_DELAY_MS, NULL);
    #ifdef PC_SIMULATOR
    sim_data_start();
    #endif
}
