#include "menu_runtime.h"

#include "../core/data.h"
#include "../core/ui_state.h"
#include "submenu_model.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <string.h>

typedef struct
{
    menu_id_t menu_id;
    uint8_t selected_idx;
} menu_stack_entry_t;

/* 当前菜单运行时状态。
 * s_rows 是 view 当前要画的行；每一行都有稳定 id，label 只是显示文字。
 * s_stack 用来返回父菜单，并恢复进入子菜单前的选中行。
 */
static menu_id_t s_current_menu = MENU_NONE;
static menu_row_t s_rows[MENU_MAX_ROWS];
static uint8_t s_row_count = 0;
static menu_stack_entry_t s_stack[SUB_HISTORY_MAX];
static uint8_t s_stack_depth = 0;
static char s_oc_tech_edit_title[24];

static void rows_clear(void)
{
    memset(s_rows, 0, sizeof(s_rows));
    s_row_count = 0;
}

static void row_add(menu_item_id_t id, menu_row_type_t type, const char *label)
{
    if (s_row_count >= MENU_MAX_ROWS)
    {
        return;
    }
    s_rows[s_row_count].id = id;
    s_rows[s_row_count].type = type;
    s_rows[s_row_count].label = label;
    s_rows[s_row_count].badge = NULL;
    s_row_count++;
}

/* 过渡期很多 label 仍由 submenu_model 动态生成。
 * 这里把旧 label 数组包一层稳定 id/type，后续 view/action 就不再关心文字。
 */
static void rows_from_labels(const char **labels,
                             uint8_t count,
                             const menu_item_id_t *ids,
                             const menu_row_type_t *types)
{
    rows_clear();
    for (uint8_t i = 0; i < count && i < MENU_MAX_ROWS; i++)
    {
        row_add(ids ? ids[i] : MENU_ITEM_READONLY,
                types ? types[i] : MENU_ROW_NORMAL,
                labels[i]);
    }
}

static void build_info_rows(uint8_t index)
{
    uint8_t count = 0;
    const char **labels = submenu_build_info_items(index, &count);
    rows_clear();
    if (!labels)
    {
        return;
    }
    for (uint8_t i = 0; i < count && i < MENU_MAX_ROWS; i++)
    {
        menu_item_id_t id = MENU_ITEM_READONLY;
        menu_row_type_t type = MENU_ROW_READONLY;
        /* INFO 普通详情页只读；DIVE PLAN 例外，它有 EXIT/NEXT 等可操作行。 */
        if (s_current_menu == MENU_INFO_DIVE_PLAN)
        {
            if (i == 0U)
            {
                id = MENU_ITEM_DIVE_PLAN_EXIT;
            }
            else if (i == 1U)
            {
                id = MENU_ITEM_DIVE_PLAN_NEXT;
            }
            else if (i == 2U)
            {
                id = MENU_ITEM_DIVE_PLAN_MORE;
            }
            else
            {
                id = MENU_ITEM_DIVE_PLAN_PLAN;
            }
            type = MENU_ROW_DIVE_PLAN;
        }
        row_add(id, type, labels[i]);
    }
}

static void build_setup_gas_switch(void)
{
    uint8_t count = 0;
    const char **labels = submenu_build_setup_items(0, &count);
    static const menu_item_id_t ids[] =
    {
        MENU_ITEM_GAS_SLOT_0,
        MENU_ITEM_GAS_SLOT_1,
        MENU_ITEM_GAS_SLOT_2,
        MENU_ITEM_GAS_SLOT_3,
        MENU_ITEM_GAS_SLOT_4,
    };
    rows_from_labels(labels, count, ids, NULL);
}

static void build_setup_conservatism(void)
{
    uint8_t count = 0;
    const char **labels = submenu_build_setup_items(1, &count);
    static const menu_item_id_t ids[] =
    {
        MENU_ITEM_CONSERVATISM_LOW,
        MENU_ITEM_CONSERVATISM_MED,
        MENU_ITEM_CONSERVATISM_HIGH,
        MENU_ITEM_CONSERVATISM_CUSTOM,
    };
    rows_from_labels(labels, count, ids, NULL);
}

