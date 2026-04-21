#include "arex_screen.h"
#include "arex_ui_engine.h"
#include "arex_data.h"
#include "arex_ui_state.h"
#include "arex_card_registry.h"
#include "fonts/arex_fonts.h"
#include <stdio.h>
#include <string.h>

/* =========================================================
 * 内部句柄
 * ========================================================= */
static lv_obj_t *s_scr;
static lv_obj_t *s_safe_zone;        /* 安全区容器 (绝对坐标原点) */
static lv_obj_t *s_left_anchor;      /* 左侧锚点 (10U 沙盒) */
static lv_obj_t *s_right_cont;       /* clip container */
static lv_obj_t *s_tileview;

/* Left anchor labels */
static lv_obj_t *s_lbl_depth;
static lv_obj_t *s_lbl_ndl;
static lv_obj_t *s_lbl_tts;
static lv_obj_t *s_lbl_next_stop;
static lv_obj_t *s_lbl_pod1;
static lv_obj_t *s_lbl_pod2;
static lv_obj_t *s_lbl_gas_name;
static lv_obj_t *s_ppo2_vals[3];
static lv_obj_t *s_ppo2_seps[2];
static lv_obj_t *s_lbl_time;
static lv_obj_t *s_lbl_batt;

/* 左侧锚点组件句柄数组 (按 arex_anchor_comp_t 顺序) */
static lv_obj_t *s_anchor_titles[ANCHOR_COMP_COUNT];
static lv_obj_t *s_anchor_vals[ANCHOR_COMP_COUNT];

/* Wall indicators */
static lv_obj_t *s_wall_top;
static lv_obj_t *s_wall_bottom;
static lv_obj_t *s_wall_text_top,    *s_wall_blocks_top;
static lv_obj_t *s_wall_text_bottom, *s_wall_blocks_bottom;

/* Scroll dots */
static lv_obj_t *s_scroll_dots[AREX_DASH_CARD_COUNT];

/* Modal overlay */
static lv_obj_t *s_modal;
static lv_obj_t *s_modal_box;

/* Sub-menu layer */
static lv_obj_t *s_submenu_layer;
static lv_obj_t *s_submenu_title;
static lv_obj_t *s_submenu_list;

/* INFO / SETUP list objects */
static lv_obj_t *s_info_list;
static lv_obj_t *s_setup_list;

/* 排版缓存 (避免每次重算) */
static uint16_t s_cached_right_w = 0;

/* =========================================================
 * 样式 (静态初始化一次)
 * ========================================================= */
static lv_style_t s_style_screen;
static lv_style_t s_style_panel;
static lv_style_t s_style_anchor_bg;
static lv_style_t s_style_label_huge;
static lv_style_t s_style_label_med;
static lv_style_t s_style_label_small;
static lv_style_t s_style_title_zone;
static lv_style_t s_style_val_zone;
static lv_style_t s_style_menu_item;
static lv_style_t s_style_menu_item_active;
static lv_style_t s_style_sep_line;
static bool       s_styles_inited = false;

static void styles_init(void)
{
    if (s_styles_inited) return;
    s_styles_inited = true;

    lv_style_init(&s_style_screen);
    lv_style_set_bg_color(&s_style_screen, AREX_BG);
    lv_style_set_bg_opa(&s_style_screen, LV_OPA_COVER);
    lv_style_set_pad_all(&s_style_screen, 0);
    lv_style_set_border_width(&s_style_screen, 0);

    lv_style_init(&s_style_anchor_bg);
    lv_style_set_bg_color(&s_style_anchor_bg, AREX_BLACK);
    lv_style_set_bg_opa(&s_style_anchor_bg, LV_OPA_20);
    lv_style_set_border_color(&s_style_anchor_bg, AREX_DARK);
    lv_style_set_border_width(&s_style_anchor_bg, 1);
    lv_style_set_pad_all(&s_style_anchor_bg, 0);     /* 必须显式清零，否则 LVGL 默认 padding 会偏移所有子组件坐标 */
    lv_style_set_radius(&s_style_anchor_bg, 0);

    lv_style_init(&s_style_panel);
    lv_style_set_bg_color(&s_style_panel, AREX_BLACK);
    lv_style_set_bg_opa(&s_style_panel, LV_OPA_COVER);
    lv_style_set_pad_all(&s_style_panel, 0);
    lv_style_set_border_width(&s_style_panel, 0);

    lv_style_init(&s_style_label_huge);
    lv_style_set_text_color(&s_style_label_huge, AREX_GREEN);
    lv_style_set_text_font(&s_style_label_huge, AREX_FONT_HUGE);

    lv_style_init(&s_style_label_med);
    lv_style_set_text_color(&s_style_label_med, AREX_GREEN);
    lv_style_set_text_font(&s_style_label_med, AREX_FONT_MEDIUM);

    lv_style_init(&s_style_label_small);
    lv_style_set_text_color(&s_style_label_small, AREX_LIGHT);
    lv_style_set_text_font(&s_style_label_small, AREX_FONT_SMALL);

    lv_style_init(&s_style_title_zone);
    lv_style_set_text_color(&s_style_title_zone, AREX_LIGHT);
    lv_style_set_text_font(&s_style_title_zone, AREX_FONT_SMALL);
    lv_style_set_bg_opa(&s_style_title_zone, LV_OPA_TRANSP);

    lv_style_init(&s_style_val_zone);
    lv_style_set_text_color(&s_style_val_zone, AREX_GREEN);
    lv_style_set_bg_opa(&s_style_val_zone, LV_OPA_TRANSP);

    lv_style_init(&s_style_sep_line);
    lv_style_set_bg_color(&s_style_sep_line, AREX_DARK);
    lv_style_set_bg_opa(&s_style_sep_line, LV_OPA_50);
    lv_style_set_border_width(&s_style_sep_line, 0);

    lv_style_init(&s_style_menu_item);
    lv_style_set_bg_color(&s_style_menu_item, AREX_BLACK);
    lv_style_set_bg_opa(&s_style_menu_item, LV_OPA_COVER);
    lv_style_set_text_color(&s_style_menu_item, AREX_GREEN);
    lv_style_set_border_color(&s_style_menu_item, AREX_DARK);
    lv_style_set_border_width(&s_style_menu_item, 2);
    lv_style_set_pad_all(&s_style_menu_item, 12);

    lv_style_init(&s_style_menu_item_active);
    lv_style_set_bg_color(&s_style_menu_item_active, AREX_GREEN);
    lv_style_set_bg_opa(&s_style_menu_item_active, LV_OPA_COVER);
    lv_style_set_text_color(&s_style_menu_item_active, AREX_BLACK);
    lv_style_set_border_color(&s_style_menu_item_active, AREX_GREEN);
}

/* =========================================================
 * 辅助函数
 * ========================================================= */

/* =========================================================
 * 左侧锚点：绝对坐标重建
 *
 * 核心铁律：所有子组件以 s_left_anchor 左上角为原点 (0,0)，
 * 不使用任何 LV_FLEX / LV_GRID，完全通过数学累加 current_y。
 *
 * Tech 模式下：
 *   - DEPTH: (0, cur_y), 宽=160px, 高=h_depth*10
 *   - NDL/TTS 双拼行: cur_y += h_depth*10 + gap
 *     NDL: (0, cur_y), 宽=80px, 高=h_ndl*10
 *     TTS: (80, cur_y), 宽=80px, 高=h_ndl*10
 *   - 以此类推...
 *
 * Classic 模式下：使用同样逻辑，宽度扩展为 safe_zone_w
 * ========================================================= */
static void left_anchor_rebuild(void)
{
    if (!s_left_anchor || !s_safe_zone) return;

    /* 推算左侧锚点尺寸 */
    arex_anchor_comp_t comps[ANCHOR_COMP_COUNT];
    uint16_t total_h = 0;
    arex_calc_anchor_layout(comps, &total_h);

    uint16_t anchor_w = AREX_LEFT_ANCHOR_W;
    uint16_t anchor_h = g_sys_config.safe_zone_h;
    lv_obj_set_size(s_left_anchor, anchor_w, anchor_h);

    /* 重建每个组件 */
    for (uint8_t i = 0; i < ANCHOR_COMP_COUNT; i++) {
        arex_anchor_comp_t *c = &comps[i];
        lv_obj_t *title_obj = s_anchor_titles[i];
        lv_obj_t *val_obj   = s_anchor_vals[i];

        if (!title_obj || !val_obj) continue;

        /* 设置坐标和尺寸 */
        lv_obj_set_pos(title_obj, 0, c->y);
        lv_obj_set_size(title_obj, c->w, c->title_h);

        lv_obj_set_pos(val_obj, 0, c->y + c->title_h);
        lv_obj_set_size(val_obj, c->w, c->val_h);

        /* 分割线：标题底部画一条线 */
        lv_obj_t *sep = lv_obj_get_child(title_obj, 0);
        if (sep) {
            lv_obj_set_size(sep, c->w, g_sys_config.sep_thick);
            lv_obj_set_pos(sep, 0, c->title_h - g_sys_config.sep_thick);
            lv_obj_set_style_bg_color(sep, AREX_DARK, 0);
            lv_obj_set_style_bg_opa(sep, g_sys_config.sep_alpha, 0);
        }

        /* 双拼对齐引擎 */
        if (c->split > 0 && g_sys_config.split_outward) {
            if (c->split == 1) {
                /* 左块靠左 */
                lv_obj_set_style_text_align(val_obj, LV_TEXT_ALIGN_LEFT, 0);
            } else {
                /* 右块靠右 */
                lv_obj_set_style_text_align(val_obj, LV_TEXT_ALIGN_RIGHT, 0);
            }
        }
    }
}

