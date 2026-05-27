#ifndef SUBMENU_MODEL_H
#define SUBMENU_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SUBMENU_INFO_COUNT   5
#define SUBMENU_SETUP_COUNT  6

typedef enum
{
    SUBMENU_SETTING_NONE = 0,
    SUBMENU_SETTING_SALINITY,
    SUBMENU_SETTING_SAFETY_STOP,
    SUBMENU_SETTING_LAST_DECO,
    SUBMENU_SETTING_ALTITUDE,
    SUBMENU_SETTING_DIVE_MODE,
    SUBMENU_SETTING_NITROX_O2,
    SUBMENU_SETTING_3GAS_O2,
    SUBMENU_SETTING_3GAS_COUNT,
    SUBMENU_SETTING_OC_TECH_GAS,
    SUBMENU_SETTING_OC_TECH_SAVE,
    SUBMENU_SETTING_AI_PAIR,
    SUBMENU_SETTING_AI_TANK_STATE,
    SUBMENU_SETTING_GTR_MODE,
    SUBMENU_SETTING_MOD_PPO2,
    SUBMENU_SETTING_DEPTH_ALARM,
    SUBMENU_SETTING_TIME_ALARM,
    SUBMENU_SETTING_NDL_ALARM,
    SUBMENU_SETTING_VIBRATION_TEST,
    SUBMENU_SETTING_UNITS,
    SUBMENU_SETTING_DATETIME_FIELD,
    SUBMENU_SETTING_DATETIME_ACTION,
    SUBMENU_SETTING_LOG_RATE,
    SUBMENU_SETTING_BLUETOOTH,
    SUBMENU_SETTING_RESET_DEFAULTS,
    SUBMENU_SETTING_PLAN_DEPTH,
    SUBMENU_SETTING_PLAN_TIME,
    SUBMENU_SETTING_PLAN_RMV,
} submenu_setting_kind_t;

typedef struct
{
    uint8_t value;
    const char *menu_label;
    const char *badge_label;
} setting_option_t;

typedef struct
{
    uint8_t value;
    const char *menu_label;
    const char *badge_label;
    uint8_t visible_opa;
} brightness_option_t;

typedef struct
{
    submenu_setting_kind_t kind;
    uint8_t arg;
    uint16_t value;
    char body[48];
} submenu_setting_confirm_t;

typedef struct
{
    submenu_setting_kind_t kind;
    uint8_t arg;
    float value;
    float min;
    float max;
    float step;
    uint8_t decimals;
    char label[20];
} submenu_edit_spec_t;

typedef enum
{
    DIVE_PLAN_PAGE_DEPTH = 0,
    DIVE_PLAN_PAGE_TIME,
    DIVE_PLAN_PAGE_RMV,
    DIVE_PLAN_PAGE_READY,
    DIVE_PLAN_PAGE_RESULT,
    DIVE_PLAN_PAGE_ERROR,
} dive_plan_page_t;

typedef enum
{
    DIVE_PLAN_ROW_BOTTOM = 0,
    DIVE_PLAN_ROW_ASCENT,
    DIVE_PLAN_ROW_DECO_STOP,
} dive_plan_row_type_t;

typedef struct
{
    dive_plan_row_type_t type;
    int16_t depth_m;
    uint16_t time_min;
    uint16_t run_min;
    uint8_t o2_pct;
    uint8_t he_pct;
    uint16_t gas_l;
} dive_plan_row_t;

const char *submenu_info_title(uint8_t index);
const char **submenu_build_info_items(uint8_t index, uint8_t *out_count);