static void build_setup_brightness(void)
{
    uint8_t count = 0;
    const char **labels = submenu_build_setup_items(2, &count);
    static const menu_item_id_t ids[] =
    {
        MENU_ITEM_BRIGHTNESS_ECO,
        MENU_ITEM_BRIGHTNESS_MED,
        MENU_ITEM_BRIGHTNESS_HIGH,
        MENU_ITEM_BRIGHTNESS_MAX,
        MENU_ITEM_BRIGHTNESS_SUN,
    };
    rows_from_labels(labels, count, ids, NULL);
}

static void build_setup_compass(void)
{
    uint8_t count = 0;
    const char **labels = submenu_build_compass_cal_items(&count);
    static const menu_item_id_t ids[] =
    {
        MENU_ITEM_COMPASS_CAL_START,
        MENU_ITEM_COMPASS_CAL_RESET,
    };
    rows_from_labels(labels, count, ids, NULL);
}

static void build_setup_light(void)
{
    uint8_t count = 0;
    const char **labels = submenu_build_setup_items(4, &count);
    static const menu_item_id_t ids[] =
    {
        MENU_ITEM_LIGHT_POWER,
        MENU_ITEM_LIGHT_RED,
        MENU_ITEM_LIGHT_GREEN,
        MENU_ITEM_LIGHT_BLUE,
        MENU_ITEM_LIGHT_WHITE,
    };
    static const menu_row_type_t types[] =
    {
        MENU_ROW_LIGHT_POWER,
        MENU_ROW_NORMAL,
        MENU_ROW_NORMAL,
        MENU_ROW_NORMAL,
        MENU_ROW_NORMAL,
    };
    rows_from_labels(labels, count, ids, types);
}

static void build_setup_systems(void)
{
    uint8_t count = 0;
    const char **labels = submenu_build_setup_items(5, &count);
    static const menu_item_id_t ids[] =
    {
        MENU_ITEM_SYSTEM_VERSION,
        MENU_ITEM_SYSTEM_MODE_SETUP,
        MENU_ITEM_SYSTEM_DIVE_SETUP,
        MENU_ITEM_SYSTEM_AI_SETUP,
        MENU_ITEM_SYSTEM_ALERTS_SETUP,
        MENU_ITEM_SYSTEM_DISPLAY,
    };
    static const menu_row_type_t types[] =
    {
        MENU_ROW_READONLY,
        MENU_ROW_NORMAL,
        MENU_ROW_NORMAL,
        MENU_ROW_NORMAL,
        MENU_ROW_NORMAL,
        MENU_ROW_NORMAL,
    };
    rows_from_labels(labels, count, ids, types);
}

static void build_nested_by_title(const char *title,
                                  const menu_item_id_t *ids,
                                  const menu_row_type_t *types)
{
    uint8_t count = 0;
    const char **labels = submenu_nested_items_for(title, &count);
    rows_from_labels(labels, count, ids, types);
}

static void build_light_color(void)
{
    static const menu_item_id_t ids[] =
    {
        MENU_ITEM_LIGHT_LEVEL_10,
        MENU_ITEM_LIGHT_LEVEL_30,
        MENU_ITEM_LIGHT_LEVEL_50,
        MENU_ITEM_LIGHT_LEVEL_70,
        MENU_ITEM_LIGHT_LEVEL_100,
    };
    build_nested_by_title(menu_defs_title(s_current_menu), ids, NULL);
}

