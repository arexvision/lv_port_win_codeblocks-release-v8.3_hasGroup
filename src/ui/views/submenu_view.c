#include "submenu_view.h"

#include "../core/callbacks.h"
#include "../screen/screen.h"
#include "menu_actions.h"
#include "menu_runtime.h"
#include "submenu_model.h"
#include "../core/ui_state.h"
#include "../fonts/fonts.h"

static lv_obj_t *s_submenu_layer = NULL;
static lv_obj_t *s_submenu_title = NULL;
static lv_obj_t *s_submenu_title_line = NULL;
static lv_obj_t *s_submenu_list = NULL;
static lv_obj_t *s_light_status_lbl = NULL;
static uint16_t s_submenu_width = 0;
static uint16_t s_submenu_height = 0;

static bool submenu_is_dive_plan_visible(void)
{
    return menu_runtime_is_dive_plan();
}

void submenu_view_reset(void)
{
    s_submenu_layer = NULL;
    s_submenu_title = NULL;
    s_submenu_title_line = NULL;
    s_submenu_list = NULL;
    s_light_status_lbl = NULL;
    s_submenu_width = 0;
    s_submenu_height = 0;
    menu_runtime_reset();
    menu_actions_clear_pending();
}

lv_obj_t *submenu_view_get_list(void)
{
    return s_submenu_list;
}

void submenu_view_create(lv_obj_t *parent, uint16_t width, uint16_t height)
{
    s_submenu_width = width;
    s_submenu_height = height;

    s_submenu_layer = lv_obj_create(parent);
    lv_obj_set_size(s_submenu_layer, s_submenu_width, s_submenu_height);
    lv_obj_set_pos(s_submenu_layer, s_submenu_width, 0);
    lv_obj_set_style_bg_color(s_submenu_layer, BLACK, 0);
    lv_obj_set_style_bg_opa(s_submenu_layer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_submenu_layer, 0, 0);
    lv_obj_set_style_pad_all(s_submenu_layer, 0, 0);
    lv_obj_clear_flag(s_submenu_layer, LV_OBJ_FLAG_SCROLLABLE);

    s_submenu_title = lv_label_create(s_submenu_layer);
    lv_obj_remove_style_all(s_submenu_title);
    lv_obj_set_pos(s_submenu_title, 16, 8);
    lv_obj_set_size(s_submenu_title, s_submenu_width - 32, 40);
    lv_label_set_long_mode(s_submenu_title, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_submenu_title, "> SUB MENU");
    lv_obj_set_style_text_color(s_submenu_title, LIGHT, 0);
    lv_obj_set_style_text_font(s_submenu_title, get_font(FONT_ID_TITLE), 0);

    s_submenu_title_line = lv_obj_create(s_submenu_layer);
    lv_obj_remove_style_all(s_submenu_title_line);
    lv_obj_set_size(s_submenu_title_line, s_submenu_width - 32, 2);
    lv_obj_set_pos(s_submenu_title_line, 16, 48);
    lv_obj_set_style_bg_color(s_submenu_title_line, DARK, 0);
    lv_obj_set_style_bg_opa(s_submenu_title_line, LV_OPA_COVER, 0);

    s_submenu_list = lv_obj_create(s_submenu_layer);
    lv_obj_set_size(s_submenu_list, s_submenu_width - 15, s_submenu_height - CARD_TITLE_H - 10);
    lv_obj_set_pos(s_submenu_list, 0, CARD_TITLE_H);
    lv_obj_set_style_bg_opa(s_submenu_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_submenu_list, 0, 0);
    lv_obj_set_style_pad_all(s_submenu_list, 0, 0);
    lv_obj_clear_flag(s_submenu_list, LV_OBJ_FLAG_SCROLLABLE);
}

/* =========================================================
 * Sub-menu layer
 * ========================================================= */
static void submenu_slide_in(void)
{
    if (!s_submenu_layer) return;
    uint16_t right_w_fallback = g_sys_config.safe_zone_w - LEFT_ANCHOR_W
                                - g_sys_config.panel_gap_u * BASE_U;
    uint16_t slide_w = s_submenu_width > 0 ? s_submenu_width : right_w_fallback;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_submenu_layer);
    lv_anim_set_values(&a, slide_w, 0);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 250);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void submenu_slide_out(void)
{
    if (!s_submenu_layer) return;
    uint16_t right_w_fallback = g_sys_config.safe_zone_w - LEFT_ANCHOR_W
                                - g_sys_config.panel_gap_u * BASE_U;
    uint16_t slide_w = s_submenu_width > 0 ? s_submenu_width : right_w_fallback;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_submenu_layer);
    lv_anim_set_values(&a, 0, slide_w);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 250);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);
}

