/*
 * 文件: src/app_ui/ui/views/menu_runtime.c
 * 作用: 该文件属于视图表现模块，负责菜单、子菜单、模态框或特定视图状态的展示与交互联动。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

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
    /* 清空当前菜单行缓存，下一轮 build_* 会重新填充。 */
    memset(s_rows, 0, sizeof(s_rows));
    s_row_count = 0;
}

static void row_add(menu_item_id_t id, menu_row_type_t type, const char *label)
{
    /* 菜单每一行都保存稳定 ID，避免后续点击逻辑依赖字符串内容。 */
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
    /* 把 submenu_model 给出的文本数组，包装成 view 层统一使用的 row 结构。 */
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
    /* INFO 菜单的行内容按当前子页面 index 动态生成。 */
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
            submenu_dive_plan_snapshot_t snapshot;

            submenu_dive_plan_get_snapshot(&snapshot);
            if (i == 0U)
            {
                id = MENU_ITEM_DIVE_PLAN_EXIT;
            }
            else if (snapshot.page == DIVE_PLAN_PAGE_READY)
            {
                id = MENU_ITEM_DIVE_PLAN_PLAN;
            }
            else if (snapshot.page == DIVE_PLAN_PAGE_RESULT &&
                     snapshot.result_page_index + 1U < snapshot.result_total_pages)
            {
                id = MENU_ITEM_DIVE_PLAN_MORE;
            }
            else
            {
                id = MENU_ITEM_DIVE_PLAN_NEXT;
            }
            type = MENU_ROW_DIVE_PLAN;
        }
        row_add(id, type, labels[i]);
    }
}

static void build_logbook_rows(void)
{
    rows_clear();
}

static void build_setup_gas_switch(void)
{
    /* 气体切换页直接把气体槽列表映射为可点击菜单行。 */
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
    /* 保守度菜单是固定枚举项，行 ID 与语义一一对应。 */
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
    /* 亮度菜单同样是固定枚举项，供后续 action 层执行档位切换。 */
    uint8_t count = 0;
    const char **labels = submenu_build_setup_items(2, &count);
    static const menu_item_id_t ids[] =
    {
        MENU_ITEM_BRIGHTNESS_LOW,
        MENU_ITEM_BRIGHTNESS_MED,
        MENU_ITEM_BRIGHTNESS_HIGH,
        MENU_ITEM_BRIGHTNESS_MAX,
    };
    rows_from_labels(labels, count, ids, NULL);
}

static void build_setup_compass(void)
{
    /* 罗盘校准菜单显示的是校准流程入口和复位入口。 */
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
    /* 灯光菜单既包含总开关，也包含各档位/颜色的可选行。 */
    uint8_t count = 0;
    const char **labels = submenu_build_setup_items(4, &count);
    static const menu_item_id_t ids[] =
    {
        MENU_ITEM_LIGHT_POWER,
        MENU_ITEM_LIGHT_MODE,
        MENU_ITEM_LIGHT_RED,
        MENU_ITEM_LIGHT_GREEN,
        MENU_ITEM_LIGHT_BLUE,
        MENU_ITEM_LIGHT_WHITE,
    };
    static const menu_row_type_t types[] =
    {
        MENU_ROW_LIGHT_POWER,
        MENU_ROW_LIGHT_MODE,
        MENU_ROW_NORMAL,
        MENU_ROW_NORMAL,
        MENU_ROW_NORMAL,
        MENU_ROW_NORMAL,
    };
    rows_from_labels(labels, count, ids, types);
}

static void build_setup_systems(void)
{
    /* 系统菜单以版本、模式、潜水参数、AI 和告警等入口为主。 */
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
    /* 部分子菜单仍按标题驱动生成，这里统一包一层以便 view 层复用。 */
    uint8_t count = 0;
    const char **labels = submenu_nested_items_for(title, &count);
    rows_from_labels(labels, count, ids, types);
}