static void build_rows(void)
{
    /* 根据当前 menu_id_t 生成当前 rows。
     * 这一层是“菜单页面 -> 行 ID 列表”的映射，不做点击后的业务动作。
     */
    switch (s_current_menu)
    {
    case MENU_INFO_LAST_DIVE:
        build_info_rows(0);
        break;
    case MENU_INFO_DIVE_PLAN:
        build_info_rows(1);
        break;
    case MENU_INFO_TISSUE_TOX:
        build_info_rows(2);
        break;
    case MENU_INFO_GAS_CALC:
        build_info_rows(3);
        break;
    case MENU_INFO_SENSOR_DEVICE:
        build_info_rows(4);
        break;
    case MENU_SETUP_GAS_SWITCH:
        build_setup_gas_switch();
        break;
    case MENU_SETUP_CONSERVATISM:
        build_setup_conservatism();
        break;
    case MENU_SETUP_BRIGHTNESS:
        build_setup_brightness();
        break;
    case MENU_SETUP_COMPASS_CAL:
        build_setup_compass();
        break;
    case MENU_SETUP_LIGHT_CONTROL:
        build_setup_light();
        break;
    case MENU_SETUP_SYSTEMS:
        build_setup_systems();
        break;
    case MENU_MODE_SETUP:
    {
        static const menu_item_id_t ids[] =
        {
            MENU_ITEM_MODE_AIR,
            MENU_ITEM_MODE_NITROX,
            MENU_ITEM_MODE_THREE_GAS,
            MENU_ITEM_MODE_OC_TECH,
        };
        build_nested_by_title("MODE SETUP", ids, NULL);
        break;
    }
    case MENU_NITROX:
    {
        static const menu_item_id_t ids[] =
        {
            MENU_ITEM_NITROX_O2,
            MENU_ITEM_NITROX_CONFIRM,
        };
        build_nested_by_title("NITROX", ids, NULL);
        break;
    }
    case MENU_THREE_GAS:
    {
        static const menu_item_id_t ids[] =
        {
            MENU_ITEM_THREE_GAS_O2_0,
            MENU_ITEM_THREE_GAS_O2_1,
            MENU_ITEM_THREE_GAS_O2_2,
            MENU_ITEM_THREE_GAS_COUNT,
            MENU_ITEM_THREE_GAS_CONFIRM,
        };
        build_nested_by_title("3 GAS", ids, NULL);
        break;
    }
    case MENU_OC_TECH:
    {
        static const menu_item_id_t ids[] =
        {
            MENU_ITEM_OC_TECH_SLOT_0,
            MENU_ITEM_OC_TECH_SLOT_1,
            MENU_ITEM_OC_TECH_SLOT_2,
            MENU_ITEM_OC_TECH_SLOT_3,
            MENU_ITEM_OC_TECH_SLOT_4,
            MENU_ITEM_OC_TECH_CONFIRM,
        };
        build_nested_by_title("OC Tech", ids, NULL);
        break;
    }
    case MENU_OC_TECH_EDIT:
    {
        static const menu_item_id_t ids[] =
        {
            MENU_ITEM_OC_TECH_EDIT_O2,
            MENU_ITEM_OC_TECH_EDIT_HE,
            MENU_ITEM_OC_TECH_EDIT_SAVE,
            MENU_ITEM_BACK,
        };
        build_nested_by_title(s_oc_tech_edit_title, ids, NULL);
        break;
    }
    case MENU_DIVE_SETUP:
    {
        static const menu_item_id_t ids[] =
        {
            MENU_ITEM_DIVE_SALINITY,
            MENU_ITEM_DIVE_MOD_PPO2,
            MENU_ITEM_DIVE_SAFETY_STOP,
            MENU_ITEM_DIVE_LAST_DECO,
            MENU_ITEM_DIVE_ALTITUDE,
        };
        build_nested_by_title("DIVE SETUP", ids, NULL);
        break;
    }
    case MENU_AI_SETUP:
    {
        static const menu_item_id_t ids[] =
        {
            MENU_ITEM_AI_TANK_0,
            MENU_ITEM_AI_TANK_1,
            MENU_ITEM_AI_GTR,
        };
        build_nested_by_title("AI SETUP", ids, NULL);
        break;
    }
    case MENU_ALERTS_SETUP:
    {
        static const menu_item_id_t ids[] =
        {
            MENU_ITEM_ALERT_DEPTH,
            MENU_ITEM_ALERT_TIME,
            MENU_ITEM_ALERT_NDL,
        };
        build_nested_by_title("ALERTS SETUP", ids, NULL);
        break;
    }
    case MENU_DISPLAY:
    {
        static const menu_item_id_t ids[] =
        {
            MENU_ITEM_DISPLAY_UNITS,
            MENU_ITEM_DISPLAY_DATE_CLOCK,
            MENU_ITEM_DISPLAY_LOG_RATE,
            MENU_ITEM_DISPLAY_BLUETOOTH,
            MENU_ITEM_DISPLAY_RESET,
        };
        build_nested_by_title("DISPLAY", ids, NULL);
        break;
    }
    case MENU_DATE_CLOCK:
    {
        static const menu_item_id_t ids[] =
        {
            MENU_ITEM_DATE_YEAR,
            MENU_ITEM_DATE_MONTH,
            MENU_ITEM_DATE_DAY,
            MENU_ITEM_DATE_HOUR,
            MENU_ITEM_DATE_MINUTE,
        };
        build_nested_by_title("DATE & CLOCK", ids, NULL);
        break;
    }
    case MENU_LIGHT_RED:
    case MENU_LIGHT_GREEN:
    case MENU_LIGHT_BLUE:
    case MENU_LIGHT_WHITE:
        build_light_color();
        break;
    default:
        rows_clear();
        break;
    }
}

