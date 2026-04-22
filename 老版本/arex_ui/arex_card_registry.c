#include "arex_card_registry.h"
#include "arex_data.h"

/* Forward declarations — each card's .c file implements these */
void card_info_create(lv_obj_t *parent);    void card_info_update(void);
void card_compass_create(lv_obj_t *parent); void card_compass_update(void);
void card_deco_create(lv_obj_t *parent);    void card_deco_update(void);
void card_gas_create(lv_obj_t *parent);     void card_gas_update(void);
void card_plan_create(lv_obj_t *parent);    void card_plan_update(void);
void card_setup_create(lv_obj_t *parent);   void card_setup_update(void);

/* =========================================
   Master registry table
   Order here is STABLE — display order is
   controlled by g_arex.settings.card_order[]
   ========================================= */
static arex_card_reg_t s_registry[AREX_CARD_COUNT] = {
    [CARD_ID_INFO] = {
        .id          = CARD_ID_INFO,
        .title       = "INFO MENU",
        .tile_obj    = NULL,
        .create_cb   = card_info_create,
        .update_cb   = card_info_update,
        .on_enter_cb = NULL,
    },
    [CARD_ID_COMPASS] = {
        .id          = CARD_ID_COMPASS,
        .title       = "NAV COMPASS",
        .tile_obj    = NULL,
        .create_cb   = card_compass_create,
        .update_cb   = card_compass_update,
        .on_enter_cb = NULL,
    },
    [CARD_ID_DECO] = {
        .id          = CARD_ID_DECO,
        .title       = "TISSUES & DECO",
        .tile_obj    = NULL,
        .create_cb   = card_deco_create,
        .update_cb   = card_deco_update,
        .on_enter_cb = NULL,
    },
    [CARD_ID_GAS] = {
        .id          = CARD_ID_GAS,
        .title       = "GAS SWITCH",
        .tile_obj    = NULL,
        .create_cb   = card_gas_create,
        .update_cb   = card_gas_update,
        .on_enter_cb = NULL,
    },
    [CARD_ID_PLAN] = {
        .id          = CARD_ID_PLAN,
        .title       = "DIVE PLAN TRACK",
        .tile_obj    = NULL,
        .create_cb   = card_plan_create,
        .update_cb   = card_plan_update,
        .on_enter_cb = NULL,
    },
    [CARD_ID_SETUP] = {
        .id          = CARD_ID_SETUP,
        .title       = "DIVE SETUP",
        .tile_obj    = NULL,
        .create_cb   = card_setup_create,
        .update_cb   = card_setup_update,
        .on_enter_cb = NULL,
    },
};

/* =========================================
   API implementations
   ========================================= */
arex_card_reg_t *arex_card_registry(void)
{
    return s_registry;
}

uint8_t arex_card_count(void)
{
    return AREX_CARD_COUNT;
}

arex_card_reg_t *arex_card_get(uint8_t order_pos)
{
    if (order_pos >= AREX_CARD_COUNT) return NULL;
    uint8_t id = g_arex.settings.card_order[order_pos];
    if (id >= AREX_CARD_COUNT) return NULL;
    return &s_registry[id];
}

arex_card_reg_t *arex_card_get_by_id(arex_card_id_t id)
{
    if (id >= AREX_CARD_COUNT) return NULL;
    return &s_registry[id];
}
