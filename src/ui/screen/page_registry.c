/**
 * @file page_registry.c
 * @brief 右侧页面注册表：统一管理 tileview 页面元数据和显示位置映射。
 *
 * 设计核心：双层位置空间
 *   - display_pos: 用户看到的滑动顺序（0=INFO, 1~N=动态页, 最后=DIVE MENU）
 *   - storage_pos: g_sys_config.card_order[] 的数组索引（INFO=0, SETUP=13, 动态槽=1~12）
 *
 * BLE 旧协议字段仍叫 card_order[8]，但 UI 内部概念是 page。
 */

#include "page_registry.h"
#include "../core/ui_engine.h"

/* INFO/DIVE MENU 是顶层菜单页，源码在 menus/。
 * 它们仍登记在这里，是因为右侧 tileview 由 page_registry 统一管理。
 */
void menu_info_create(lv_obj_t *parent);
void menu_info_update(void);
void card_compass_create(lv_obj_t *parent);
void card_compass_update(void);
void card_deco_create(lv_obj_t *parent);
void card_deco_update(void);
void card_gas_create(lv_obj_t *parent);
void card_gas_update(void);
void card_plan_create(lv_obj_t *parent);
void card_plan_update(void);
void card_blank_create(lv_obj_t *parent);
void card_blank_update(void);
void menu_setup_create(lv_obj_t *parent);
void menu_setup_update(void);

extern const menu_list_cfg_t menu_info_cfg;
extern const menu_list_cfg_t menu_setup_cfg;

static page_t g_pages[PAGE_ID_COUNT] =
{
    [PAGE_ID_INFO] = {
        .id          = PAGE_ID_INFO,
        .title       = "INFO MENU",
        .engine_type = PAGE_ENGINE_MENU,
        .config_data = &menu_info_cfg,
        .tile_obj    = NULL,
        .create_cb   = menu_info_create,
        .update_cb   = menu_info_update,
        .on_enter_cb = NULL,
    },
    [PAGE_ID_COMPASS] = {
        .id          = PAGE_ID_COMPASS,
        .title       = "NAV COMPASS",
        .engine_type = PAGE_ENGINE_CUSTOM,
        .config_data = NULL,
        .tile_obj    = NULL,
        .create_cb   = card_compass_create,
        .update_cb   = card_compass_update,
        .on_enter_cb = NULL,
    },
    [PAGE_ID_DECO] = {
        .id          = PAGE_ID_DECO,
        .title       = "TISSUES & DECO",
        .engine_type = PAGE_ENGINE_CUSTOM,
        .config_data = NULL,
        .tile_obj    = NULL,
        .create_cb   = card_deco_create,
        .update_cb   = card_deco_update,
        .on_enter_cb = NULL,
    },
    [PAGE_ID_GAS] = {
        .id          = PAGE_ID_GAS,
        .title       = "GAS SWITCH",
        .engine_type = PAGE_ENGINE_CUSTOM,
        .config_data = NULL,
        .tile_obj    = NULL,
        .create_cb   = card_gas_create,
        .update_cb   = card_gas_update,
        .on_enter_cb = NULL,
    },
    [PAGE_ID_PLAN] = {
        .id          = PAGE_ID_PLAN,
        .title       = "DIVE PLAN TRACK",
        .engine_type = PAGE_ENGINE_CUSTOM,
        .config_data = NULL,
        .tile_obj    = NULL,
        .create_cb   = card_plan_create,
        .update_cb   = card_plan_update,
        .on_enter_cb = NULL,
    },
    [PAGE_ID_CUSTOM_GRID] = {
        .id          = PAGE_ID_CUSTOM_GRID,
        .title       = "5F: CUSTOM WIDGETS",
        .engine_type = PAGE_ENGINE_GRID,
        .config_data = NULL,
        .tile_obj    = NULL,
        .create_cb   = NULL,
        .update_cb   = NULL,
        .on_enter_cb = NULL,
    },
    [PAGE_ID_BLANK] = {
        .id          = PAGE_ID_BLANK,
        .title       = "BLANK",
        .engine_type = PAGE_ENGINE_CUSTOM,
        .config_data = NULL,
        .tile_obj    = NULL,
        .create_cb   = card_blank_create,
        .update_cb   = card_blank_update,
        .on_enter_cb = NULL,
    },
    [PAGE_ID_SETUP] = {
        .id          = PAGE_ID_SETUP,
        .title       = "DIVE MENU",
        .engine_type = PAGE_ENGINE_MENU,
        .config_data = &menu_setup_cfg,
        .tile_obj    = NULL,
        .create_cb   = menu_setup_create,
        .update_cb   = menu_setup_update,
        .on_enter_cb = NULL,
    },
};

static uint8_t dynamic_page_count_all(void)
{
    uint8_t count = 0;
    for (uint8_t pos = PAGE_POS_DYNAMIC_FIRST; pos < PAGE_POS_SETUP; ++pos)
    {
        uint8_t id = g_sys_page_order(pos);
        if (id != PAGE_ID_UNUSED)
        {
            count++;
        }
    }
    return (count > 0) ? count : 1;
}

uint8_t page_visible_dash_count(void)
{
    uint8_t count = 0;
    for (uint8_t pos = PAGE_POS_DYNAMIC_FIRST; pos < PAGE_POS_SETUP; ++pos)
    {
        uint8_t id = g_sys_page_order(pos);
        if (id != PAGE_ID_UNUSED && id != PAGE_ID_BLANK)
        {
            count++;
        }
    }
    return (count > 0) ? count : 1;
}

uint8_t page_setup_display_pos(void)
{
    return (uint8_t)(PAGE_POS_DYNAMIC_FIRST + dynamic_page_count_all());
}

uint8_t page_count(void)
{
    return (uint8_t)(page_setup_display_pos() + 1);
}

uint8_t page_storage_pos(uint8_t display_pos)
{
    uint8_t setup_pos = page_setup_display_pos();

    if (display_pos == PAGE_POS_INFO)
    {
        return PAGE_POS_INFO;
    }
    if (display_pos >= PAGE_POS_DYNAMIC_FIRST && display_pos < setup_pos)
    {
        return display_pos;
    }
    if (display_pos == setup_pos)
    {
        return PAGE_POS_SETUP;
    }

    return 0xFF;
}

uint8_t page_id_at(uint8_t display_pos)
{
    uint8_t storage_pos = page_storage_pos(display_pos);
    if (storage_pos == 0xFF)
    {
        return PAGE_ID_UNUSED;
    }
    return g_sys_page_order(storage_pos);
}

page_t *page_get(uint8_t display_pos)
{
    if (display_pos >= page_count()) return NULL;
    uint8_t id = page_id_at(display_pos);
    if (id >= PAGE_ID_COUNT) return NULL;
    return &g_pages[id];
}

page_t *page_get_by_id(page_id_t id)
{
    if (id >= PAGE_ID_COUNT) return NULL;
    return &g_pages[id];
}
