#include "arex_data.h"
#include "arex_ui_engine.h"
#include <string.h>

/* =========================================
   实时数据总线定义 (RAM Only)
   ========================================= */
arex_sensor_data_t g_sensor_data;

/* =========================================
   气体名称表 (供外部引用)
   ========================================= */
const char *AREX_GAS_NAMES[AREX_GAS_COUNT] = {
    "AIR",
    "NX 32",
    "TX 18/45",
    "O2 100%"
};

/* 气体 MOD 表 (单位: 米) */
const uint8_t AREX_GAS_MOD_M[AREX_GAS_COUNT] = {
    56,  /* AIR */
    34,  /* NX 32 */
    68,  /* TX 18/45 */
    6    /* O2 100% */
};

/* =========================================
   Legacy g_arex 全局实例
   兼容旧代码，逐步迁移至 g_sensor_data
   ========================================= */
arex_state_t g_arex;

/* =========================================
   arex_data_init — 同时初始化 legacy g_arex
   和新的 g_sensor_data (由 arex_ui_init 调用)
   ========================================= */
void arex_data_init(void)
{
    /* Dive */
    g_arex.dive.depth          = 45.2f;
    g_arex.dive.ndl            = 5;
    g_arex.dive.tts            = 24;
    g_arex.dive.next_stop_m    = 21;
    g_arex.dive.next_stop_min  = 3;
    g_arex.dive.dive_time_s    = 38 * 60 + 14;
    g_arex.dive.pod1_bar       = 210;
    g_arex.dive.pod2_bar       = 195;

    /* Compass */
    g_arex.compass.heading     = 265.0f;
    g_arex.compass.marked      = false;
    g_arex.compass.target      = 0.0f;
    g_arex.compass.style       = 0;

    /* Deco / Tissues */
    static const uint8_t tissue_demo[16] = {
        95, 85, 75, 60, 50, 40, 35, 20,
        15, 10,  8,  5,  4,  3,  2,  1
    };
    for (int i = 0; i < 16; i++) {
        g_arex.deco.tissue_pct[i] = tissue_demo[i];
    }
    g_arex.deco.gf99    = 82;
    g_arex.deco.surf_gf = 145;
    g_arex.deco.cns_pct = 15;
    g_arex.deco.otu     = 22;

    /* Gas */
    g_arex.gas.active_idx = 2;
    g_arex.gas.ppo2[0]  = 1.2f;
    g_arex.gas.ppo2[1]  = 1.2f;
    g_arex.gas.ppo2[2]  = 1.3f;

    /* Settings */
    g_arex.settings.mod_ppo2     = 1.4f;
    g_arex.settings.conservatism = 1;
    g_arex.settings.brightness   = 2;
    for (uint8_t i = 0; i < 6; i++) {
        g_arex.settings.card_order[i] = i;
    }

    /* --- 同步到新的 g_sensor_data --- */
    g_sensor_data.depth          = g_arex.dive.depth;
    g_sensor_data.ndl           = g_arex.dive.ndl;
    g_sensor_data.tts            = g_arex.dive.tts;
    g_sensor_data.pod1_bar       = (float)g_arex.dive.pod1_bar;
    g_sensor_data.pod2_bar       = (float)g_arex.dive.pod2_bar;
    g_sensor_data.battery_pct    = 85.0f;
    g_sensor_data.ppo2[0]        = g_arex.gas.ppo2[0];
    g_sensor_data.ppo2[1]        = g_arex.gas.ppo2[1];
    g_sensor_data.ppo2[2]        = g_arex.gas.ppo2[2];
    g_sensor_data.gas_active_idx = g_arex.gas.active_idx;
    strncpy(g_sensor_data.gas_name, AREX_GAS_NAMES[g_arex.gas.active_idx], 15);
    g_sensor_data.gas_name[15] = '\0';
    g_sensor_data.heading        = (uint16_t)g_arex.compass.heading;
    g_sensor_data.heading_locked = g_arex.compass.marked;
    g_sensor_data.heading_target  = (uint16_t)g_arex.compass.target;
    g_sensor_data.dive_time_s    = g_arex.dive.dive_time_s;
    for (uint8_t i = 0; i < 16; i++) {
        g_sensor_data.tissue_pct[i] = g_arex.deco.tissue_pct[i];
    }
    g_sensor_data.cns_pct          = g_arex.deco.cns_pct;
    g_sensor_data.otu              = g_arex.deco.otu;
    g_sensor_data.next_stop_m       = g_arex.dive.next_stop_m;
    g_sensor_data.next_stop_min     = g_arex.dive.next_stop_min;
    g_sensor_data.deco_stop_count   = 0;
}

/* =========================================
   兼容层: 从 g_arex.settings.card_order 读取
   (新版使用 g_sys_config 的 card_order)
   ========================================= */
uint8_t g_arex_card_order(uint8_t pos)
{
    if (pos >= 6) return 0;
    return g_arex.settings.card_order[pos];
}
