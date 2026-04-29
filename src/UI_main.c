#include "../arex_ui/arex_ui_engine.h"
#include "../arex_ui/arex_ui_state.h"
#include "../arex_ui/arex_screen.h"
#include "../arex_ui/arex_data.h"
#include "../lvgl/lvgl.h"
#include "arex_hal_sim/arex_input_pc.h"
#include <math.h>

static lv_timer_t *s_update_task_timer;  /* 50ms UI 消费定时器 */

/* =========================================================
 * sim_tick_cb — 模拟硬件数据源
 *
 * 铁律：只能调用 arex_bus_set_*() 系列函数！
 * 绝对禁止调用任何 LVGL 函数，禁止直接操作 g_sensor_data！
 * ========================================================= */
static void sim_tick_cb(lv_timer_t *t)
{
    (void)t;

    /* 航向缓慢顺时针旋转 */
    uint16_t new_heading = (g_sensor_data.heading + 1) % 360;
    arex_bus_set_heading(new_heading);

    /* 潜水时间 +1s（同时触发左侧面板 + 曲线图刷新） */
    arex_bus_set_dive_time(g_sensor_data.dive_time_s + 1);

    /* 水面休息计时（用于 W.TIME 显示） */
    arex_bus_set_surface_time(g_sensor_data.surface_time_s + 1);

    /* 深度模拟：每秒增加 0.5m */
    float new_depth = g_sensor_data.depth + 2.5f;
    if (new_depth > 50.0f) new_depth = 50.0f;
    arex_bus_set_depth(new_depth);

    /* 深度超过 12m 时，推送模拟减压站序列 */
    if (new_depth > 12.0f) {
        arex_deco_stop_t sim_stops[] = {
            { .depth_m = 9.0f, .stay_min = 2.0f },
            { .depth_m = 6.0f, .stay_min = 3.0f },
            { .depth_m = 3.0f, .stay_min = 1.0f },
        };
        arex_bus_set_deco_plan(sim_stops, 3);
    }

    /* 模拟 NDL 递减 */
    if (g_sensor_data.ndl > 0) {
        arex_bus_set_ndl((int16_t)(g_sensor_data.ndl - 1));
    }

    /* 模拟 TTS 递增 */
    arex_bus_set_tts(g_sensor_data.tts + 1);

    /* 模拟 CNS 缓慢递增 */
    if (g_sensor_data.cns_pct < 100) {
        arex_bus_set_cns((uint8_t)(g_sensor_data.cns_pct + 1));
    }

    /* 模拟 OTU 递增 */
    arex_bus_set_otu((uint16_t)(g_sensor_data.otu + 1));

    /* 模拟 PO2 随深度变化（基于 AIR，fO2=0.21） */
    float new_ppo2 = g_sensor_data.depth * 0.21f;
    if (new_ppo2 < 0.21f) new_ppo2 = 0.21f;
    if (new_ppo2 > 1.6f) new_ppo2 = 1.6f;
    arex_bus_set_ppo2(2, new_ppo2);

    /* 推流历史轨迹点到 4F 曲线图 */
    arex_dive_log_append((float)g_sensor_data.dive_time_s, g_sensor_data.depth);

    /* 模拟温度缓慢变化 */
    static float s_temp_offset = 0.0f;
    s_temp_offset += 1.0f;
    if (s_temp_offset > 5.0f) s_temp_offset = -5.0f;
    arex_bus_set_temperature(25.0f + s_temp_offset);
}

/* =========================================================
 * UI_main — UI 入口
 * ========================================================= */
void UI_main(void)
{
    /* 1. 初始化 UI 引擎（加载默认配置 + 初始化传感器数据） */
    arex_ui_init();

    /* 2. 创建 UI 界面 (安全区 + 左侧锚点 + 卡片) */
    arex_screen_create();

    /* 3. 初始化输入处理[!!!注意：只有PC模拟器需要这个，真机不需要，真机输入直接调用UI_handle_rotate()/UI_handle_click()/UI_handle_back()] */
    lv_obj_t *scr = lv_scr_act();
    arex_input_init(scr);

    /* 4. 渲染初始状态（UI 消费任务一次性消费初始化脏标记） */
    arex_ui_update_task(NULL);

    /* 5. 初始化 UI 状态机 */
    arex_ui_state_init();

    /* 6. 启动在 INFO 卡 (tile 0) */
    arex_screen_scroll_to_card(0);
    arex_screen_set_info_selection(0);

    /* 7. 启动 UI 消费任务定时器：50ms 周期（20 FPS） */
    s_update_task_timer = lv_timer_create(arex_ui_update_task, 50, NULL);

    /* 8. 启动模拟数据定时器：1Hz */
    lv_timer_create(sim_tick_cb, 1000, NULL);
}
