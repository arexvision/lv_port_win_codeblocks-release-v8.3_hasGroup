#include "arex_ui/arex_ui_engine.h"
#include "arex_ui/arex_ui_state.h"
#include "arex_ui/arex_screen.h"
#include "arex_ui/arex_data.h"
#include "lvgl/lvgl.h"
#include "arex_hal_sim/arex_input_pc.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static lv_timer_t *s_update_task_timer;  /* 50ms UI 消费定时器 */

/* =========================================================
 * arex_test_set_ui_layout — 构造 BLE 同步帧，模拟 APP 下发布局配置
 *
 * 测试布局 A (phase=0): 标准 2x6 锚点 + 默认卡片顺序
 * 测试布局 B (phase=1): 翻转 2x6 锚点 + 翻转卡片顺序
 * ========================================================= */
static void arex_test_set_ui_layout(uint8_t phase)
{
    static arex_ble_ui_sync_payload_t s_payload;

    memset(&s_payload, 0, sizeof(s_payload));
    s_payload.version = AREX_BLE_CFG_VERSION;

    if (phase == 0) {
        /* 标准布局 */
        uint8_t left_def[][6] = {
            /* id,   x,  y,  w,  h,  font_id */
            { AREX_WIDGET_DEPTH,    0, 0, 2, 1, AREX_FONT_ID_HUGE },   /* 深度占满顶行 */
            { AREX_WIDGET_NDL,     0, 1, 1, 1, AREX_FONT_ID_MEDIUM },   /* NDL 左 */
            { AREX_WIDGET_TTS,     1, 1, 1, 1, AREX_FONT_ID_MEDIUM },   /* TTS 右 */
            { AREX_WIDGET_POD1,    0, 2, 1, 1, AREX_FONT_ID_MEDIUM }, /* POD1 左 */
            { AREX_WIDGET_POD2,    1, 2, 1, 1, AREX_FONT_ID_MEDIUM }, /* POD2 右 */
            { AREX_WIDGET_PPO2,    0, 3, 1, 1, AREX_FONT_ID_MEDIUM }, /* PPO2 左 */
            { AREX_WIDGET_GAS,     1, 3, 1, 1, AREX_FONT_ID_MEDIUM }, /* GAS 右 */
            { AREX_WIDGET_WTIME,   0, 4, 1, 1, AREX_FONT_ID_MEDIUM }, /* W.TIME 左 */
            { AREX_WIDGET_BATTERY, 1, 4, 1, 1, AREX_FONT_ID_MEDIUM }, /* BATTERY 右 */
            { AREX_WIDGET_TEMP,    0, 5, 2, 1, AREX_FONT_ID_SMALL  }, /* TEMP 底行 */
        };
        s_payload.left_count = sizeof(left_def) / sizeof(left_def[0]);
        for (uint8_t i = 0; i < s_payload.left_count; i++) {
            s_payload.left_widgets[i].id      = left_def[i][0];
            s_payload.left_widgets[i].x      = left_def[i][1];
            s_payload.left_widgets[i].y      = left_def[i][2];
            s_payload.left_widgets[i].w      = left_def[i][3];
            s_payload.left_widgets[i].h      = left_def[i][4];
            s_payload.left_widgets[i].font_id = left_def[i][5];
        }
        /* 默认卡片顺序 */
        uint8_t card_order_default[] = { 0, 1, 2, 3, 4, 5 };
        memcpy(s_payload.card_order, card_order_default, sizeof(card_order_default));
        s_payload.custom_5f_count = 0;
    } else {
        /* 翻转布局：交换 x 列索引，左右对调 */
        uint8_t left_rev[][6] = {
            /* id,   x,  y,  w,  h,  font_id */
            { AREX_WIDGET_HEADING,  0, 0, 2, 1, AREX_FONT_ID_HUGE },   /* 航向顶行 */
            { AREX_WIDGET_CNS,     1, 1, 1, 1, AREX_FONT_ID_MEDIUM },  /* CNS 左 */
            { AREX_WIDGET_SAC_RATE,0, 1, 1, 1, AREX_FONT_ID_MEDIUM },  /* SAC 右 */
            { AREX_WIDGET_BATTERY, 1, 2, 1, 1, AREX_FONT_ID_MEDIUM }, /* BATTERY 左 */
            { AREX_WIDGET_TEMP,    0, 2, 1, 1, AREX_FONT_ID_MEDIUM }, /* TEMP 右 */
            { AREX_WIDGET_WTIME,   1, 3, 1, 1, AREX_FONT_ID_MEDIUM }, /* W.TIME 左 */
            { AREX_WIDGET_DEPTH,   0, 3, 1, 1, AREX_FONT_ID_MEDIUM }, /* DEPTH 右 */
            { AREX_WIDGET_PPO2,    0, 4, 2, 1, AREX_FONT_ID_MEDIUM }, /* PPO2 双列 */
            { AREX_WIDGET_NDL,     0, 5, 1, 1, AREX_FONT_ID_SMALL  }, /* NDL 底 */
            { AREX_WIDGET_TTS,     1, 5, 1, 1, AREX_FONT_ID_SMALL  }, /* TTS 底 */
        };
        s_payload.left_count = sizeof(left_rev) / sizeof(left_rev[0]);
        for (uint8_t i = 0; i < s_payload.left_count; i++) {
            s_payload.left_widgets[i].id      = left_rev[i][0];
            s_payload.left_widgets[i].x      = 1 - left_rev[i][1]; /* 列索引翻转 */
            s_payload.left_widgets[i].y      = left_rev[i][2];
            s_payload.left_widgets[i].w      = left_rev[i][3];
            s_payload.left_widgets[i].h      = left_rev[i][4];
            s_payload.left_widgets[i].font_id = left_rev[i][5];
        }
        /* 翻转卡片顺序 */
        uint8_t card_order_rev[] = { 5, 4, 3, 2, 1, 0 };
        memcpy(s_payload.card_order, card_order_rev, sizeof(card_order_rev));
        s_payload.custom_5f_count = 0;
    }

    printf("[TEST] Calling arex_bus_set_ui_layout(phase=%u, left_count=%u)\r\n",
           phase, s_payload.left_count);
    arex_bus_set_ui_layout(&s_payload);
}

