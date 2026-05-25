#include "submenu_view.h"

#include "../core/callbacks.h"
#include "../screen/screen.h"
#include "submenu_model.h"
#include "../core/ui_state.h"
#include "../fonts/fonts.h"

#include <string.h>

static lv_obj_t *s_submenu_layer = NULL;
static lv_obj_t *s_submenu_title = NULL;
static lv_obj_t *s_submenu_title_line = NULL;
static lv_obj_t *s_submenu_list = NULL;
static lv_obj_t *s_light_status_lbl = NULL;
static uint16_t s_submenu_width = 0;
static uint16_t s_submenu_height = 0;
static arex_submenu_setting_confirm_t s_pending_setting;

static bool submenu_is_dive_plan_visible(void)
{
    if (!s_submenu_title)
    {
        return false;
    }
    const char *title = lv_label_get_text(s_submenu_title);
    if (!title)
    {
        return false;
    }
    if (title[0] == '>' && title[1] == ' ')
    {
        title += 2;
    }
    return strcmp(title, "DIVE PLAN") == 0;
}

static bool submenu_is_dive_plan_result_visible(void)
{
    return submenu_is_dive_plan_visible() && arex_submenu_dive_plan_is_result_page();
}

void arex_submenu_view_reset(void)
{
    s_submenu_layer = NULL;
    s_submenu_title = NULL;
    s_submenu_title_line = NULL;
    s_submenu_list = NULL;
    s_light_status_lbl = NULL;
    s_submenu_width = 0;
    s_submenu_height = 0;
    memset(&s_pending_setting, 0, sizeof(s_pending_setting));
}

lv_obj_t *arex_submenu_view_get_list(void)
{
    return s_submenu_list;
}

void arex_submenu_view_create(lv_obj_t *parent, uint16_t width, uint16_t height)
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
    lv_obj_set_style_text_font(s_submenu_title, arex_get_font(FONT_ID_TITLE), 0);

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
    lv_obj_set_style_text_font(lbl, arex_get_font(font_id), 0);
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
    arex_submenu_dive_plan_get_inputs(&depth_m, &time_min, &rmv_lpm);

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

    arex_dive_plan_page_t page = arex_submenu_dive_plan_page();
    lv_snprintf(buf, sizeof(buf), "%.0f", (double)depth_m);
    plan_make_label(parent,
                    (page == AREX_DIVE_PLAN_PAGE_DEPTH) ? "---" : buf,
                    FONT_ID_SMALL,
                    LIGHT,
                    70,
                    34,
                    70,
                    18,
                    LV_TEXT_ALIGN_CENTER);
    lv_snprintf(buf, sizeof(buf), "%u", (unsigned)time_min);
    plan_make_label(parent,
                    (page <= AREX_DIVE_PLAN_PAGE_TIME) ? "--" : buf,
                    FONT_ID_SMALL,
                    LIGHT,
                    155,
                    34,
                    70,
                    18,
                    LV_TEXT_ALIGN_CENTER);
    lv_snprintf(buf, sizeof(buf), "%.0f", (double)rmv_lpm);
    plan_make_label(parent,
                    (page <= AREX_DIVE_PLAN_PAGE_RMV) ? "--" : buf,
                    FONT_ID_SMALL,
                    LIGHT,
                    240,
                    34,
                    70,
                    18,
                    LV_TEXT_ALIGN_CENTER);

    uint8_t header_o2 = arex_submenu_dive_plan_header_gas_o2();
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
    arex_dive_plan_page_t page = arex_submenu_dive_plan_page();

    arex_submenu_dive_plan_get_inputs(&depth_m, &time_min, &rmv_lpm);
    if (page == AREX_DIVE_PLAN_PAGE_TIME)
    {
        prompt = "Enter Bottom Time";
        unit = "in minutes";
        min_v = 1U;
        max_v = 300U;
        value = time_min;
    }
    else if (page == AREX_DIVE_PLAN_PAGE_RMV)
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
                (unsigned)arex_submenu_dive_plan_gf_low(),
                (unsigned)arex_submenu_dive_plan_gf_high());
    plan_make_label(parent, buf, FONT_ID_SMALL, LIGHT, 96, 168, w - 192, 22, LV_TEXT_ALIGN_LEFT);
    lv_snprintf(buf,
                sizeof(buf),
                "Last Stop:       %um",
                (unsigned)arex_submenu_dive_plan_last_stop_m());
    plan_make_label(parent, buf, FONT_ID_SMALL, LIGHT, 96, 200, w - 192, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, "Start CNS:       0%", FONT_ID_SMALL, LIGHT, 96, 232, w - 192, 22, LV_TEXT_ALIGN_LEFT);
}

