#ifndef AREX_DATA_H
#define AREX_DATA_H

#include <stdint.h>
#include <stdbool.h>

/* 气体数量 */
#define AREX_GAS_COUNT 4

/* arex_sensor_data_t 和 g_sensor_data 的声明在 arex_ui_engine.h 中 */

/* 气体表 (Name + MOD) */
extern const char  *AREX_GAS_NAMES[AREX_GAS_COUNT];
extern const uint8_t AREX_GAS_MOD_M[AREX_GAS_COUNT];

/* 从 g_arex.settings.card_order 读取 (供外部调用) */
extern uint8_t g_arex_card_order(uint8_t pos);

/* =========================================
   Module: Dive / Decompression
   ========================================= */
typedef struct {
    float    depth;
    int16_t  ndl;
    uint16_t tts;
    uint16_t next_stop_m;
    uint8_t  next_stop_min;
    uint32_t dive_time_s;
    uint16_t pod1_bar;
    uint16_t pod2_bar;
} dive_data_t;

/* =========================================
   Module: Compass
   ========================================= */
typedef struct {
    float   heading;
    bool    marked;
    float   target;
    uint8_t style;
} compass_data_t;

/* =========================================
   Module: Deco / Tissues
   ========================================= */
typedef struct {
    uint8_t  tissue_pct[16];
    uint8_t  gf99;
    uint8_t  surf_gf;
    uint8_t  cns_pct;
    uint16_t otu;
} deco_data_t;

/* =========================================
   Module: Gas
   ========================================= */
typedef struct {
    uint8_t active_idx;
    float   ppo2[3];
} gas_data_t;

/* =========================================
   Module: Settings (persisted)
   ========================================= */
typedef struct {
    float   mod_ppo2;
    uint8_t conservatism;
    uint8_t brightness;
    uint8_t card_order[6];
} settings_data_t;

/* =========================================
   Legacy top-level aggregate (旧版兼容)
   ========================================= */
typedef struct {
    dive_data_t     dive;
    compass_data_t  compass;
    deco_data_t     deco;
    gas_data_t      gas;
    settings_data_t settings;
} arex_state_t;

extern arex_state_t g_arex;

void arex_data_init(void);

#endif /* AREX_DATA_H */
