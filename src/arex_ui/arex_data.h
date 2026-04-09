#ifndef AREX_DATA_H
#define AREX_DATA_H

#include <stdint.h>
#include <stdbool.h>

/* =========================================
   Gas table entry
   ========================================= */
typedef struct {
    const char *name;
    uint8_t     mod_m;   /* Max Operating Depth in meters */
} arex_gas_entry_t;

#define AREX_GAS_COUNT 4
extern const arex_gas_entry_t AREX_GAS_TABLE[AREX_GAS_COUNT];

/* =========================================
   Module: Dive / Decompression
   ========================================= */
typedef struct {
    float    depth;           /* meters, e.g. 45.2 */
    int16_t  ndl;             /* No-Deco Limit in min; negative = deco */
    uint16_t tts;             /* Time To Surface in min */
    uint16_t next_stop_m;     /* meters */
    uint8_t  next_stop_min;   /* minutes at next stop */
    uint32_t dive_time_s;     /* total dive time in seconds */
    uint16_t pod1_bar;
    uint16_t pod2_bar;
} dive_data_t;

/* =========================================
   Module: Compass
   ========================================= */
typedef struct {
    float   heading;          /* 0.0 ~ 359.9 degrees */
    bool    marked;           /* target heading locked */
    float   target;           /* locked target heading */
    uint8_t style;            /* 0=classic  1=aero  2=sub */
} compass_data_t;

/* =========================================
   Module: Deco / Tissues
   ========================================= */
typedef struct {
    uint8_t  tissue_pct[16];  /* saturation 0~100 (>100 = over M-value) */
    uint8_t  gf99;            /* GF99 % */
    uint8_t  surf_gf;         /* Surface GF % (>100 triggers flash) */
    uint8_t  cns_pct;         /* CNS oxygen % */
    uint16_t otu;             /* Oxygen Tolerance Units */
} deco_data_t;

/* =========================================
   Module: Gas
   ========================================= */
typedef struct {
    uint8_t active_idx;       /* index into AREX_GAS_TABLE */
    float   ppo2[3];          /* current / ceil / floor ppo2 */
} gas_data_t;

/* =========================================
   Module: Settings (persisted)
   ========================================= */
typedef struct {
    float   mod_ppo2;         /* 1.0 ~ 1.6 */
    uint8_t conservatism;     /* 0=low  1=med  2=high */
    uint8_t brightness;       /* 0=low  1=med  2=high  3=max */
    uint8_t card_order[6];    /* index into card_registry, user-sortable */
} settings_data_t;

/* =========================================
   Top-level aggregate state
   ========================================= */
typedef struct {
    dive_data_t     dive;
    compass_data_t  compass;
    deco_data_t     deco;
    gas_data_t      gas;
    settings_data_t settings;
} arex_state_t;

extern arex_state_t g_arex;

/* Init with demo / default values */
void arex_data_init(void);

#endif /* AREX_DATA_H */
