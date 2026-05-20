#ifndef AREX_CALLBACKS_H
#define AREX_CALLBACKS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Business-layer callbacks used by setup/light-control UI. */
extern bool g_light_power_state;
void arex_bus_set_light_power(bool on);
void arex_ui_on_light_color_set(const char *color, const char *level);
void arex_set_software_brightness_enabled(bool enabled);
void arex_apply_software_brightness(uint8_t level);
void arex_set_brightness(uint8_t level);
void arex_ui_on_conservatism_set(uint8_t level);
void arex_ui_on_salinity_set(uint8_t mode);
void arex_ui_on_safety_stop_depth_set(uint8_t depth_m);
void arex_ui_on_altitude_range_set(uint8_t level);
void arex_ui_on_dive_mode_set(uint8_t mode);
void arex_ui_on_ai_pair(uint8_t tank_index);
void arex_ui_on_gtr_mode_set(bool enabled);
void arex_ui_on_depth_alarm_set(uint16_t depth_m);
void arex_ui_on_time_alarm_set(uint16_t minutes);
void arex_ui_on_ndl_alarm_set(uint16_t minutes);
void arex_ui_on_vibration_test(void);
void arex_ui_on_units_set(uint8_t units);
void arex_ui_on_datetime_field_set(uint8_t field, uint16_t value);
void arex_ui_on_datetime_action(uint8_t action);
void arex_ui_on_log_rate_set(uint8_t seconds);
void arex_ui_on_bluetooth_set(bool enabled);
void arex_ui_on_reset_defaults(void);

#ifdef __cplusplus
}
#endif

#endif /* AREX_CALLBACKS_H */