static lv_obj_t *plan_make_label(lv_obj_t *parent,
                                 const char *text,
                                 uint8_t font_id,
                                 lv_color_t color,
                                 int x,
                                 int y,
                                 int w,
                                 int h,
                                 lv_text_align_t align)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_remove_style_all(lbl);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_size(lbl, w, h);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_font(lbl, get_font(font_id), 0);
    lv_obj_set_style_text_align(lbl, align, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl, text ? text : "");
    return lbl;
}

static lv_obj_t *plan_make_button(lv_obj_t *parent, const char *text, int x, int y)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, 80, 28);
    lv_obj_set_style_bg_color(btn, BLACK, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, GREEN, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = plan_make_label(btn,
                                    text,
                                    FONT_ID_SMALL,
                                    LIGHT,
                                    0,
                                    3,
                                    80,
                                    22,
                                    LV_TEXT_ALIGN_CENTER);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    return btn;
}

static void plan_draw_header(lv_obj_t *parent, int w)
{
    float depth_m = 0.0f;
    float rmv_lpm = 0.0f;
    uint16_t time_min = 0U;
    char buf[24];
    submenu_dive_plan_get_inputs(&depth_m, &time_min, &rmv_lpm);

    lv_obj_t *oc = lv_obj_create(parent);
    lv_obj_remove_style_all(oc);
    lv_obj_set_pos(oc, 12, 8);
    lv_obj_set_size(oc, 30, 42);
    lv_obj_set_style_bg_color(oc, BLACK, 0);
    lv_obj_set_style_bg_opa(oc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(oc, GREEN, 0);
    lv_obj_set_style_border_width(oc, 2, 0);
    plan_make_label(oc, "OC", FONT_ID_SMALL, LIGHT, 0, 9, 30, 22, LV_TEXT_ALIGN_CENTER);

    plan_make_label(parent, "DEPTH", FONT_ID_SMALL, GREEN, 70, 12, 70, 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "TIME", FONT_ID_SMALL, GREEN, 155, 12, 70, 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "RMV", FONT_ID_SMALL, GREEN, 240, 12, 70, 18, LV_TEXT_ALIGN_CENTER);

    dive_plan_page_t page = submenu_dive_plan_page();
    lv_snprintf(buf, sizeof(buf), "%u", (unsigned)(depth_m + 0.5f));
    plan_make_label(parent,
                    (page == DIVE_PLAN_PAGE_DEPTH) ? "---" : buf,
                    FONT_ID_SMALL,
                    LIGHT,
                    70,
                    34,
                    70,
                    18,
                    LV_TEXT_ALIGN_CENTER);
    lv_snprintf(buf, sizeof(buf), "%u", (unsigned)time_min);
    plan_make_label(parent,
                    (page <= DIVE_PLAN_PAGE_TIME) ? "--" : buf,
                    FONT_ID_SMALL,
                    LIGHT,
                    155,
                    34,
                    70,
                    18,
                    LV_TEXT_ALIGN_CENTER);
    lv_snprintf(buf, sizeof(buf), "%u", (unsigned)(rmv_lpm + 0.5f));
    plan_make_label(parent,
                    (page <= DIVE_PLAN_PAGE_RMV) ? "--" : buf,
                    FONT_ID_SMALL,
                    LIGHT,
                    240,
                    34,
                    70,
                    18,
                    LV_TEXT_ALIGN_CENTER);

    uint8_t header_o2 = submenu_dive_plan_header_gas_o2();
    if (header_o2)
    {
        lv_snprintf(buf, sizeof(buf), "%u", (unsigned)header_o2);
    }
    else
    {
        lv_snprintf(buf, sizeof(buf), "---");
    }
    plan_make_label(parent, buf, FONT_ID_SMALL, LIGHT, w - 74, 12, 54, 18, LV_TEXT_ALIGN_CENTER);
}

static void plan_draw_bottom_line(lv_obj_t *parent, int w)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, 0, (int)s_submenu_height - 42);
    lv_obj_set_size(line, w, 1);
    lv_obj_set_style_bg_color(line, GREEN, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
}

