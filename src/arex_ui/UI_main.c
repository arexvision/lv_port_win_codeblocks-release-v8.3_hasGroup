#include "arex_ui_engine.h"
#include "arex_data.h"          /* g_arex — 旧数据总线（过渡期保留） */
#include "arex_ui_state.h"
#include "arex_input.h"
#include "../../lvgl/lvgl.h"
#include <string.h>

static lv_timer_t *s_sim_timer;

/*
 * 每秒仿真 tick：
 *   - 写传感器数据（由底层传感器写入，这里用仿真数据填充）
 *   - 调用 arex_ui_update_data() 刷新 label，绝不触发重新排版
 */
static void sim_tick_cb(lv_timer_t *t)
{
    (void)t;

    /* 仿真：航向 +1°/s，潜水时间 +1s */
    g_sensor.heading_deg = (g_sensor.heading_deg + 1) % 360;
    g_sensor.dive_time_sec++;

    /* 同步到旧数据总线（过渡期兼容旧卡片的 update_cb） */
    g_arex.compass.heading = (float)g_sensor.heading_deg;
    g_arex.dive.dive_time_s = g_sensor.dive_time_sec;

    /* 新引擎：只刷新文本 */
    arex_ui_update_data();

    /* 旧卡片刷新（COMPASS / DECO / GAS / PLAN 等尚未迁移的卡片） */
    arex_ui_refresh_all();
}

void UI_main(void)
{
    /* -------------------------------------------------------
     * 1. 初始化旧数据总线（过渡期保留）
     * ----------------------------------------------------- */
    arex_data_init();
    arex_ui_state_init();

    /* -------------------------------------------------------
     * 2. 将旧数据总线同步到新传感器结构体（桥接）
     * ----------------------------------------------------- */
    memset(&g_sensor, 0, sizeof(g_sensor));
    g_sensor.depth_m        = g_arex.dive.depth;
    g_sensor.heading_deg    = (uint16_t)g_arex.compass.heading;
    g_sensor.dive_time_sec  = g_arex.dive.dive_time_s;
    g_sensor.ndl_min        = g_arex.dive.ndl;
    g_sensor.tts_min        = g_arex.dive.tts;
    g_sensor.next_stop_m    = g_arex.dive.next_stop_m;
    g_sensor.next_stop_min  = g_arex.dive.next_stop_min;
    g_sensor.pod1_bar       = g_arex.dive.pod1_bar;
    g_sensor.pod2_bar       = g_arex.dive.pod2_bar;
    g_sensor.battery_pct    = 85;
    g_sensor.ppo2[0]        = g_arex.gas.ppo2[0];
    g_sensor.ppo2[1]        = g_arex.gas.ppo2[1];
    g_sensor.ppo2[2]        = g_arex.gas.ppo2[2];
    for (int i = 0; i < 16; i++)
        g_sensor.tissue_pct[i] = g_arex.deco.tissue_pct[i];
    g_sensor.gf99    = g_arex.deco.gf99;
    g_sensor.surf_gf = g_arex.deco.surf_gf;
    g_sensor.cns_pct = g_arex.deco.cns_pct;
    g_sensor.otu     = g_arex.deco.otu;

    /* -------------------------------------------------------
     * 3. 启动新 UI 引擎（创建 safe_zone、左锚区、右卡片区）
     * ----------------------------------------------------- */
    arex_ui_engine_init();

    /* -------------------------------------------------------
     * 3b. 创建覆盖浮层（wall/modal/submenu，挂到 right_canvas）
     *     必须在 engine_init 之后调用，因为需要 g_layout.right_canvas
     * ----------------------------------------------------- */
    arex_screen_overlay_create();

    /* -------------------------------------------------------
     * 4. 输入初始化（绑定到当前屏幕）
     * ----------------------------------------------------- */
    lv_obj_t *scr = lv_scr_act();
    arex_input_init(scr);

    /* -------------------------------------------------------
     * 5. 初始化第一帧数据显示
     * ----------------------------------------------------- */
    arex_ui_update_data();

    /* -------------------------------------------------------
     * 6. 启动仿真 tick（1 秒）
     * ----------------------------------------------------- */
    s_sim_timer = lv_timer_create(sim_tick_cb, 1000, NULL);
}
