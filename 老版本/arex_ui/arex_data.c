#include "arex_data.h"

/* =========================================
   Gas table (static, matches HTML gasData)
   ========================================= */
const arex_gas_entry_t AREX_GAS_TABLE[AREX_GAS_COUNT] = {
    { "AIR",      56 },
    { "NX 32",    34 },
    { "TX 18/45", 68 },
    { "O2 100%",   6 },
};

/* =========================================
   Global state instance
   ========================================= */
arex_state_t g_arex;

/* =========================================
   Default / demo initialisation
   Mirrors the HTML's hardcoded demo values.
   ========================================= */
void arex_data_init(void)
{
    /* Dive */
    g_arex.dive.depth          = 45.2f;
    g_arex.dive.ndl            = 0;
    g_arex.dive.tts            = 24;
    g_arex.dive.next_stop_m    = 21;
    g_arex.dive.next_stop_min  = 3;
    g_arex.dive.dive_time_s    = 38 * 60 + 14;  /* 38:14 */
    g_arex.dive.pod1_bar       = 210;
    g_arex.dive.pod2_bar       = 195;

    /* Compass */
    g_arex.compass.heading     = 265.0f;
    g_arex.compass.marked      = false;
    g_arex.compass.target      = 0.0f;
    g_arex.compass.style       = 0;   /* classic */

    /* Deco / Tissues */
    static const uint8_t tissue_demo[16] = {
        95, 85, 75, 60, 50, 40, 35, 20,
        15, 10,  8,  5,  4,  3,  2,  1
    };
    for (int i = 0; i < 16; i++) {
        g_arex.deco.tissue_pct[i] = tissue_demo[i];
    }
    g_arex.deco.gf99    = 82;
    g_arex.deco.surf_gf = 145;   /* >100 → HTML .highlight-invert (green chip / black text) */
    g_arex.deco.cns_pct = 15;
    g_arex.deco.otu     = 22;

    /* Gas */
    g_arex.gas.active_idx = 2;   /* TX 18/45 */
    g_arex.gas.ppo2[0]    = 1.2f;
    g_arex.gas.ppo2[1]    = 1.2f;
    g_arex.gas.ppo2[2]    = 1.3f;

    /* Settings */
    g_arex.settings.mod_ppo2      = 1.4f;
    g_arex.settings.conservatism  = 1;   /* med */
    g_arex.settings.brightness    = 2;   /* high */
    /* Default card order: INFO, COMPASS, DECO, GAS, PLAN, SETUP */
    for (uint8_t i = 0; i < 6; i++) {
        g_arex.settings.card_order[i] = i;
    }
}
