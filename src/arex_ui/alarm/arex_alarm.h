#ifndef AREX_ALARM_H
#define AREX_ALARM_H

#include "../core/arex_ui_engine.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AREX_ALARM_TARGET_MAX  12

typedef enum
{
    AREX_ALARM_ID_CRIT_ASCENT_RATE = 0,
    AREX_ALARM_ID_CRIT_PO2_MAX,
    AREX_ALARM_ID_CRIT_CEIL_BROKEN,
    AREX_ALARM_ID_CRIT_ALGO_LOCK,
    AREX_ALARM_ID_CRIT_TANK_EMPTY,
    AREX_ALARM_ID_CRIT_BATTERY_DEAD,

    AREX_ALARM_ID_WARN_PO2_ELEVATED,
    AREX_ALARM_ID_WARN_NDL_LOW,
    AREX_ALARM_ID_WARN_CNS_HIGH,
    AREX_ALARM_ID_WARN_OTU_HIGH,
    AREX_ALARM_ID_WARN_SAFETY_BROKEN,
    AREX_ALARM_ID_WARN_TANK_TURN,
    AREX_ALARM_ID_WARN_SIDEMOUNT_DIFF,
    AREX_ALARM_ID_WARN_DEPTH_LIMIT,
    AREX_ALARM_ID_WARN_TIME_LIMIT,
    AREX_ALARM_ID_WARN_BATTERY_LOW,
    AREX_ALARM_ID_WARN_POD_LOST,

    AREX_ALARM_ID_INFO_SAFETY_STOP,
    AREX_ALARM_ID_INFO_GAS_SWITCH,
    AREX_ALARM_ID_INFO_STOP_DONE,
    AREX_ALARM_ID_INFO_COMPASS_CALI,

    AREX_ALARM_ID_COUNT
} arex_alarm_id_t;

typedef struct
{
    bool visible;
    arex_alarm_level_t level;
    const char *text;
    comp_id_t banner_target;
    uint32_t revision;
} arex_alarm_display_t;

void arex_alarm_init(void);
bool arex_alarm_set_active(arex_alarm_id_t id, bool active);
bool arex_alarm_raise_custom(arex_alarm_level_t level,
                             const char *text,
                             comp_id_t target);
void arex_alarm_clear_all(void);
bool arex_alarm_ack_current(void);
void arex_alarm_tick(uint32_t now_ms);

const arex_alarm_display_t *arex_alarm_get_display(void);
uint8_t arex_alarm_get_active_targets(arex_alarm_level_t level,
                                      comp_id_t *targets,
                                      uint8_t max_targets);

#ifdef __cplusplus
}
#endif

#endif /* AREX_ALARM_H */
