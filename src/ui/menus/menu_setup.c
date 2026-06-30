/*
 * 文件: src/app_ui/ui/menus/menu_setup.c
 * 作用: 该文件属于菜单定义模块，负责信息菜单或设置菜单的条目组织、入口描述与页面装配。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时建议先理解该文件在 app_ui 流程中的位置，再修改注释所描述的职责边界，避免把数据准备、状态切换和视图渲染职责混杂到一起。
 */

#include "../screen/screen.h"
#include "../core/ui_engine.h"
#include "../core/vm/ui_vm_menu.h"
#include "../core/vm/ui_vm_menu_types.h"
#include "../core/ui_state.h"
#include "../screen/layout_view.h"
#include "../views/menu_defs.h"
#include "../views/submenu_types.h"
#include "lvgl/lvgl.h"
#include "../fonts/fonts.h"
#include <stdio.h>
#include <string.h>

void screen_register_setup_list(lv_obj_t *list);
void menu_entry_update(void);
void menu_setup_update(void);

/* DIVE MENU 顶层菜单页。
 * 它占用右侧 tileview 中的一个固定页面，但职责是设置入口和 badge 展示。
 */
#define SETUP_ITEM_COUNT SUBMENU_SETUP_COUNT
#define MENU_ENTRY_COUNT 4U
#define MENU_ENTRY_ITEM_H_U 7U  /* MENU HUB 双层入口高度 */
#define MENU_ENTRY_TITLE_Y 5    /* HUB 标题层Y */
#define MENU_ENTRY_PREVIEW_Y 38 /* HUB 预览层Y */
#define MENU_ENTRY_PREVIEW_H 24 /* HUB 预览层高度 */

const menu_list_cfg_t menu_setup_cfg =
{
    .items = g_menu_setup_items,
    .count = SETUP_ITEM_COUNT,
};

static lv_obj_t *s_list;
static ui_vm_menu_layout_t s_menu_layout_vm;
static lv_obj_t *s_setup_title_lbl;

/* badge 句柄数组：由动态菜单工厂输出的 item handles 填充。
 * child 0=title label，child 1=badge label。
 */
static lv_obj_t *s_setup_item_objs[SETUP_ITEM_COUNT];
static lv_obj_t *s_setup_badge_lbls[SETUP_ITEM_COUNT];
static lv_obj_t *s_menu_entry_list;
static lv_obj_t *s_menu_entry_items[MENU_ENTRY_COUNT];
static lv_obj_t *s_menu_entry_hint_lbl;
static uint8_t s_menu_entry_selected = 0xFFU;