static const char *plan_time_text(const arex_dive_plan_row_t *row, char *buf, size_t buf_size)
{
    if (row->type == AREX_DIVE_PLAN_ROW_BOTTOM)
    {
        return "bot";
    }
    if (row->type == AREX_DIVE_PLAN_ROW_ASCENT)
    {
        return "asc";
    }
    lv_snprintf(buf, buf_size, "%u", (unsigned)row->time_min);
    return buf;
}

static void plan_draw_result(lv_obj_t *parent, int w)
{
    char buf[56];
    static const int col_x[] = { 42, 116, 190, 266, 340 };
    uint8_t page = arex_submenu_dive_plan_result_page_index();
    uint8_t total_pages = arex_submenu_dive_plan_result_total_pages();
    uint8_t entry_count = arex_submenu_dive_plan_result_entry_count();
    uint8_t start = (uint8_t)(page * 8U);
    uint8_t end = (uint8_t)(start + 8U);
    if (end > entry_count) end = entry_count;

    plan_make_label(parent, "Stp", FONT_ID_SMALL, GREEN, col_x[0] - 24, 68, 60, 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "Tme", FONT_ID_SMALL, GREEN, col_x[1] - 24, 68, 60, 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "Run", FONT_ID_SMALL, GREEN, col_x[2] - 24, 68, 60, 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "Gas", FONT_ID_SMALL, GREEN, col_x[3] - 24, 68, 60, 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "Qty", FONT_ID_SMALL, GREEN, col_x[4] - 24, 68, 60, 18, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, 16, 88);
    lv_obj_set_size(line, w - 32, 1);
    lv_obj_set_style_bg_color(line, GREEN, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);

    int y = 100;
    for (uint8_t i = start; i < end; i++)
    {
        arex_dive_plan_row_t row;
        char tme[8];
        if (!arex_submenu_dive_plan_result_row(i, &row))
        {
            continue;
        }
        const char *tme_text = plan_time_text(&row, tme, sizeof(tme));
        lv_snprintf(buf,
                    sizeof(buf),
                    "%3dm      %3s      %3u      %02u/%02u      %4u",
                    (int)row.depth_m,
                    tme_text,
                    (unsigned)row.run_min,
                    (unsigned)row.o2_pct,
                    (unsigned)row.he_pct,
                    (unsigned)row.gas_l);
        plan_make_label(parent, buf, FONT_ID_SMALL, LIGHT, 18, y, w - 36, 18, LV_TEXT_ALIGN_LEFT);
        y += 26;
    }

    lv_snprintf(buf, sizeof(buf), "Page %u/%u", (unsigned)(page + 1U), (unsigned)total_pages);
    plan_make_label(parent, buf, FONT_ID_SMALL, GREEN, 0, 282, w, 18, LV_TEXT_ALIGN_CENTER);
}

static void plan_draw_error(lv_obj_t *parent, int w)
{
    plan_make_label(parent, "Plan Failed", FONT_ID_MEDIUM, LIGHT, 0, 118, w, 40, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, "Check depth, time, RMV and gas setup",
                    FONT_ID_SMALL, LIGHT, 0, 176, w, 24, LV_TEXT_ALIGN_CENTER);
}

