#ifndef ALARM_H
#define ALARM_H

#include "../core/ui_engine.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALARM_TARGET_MAX  12

typedef enum
{
    ALARM_ID_CRIT_ASCENT_RATE = 0,
    ALARM_ID_CRIT_PO2_MAX,
    ALARM_ID_CRIT_CEIL_BROKEN,
    ALARM_ID_CRIT_ALGO_LOCK,
    ALARM_ID_CRIT_TANK_EMPTY,
    ALARM_ID_CRIT_BATTERY_DEAD,

    ALARM_ID_WARN_PO2_ELEVATED,
    ALARM_ID_WARN_NDL_LOW,
    ALARM_ID_WARN_CNS_HIGH,
    ALARM_ID_WARN_OTU_HIGH,
    ALARM_ID_WARN_SAFETY_BROKEN,
    ALARM_ID_WARN_TANK_TURN,
    ALARM_ID_WARN_SIDEMOUNT_DIFF,
    ALARM_ID_WARN_DEPTH_LIMIT,
    ALARM_ID_WARN_TIME_LIMIT,
    ALARM_ID_WARN_BATTERY_LOW,
    ALARM_ID_WARN_POD_LOST,

    ALARM_ID_INFO_SAFETY_STOP,
    ALARM_ID_INFO_GAS_SWITCH,
    ALARM_ID_INFO_STOP_DONE,
    ALARM_ID_INFO_COMPASS_CALI,

    ALARM_ID_COUNT
} alarm_id_t;

typedef struct
{
    bool visible;
    alarm_level_t level;
    const char *text;
    comp_id_t banner_target;
    uint32_t revision;
} alarm_display_t;

void alarm_init(void);
bool alarm_set_active(alarm_id_t id, bool active);
bool alarm_raise_custom(alarm_level_t level,
                             const char *text,
                             comp_id_t target);
void alarm_clear_all(void);
bool alarm_ack_current(void);
void alarm_tick(uint32_t now_ms);

const alarm_display_t *alarm_get_display(void);
uint8_t alarm_get_active_targets(alarm_level_t level,
                                      comp_id_t *targets,
                                      uint8_t max_targets);

#ifdef __cplusplus
}
#endif

#endif /* ALARM_H */
