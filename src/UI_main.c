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
        /* 标准布局：APP 只下发 [id, x, y]，MCU 查样式表获取 span_w/h */
        uint8_t left_def[][3] = {
            /* id,              x,  y */
            { WIDGET_DEPTH_1612,    0, 0 },   /* 深度占满顶行 */
            { WIDGET_NDL_STOP_1606, 0, 1 },   /* NDL 左 */
            { WIDGET_TTS_0806,     1, 1 },   /* TTS 右 */
            { WIDGET_POD_0806,     0, 2 },   /* POD1 左 */
            { WIDGET_POD_0806,     1, 2 },   /* POD2 右 */
            { WIDGET_PPO2_0806,    0, 3 },   /* PPO2 左 */
            { WIDGET_GAS_1606,     1, 3 },   /* GAS 右 */
            { WIDGET_WTIME_0806,   0, 4 },   /* W.TIME 左 */
            { WIDGET_BATTERY_0806, 1, 4 },   /* BATTERY 右 */
            { WIDGET_TEMP_0806,    0, 5 },   /* TEMP 底行 */
        };
        s_payload.left_count = sizeof(left_def) / sizeof(left_def[0]);
        for (uint8_t i = 0; i < s_payload.left_count; i++) {
            s_payload.left_widgets[i].id = left_def[i][0];
            s_payload.left_widgets[i].x  = left_def[i][1];
            s_payload.left_widgets[i].y  = left_def[i][2];
        }
        /* 默认卡片顺序 */
        uint8_t card_order_default[] = { 0, 1, 2, 3, 4, 5 };
        memcpy(s_payload.card_order, card_order_default, sizeof(card_order_default));
        s_payload.custom_5f_count = 0;
    } else {
        /* 翻转布局：交换 x 列索引，左右对调 */
        uint8_t left_rev[][3] = {
            /* id,               x,  y */
            { WIDGET_HEADING_0806,  0, 0 },   /* 航向顶行 */
            { WIDGET_CNS_0806,     1, 1 },   /* CNS 左 */
            { WIDGET_SAC_RATE_0806,0, 1 },   /* SAC 右 */
            { WIDGET_BATTERY_0806, 1, 2 },   /* BATTERY 左 */
            { WIDGET_TEMP_0806,    0, 2 },   /* TEMP 右 */
            { WIDGET_WTIME_0806,   1, 3 },   /* W.TIME 左 */
            { WIDGET_DEPTH_1612,   0, 3 },   /* DEPTH 右 */
            { WIDGET_PPO2_0806,    0, 4 },   /* PPO2 双列 */
            { WIDGET_NDL_STOP_1606,0, 5 },   /* NDL 底 */
            { WIDGET_TTS_0806,     1, 5 },   /* TTS 底 */
        };
        s_payload.left_count = sizeof(left_rev) / sizeof(left_rev[0]);
        for (uint8_t i = 0; i < s_payload.left_count; i++) {
            s_payload.left_widgets[i].id = left_rev[i][0];
            s_payload.left_widgets[i].x  = 1 - left_rev[i][1]; /* 列索引翻转 */
            s_payload.left_widgets[i].y  = left_rev[i][2];
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

    /* 深度模拟：快速下潜 → 停留5秒 → 快速上升 → 3m停留10秒 → 循环 */
    static uint8_t s_depth_phase = 0;      /* 0: 快速下潜, 1: 停留5s, 2: 快速上升, 3: 3m停留10s */
    static uint8_t s_depth_phase_tick = 0; /* 当前阶段已执行 tick 数 */
    static float s_sim_depth = 0.0f;       /* 模拟深度值 */
    static uint8_t s_cycle_count = 0;     /* 循环计数，用于区分停留阶段 */

    /* 各阶段参数: { 持续tick数(秒), 每tick深度变化(m) } */
    static const struct { uint8_t ticks; float delta; } s_depth_profiles[4] = {
        { 20,  1.50f },  /* 0: 快速下潜 20s, 20×1.5=30m */
        { 5,   0.00f },  /* 1: 停留5秒 30m不变 */
        { 20, -1.35f },  /* 2: 快速上升 20s, 30m→3m, 27m÷20s=1.35m/s */
        { 10,  0.00f },  /* 3: 3m停留10秒 */
    };

    // s_sim_depth += s_depth_profiles[s_depth_phase].delta;
    // s_depth_phase_tick++;

    // /* 阶段切换 */
    // if (s_depth_phase_tick >= s_depth_profiles[s_depth_phase].ticks) {
    //     s_depth_phase_tick = 0;
    //     if (s_depth_phase == 3) {
    //         /* 3m停留结束后，回到水面，开始新循环 */
    //         s_sim_depth = 0.0f;
    //         s_depth_phase = 0;
    //     } else {
    //         s_depth_phase++;
    //     }
    // }

    // /* 边界保护 */
    // if (s_sim_depth > 50.0f) s_sim_depth = 50.0f;
    // if (s_sim_depth < 0.0f)  s_sim_depth = 0.0f;

    // arex_bus_set_depth(s_sim_depth);

    /* ============================================================
     * NDL_STOP 状态机仿真：单向剧本式状态机
     *
     * 阶段 A (0-10s): 10秒快速下潜到 60m，NDL 持续减少，状态 1 (AREX_STOP_NONE)
     * 阶段 B (10-130s): NDL 耗尽归零，在 60m 停留 2min，强制 DECO 减压，状态 3
     * 阶段 C (130-150s): 20秒内上升到 6m 减压站，开始读秒，状态 3
     * 阶段 D (150-170s): 20秒内上升到 5m，减压完成，触发安全停留，状态 2 (AREX_STOP_SAFETY)
     * 阶段 E (170s+): 全部做完，出水，循环剧本
     * ============================================================ */
    {
        static int test_dive_sec = 0;
        test_dive_sec++;

        if (test_dive_sec <= 10) {
            /* 阶段 A：10秒快速下潜到 60m，NDL 持续减少 (状态 1) */
            g_sensor_data.depth = 5.0f + (test_dive_sec * 5.5f);  /* 10秒到60m */
            g_sensor_data.ndl = 99 - (test_dive_sec * 8);  /* 快速递减 */
            g_sensor_data.stop_type = AREX_STOP_NONE;
            g_sensor_data.stop_time_total_s = 0;

        } else if (test_dive_sec <= 130) {
            /* 阶段 B：120秒停留在 60m，NDL 耗尽归零，进入 DECO 减压模式 (状态 3) */
            g_sensor_data.ndl = 0;
            g_sensor_data.depth = 60.0f;
            g_sensor_data.stop_type = AREX_STOP_DECO;
            g_sensor_data.stop_depth_m = 6.0f;
            g_sensor_data.stop_time_total_s = 120;
            g_sensor_data.stop_time_left_s = 120 - (test_dive_sec - 10);
            g_sensor_data.in_stop_zone = false;  /* 深度没到 6m，不读秒 */

        } else if (test_dive_sec <= 150) {
            /* 阶段 C：20秒内上升到 6m 减压站，开始读秒 (状态 3) */
            float rise_progress = (test_dive_sec - 130) / 20.0f;
            g_sensor_data.depth = 60.0f - (rise_progress * 54.0f);  /* 60m -> 6m */
            g_sensor_data.in_stop_zone = true;   /* 进入读秒区 */
            g_sensor_data.stop_time_left_s = 120 - (test_dive_sec - 10);

        } else if (test_dive_sec <= 170) {
            /* 阶段 D：20秒内上升到 5m，减压完成，触发安全停留 (状态 2) */
            float rise_progress = (test_dive_sec - 150) / 20.0f;
            g_sensor_data.depth = 6.0f - (rise_progress * 1.0f);  /* 6m -> 5m */
            g_sensor_data.ndl = 15;  /* 减压完了，安全了 */
            g_sensor_data.stop_type = AREX_STOP_SAFETY;
            g_sensor_data.stop_depth_m = 5.0f;
            g_sensor_data.stop_time_total_s = 180;
            g_sensor_data.stop_time_left_s = 180 - (test_dive_sec - 150);
            g_sensor_data.in_stop_zone = true;

        } else {
            /* 阶段 E：全部做完，出水 */
            test_dive_sec = 0;  /* 循环剧本 */
        }

        /* 强制唤醒 UI 更新 */
        g_sensor_data.dirty_mask |= (DIRTY_NDL | DIRTY_DEPTH | DIRTY_NDL_STOP);
    }
    arex_bus_set_depth(g_sensor_data.depth);
    /* 深度超过 12m 时，推送模拟减压站序列 */
    if (s_sim_depth > 12.0f) {
        arex_deco_stop_t sim_stops[] = {
            { .depth_m = 9.0f, .stay_min = 2.0f },
            { .depth_m = 6.0f, .stay_min = 3.0f },
            { .depth_m = 3.0f, .stay_min = 1.0f },
        };
        arex_bus_set_deco_plan(sim_stops, 3);
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

    arex_bus_set_battery(g_sensor_data.battery_pct + 1.2);
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