const char *submenu_setup_title(uint8_t index);
int8_t submenu_setup_index_for_title(const char *title);
const char **submenu_build_setup_items(uint8_t index, uint8_t *out_count);
const setting_option_t *submenu_conservatism_option(uint8_t index);
const char *submenu_conservatism_badge(uint8_t level);
const brightness_option_t *submenu_brightness_option(uint8_t index);
const char *submenu_brightness_badge(uint8_t level);
uint8_t submenu_brightness_visible_opa(uint8_t level);
uint8_t submenu_safety_stop_mode(void);
uint8_t submenu_altitude_level(void);
uint8_t submenu_ai_tank_state(uint8_t tank_index);
bool submenu_gtr_enabled(void);
uint8_t submenu_units_mode(void);
uint8_t submenu_log_rate_s(void);
bool submenu_bluetooth_enabled(void);
uint8_t submenu_three_gas_count(void);
uint8_t submenu_nitrox_o2_pct(void);
uint8_t submenu_three_gas_o2_pct(uint8_t gas_index);
uint8_t submenu_oc_tech_draft_o2_pct(uint8_t slot);
uint8_t submenu_oc_tech_draft_he_pct(uint8_t slot);
uint8_t submenu_oc_tech_edit_slot(void);
uint16_t submenu_depth_alarm_m(void);
uint16_t submenu_time_alarm_min(void);
uint16_t submenu_datetime_year(void);
uint8_t submenu_datetime_month(void);
uint8_t submenu_datetime_day(void);
uint8_t submenu_datetime_hour(void);
uint8_t submenu_datetime_minute(void);

const char **submenu_build_compass_cal_items(uint8_t *out_count);
const char **submenu_nested_items_for(const char *title, uint8_t *out_count);
const char **submenu_child_items_for(const char *current_title,
                                          uint8_t item_index,
                                          const char *item_text,
                                          char *out_title,
                                          uint8_t out_title_size,
                                          uint8_t *out_count);

bool submenu_is_readonly_info_title(const char *title);
bool submenu_setting_from_selection(const char *current_title,
                                         uint8_t item_index,
                                         const char *item_text,
                                         submenu_setting_confirm_t *out_setting);
bool submenu_direct_setting_from_selection(const char *current_title,
                                                uint8_t item_index,
                                                const char *item_text,
                                                submenu_setting_confirm_t *out_setting);
bool submenu_edit_spec_from_selection(const char *current_title,
                                           uint8_t item_index,
                                           const char *item_text,
                                           submenu_edit_spec_t *out_spec);
void submenu_apply_setting(submenu_setting_kind_t kind, uint8_t arg, uint16_t value);
void submenu_apply_edit_value(submenu_setting_kind_t kind, uint8_t arg, float value);
uint8_t submenu_safety_stop_depth_m(uint8_t value);
dive_plan_page_t submenu_dive_plan_page(void);
void submenu_dive_plan_get_inputs(float *out_depth_m,
                                       uint16_t *out_time_min,
                                       float *out_rmv_lpm);
uint8_t submenu_dive_plan_gf_low(void);
uint8_t submenu_dive_plan_gf_high(void);
uint8_t submenu_dive_plan_last_stop_m(void);
uint8_t submenu_dive_plan_header_gas_o2(void);
void submenu_dive_plan_gas_summary(char *out, size_t out_size);
void submenu_dive_plan_reset(void);
bool submenu_dive_plan_handle_rotate(int8_t dir);
bool submenu_dive_plan_is_result_page(void);
uint8_t submenu_dive_plan_result_page_index(void);
uint8_t submenu_dive_plan_result_total_pages(void);
uint8_t submenu_dive_plan_result_entry_count(void);
bool submenu_dive_plan_result_row(uint8_t row_index, dive_plan_row_t *out_row);
uint16_t submenu_dive_plan_total_runtime_min(void);
uint16_t submenu_dive_plan_total_deco_min(void);
uint16_t submenu_dive_plan_total_gas_l(void);
uint16_t submenu_dive_plan_cns_pct(void);
uint16_t submenu_dive_plan_otu(void);
bool submenu_dive_plan_handle_action(bool exit_action,
                                          bool *out_close_submenu,
                                          uint8_t *out_keep_index);

#ifdef __cplusplus
}
#endif

#endif /* SUBMENU_MODEL_H */