static void submenu_populate_dive_plan(const char **items, uint8_t count)
{
    if (!s_submenu_list) return;

    int w = (s_submenu_width > 0)
            ? (int)s_submenu_width
            : (int)(g_sys_config.safe_zone_w - LEFT_ANCHOR_W - g_sys_config.panel_gap_u * BASE_U);

    lv_obj_clean(s_submenu_list);
    s_light_status_lbl = NULL;

    if (count > 0U)
    {
        (void)plan_make_button(s_submenu_list, items[0], 12, (int)s_submenu_height - 34);
    }
    if (count > 1U)
    {
        (void)plan_make_button(s_submenu_list, items[1], w - 92, (int)s_submenu_height - 34);
    }

    plan_draw_header(s_submenu_list, w);
    switch (arex_submenu_dive_plan_page())
    {
    case AREX_DIVE_PLAN_PAGE_READY:
        plan_draw_ready(s_submenu_list, w);
        break;
    case AREX_DIVE_PLAN_PAGE_RESULT:
        plan_draw_result(s_submenu_list, w);
        break;
    case AREX_DIVE_PLAN_PAGE_ERROR:
        plan_draw_error(s_submenu_list, w);
        break;
    case AREX_DIVE_PLAN_PAGE_DEPTH:
    case AREX_DIVE_PLAN_PAGE_TIME:
    case AREX_DIVE_PLAN_PAGE_RMV:
    default:
        plan_draw_input(s_submenu_list, w);
        break;
    }
    plan_draw_bottom_line(s_submenu_list, w);
    arex_screen_set_submenu_selection((count > 1U) ? 1U : 0U);
}

static void submenu_populate(const char *title, const char **items, uint8_t count)
{
    if (!s_submenu_title || !s_submenu_list) return;

    bool is_dive_plan = (strcmp(title, "DIVE PLAN") == 0);
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
        submenu_populate_dive_plan(items, count);
        return;
    }

    lv_obj_clear_flag(s_submenu_title, LV_OBJ_FLAG_HIDDEN);
    if (s_submenu_title_line)
    {
        lv_obj_clear_flag(s_submenu_title_line, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_size(s_submenu_list, s_submenu_width - 15, s_submenu_height - CARD_TITLE_H - 10);
    lv_obj_set_pos(s_submenu_list, 0, CARD_TITLE_H);

    lv_label_set_text(s_submenu_title, title);
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
    bool compact_plan = (strcmp(title, "DIVE PLAN") == 0 &&
                         arex_submenu_dive_plan_is_result_page());
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
        if (strcmp(title, "LIGHT CONTROL") == 0 && i == 0)
        {
            /* "LIGHT" 标签在左侧 */
            lv_obj_t *lbl_light = lv_label_create(item);
            lv_obj_set_style_text_color(lbl_light, GREEN, 0);
            lv_obj_set_style_text_font(lbl_light, arex_get_font(FONT_ID_TITLE), 0);
            lv_obj_set_size(lbl_light, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(lbl_light, LV_ALIGN_LEFT_MID, 12, 0);
            lv_obj_set_style_text_align(lbl_light, LV_TEXT_ALIGN_LEFT, 0);
            lv_label_set_text(lbl_light, "LIGHT");

            /* "ON"/"OFF" 标签在右侧 */
            lv_obj_t *lbl_status = lv_label_create(item);
            lv_obj_set_style_text_color(lbl_status, g_light_power_state ? GREEN : LIGHT, 0);
            lv_obj_set_style_text_font(lbl_status, arex_get_font(FONT_ID_TITLE), 0);
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
        lv_obj_set_style_text_font(lbl, arex_get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_TITLE), 0);
        lv_obj_set_size(lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_label_set_text(lbl, items[i]);

        current_y += item_h + gap_y;
    }
    arex_screen_set_submenu_selection(0);
}

void arex_screen_set_submenu_selection(uint8_t idx)
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

    bool compact_plan = submenu_is_dive_plan_result_visible();
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
                lv_obj_set_style_text_font(lbl, arex_get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_MEDIUM), 0);
            }
            /* LIGHT CONTROL second column uses the same selected emphasis. */
            lv_obj_t *lbl2 = lv_obj_get_child(item, 1);
            if (lbl2)
            {
                lv_obj_set_style_text_color(lbl2, LIGHT, 0);
                lv_obj_set_style_text_font(lbl2, arex_get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_MEDIUM), 0);
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
                lv_obj_set_style_text_font(lbl, arex_get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_TITLE), 0);
            }
            /* LIGHT CONTROL 特殊处理：第二列（ON/OFF）恢复状态色 */
            lv_obj_t *lbl2 = lv_obj_get_child(item, 1);
            if (lbl2)
            {
                lv_obj_set_style_text_color(lbl2, g_light_power_state ? GREEN : LIGHT, 0);
                lv_obj_set_style_text_font(lbl2, arex_get_font(compact_plan ? FONT_ID_SMALL : FONT_ID_TITLE), 0);
            }
        }
    }
}

