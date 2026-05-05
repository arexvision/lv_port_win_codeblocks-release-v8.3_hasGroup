#include "arex_ui/arex_ui_engine.h"
#include "arex_ui/arex_ui_state.h"
#include "arex_ui/arex_screen.h"
#include "arex_ui/arex_data.h"
#include "lvgl/lvgl.h"
#include "UI_main.h"
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
    s_payload.version = 0x01; // AREX_BLE_CFG_VERSION

    /* ========== 卡片顺序：固定不变 ========== */
    uint8_t card_order[] = { 0, 1, 2, 3, 4, 5, 6 };
    memcpy(s_payload.card_order, card_order, sizeof(card_order));

    if (phase == 0) {
        /* ========== 布局 A: DEPTH 2x1 (短条形) ========== */
        uint8_t left_def[][3] = {
            /* id,                   x,  y */
            { WIDGET_NDL_STOP_1606,  0, 0 },
            { WIDGET_DEPTH_1606,     0, 1 },
            { WIDGET_DIVE_TIME_1606, 0, 3 },  /* 潜水时间 */
            { WIDGET_GAS_1606,       0, 4 },
            { WIDGET_POD_0806,       0, 5 },
            { WIDGET_POD_0806,       1, 5 },
            { WIDGET_SYS_1606,       0, 6 },
        };
        s_payload.left_count = sizeof(left_def) / sizeof(left_def[0]);
        for (uint8_t i = 0; i < s_payload.left_count; i++) {
            s_payload.left_widgets[i].widget_id = left_def[i][0];
            s_payload.left_widgets[i].x  = left_def[i][1];
            s_payload.left_widgets[i].y  = left_def[i][2];
        }

        uint8_t custom_5f[][3] = {
            /* id,                   r,  c */
            { WIDGET_TEMP_0806,      0, 0 },
            { WIDGET_TEMP_0806,      0, 2 },
            { WIDGET_HEADING_0806,   0, 3 },
            { WIDGET_EMPTY,            2, 0 },  /* SAC 已移除 */
            { WIDGET_BATTERY_0806,   2, 2 },
            { WIDGET_PPO2_0806,      2, 4 },
            { WIDGET_NDL_STOP_1606,  3, 0 },
            { WIDGET_TTS_0806,       3, 2 },
            { WIDGET_CNS_0806,       3, 4 },
            { WIDGET_POD_0806,       4, 0 },
            { WIDGET_POD_0806,       4, 2 },
            { WIDGET_EMPTY,            4, 4 },  /* WTIME 已移除 */
        };
        s_payload.custom_5f_count = sizeof(custom_5f) / sizeof(custom_5f[0]);
        for (uint8_t i = 0; i < s_payload.custom_5f_count; i++) {
            s_payload.custom_5f_widgets[i].widget_id = custom_5f[i][0];
            s_payload.custom_5f_widgets[i].r  = custom_5f[i][1];
            s_payload.custom_5f_widgets[i].c  = custom_5f[i][2];
        }

    } else if (phase == 1) {
        /* ========== 布局 B: DEPTH 2x2 (大块) ========== */
        uint8_t left_min[][3] = {
            /* id,                   x,  y */
            { WIDGET_NDL_STOP_1606,  0, 0 },
            { WIDGET_DEPTH_1612,     0, 1 },  /* 2x2 大块 */
            { WIDGET_DIVE_TIME_1606, 0, 3 },  /* 潜水时间 */
            { WIDGET_GAS_1606,       0, 4 },
            { WIDGET_POD_0806,       0, 5 },
            { WIDGET_POD_0806,       1, 5 },
            { WIDGET_SYS_1606,       0, 6 },
        };
        s_payload.left_count = sizeof(left_min) / sizeof(left_min[0]);
        for (uint8_t i = 0; i < s_payload.left_count; i++) {
            s_payload.left_widgets[i].widget_id = left_min[i][0];
            s_payload.left_widgets[i].x  = left_min[i][1];
            s_payload.left_widgets[i].y  = left_min[i][2];
        }

        uint8_t custom_min[][3] = {
            /* id,                   r,  c */
            { WIDGET_TEMP_0806,      0, 0 },
            { WIDGET_TEMP_0806,      0, 2 },
            { WIDGET_BATTERY_0806,   2, 0 },
            { WIDGET_PPO2_0806,      2, 2 },
            { WIDGET_NDL_STOP_1606,  3, 0 },
            { WIDGET_EMPTY,            4, 0 },  /* WTIME 已移除 */
        };
        s_payload.custom_5f_count = sizeof(custom_min) / sizeof(custom_min[0]);
        for (uint8_t i = 0; i < s_payload.custom_5f_count; i++) {
            s_payload.custom_5f_widgets[i].widget_id = custom_min[i][0];
            s_payload.custom_5f_widgets[i].r  = custom_min[i][1];
            s_payload.custom_5f_widgets[i].c  = custom_min[i][2];
        }
        
    } else {
        /* ========== 布局 C: 极端打乱测试 (验证架构解耦) ========== */
        uint8_t left_crazy[][3] = {
            /* id,                   x,  y */
            { WIDGET_SYS_1606,       0, 0 }, /* 🚨 SYS 放最顶上！验证硬编码是否彻底拔除 */
            { WIDGET_COMPASS_1612,   0, 1 }, /* 罗盘也可以放在左侧 (2x2) */
            { WIDGET_TIME_1606,      0, 3 }, 
            { WIDGET_POD_0806,       0, 4 }, /* POD 1 */
            { WIDGET_POD_0806,       1, 4 }, /* POD 2 */
            { WIDGET_NDL_STOP_1606,  0, 5 }, /* NDL 放最底下 (2x1) */
        };
        s_payload.left_count = sizeof(left_crazy) / sizeof(left_crazy[0]);
        for (uint8_t i = 0; i < s_payload.left_count; i++) {
            s_payload.left_widgets[i].widget_id = left_crazy[i][0];
            s_payload.left_widgets[i].x  = left_crazy[i][1];
            s_payload.left_widgets[i].y  = left_crazy[i][2];
        }

        uint8_t custom_crazy[][3] = {
            /* id,                   r,  c */
            { WIDGET_ASCENT_0812,    0, 0 }, /* 速率箭头 1x2 */
            { WIDGET_ASCENT_0812,    0, 2 }, /* 速率箭头 1x2 */
            { WIDGET_GAS_1606,       2, 0 },
            { WIDGET_SURF_GF_0806,   3, 0 },
            { WIDGET_GF99_0806,      3, 1 },
            { WIDGET_MOD_0806,       3, 2 },
            { WIDGET_CEILING_0806,   3, 3 },
            { WIDGET_BATTERY_0806,   4, 0 },
            { WIDGET_TEMP_0806,      4, 1 },
        };
        s_payload.custom_5f_count = sizeof(custom_crazy) / sizeof(custom_crazy[0]);
        for (uint8_t i = 0; i < s_payload.custom_5f_count; i++) {
            s_payload.custom_5f_widgets[i].widget_id = custom_crazy[i][0];
            s_payload.custom_5f_widgets[i].r  = custom_crazy[i][1];
            s_payload.custom_5f_widgets[i].c  = custom_crazy[i][2];
        }
    }

    printf("[TEST] arex_bus_set_ui_layout(phase=%u, left=%u, 5f=%u)\r\n",
           phase, s_payload.left_count, s_payload.custom_5f_count);
           
    /* 触发底层总线数据装载与清屏重建 */
    arex_bus_set_ui_layout(&s_payload);
}

