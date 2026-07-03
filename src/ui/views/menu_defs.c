/*
 * 文件: src/app_ui/ui/views/menu_defs.c
 * 作用: 该文件属于视图表现模块，负责菜单、子菜单、模态框或特定视图状态的展示与交互联动。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "menu_defs.h"

const menu_item_cfg_t g_menu_info_items[SUBMENU_INFO_COUNT] =
{
    /* INFO 菜单是只读信息页集合，主要提供潜水状态与历史摘要。 */
    { "LAST DIVE",       NULL, FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "DIVE PLAN",       NULL, FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "TISSUE & TOX",    NULL, FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "GAS & CALC",      NULL, FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "SENSOR & DEVICE", NULL, FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "DIVE LOG",        NULL, FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
};

const menu_item_cfg_t g_menu_setup_items[SUBMENU_SETUP_COUNT] =
{
    /* DIVE MENU 保留原设置入口层级，硬件即时控制单独放到 DEVICE CONTROL。 */
    { "GAS SWITCH",    "",     FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "CONSERVATISM",  "",     FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "SYSTEM SETUP",  NULL,   FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "",              NULL,   FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "",              NULL,   FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "",              NULL,   FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
};

static const menu_item_cfg_t s_menu_device_items[] =
{
    /* DEVICE CONTROL 放即时硬件控制和校准入口。 */
    { "BRIGHTNESS",    "",     FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "COMPASS CAL",   "IDLE", FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "LIGHT CONTROL", NULL,   FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "BLUETOOTH",     NULL,   FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "TURN OFF",      NULL,   FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
};

#define MENU_SETUP_DIVE_ROOT_COUNT 3U

static menu_setup_root_t s_setup_root = MENU_SETUP_ROOT_DIVE;

const menu_item_cfg_t *menu_defs_info_items(uint8_t *out_count)
{
    /* 对外只返回静态表和条目数量，方便 view 层直接渲染。 */
    if (out_count)
    {
        *out_count = (uint8_t)(sizeof(g_menu_info_items) / sizeof(g_menu_info_items[0]));
    }
    return g_menu_info_items;
}

const menu_item_cfg_t *menu_defs_setup_items(uint8_t *out_count)
{
    /* SETUP 物理页根据 MENU HUB 选择切换为 DIVE MENU 或 DEVICE CONTROL。 */
    if (out_count)
    {
        *out_count = (s_setup_root == MENU_SETUP_ROOT_DEVICE)
                         ? (uint8_t)(sizeof(s_menu_device_items) / sizeof(s_menu_device_items[0]))
                         : MENU_SETUP_DIVE_ROOT_COUNT;
    }
    return (s_setup_root == MENU_SETUP_ROOT_DEVICE) ? s_menu_device_items : g_menu_setup_items;
}

void menu_defs_set_setup_root(menu_setup_root_t root)
{
    s_setup_root = (root == MENU_SETUP_ROOT_DEVICE) ? MENU_SETUP_ROOT_DEVICE : MENU_SETUP_ROOT_DIVE;
}

menu_setup_root_t menu_defs_get_setup_root(void)
{
    return s_setup_root;
}

const char *menu_defs_setup_root_title(void)
{
    return (s_setup_root == MENU_SETUP_ROOT_DEVICE) ? "DEVICE CONTROL" : "DIVE MENU";
}

menu_id_t menu_defs_info_menu_for_index(uint8_t index)
{
    /* INFO 菜单索引到具体子页的映射表。 */
    static const menu_id_t map[] =
    {
        MENU_INFO_LAST_DIVE,
        MENU_INFO_DIVE_PLAN,
        MENU_INFO_TISSUE_TOX,
        MENU_INFO_GAS_CALC,
        MENU_INFO_SENSOR_DEVICE,
        MENU_INFO_DIVE_LOG,
    };
    return (index < (sizeof(map) / sizeof(map[0]))) ? map[index] : MENU_NONE;
}

menu_id_t menu_defs_setup_menu_for_index(uint8_t index)
{
    /* SETUP 菜单索引到具体子页的映射表。 */
    static const menu_id_t dive_map[] =
    {
        MENU_SETUP_GAS_SWITCH,
        MENU_SETUP_CONSERVATISM,
        MENU_SETUP_SYSTEMS,
    };
    static const menu_id_t device_map[] =
    {
        MENU_SETUP_BRIGHTNESS,
        MENU_SETUP_COMPASS_CAL,
        MENU_SETUP_LIGHT_CONTROL,
        MENU_SETUP_BLUETOOTH,
        MENU_SETUP_TURN_OFF,
    };
    if (s_setup_root == MENU_SETUP_ROOT_DEVICE)
    {
        return (index < (sizeof(device_map) / sizeof(device_map[0]))) ? device_map[index] : MENU_NONE;
    }
    return (index < (sizeof(dive_map) / sizeof(dive_map[0]))) ? dive_map[index] : MENU_NONE;
}

const char *menu_defs_title(menu_id_t id)
{
    /* menu_id 的统一标题出口，view 层不直接写死字符串。 */
    switch (id)
    {
    case MENU_INFO_LAST_DIVE:      return "LAST DIVE";
    case MENU_INFO_DIVE_PLAN:      return "DIVE PLAN";
    case MENU_INFO_TISSUE_TOX:     return "TISSUE & TOX";
    case MENU_INFO_GAS_CALC:       return "GAS & CALC";
    case MENU_INFO_SENSOR_DEVICE:  return "SENSOR & DEVICE";
    case MENU_INFO_DIVE_LOG:       return "DIVE LOG";
    case MENU_SETUP_GAS_SWITCH:    return "GAS SWITCH";
    case MENU_SETUP_CONSERVATISM:  return "CONSERVATISM";
    case MENU_SETUP_BRIGHTNESS:    return "BRIGHTNESS";
    case MENU_SETUP_COMPASS_CAL:   return "COMPASS CAL";
    case MENU_SETUP_LIGHT_CONTROL: return "LIGHT CONTROL";
    case MENU_SETUP_BLUETOOTH:     return "BLUETOOTH";
    case MENU_SETUP_TURN_OFF:      return "TURN OFF";
    case MENU_SETUP_SYSTEMS:       return "SYSTEMS SETUP";
    case MENU_MODE_SETUP:          return "MODE SETUP";
    case MENU_NITROX:              return "NITROX";
    case MENU_THREE_GAS:           return "3 GAS";
    case MENU_OC_TECH:             return "OC Tech";
    case MENU_OC_TECH_EDIT:        return "GAS CONFIG";
    case MENU_DIVE_SETUP:          return "DIVE SETUP";
    case MENU_AI_SETUP:            return "AI SETUP";
    case MENU_ALERTS_SETUP:        return "ALERTS SETUP";
    case MENU_DISPLAY:             return "DISPLAY";
    case MENU_DATE_CLOCK:          return "DATE & CLOCK";
    case MENU_TIME_ADJUST:         return "TIME";
    case MENU_DATE_ADJUST:         return "DATE";
    case MENU_DATE_FORMAT:         return "DATE FORMAT";
    case MENU_LIGHT_COLOR:         return "LIGHT COLOR";
    case MENU_LIGHT_LEVEL:         return "LIGHT LEVEL";
    case MENU_LIGHT_RED:           return "RED";
    case MENU_LIGHT_GREEN:         return "GREEN";
    case MENU_LIGHT_BLUE:          return "BLUE";
    case MENU_LIGHT_WHITE:         return "WHITE";
    default:                       return "";
    }
}

menu_item_id_t menu_defs_back_item(void)
{
    /* 所有子菜单默认共用同一个返回项。 */
    return MENU_ITEM_BACK;
}

menu_id_t menu_defs_child_menu_for_item(menu_item_id_t id)
{
    /* 根据菜单项 ID 决定是否继续进入下一层菜单。 */
    switch (id)
    {
    case MENU_ITEM_SYSTEM_MODE_SETUP:    return MENU_MODE_SETUP;
    case MENU_ITEM_SYSTEM_DIVE_SETUP:    return MENU_DIVE_SETUP;
    case MENU_ITEM_SYSTEM_AI_SETUP:      return MENU_AI_SETUP;
    case MENU_ITEM_SYSTEM_ALERTS_SETUP:  return MENU_ALERTS_SETUP;
    case MENU_ITEM_SYSTEM_DISPLAY:       return MENU_DISPLAY;
    case MENU_ITEM_MODE_AIR:             return MENU_OC_TECH_EDIT;
    case MENU_ITEM_MODE_NITROX:          return MENU_OC_TECH_EDIT;
    case MENU_ITEM_MODE_THREE_GAS:       return MENU_THREE_GAS;
    case MENU_ITEM_MODE_OC_TECH:         return MENU_OC_TECH;
    case MENU_ITEM_NITROX_O2:            return MENU_OC_TECH_EDIT;
    case MENU_ITEM_THREE_GAS_O2_0:
    case MENU_ITEM_THREE_GAS_O2_1:
    case MENU_ITEM_THREE_GAS_O2_2:       return MENU_OC_TECH_EDIT;
    case MENU_ITEM_DISPLAY_DATE_CLOCK:   return MENU_DATE_CLOCK;
    case MENU_ITEM_DATETIME_TIME:        return MENU_TIME_ADJUST;
    case MENU_ITEM_DATETIME_DATE:        return MENU_DATE_ADJUST;
    case MENU_ITEM_DATETIME_DATE_FORMAT: return MENU_DATE_FORMAT;
    case MENU_ITEM_LIGHT_COLOR:          return MENU_LIGHT_COLOR;
    case MENU_ITEM_LIGHT_LEVEL:          return MENU_LIGHT_LEVEL;
    case MENU_ITEM_OC_TECH_SLOT_0:
    case MENU_ITEM_OC_TECH_SLOT_1:
    case MENU_ITEM_OC_TECH_SLOT_2:
    case MENU_ITEM_OC_TECH_SLOT_3:
    case MENU_ITEM_OC_TECH_SLOT_4:       return MENU_OC_TECH_EDIT;
    default:                             return MENU_NONE;
    }
}

bool menu_defs_is_info_menu(menu_id_t id)
{
    /* INFO 类菜单在 view 层通常走只读展示逻辑。 */
    return id == MENU_INFO_LAST_DIVE ||
           id == MENU_INFO_DIVE_PLAN ||
           id == MENU_INFO_TISSUE_TOX ||
           id == MENU_INFO_GAS_CALC ||
           id == MENU_INFO_SENSOR_DEVICE ||
           id == MENU_INFO_DIVE_LOG;
}

bool menu_defs_is_readonly_menu(menu_id_t id)
{
    /* 这些页面只展示数据，不允许交互修改。 */
    return id == MENU_INFO_LAST_DIVE ||
           id == MENU_INFO_TISSUE_TOX ||
           id == MENU_INFO_GAS_CALC ||
           id == MENU_INFO_SENSOR_DEVICE;
}

bool menu_defs_is_light_color_menu(menu_id_t id)
{
    /* 灯光颜色菜单是特殊的子级选择页。 */
    return id == MENU_LIGHT_RED ||
           id == MENU_LIGHT_GREEN ||
           id == MENU_LIGHT_BLUE ||
           id == MENU_LIGHT_WHITE;
}

const char *menu_defs_light_color_name(menu_id_t id)
{
    /* 灯光颜色的统一文本出口。 */
    switch (id)
    {
    case MENU_LIGHT_RED:   return "RED";
    case MENU_LIGHT_GREEN: return "GREEN";
    case MENU_LIGHT_BLUE:  return "BLUE";
    case MENU_LIGHT_WHITE: return "WHITE";
    default:               return "";
    }
}