/* =========================================================
 * 左侧锚点创建 (首次初始化)
 * ========================================================= */
static void left_anchor_create(void)
{
    s_left_anchor = lv_obj_create(s_safe_zone);
    lv_obj_set_size(s_left_anchor, AREX_LEFT_ANCHOR_W, g_sys_config.safe_zone_h);
    lv_obj_set_pos(s_left_anchor, 0, 0);
    lv_obj_add_style(s_left_anchor, &s_style_anchor_bg, 0);
    lv_obj_set_style_pad_all(s_left_anchor, 0, 0);   /* 兜底：确保 theme 默认 padding 不干扰绝对坐标 */
    lv_obj_set_scrollbar_mode(s_left_anchor, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_left_anchor, LV_OBJ_FLAG_SCROLLABLE);

    /* 推算布局参数 */
    arex_anchor_comp_t comps[ANCHOR_COMP_COUNT];
    uint16_t total_h = 0;
    arex_calc_anchor_layout(comps, &total_h);

    static const char *title_texts[ANCHOR_COMP_COUNT] = {
        "DEPTH", "NDL", "TTS", "POD 1", "POD 2", "BATT", "W.TIME", "GAS", "TIME"
    };

    /* 字号映射：
     * i=0  DEPTH   → HUGE  (48px)，val_h = (8-2)*10 = 60px ✓
     * i=1  NDL     → MED   (28px)，val_h = (6-2)*10 = 40px ✓
     * i=2  TTS     → MED   (28px)，val_h = 40px ✓
     * i=3  POD 1   → TITLE (20px)，val_h = 40px ✓
     * i=4  POD 2   → TITLE (20px)，val_h = 40px ✓
     * i=5  BATT    → SMALL (14px)，val_h = (4-2)*10 = 20px ✓
     * i=6  W.TIME  → SMALL (14px)，val_h = 20px ✓
     * i=7  GAS     → MED   (28px)，val_h = (6-2)*10 = 40px ✓
     * i=8  TIME    → SMALL (14px)，val_h = (4-2)*10 = 20px ✓
     */
    const lv_font_t *title_fonts[ANCHOR_COMP_COUNT];
    const lv_font_t *val_fonts[ANCHOR_COMP_COUNT];
    for (uint8_t i = 0; i < ANCHOR_COMP_COUNT; i++) {
        title_fonts[i] = AREX_FONT_SMALL;
        switch (i) {
            case 0:                          val_fonts[i] = AREX_FONT_HUGE;   break;
            case 1: case 2: case 7:          val_fonts[i] = AREX_FONT_MEDIUM; break;
            case 3: case 4:                  val_fonts[i] = AREX_FONT_TITLE;  break;
            default: /* 5,6,8 — val_h=20px */ val_fonts[i] = AREX_FONT_SMALL;  break;
        }
    }

    /* 清除旧子对象 (rebuild 时会用到) */
    lv_obj_clean(s_left_anchor);

    for (uint8_t i = 0; i < ANCHOR_COMP_COUNT; i++) {
        arex_anchor_comp_t *c = &comps[i];

        /* 标题区：双拼右块 x = half_w，其余 x = 0 */
        lv_coord_t comp_x = (c->split == 2) ? (lv_coord_t)(AREX_LEFT_ANCHOR_W / 2) : 0;

        /* 对齐方向：split_outward 时左块靠左、右块靠右；否则统一用全局对齐 */
        lv_text_align_t comp_align;
        if (g_sys_config.split_outward && c->split == 2)
            comp_align = LV_TEXT_ALIGN_RIGHT;
        else if (g_sys_config.split_outward && c->split == 1)
            comp_align = LV_TEXT_ALIGN_LEFT;
        else
            comp_align = arex_align_to_lv(g_sys_config.align_med);

        lv_obj_t *title_zone = lv_obj_create(s_left_anchor);
        lv_obj_set_pos(title_zone, comp_x, c->y);
        lv_obj_set_size(title_zone, c->w, c->title_h);
        lv_obj_set_style_bg_opa(title_zone, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(title_zone, 1, 0);
        lv_obj_set_style_border_color(title_zone, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_pad_all(title_zone, 0, 0);
        lv_obj_set_style_clip_corner(title_zone, true, 0);
        lv_obj_clear_flag(title_zone, LV_OBJ_FLAG_SCROLLABLE);

        /* 分割线 */
        lv_obj_t *sep = lv_obj_create(title_zone);
        lv_obj_set_size(sep, c->w, g_sys_config.sep_thick);
        lv_obj_set_pos(sep, 0, c->title_h - g_sys_config.sep_thick);
        lv_obj_set_style_bg_color(sep, AREX_DARK, 0);
        lv_obj_set_style_bg_opa(sep, g_sys_config.sep_alpha, 0);
        lv_obj_set_style_border_width(sep, 0, 0);
        lv_obj_set_style_pad_all(sep, 0, 0);

        /* 标题文字 — 对齐与数值区统一 */
        lv_obj_t *lbl_title = lv_label_create(title_zone);
        lv_obj_set_pos(lbl_title, 4, 0);
        lv_obj_set_size(lbl_title, c->w - 8, c->title_h);
        lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(lbl_title, comp_align, 0);
        lv_obj_set_style_text_color(lbl_title, AREX_LIGHT, 0);
        lv_obj_set_style_text_font(lbl_title, title_fonts[i], 0);
        lv_obj_set_style_bg_opa(lbl_title, LV_OPA_TRANSP, 0);
        lv_label_set_text(lbl_title, title_texts[i]);

        /* 数值区：双拼右块 x = half_w，其余 x = 0 */
        lv_obj_t *val_zone = lv_obj_create(s_left_anchor);
        lv_obj_set_pos(val_zone, comp_x, c->y + c->title_h);
        lv_obj_set_size(val_zone, c->w, c->val_h);
        lv_obj_set_style_bg_opa(val_zone, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(val_zone, 1, 0);
        lv_obj_set_style_border_color(val_zone, lv_color_hex(0x00FF00), 0);
        lv_obj_set_style_pad_all(val_zone, 0, 0);
        lv_obj_set_style_clip_corner(val_zone, true, 0);
        lv_obj_clear_flag(val_zone, LV_OBJ_FLAG_SCROLLABLE);

        /* 数值文字 */
        lv_obj_t *lbl_val = lv_label_create(val_zone);
        lv_obj_set_pos(lbl_val, 4, 0);
        lv_obj_set_size(lbl_val, c->w - 8, c->val_h);
        lv_label_set_long_mode(lbl_val, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(lbl_val, comp_align, 0);
        lv_obj_set_style_text_color(lbl_val, AREX_GREEN, 0);
        lv_obj_set_style_text_font(lbl_val, val_fonts[i], 0);
        lv_obj_set_style_bg_opa(lbl_val, LV_OPA_TRANSP, 0);
        if (i == 0) {
            /* DEPTH 大字 */
            lv_label_set_text(lbl_val, "45.2");
            lv_obj_set_style_text_letter_space(lbl_val, -2, 0);
        } else if (i == 1) {
            lv_label_set_text(lbl_val, "0");
            s_lbl_ndl = lbl_val;
        } else if (i == 2) {
            lv_label_set_text(lbl_val, "24'");
            s_lbl_tts = lbl_val;
        } else if (i == 3) {
            lv_label_set_text(lbl_val, "210");
            s_lbl_pod1 = lbl_val;
        } else if (i == 4) {
            lv_label_set_text(lbl_val, "195");
            lv_obj_set_style_text_color(lbl_val, AREX_LIGHT, 0);
            s_lbl_pod2 = lbl_val;
        } else if (i == 5) {
            lv_label_set_text(lbl_val, "85%");
            s_lbl_batt = lbl_val;
        } else if (i == 6) {
            lv_label_set_text(lbl_val, "10:45");
        } else if (i == 7) {
            lv_label_set_text(lbl_val, "TX 18/45");
            s_lbl_gas_name = lbl_val;
        } else if (i == 8) {
            lv_label_set_text(lbl_val, "38:14");
            s_lbl_time = lbl_val;
        }

        /* NEXT STOP 标签 — 位置待重新规划，暂时隐藏 */
        if (i == 0) {
            lv_obj_t *lbl_ns = lv_label_create(s_left_anchor);
            lv_obj_set_pos(lbl_ns, AREX_LEFT_ANCHOR_W / 2 + 8, 132);
            lv_obj_set_style_text_color(lbl_ns, AREX_LIGHT, 0);
            lv_obj_set_style_text_font(lbl_ns, AREX_FONT_SMALL, 0);
            lv_obj_set_style_bg_opa(lbl_ns, LV_OPA_TRANSP, 0);
            lv_label_set_text(lbl_ns, "NEXT STOP");
            lv_obj_add_flag(lbl_ns, LV_OBJ_FLAG_HIDDEN);   /* 坐标待重新计算后再启用 */
            s_lbl_next_stop = lv_label_create(s_left_anchor);
            lv_obj_set_pos(s_lbl_next_stop, AREX_LEFT_ANCHOR_W / 2 + 8, 148);
            lv_obj_set_style_text_color(s_lbl_next_stop, AREX_LIGHT, 0);
            lv_obj_set_style_text_font(s_lbl_next_stop, AREX_FONT_MEDIUM, 0);
            lv_obj_set_style_bg_opa(s_lbl_next_stop, LV_OPA_TRANSP, 0);
            lv_label_set_text(s_lbl_next_stop, "21m 3'");
            lv_obj_add_flag(s_lbl_next_stop, LV_OBJ_FLAG_HIDDEN);
        }

        /* PO2 标签行 — 位置待重新规划，暂时隐藏 */
        if (i == 0) {
            lv_obj_t *lbl_po2_cap = lv_label_create(s_left_anchor);
            lv_obj_set_pos(lbl_po2_cap, AREX_LEFT_ANCHOR_W / 2 + 8, 192);
            lv_obj_set_style_text_color(lbl_po2_cap, AREX_LIGHT, 0);
            lv_obj_set_style_text_font(lbl_po2_cap, AREX_FONT_SMALL, 0);
            lv_obj_set_style_bg_opa(lbl_po2_cap, LV_OPA_TRANSP, 0);
            lv_label_set_text(lbl_po2_cap, "PO2");
            lv_obj_add_flag(lbl_po2_cap, LV_OBJ_FLAG_HIDDEN);

            static const lv_coord_t ppo2_x[5] = {
                AREX_LEFT_ANCHOR_W / 2 + 30,
                AREX_LEFT_ANCHOR_W / 2 + 60,
                AREX_LEFT_ANCHOR_W / 2 + 68,
                AREX_LEFT_ANCHOR_W / 2 + 96,
                AREX_LEFT_ANCHOR_W / 2 + 104
            };

            s_ppo2_vals[0] = lv_label_create(s_left_anchor);
            lv_obj_set_pos(s_ppo2_vals[0], ppo2_x[0], 210);
            lv_obj_set_style_text_color(s_ppo2_vals[0], AREX_GREEN, 0);
            lv_obj_set_style_text_font(s_ppo2_vals[0], AREX_FONT_SMALL, 0);
            lv_obj_set_style_bg_opa(s_ppo2_vals[0], LV_OPA_TRANSP, 0);
            lv_label_set_text(s_ppo2_vals[0], "1.2");
            lv_obj_add_flag(s_ppo2_vals[0], LV_OBJ_FLAG_HIDDEN);

            s_ppo2_seps[0] = lv_label_create(s_left_anchor);
            lv_obj_set_pos(s_ppo2_seps[0], ppo2_x[1], 210);
            lv_obj_set_style_text_color(s_ppo2_seps[0], AREX_GREEN, 0);
            lv_obj_set_style_text_font(s_ppo2_seps[0], AREX_FONT_SMALL, 0);
            lv_obj_set_style_text_opa(s_ppo2_seps[0], LV_OPA_30, 0);
            lv_obj_set_style_bg_opa(s_ppo2_seps[0], LV_OPA_TRANSP, 0);
            lv_label_set_text(s_ppo2_seps[0], "|");
            lv_obj_add_flag(s_ppo2_seps[0], LV_OBJ_FLAG_HIDDEN);

            s_ppo2_vals[1] = lv_label_create(s_left_anchor);
            lv_obj_set_pos(s_ppo2_vals[1], ppo2_x[2], 210);
            lv_obj_set_style_text_color(s_ppo2_vals[1], AREX_GREEN, 0);
            lv_obj_set_style_text_font(s_ppo2_vals[1], AREX_FONT_SMALL, 0);
            lv_obj_set_style_bg_opa(s_ppo2_vals[1], LV_OPA_TRANSP, 0);
            lv_label_set_text(s_ppo2_vals[1], "1.2");
            lv_obj_add_flag(s_ppo2_vals[1], LV_OBJ_FLAG_HIDDEN);

            s_ppo2_seps[1] = lv_label_create(s_left_anchor);
            lv_obj_set_pos(s_ppo2_seps[1], ppo2_x[3], 210);
            lv_obj_set_style_text_color(s_ppo2_seps[1], AREX_GREEN, 0);
            lv_obj_set_style_text_font(s_ppo2_seps[1], AREX_FONT_SMALL, 0);
            lv_obj_set_style_text_opa(s_ppo2_seps[1], LV_OPA_30, 0);
            lv_obj_set_style_bg_opa(s_ppo2_seps[1], LV_OPA_TRANSP, 0);
            lv_label_set_text(s_ppo2_seps[1], "|");
            lv_obj_add_flag(s_ppo2_seps[1], LV_OBJ_FLAG_HIDDEN);

            s_ppo2_vals[2] = lv_label_create(s_left_anchor);
            lv_obj_set_pos(s_ppo2_vals[2], ppo2_x[4], 210);
            lv_obj_set_style_text_color(s_ppo2_vals[2], AREX_GREEN, 0);
            lv_obj_set_style_text_font(s_ppo2_vals[2], AREX_FONT_SMALL, 0);
            lv_obj_set_style_bg_opa(s_ppo2_vals[2], LV_OPA_TRANSP, 0);
            lv_label_set_text(s_ppo2_vals[2], "1.3");
            lv_obj_add_flag(s_ppo2_vals[2], LV_OBJ_FLAG_HIDDEN);
        }

        s_anchor_titles[i] = title_zone;
        s_anchor_vals[i]   = val_zone;

        if (i == 0) s_lbl_depth = lbl_val;
    }

    /* DEPTH 特殊处理: 负字间距让大字更紧凑 */
    if (s_lbl_depth) {
        lv_obj_set_style_text_letter_space(s_lbl_depth, -2, 0);
    }

    /* NEXT STOP 独立标签 (不占锚点格子,放 DEPTH 下方) */
    /* (简化版暂不实现，后续可加) */
}

/* =========================================================
 * 右侧区域: Safe Zone 动态排版
 *
 * Safe Zone 内部布局 (绝对坐标):
 *
 *  [s_left_anchor] | [s_right_cont]
 *  (Tech 模式)
 *
 * Safe Zone 坐标原点为 (0,0) 在 s_safe_zone 左上角。
 * 所有子组件的坐标以此为基准计算。
 * ========================================================= */
static void safe_zone_reposition(void)
{
    if (!s_safe_zone) return;

    /* 1. 安全区容器定位 */
    lv_obj_set_size(s_safe_zone, g_sys_config.safe_zone_w, g_sys_config.safe_zone_h);
    lv_obj_align(s_safe_zone, LV_ALIGN_CENTER,
                 g_sys_config.offset_x, g_sys_config.offset_y);

    /* 2. 计算左右分界线 */
    uint16_t gap = g_sys_config.gap_u * AREX_BASE_U;
    uint16_t right_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - gap;

    /* 3. 定位左侧锚点 */
    if (g_sys_config.layout_order == AREX_ORDER_NORMAL) {
        lv_obj_set_pos(s_left_anchor, 0, 0);
        lv_obj_set_pos(s_right_cont, (lv_coord_t)(AREX_LEFT_ANCHOR_W + gap), 0);
    } else {
        /* 翻转: 右侧容器放左边, 左侧锚点放右边 */
        lv_obj_set_pos(s_right_cont, 0, 0);
        lv_obj_set_pos(s_left_anchor, (lv_coord_t)right_w + gap, 0);
    }

    /* 4. 设置右侧容器尺寸 */
    lv_obj_set_size(s_right_cont, right_w, g_sys_config.safe_zone_h);

    s_cached_right_w = right_w;
}

/* =========================================================
 * 右侧区域: clip container + tileview
 * ========================================================= */
static void right_panel_create(void)
{
    uint16_t right_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                       - g_sys_config.gap_u * AREX_BASE_U;
    s_cached_right_w = right_w;

    s_right_cont = lv_obj_create(s_safe_zone);
    lv_obj_set_size(s_right_cont, right_w, g_sys_config.safe_zone_h);
    lv_obj_set_pos(s_right_cont, (lv_coord_t)(AREX_LEFT_ANCHOR_W + g_sys_config.gap_u * AREX_BASE_U), 0);
    lv_obj_set_style_bg_color(s_right_cont, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(s_right_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_right_cont, 0, 0);
    lv_obj_set_style_border_width(s_right_cont, 0, 0);
    lv_obj_clear_flag(s_right_cont, LV_OBJ_FLAG_SCROLLABLE);

    /* Tileview */
    s_tileview = lv_tileview_create(s_right_cont);
    lv_obj_set_size(s_tileview, right_w, g_sys_config.safe_zone_h);
    lv_obj_set_pos(s_tileview, 0, 0);
    lv_obj_set_style_bg_color(s_tileview, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(s_tileview, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_tileview, 0, 0);
    lv_obj_set_scrollbar_mode(s_tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_tileview, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建 tiles */
    uint8_t count = arex_card_count();
    for (uint8_t i = 0; i < count; i++) {
        arex_card_reg_t *card = arex_card_get(i);
        if (!card) continue;

        lv_obj_t *tile = lv_tileview_add_tile(s_tileview, 0, i,
                                               LV_DIR_TOP | LV_DIR_BOTTOM);
        lv_obj_set_style_bg_color(tile, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(tile, 0, 0);
        lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        card->tile_obj = tile;

        if (card->create_cb) card->create_cb(tile);
    }

    /* Scroll dots */
    lv_obj_t *dot_cont = lv_obj_create(s_right_cont);
    uint16_t dot_cont_h = AREX_DASH_CARD_COUNT * 14;
    lv_obj_set_size(dot_cont, 10, dot_cont_h);
    lv_obj_set_style_bg_opa(dot_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dot_cont, 0, 0);
    lv_obj_set_style_pad_all(dot_cont, 0, 0);
    lv_obj_set_flex_flow(dot_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dot_cont, LV_FLEX_ALIGN_SPACE_EVENLY,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(dot_cont, LV_SCROLLBAR_MODE_OFF);

    /* Dots 位置跟随配置 */
    if (g_sys_config.dots_position == AREX_DOTS_LEFT) {
        lv_obj_align(dot_cont, LV_ALIGN_LEFT_MID, 8, 0);
    } else if (g_sys_config.dots_position == AREX_DOTS_RIGHT) {
        lv_obj_align(dot_cont, LV_ALIGN_RIGHT_MID, -8, 0);
    } else if (g_sys_config.dots_position == AREX_DOTS_BOTTOM) {
        lv_obj_align(dot_cont, LV_ALIGN_BOTTOM_MID, 0, -8);
    }
    /* AREX_DOTS_NONE 时不显示 dot_cont */

    for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++) {
        s_scroll_dots[i] = lv_obj_create(dot_cont);
        lv_obj_set_size(s_scroll_dots[i], 6, 6);
        lv_obj_set_style_radius(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_bg_color(s_scroll_dots[i], AREX_DARK, 0);
        lv_obj_set_style_bg_opa(s_scroll_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_pad_all(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_shadow_width(s_scroll_dots[i], 0, 0);
        lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_SCROLLABLE);
        if (g_sys_config.dots_position == AREX_DOTS_NONE)
            lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
    }

    arex_screen_update_scroll_dots(0, true);
}

/* =========================================================
 * Safe Zone 容器重建 (配置变更后调用)
 * 不重建 cards，只重建布局框架
 * ========================================================= */
void arex_screen_rebuild_layout(void)
{
    /* 重建左侧锚点排版 */
    if (s_left_anchor) {
        left_anchor_rebuild();
    }

    /* 重建 Safe Zone 内部定位 */
    safe_zone_reposition();

    /* 重建滚动指示器 */
    for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++) {
        if (s_scroll_dots[i]) {
            if (g_sys_config.dots_position == AREX_DOTS_NONE)
                lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* =========================================================
 * Tileview 重建 (卡片顺序变更时调用)
 * ========================================================= */
void arex_screen_rebuild_tileview(void)
{
    uint8_t count = arex_card_count();
    for (uint8_t i = 0; i < count; i++) {
        arex_card_reg_t *card = arex_card_get_by_id(i);
        if (card) card->tile_obj = NULL;
    }

    if (s_right_cont) {
        lv_obj_del(s_right_cont);
        s_right_cont = NULL;
        s_tileview   = NULL;
        for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++)
            s_scroll_dots[i] = NULL;
    }

    right_panel_create();
    arex_screen_update_scroll_dots(
        g_ui.dash_card > 0 ? g_ui.dash_card - 1 : 0, true);
}

/* =========================================================
 * Wall indicator
 * ========================================================= */
static lv_obj_t *make_wall(lv_obj_t *parent, lv_coord_t y)
{
    lv_obj_t *w = lv_obj_create(parent);
    lv_obj_set_size(w, s_cached_right_w > 0 ? s_cached_right_w : 420, 90);
    lv_obj_set_pos(w, 0, y);
    lv_obj_set_style_bg_color(w, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(w, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(w, AREX_DARK, 0);
    lv_obj_set_style_border_width(w, 2, 0);
    lv_obj_set_style_border_side(w, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(w, 0, 0);
    lv_obj_add_flag(w, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(w, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *txt = lv_label_create(w);
    lv_obj_set_style_text_color(txt, AREX_GREEN, 0);
    lv_obj_set_style_text_font(txt, AREX_FONT_TITLE, 0);
    lv_obj_set_width(txt, s_cached_right_w > 0 ? s_cached_right_w : 420);
    lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(txt, 0, 12);
    lv_label_set_text(txt, "");

    lv_obj_t *blk = lv_label_create(w);
    lv_obj_set_style_text_color(blk, AREX_GREEN, 0);
    lv_obj_set_style_text_font(blk, AREX_FONT_MEDIUM, 0);
    lv_obj_set_width(blk, s_cached_right_w > 0 ? s_cached_right_w : 420);
    lv_obj_set_style_text_align(blk, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(blk, 0, 50);
    lv_label_set_text(blk, "");

    return w;
}

static void wall_create(void)
{
    uint16_t wall_y_bottom = g_sys_config.safe_zone_h > 0 ? g_sys_config.safe_zone_h - 90 : 390;

    s_wall_top    = make_wall(s_right_cont, 0);
    s_wall_bottom = make_wall(s_right_cont, wall_y_bottom);
    s_wall_text_top      = lv_obj_get_child(s_wall_top, 0);
    s_wall_blocks_top    = lv_obj_get_child(s_wall_top, 1);
    s_wall_text_bottom   = lv_obj_get_child(s_wall_bottom, 0);
    s_wall_blocks_bottom = lv_obj_get_child(s_wall_bottom, 1);
}

/* =========================================================
 * Modal overlay
 * ========================================================= */
static void modal_create(void)
{
    s_modal = lv_obj_create(s_right_cont);
    lv_obj_set_size(s_modal, s_cached_right_w > 0 ? s_cached_right_w : 420,
                     g_sys_config.safe_zone_h);
    lv_obj_set_pos(s_modal, 0, 0);
    lv_obj_set_style_bg_color(s_modal, lv_color_make(0,0,0), 0);
    lv_obj_set_style_bg_opa(s_modal, 242, 0);
    lv_obj_set_style_border_width(s_modal, 0, 0);
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);

    s_modal_box = lv_obj_create(s_modal);
    lv_obj_set_size(s_modal_box, 400, 260);
    lv_obj_center(s_modal_box);
    lv_obj_set_style_bg_color(s_modal_box, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(s_modal_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_modal_box, AREX_GREEN, 0);
    lv_obj_set_style_border_width(s_modal_box, 4, 0);
    lv_obj_set_style_radius(s_modal_box, 0, 0);
    lv_obj_set_style_pad_all(s_modal_box, 30, 0);
}

/* =========================================================
 * Sub-menu layer
 * ========================================================= */
static void submenu_layer_create(void)
{
    uint16_t sub_w = s_cached_right_w > 0 ? s_cached_right_w : 420;

    s_submenu_layer = lv_obj_create(s_right_cont);
    lv_obj_set_size(s_submenu_layer, sub_w, g_sys_config.safe_zone_h);
    lv_obj_set_pos(s_submenu_layer, sub_w, 0);
    lv_obj_set_style_bg_color(s_submenu_layer, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(s_submenu_layer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_submenu_layer, 0, 0);
    lv_obj_set_style_pad_all(s_submenu_layer, 0, 0);
    lv_obj_clear_flag(s_submenu_layer, LV_OBJ_FLAG_SCROLLABLE);

    s_submenu_title = lv_label_create(s_submenu_layer);
    lv_obj_set_style_text_color(s_submenu_title, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(s_submenu_title, AREX_FONT_TITLE, 0);
    lv_obj_set_pos(s_submenu_title, 16, 12);
    lv_label_set_text(s_submenu_title, "> SUB MENU");

    lv_obj_t *title_line = lv_obj_create(s_submenu_layer);
    lv_obj_set_size(title_line, sub_w - 32, 2);
    lv_obj_set_pos(title_line, 16, 38);
    lv_obj_set_style_bg_color(title_line, AREX_DARK, 0);
    lv_obj_set_style_bg_opa(title_line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(title_line, 0, 0);
    lv_obj_set_style_pad_all(title_line, 0, 0);

    s_submenu_list = lv_obj_create(s_submenu_layer);
    lv_obj_set_size(s_submenu_list, sub_w - 32, g_sys_config.safe_zone_h - 50);
    lv_obj_set_pos(s_submenu_list, 16, 50);
    lv_obj_set_style_bg_opa(s_submenu_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_submenu_list, 0, 0);
    lv_obj_set_style_pad_all(s_submenu_list, 0, 0);
    lv_obj_set_style_pad_row(s_submenu_list, 8, 0);
    lv_obj_set_flex_flow(s_submenu_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_submenu_list, LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(s_submenu_list, LV_SCROLLBAR_MODE_OFF);
}

/* =========================================================
 * arex_screen_create — 公开入口
 * ========================================================= */
void arex_screen_create(void)
{
    styles_init();

    s_scr = lv_obj_create(NULL);
    lv_obj_add_style(s_scr, &s_style_screen, 0);

    /* 安全区容器 (相对于 s_scr 居中定位) */
    s_safe_zone = lv_obj_create(s_scr);
    lv_obj_set_size(s_safe_zone, g_sys_config.safe_zone_w, g_sys_config.safe_zone_h);
    lv_obj_align(s_safe_zone, LV_ALIGN_CENTER,
                 g_sys_config.offset_x, g_sys_config.offset_y);
    lv_obj_set_style_bg_opa(s_safe_zone, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_safe_zone, 0, 0);
    lv_obj_set_style_pad_all(s_safe_zone, 0, 0);
    lv_obj_clear_flag(s_safe_zone, LV_OBJ_FLAG_SCROLLABLE);

    /* 安全区危险边框 */
    if (arex_safe_zone_in_danger()) {
        lv_obj_set_style_border_color(s_safe_zone, lv_color_make(255,0,0), 0);
        lv_obj_set_style_border_width(s_safe_zone, 3, 0);
    } else {
        lv_obj_set_style_border_color(s_safe_zone, AREX_DARK, 0);
        lv_obj_set_style_border_width(s_safe_zone, 1, 0);
    }

    left_anchor_create();
    right_panel_create();
    wall_create();
    submenu_layer_create();
    modal_create();

    lv_scr_load(s_scr);
}

/* =========================================================
 * Tileview 导航
 * ========================================================= */
void arex_screen_scroll_to_card(uint8_t tile_pos)
{
    if (tile_pos >= AREX_CARD_COUNT) return;
    arex_card_reg_t *card = arex_card_get(tile_pos);
    if (!card || !card->tile_obj) return;

    if (tile_pos == 0) {
        lv_anim_del(s_tileview, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_obj_set_y(s_tileview, 0);
    }

    lv_obj_set_tile(s_tileview, card->tile_obj, LV_ANIM_ON);

    if (tile_pos >= 1) {
        arex_screen_update_scroll_dots(tile_pos - 1, true);
    } else {
        arex_screen_update_scroll_dots(0, false);
    }
}

/* =========================================================
 * 左侧面板刷新 (仅更新文字，不重建布局)
 * ========================================================= */
void arex_screen_refresh_left_panel(void)
{
    char buf[32];

    snprintf(buf, sizeof(buf), "%.1f", g_sensor_data.depth);
    if (s_lbl_depth) lv_label_set_text(s_lbl_depth, buf);

    snprintf(buf, sizeof(buf), "%d", g_sensor_data.ndl);
    if (s_lbl_ndl) lv_label_set_text(s_lbl_ndl, buf);

    snprintf(buf, sizeof(buf), "%d'", g_sensor_data.tts);
    if (s_lbl_tts) lv_label_set_text(s_lbl_tts, buf);

    snprintf(buf, sizeof(buf), "%dm %d'", g_sensor_data.next_stop_m,
             g_sensor_data.next_stop_min);
    if (s_lbl_next_stop) lv_label_set_text(s_lbl_next_stop, buf);

    snprintf(buf, sizeof(buf), "%.0f", g_sensor_data.pod1_bar);
    if (s_lbl_pod1) lv_label_set_text(s_lbl_pod1, buf);

    snprintf(buf, sizeof(buf), "%.0f", g_sensor_data.pod2_bar);
    if (s_lbl_pod2) lv_label_set_text(s_lbl_pod2, buf);

    if (s_lbl_gas_name) {
        lv_label_set_text(s_lbl_gas_name, g_sensor_data.gas_name);
    }

    if (s_ppo2_vals[0]) {
        char pbuf[8];
        snprintf(pbuf, sizeof(pbuf), "%.1f", g_sensor_data.ppo2[0]);
        lv_label_set_text(s_ppo2_vals[0], pbuf);
        snprintf(pbuf, sizeof(pbuf), "%.1f", g_sensor_data.ppo2[1]);
        lv_label_set_text(s_ppo2_vals[1], pbuf);
        snprintf(pbuf, sizeof(pbuf), "%.1f", g_sensor_data.ppo2[2]);
        lv_label_set_text(s_ppo2_vals[2], pbuf);
    }

    uint32_t s = g_sensor_data.dive_time_s;
    snprintf(buf, sizeof(buf), "%02d:%02d", s / 60, s % 60);
    if (s_lbl_time) lv_label_set_text(s_lbl_time, buf);
}

/* =========================================================
 * Wall indicators
 * ========================================================= */
static const char *charge_blocks[] = {
    "[   ]   [   ]   [   ]",
    "[ \xE2\x96\xA0 ]   [   ]   [   ]",
    "[ \xE2\x96\xA0 ]   [ \xE2\x96\xA0 ]   [   ]",
    "[ \xE2\x96\xA0 ]   [ \xE2\x96\xA0 ]   [ \xE2\x96\xA0 ]",
};

static void wall_nudge_tileview(lv_coord_t offset_y)
{
    lv_anim_del(s_tileview, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_coord_t cur_y = lv_obj_get_y(s_tileview);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_tileview);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, cur_y, offset_y);
    lv_anim_set_time(&a, 350);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

void arex_screen_show_wall(wall_side_t side, uint8_t charge, const char *text)
{
    if (charge > 3) charge = 3;

    lv_obj_t *wall    = (side == WALL_TOP) ? s_wall_top    : s_wall_bottom;
    lv_obj_t *txt_lbl = (side == WALL_TOP) ? s_wall_text_top    : s_wall_text_bottom;
    lv_obj_t *blk_lbl = (side == WALL_TOP) ? s_wall_blocks_top  : s_wall_blocks_bottom;

    lv_label_set_text(txt_lbl, text);
    lv_label_set_text(blk_lbl, charge_blocks[charge]);
    lv_obj_clear_flag(wall, LV_OBJ_FLAG_HIDDEN);

    lv_coord_t nudge = (lv_coord_t)(charge * 20);
    wall_nudge_tileview(side == WALL_TOP ? nudge : -nudge);
}

void arex_screen_hide_walls(void)
{
    lv_obj_add_flag(s_wall_top,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_wall_bottom, LV_OBJ_FLAG_HIDDEN);
    lv_coord_t cur_y = lv_obj_get_y(s_tileview);
    if (cur_y == 0) return;
    lv_anim_del(s_tileview, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_tileview);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, cur_y, 0);
    lv_anim_set_time(&a, 350);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

void arex_screen_hide_walls_snap(void)
{
    lv_obj_add_flag(s_wall_top,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_wall_bottom, LV_OBJ_FLAG_HIDDEN);
    lv_anim_del(s_tileview, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_obj_set_y(s_tileview, 0);
}

/* =========================================================
 * Scroll dots
 * ========================================================= */
void arex_screen_update_scroll_dots(uint8_t active_idx, bool visible)
{
    bool in_dash_or_edit = (g_ui.state == UI_DASH || g_ui.state == UI_EDIT_GAS);
    bool dots_enabled = (g_sys_config.dots_position != AREX_DOTS_NONE);

    for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++) {
        if (!s_scroll_dots[i]) continue;
        bool show = visible && in_dash_or_edit && dots_enabled;
        if (!show) {
            lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
        if (i == active_idx) {
            lv_obj_set_style_bg_color(s_scroll_dots[i], AREX_GREEN, 0);
            lv_obj_set_style_shadow_width(s_scroll_dots[i], 8, 0);
            lv_obj_set_style_shadow_color(s_scroll_dots[i], AREX_GREEN, 0);
            lv_obj_set_style_shadow_opa(s_scroll_dots[i], 255, 0);
        } else {
            lv_obj_set_style_bg_color(s_scroll_dots[i], AREX_DARK, 0);
            lv_obj_set_style_shadow_width(s_scroll_dots[i], 0, 0);
            lv_obj_set_style_shadow_opa(s_scroll_dots[i], 0, 0);
        }
    }
}

/* =========================================================
 * Info / Setup list
 * ========================================================= */
void arex_screen_set_info_selection(uint8_t idx)
{
    if (!s_info_list) return;
    uint32_t cnt = lv_obj_get_child_cnt(s_info_list);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *item = lv_obj_get_child(s_info_list, i);
        lv_obj_t *lbl  = lv_obj_get_child(item, 0);
        if (i == idx) {
            lv_obj_set_style_bg_color(item, AREX_GREEN, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_BLACK, 0);
        } else {
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        }
    }
}

uint8_t arex_screen_info_item_count(void)
{
    if (!s_info_list) return 0;
    return (uint8_t)lv_obj_get_child_cnt(s_info_list);
}

void arex_screen_set_setup_selection(uint8_t idx)
{
    if (!s_setup_list) return;
    uint32_t cnt = lv_obj_get_child_cnt(s_setup_list);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *item  = lv_obj_get_child(s_setup_list, i);
        lv_obj_t *lbl   = lv_obj_get_child(item, 0);
        lv_obj_t *badge = lv_obj_get_child(item, 1);
        if (i == idx) {
            lv_obj_set_style_bg_color(item, AREX_GREEN, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl)   lv_obj_set_style_text_color(lbl,   AREX_BLACK, 0);
            if (badge) lv_obj_set_style_text_color(badge, AREX_BLACK, 0);
        } else {
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl)   lv_obj_set_style_text_color(lbl,   AREX_GREEN, 0);
            if (badge) lv_obj_set_style_text_color(badge, AREX_LIGHT, 0);
        }
    }
}

uint8_t arex_screen_setup_item_count(void)
{
    if (!s_setup_list) return 0;
    return (uint8_t)lv_obj_get_child_cnt(s_setup_list);
}

void arex_screen_register_info_list(lv_obj_t *list)  { s_info_list  = list; }
void arex_screen_register_setup_list(lv_obj_t *list) { s_setup_list = list; }

/* =========================================================
 * Sub-menu layer
 * ========================================================= */
static void submenu_slide_in(void)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_submenu_layer);
    uint16_t sub_w = s_cached_right_w > 0 ? s_cached_right_w : 420;
    lv_anim_set_values(&a, sub_w, 0);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 250);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void submenu_slide_out(void)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_submenu_layer);
    uint16_t sub_w = s_cached_right_w > 0 ? s_cached_right_w : 420;
    lv_anim_set_values(&a, 0, sub_w);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 250);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);
}

static void submenu_populate(const char *title, const char **items, uint8_t count)
{
    lv_label_set_text(s_submenu_title, title);
    lv_obj_clean(s_submenu_list);

    for (uint8_t i = 0; i < count; i++) {
        lv_obj_t *item = lv_obj_create(s_submenu_list);
        lv_obj_set_size(item, LV_PCT(100), 48);
        lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, AREX_DARK, 0);
        lv_obj_set_style_border_width(item, 2, 0);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, 0);          /* 零边距，防止撑高 */
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_clip_corner(item, true, 0);   /* 强制裁剪溢出内容 */

        lv_obj_t *lbl = lv_label_create(item);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        lv_obj_set_style_text_font(lbl, AREX_FONT_TITLE, 0);
        lv_obj_set_size(lbl, LV_PCT(100), LV_SIZE_CONTENT);  /* 高度自适应，让 align 居中生效 */
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_label_set_text(lbl, items[i]);
    }
    arex_screen_set_submenu_selection(0);
}

void arex_screen_set_submenu_selection(uint8_t idx)
{
    uint32_t cnt = lv_obj_get_child_cnt(s_submenu_list);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *item = lv_obj_get_child(s_submenu_list, i);
        lv_obj_t *lbl  = lv_obj_get_child(item, 0);
        /* 正在编辑的 item 由 begin_edit_value 单独管理，不参与选中态刷新 */
        if (g_ui.edit_ctx.active && (uint8_t)i == g_ui.edit_ctx.item_index) continue;
        if (i == idx) {
            lv_obj_set_style_bg_color(item, AREX_GREEN, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_BLACK, 0);
        } else {
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        }
    }
}

/* INFO sub-menu */
static const char *s_info_titles[] = {
    "> LAST DIVE", "> DIVE PLAN", "> TISSUE & TOX", "> GAS & CALC", "> SENSOR & DEVICE"
};

static char s_info_str[5][5][32];
static const char *s_info_dyn[5][6];

static void build_info_submenu(uint8_t idx)
{
    uint8_t n = 0;
    switch (idx) {
        case 0:
            snprintf(s_info_str[0][0], 32, "MAX DEPTH: %dm", (int)g_sensor_data.depth);
            snprintf(s_info_str[0][1], 32, "DIVE TIME: %dm", (int)(g_sensor_data.dive_time_s / 60));
            s_info_dyn[0][n++] = s_info_str[0][0];
            s_info_dyn[0][n++] = s_info_str[0][1];
            s_info_dyn[0][n++] = "SURFACE INT: 2h 10m";
            s_info_dyn[0][n++] = "< BACK";
            break;
        case 1:
            s_info_dyn[1][n++] = "VIEW PROFILE";
            s_info_dyn[1][n++] = "RECALCULATE";
            s_info_dyn[1][n++] = "< BACK";
            break;
        case 2:
            snprintf(s_info_str[2][0], 32, "GF: %d/%d", 30, 70);
            snprintf(s_info_str[2][1], 32, "CNS: %d%%", g_sensor_data.cns_pct);
            snprintf(s_info_str[2][2], 32, "OTU: %d", g_sensor_data.otu);
            s_info_dyn[2][n++] = "VIEW BAR GRAPH";
            s_info_dyn[2][n++] = s_info_str[2][0];
            s_info_dyn[2][n++] = s_info_str[2][1];
            s_info_dyn[2][n++] = s_info_str[2][2];
            s_info_dyn[2][n++] = "< BACK";
            break;
        case 3:
            snprintf(s_info_str[3][0], 32, "GAS 1: %s", g_sensor_data.gas_name);
            s_info_dyn[3][n++] = s_info_str[3][0];
            s_info_dyn[3][n++] = "ALGO: ZHL-16C";
            s_info_dyn[3][n++] = "< BACK";
            break;
        case 4:
            snprintf(s_info_str[4][0], 32, "POD 1: %.0f BAR", g_sensor_data.pod1_bar);
            snprintf(s_info_str[4][1], 32, "POD 2: %.0f BAR", g_sensor_data.pod2_bar);
            snprintf(s_info_str[4][2], 32, "BATTERY: %.0f%%", g_sensor_data.battery_pct);
            snprintf(s_info_str[4][3], 32, "TEMP: 24C");
            s_info_dyn[4][n++] = s_info_str[4][0];
            s_info_dyn[4][n++] = s_info_str[4][1];
            s_info_dyn[4][n++] = s_info_str[4][2];
            s_info_dyn[4][n++] = s_info_str[4][3];
            s_info_dyn[4][n++] = "< BACK";
            break;
        default:
            s_info_dyn[idx][n++] = "< BACK";
            break;
    }
    s_info_dyn[idx][n] = NULL;
}

void arex_screen_open_info_submenu(uint8_t item_idx)
{
    if (item_idx >= 5) return;
    build_info_submenu(item_idx);
    uint8_t count = 0;
    while (count < 6 && s_info_dyn[item_idx][count]) count++;
    submenu_populate(s_info_titles[item_idx], s_info_dyn[item_idx], count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.sub_parent     = UI_INFO;
    g_ui.state          = UI_SUB_MENU;
    submenu_slide_in();
}

/* SETUP sub-menu */
static const char *s_setup_sub[][7] = {
    { "SELECT AIR", "SELECT NX 32", "SELECT TX 18/45", "SELECT O2 100%", "< BACK", NULL },
    { "LOW (GF 40/85)", "MED (GF 30/70)", "HIGH (GF 20/65)", "< BACK", NULL },
    { "LOW", "MED", "HIGH", "MAX", "< BACK", NULL },
    { "START CALIBRATION", "< BACK", NULL },
    { "MODE SETUP >", "DIVE SETUP >", "AI SETUP >", "ALERTS SETUP >", "DISPLAY / SYS >", "< BACK", NULL },
};
static const char *s_setup_titles[] = {
    "> GAS SWITCH", "> CONSERVATISM", "> BRIGHTNESS", "> COMPASS CAL", "> SYSTEM SETUP"
};

void arex_screen_open_setup_submenu(uint8_t item_idx)
{
    if (item_idx >= 5) return;
    uint8_t count = 0;
    while (count < 7 && s_setup_sub[item_idx][count]) count++;
    submenu_populate(s_setup_titles[item_idx], s_setup_sub[item_idx], count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.sub_parent     = UI_SETUP;
    g_ui.state          = UI_SUB_MENU;
    submenu_slide_in();
}

/* Nested sub-menus */
static const char *s_nested_mode_setup[]   = { "AIR", "NITROX", "3 GAS NX", "GAUGE", "< BACK", NULL };
static const char *s_nested_ai_setup[]     = { "PAIR T1", "PAIR T2", "GTR MODE: ON", "< BACK", NULL };
static const char *s_nested_alerts_setup[] = { "DEPTH ALARM: 40m", "TIME ALARM: 60m", "LOW NDL: 5m", "TEST VIBRATION", "< BACK", NULL };
static const char *s_nested_display_sys[]  = { "UNITS: METRIC", "DATE & CLOCK", "LOG RATE: 10s", "BLUETOOTH: OFF", "RESET DEFAULTS", "< BACK", NULL };

static char s_modppo2_str[20];
static const char *s_nested_dive_setup[6];

static void build_nested_dive_setup(void)
{
    extern arex_sensor_data_t g_sensor_data;
    (void)g_sensor_data;
    snprintf(s_modppo2_str, sizeof(s_modppo2_str), "MOD PO2: %.1f", 1.4f);
    s_nested_dive_setup[0] = "SALINITY: FRESH";
    s_nested_dive_setup[1] = s_modppo2_str;
    s_nested_dive_setup[2] = "SAFETY STOP: 3 MIN";
    s_nested_dive_setup[3] = "ALTITUDE: AUTO";
    s_nested_dive_setup[4] = "< BACK";
    s_nested_dive_setup[5] = NULL;
}

static const char **nested_items_for(const char *title, uint8_t *out_count)
{
    const char **tbl = NULL;
    if      (strcmp(title, "MODE SETUP")    == 0) tbl = s_nested_mode_setup;
    else if (strcmp(title, "DIVE SETUP")    == 0) { build_nested_dive_setup(); tbl = s_nested_dive_setup; }
    else if (strcmp(title, "AI SETUP")      == 0) tbl = s_nested_ai_setup;
    else if (strcmp(title, "ALERTS SETUP")  == 0) tbl = s_nested_alerts_setup;
    else if (strcmp(title, "DISPLAY / SYS") == 0) tbl = s_nested_display_sys;

    if (tbl && out_count) {
        *out_count = 0;
        while (*out_count < 8 && tbl[*out_count]) (*out_count)++;
    }
    return tbl;
}

static void submenu_history_push(void)
{
    if (g_ui.sub_history_depth >= AREX_SUB_HISTORY_MAX) return;
    arex_sub_history_t *h = &g_ui.sub_history[g_ui.sub_history_depth];
    const char *cur_title = lv_label_get_text(s_submenu_title);
    lv_snprintf(h->title, sizeof(h->title), "%s", cur_title ? cur_title : "");
    h->idx = g_ui.sub_menu_idx;
    g_ui.sub_history_depth++;
}

void arex_screen_open_nested_submenu(const char *title, const char **items, uint8_t count)
{
    submenu_history_push();
    char full_title[40];
    lv_snprintf(full_title, sizeof(full_title), "> %s", title);
    submenu_populate(full_title, items, count);
    g_ui.sub_item_count = count;
    g_ui.sub_menu_idx   = 0;
    g_ui.state = UI_SUB_MENU;
}

void arex_screen_handle_submenu_select(uint8_t item_idx)
{
    if (item_idx >= g_ui.sub_item_count) return;
    lv_obj_t *item = lv_obj_get_child(s_submenu_list, item_idx);
    lv_obj_t *lbl  = item ? lv_obj_get_child(item, 0) : NULL;
    if (!lbl) return;
    const char *text = lv_label_get_text(lbl);
    if (!text) return;

    if (strcmp(text, "< BACK") == 0) {
        arex_screen_close_submenu();
        return;
    }

    const char *raw_title = lv_label_get_text(s_submenu_title);
    char cur_title[40] = {0};
    if (raw_title) {
        const char *p = raw_title;
        if (p[0] == '>' && p[1] == ' ') p += 2;
        lv_snprintf(cur_title, sizeof(cur_title), "%s", p);
    }

    if (text[strlen(text) - 1] == '>') {
        char nested_name[40] = {0};
        size_t len = strlen(text);
        size_t copy_len = (len >= 2) ? len - 2 : 0;
        if (copy_len >= sizeof(nested_name)) copy_len = sizeof(nested_name) - 1;
        memcpy(nested_name, text, copy_len);
        while (copy_len > 0 && nested_name[copy_len - 1] == ' ') {
            nested_name[--copy_len] = '\0';
        }
        uint8_t ncnt = 0;
        const char **nitems = nested_items_for(nested_name, &ncnt);
        if (nitems && ncnt > 0) {
            arex_screen_open_nested_submenu(nested_name, nitems, ncnt);
        }
        return;
    }

    if (strcmp(cur_title, "GAS SWITCH") == 0) {
        const char *gas_name = text;
        if (strncmp(text, "SELECT ", 7) == 0) gas_name = text + 7;
        extern const char *AREX_GAS_NAMES[4];
        for (uint8_t i = 0; i < 4; i++) {
            if (strcmp(AREX_GAS_NAMES[i], gas_name) == 0) {
                g_sensor_data.gas_active_idx = i;
                strncpy(g_sensor_data.gas_name, gas_name, 15);
                g_sensor_data.gas_name[15] = '\0';
                arex_screen_refresh_gas_menu();
                arex_screen_refresh_left_panel();
                arex_screen_close_submenu();
                return;
            }
        }
        return;
    }

    if (strcmp(cur_title, "CONSERVATISM") == 0) {
        arex_screen_update_setup_badge(1, text);
        arex_screen_close_submenu();
        return;
    }

    if (strcmp(cur_title, "BRIGHTNESS") == 0) {
        arex_screen_update_setup_badge(2, text);
        arex_screen_close_submenu();
        return;
    }

    if (strcmp(cur_title, "DIVE SETUP") == 0) {
        if (strncmp(text, "MOD PO2:", 8) == 0 || strncmp(text, "MOD PO2 ", 8) == 0) {
            arex_screen_begin_edit_value(item_idx, 1.4f, 1.0f, 1.6f, 0.1f);
            return;
        }
        arex_screen_show_modal_act(text);
        return;
    }

    arex_screen_show_modal_act(text);
}

void arex_screen_close_submenu(void)
{
    if (g_ui.sub_history_depth > 0) {
        g_ui.sub_history_depth--;
        arex_sub_history_t *h = &g_ui.sub_history[g_ui.sub_history_depth];
        const char *prev_title = h->title;
        if (prev_title[0] == '>' && prev_title[1] == ' ') prev_title += 2;

        bool found = false;
        for (uint8_t i = 0; i < 5 && !found; i++) {
            const char *setup_title_stripped = s_setup_titles[i];
            if (setup_title_stripped[0] == '>' && setup_title_stripped[1] == ' ')
                setup_title_stripped += 2;
            if (strcmp(prev_title, setup_title_stripped) == 0) {
                uint8_t cnt = 0;
                while (cnt < 6 && s_setup_sub[i][cnt]) cnt++;
                submenu_populate(s_setup_titles[i], s_setup_sub[i], cnt);
                g_ui.sub_item_count = cnt;
                g_ui.sub_menu_idx   = h->idx;
                arex_screen_set_submenu_selection(h->idx);
                found = true;
            }
        }
        if (!found) {
            uint8_t ncnt = 0;
            const char **nitems = nested_items_for(prev_title, &ncnt);
            if (nitems && ncnt > 0) {
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

/* =========================================================
 * Setup badge update
 * ========================================================= */
void arex_screen_update_setup_badge(uint8_t item_idx, const char *value)
{
    if (!s_setup_list) return;
    lv_obj_t *item = lv_obj_get_child(s_setup_list, item_idx);
    if (!item) return;
    lv_obj_t *badge = lv_obj_get_child(item, 1);
    if (!badge) return;
    lv_label_set_text(badge, value ? value : "");
}

/* =========================================================
 * Modal
 * ========================================================= */
static void modal_act_timer_cb(lv_timer_t *t)
{
    (void)t;
    arex_screen_hide_modal();
    if (g_ui.state == UI_MODAL_ACT) {
        g_ui.state = UI_SUB_MENU;
    }
    lv_timer_del(t);
}

static void modal_set_content(const char *title, const char *body, const char *hint)
{
    lv_obj_clean(s_modal_box);

    lv_obj_t *t = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(t, AREX_GREEN, 0);
    lv_obj_set_style_text_font(t, AREX_FONT_TITLE, 0);
    lv_label_set_text(t, title);
    lv_obj_set_pos(t, 0, 0);

    lv_obj_t *b = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(b, AREX_GREEN, 0);
    lv_obj_set_style_text_font(b, AREX_FONT_MEDIUM, 0);
    lv_label_set_text(b, body);
    lv_obj_set_pos(b, 0, 40);

    lv_obj_t *h = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(h, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(h, AREX_FONT_SMALL, 0);
    lv_label_set_text(h, hint);
    lv_obj_set_pos(h, 0, 100);
}

void arex_screen_show_modal_act(const char *action_text)
{
    modal_set_content("ACTION", action_text ? action_text : "", "[ ESC TO BACK ]");
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
    g_ui.state = UI_MODAL_ACT;
    lv_timer_create(modal_act_timer_cb, 1000, NULL);
}

void arex_screen_show_modal_gas(void)
{
    uint8_t ci = g_ui.gas_cursor;
    extern const char *AREX_GAS_NAMES[4];
    extern const uint8_t AREX_GAS_MOD_M[4];
    char body[32];
    snprintf(body, sizeof(body), "%s\nMOD: %dm", AREX_GAS_NAMES[ci], AREX_GAS_MOD_M[ci]);

    const char *hint = (g_sensor_data.depth > AREX_GAS_MOD_M[ci])
        ? "[ FATAL: OVER MOD ]"
        : "[ ENTER CONFIRM ]  [ ESC CANCEL ]";

    modal_set_content("CONFIRM GAS", body, hint);
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void arex_screen_show_modal_compass(void)
{
    modal_set_content("CLEAR TARGET?", "REMOVE HEADING MARKER?",
                      "[ ENTER CONFIRM ]  [ ESC CANCEL ]");
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void arex_screen_pulse_modal(void)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_modal_box);
    lv_anim_set_values(&a, 0, 6);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 80);
    lv_anim_set_playback_time(&a, 80);
    lv_anim_set_repeat_count(&a, 2);
    lv_anim_start(&a);
}

void arex_screen_hide_modal(void)
{
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

/* =========================================================
 * Compass / Gas / Edit callbacks
 * ========================================================= */
void arex_screen_refresh_compass_target(void)
{
    arex_card_reg_t *c = arex_card_get_by_id(CARD_ID_COMPASS);
    if (c && c->update_cb) c->update_cb();
}

void arex_screen_refresh_gas_menu(void)
{
    arex_card_reg_t *c = arex_card_get_by_id(CARD_ID_GAS);
    if (c && c->update_cb) c->update_cb();
}

/* =========================================================
 * Inline value edit (MOD PO2 pattern)
 * ========================================================= */
static lv_timer_t *s_edit_flash_timer;
static lv_obj_t   *s_edit_flash_badge;
static lv_obj_t   *s_edit_flash_val_lbl;
static bool        s_edit_flash_on;

static void edit_flash_timer_cb(lv_timer_t *t)
{
    (void)t;
    s_edit_flash_on = !s_edit_flash_on;
    if (s_edit_flash_val_lbl) {
        /* 文字颜色在绿/暗绿之间闪烁，无背景色切换 */
        lv_color_t fg = s_edit_flash_on ? AREX_GREEN : AREX_DARK;
        lv_obj_set_style_text_color(s_edit_flash_val_lbl, fg, 0);
    }
}

static void edit_flash_stop(void)
{
    if (s_edit_flash_timer) {
        lv_timer_del(s_edit_flash_timer);
        s_edit_flash_timer = NULL;
    }
    s_edit_flash_badge   = NULL;
    s_edit_flash_val_lbl = NULL;
}

static void edit_flash_start(void)
{
    if (s_edit_flash_timer) {
        lv_timer_del(s_edit_flash_timer);
        s_edit_flash_timer = NULL;
    }
    s_edit_flash_on = true;
    s_edit_flash_timer = lv_timer_create(edit_flash_timer_cb, 350, NULL);
}

static void edit_value_cleanup(lv_obj_t *item);

void arex_screen_refresh_edit_value(void)
{
    if (!g_ui.edit_ctx.active || !s_edit_flash_val_lbl) return;
    static float last_drawn = -9999.f;
    float cur = g_ui.edit_ctx.value;
    if (cur == last_drawn) return;   /* dirty check：值未变则跳过，不触发重绘 */
    last_drawn = cur;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f ^v", cur);
    lv_label_set_text(s_edit_flash_val_lbl, buf);
}

void arex_screen_begin_edit_value(uint8_t item_idx, float value,
                                   float min, float max, float step)
{
    g_ui.edit_ctx.value      = value;
    g_ui.edit_ctx.original   = value;
    g_ui.edit_ctx.min        = min;
    g_ui.edit_ctx.max        = max;
    g_ui.edit_ctx.step       = step;
    g_ui.edit_ctx.item_index = item_idx;
    g_ui.edit_ctx.active     = true;
    g_ui.state = UI_EDIT_VALUE;

    lv_obj_t *item = lv_obj_get_child(s_submenu_list, item_idx);
    if (!item) return;

    /* 从选中态切换到编辑态：绿底→黑底绿框，title 文字恢复绿色 */
    lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(item, AREX_GREEN, 0);
    lv_obj_set_style_border_width(item, 2, 0);

    /* 复用 child 0 作为左侧标签，恢复绿色文字 */
    lv_obj_t *prefix_lbl = lv_obj_get_child(item, 0);
    if (prefix_lbl) {
        lv_label_set_text(prefix_lbl, "MOD PO2:");
        lv_obj_set_style_text_color(prefix_lbl, AREX_GREEN, 0);
        lv_obj_set_size(prefix_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(prefix_lbl, LV_ALIGN_LEFT_MID, 12, 0);
    }

    /* child 1 是 badge label（CONSERVATISM 等无 badge 时为 NULL），恢复颜色 */
    lv_obj_t *old_badge = lv_obj_get_child(item, 1);
    if (old_badge) lv_obj_set_style_text_color(old_badge, AREX_GREEN, 0);

    /* 创建右侧数值 + 箭头 label，透明背景，靠右居中 */
    lv_obj_t *val_lbl = lv_label_create(item);
    lv_obj_set_style_text_color(val_lbl, AREX_GREEN, 0);
    lv_obj_set_style_text_font(val_lbl, AREX_FONT_TITLE, 0);
    lv_obj_set_style_bg_opa(val_lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_size(val_lbl, 120, LV_SIZE_CONTENT);
    lv_obj_align(val_lbl, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_text_align(val_lbl, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(val_lbl, LV_LABEL_LONG_DOT);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f ^v", value);
    lv_label_set_text(val_lbl, buf);

    s_edit_flash_badge    = val_lbl;   /* 复用指针用于闪烁 */
    s_edit_flash_val_lbl  = val_lbl;

    edit_flash_start();
}

static void edit_value_cleanup(lv_obj_t *item)
{
    if (!item) return;
    edit_flash_stop();
    lv_obj_set_style_border_color(item, AREX_DARK, 0);
    uint32_t cnt = lv_obj_get_child_cnt(item);
    if (cnt > 2) lv_obj_del(lv_obj_get_child(item, 2));
    cnt = lv_obj_get_child_cnt(item);
    if (cnt > 1) lv_obj_del(lv_obj_get_child(item, 1));
    lv_obj_set_layout(item, 0);
    lv_obj_update_layout(item);
}

void arex_screen_commit_edit_value(void)
{
    lv_obj_t *item = lv_obj_get_child(s_submenu_list, g_ui.edit_ctx.item_index);
    if (!item) return;
    edit_value_cleanup(item);
    lv_obj_t *lbl = lv_obj_get_child(item, 0);
    if (lbl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "MOD PO2: %.1f", g_ui.edit_ctx.value);
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
    }
    arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
}

void arex_screen_cancel_edit_value(void)
{
    arex_screen_commit_edit_value();
}

/* =========================================================
 * Card title helper
 * ========================================================= */
lv_obj_t *arex_screen_make_card_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(lbl, AREX_FONT_TITLE, 0);
    lv_obj_set_pos(lbl, 16, 12);
    lv_obj_set_size(lbl, (s_cached_right_w > 0 ? s_cached_right_w : 420) - 32, 28);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl, text);

    lv_obj_t *line = lv_obj_create(parent);
    uint16_t right_w = s_cached_right_w > 0 ? s_cached_right_w : 420;
    lv_obj_set_size(line, right_w - 32, 2);
    lv_obj_set_pos(line, 16, 38);
    lv_obj_set_style_bg_color(line, AREX_DARK, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);

    return lbl;
}
