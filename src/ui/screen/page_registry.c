/*
 * 文件: src/app_ui/ui/screen/page_registry.c
 * 作用: 该文件属于屏幕或页面编排模块，负责整屏布局、分页切换、覆盖层、编辑态显示或页面注册管理。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "page_registry.h"
#include "../core/ui_engine.h"
#include "../core/vm/ui_vm_plan_chart.h"

/*
 * 这个文件负责页面注册和页面位置映射。
 * 它不直接绘制界面，而是为 screen 层提供统一的页面元数据入口。
 */

/* INFO/DIVE MENU 是顶层菜单页，源码在 menus/。
 * 它们仍登记在这里，是因为右侧 tileview 由 page_registry 统一管理。
 */
void menu_info_create(lv_obj_t *parent);
void menu_info_update(void);
void card_compass_create(lv_obj_t *parent);
void card_compass_update(void);
void card_deco_create(lv_obj_t *parent);
void card_deco_update(void);
void card_deco_update_vm(const ui_vm_deco_t *vm);
void card_gas_create(lv_obj_t *parent);
void card_gas_update(void);
void card_gas_update_vm(const ui_vm_gas_t *vm);
void card_plan_create(lv_obj_t *parent);
void card_plan_update(const ui_vm_plan_chart_t *vm);
void card_blank_create(lv_obj_t *parent);
void card_blank_update(void);
void menu_setup_create(lv_obj_t *parent);
void menu_setup_update(void);

extern const menu_list_cfg_t menu_info_cfg;
extern const menu_list_cfg_t menu_setup_cfg;

static void page_plan_update_vm_bridge(const void *vm)
{
    /* 计划页的 VM 数据需要先转回具体类型，再交给卡片更新函数。 */
    card_plan_update((const ui_vm_plan_chart_t *)vm);
}

static void page_deco_update_vm_bridge(const void *vm)
{
    card_deco_update_vm((const ui_vm_deco_t *)vm);
}

static void page_gas_update_vm_bridge(const void *vm)
{
    card_gas_update_vm((const ui_vm_gas_t *)vm);
}

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
        .update_vm_cb = page_deco_update_vm_bridge,
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
        .update_vm_cb = page_gas_update_vm_bridge,
    },
    [PAGE_ID_PLAN] = {
        .id          = PAGE_ID_PLAN,
        .title       = "DIVE PLAN TRACK",
        .engine_type = PAGE_ENGINE_CUSTOM,
        .config_data = NULL,
        .tile_obj    = NULL,
        .create_cb   = card_plan_create,
        .update_cb   = NULL,
        .on_enter_cb = NULL,
        .update_vm_cb = page_plan_update_vm_bridge,
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

uint8_t page_visible_dash_count(void)
{
    /* 只统计真正会显示在 DASH 区域的动态页。 */
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
    /* 设置页固定排在动态页之后。 */
    return (uint8_t)(PAGE_POS_DYNAMIC_FIRST + page_visible_dash_count());
}

uint8_t page_count(void)
{
    /* 总页数 = INFO + 动态页 + SETUP。 */
    return (uint8_t)(page_setup_display_pos() + 1);
}

uint8_t page_storage_pos(uint8_t display_pos)
{
    /* 这里要区分两个概念：
     * 1. storage_pos: 配置数组 card_order[] 里的原始槽位
     * 2. display_pos: tileview 当前真正显示出来的连续页序号
     *
     * 因为 PAGE_ID_UNUSED / PAGE_ID_BLANK 会在显示时被折叠或跳过，
     * 所以 display_pos 不能直接拿来索引 card_order[]。 */
    uint8_t setup_pos = page_setup_display_pos();

    if (display_pos == PAGE_POS_INFO)
    {
        return PAGE_POS_INFO;
    }
    if (display_pos >= PAGE_POS_DYNAMIC_FIRST && display_pos < setup_pos)
    {
        /* 动态页区间需要做一次“可见页压缩”：
         * 遍历存储槽位时跳过 UNUSED/BLANK，直到找到第 N 个可见页。 */
        uint8_t visible_pos = PAGE_POS_DYNAMIC_FIRST;

        for (uint8_t storage_pos = PAGE_POS_DYNAMIC_FIRST;
             storage_pos < PAGE_POS_SETUP;
             storage_pos++)
        {
            uint8_t id = g_sys_page_order(storage_pos);

            if (id == PAGE_ID_UNUSED || id == PAGE_ID_BLANK)
            {
                continue;
            }

            if (visible_pos == display_pos)
            {
                return storage_pos;
            }
            visible_pos++;
        }

        return 0xFF;
    }
    if (display_pos == setup_pos)
    {
        return PAGE_POS_SETUP;
    }

    return 0xFF;
}

void page_registry_update_plan_vm(const ui_vm_plan_chart_t *vm)
{
    /* 仅当计划页存在且支持 VM 刷新时才转发数据。 */
    page_t *page = page_get_by_id(PAGE_ID_PLAN);

    if (page == NULL || page->update_vm_cb == NULL)
    {
        return;
    }

    page->update_vm_cb(vm);
}

void page_registry_update_deco_vm(const ui_vm_deco_t *vm)
{
    page_t *page = page_get_by_id(PAGE_ID_DECO);

    if (page == NULL || page->update_vm_cb == NULL)
    {
        return;
    }

    page->update_vm_cb(vm);
}

void page_registry_update_gas_vm(const ui_vm_gas_t *vm)
{
    page_t *page = page_get_by_id(PAGE_ID_GAS);

    if (page == NULL || page->update_vm_cb == NULL)
    {
        return;
    }

    page->update_vm_cb(vm);
}

uint8_t page_id_at(uint8_t display_pos)
{
    /* 先转存储位置，再映射到真实页面 ID。 */
    /* 这是 screen/ui_state 层最应该调用的入口。
     * 只要涉及“当前显示位置对应哪个页面”，都不要直接碰 card_order[]，
     * 否则一旦中间有空槽或 blank 页，索引马上会错位。 */
    uint8_t storage_pos = page_storage_pos(display_pos);
    if (storage_pos == 0xFF)
    {
        return PAGE_ID_UNUSED;
    }
    return g_sys_page_order(storage_pos);
}

page_t *page_get(uint8_t display_pos)
{
    /* 通过显示位置获取页面对象。 */
    /* display_pos 面向的是 tileview 顺序，不是 page_id_t 枚举值。 */
    if (display_pos >= page_count()) return NULL;
    uint8_t id = page_id_at(display_pos);
    if (id >= PAGE_ID_COUNT) return NULL;
    return &g_pages[id];
}

page_t *page_get_by_id(page_id_t id)
{
    /* 通过页面 ID 直接访问注册表。 */
    if (id >= PAGE_ID_COUNT) return NULL;
    return &g_pages[id];
}