static void push_current(menu_item_id_t source_item)
{
    (void)source_item;
    if (s_stack_depth >= SUB_HISTORY_MAX)
    {
        return;
    }
    s_stack[s_stack_depth].menu_id = s_current_menu;
    s_stack[s_stack_depth].selected_idx = ui_state_get_sub_menu_idx();
    s_stack_depth++;
}

void menu_runtime_reset(void)
{
    s_current_menu = MENU_NONE;
    s_stack_depth = 0;
    s_oc_tech_edit_title[0] = '\0';
    rows_clear();
}

bool menu_runtime_open_info(uint8_t index)
{
    s_current_menu = menu_defs_info_menu_for_index(index);
    s_stack_depth = 0;
    build_rows();
    return s_current_menu != MENU_NONE && s_row_count > 0U;
}

bool menu_runtime_open_setup(uint8_t index)
{
    s_current_menu = menu_defs_setup_menu_for_index(index);
    s_stack_depth = 0;
    build_rows();
    return s_current_menu != MENU_NONE && s_row_count > 0U;
}

bool menu_runtime_open_child(menu_id_t child_id, menu_item_id_t source_item)
{
    if (child_id == MENU_NONE)
    {
        return false;
    }
    if (child_id == MENU_OC_TECH_EDIT)
    {
        submenu_prepare_oc_tech_child(source_item,
                                      s_oc_tech_edit_title,
                                      sizeof(s_oc_tech_edit_title));
    }
    push_current(source_item);
    s_current_menu = child_id;
    build_rows();
    return s_row_count > 0U;
}

bool menu_runtime_back(void)
{
    if (s_stack_depth == 0U)
    {
        return false;
    }
    s_stack_depth--;
    s_current_menu = s_stack[s_stack_depth].menu_id;
    build_rows();
    ui_state_set_sub_menu_idx(s_stack[s_stack_depth].selected_idx);
    return true;
}

void menu_runtime_refresh(void)
{
    build_rows();
}

menu_id_t menu_runtime_current_id(void)
{
    return s_current_menu;
}

const char *menu_runtime_current_title(void)
{
    if (s_current_menu == MENU_OC_TECH_EDIT && s_oc_tech_edit_title[0] != '\0')
    {
        return s_oc_tech_edit_title;
    }
    return menu_defs_title(s_current_menu);
}

const menu_row_t *menu_runtime_current_rows(uint8_t *out_count)
{
    if (out_count)
    {
        *out_count = s_row_count;
    }
    return s_rows;
}

const menu_row_t *menu_runtime_row_at(uint8_t index)
{
    return (index < s_row_count) ? &s_rows[index] : NULL;
}

bool menu_runtime_is_dive_plan(void)
{
    return s_current_menu == MENU_INFO_DIVE_PLAN;
}

bool menu_runtime_is_dive_plan_result(void)
{
    return menu_runtime_is_dive_plan() && submenu_dive_plan_is_result_page();
}

bool menu_runtime_is_nested(void)
{
    return s_stack_depth > 0U;
}

uint8_t menu_runtime_default_selection(void)
{
    return menu_runtime_is_dive_plan() && s_row_count > 1U ? 1U : 0U;
}