static void plan_draw_input(lv_obj_t *parent, int w)
{
    float depth_m = 0.0f;
    float rmv_lpm = 0.0f;
    uint16_t time_min = 0U;
    char buf[48];
    const char *prompt = "Enter Bottom Depth";
    const char *unit = "in meters";
    uint16_t min_v = 3U;
    uint16_t max_v = 120U;
    uint16_t value = 30U;
    dive_plan_page_t page = submenu_dive_plan_page();

    submenu_dive_plan_get_inputs(&depth_m, &time_min, &rmv_lpm);
    if (page == DIVE_PLAN_PAGE_TIME)
    {
        prompt = "Enter Bottom Time";
        unit = "in minutes";
        min_v = 1U;
        max_v = 300U;
        value = time_min;
    }
    else if (page == DIVE_PLAN_PAGE_RMV)
    {
        prompt = "Enter RMV";
        unit = "in Liters/min";
        min_v = 5U;
        max_v = 50U;
        value = (uint16_t)(rmv_lpm + 0.5f);
    }
    else
    {
        value = (uint16_t)(depth_m + 0.5f);
    }

    lv_snprintf(buf, sizeof(buf), "%u", (unsigned)value);
    plan_make_label(parent, buf, FONT_ID_MEDIUM, LIGHT, 0, 98, w, 42, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *underline = lv_obj_create(parent);
    lv_obj_remove_style_all(underline);
    lv_obj_set_pos(underline, (w - 38) / 2, 132);
    lv_obj_set_size(underline, 38, 4);
    lv_obj_set_style_bg_color(underline, lv_color_make(255, 255, 0), 0);
    lv_obj_set_style_bg_opa(underline, LV_OPA_COVER, 0);

    plan_make_label(parent, prompt, FONT_ID_SMALL, LIGHT, 0, 166, w, 22, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, unit, FONT_ID_SMALL, LIGHT, 0, 190, w, 22, LV_TEXT_ALIGN_CENTER);
    lv_snprintf(buf, sizeof(buf), "MIN: %u", (unsigned)min_v);
    plan_make_label(parent, buf, FONT_ID_SMALL, LIGHT, 0, 224, w, 18, LV_TEXT_ALIGN_CENTER);
    lv_snprintf(buf, sizeof(buf), "MAX: %u", (unsigned)max_v);
    plan_make_label(parent, buf, FONT_ID_SMALL, LIGHT, 0, 246, w, 18, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *spin = lv_obj_create(parent);
    lv_obj_remove_style_all(spin);
    lv_obj_set_pos(spin, (w - 100) / 2, 276);
    lv_obj_set_size(spin, 100, 25);
    lv_obj_set_style_bg_color(spin, LIGHT, 0);
    lv_obj_set_style_bg_opa(spin, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(spin, DARK, 0);
    lv_obj_set_style_border_width(spin, 1, 0);
    lv_snprintf(buf, sizeof(buf), "%u", (unsigned)value);
    plan_make_label(spin, buf, FONT_ID_SMALL, BLACK, 6, 2, 78, 20, LV_TEXT_ALIGN_RIGHT);
    plan_make_label(spin, "^", FONT_ID_SMALL, DARK, 84, 0, 14, 12, LV_TEXT_ALIGN_CENTER);
    plan_make_label(spin, "v", FONT_ID_SMALL, DARK, 84, 12, 14, 12, LV_TEXT_ALIGN_CENTER);
}

static void plan_draw_ready(lv_obj_t *parent, int w)
{
    char buf[48];
    plan_make_label(parent, "Ready to Plan Dive", FONT_ID_SMALL, LIGHT, 0, 106, w, 24, LV_TEXT_ALIGN_CENTER);
    lv_snprintf(buf,
                sizeof(buf),
                "GF:              %u/%u",
                (unsigned)submenu_dive_plan_gf_low(),
                (unsigned)submenu_dive_plan_gf_high());
    plan_make_label(parent, buf, FONT_ID_SMALL, LIGHT, 96, 168, w - 192, 22, LV_TEXT_ALIGN_LEFT);
    lv_snprintf(buf,
                sizeof(buf),
                "Last Stop:       %um",
                (unsigned)submenu_dive_plan_last_stop_m());
    plan_make_label(parent, buf, FONT_ID_SMALL, LIGHT, 96, 200, w - 192, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, "Start CNS:       0%", FONT_ID_SMALL, LIGHT, 96, 232, w - 192, 22, LV_TEXT_ALIGN_LEFT);
}

static const char *plan_time_text(const dive_plan_row_t *row, char *buf, size_t buf_size)
{
    if (row->type == DIVE_PLAN_ROW_BOTTOM)
    {
        return "bot";
    }
    if (row->type == DIVE_PLAN_ROW_ASCENT)
    {
        return "asc";
    }
    lv_snprintf(buf, buf_size, "%u", (unsigned)row->time_min);
    return buf;
}

static void plan_draw_result(lv_obj_t *parent, int w)
{
    char buf[16];
    static const int col_x[] = { 20, 88, 166, 244, 334 };
    static const int col_w[] = { 64, 72, 72, 82, 72 };
    uint8_t page = submenu_dive_plan_result_page_index();
    uint8_t total_pages = submenu_dive_plan_result_total_pages();
    uint8_t entry_count = submenu_dive_plan_result_entry_count();
    uint8_t start = (uint8_t)(page * 8U);
    uint8_t end = (uint8_t)(start + 8U);
    if (end > entry_count) end = entry_count;

    plan_make_label(parent, "Stp", FONT_ID_SMALL, GREEN, col_x[0], 68, col_w[0], 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "Tme", FONT_ID_SMALL, GREEN, col_x[1], 68, col_w[1], 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "Run", FONT_ID_SMALL, GREEN, col_x[2], 68, col_w[2], 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "Gas", FONT_ID_SMALL, GREEN, col_x[3], 68, col_w[3], 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "Qty", FONT_ID_SMALL, GREEN, col_x[4], 68, col_w[4], 18, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, 16, 88);
    lv_obj_set_size(line, w - 32, 1);
    lv_obj_set_style_bg_color(line, GREEN, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);

    int y = 100;
    for (uint8_t i = start; i < end; i++)
    {
        dive_plan_row_t row;
        char tme[8];
        if (!submenu_dive_plan_result_row(i, &row))
        {
            continue;
        }
        const char *tme_text = plan_time_text(&row, tme, sizeof(tme));
        lv_snprintf(buf, sizeof(buf), "%dm", (int)row.depth_m);
        plan_make_label(parent, buf, FONT_ID_SMALL, LIGHT, col_x[0], y, col_w[0], 18, LV_TEXT_ALIGN_CENTER);
        plan_make_label(parent, tme_text, FONT_ID_SMALL, LIGHT, col_x[1], y, col_w[1], 18, LV_TEXT_ALIGN_CENTER);
        lv_snprintf(buf, sizeof(buf), "%u", (unsigned)row.run_min);
        plan_make_label(parent, buf, FONT_ID_SMALL, LIGHT, col_x[2], y, col_w[2], 18, LV_TEXT_ALIGN_CENTER);
        lv_snprintf(buf, sizeof(buf), "%02u/%02u", (unsigned)row.o2_pct, (unsigned)row.he_pct);
        plan_make_label(parent, buf, FONT_ID_SMALL, LIGHT, col_x[3], y, col_w[3], 18, LV_TEXT_ALIGN_CENTER);
        lv_snprintf(buf, sizeof(buf), "%u", (unsigned)row.gas_l);
        plan_make_label(parent, buf, FONT_ID_SMALL, LIGHT, col_x[4], y, col_w[4], 18, LV_TEXT_ALIGN_RIGHT);
        y += 26;
    }

    lv_snprintf(buf, sizeof(buf), "Page %u/%u", (unsigned)(page + 1U), (unsigned)total_pages);
    plan_make_label(parent, buf, FONT_ID_SMALL, GREEN, 0, (int)s_submenu_height - 86, w, 18, LV_TEXT_ALIGN_CENTER);
}

static void plan_draw_error(lv_obj_t *parent, int w)
{
    plan_make_label(parent, "Plan Failed", FONT_ID_MEDIUM, LIGHT, 0, 118, w, 40, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "Check depth, time, RMV and gas setup",
                    FONT_ID_SMALL, LIGHT, 0, 176, w, 24, LV_TEXT_ALIGN_CENTER);
}

static void submenu_populate_dive_plan(const menu_row_t *rows, uint8_t count)
{
    if (!s_submenu_list) return;

    int w = (s_submenu_width > 0)
            ? (int)s_submenu_width
            : (int)(g_sys_config.safe_zone_w - LEFT_ANCHOR_W - g_sys_config.panel_gap_u * BASE_U);

    lv_obj_clean(s_submenu_list);
    s_light_status_lbl = NULL;

    if (count > 0U)
    {
        (void)plan_make_button(s_submenu_list, rows[0].label, 12, (int)s_submenu_height - 34);
    }
    if (count > 1U)
    {
        (void)plan_make_button(s_submenu_list, rows[1].label, w - 92, (int)s_submenu_height - 34);
    }

    plan_draw_header(s_submenu_list, w);
    switch (submenu_dive_plan_page())
    {
    case DIVE_PLAN_PAGE_READY:
        plan_draw_ready(s_submenu_list, w);
        break;
    case DIVE_PLAN_PAGE_RESULT:
        plan_draw_result(s_submenu_list, w);
        break;
    case DIVE_PLAN_PAGE_ERROR:
        plan_draw_error(s_submenu_list, w);
        break;
    case DIVE_PLAN_PAGE_DEPTH:
    case DIVE_PLAN_PAGE_TIME:
    case DIVE_PLAN_PAGE_RMV:
    default:
        plan_draw_input(s_submenu_list, w);
        break;
    }
    plan_draw_bottom_line(s_submenu_list, w);
    screen_set_submenu_selection((count > 1U) ? 1U : 0U);
}

static void submenu_populate(const char *title, const menu_row_t *rows, uint8_t count)
{
    if (!s_submenu_title || !s_submenu_list) return;

    bool is_dive_plan = menu_runtime_is_dive_plan();
    if (is_dive_plan)
    {
        lv_label_set_text(s_submenu_title, "DIVE PLAN");
        lv_obj_add_flag(s_submenu_title, LV_OBJ_FLAG_HIDDEN);
        if (s_submenu_title_line)
        {
            lv_obj_add_flag(s_submenu_title_line, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_set_pos(s_submenu_list, 0, 0);
        lv_obj_set_size(s_submenu_list, s_submenu_width, s_submenu_height);
        submenu_populate_dive_plan(rows, count);
        return;
    }

    lv_obj_clear_flag(s_submenu_title, LV_OBJ_FLAG_HIDDEN);
    if (s_submenu_title_line)
    {
        lv_obj_clear_flag(s_submenu_title_line, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_size(s_submenu_list, s_submenu_width - 15, s_submenu_height - CARD_TITLE_H - 10);
    lv_obj_set_pos(s_submenu_list, 0, CARD_TITLE_H);

    if (menu_runtime_is_nested())
    {
        char nested_title[48];
        lv_snprintf(nested_title, sizeof(nested_title), "> %s", title ? title : "");
        lv_label_set_text(s_submenu_title, nested_title);
    }
    else
    {
        lv_label_set_text(s_submenu_title, title);
    }
    lv_obj_clean(s_submenu_list);
    s_light_status_lbl = NULL;  /* 重置 LIGHT 状态标签 */

    /* right_w 从缓存读取，fallback = safe_zone_w - left_anchor_w - panel_gap */
    uint16_t right_w = (s_submenu_width > 0)
                       ? s_submenu_width
                       : (g_sys_config.safe_zone_w - LEFT_ANCHOR_W - g_sys_config.panel_gap_u * BASE_U);
    uint16_t sub_w = right_w;
    int item_h = (int)(g_sys_config.h_menu_item * BASE_U);  /* 5U=50px */
    int item_w = (int)sub_w - 15;
    int gap_y  = (int)(g_sys_config.gap_menu * BASE_U);   /* 1U=10px */
    int current_y = 0;
    bool compact_plan = menu_runtime_is_dive_plan_result();
    if (compact_plan)
    {
        item_h = 24;
        gap_y = 2;
    }

    for (uint8_t i = 0; i < count; i++)
    {
        lv_obj_t *item = lv_obj_create(s_submenu_list);
        lv_obj_set_pos(item, 0, current_y);
        lv_obj_set_size(item, item_w, item_h);
        lv_obj_set_style_bg_color(item, BLACK, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, DARK, 0);
        lv_obj_set_style_border_width(item, INNER_BORDER_W, LV_PART_MAIN);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        /* LIGHT CONTROL 特殊布局: LIGHT 左侧，ON/OFF 右侧 */
        if (rows[i].type == MENU_ROW_LIGHT_POWER)
        {
            /* "LIGHT" 标签在左侧 */
            lv_obj_t *lbl_light = lv_label_create(item);
            lv_obj_set_style_text_color(lbl_light, GREEN, 0);
            lv_obj_set_style_text_font(lbl_light, get_font(FONT_ID_TITLE), 0);
            lv_obj_set_size(lbl_light, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(lbl_light, LV_ALIGN_LEFT_MID, 12, 0);
            lv_obj_set_style_text_align(lbl_light, LV_TEXT_ALIGN_LEFT, 0);
            lv_label_set_text(lbl_light, "LIGHT");

            /* "ON"/"OFF" 标签在右侧 */
            lv_obj_t *lbl_status = lv_label_create(item);
            lv_obj_set_style_text_color(lbl_status, g_light_power_state ? GREEN : LIGHT, 0);
            lv_obj_set_style_text_font(lbl_status, get_font(FONT_ID_TITLE), 0);
            lv_obj_set_size(lbl_status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(lbl_status, LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(lbl_status, g_light_power_state ? "ON" : "OFF");

            /* 保存状态标签引用，用于点击时更新 */
            s_light_status_lbl = lbl_status;
            current_y += item_h + gap_y;
            continue;
        }

        /* 普通菜单项 */
        lv_obj_t *lbl = lv_label_create(item);
        lv_obj_set_style_text_color(lbl, GREEN, 0);
        lv_obj_set_style_text_font(lbl, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_TITLE), 0);
        lv_obj_set_size(lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_label_set_text(lbl, rows[i].label ? rows[i].label : "");

        current_y += item_h + gap_y;
    }
    screen_set_submenu_selection(0);
}

static void submenu_populate_current(void)
{
    uint8_t count = 0;
    const menu_row_t *rows = menu_runtime_current_rows(&count);
    submenu_populate(menu_runtime_current_title(), rows, count);
    g_ui.sub_item_count = count;
}

void screen_set_submenu_selection(uint8_t idx)
{
    if (!s_submenu_list) return;
    if (submenu_is_dive_plan_visible())
    {
        uint32_t cnt = lv_obj_get_child_cnt(s_submenu_list);
        uint32_t action_count = g_ui.sub_item_count;
        if (action_count > cnt)
        {
            action_count = cnt;
        }
        for (uint32_t i = 0; i < action_count; i++)
        {
            lv_obj_t *item = lv_obj_get_child(s_submenu_list, i);
            lv_obj_t *lbl = item ? lv_obj_get_child(item, 0) : NULL;
            if (!item) continue;
            if (i == idx)
            {
                lv_obj_set_style_border_color(item, LIGHT, 0);
                lv_obj_set_style_border_width(item, 2, 0);
                if (lbl) lv_obj_set_style_text_color(lbl, LIGHT, 0);
            }
            else
            {
                lv_obj_set_style_border_color(item, GREEN, 0);
                lv_obj_set_style_border_width(item, 1, 0);
                if (lbl) lv_obj_set_style_text_color(lbl, LIGHT, 0);
            }
        }
        return;
    }

    bool compact_plan = menu_runtime_is_dive_plan_result();
    uint32_t cnt = lv_obj_get_child_cnt(s_submenu_list);
    for (uint32_t i = 0; i < cnt; i++)
    {
        lv_obj_t *item = lv_obj_get_child(s_submenu_list, i);
        lv_obj_t *lbl  = lv_obj_get_child(item, 0);
        /* 正在编辑的 item 由 begin_edit_value 单独管理，不参与选中态刷新 */
        if (g_ui.edit_ctx.active && (uint8_t)i == g_ui.edit_ctx.item_index) continue;
        if (i == idx)
        {
            lv_obj_add_state(item, LV_STATE_FOCUSED);  // HOTFIX: Clear LVGL states to fix bold residue.
            lv_obj_set_style_bg_color(item, BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, GREEN, 0);
            lv_obj_set_style_border_width(item, INNER_BORDER_W + 2, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, LIGHT, 0);
                lv_obj_set_style_text_font(lbl, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_MEDIUM), 0);
            }
            /* LIGHT CONTROL second column uses the same selected emphasis. */
            lv_obj_t *lbl2 = lv_obj_get_child(item, 1);
            if (lbl2)
            {
                lv_obj_set_style_text_color(lbl2, LIGHT, 0);
                lv_obj_set_style_text_font(lbl2, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_MEDIUM), 0);
            }
        }
        else
        {
            lv_obj_clear_state(item, LV_STATE_FOCUSED | LV_STATE_EDITED | LV_STATE_CHECKED);  // HOTFIX: Clear LVGL states to fix bold residue.
            if (lbl) lv_obj_clear_state(lbl, LV_STATE_ANY);  // HOTFIX: Clear LVGL states to fix bold residue.
            lv_obj_set_style_bg_color(item, BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, DARK, 0);
            lv_obj_set_style_border_width(item, INNER_BORDER_W, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, GREEN, 0);
                lv_obj_set_style_text_font(lbl, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_TITLE), 0);
            }
            /* LIGHT CONTROL 特殊处理：第二列（ON/OFF）恢复状态色 */
            lv_obj_t *lbl2 = lv_obj_get_child(item, 1);
            if (lbl2)
            {
                lv_obj_set_style_text_color(lbl2, g_light_power_state ? GREEN : LIGHT, 0);
                lv_obj_set_style_text_font(lbl2, get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_TITLE), 0);
            }
        }
    }
}

/* INFO sub-menu */
void screen_open_info_submenu(uint8_t item_idx)
{
    uint8_t count = 0;
    if (!menu_runtime_open_info(item_idx))
    {
        return;
    }
    if (menu_runtime_is_dive_plan())
    {
        submenu_dive_plan_reset();
        menu_runtime_refresh();
    }

    submenu_populate_current();
    (void)menu_runtime_current_rows(&count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = menu_runtime_default_selection();
    g_ui.sub_parent     = UI_INFO;
    g_ui.state          = UI_SUB_MENU;
    g_ui.sub_history_depth = 0;
    screen_set_submenu_selection(g_ui.sub_menu_idx);
    submenu_slide_in();
}

static void refresh_info_submenu_page(uint8_t keep_idx)
{
    uint8_t count = 0;
    menu_runtime_refresh();
    (void)menu_runtime_current_rows(&count);
    if (count == 0)
    {
        return;
    }

    submenu_populate_current();
    g_ui.sub_item_count = count;
    if (keep_idx >= count)
    {
        keep_idx = (uint8_t)(count - 1U);
    }
    g_ui.sub_menu_idx = keep_idx;
    screen_set_submenu_selection(keep_idx);
}

void screen_refresh_info_submenu_if_open(void)
{
    if (!s_submenu_title || !s_submenu_list)
    {
        return;
    }
    if (g_ui.state != UI_SUB_MENU || g_ui.sub_parent != UI_INFO || g_ui.sub_history_depth != 0)
    {
        return;
    }

    refresh_info_submenu_page(g_ui.sub_menu_idx);
}

bool screen_handle_dive_plan_rotate(int8_t dir)
{
    if (g_ui.state != UI_SUB_MENU || g_ui.sub_parent != UI_INFO || !submenu_is_dive_plan_visible())
    {
        return false;
    }
    if (!submenu_dive_plan_handle_rotate(dir))
    {
        return false;
    }
    refresh_info_submenu_page(1U);
    return true;
}

static bool refresh_compass_cal_submenu(void)
{
    uint8_t count = 0;
    if (!s_submenu_list || !s_submenu_title)
    {
        return false;
    }

    if (menu_runtime_current_id() != MENU_SETUP_COMPASS_CAL)
    {
        return false;
    }

    menu_runtime_refresh();
    (void)menu_runtime_current_rows(&count);
    if (count == 0)
    {
        return false;
    }
    submenu_populate_current();
    g_ui.sub_item_count = count;
    if (g_ui.sub_menu_idx >= count)
    {
        g_ui.sub_menu_idx = count - 1;
    }
    screen_set_submenu_selection(g_ui.sub_menu_idx);
    return true;
}

void screen_open_setup_submenu(uint8_t item_idx)
{
    uint8_t count = 0;
    if (!menu_runtime_open_setup(item_idx))
    {
        return;
    }

    submenu_populate_current();
    (void)menu_runtime_current_rows(&count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.sub_parent     = UI_SETUP;
    g_ui.state          = UI_SUB_MENU;
    g_ui.sub_history_depth = 0;
    submenu_slide_in();
}

void screen_refresh_compass_cal_submenu_if_open(void)
{
    (void)refresh_compass_cal_submenu();
}

void screen_open_nested_submenu(const char *title, const char **items, uint8_t count)
{
    (void)title;
    (void)items;
    (void)count;
    submenu_populate_current();
    (void)menu_runtime_current_rows(&count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.state = UI_SUB_MENU;
}

static void refresh_current_submenu_page(uint8_t keep_idx)
{
    uint8_t count = 0;
    menu_runtime_refresh();
    (void)menu_runtime_current_rows(&count);
    if (count == 0)
    {
        return;
    }

    submenu_populate_current();
    g_ui.sub_item_count = count;
    if (keep_idx >= count)
    {
        keep_idx = (uint8_t)(count - 1);
    }
    g_ui.sub_menu_idx = keep_idx;
    screen_set_submenu_selection(keep_idx);
}

void screen_handle_submenu_select(uint8_t item_idx)
{
    menu_action_t action;
    const menu_row_t *row;
    if (!s_submenu_list || !s_submenu_title) return;
    if (item_idx >= g_ui.sub_item_count) return;
    row = menu_runtime_row_at(item_idx);
    if (!menu_actions_handle_select(item_idx, row, &action))
    {
        return;
    }

    switch (action.type)
    {
    case MENU_ACTION_BACK:
    case MENU_ACTION_CLOSE:
        screen_close_submenu();
        break;
    case MENU_ACTION_OPEN_CHILD:
        if (menu_runtime_open_child(action.child_menu, row->id))
        {
            submenu_populate_current();
            g_ui.sub_menu_idx = 0;
            screen_set_submenu_selection(g_ui.sub_menu_idx);
        }
        break;
    case MENU_ACTION_REFRESH:
        refresh_current_submenu_page(action.keep_index);
        break;
    case MENU_ACTION_SHOW_CONFIRM:
        screen_show_modal_setup_confirm(action.modal_text);
        g_ui.state = UI_MODAL_SETUP_CONFIRM;
        break;
    case MENU_ACTION_BEGIN_EDIT:
        screen_begin_edit_value(item_idx, &action.edit_spec);
        break;
    case MENU_ACTION_SHOW_GAS_MODAL:
        screen_show_modal_gas();
        g_ui.state = UI_MODAL_GAS;
        break;
    case MENU_ACTION_SHOW_TEXT_MODAL:
        screen_show_modal_act(action.modal_text);
        break;
    default:
        break;
    }
}

void screen_close_submenu(void)
{
    if (!s_submenu_layer || !s_submenu_title || !s_submenu_list)
    {
        g_ui.sub_history_depth = 0;
        g_ui.sub_item_count = 0;
        g_ui.sub_menu_idx = 0;
        g_ui.edit_ctx.active = false;
        g_ui.gas_modal_from_submenu = false;
        g_ui.state = g_ui.sub_parent;
        return;
    }

    if (menu_runtime_back())
    {
        uint8_t count = 0;
        (void)menu_runtime_current_rows(&count);
        if (count > 0)
        {
            submenu_populate_current();
            g_ui.sub_item_count = count;
            if (g_ui.sub_menu_idx >= count)
            {
                g_ui.sub_menu_idx = count - 1;
            }
            screen_set_submenu_selection(g_ui.sub_menu_idx);
        }
        g_ui.state = UI_SUB_MENU;
        return;
    }
    submenu_slide_out();
    menu_runtime_reset();
    menu_actions_clear_pending();
    g_ui.sub_history_depth = 0;
    g_ui.sub_item_count = 0;
    g_ui.sub_menu_idx = 0;
    g_ui.state = g_ui.sub_parent;
}

void screen_confirm_submenu_setting(void)
{
    bool close_extra_mode_layer = false;
    bool return_dash_after_apply = false;
    if (!menu_actions_confirm_pending(&close_extra_mode_layer, &return_dash_after_apply))
    {
        screen_hide_modal();
        g_ui.state = UI_SUB_MENU;
        return;
    }
    screen_hide_modal();
    if (return_dash_after_apply)
    {
        g_ui.sub_history_depth = 0;
        g_ui.edit_ctx.active = false;
        menu_runtime_reset();
        menu_actions_clear_pending();
        submenu_slide_out();
        g_ui.state = UI_DASH;
        return;
    }
    screen_close_submenu();
    if (close_extra_mode_layer)
    {
        screen_close_submenu();
    }
}

void screen_cancel_submenu_setting(void)
{
    menu_actions_clear_pending();
    screen_hide_modal();
    g_ui.state = UI_SUB_MENU;
}
