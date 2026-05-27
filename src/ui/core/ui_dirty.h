#ifndef UI_DIRTY_H
#define UI_DIRTY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DIRTY_NONE       = 0,
    DIRTY_DEPTH      = (1U << 0),
    DIRTY_NDL        = (1U << 1),
    DIRTY_NDL_STOP   = (1U << 2),
    DIRTY_TTS        = (1U << 3),
    DIRTY_DIVE_TIME  = (1U << 4),
    DIRTY_GAS        = (1U << 5),
    DIRTY_TEMP       = (1U << 6),
    DIRTY_BATT       = (1U << 7),
    DIRTY_TIME_DAY   = (1U << 8),
    DIRTY_ASCENT     = (1U << 9),
    DIRTY_HEADING    = (1U << 10),
    DIRTY_PPO2       = (1U << 11),
    DIRTY_STOP_DEPTH = (1U << 12),
    DIRTY_STOP_TIME  = (1U << 13),
    DIRTY_POD        = (1U << 14),
    DIRTY_SAC        = (1U << 15),
    DIRTY_DEPTH_STATS = (1U << 16),
    DIRTY_TEMP_STATS = (1U << 17),
    DIRTY_SURF_GF    = (1U << 18),
    DIRTY_GF99       = (1U << 19),
    DIRTY_GF_SETTING = (1U << 20),
    DIRTY_MOD        = (1U << 21),
    DIRTY_CEILING    = (1U << 22),
    DIRTY_GAS_MIX    = (1U << 23),
    DIRTY_GAS_DENS   = (1U << 24),
    DIRTY_FIO2       = (1U << 25),
    DIRTY_TISSUES    = (1U << 26),
    DIRTY_TRAJECTORY = (1U << 27),
    DIRTY_CNS        = (1U << 28),
    DIRTY_OTU        = (1U << 29),
    DIRTY_ALARM      = (1U << 30),
    DIRTY_UI_LAYOUT  = (1U << 31),
} dirty_bit_t;

#ifdef __cplusplus
}
#endif

#endif /* UI_DIRTY_H */
