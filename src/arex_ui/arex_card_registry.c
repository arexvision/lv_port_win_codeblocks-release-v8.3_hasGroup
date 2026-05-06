#include "arex_card_registry.h"
#include "arex_ui_engine.h"

void card_info_create(lv_obj_t *parent);    void card_info_update(void);
void card_compass_create(lv_obj_t *parent); void card_compass_update(void);
void card_deco_create(lv_obj_t *parent);    void card_deco_update(void);
void card_gas_create(lv_obj_t *parent);     void card_gas_update(void);
void card_plan_create(lv_obj_t *parent);    void card_plan_update(void);
void card_blank_create(lv_obj_t *parent);   void card_blank_update(void);
void card_setup_create(lv_obj_t *parent);   void card_setup_update(void);

extern const arex_menu_list_cfg_t info_menu_cfg;
extern const arex_menu_list_cfg_t setup_menu_cfg;

/* Single unified table — ROM fields set here, tile_obj filled at runtime */
static arex_card_t g_cards[AREX_CARD_ID_COUNT] = {
    [CARD_ID_INFO] = {
        .id          = CARD_ID_INFO,
        .title       = "INFO MENU",
        .engine_type = CARD_ENGINE_MENU,
        .config_data = &info_menu_cfg,
        .tile_obj    = NULL,
        .create_cb   = card_info_create,
        .update_cb   = card_info_update,
        .on_enter_cb = NULL,
    },
    [CARD_ID_COMPASS] = {
        .id          = CARD_ID_COMPASS,
        .title       = "NAV COMPASS",
        .engine_type = CARD_ENGINE_CUSTOM,
        .config_data = NULL,
        .tile_obj    = NULL,
        .create_cb   = card_compass_create,
        .update_cb   = card_compass_update,
        .on_enter_cb = NULL,
    },
    [CARD_ID_DECO] = {
        .id          = CARD_ID_DECO,
        .title       = "TISSUES & DECO",
        .engine_type = CARD_ENGINE_CUSTOM,
        .config_data = NULL,
        .tile_obj    = NULL,
        .create_cb   = card_deco_create,
        .update_cb   = card_deco_update,
        .on_enter_cb = NULL,
    },
    [CARD_ID_GAS] = {
        .id          = CARD_ID_GAS,
        .title       = "GAS SWITCH",
        .engine_type = CARD_ENGINE_CUSTOM,
        .config_data = NULL,
        .tile_obj    = NULL,
        .create_cb   = card_gas_create,
        .update_cb   = card_gas_update,
        .on_enter_cb = NULL,
    },
    [CARD_ID_PLAN] = {
        .id          = CARD_ID_PLAN,
        .title       = "DIVE PLAN TRACK",
        .engine_type = CARD_ENGINE_CUSTOM,
        .config_data = NULL,
        .tile_obj    = NULL,
        .create_cb   = card_plan_create,
        .update_cb   = card_plan_update,
        .on_enter_cb = NULL,
    },
    [CARD_ID_CUSTOM_GRID] = {
        .id          = CARD_ID_CUSTOM_GRID,
        .title       = "5F: CUSTOM WIDGETS",
        .engine_type = CARD_ENGINE_GRID,
        .config_data = NULL,
        .tile_obj    = NULL,
        .create_cb   = NULL,   /* GRID 引擎由 arex_screen.c switch 分支直接调度 */
        .update_cb   = NULL,
        .on_enter_cb = NULL,
    },
    [CARD_ID_BLANK] = {
        .id          = CARD_ID_BLANK,
        .title       = "BLANK",
        .engine_type = CARD_ENGINE_CUSTOM,
        .config_data = NULL,
        .tile_obj    = NULL,
        .create_cb   = card_blank_create,
        .update_cb   = card_blank_update,
        .on_enter_cb = NULL,
    },
    [CARD_ID_SETUP] = {
        .id          = CARD_ID_SETUP,
        .title       = "DIVE MENU",
        .engine_type = CARD_ENGINE_MENU,
        .config_data = &setup_menu_cfg,
        .tile_obj    = NULL,
        .create_cb   = card_setup_create,
        .update_cb   = card_setup_update,
        .on_enter_cb = NULL,
    },
};

uint8_t arex_visible_dash_count(void)
{
    /* 统计有效卡片数量（排除 INFO/SETUP 菜单，以及未使用的槽位） */
    uint8_t count = 0;
    for (uint8_t pos = CARD_POS_DYNAMIC_FIRST; pos < CARD_POS_SETUP; ++pos) {
        uint8_t id = g_sys_card_order(pos);
    /* 有效卡片：不是 UNUSED（0xFF=未占用槽位），也不是 INFO/SETUP（虽然它们不在这个范围内） */
        if (id != CARD_ID_UNUSED)
        {
            count++;
        }
    }

    return (count > 0) ? count : 1;
}

uint8_t arex_setup_display_pos(void)
{
    return (uint8_t)(CARD_POS_DYNAMIC_FIRST + arex_visible_dash_count());
}

uint8_t arex_card_count(void)
{
    return (uint8_t)(arex_setup_display_pos() + 1);
}

uint8_t arex_card_storage_pos(uint8_t display_pos)
{
    uint8_t setup_pos = arex_setup_display_pos();

    if (display_pos == CARD_POS_INFO) {
        return CARD_POS_INFO;
    }
    if (display_pos >= CARD_POS_DYNAMIC_FIRST && display_pos < setup_pos) {
        return display_pos;
    }
    if (display_pos == setup_pos) {
        return CARD_POS_SETUP;
    }

    return 0xFF;
}

uint8_t arex_card_id_at(uint8_t display_pos)
{
    uint8_t storage_pos = arex_card_storage_pos(display_pos);
    if (storage_pos == 0xFF) {
        return CARD_ID_UNUSED;
    }
    return g_sys_card_order(storage_pos);
}

arex_card_t *arex_card_get(uint8_t order_pos)
{
    if (order_pos >= arex_card_count()) return NULL;
    uint8_t id = arex_card_id_at(order_pos);
    if (id >= AREX_CARD_ID_COUNT) return NULL;
    return &g_cards[id];
}

arex_card_t *arex_card_get_by_id(arex_card_id_t id)
{
    if (id >= AREX_CARD_ID_COUNT) return NULL;
    return &g_cards[id];
}
