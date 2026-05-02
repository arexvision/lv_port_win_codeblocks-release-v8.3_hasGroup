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
 * 测试场景（极简版）：
 *   phase=0: 左侧 7 组件 + 右侧 12 组件（标准布局）
 *   phase=1: 左侧 5 组件（减少） + 右侧 6 组件（减少）
 * ========================================================= */
static void arex_test_set_ui_layout(uint8_t phase)
{
    static arex_ble_ui_sync_payload_t s_payload;

    memset(&s_payload, 0, sizeof(s_payload));
    s_payload.version = AREX_BLE_CFG_VERSION;

    /* ========== 卡片顺序：固定不变 ========== */
    uint8_t card_order[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    memcpy(s_payload.card_order, card_order, sizeof(card_order));

    if (phase == 0) {
        /* ========== 布局 A: 标准 7+12 ========== */
        uint8_t left_def[][3] = {
            /* widget_id,           x,  y */
            { WIDGET_NDL_STOP_1606,  0, 0 },
            { WIDGET_DEPTH_1612,    0, 1 },
            { WIDGET_POD_0806,      0, 3 },
            { WIDGET_POD_0806,      1, 3 },
            { WIDGET_WTIME_0806,    0, 4 },
            { WIDGET_GAS_1606,      0, 5 },
            { WIDGET_SYS_1606,      0, 6 },
        };
        s_payload.left_count = sizeof(left_def) / sizeof(left_def[0]);
        for (uint8_t i = 0; i < s_payload.left_count; i++) {
            s_payload.left_widgets[i].widget_id = left_def[i][0];
            s_payload.left_widgets[i].x  = left_def[i][1];
            s_payload.left_widgets[i].y  = left_def[i][2];
        }

        uint8_t custom_5f[][3] = {
            /* widget_id,            r,  c */
            { WIDGET_DEPTH_1612,     0, 0 },
            { WIDGET_TEMP_0806,      0, 2 },
            { WIDGET_HEADING_0806,   0, 3 },
            { WIDGET_SAC_RATE_0806,  2, 0 },
            { WIDGET_BATTERY_0806,   2, 2 },
            { WIDGET_PPO2_0806,      2, 4 },
            { WIDGET_NDL_STOP_1606,  3, 0 },
            { WIDGET_TTS_0806,       3, 2 },
            { WIDGET_CNS_0806,       3, 4 },
            { WIDGET_POD_0806,       4, 0 },
            { WIDGET_POD_0806,       4, 2 },
            { WIDGET_WTIME_0806,     4, 4 },
        };
        s_payload.custom_5f_count = sizeof(custom_5f) / sizeof(custom_5f[0]);
        for (uint8_t i = 0; i < s_payload.custom_5f_count; i++) {
            s_payload.custom_5f_widgets[i].widget_id = custom_5f[i][0];
            s_payload.custom_5f_widgets[i].r  = custom_5f[i][1];
            s_payload.custom_5f_widgets[i].c  = custom_5f[i][2];
        }

    } else {
        /* ========== 布局 B: 减少组件（左侧5个 + 右侧6个）========== */
        uint8_t left_min[][3] = {
            /* widget_id,           x,  y */
            { WIDGET_NDL_STOP_1606,  0, 0 },
            { WIDGET_DEPTH_1612,    0, 1 },
            { WIDGET_POD_0806,      0, 3 },
            { WIDGET_GAS_1606,      0, 5 },
            { WIDGET_SYS_1606,      0, 6 },
        };
        s_payload.left_count = sizeof(left_min) / sizeof(left_min[0]);
        for (uint8_t i = 0; i < s_payload.left_count; i++) {
            s_payload.left_widgets[i].widget_id = left_min[i][0];
            s_payload.left_widgets[i].x  = left_min[i][1];
            s_payload.left_widgets[i].y  = left_min[i][2];
        }

        uint8_t custom_min[][3] = {
            /* widget_id,            r,  c */
            { WIDGET_DEPTH_1612,     0, 0 },
            { WIDGET_TEMP_0806,      0, 2 },
            { WIDGET_BATTERY_0806,   2, 0 },
            { WIDGET_PPO2_0806,      2, 2 },
            { WIDGET_NDL_STOP_1606,  3, 0 },
            { WIDGET_WTIME_0806,     4, 0 },
        };
        s_payload.custom_5f_count = sizeof(custom_min) / sizeof(custom_min[0]);
        for (uint8_t i = 0; i < s_payload.custom_5f_count; i++) {
            s_payload.custom_5f_widgets[i].widget_id = custom_min[i][0];
            s_payload.custom_5f_widgets[i].r  = custom_min[i][1];
            s_payload.custom_5f_widgets[i].c  = custom_min[i][2];
        }
    }

    printf("[TEST] arex_bus_set_ui_layout(phase=%u, left=%u, 5f=%u)\r\n",
           phase, s_payload.left_count, s_payload.custom_5f_count);
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

    /* 布局切换测试：每 5 秒切换一次布局（phase: 0→1→2→0 循环） */
    static uint16_t s_layout_tick = 0;
    static bool s_started = false;
    if (!s_started) {
        printf("[TEST] Layout switch test started (every 5s)...\r\n");
        s_started = true;
    }
    s_layout_tick++;
    if (s_layout_tick % 5 == 0) {  /* 5 秒触发一次 */
        static uint8_t s_layout_phase = 0;
        printf("[TEST] Switching to phase %u\r\n", s_layout_phase);

        /* 【注意】不要在这里调用 lv_disp_enable_invalidation！
         * arex_ui_update_task() 内部已经正确处理了 invalidation。
         * 如果同时在 sim_tick_cb 中禁用，LVGL 显示缓冲区可能在重建后无法正确刷新。 */
        arex_test_set_ui_layout(s_layout_phase);

        s_layout_phase = (s_layout_phase + 1) % 2;  /* 0→1→0 循环测试 */
    }

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

    /* 各阶段参数: { 持续tick数(秒), 每tick深度变化(m) } */
    static const struct { uint8_t ticks; float delta; } s_depth_profiles[4] = {
        { 20,  1.50f },  /* 0: 快速下潜 20s, 20×1.5=30m */
        { 5,   0.00f },  /* 1: 停留5秒 30m不变 */
        { 20, -1.35f },  /* 2: 快速上升 20s, 30m→3m, 27m÷20s=1.35m/s */
        { 10,  0.00f },  /* 3: 3m停留10秒 */
    };

    s_sim_depth += s_depth_profiles[s_depth_phase].delta;
    s_depth_phase_tick++;

    /* 阶段切换 */
    if (s_depth_phase_tick >= s_depth_profiles[s_depth_phase].ticks) {
        s_depth_phase_tick = 0;
        if (s_depth_phase == 3) {
            /* 3m停留结束后，回到水面，开始新循环 */
            s_sim_depth = 0.0f;
            s_depth_phase = 0;
        } else {
            s_depth_phase++;
        }
    }

    /* 边界保护 */
    if (s_sim_depth > 50.0f) s_sim_depth = 50.0f;
    if (s_sim_depth < 0.0f)  s_sim_depth = 0.0f;

    arex_bus_set_depth(s_sim_depth);

    /* ============================================================
     * NDL_STOP 状态机仿真：NDL常态 → Safety停留 → Deco停留
     * 每秒调用一次，驱动停留状态机的自动变身效果
     * ============================================================ */
    {
        static uint16_t s_ndl_tick = 0;
        s_ndl_tick++;

        /* 1. NDL 递减 */
        if (g_sensor_data.ndl > 0) {
            arex_bus_set_ndl((int16_t)(g_sensor_data.ndl - 1));
        }

        /* 2. 常态: 深度 < 5m 且 NDL > 0 */
        g_sensor_data.stop_type = AREX_STOP_NONE;
        g_sensor_data.in_stop_zone = false;

        /* 3. 安全停留: 深度 5~10m，触发 3m 安全停留 180秒 */
        if (s_sim_depth >= 5.0f && s_sim_depth < 10.0f && g_sensor_data.ndl > 0) {
            g_sensor_data.stop_type = AREX_STOP_SAFETY;
            g_sensor_data.stop_depth_m = 3.0f;
            g_sensor_data.stop_time_total_s = 180;
            g_sensor_data.stop_time_left_s = 180 - (s_ndl_tick % 180);
            /* 是否在 ±1.5m 范围内？ */
            g_sensor_data.in_stop_zone = (fabsf(s_sim_depth - 3.0f) <= 1.5f);
        }
        /* 4. 减压停留: 深度 >= 10m 或 NDL 耗尽 */
        else if (s_sim_depth >= 10.0f || g_sensor_data.ndl <= 0) {
            if (g_sensor_data.ndl <= 0) {
                g_sensor_data.ndl = 0;
            }
            g_sensor_data.stop_type = AREX_STOP_DECO;
            g_sensor_data.stop_depth_m = 6.0f;
            g_sensor_data.stop_time_total_s = 300;
            g_sensor_data.stop_time_left_s = 300 - (s_ndl_tick % 300);
            /* 是否在 ±1.5m 范围内？ */
            g_sensor_data.in_stop_zone = (fabsf(s_sim_depth - 6.0f) <= 1.5f);
        }

        /* 强制唤醒 UI 更新（停留状态变化时触发 DIRTY_NDL_STOP） */
        g_sensor_data.dirty_mask |= DIRTY_NDL_STOP;
    }

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
    // lv_timer_create(sim_tick_cb, 1000, NULL);
}