/* =========================================================
 * sim_tick_cb — 模拟硬件数据源
 *
 * 铁律：只能调用 arex_bus_set_*() 系列函数！
 * 绝对禁止调用任何 LVGL 函数，禁止直接操作 g_sensor_data！
 * ========================================================= */
static void sim_tick_cb(lv_timer_t *t)
{
    (void)t;

    // static uint16_t s_layout_tick = 0;
    // s_layout_tick++;
    // if (s_layout_tick % 2 == 0) {
    //     static uint8_t phase = 0;
    //     arex_test_set_ui_layout(phase);
    //     phase = (phase + 1) % 2;
    // }

    /* 航向缓慢顺时针旋转 */
    uint16_t new_heading = (g_sensor_data.heading + 1) % 360;
    arex_bus_set_heading(new_heading);

    /* 每 2 秒切换一次气体，用于测试 */
    arex_bus_set_gas(g_sensor_data.gas_active_idx,g_sensor_data.gas_name);

    /* 潜水时间 +1s（同时触发左侧面板 + 曲线图刷新） */
    arex_bus_set_dive_time(g_sensor_data.dive_time_s + 1);

    /* 水面休息计时（用于 W.TIME 显示） */
    arex_bus_set_surface_time(g_sensor_data.surface_time_s + 1);

    /* 深度模拟：每秒增加 0.5m */
    float new_depth = g_sensor_data.depth + 2.0f;
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

    arex_bus_set_battery(g_sensor_data.battery_pct + 1);
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
    lv_timer_create(sim_tick_cb, 100, NULL);
}
