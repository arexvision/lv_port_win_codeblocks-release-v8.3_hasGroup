/*
 * 文件: src/app_ui/ui/core/callbacks.h
 * 作用: 该文件属于 UI 核心模块，负责状态机、数据桥接、事件分发、更新调度或 UI 运行时公共定义。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    LIGHT_MODE_ALWAYS = 0,
    LIGHT_MODE_BREATH,
} light_mode_t;

/* 这一层把 UI 操作翻译成业务动作，供菜单和设置页面统一调用。 */
extern bool g_light_power_state;
extern light_mode_t g_light_mode_state;
void bus_set_light_power(bool on);
bool bus_get_light_power(void);
void bus_toggle_light_power(void);
void bus_set_light_mode(light_mode_t mode);
light_mode_t bus_get_light_mode(void);
void bus_toggle_light_mode(void);
void ui_on_light_color_set(const char *color, const char *level);
void set_software_brightness_enabled(bool enabled);
void apply_software_brightness(uint8_t level);
void set_brightness(uint8_t level);
void ui_on_conservatism_set(uint8_t level);
void ui_on_salinity_set(uint8_t mode);
void ui_on_safety_stop_depth_set(uint8_t depth_m);
void ui_on_safety_stop_time_set(uint8_t minutes);
void ui_on_last_deco_stop_set(uint8_t depth_m);
void ui_on_altitude_range_set(uint8_t level);
void ui_on_dive_mode_set(uint8_t mode);
void ui_on_gas_profile_commit(void);
void ui_on_ai_pair(uint8_t tank_index);
void ui_on_ai_tank_state_set(uint8_t tank_index, uint8_t state);
void ui_on_gtr_mode_set(bool enabled);
void ui_on_mod_ppo2_set(float ppo2);
void ui_on_depth_alarm_set(uint16_t depth_m);
void ui_on_time_alarm_set(uint16_t minutes);
void ui_on_ndl_alarm_set(uint16_t minutes);
void ui_on_vibration_test(void);
void ui_on_units_set(uint8_t units);
void ui_on_datetime_field_set(uint8_t field, uint16_t value);
void ui_on_datetime_action(uint8_t action);
void ui_on_log_rate_set(uint8_t seconds);
void ui_on_bluetooth_set(bool enabled);
void ui_on_reset_defaults(void);

#ifdef __cplusplus
}
#endif

#endif /* CALLBACKS_H */