static void build_light_color(void)
{
    /* 灯光颜色选择页是典型的嵌套菜单，按标题拿到对应的选择项。 */
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
    /* 可以把这一层理解成“菜单内容编排器”：
     * submenu_model 提供原始文案或动态数据，
     * menu_runtime 负责把它们整理成稳定的 row/id/type 结构，
     * 后面的 view 和 action 才能不依赖字符串做判断。 */
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
    case MENU_INFO_DIVE_LOG:
        build_logbook_rows();
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
        menu_item_id_t ids[6];
        menu_row_type_t types[6];
        for (uint8_t i = 0U; i < (uint8_t)(sizeof(ids) / sizeof(ids[0])); i++)
        {
            ids[i] = MENU_ITEM_READONLY;
            types[i] = MENU_ROW_READONLY;
        }
        uint8_t id_count = submenu_gas_edit_item_ids(ids, (uint8_t)(sizeof(ids) / sizeof(ids[0])));
        for (uint8_t i = 0U; i < id_count; i++) types[i] = (ids[i] == MENU_ITEM_GAS_EDIT_MOD) ? MENU_ROW_READONLY : MENU_ROW_NORMAL;
        build_nested_by_title(s_oc_tech_edit_title, ids, types);
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
            MENU_ITEM_DIVE_SURFACE_CONFIRM,
            MENU_ITEM_DIVE_START_DEPTH,
            MENU_ITEM_DIVE_TISSUE_RESET,
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
            MENU_ITEM_DISPLAY_TEMP_UNIT,
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
            MENU_ITEM_DATETIME_TIME,
            MENU_ITEM_DATETIME_DATE,
            MENU_ITEM_DATETIME_24H,
            MENU_ITEM_DATETIME_DATE_FORMAT,
        };
        build_nested_by_title("DATE & CLOCK", ids, NULL);
        break;
    }
    case MENU_TIME_ADJUST:
    {
        static const menu_item_id_t ids[] =
        {
            MENU_ITEM_DATE_HOUR,
            MENU_ITEM_DATE_MINUTE,
        };
        build_nested_by_title("TIME", ids, NULL);
        break;
    }
    case MENU_DATE_ADJUST:
    {
        static const menu_item_id_t ids[] =
        {
            MENU_ITEM_DATE_YEAR,
            MENU_ITEM_DATE_MONTH,
            MENU_ITEM_DATE_DAY,
        };
        build_nested_by_title("DATE", ids, NULL);
        break;
    }
    case MENU_DATE_FORMAT:
    {
        static const menu_item_id_t ids[] =
        {
            MENU_ITEM_DATE_FORMAT_MM_DD_YY,
            MENU_ITEM_DATE_FORMAT_DD_MM_YY,
        };
        build_nested_by_title("DATE FORMAT", ids, NULL);
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
    /* 退出菜单层时把运行时状态和返回栈一起清空。 */
    s_current_menu = MENU_NONE;
    s_stack_depth = 0;
    s_oc_tech_edit_title[0] = '\0';
    rows_clear();
}

bool menu_runtime_open_info(uint8_t index)
{
    /* 打开 INFO 菜单时，先把当前页面切到对应索引，再重建条目。 */
    s_current_menu = menu_defs_info_menu_for_index(index);
    s_stack_depth = 0;
    build_rows();
    return s_current_menu != MENU_NONE && (s_row_count > 0U || menu_runtime_is_logbook());
}

bool menu_runtime_open_setup(uint8_t index)
{
    /* 打开 SETUP 菜单时同样重建当前行集合。 */
    s_current_menu = menu_defs_setup_menu_for_index(index);
    s_stack_depth = 0;
    build_rows();
    return s_current_menu != MENU_NONE && s_row_count > 0U;
}

bool menu_runtime_open_child(menu_id_t child_id, menu_item_id_t source_item)
{
    /* 进入子菜单前，把父级菜单和当前选中项压入栈。 */
    /* 这个返回栈很关键：它保存的不是字符串标题，而是 menu_id + selected_idx，
     * 所以即使父菜单内容在返回前动态刷新了，也能尽量回到原焦点位置。 */
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
    /* 返回上一层时从栈顶恢复父菜单和选中项。 */
    /* 注意顺序是：
     * 1. 恢复父 menu_id
     * 2. 重建 rows
     * 3. 恢复原选中索引
     * 如果先恢复索引再 build_rows，动态菜单可能把该索引覆盖掉。 */
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
    /* 当前菜单的文本或状态变化后，直接重建行列表。 */
    build_rows();
}

menu_id_t menu_runtime_current_id(void)
{
    /* 查询当前打开的 menu_id。 */
    return s_current_menu;
}

const char *menu_runtime_current_title(void)
{
    /* 标题查询只读，不修改菜单状态。 */
    if (s_current_menu == MENU_OC_TECH_EDIT && s_oc_tech_edit_title[0] != '\0')
    {
        return s_oc_tech_edit_title;
    }
    return menu_defs_title(s_current_menu);
}

const menu_row_t *menu_runtime_current_rows(uint8_t *out_count)
{
    /* 返回当前菜单的行数组和条目数量。 */
    if (out_count)
    {
        *out_count = s_row_count;
    }
    return s_rows;
}

const menu_row_t *menu_runtime_row_at(uint8_t index)
{
    /* 通过索引读取某一行，供 view 层逐项渲染。 */
    return (index < s_row_count) ? &s_rows[index] : NULL;
}

bool menu_runtime_is_dive_plan(void)
{
    /* INFO/DIVE PLAN 是特殊计划页，后续要走专用绘制逻辑。 */
    return s_current_menu == MENU_INFO_DIVE_PLAN;
}

bool menu_runtime_is_logbook(void)
{
    return s_current_menu == MENU_INFO_DIVE_LOG;
}

bool menu_runtime_is_dive_plan_result(void)
{
    /* 判断当前是否处在计划结果页。 */
    return menu_runtime_is_dive_plan() && submenu_dive_plan_is_result_page();
}

bool menu_runtime_is_nested(void)
{
    /* 栈深度大于 0 说明当前处在子菜单层。 */
    return s_stack_depth > 0U;
}

uint8_t menu_runtime_stack_depth(void)
{
    /* 对外暴露真实返回栈深度，供 UI 状态机在重建/返回时恢复层级语义。 */
    return s_stack_depth;
}

uint8_t menu_runtime_default_selection(void)
{
    /* 潜水计划页默认选中第 1 行，其余菜单默认选中第 0 行。 */
    return menu_runtime_is_dive_plan() && s_row_count > 1U ? 1U : 0U;
}