static const menu_item_cfg_t s_menu_entry_cfg[] =
{
    { "INFO MENU", NULL, FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "DIVE MENU", NULL, FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "DEVICE CONTROL", NULL, FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
    { "END DIVE", NULL, FONT_ID_TITLE, FONT_ID_SMALL, 2, 0 },
};

static const char *const s_menu_entry_previews[] =
{
    "LAST DIVE / PLAN / TISSUE / LOG",
    "GAS / GF / DIVE / AI / DISPLAY",
    "BRIGHTNESS / COMPASS / LIGHT",
    "",
};

static bool menu_setup_obj_is_valid(lv_obj_t **obj_ref)
{
    if (obj_ref == NULL || *obj_ref == NULL)
    {
        return false;
    }

    if (!lv_obj_is_valid(*obj_ref))
    {
        *obj_ref = NULL;
        return false;
    }

    return true;
}

static void menu_setup_badge_set_text_if_changed(lv_obj_t *label, const char *text)
{
    const char *old_text;

    if (label == NULL || text == NULL)
    {
        return;
    }

    old_text = lv_label_get_text(label);
    if ((old_text == NULL) || (strcmp(old_text, text) != 0))
    {
        lv_label_set_text(label, text);
    }
}

static bool menu_entry_info_visible(void)
{
    dive_lifecycle_phase_t phase = bus_get_dive_lifecycle_phase();
    return (phase != DIVE_LIFECYCLE_ACTIVE) && (phase != DIVE_LIFECYCLE_SURFACING_PENDING);
}

static bool menu_entry_end_dive_visible(void)
{
    return bus_get_dive_lifecycle_phase() == DIVE_LIFECYCLE_SURFACING_PENDING;
}

static bool menu_entry_raw_visible(uint8_t raw_idx)
{
#if !ENABLE_INFO_MENU
    if (raw_idx == 0U) return false;
#endif
    if (raw_idx == 0U) return menu_entry_info_visible();
    if (raw_idx == 3U) return menu_entry_end_dive_visible();
    return raw_idx < MENU_ENTRY_COUNT;
}

static uint8_t menu_entry_raw_for_visible(uint8_t idx)
{
    uint8_t visible_idx = 0U;
    for (uint8_t raw_idx = 0U; raw_idx < MENU_ENTRY_COUNT; raw_idx++)
    {
        if (!menu_entry_raw_visible(raw_idx)) continue;
        if (visible_idx == idx) return raw_idx;
        visible_idx++;
    }
    return 1U;
}

static uint8_t menu_entry_visible_count(void)
{
    uint8_t count = 0U;
    for (uint8_t raw_idx = 0U; raw_idx < MENU_ENTRY_COUNT; raw_idx++)
    {
        if (menu_entry_raw_visible(raw_idx)) count++;
    }
    return count;
}

static void menu_entry_apply_row_style(lv_obj_t *item, lv_obj_t *label, bool selected)
{
    if (item == NULL || label == NULL) return;

    lv_obj_t *preview = lv_obj_get_child(item, 1);
    lv_obj_set_style_bg_color(item, BLACK, 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(item, selected ? GREEN : DARK, 0);
    lv_obj_set_style_border_width(item, selected ? (INNER_BORDER_W + 2) : INNER_BORDER_W, 0);
    lv_obj_set_style_text_color(label, selected ? LIGHT : GREEN, 0);
    lv_obj_set_style_text_font(label, get_font(selected ? FONT_ID_MEDIUM : FONT_ID_TITLE), 0);
    if (preview != NULL)
    {
        lv_obj_set_style_text_color(preview, selected ? LIGHT : GREEN, 0);
        lv_obj_set_style_text_font(preview, get_font(FONT_ID_SMALL), 0);
    }
}

void menu_entry_set_selection(uint8_t idx)
{
    uint8_t count = menu_entry_visible_count();
    if (count == 0U) return;
    if (idx >= count) idx = (uint8_t)(count - 1U);
    s_menu_entry_selected = idx;
    menu_entry_update();
}

void menu_entry_clear_selection(void)
{
    s_menu_entry_selected = 0xFFU;
    menu_entry_update();
}

uint8_t menu_entry_item_count(void)
{
    return menu_entry_visible_count();
}

bool menu_entry_selection_is_info(uint8_t idx)
{
#if ENABLE_INFO_MENU
    return menu_entry_raw_for_visible(idx) == 0U;
#else
    (void)idx;
    return false;
#endif
}

bool menu_entry_selection_is_device(uint8_t idx)
{
    return menu_entry_raw_for_visible(idx) == 2U;
}

bool menu_entry_selection_is_end_dive(uint8_t idx)
{
    return menu_entry_raw_for_visible(idx) == 3U;
}

static void menu_setup_render_title(lv_obj_t *parent)
{
    uint16_t right_w = ui_content_w_get();

    s_setup_title_lbl = lv_label_create(parent);
    lv_obj_remove_style_all(s_setup_title_lbl);
    lv_obj_set_pos(s_setup_title_lbl, 16, CARD_TITLE_TEXT_Y);
    lv_obj_set_size(s_setup_title_lbl, right_w - 32, CARD_TITLE_TEXT_H);
    lv_label_set_long_mode(s_setup_title_lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_setup_title_lbl, menu_defs_setup_root_title());
    lv_obj_set_style_text_font(s_setup_title_lbl, get_font(FONT_ID_TITLE), 0);
    lv_obj_set_style_text_color(s_setup_title_lbl, LIGHT, 0);

    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, right_w - 32, CARD_TITLE_LINE_H);
    lv_obj_set_pos(line, 16, CARD_TITLE_LINE_Y);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(line, DARK, 0);
}

static lv_obj_t *menu_entry_create_item(lv_obj_t *parent, const menu_item_cfg_t *cfg, const char *preview, int y, int w, int h)
{
    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_remove_style_all(item);
    lv_obj_set_pos(item, 0, y);
    lv_obj_set_size(item, w, h);
    lv_obj_set_style_bg_color(item, BLACK, 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(item, DARK, 0);
    lv_obj_set_style_border_width(item, INNER_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_radius(item, 0, 0);
    lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_lbl = lv_label_create(item);
    lv_label_set_text(title_lbl, cfg->title_text);
    lv_obj_set_style_text_font(title_lbl, get_font(cfg->title_font_id), 0);
    lv_obj_set_style_text_color(title_lbl, GREEN, 0);
    lv_obj_set_pos(title_lbl, 12, MENU_ENTRY_TITLE_Y);
    lv_obj_set_size(title_lbl, (lv_coord_t)(w - 24), 32);
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);

    lv_obj_t *preview_lbl = lv_label_create(item);
    lv_label_set_text(preview_lbl, preview);
    lv_obj_set_style_text_font(preview_lbl, get_font(FONT_ID_SMALL), 0);
    lv_obj_set_style_text_color(preview_lbl, GREEN, 0);
    lv_obj_set_pos(preview_lbl, 12, MENU_ENTRY_PREVIEW_Y);
    lv_obj_set_size(preview_lbl, (lv_coord_t)(w - 24), MENU_ENTRY_PREVIEW_H);
    lv_label_set_long_mode(preview_lbl, LV_LABEL_LONG_DOT);

    return item;
}

static void menu_entry_render_items(lv_obj_t *parent, int item_w, int item_h, int gap_y)
{
    int current_y = MENU_LIST_EDGE_PAD_PX;
    for (uint8_t i = 0U; i < MENU_ENTRY_COUNT; i++)
    {
        s_menu_entry_items[i] = menu_entry_create_item(parent, &s_menu_entry_cfg[i], s_menu_entry_previews[i], current_y, item_w, item_h);
        current_y += item_h + gap_y;
    }
}

void menu_entry_create(lv_obj_t *parent)
{
    ui_vm_menu_layout_update(&s_menu_layout_vm, NULL);
    int right_canvas_w = (int)s_menu_layout_vm.right_canvas_w;
    uint16_t item_h_px = MENU_ENTRY_ITEM_H_U * BASE_U;
    uint16_t gap_y_px = s_menu_layout_vm.gap_y_px;
    uint16_t list_h = MENU_ENTRY_COUNT * item_h_px + (MENU_ENTRY_COUNT - 1U) * gap_y_px;
    uint16_t list_y = (CARD_TITLE_H > MENU_LIST_TOP_NUDGE_PX) ? (uint16_t)(CARD_TITLE_H - MENU_LIST_TOP_NUDGE_PX) : CARD_TITLE_H;
    uint16_t visible_h = ui_content_h_get() > list_y ? (uint16_t)(ui_content_h_get() - list_y) : list_h;

    memset(s_menu_entry_items, 0, sizeof(s_menu_entry_items));
    render_card_title(parent, "MENU HUB");

    s_menu_entry_hint_lbl = lv_label_create(parent);
    lv_obj_remove_style_all(s_menu_entry_hint_lbl);
    lv_obj_set_pos(s_menu_entry_hint_lbl, 150, CARD_TITLE_TEXT_Y);
    lv_obj_set_size(s_menu_entry_hint_lbl, right_canvas_w > 166 ? right_canvas_w - 166 : right_canvas_w, CARD_TITLE_TEXT_H);
    lv_label_set_long_mode(s_menu_entry_hint_lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_menu_entry_hint_lbl, "[ ENTER ] select menu");
    lv_obj_set_style_text_font(s_menu_entry_hint_lbl, get_font(FONT_ID_SMALL), 0);
    lv_obj_set_style_text_color(s_menu_entry_hint_lbl, LIGHT, 0);

    s_menu_entry_list = lv_obj_create(parent);
    lv_obj_remove_style_all(s_menu_entry_list);
    lv_obj_set_size(s_menu_entry_list, right_canvas_w, visible_h);
    lv_obj_set_pos(s_menu_entry_list, 0, list_y);
    lv_obj_set_style_bg_opa(s_menu_entry_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_menu_entry_list, 0, 0);
    lv_obj_set_style_pad_all(s_menu_entry_list, 0, 0);
    lv_obj_set_style_pad_bottom(s_menu_entry_list, MENU_LIST_EDGE_PAD_PX, 0);
    lv_obj_add_flag(s_menu_entry_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_menu_entry_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_menu_entry_list, LV_SCROLLBAR_MODE_OFF);

    menu_entry_render_items(s_menu_entry_list, right_canvas_w - 15, item_h_px, gap_y_px);
    menu_entry_clear_selection();
}

void menu_entry_update(void)
{
    uint8_t count = menu_entry_visible_count();
    bool active = ui_state_get_state() == UI_MENU_ENTRY;

    if (s_menu_entry_hint_lbl != NULL && lv_obj_is_valid(s_menu_entry_hint_lbl))
    {
        if (active)
        {
            lv_obj_add_flag(s_menu_entry_hint_lbl, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_clear_flag(s_menu_entry_hint_lbl, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else
    {
        s_menu_entry_hint_lbl = NULL;
    }

    if (s_menu_entry_list == NULL || !lv_obj_is_valid(s_menu_entry_list))
    {
        s_menu_entry_list = NULL;
        memset(s_menu_entry_items, 0, sizeof(s_menu_entry_items));
        return;
    }
    for (uint8_t i = 0U; i < MENU_ENTRY_COUNT; i++)
    {
        if (s_menu_entry_items[i] != NULL && !lv_obj_is_valid(s_menu_entry_items[i])) s_menu_entry_items[i] = NULL;
    }
    if (s_menu_entry_items[1] == NULL) return;

    if (!active) s_menu_entry_selected = 0xFFU;
    if (active && s_menu_entry_selected >= count) s_menu_entry_selected = 0U;

    lv_coord_t current_y = MENU_LIST_EDGE_PAD_PX;
    uint8_t visible_idx = 0U;
    for (uint8_t raw_idx = 0U; raw_idx < MENU_ENTRY_COUNT; raw_idx++)
    {
        if (s_menu_entry_items[raw_idx] == NULL) continue;
        if (menu_entry_raw_visible(raw_idx))
        {
            lv_obj_clear_flag(s_menu_entry_items[raw_idx], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_y(s_menu_entry_items[raw_idx], current_y);
            menu_entry_apply_row_style(s_menu_entry_items[raw_idx], lv_obj_get_child(s_menu_entry_items[raw_idx], 0), active && s_menu_entry_selected == visible_idx);
            current_y += lv_obj_get_height(s_menu_entry_items[raw_idx]) + (lv_coord_t)ui_menu_gap_px_get();
            visible_idx++;
        }
        else
        {
            lv_obj_add_flag(s_menu_entry_items[raw_idx], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void menu_setup_create(lv_obj_t *parent)
{
    uint8_t setup_count = SETUP_ITEM_COUNT;
    const menu_item_cfg_t *setup_items = g_menu_setup_items;

    s_setup_title_lbl = NULL;
    memset(s_setup_item_objs, 0, sizeof(s_setup_item_objs));
    memset(s_setup_badge_lbls, 0, sizeof(s_setup_badge_lbls));
    menu_setup_render_title(parent);

    ui_vm_menu_layout_update(&s_menu_layout_vm, NULL);
    int right_canvas_w = (int)s_menu_layout_vm.right_canvas_w;

    uint16_t item_h_px = s_menu_layout_vm.item_h_px;
    uint16_t gap_y_px  = s_menu_layout_vm.gap_y_px;
    uint16_t list_h = setup_count * item_h_px
                      + (setup_count - 1) * gap_y_px;
    uint16_t list_y = (CARD_TITLE_H > MENU_LIST_TOP_NUDGE_PX) ? (uint16_t)(CARD_TITLE_H - MENU_LIST_TOP_NUDGE_PX) : CARD_TITLE_H;
    uint16_t visible_h = ui_content_h_get() > list_y ? (uint16_t)(ui_content_h_get() - list_y) : list_h;

    s_list = lv_obj_create(parent);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list, right_canvas_w, visible_h);
    lv_obj_set_pos(s_list, 0, list_y);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_style_pad_bottom(s_list, MENU_LIST_EDGE_PAD_PX, 0);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);

    /* 顶层菜单也走同一套菜单行渲染，减少重复维护。 */
    render_dynamic_menu(s_list, setup_items, setup_count, MENU_LIST_EDGE_PAD_PX, s_setup_item_objs);

    for (uint8_t i = 0; i < setup_count; i++)
    {
        s_setup_badge_lbls[i] = lv_obj_get_child(s_setup_item_objs[i], 1);
    }

    /* 首次创建后，立刻按当前系统配置刷新 badge。 */
    menu_setup_update();
    screen_register_setup_list(s_list);
    if (ui_state_get_state() == UI_SETUP)
    {
        screen_set_setup_selection(ui_state_get_menu_setup_idx());
    }
}

void menu_setup_update(void)
{
    ui_vm_setup_menu_t vm;
    uint8_t setup_count = 0;
    const menu_item_cfg_t *setup_items = menu_defs_setup_items(&setup_count);

    if (!menu_setup_obj_is_valid(&s_list)) return;

    static compass_cal_ui_state_t last_cal_state = COMPASS_CAL_IDLE;
    static const char *const cal_str[] = { "AUTO", "LEARN", "OK" };

    ui_vm_setup_menu_update(&vm);

    if (s_setup_title_lbl != NULL && lv_obj_is_valid(s_setup_title_lbl))
    {
        menu_setup_badge_set_text_if_changed(s_setup_title_lbl, menu_defs_setup_root_title());
    }
    else
    {
        s_setup_title_lbl = NULL;
    }

    for (uint8_t i = 0U; i < SETUP_ITEM_COUNT; i++)
    {
        if (!menu_setup_obj_is_valid(&s_setup_item_objs[i])) continue;
        if (i < setup_count)
        {
            lv_obj_t *label = lv_obj_get_child(s_setup_item_objs[i], 0);
            lv_obj_clear_flag(s_setup_item_objs[i], LV_OBJ_FLAG_HIDDEN);
            if (label != NULL) menu_setup_badge_set_text_if_changed(label, setup_items[i].title_text);
        }
        else
        {
            lv_obj_add_flag(s_setup_item_objs[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (uint8_t i = 0U; i < SETUP_ITEM_COUNT; i++)
    {
        if (!menu_setup_obj_is_valid(&s_setup_badge_lbls[i])) continue;
        menu_setup_badge_set_text_if_changed(s_setup_badge_lbls[i], "");
        lv_obj_add_flag(s_setup_badge_lbls[i], LV_OBJ_FLAG_HIDDEN);
    }

    if (menu_defs_get_setup_root() == MENU_SETUP_ROOT_DIVE)
    {
        if (menu_setup_obj_is_valid(&s_setup_badge_lbls[1]))
        {
            lv_obj_clear_flag(s_setup_badge_lbls[1], LV_OBJ_FLAG_HIDDEN);
            menu_setup_badge_set_text_if_changed(s_setup_badge_lbls[1], vm.conservatism_badge);
        }
    }
    else
    {
        if (menu_setup_obj_is_valid(&s_setup_badge_lbls[0]))
        {
            lv_obj_clear_flag(s_setup_badge_lbls[0], LV_OBJ_FLAG_HIDDEN);
            menu_setup_badge_set_text_if_changed(s_setup_badge_lbls[0], vm.brightness_badge);
        }
        if (menu_setup_obj_is_valid(&s_setup_badge_lbls[1]))
        {
            uint8_t idx = (vm.compass_cal_badge_idx <= 2U) ? vm.compass_cal_badge_idx : 0U;
            lv_obj_clear_flag(s_setup_badge_lbls[1], LV_OBJ_FLAG_HIDDEN);
            menu_setup_badge_set_text_if_changed(s_setup_badge_lbls[1], cal_str[idx]);
        }
    }
    if (vm.compass_cal_state != last_cal_state)
    {
        last_cal_state = vm.compass_cal_state;
        screen_refresh_compass_cal_submenu_if_open();
    }
}