/* =========================================================
 * arex_test_set_ui_offset — 模拟 BLE 下发 SafeZone 偏移
 *
 * 测试场景：10秒内 y 先变化，然后 x 再变化
 *   - 0~10s:  y 从 -10 → -20 → -10 循环
 *   - 10~20s: x 从 0 → 20 → 0 循环
 *   - 之后:    x/y 交替变化
 * ========================================================= */
static void arex_test_set_ui_offset(void)
{
    static uint32_t s_offset_start_tick = 0;
    static uint8_t  s_offset_phase = 0;

    if (s_offset_start_tick == 0) {
        s_offset_start_tick = lv_tick_get();
        printf("[TEST] arex_bus_set_ui_offset test started\r\n");
    }

    uint32_t elapsed_s = (lv_tick_get() - s_offset_start_tick) / 1000;

    if (elapsed_s < 10) {
        /* 0~10s: y 变化 */
        int16_t y_val = -10 + (int16_t)((elapsed_s % 5) * 2); /* -10, -8, -6, -8, -10 循环 */
        arex_bus_set_ui_offset(0, y_val);
    } else if (elapsed_s < 20) {
        /* 10~20s: x 变化 */
        int16_t x_val = (int16_t)((elapsed_s % 5) * 5); /* 0, 5, 10, 15, 0 循环 */
        arex_bus_set_ui_offset(x_val, -10);
    } else {
        /* 20s+: x/y 交替变化 */
        if (s_offset_phase == 0) {
            arex_bus_set_ui_offset(10, -10);
        } else {
            arex_bus_set_ui_offset(0, -20);
        }
        s_offset_phase = 1 - s_offset_phase;
    }
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
// arex_bus_toggle_layout_order();
    // /* 布局切换测试：每秒切换一次布局（phase: 0→1→0 循环，DEPTH 2x1 ↔ 2x2） */
    // static uint16_t s_layout_tick = 0;
    // static bool s_started = false;
    // if (!s_started) {
    //     printf("[TEST] DEPTH layout switch test started (every 1s): 2x1 ↔ 2x2\r\n");
    //     s_started = true;
    // }
    // s_layout_tick++;
    // if (s_layout_tick % 1 == 0) {  /* 每秒触发一次 */
    //     static uint8_t s_layout_phase = 0;
    //     printf("[TEST] Switching to phase %u: DEPTH %s\r\n",
    //            s_layout_phase, (s_layout_phase == 0) ? "WIDGET_DEPTH_1606 (2x1)" : "WIDGET_DEPTH_1612 (2x2)");

    //     arex_test_set_ui_layout(s_layout_phase);

    //     s_layout_phase = 1 - s_layout_phase;  /* 0↔1 切换 */
    // }

    /* 航向缓慢顺时针旋转 */
    uint16_t new_heading = (g_sensor_data.heading + 1) % 360;
    arex_bus_set_heading(new_heading);

    /* SafeZone 偏移测试：10秒内 y 先变化，然后 x 再变化 */
    // arex_test_set_ui_offset();

    /* 模拟器：交替写入真实的气体名字 */
    static uint8_t mock_gas_toggle = 0;
    if (mock_gas_toggle == 0) {
        arex_bus_set_gas(0, "AIR");
        mock_gas_toggle = 1;
    } else {
        arex_bus_set_gas(1, "NX 32");
        mock_gas_toggle = 0;
    }

    /* 潜水时间 +1s（同时触发左侧面板 + 曲线图刷新） */
    g_sensor_data.dive_time_s ++;
    arex_bus_set_dive_time(g_sensor_data.dive_time_s);

    /* 水面休息计时（用于 W.TIME 显示） */
    arex_bus_set_surface_time(g_sensor_data.surface_time_s + 1);

    /* ============================================================
     * 纯净版完美模拟剧本：常态下潜 -> 安全停留 -> 强制减压
     * 保证你在 90 秒内看完所有最完美的形态转换与动画！
     * ============================================================ */
    // {
    //     static uint32_t s_sim_ticks = 0;
    //     s_sim_ticks++;

    //     float current_sim_depth = 0.0f;

    //     if (s_sim_ticks < 5) {
    //         /* 阶段 1：平滑下潜 (0~30秒) 5m 缓缓沉入 18.5m */
    //         current_sim_depth = 5.0f + (s_sim_ticks * 0.45f);

    //         /* 常态 NDL 模式：使用综合接口一次性更新 */
    //         arex_bus_update_deco(
    //             45,              /* ndl_min: NDL 充足 */
    //             AREX_STOP_NONE,   /* stop_type: 无停留 */
    //             0.0f,            /* depth_m: 无停留深度 */
    //             0                /* time_s: 无停留时间 */
    //         );
    //     }
    //     else if (s_sim_ticks < 10) {
    //         /* 阶段 2：快速上升到 5m，触发安全停留 (30~60秒) */
    //         current_sim_depth = 4.8f;
    //         uint16_t elapsed_s = (s_sim_ticks - 30) * 6;
    //         uint16_t left_s = (elapsed_s > 180) ? 0 : (180 - elapsed_s);

    //         /* 安全停留模式：使用综合接口一次性更新 */
    //         arex_bus_update_deco(
    //             45,                 /* ndl_min: NDL 充足 */
    //             AREX_STOP_SAFETY,    /* stop_type: 安全停留 */
    //             5.0f,               /* depth_m: 3m 停留深度 */
    //             180               /* time_s: 剩余秒数 */
    //         );
    //     }
    //     else if (s_sim_ticks < 15) {
    //         /* 阶段 3：突发状况，下沉到 6.2m，NDL 耗尽触发强制减压 (60~90秒) */
    //         current_sim_depth = 6.2f;
    //         uint16_t elapsed_s = (s_sim_ticks - 60) * 10;
    //         uint16_t left_s = (elapsed_s > 300) ? 0 : (300 - elapsed_s);

    //         /* 减压停留模式：使用综合接口一次性更新 */
    //         arex_bus_update_deco(
    //             0,                    /* ndl_min: NDL 归零，进入减压区 */
    //             AREX_STOP_DECO,       /* stop_type: 减压停留 */
    //             6.0f,               /* depth_m: 6m 停留深度 */
    //             50                /* time_s: 剩余秒数 */
    //         );
    //     }
    //     else {
    //         /* 循环重置 */
    //         s_sim_ticks = 0;
    //         current_sim_depth = 5.0f;
    //     }

        /* 剧本：下降 → 停留5秒 → 上升 → 循环 */
        static uint16_t i = 0;
        static uint8_t phase = 0;  /* 0=下降, 1=停留, 2=上升 */
        static float current_sim_depth = 0.0f;

        switch (phase) {
            case 0:  /* 下降阶段 */
                if (i < 6) {
                    i++;
                    current_sim_depth += 2.12f;
                } else {
                    phase = 1;  /* 进入停留 */
                    i = 0;
                }
                break;
            case 1:  /* 停留5秒 */
                if (i < 5) {
                    i++;
                    /* 深度不变 */
                } else {
                    phase = 2;  /* 进入上升 */
                    i = 0;
                }
                break;
            case 2:  /* 上升阶段 */
                if (i < 5) {
                    i++;
                    current_sim_depth -= 2.12f;
                    if (current_sim_depth < 0) current_sim_depth = 0;
                } else {
                    phase = 0;  /* 回到下降 */
                    i = 0;
                    current_sim_depth = 0;
                }
                break;
        }
        // printf("g_sensor_data.dive_time_s: %d\r\n", g_sensor_data.dive_time_s);
        arex_dive_log_append((float)g_sensor_data.dive_time_s, current_sim_depth);
        arex_bus_set_depth(current_sim_depth);

        // /* 深度超过 12m 时，推送模拟减压站序列 */
        if (current_sim_depth > 12.0f) {
            arex_deco_stop_t sim_stops[] = {
                { .depth_m = 9.0f, .stay_min = 2.0f },
                { .depth_m = 6.0f, .stay_min = 3.0f },
                { .depth_m = 3.0f, .stay_min = 1.0f },
            };
            arex_bus_set_deco_plan(sim_stops, 3);
        }
    // }

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

    /* ============================================================
     * 告警模拟测试：每 5 秒切换一次 DEPTH 告警
     * 验证左侧锚点 DEPTH 组件是否能同步闪烁
     * ============================================================ */
    // {
    //     static uint32_t s_alarm_test_tick = 0;
    //     static bool s_alarm_active = false;
    //     s_alarm_test_tick++;

    //     if (s_alarm_test_tick % 3 == 0) {  /* 每 3 秒 (1s * 3 = 3s) */
    //         if (s_alarm_active) {
    //             /* 清除告警 */
    //             arex_clear_all_alarm_styles();
    //             printf("[ALARM TEST] Critical Alarm cleared!\r\n");
    //             s_alarm_active = false;
    //         } else {
    //             /* 触发 Level 3 致命告警，文字为 "ASCENT RATE FAST" */
    //             arex_trigger_alarm(AREX_ALARM_CRIT, "ASCENT RATE FAST", WIDGET_DEPTH_1606);

    //             printf("[ALARM TEST] CRITICAL ASCENT RATE triggered!\r\n");
    //             s_alarm_active = true;
    //         }
    //     }
    // }

    arex_bus_set_battery(g_sensor_data.battery_pct + 1.2);
    /* 模拟温度缓慢变化 */
    static float s_temp_offset = 0.0f;
    s_temp_offset += 1.0f;
    if (s_temp_offset > 5.0f) s_temp_offset = -5.0f;
    arex_bus_set_temperature(25.0f + s_temp_offset);

    /* 组织舱模拟：硬编码 16 舱饱和度 */
    static uint8_t s_tissue[16] = { 20, 30, 40, 50, 60, 65, 70, 72, 74, 76, 78, 80, 82, 85, 88, 90 };
    arex_bus_set_tissues(s_tissue);
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

    // /* 8. 启动模拟数据定时器：1Hz */
    lv_timer_create(sim_tick_cb, 1000, NULL);
}