/* INFO sub-menu */
void arex_screen_open_info_submenu(uint8_t item_idx)
{
    uint8_t count = 0;
    const char *title = arex_submenu_info_title(item_idx);
    const char **items = arex_submenu_build_info_items(item_idx, &count);
    if (!title || !items || count == 0) return;

    submenu_populate(title, items, count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = (strcmp(title, "DIVE PLAN") == 0 && count > 1U) ? 1U : 0U;
    g_ui.sub_parent     = UI_INFO;
    g_ui.state          = UI_SUB_MENU;
    arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
    submenu_slide_in();
}

static void refresh_info_submenu_page(uint8_t keep_idx)
{
    uint8_t count = 0;
    const char *title = arex_submenu_info_title(g_ui.menu_info_idx);
    const char **items = arex_submenu_build_info_items(g_ui.menu_info_idx, &count);
    if (!title || !items || count == 0)
    {
        return;
    }

    submenu_populate(title, items, count);
    g_ui.sub_item_count = count;
    if (keep_idx >= count)
    {
        keep_idx = (uint8_t)(count - 1U);
    }
    g_ui.sub_menu_idx = keep_idx;
    arex_screen_set_submenu_selection(keep_idx);
}

void arex_screen_refresh_info_submenu_if_open(void)
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

bool arex_screen_handle_dive_plan_rotate(int8_t dir)
{
    if (g_ui.state != UI_SUB_MENU || g_ui.sub_parent != UI_INFO || !submenu_is_dive_plan_visible())
    {
        return false;
    }
    if (!arex_submenu_dive_plan_handle_rotate(dir))
    {
        return false;
    }
    refresh_info_submenu_page(1U);
    return true;
}

static bool refresh_compass_cal_submenu(void)
{
    if (!s_submenu_list || !s_submenu_title)
    {
        return false;
    }

    const char *raw_title = lv_label_get_text(s_submenu_title);
    const char *title = raw_title;
    if (title && title[0] == '>' && title[1] == ' ')
    {
        title += 2;
    }
    if (!title || strcmp(title, "COMPASS CAL") != 0)
    {
        return false;
    }

    uint8_t count = 0;
    const char **items = arex_submenu_build_compass_cal_items(&count);
    if (!items || count == 0)
    {
        return false;
    }
    submenu_populate("COMPASS CAL", items, count);
    g_ui.sub_item_count = count;
    if (g_ui.sub_menu_idx >= count)
    {
        g_ui.sub_menu_idx = count - 1;
    }
    arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
    return true;
}

void arex_screen_open_setup_submenu(uint8_t item_idx)
{
    uint8_t count = 0;
    const char *title = arex_submenu_setup_title(item_idx);
    const char **items = arex_submenu_build_setup_items(item_idx, &count);
    if (!title || !items || count == 0) return;

    submenu_populate(title, items, count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.sub_parent     = UI_SETUP;
    g_ui.state          = UI_SUB_MENU;
    submenu_slide_in();
}

void arex_screen_refresh_compass_cal_submenu_if_open(void)
{
    (void)refresh_compass_cal_submenu();
}

static void submenu_history_push(void)
{
    if (!s_submenu_title) return;
    if (g_ui.sub_history_depth >= AREX_SUB_HISTORY_MAX) return;
    arex_sub_history_t *h = &g_ui.sub_history[g_ui.sub_history_depth];
    const char *cur_title = lv_label_get_text(s_submenu_title);
    lv_snprintf(h->title, sizeof(h->title), "%s", cur_title ? cur_title : "");
    h->idx = g_ui.sub_menu_idx;
    g_ui.sub_history_depth++;
}

void arex_screen_open_nested_submenu(const char *title, const char **items, uint8_t count)
{
    if (!title || !items) return;
    submenu_history_push();
    char full_title[40];
    lv_snprintf(full_title, sizeof(full_title), "> %s", title);
    submenu_populate(full_title, items, count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.state = UI_SUB_MENU;
}

static void dispatch_submenu_setting_callback(const arex_submenu_setting_confirm_t *setting)
{
    if (!setting)
    {
        return;
    }

    switch (setting->kind)
    {
    case AREX_SUBMENU_SETTING_DIVE_MODE:
        arex_ui_on_dive_mode_set((uint8_t)setting->value);
        arex_screen_refresh_gas_menu();
        arex_screen_refresh_left_panel();
        break;
    case AREX_SUBMENU_SETTING_SALINITY:
        arex_ui_on_salinity_set((uint8_t)setting->value);
        break;
    case AREX_SUBMENU_SETTING_SAFETY_STOP:
        if (setting->value < 4)
        {
            static const uint8_t minutes[] = { 0, 3, 4, 5 };
            arex_ui_on_safety_stop_time_set(minutes[setting->value]);
        }
        break;
    case AREX_SUBMENU_SETTING_LAST_DECO:
        arex_ui_on_last_deco_stop_set(setting->value == 1 ? 6 : 3);
        break;
    case AREX_SUBMENU_SETTING_ALTITUDE:
        arex_ui_on_altitude_range_set((uint8_t)setting->value);
        break;
    case AREX_SUBMENU_SETTING_AI_PAIR:
        arex_ui_on_ai_pair((uint8_t)setting->value);
        break;
    case AREX_SUBMENU_SETTING_AI_TANK_STATE:
        arex_ui_on_ai_tank_state_set(setting->arg, (uint8_t)setting->value);
        break;
    case AREX_SUBMENU_SETTING_GTR_MODE:
        arex_ui_on_gtr_mode_set(setting->value != 0);
        break;
    case AREX_SUBMENU_SETTING_DEPTH_ALARM:
        arex_ui_on_depth_alarm_set(setting->value);
        break;
    case AREX_SUBMENU_SETTING_TIME_ALARM:
        arex_ui_on_time_alarm_set(setting->value);
        break;
    case AREX_SUBMENU_SETTING_NDL_ALARM:
        arex_ui_on_ndl_alarm_set(setting->value);
        break;
    case AREX_SUBMENU_SETTING_VIBRATION_TEST:
        arex_ui_on_vibration_test();
        break;
    case AREX_SUBMENU_SETTING_UNITS:
        arex_ui_on_units_set((uint8_t)setting->value);
        break;
    case AREX_SUBMENU_SETTING_DATETIME_FIELD:
    {
        uint16_t field_value = setting->value;
        if (setting->arg == 0)
        {
            field_value = (uint16_t)(2024 + setting->value);
        }
        arex_ui_on_datetime_field_set(setting->arg, field_value);
        break;
    }
    case AREX_SUBMENU_SETTING_DATETIME_ACTION:
        arex_ui_on_datetime_action((uint8_t)setting->value);
        break;
    case AREX_SUBMENU_SETTING_LOG_RATE:
        arex_ui_on_log_rate_set((uint8_t)setting->value);
        break;
    case AREX_SUBMENU_SETTING_BLUETOOTH:
        arex_ui_on_bluetooth_set(setting->value != 0);
        break;
    case AREX_SUBMENU_SETTING_RESET_DEFAULTS:
        arex_ui_on_reset_defaults();
        break;
    default:
        break;
    }
}

static void refresh_current_submenu_page(const char *cur_title, uint8_t keep_idx)
{
    uint8_t count = 0;
    const char **items = arex_submenu_nested_items_for(cur_title, &count);
    if (!items || count == 0)
    {
        return;
    }

    char full_title[40];
    lv_snprintf(full_title, sizeof(full_title), "> %s", cur_title);
    submenu_populate(full_title, items, count);
    g_ui.sub_item_count = count;
    if (keep_idx >= count)
    {
        keep_idx = (uint8_t)(count - 1);
    }
    g_ui.sub_menu_idx = keep_idx;
    arex_screen_set_submenu_selection(keep_idx);
}

void arex_screen_handle_submenu_select(uint8_t item_idx)
{
    if (!s_submenu_list || !s_submenu_title) return;
    if (item_idx >= g_ui.sub_item_count) return;
    lv_obj_t *item = lv_obj_get_child(s_submenu_list, item_idx);
    lv_obj_t *lbl  = item ? lv_obj_get_child(item, 0) : NULL;
    if (!lbl) return;
    const char *text = lv_label_get_text(lbl);
    if (!text) return;

    const char *raw_title = lv_label_get_text(s_submenu_title);
    char cur_title[40] = {0};
    if (raw_title)
    {
        const char *p = raw_title;
        if (p[0] == '>' && p[1] == ' ') p += 2;
        lv_snprintf(cur_title, sizeof(cur_title), "%s", p);
    }

    if (strcmp(text, "< BACK") == 0)
    {
        arex_screen_close_submenu();
        return;
    }

    if (strcmp(cur_title, "DIVE PLAN") == 0)
    {
        bool close_submenu = false;
        uint8_t keep_idx = item_idx;
        if (arex_submenu_dive_plan_handle_action(item_idx, text, &close_submenu, &keep_idx))
        {
            if (close_submenu)
            {
                arex_screen_close_submenu();
                return;
            }
            refresh_info_submenu_page(keep_idx);
            return;
        }
        if (arex_submenu_dive_plan_is_result_page())
        {
            return;
        }
    }

    // HOTFIX: Block action for Info detail rows.
    if (arex_submenu_is_readonly_info_title(cur_title))
    {
        return;
    }

    {
        arex_submenu_edit_spec_t edit_spec;
        if (arex_submenu_edit_spec_from_selection(cur_title, item_idx, text, &edit_spec))
        {
            arex_screen_begin_edit_value(item_idx, &edit_spec);
            return;
        }
    }

    if (strcmp(cur_title, "DIVE PLAN") == 0)
    {
        return;
    }

    {
        arex_submenu_setting_confirm_t direct_setting;
        if (arex_submenu_direct_setting_from_selection(cur_title, item_idx, text, &direct_setting))
        {
            arex_submenu_apply_setting(direct_setting.kind,
                                       direct_setting.arg,
                                       direct_setting.value);
            dispatch_submenu_setting_callback(&direct_setting);
            if (direct_setting.kind == AREX_SUBMENU_SETTING_OC_TECH_SAVE)
            {
                arex_screen_close_submenu();
                return;
            }
            refresh_current_submenu_page(cur_title, item_idx);
            return;
        }
    }

    if (arex_submenu_setting_from_selection(cur_title, item_idx, text, &s_pending_setting))
    {
        arex_screen_show_modal_setup_confirm(s_pending_setting.body);
        g_ui.state = UI_MODAL_SETUP_CONFIRM;
        return;
    }

    if (strcmp(cur_title, "LIGHT CONTROL") == 0 && item_idx == 0)
    {
        g_light_power_state = !g_light_power_state;
        arex_bus_set_light_power(g_light_power_state);

        /* 同步当前子菜单显示 */
        if (s_light_status_lbl)
        {
            lv_label_set_text(s_light_status_lbl, g_light_power_state ? "ON" : "OFF");
        }
        /* 保持选中项停留在 ON/OFF 行 */
        arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
        return;
    }

    {
        char child_title[40] = {0};
        uint8_t ncnt = 0;
        const char **nitems = arex_submenu_child_items_for(cur_title,
                                                           item_idx,
                                                           text,
                                                           child_title,
                                                           sizeof(child_title),
                                                           &ncnt);
        if (nitems && ncnt > 0)
        {
            arex_screen_open_nested_submenu(child_title, nitems, ncnt);
            return;
        }
    }

    if (strcmp(cur_title, "GAS SWITCH") == 0)
    {
        uint8_t gas_count = g_sensor_data.gas_slot_count;
        if (gas_count > GAS_COUNT)
        {
            gas_count = GAS_COUNT;
        }
        if (item_idx < gas_count)
        {
            // HOTFIX: Route gas switch to safety modal.
            g_ui.gas_cursor = item_idx;
            g_ui.gas_modal_from_submenu = true;  // HOTFIX: Route GAS modal exit based on context.
            arex_screen_show_modal_gas();
            g_ui.state = UI_MODAL_GAS;
            return;
        }
        return;
    }

    if (strcmp(cur_title, "CONSERVATISM") == 0)
    {
        if (strcmp(text, "< BACK") != 0)
        {
            /* 解析 conservatism 等级：LOW/MED/HIGH/CUSTOM */
            uint8_t level = 1;  /* 默认 MED */
            static const char *badge_text[] = { "LOW", "MED", "HIGH", "CUSTOM" };
            if (strncmp(text, "LOW", 3) == 0) level = 0;
            else if (strncmp(text, "MED", 3) == 0) level = 1;
            else if (strncmp(text, "HIGH", 4) == 0) level = 2;
            else if (strncmp(text, "CUSTOM", 6) == 0) level = 3;

            /* 通知业务层应用保守度设置 */
            arex_ui_on_conservatism_set(level);

            arex_screen_refresh_setup_menu();
            arex_screen_update_setup_badge(1, badge_text[level]);
        }
        arex_screen_close_submenu();
        return;
    }

    if (strcmp(cur_title, "COMPASS CAL") == 0)
    {
        if (strncmp(text, "AUTO CAL:", 9) == 0)
        {
            arex_request_compass_calibration_start();
            arex_set_compass_calibration_ui_state(AREX_COMPASS_CAL_RUNNING);
            arex_screen_refresh_setup_menu();
            refresh_compass_cal_submenu();
            return;
        }
        if (strcmp(text, "RESET AUTO CAL") == 0)
        {
            arex_request_compass_calibration_reset();
            arex_set_compass_calibration_ui_state(AREX_COMPASS_CAL_IDLE);
            arex_screen_refresh_setup_menu();
            refresh_compass_cal_submenu();
            return;
        }
        return;
    }

    if (strcmp(cur_title, "BRIGHTNESS") == 0)
    {
        if (strcmp(text, "< BACK") != 0)
        {
            /* 更新亮度配置并刷新 badge */
            if (strcmp(text, "LOW") == 0)
            {
                g_sys_config.brightness = 0;
            }
            else if (strcmp(text, "ECO") == 0)
            {
                g_sys_config.brightness = 1;
            }
            else if (strcmp(text, "MED") == 0)
            {
                g_sys_config.brightness = 2;
            }
            else if (strcmp(text, "HIGH") == 0)
            {
                g_sys_config.brightness = 3;
            }
            else if (strcmp(text, "MAX") == 0)
            {
                g_sys_config.brightness = 4;
            }
            else if (strcmp(text, "SUN") == 0)
            {
                g_sys_config.brightness = 5;
            }
            /* 通过业务回调应用亮度 */
            arex_set_brightness(g_sys_config.brightness);
        }
        arex_screen_update_setup_badge(2, text);
        arex_screen_close_submenu();
        return;
    }

    /* 颜色子菜单：RED/GREEN/BLUE/WHITE */
    if (strcmp(cur_title, "RED") == 0 || strcmp(cur_title, "GREEN") == 0 ||
            strcmp(cur_title, "BLUE") == 0 || strcmp(cur_title, "WHITE") == 0)
    {
        /* 非返回项表示选择了亮度档位 */
        if (strcmp(text, "< BACK") != 0)
        {
            /* 通知业务层处理灯光颜色亮度 */
            arex_ui_on_light_color_set(cur_title, text);

        }
        /* 完成后关闭颜色子菜单 */
        arex_screen_close_submenu();
        return;
    }

    if (strcmp(cur_title, "DIVE MENU") == 0 || strcmp(cur_title, "DIVE SETUP") == 0)
    {
        arex_screen_show_modal_act(text);
        return;
    }

    if (strcmp(cur_title, "ALERTS SETUP") == 0 && item_idx == 2)
    {
        return;
    }

    arex_screen_show_modal_act(text);
}

void arex_screen_close_submenu(void)
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

    if (g_ui.sub_history_depth > 0)
    {
        g_ui.sub_history_depth--;
        arex_sub_history_t *h = &g_ui.sub_history[g_ui.sub_history_depth];
        const char *prev_title = h->title;
        if (prev_title[0] == '>' && prev_title[1] == ' ') prev_title += 2;

        bool found = false;
        int8_t setup_idx = arex_submenu_setup_index_for_title(prev_title);
        if (setup_idx >= 0)
        {
            uint8_t cnt = 0;
            const char **items = arex_submenu_build_setup_items((uint8_t)setup_idx, &cnt);
            const char *title = arex_submenu_setup_title((uint8_t)setup_idx);
            if (items && title && cnt > 0)
            {
                submenu_populate(title, items, cnt);
                g_ui.sub_item_count = cnt;
                g_ui.sub_menu_idx   = h->idx;
                if (g_ui.sub_menu_idx >= cnt)
                {
                    g_ui.sub_menu_idx = cnt - 1;
                }
                arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
                found = true;
            }
        }
        if (!found)
        {
            uint8_t ncnt = 0;
            const char **nitems = arex_submenu_nested_items_for(prev_title, &ncnt);
            if (nitems && ncnt > 0)
            {
                char full_title[40];
                lv_snprintf(full_title, sizeof(full_title), "> %s", prev_title);
                submenu_populate(full_title, nitems, ncnt);
                g_ui.sub_item_count = ncnt;
                g_ui.sub_menu_idx   = h->idx;
                arex_screen_set_submenu_selection(h->idx);
                found = true;
            }
        }
        g_ui.state = UI_SUB_MENU;
        return;
    }
    submenu_slide_out();
    g_ui.state = g_ui.sub_parent;
}

void arex_screen_confirm_submenu_setting(void)
{
    bool close_extra_mode_layer = false;
    bool return_dash_after_apply = false;
    if (s_pending_setting.kind == AREX_SUBMENU_SETTING_NONE)
    {
        arex_screen_hide_modal();
        g_ui.state = UI_SUB_MENU;
        return;
    }

    close_extra_mode_layer =
        (s_pending_setting.kind == AREX_SUBMENU_SETTING_DIVE_MODE &&
         s_pending_setting.value != 0);
    return_dash_after_apply =
        (s_pending_setting.kind == AREX_SUBMENU_SETTING_DIVE_MODE &&
         s_pending_setting.value == 3);

    arex_submenu_apply_setting(s_pending_setting.kind, s_pending_setting.arg, s_pending_setting.value);
    dispatch_submenu_setting_callback(&s_pending_setting);

    memset(&s_pending_setting, 0, sizeof(s_pending_setting));
    arex_screen_hide_modal();
    if (return_dash_after_apply)
    {
        g_ui.sub_history_depth = 0;
        g_ui.edit_ctx.active = false;
        submenu_slide_out();
        g_ui.state = UI_DASH;
        return;
    }
    arex_screen_close_submenu();
    if (close_extra_mode_layer)
    {
        arex_screen_close_submenu();
    }
}

void arex_screen_cancel_submenu_setting(void)
{
    memset(&s_pending_setting, 0, sizeof(s_pending_setting));
    arex_screen_hide_modal();
    g_ui.state = UI_SUB_MENU;
}
