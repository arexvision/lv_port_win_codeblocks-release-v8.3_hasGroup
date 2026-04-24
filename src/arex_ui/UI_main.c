#include "arex_ui_engine.h"
#include "arex_ui_state.h"
#include "arex_screen.h"
#include "arex_input.h"
#include "../../lvgl/lvgl.h"

static lv_timer_t *s_sim_timer;

/* 模拟数据跳动回调 (1Hz)
 * 仅更新 lv_label 文字，不触发排版重构。 */
static void sim_tick_cb(lv_timer_t *t)
{
    (void)t;

    /* 航向缓慢顺时针旋转 */
    g_sensor_data.heading = (g_sensor_data.heading + 1) % 360;
    /* 倍速模拟: 1 秒真实时间 = 10 秒潜水时间 */
    g_sensor_data.dive_time_s += 10;
    /* 深度: 每秒增加 0.2m，模拟下潜 */
    g_sensor_data.depth += 0.5f;
    if (g_sensor_data.depth > 50) g_sensor_data.depth = 50;

    /* 刷新左侧面板 */
    arex_screen_refresh_left_panel();

    /* 刷新所有卡片 */
    arex_ui_refresh_all();
}

void UI_main(void)
{
    /* 1. 初始化 UI 引擎（加载默认配置 + 初始化潜水数据） */
    arex_ui_init();

    /* 2. 创建 UI 界面 (安全区 + 左侧锚点 + 卡片) */
    arex_screen_create();

    /* 3. 初始化输入处理 */
    lv_obj_t *scr = lv_scr_act();
    arex_input_init(scr);

    /* 4. 刷新左侧面板初始值 */
    arex_screen_refresh_left_panel();

    /* 5. 初始化 UI 状态机 */
    arex_ui_state_init();

    /* 7. 启动在 INFO 卡 (tile 0) */
    arex_screen_scroll_to_card(0);
    arex_screen_set_info_selection(0);

    /* 8. 模拟定时器: 1Hz */
    s_sim_timer = lv_timer_create(sim_tick_cb, 1000, NULL);
}
