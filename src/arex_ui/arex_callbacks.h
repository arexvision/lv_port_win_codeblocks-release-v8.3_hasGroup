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

#ifdef __cplusplus
}
#endif

#endif /* AREX_CALLBACKS_H */
