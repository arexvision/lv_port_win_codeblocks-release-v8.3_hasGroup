#ifndef AREX_SUBMENU_MODEL_H
#define AREX_SUBMENU_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AREX_SUBMENU_INFO_COUNT   5
#define AREX_SUBMENU_SETUP_COUNT  6

typedef enum
{
    AREX_SUBMENU_SETTING_NONE = 0,
    AREX_SUBMENU_SETTING_SALINITY,
    AREX_SUBMENU_SETTING_SAFETY_STOP,
    AREX_SUBMENU_SETTING_LAST_DECO,
    AREX_SUBMENU_SETTING_ALTITUDE,
    AREX_SUBMENU_SETTING_DIVE_MODE,
    AREX_SUBMENU_SETTING_NITROX_O2,
    AREX_SUBMENU_SETTING_3GAS_O2,
    AREX_SUBMENU_SETTING_3GAS_COUNT,
    AREX_SUBMENU_SETTING_OC_TECH_GAS,
    AREX_SUBMENU_SETTING_OC_TECH_SAVE,
    AREX_SUBMENU_SETTING_AI_PAIR,
    AREX_SUBMENU_SETTING_AI_TANK_STATE,
    AREX_SUBMENU_SETTING_GTR_MODE,
    AREX_SUBMENU_SETTING_MOD_PPO2,
    AREX_SUBMENU_SETTING_DEPTH_ALARM,
    AREX_SUBMENU_SETTING_TIME_ALARM,
    AREX_SUBMENU_SETTING_NDL_ALARM,
    AREX_SUBMENU_SETTING_VIBRATION_TEST,
    AREX_SUBMENU_SETTING_UNITS,
    AREX_SUBMENU_SETTING_DATETIME_FIELD,
    AREX_SUBMENU_SETTING_DATETIME_ACTION,
    AREX_SUBMENU_SETTING_LOG_RATE,
    AREX_SUBMENU_SETTING_BLUETOOTH,
    AREX_SUBMENU_SETTING_RESET_DEFAULTS,
    AREX_SUBMENU_SETTING_PLAN_DEPTH,
    AREX_SUBMENU_SETTING_PLAN_TIME,
    AREX_SUBMENU_SETTING_PLAN_RMV,
} arex_submenu_setting_kind_t;

typedef struct
{
    arex_submenu_setting_kind_t kind;
    uint8_t arg;
    uint16_t value;
    char body[48];
} arex_submenu_setting_confirm_t;

typedef struct
{
    arex_submenu_setting_kind_t kind;
    uint8_t arg;
    float value;
    float min;
    float max;
    float step;
    uint8_t decimals;
    char label[20];
} arex_submenu_edit_spec_t;

typedef enum
{
    AREX_DIVE_PLAN_PAGE_DEPTH = 0,
    AREX_DIVE_PLAN_PAGE_TIME,
    AREX_DIVE_PLAN_PAGE_RMV,
    AREX_DIVE_PLAN_PAGE_READY,
    AREX_DIVE_PLAN_PAGE_RESULT,
    AREX_DIVE_PLAN_PAGE_ERROR,
} arex_dive_plan_page_t;

typedef enum
{
    AREX_DIVE_PLAN_ROW_BOTTOM = 0,
    AREX_DIVE_PLAN_ROW_ASCENT,
    AREX_DIVE_PLAN_ROW_DECO_STOP,
} arex_dive_plan_row_type_t;

typedef struct
{
    arex_dive_plan_row_type_t type;
    int16_t depth_m;
    uint16_t time_min;
    uint16_t run_min;
    uint8_t o2_pct;
    uint8_t he_pct;
    uint16_t gas_l;
} arex_dive_plan_row_t;

const char *arex_submenu_info_title(uint8_t index);
const char **arex_submenu_build_info_items(uint8_t index, uint8_t *out_count);

const char *arex_submenu_setup_title(uint8_t index);
int8_t arex_submenu_setup_index_for_title(const char *title);
const char **arex_submenu_build_setup_items(uint8_t index, uint8_t *out_count);

const char **arex_submenu_build_compass_cal_items(uint8_t *out_count);
const char **arex_submenu_nested_items_for(const char *title, uint8_t *out_count);
const char **arex_submenu_child_items_for(const char *current_title,
                                          uint8_t item_index,
                                          const char *item_text,
                                          char *out_title,
                                          uint8_t out_title_size,
                                          uint8_t *out_count);

bool arex_submenu_is_readonly_info_title(const char *title);
bool arex_submenu_setting_from_selection(const char *current_title,
                                         uint8_t item_index,
                                         const char *item_text,
                                         arex_submenu_setting_confirm_t *out_setting);
bool arex_submenu_direct_setting_from_selection(const char *current_title,
                                                uint8_t item_index,
                                                const char *item_text,
                                                arex_submenu_setting_confirm_t *out_setting);
bool arex_submenu_edit_spec_from_selection(const char *current_title,
                                           uint8_t item_index,
                                           const char *item_text,
                                           arex_submenu_edit_spec_t *out_spec);
void arex_submenu_apply_setting(arex_submenu_setting_kind_t kind, uint8_t arg, uint16_t value);
void arex_submenu_apply_edit_value(arex_submenu_setting_kind_t kind, uint8_t arg, float value);
uint8_t arex_submenu_safety_stop_depth_m(uint8_t value);
arex_dive_plan_page_t arex_submenu_dive_plan_page(void);
void arex_submenu_dive_plan_get_inputs(float *out_depth_m,
                                       uint16_t *out_time_min,
                                       float *out_rmv_lpm);
uint8_t arex_submenu_dive_plan_gf_low(void);
uint8_t arex_submenu_dive_plan_gf_high(void);
uint8_t arex_submenu_dive_plan_last_stop_m(void);
uint8_t arex_submenu_dive_plan_header_gas_o2(void);
void arex_submenu_dive_plan_gas_summary(char *out, size_t out_size);
bool arex_submenu_dive_plan_handle_rotate(int8_t dir);
bool arex_submenu_dive_plan_is_result_page(void);
uint8_t arex_submenu_dive_plan_result_page_index(void);
uint8_t arex_submenu_dive_plan_result_total_pages(void);
uint8_t arex_submenu_dive_plan_result_entry_count(void);
bool arex_submenu_dive_plan_result_row(uint8_t row_index, arex_dive_plan_row_t *out_row);
uint16_t arex_submenu_dive_plan_total_runtime_min(void);
uint16_t arex_submenu_dive_plan_total_deco_min(void);
uint16_t arex_submenu_dive_plan_total_gas_l(void);
uint16_t arex_submenu_dive_plan_cns_pct(void);
uint16_t arex_submenu_dive_plan_otu(void);
bool arex_submenu_dive_plan_handle_action(uint8_t item_index,
                                          const char *item_text,
                                          bool *out_close_submenu,
                                          uint8_t *out_keep_index);

#ifdef __cplusplus
}
#endif

#endif /* AREX_SUBMENU_MODEL_H */
