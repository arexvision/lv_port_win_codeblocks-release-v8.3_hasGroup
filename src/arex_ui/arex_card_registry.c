/**
 * @file arex_card_registry.c
 * @brief 卡片注册表 — 所有卡片的元数据表 + 位置映射查询入口
 *
 * 设计核心：双层位置空间
 *   - display_pos: 用户看到的滑动顺序（0=INFO, 1~N=动态卡片, 最后=SETUP）
 *   - storage_pos: card_order[] 数组索引（INFO=0, SETUP=13, 动态槽=1~12）
 *
 * 映射关系由 g_sys_card_order() 间接层实现，支持 BLE 下发动态重排。
 */

#include "arex_card_registry.h"
#include "arex_ui_engine.h"

/* 前向声明：各卡片的具体创建/更新实现（由具体卡片模块提供） */
void card_info_create(lv_obj_t *parent);    void card_info_update(void);
void card_compass_create(lv_obj_t *parent); void card_compass_update(void);
void card_deco_create(lv_obj_t *parent);    void card_deco_update(void);
void card_gas_create(lv_obj_t *parent);     void card_gas_update(void);
void card_plan_create(lv_obj_t *parent);    void card_plan_update(void);
void card_blank_create(lv_obj_t *parent);   void card_blank_update(void);
void card_setup_create(lv_obj_t *parent);   void card_setup_update(void);

/* 菜单配置（INFO/SETUP 两个菜单卡片使用） */
extern const arex_menu_list_cfg_t info_menu_cfg;
extern const arex_menu_list_cfg_t setup_menu_cfg;

/**
 * @brief 全局卡片注册表
 *
 * ROM 字段在初始化时固定，tile_obj 在屏幕创建时由 arex_screen.c 填充。
 * 使用指定初始化器确保字段对齐，枚举 ID 直接作为数组下标。
 *
 * @note CARD_ID_CUSTOM_GRID 的 create_cb/update_cb 为 NULL，
 *       因为 GRID 引擎由 arex_screen.c 的 switch 分支直接调度，不走回调。
 */
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

/**
 * @brief 统计可见动态卡片数量
 *
 * 遍历 CARD_POS_DYNAMIC_FIRST ~ CARD_POS_SETUP 范围，
 * 统计 card_order[] 中非 CARD_ID_UNUSED 的槽位数量。
 * 至少返回 1，保证 INFO 左侧有可滑动目标。
 *
 * @return 有效动态卡片数量（1~AREX_MAX_DYNAMIC_SLOTS）
 */
uint8_t arex_visible_dash_count(void)
{
    uint8_t count = 0;
    for (uint8_t pos = CARD_POS_DYNAMIC_FIRST; pos < CARD_POS_SETUP; ++pos) {
        uint8_t id = g_sys_card_order(pos);
        if (id != CARD_ID_UNUSED) {
            count++;
        }
    }
    return (count > 0) ? count : 1;
}

/**
 * @brief 获取 SETUP 菜单在显示序列中的位置
 *
 * 公式：SETUP 显示位置 = 动态区起始 + 可见动态卡片数
 * 例如：INFO(0) + 5张动态卡片 → SETUP 在显示序列 index=6
 *
 * @return SETUP 在 display_pos 序列中的索引
 */
uint8_t arex_setup_display_pos(void)
{
    return (uint8_t)(CARD_POS_DYNAMIC_FIRST + arex_visible_dash_count());
}

/**
 * @brief 获取 tileview 总卡片数
 *
 * = INFO(1) + 动态卡片(N) + SETUP(1)
 *
 * @return 总卡片数量
 */
uint8_t arex_card_count(void)
{
    return (uint8_t)(arex_setup_display_pos() + 1);
}

/**
 * @brief 显示位置 → 存储位置转换
 *
 * 处理 INFO/SETUP 固定卡与动态区的不同索引映射：
 *   - display_pos 0 (INFO)  → storage_pos = CARD_POS_INFO (0)
 *   - display_pos 1~N       → storage_pos = display_pos（动态区一一对应）
 *   - display_pos SETUP     → storage_pos = CARD_POS_SETUP (13)
 *
 * @param display_pos 用户看到的卡片索引
 * @return storage_pos card_order[] 数组索引，0xFF 表示无效位置
 */
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

/**
 * @brief 根据显示位置查询卡片 ID
 *
 * @param display_pos 用户看到的卡片索引
 * @return 卡片 ID (CARD_ID_*)，CARD_ID_UNUSED 表示无效
 */
uint8_t arex_card_id_at(uint8_t display_pos)
{
    uint8_t storage_pos = arex_card_storage_pos(display_pos);
    if (storage_pos == 0xFF) {
        return CARD_ID_UNUSED;
    }
    return g_sys_card_order(storage_pos);
}

/**
 * @brief 按显示顺序获取卡片对象（主要入口）
 *
 * 供 arex_screen.c 遍历 tileview 时调用：
 *   for (int i = 0; i < arex_card_count(); i++) {
 *       arex_card_t *card = arex_card_get(i);
 *       lv_tileview_add_tile(..., card->tile_obj);
 *   }
 *
 * @param order_pos 显示顺序索引（0=INFO）
 * @return 卡片对象指针，NULL 表示越界或无效
 */
arex_card_t *arex_card_get(uint8_t order_pos)
{
    if (order_pos >= arex_card_count()) return NULL;
    uint8_t id = arex_card_id_at(order_pos);
    if (id >= AREX_CARD_ID_COUNT) return NULL;
    return &g_cards[id];
}

/**
 * @brief 按卡片 ID 直接获取卡片对象
 *
 * 用于已知卡片类型时的快速查找（如菜单项点击回调）。
 *
 * @param id 卡片 ID (CARD_ID_*)
 * @return 卡片对象指针，NULL 表示无效 ID
 */
arex_card_t *arex_card_get_by_id(arex_card_id_t id)
{
    if (id >= AREX_CARD_ID_COUNT) return NULL;
    return &g_cards[id];
}
