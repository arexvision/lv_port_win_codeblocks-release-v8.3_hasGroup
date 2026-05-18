#include "arex_screen.h"
#include "arex_ui_engine.h"
#include "arex_data.h"
#include "arex_ui_state.h"
#include "arex_card_registry.h"
#include "fonts/arex_fonts.h"
#include <stdio.h>
#include <string.h>

extern lv_obj_t *s_compass_tape_obj;
extern lv_obj_t *s_heading_val_lbl;
extern lv_obj_t *s_heading_hint_lbl;

/* =========================================================
 * 鍐呴儴鍙ユ焺
 * ========================================================= */
static lv_obj_t *s_scr;
static lv_obj_t *s_safe_zone;        /* 瀹夊叏鍖哄鍣?(缁濆鍧愭爣鍘熺偣) */
static lv_obj_t *s_left_anchor;      /* 宸︿晶閿氱偣 (10U 娌欑洅) */
static lv_obj_t *s_right_cont;       /* clip container */
static lv_obj_t *s_tileview;
static lv_obj_t *s_tile_objs[AREX_CARD_COUNT];

/* 鐏厜鎺у埗鐘舵€侊紙渚?LIGHT CONTROL 瀛愯彍鍗曞叏灞€鍏变韩锛?*/
/* 闂4淇锛氱伅鍏夌‖浠堕粯璁ゅ紑鍚紝UI 鍒濆鍊煎繀椤诲尮閰嶇‖浠剁姸鎬?*/
bool g_light_power_state = false;
static lv_obj_t *s_light_status_lbl = NULL;  /* LIGHT ON/OFF 鐘舵€佹爣绛?*/

/* Wall indicators */
static lv_obj_t *s_wall_top;
static lv_obj_t *s_wall_bottom;
static lv_obj_t *s_wall_text_top,    *s_wall_blocks_top;
static lv_obj_t *s_wall_text_bottom, *s_wall_blocks_bottom;

/* Scroll dots */
static lv_obj_t *s_dot_cont;  /* dots 瀹瑰櫒锛堢埗瀵硅薄涓?s_safe_zone锛屽彲瀹氫綅鍒伴棿闅欎腑闂达級 */
static lv_obj_t *s_scroll_dots[AREX_DASH_CARD_COUNT];

/* Modal overlay */
static lv_obj_t *s_modal;
static lv_obj_t *s_modal_box;
static lv_obj_t *s_brightness_overlay;
static bool s_software_brightness_enabled = true;

/* Sub-menu layer */
static lv_obj_t *s_submenu_layer;
static lv_obj_t *s_submenu_title;
static lv_obj_t *s_submenu_list;

/* INFO / SETUP list objects */
static lv_obj_t *s_info_list;
static lv_obj_t *s_setup_list;

/* Inline value edit (MOD PO2 pattern) */
static lv_timer_t *s_edit_flash_timer;
static lv_obj_t   *s_edit_flash_badge;
static lv_obj_t   *s_edit_flash_val_lbl;
static bool        s_edit_flash_on;

/* 鎺掔増缂撳瓨 (閬垮厤姣忔閲嶇畻) */
static uint16_t s_cached_right_w = 0;

/* Forward declarations for static functions */
static void wall_create(void);
static void modal_create(void);
static void submenu_layer_create(void);
static void reset_transient_ui_refs(void);
static void edit_flash_stop(void);
static void restore_brightness_overlay_state(void);

/* =========================================================
 * 鏍峰紡 (闈欐€佸垵濮嬪寲涓€娆?
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

static void reset_transient_ui_refs(void)
{
    s_wall_top = NULL;
    s_wall_bottom = NULL;
    s_wall_text_top = NULL;
    s_wall_blocks_top = NULL;
    s_wall_text_bottom = NULL;
    s_wall_blocks_bottom = NULL;
    s_modal = NULL;
    s_modal_box = NULL;
    s_submenu_layer = NULL;
    s_submenu_title = NULL;
    s_submenu_list = NULL;
    s_info_list = NULL;
    s_setup_list = NULL;
    s_light_status_lbl = NULL;
    s_edit_flash_badge = NULL;
    s_edit_flash_val_lbl = NULL;
    edit_flash_stop();

    g_ui.sub_history_depth = 0;
    g_ui.sub_item_count = 0;
    g_ui.sub_menu_idx = 0;
    g_ui.gas_modal_from_submenu = false;
    g_ui.edit_ctx.active = false;
}

static void restore_brightness_overlay_state(void)
{
    if (s_scr == NULL)
    {
        return;
    }

    /* 亮度遮罩挂在根屏 s_scr 上，布局翻转/重建右侧容器时不会被删除。
     * 这里必须保留对象引用并在重建后立即按当前档位重放一次，
     * 否则 APP 下发布局后会出现旧遮罩残留或前后亮度现象不一致。 */
    arex_apply_software_brightness(g_sys_config.brightness);
}

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
    lv_style_set_bg_opa(&s_style_anchor_bg, LV_OPA_COVER);
    lv_style_set_border_color(&s_style_anchor_bg, AREX_DARK);
    lv_style_set_border_width(&s_style_anchor_bg, AREX_DEBUG_BORDERS ? 1 : 0);
    lv_style_set_pad_all(&s_style_anchor_bg, 0);     /* 蹇呴』鏄惧紡娓呴浂锛屽惁鍒?LVGL 榛樿 padding 浼氬亸绉绘墍鏈夊瓙缁勪欢鍧愭爣 */
    lv_style_set_radius(&s_style_anchor_bg, 0);

    lv_style_init(&s_style_panel);
    lv_style_set_bg_color(&s_style_panel, AREX_BLACK);
    lv_style_set_bg_opa(&s_style_panel, LV_OPA_COVER);
    lv_style_set_pad_all(&s_style_panel, 0);
    lv_style_set_border_width(&s_style_panel, 0);

    lv_style_init(&s_style_label_huge);
    lv_style_set_text_color(&s_style_label_huge, AREX_GREEN);
    lv_style_set_text_font(&s_style_label_huge, arex_get_font(AREX_FONT_ID_HUGE));

    lv_style_init(&s_style_label_med);
    lv_style_set_text_color(&s_style_label_med, AREX_GREEN);
    lv_style_set_text_font(&s_style_label_med, arex_get_font(AREX_FONT_ID_MEDIUM));

    lv_style_init(&s_style_label_small);
    lv_style_set_text_color(&s_style_label_small, AREX_LIGHT);
    lv_style_set_text_font(&s_style_label_small, arex_get_font(AREX_FONT_ID_SMALL));

    lv_style_init(&s_style_title_zone);
    lv_style_set_text_color(&s_style_title_zone, AREX_LIGHT);
    lv_style_set_text_font(&s_style_title_zone, arex_get_font(AREX_FONT_ID_SMALL));
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
    lv_style_set_border_width(&s_style_menu_item, AREX_INNER_BORDER_W);
    lv_style_set_pad_all(&s_style_menu_item, 12);

    lv_style_init(&s_style_menu_item_active);
    lv_style_set_bg_color(&s_style_menu_item_active, AREX_GREEN);
    lv_style_set_bg_opa(&s_style_menu_item_active, LV_OPA_COVER);
    lv_style_set_text_color(&s_style_menu_item_active, AREX_BLACK);
    lv_style_set_border_color(&s_style_menu_item_active, AREX_GREEN);
}

/* =========================================================
 * 杈呭姪鍑芥暟
 * ========================================================= */

/* 娓呯┖ ascent/NDL widget 鍙ユ焺鏁扮粍锛堝湪浠讳綍缃戞牸娓叉煋涔嬪墠璋冪敤锛?
 * 鍦?arex_screen_rebuild_layout() 鍜?left_anchor_create() 鍏ュ彛鍚勮皟鐢ㄤ竴娆★紝
 * 纭繚鏁扮粍浠庡共鍑€鐘舵€佸紑濮嬶紝涓や晶缃戞牸娓叉煋鍑芥暟鍧囦互杩藉姞妯″紡杩愯銆?*/
/* =========================================================
 * 缁熶竴閲嶇疆 UI 娓叉煋鐘舵€侊紙闃叉鎮┖鎸囬拡璁块棶锛?
 * 璋冪敤閾撅細arex_screen_rebuild_layout 鈫?clear_widget_arrays
 * ========================================================= */
static void clear_widget_arrays(void)
{
    /* 閲嶇疆閫熺巼鍥炬爣闃靛垪 */
    memset(s_img_ascent_rate, 0, sizeof(s_img_ascent_rate));
    s_ascent_icon_count = 0;

    /* 閲嶇疆 NDL 鐘舵€佹満 */
    memset(s_ndl_handles, 0, sizeof(s_ndl_handles));
    s_ndl_handle_count = 0;

    /* 閲嶇疆娓叉煋璁℃暟鍣ㄥ拰 SystemData 闈欐€佸彞鏌?*/
    arex_reset_widget_render_state();
}

/* =========================================================
 * 宸︿晶閿氱偣锛氱粷瀵瑰潗鏍囬噸寤?
 *
 * 鏍稿績閾佸緥锛氭墍鏈夊瓙缁勪欢浠?s_left_anchor 宸︿笂瑙掍负鍘熺偣 (0,0)锛?
 * 涓嶄娇鐢ㄤ换浣?LV_FLEX / LV_GRID锛屽畬鍏ㄩ€氳繃鏁板绱姞 current_y銆?
 *
 * Tech 妯″紡涓嬶細
 *   - DEPTH: (0, cur_y), 瀹?160px, 楂?h_depth*10
 *   - NDL/TTS 鍙屾嫾琛? cur_y += h_depth*10 + gap
 *     NDL: (0, cur_y), 瀹?80px, 楂?h_ndl*10
 *     TTS: (80, cur_y), 瀹?80px, 楂?h_ndl*10
 *   - 浠ユ绫绘帹...
 *
 * Classic 妯″紡涓嬶細浣跨敤鍚屾牱閫昏緫锛屽搴︽墿灞曚负 safe_zone_w
 * ========================================================= */
/* =========================================================
 * 宸︿晶閿氱偣閲嶅缓 (閰嶇疆鍙樻洿鏃惰皟鐢?
 *
 * 2x7 缁濆缃戞牸鐗堟湰锛氱洿鎺ヨ皟鐢ㄧ綉鏍兼覆鏌撳紩鎿庨噸寤恒€?
 * ========================================================= */
static void left_anchor_rebuild(uint8_t comp_count)
{
    (void)comp_count;
    if (!s_left_anchor || !s_safe_zone) return;

    /* 閲嶅缓閿氱偣瀹瑰櫒灏哄锛堝竷灞€椤哄簭鍙兘鍙樺寲锛?*/
    uint16_t anchor_w = AREX_LEFT_ANCHOR_W;
    uint16_t anchor_h = g_sys_config.safe_zone_h;
    lv_obj_set_size(s_left_anchor, anchor_w, anchor_h);

    /* 娓呴櫎鎵€鏈夊瓙瀵硅薄 */
    lv_obj_clean(s_left_anchor);

    /* 閲嶆柊璋冪敤 2x7 缁濆缃戞牸娓叉煋寮曟搸 */
    arex_render_left_anchor_grid(s_left_anchor);
}

/* =========================================================
 * 宸︿晶閿氱偣鍒涘缓 (棣栨鍒濆鍖?
 * ========================================================= */
/* =========================================================
 * 宸︿晶閿氱偣鍒涘缓 (2x7 缁濆缃戞牸鐗堟湰)
 *
 * 搴熷純 current_y 绱姞鎺掔増锛屾敼涓?2鍒?80px) x 7琛?60px) 缁濆缃戞牸鐭╅樀銆?
 * 鎵€鏈夌粍浠堕€氳繃 arex_render_left_anchor_grid() 浣跨敤 render_widget_by_id 宸ュ巶娓叉煋锛?
 * 骞舵敞鍏?arex_widget_id_t 鐑欏嵃渚?arex_widget_set_value() 瀹氫綅鏇存柊銆?
 * SystemData 宸蹭綔涓?g_sys_config.left_widgets[6] 鍙備笌缃戞牸鎺掔増锛屼笉鍐嶇嫭绔嬫覆鏌撱€?
 * ========================================================= */
static void left_anchor_create(void)
{
    /* 1. 鍒涘缓閿氱偣瀹瑰櫒 */
    s_left_anchor = lv_obj_create(s_safe_zone);
    lv_obj_set_size(s_left_anchor, AREX_LEFT_ANCHOR_W, g_sys_config.safe_zone_h);
    if (g_sys_config.layout_order == AREX_ORDER_NORMAL)
    {
        lv_obj_set_pos(s_left_anchor, 0, 0);
    }
    else
    {
        uint16_t panel_gap = g_sys_config.panel_gap_u * AREX_BASE_U;
        uint16_t right_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - panel_gap;
        lv_obj_set_pos(s_left_anchor, (lv_coord_t)(right_w + panel_gap), 0);
    }
    lv_obj_add_style(s_left_anchor, &s_style_anchor_bg, 0);
    lv_obj_set_style_pad_all(s_left_anchor, 0, 0);
    lv_obj_set_scrollbar_mode(s_left_anchor, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_left_anchor, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_clean(s_left_anchor);

    /* 娓呯┖ widget 鍙ユ焺鏁扮粍锛堥娆″垱寤烘椂閲嶇疆锛?*/
    clear_widget_arrays();

    /* 2. 璋冪敤 2x7 缁濆缃戞牸娓叉煋寮曟搸锛堝甫 sudu 閫熺巼鍥炬爣锛?*/
    arex_render_left_anchor_grid(s_left_anchor);
}

/* =========================================================
 * 鍙充晶鍖哄煙: Safe Zone 鍔ㄦ€佹帓鐗?
 *
 * Safe Zone 鍐呴儴甯冨眬 (缁濆鍧愭爣):
 *
 *  [s_left_anchor] | [s_right_cont]
 *  (Tech 妯″紡)
 *
 * Safe Zone 鍧愭爣鍘熺偣涓?(0,0) 鍦?s_safe_zone 宸︿笂瑙掋€?
 * 鎵€鏈夊瓙缁勪欢鐨勫潗鏍囦互姝や负鍩哄噯璁＄畻銆?
 * ========================================================= */
static void safe_zone_reposition(void)
{
    if (!s_safe_zone) return;

    /* 1. 瀹夊叏鍖哄鍣ㄥ畾浣?*/
    lv_obj_set_size(s_safe_zone, g_sys_config.safe_zone_w, g_sys_config.safe_zone_h);
    lv_obj_align(s_safe_zone, LV_ALIGN_CENTER,
                 g_sys_config.offset_x, g_sys_config.offset_y);

    /* 2. 璁＄畻宸﹀彸鍒嗙晫绾?*/
    uint16_t panel_gap = g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t right_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - panel_gap;

    /* 3. 瀹氫綅宸︿晶閿氱偣 */
    if (g_sys_config.layout_order == AREX_ORDER_NORMAL)
    {
        lv_obj_set_pos(s_left_anchor, 0, 0);
        lv_obj_set_pos(s_right_cont, (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap), 0);
    }
    else
    {
        /* 缈昏浆: 鍙充晶瀹瑰櫒鏀惧乏杈? 宸︿晶閿氱偣鏀惧彸杈?*/
        lv_obj_set_pos(s_right_cont, 0, 0);
        lv_obj_set_pos(s_left_anchor, (lv_coord_t)right_w + panel_gap, 0);
    }

    /* 鍥哄畾鍖哄湪宸﹀彸浜掓崲鍚庝粛瑕佷繚鎸佽嚜宸辩殑灞傜骇鍜岄噸缁樹紭鍏堢骇锛岄槻姝㈠垎闅旂嚎琚悗缁鍣ㄨ鐩?*/
    if (s_left_anchor)
    {
        lv_obj_move_foreground(s_left_anchor);
        lv_obj_invalidate(s_left_anchor);
    }

    /* 4. 璁剧疆鍙充晶瀹瑰櫒灏哄 */
    lv_obj_set_size(s_right_cont, right_w, g_sys_config.safe_zone_h);

    s_cached_right_w = right_w;

    /* 5. 閲嶆柊瀹氫綅 scroll dots锛堝鐞嗘墍鏈変綅缃ā寮忥級 */
    if (s_dot_cont)
    {
        lv_coord_t dot_x, dot_y;
        uint16_t dot_cont_h = arex_visible_dash_count() * 14;

        printf("[SAFE_ZONE] reposition dots: position=%d, visible_dash=%u, dot_cont_h=%u, safe_zone=%ux%u\r\n",
               g_sys_config.dots_position, arex_visible_dash_count(), dot_cont_h,
               g_sys_config.safe_zone_w, g_sys_config.safe_zone_h);

        /* 鏇存柊瀹瑰櫒澶у皬浠ュ尮閰嶅疄闄呮樉绀烘暟閲?*/
        lv_obj_set_size(s_dot_cont, 10, (lv_coord_t)dot_cont_h);

        if (g_sys_config.dots_position == AREX_DOTS_LEFT)
        {
            /* 宸︿晶锛氶棿闅欎腑闂?*/
            lv_coord_t gap_center_x;
            lv_coord_t gap_center_y = (lv_coord_t)(g_sys_config.safe_zone_h / 2);
            if (g_sys_config.layout_order == AREX_ORDER_NORMAL)
            {
                gap_center_x = (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap / 2);
            }
            else
            {
                gap_center_x = (lv_coord_t)(right_w + panel_gap / 2);
            }
            dot_x = gap_center_x - 5;
            dot_y = gap_center_y - (lv_coord_t)(dot_cont_h / 2);
        }
        else if (g_sys_config.dots_position == AREX_DOTS_RIGHT)
        {
            /* 鍙充晶锛氱浉瀵逛簬鍙充晶瀹瑰櫒鐨勫彸杈圭紭 */
            lv_coord_t right_cont_x = (g_sys_config.layout_order == AREX_ORDER_NORMAL)
                                      ? (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap)
                                      : 0;
            dot_x = right_cont_x + (lv_coord_t)right_w - 18;
            dot_y = (lv_coord_t)(g_sys_config.safe_zone_h / 2 - (int)dot_cont_h / 2);
        }
        else if (g_sys_config.dots_position == AREX_DOTS_BOTTOM)
        {
            /* 搴曢儴锛氭按骞冲眳涓?*/
            dot_x = (lv_coord_t)(g_sys_config.safe_zone_w / 2 - 5);
            dot_y = (lv_coord_t)(g_sys_config.safe_zone_h - 18);
        }
        else
        {
            /* AREX_DOTS_NONE: 闅愯棌 dots */
            dot_x = -1000;  /* 绉诲嚭鍙鍖哄煙 */
            dot_y = -1000;
        }
        lv_obj_set_pos(s_dot_cont, dot_x, dot_y);

        /* 鏇存柊姣忎釜 dot 鐨勭粷瀵逛綅缃紙浣跨敤缁濆瀹氫綅锛?*/
        for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++)
        {
            if (s_scroll_dots[i])
            {
                lv_obj_set_pos(s_scroll_dots[i], 2, (lv_coord_t)(i * 14));
                /* 瓒呭嚭 visible_dash 鐨?dot 闅愯棌 */
                if (i >= arex_visible_dash_count())
                {
                    lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
                }
                else
                {
                    lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
        }

        /* 璋冪敤 arex_screen_update_scroll_dots() 鏉ユ牴鎹?UI 鐘舵€佹洿鏂板彲瑙佹€?*/
        /* 璁＄畻閫昏緫绱㈠紩 */
        uint8_t active_idx = 0;
        if (g_ui.dash_card >= CARD_POS_DYNAMIC_FIRST && g_ui.dash_card < arex_setup_display_pos())
        {
            for (uint8_t pos = CARD_POS_DYNAMIC_FIRST; pos < g_ui.dash_card; pos++)
            {
                uint8_t card_id = g_sys_card_order(pos);
                if (card_id != CARD_ID_UNUSED && card_id != CARD_ID_BLANK)
                {
                    active_idx++;
                }
            }
        }
        bool show_dots = (g_ui.state == UI_DASH || g_ui.state == UI_EDIT_GAS);
        arex_screen_update_scroll_dots(active_idx, show_dots);
    }
}

/* =========================================================
 * 鍙充晶鍖哄煙: clip container + tileview
 * ========================================================= */
static void right_panel_create(void)
{
    /* 璁＄畻鍙充晶瀹瑰櫒瀹藉害 */
    uint16_t panel_gap = g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t right_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - panel_gap;
    s_cached_right_w = right_w;

    /* 鍒涘缓鍙充晶瀹瑰櫒 鈥?鏍规嵁 layout_order 鍐冲畾宸﹀彸浣嶇疆 */
    s_right_cont = lv_obj_create(s_safe_zone);
    lv_obj_set_size(s_right_cont, right_w, g_sys_config.safe_zone_h);
    if (g_sys_config.layout_order == AREX_ORDER_NORMAL)
    {
        lv_obj_set_pos(s_right_cont, (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap), 0);
    }
    else
    {
        lv_obj_set_pos(s_right_cont, 0, 0);
    }
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

    /* 鍒涘缓 tiles */
    memset(s_tile_objs, 0, sizeof(s_tile_objs));
    uint8_t count = arex_card_count();
    for (uint8_t i = 0; i < count; i++)
    {
        arex_card_t *card = arex_card_get(i);
        if (!card) continue;

        lv_obj_t *tile = lv_tileview_add_tile(s_tileview, 0, i,
                                              LV_DIR_TOP | LV_DIR_BOTTOM);
        lv_obj_set_style_bg_color(tile, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(tile, 0, 0);
        lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        s_tile_objs[i] = tile;
        card->tile_obj = tile;

        switch (card->engine_type)
        {
        case CARD_ENGINE_GRID:
        {
            /* 鑾峰彇姝?tile 瀵瑰簲鐨?custom_card_slot 绱㈠紩 */
            uint8_t storage_pos = arex_card_storage_pos(i);
            uint8_t custom_card_idx = (storage_pos < AREX_CARD_COUNT)
                                      ? g_sys_config.custom_card_slot[storage_pos]
                                      : 0xFF;
            if (custom_card_idx < AREX_MAX_CUSTOM_CARDS)
            {
                arex_render_5f_custom_grid(tile, g_left_anchor_obj, custom_card_idx);
            }
            break;
        }
        case CARD_ENGINE_MENU:
        case CARD_ENGINE_CUSTOM:
        default:
            if (card->create_cb) card->create_cb(tile);
            break;
        }
    }

    /* Scroll dots - 鐖跺璞′负 s_safe_zone锛屽彲瀹氫綅鍒伴棿闅欎腑闂?*/
    s_dot_cont = lv_obj_create(s_safe_zone);
    /* 瀹瑰櫒楂樺害鏍规嵁瀹為檯鏄剧ず鏁伴噺璁＄畻 */
    uint16_t dot_cont_h = arex_visible_dash_count() * 14;
    lv_obj_set_size(s_dot_cont, 10, dot_cont_h);
    lv_obj_set_style_bg_opa(s_dot_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_dot_cont, 0, 0);
    lv_obj_set_style_pad_all(s_dot_cont, 0, 0);
    /* 涓嶄娇鐢?flex 甯冨眬锛屾敼鐢ㄧ粷瀵瑰畾浣嶏紝閬垮厤闅愯棌鍏冪礌鍗犵敤绌洪棿 */
    lv_obj_set_scrollbar_mode(s_dot_cont, LV_SCROLLBAR_MODE_OFF);

    /* Dots 浣嶇疆璺熼殢閰嶇疆 - 浣跨敤缁濆鍧愭爣锛堢浉瀵逛簬 s_safe_zone锛?*/
    if (g_sys_config.dots_position == AREX_DOTS_LEFT)
    {
        /* 鏀惧湪宸︿晶鍥哄畾鍖哄拰鍙充晶鍗＄墖鍖虹殑闂撮殭涓棿 */
        lv_coord_t gap_center_x;
        lv_coord_t gap_center_y = (lv_coord_t)(g_sys_config.safe_zone_h / 2);

        if (g_sys_config.layout_order == AREX_ORDER_NORMAL)
        {
            /* NORMAL 甯冨眬锛氶棿闅欎腑闂?X = AREX_LEFT_ANCHOR_W + panel_gap / 2 */
            gap_center_x = (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap / 2);
        }
        else
        {
            /* FLIPPED 甯冨眬锛氶棿闅欎腑闂?X = right_w + panel_gap / 2 */
            gap_center_x = (lv_coord_t)(right_w + panel_gap / 2);
        }
        lv_obj_set_pos(s_dot_cont, gap_center_x - 5, gap_center_y - dot_cont_h / 2);
    }
    else if (g_sys_config.dots_position == AREX_DOTS_RIGHT)
    {
        /* 鍙充晶锛氱浉瀵逛簬鍙充晶瀹瑰櫒鐨勫彸杈圭紭 */
        lv_coord_t right_cont_x = (g_sys_config.layout_order == AREX_ORDER_NORMAL)
                                  ? (lv_coord_t)(AREX_LEFT_ANCHOR_W + panel_gap)
                                  : 0;
        lv_coord_t dots_x = right_cont_x + right_w - 18;
        lv_coord_t dots_y = (lv_coord_t)(g_sys_config.safe_zone_h / 2 - dot_cont_h / 2);
        lv_obj_set_pos(s_dot_cont, dots_x, dots_y);
    }
    else if (g_sys_config.dots_position == AREX_DOTS_BOTTOM)
    {
        /* 搴曢儴锛氭按骞冲眳涓?*/
        lv_coord_t dots_x = (lv_coord_t)(g_sys_config.safe_zone_w / 2 - 5);
        lv_coord_t dots_y = (lv_coord_t)(g_sys_config.safe_zone_h - 18);
        lv_obj_set_pos(s_dot_cont, dots_x, dots_y);
    }
    /* AREX_DOTS_NONE 鏃朵笉鏄剧ず dot_cont */

    /* 浣跨敤缁濆瀹氫綅鍒涘缓 dots锛岄伩鍏嶉殣钘忔椂浠嶅崰鐢ㄧ┖闂寸殑闂 */
    for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++)
    {
        s_scroll_dots[i] = lv_obj_create(s_dot_cont);
        lv_obj_set_size(s_scroll_dots[i], 6, 6);
        lv_obj_set_pos(s_scroll_dots[i], 2, (lv_coord_t)(i * 14));  /* 缁濆瀹氫綅锛屽瀭鐩村潎鍖€鍒嗗竷 */
        lv_obj_set_style_radius(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_bg_color(s_scroll_dots[i], AREX_DARK, 0);
        lv_obj_set_style_bg_opa(s_scroll_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_pad_all(s_scroll_dots[i], 0, 0);
        lv_obj_set_style_shadow_width(s_scroll_dots[i], 0, 0);
        lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_SCROLLABLE);
        if (g_sys_config.dots_position == AREX_DOTS_NONE)
            lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
        /* 瓒呭嚭 visible_dash 鐨?dot 涔熼殣钘?*/
        if (i >= arex_visible_dash_count())
            lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* 鍙湪 DASH/EDIT 鐘舵€佹墠鏄剧ず dots锛孖NFO/SETUP 鑿滃崟涓嶆樉绀?*/
    bool show_dots = (g_ui.state == UI_DASH || g_ui.state == UI_EDIT_GAS);
    arex_screen_update_scroll_dots(0, show_dots);
}

/* =========================================================
 * Safe Zone 瀹瑰櫒閲嶅缓 (閰嶇疆鍙樻洿鍚庤皟鐢?
 * 涓嶉噸寤?cards锛屽彧閲嶅缓甯冨眬妗嗘灦
 * ========================================================= */
void arex_screen_rebuild_layout(void)
{
    printf("[REBUILD_LAYOUT] Enter: visible_dash=%u, dots_pos=%d, layout_order=%d\r\n",
           arex_visible_dash_count(), g_sys_config.dots_position, g_sys_config.layout_order);

    /* 銆愰棶棰樺洓淇銆戝繀椤诲湪娓呯┖瀵硅薄鍓嶉噸鏂板惎鐢?invalidation
     * 鍥犱负 arex_ui_timer_cb() 涓鐢ㄤ簡 invalidation 浠ヤ紭鍖栧埛灞忔€ц兘锛?
     * 浠讳綍娑夊強鍒犻櫎 LVGL 瀵硅薄鐨勪唬鐮侀兘搴旇鍋囪 invalidation 鍙兘琚鐢ㄣ€?*/
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) lv_disp_enable_invalidation(disp, true);

    /* 1. 蹇呴』鍦ㄦ竻绌哄璞″墠锛屾妸鎸囬拡鍏ㄩ儴娲楃櫧锛佹柇缁濇偓绌烘寚閽堬紒 */
    clear_widget_arrays();

    /* 2. 娓呯┖宸︿晶閿氱偣锛堟媶鎴垮瓙锛?*/
    if (s_left_anchor)
    {
        lv_obj_clean(s_left_anchor);
    }

    /* 3. 閲嶅缓宸︿晶閿氱偣鎺掔増锛?x7 缁濆缃戞牸鐗堟湰锛屽缓鎴垮瓙锛?*/
    if (s_left_anchor)
    {
        left_anchor_rebuild(0);
    }

    /* 4. 閲嶅缓鎵€鏈夎嚜瀹氫箟缃戞牸鍗＄墖 */
    arex_5f_grid_rebuild_all();

    /* 5. 閲嶅缓 Safe Zone 鍐呴儴瀹氫綅锛堝寘鎷?dots 浣嶇疆鍜屽彲瑙佹€э級 */
    safe_zone_reposition();

    /* 5.1 重建后立即把当前数据灌入全部 widget 实例，避免多实例场景下部分组件长时间停留在 "--" */
    arex_screen_refresh_all_widgets();

    /* 5.2 APP 下发布局/翻转后，亮度现象必须与默认布局保持一致。 */
    restore_brightness_overlay_state();

    /* 6. 寮哄埗鎶婃墍鏈夊父瑙勬暟鎹殑鑴忔爣璁扮疆 1锛?
     * 鍥犱负鏂板缓鐨?Label 閲岄潰鍏ㄦ槸 "--"锛屽繀椤昏瀹氭椂鍣ㄥ湪涓嬩竴甯ф妸鐪熷疄鏁版嵁鍒疯繘鍘伙紒 */
    g_sensor_data.dirty_mask |= (DIRTY_DEPTH | DIRTY_BATT | DIRTY_TEMP | DIRTY_POD);
}


/* =========================================================
 * Tileview 閲嶅缓 (鍗＄墖椤哄簭鍙樻洿鏃惰皟鐢?
 * ========================================================= */
void arex_screen_rebuild_tileview(void)
{
    /* 銆愰棶棰樺洓淇銆戝繀椤诲湪鍒犻櫎瀵硅薄鍓嶉噸鏂板惎鐢?invalidation
     * 鍥犱负 arex_ui_timer_cb() 涓鐢ㄤ簡 invalidation 浠ヤ紭鍖栧埛灞忔€ц兘锛?
     * 浠讳綍娑夊強鍒犻櫎 LVGL 瀵硅薄鐨勪唬鐮侀兘搴旇鍋囪 invalidation 鍙兘琚鐢ㄣ€?*/
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) lv_disp_enable_invalidation(disp, true);

    uint8_t count = arex_card_count();

    /* 銆愰棶棰樹簩淇銆戜繚瀛樺綋鍓嶇劍鐐逛綅缃拰鐘舵€佹満涓婁笅鏂?*/
    uint8_t saved_dash_card = g_ui.dash_card;
    arex_ui_state_t saved_state = g_ui.state;
    uint8_t saved_menu_idx = (saved_state == UI_INFO) ? g_ui.menu_info_idx
                             : (saved_state == UI_SETUP) ? g_ui.menu_setup_idx
                             : 0;

    memset(s_tile_objs, 0, sizeof(s_tile_objs));
    memset(g_card_custom_objs, 0, sizeof(g_card_custom_objs));
    g_card_custom_obj_count = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        arex_card_t *card = arex_card_get_by_id(i);
        if (card) card->tile_obj = NULL;
    }

    if (s_right_cont)
    {
        lv_obj_del(s_right_cont);
        s_right_cont = NULL;
        s_tileview   = NULL;
        reset_transient_ui_refs();
    }

    /* 鍗曠嫭鍒犻櫎 s_dot_cont锛堢埗瀵硅薄涓?s_safe_zone锛屼笉浼氶殢 s_right_cont 鍒犻櫎锛?*/
    if (s_dot_cont)
    {
        lv_obj_del(s_dot_cont);
        s_dot_cont = NULL;
        for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++)
            s_scroll_dots[i] = NULL;
    }

    right_panel_create();
    /* right_panel_create() 鍙噸寤哄彸渚?tileview / dots銆?
     * tileview 鍒犻櫎鍚庯紝鎸傚湪 s_right_cont 涓婄殑 wall / submenu / modal 鍙ユ焺涔熷凡澶辨晥锛?
     * 蹇呴』鍚屾閲嶅缓锛屽惁鍒欏悗缁姸鎬佹満璺緞浼氭妸 NULL 浼犺繘 lv_obj_add_flag/clear_flag銆?*/
    wall_create();
    submenu_layer_create();
    modal_create();
    restore_brightness_overlay_state();

    /* 銆愰棶棰樹簩淇銆戞仮澶?tile 鐒︾偣鍒颁繚瀛樼殑浣嶇疆
     * 娉ㄦ剰锛歡_ui.dash_card 宸茬粡鍦ㄥ閮ㄤ繚瀛樹簡锛岃繖閲屼娇鐢?saved_dash_card */
    if (s_tileview && saved_dash_card < AREX_CARD_COUNT && s_tile_objs[saved_dash_card])
    {
        lv_obj_set_tile(s_tileview, s_tile_objs[saved_dash_card], LV_ANIM_OFF);
    }

    /* 銆愰棶棰榅淇銆戞仮澶?UI 鐘舵€佹満
     * 甯冨眬閲嶅缓鍚庡彧鎭㈠浜?tile 浣嶇疆锛屼絾娌℃湁鎭㈠鐘舵€佹満锛?
     * 瀵艰嚧 g_ui.state 涓嶅悓姝ワ紝婊戝姩琛屼负寮傚父锛屽乏渚ф寚绀哄櫒鏄剧ず閿欒 */
    if (AREX_ENABLE_INFO_MENU && saved_dash_card == CARD_POS_INFO)
    {
        g_ui.state = UI_INFO;
        g_ui.menu_info_idx = saved_menu_idx;
    }
    else if (saved_dash_card == arex_setup_display_pos())
    {
        g_ui.state = UI_SETUP;
        g_ui.menu_setup_idx = saved_menu_idx;
    }
    else
    {
        g_ui.state = UI_DASH;
        if (saved_dash_card < CARD_POS_DYNAMIC_FIRST || saved_dash_card >= arex_setup_display_pos())
        {
            g_ui.dash_card = CARD_POS_DYNAMIC_FIRST;
            if (s_tileview && s_tile_objs[CARD_POS_DYNAMIC_FIRST])
            {
                lv_obj_set_tile(s_tileview, s_tile_objs[CARD_POS_DYNAMIC_FIRST], LV_ANIM_OFF);
            }
        }
    }

    {
        /* 璁＄畻閫昏緫绱㈠紩锛氫粠 DYNAMIC_FIRST 鍒板綋鍓嶅崱鐗囦箣闂存湁澶氬皯涓湁鏁堝崱鐗?*/
        uint8_t active_idx = 0;
        if (g_ui.dash_card >= CARD_POS_DYNAMIC_FIRST && g_ui.dash_card < arex_setup_display_pos())
        {
            for (uint8_t pos = CARD_POS_DYNAMIC_FIRST; pos < g_ui.dash_card; pos++)
            {
                uint8_t card_id = g_sys_card_order(pos);
                if (card_id != CARD_ID_UNUSED && card_id != CARD_ID_BLANK)
                {
                    active_idx++;
                }
            }
        }
        /* INFO/SETUP 鑿滃崟涓嶆樉绀?dots锛屽彧鏇存柊娲昏穬绱㈠紩 */
        bool show_dots = (g_ui.state == UI_DASH || g_ui.state == UI_EDIT_GAS);
        arex_screen_update_scroll_dots(active_idx, show_dots);
    }
}

void arex_screen_rebuild_full(void)
{
    /* 瀹屾暣閲嶅缓鍏ュ彛锛?
     * 1. card_order/custom_card_slot/custom_cards 鍙樺寲鏃跺繀椤婚噸寤?tileview
     * 2. tileview 閲嶅缓鍚庯紝safe zone / left anchor / dots 浼氶殢涔嬫寜褰撳墠 g_sys_config 閲嶅缓
     * 杩欐牱鍙互淇濊瘉鎭㈠榛樿甯冨眬鍚庯紝Dive Menu 瀵瑰簲椤甸潰缁撴瀯鍜岃繍琛屾椂閰嶇疆瀹屽叏涓€鑷淬€?*/
    arex_screen_rebuild_tileview();
    arex_screen_rebuild_layout();
}

/* =========================================================
 * Wall indicator
 * ========================================================= */
static lv_obj_t *make_wall(lv_obj_t *parent, lv_coord_t y)
{
    lv_obj_t *w = lv_obj_create(parent);
    lv_coord_t wall_h = g_sys_config.h_tissues_chart * AREX_BASE_U;  /* 90px = 9U */
    lv_coord_t wall_w = (s_cached_right_w > 0) ? s_cached_right_w
                        : (lv_coord_t)(g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W);
    lv_obj_set_size(w, wall_w, wall_h);
    lv_obj_set_pos(w, 0, y);
    lv_obj_set_style_bg_color(w, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(w, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(w, AREX_DARK, 0);
    lv_obj_set_style_border_width(w, AREX_INNER_BORDER_W, 0);
    lv_obj_set_style_border_side(w, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(w, 0, 0);
    lv_obj_add_flag(w, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(w, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *txt = lv_label_create(w);
    lv_obj_set_style_text_color(txt, AREX_GREEN, 0);
    lv_obj_set_style_text_font(txt, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_width(txt, wall_w);
    lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(txt, 0, 12);
    lv_label_set_text(txt, "");

    lv_obj_t *blk = lv_label_create(w);
    lv_obj_set_style_text_color(blk, AREX_GREEN, 0);
    /* Wall blocks 蹇呴』浣跨敤 Courier 鍐呯疆瀛椾綋浠ユ敮鎸?鈻?(U+25A0) 鏂瑰潡瀛楃 */
    lv_obj_set_style_text_font(blk, &lv_font_courier_28, 0);
    lv_obj_set_width(blk, wall_w);
    lv_obj_set_style_text_align(blk, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(blk, 0, 50);
    lv_label_set_text(blk, "");

    return w;
}

static void wall_create(void)
{
    lv_coord_t wall_h = g_sys_config.h_tissues_chart * AREX_BASE_U;  /* 90px = 9U */
    lv_coord_t wall_y_bottom = (g_sys_config.safe_zone_h > wall_h)
                               ? (lv_coord_t)(g_sys_config.safe_zone_h - wall_h)
                               : (lv_coord_t)(g_sys_config.safe_zone_h);

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
    uint16_t right_w_fallback = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                                - g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t sub_w = s_cached_right_w > 0 ? s_cached_right_w : right_w_fallback;
    lv_obj_set_size(s_modal, sub_w,
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
    uint16_t right_w_fallback = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                                - g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t sub_w = s_cached_right_w > 0 ? s_cached_right_w : right_w_fallback;

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
    lv_obj_set_style_text_font(s_submenu_title, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_pos(s_submenu_title, 16, 8);
    lv_label_set_text(s_submenu_title, "> SUB MENU");

    lv_obj_t *title_line = lv_obj_create(s_submenu_layer);
    lv_obj_set_size(title_line, sub_w - 32, 2);
    lv_obj_set_pos(title_line, 16, AREX_CARD_TITLE_H - 2);
    lv_obj_set_style_bg_color(title_line, AREX_DARK, 0);
    lv_obj_set_style_bg_opa(title_line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(title_line, 0, 0);
    lv_obj_set_style_pad_all(title_line, 0, 0);

    s_submenu_list = lv_obj_create(s_submenu_layer);
    lv_obj_set_size(s_submenu_list, sub_w - 15, g_sys_config.safe_zone_h - AREX_CARD_TITLE_H - 10);
    lv_obj_set_pos(s_submenu_list, 0, AREX_CARD_TITLE_H);
    lv_obj_set_style_bg_opa(s_submenu_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_submenu_list, 0, 0);
    lv_obj_set_style_pad_all(s_submenu_list, 0, 0);
    lv_obj_clear_flag(s_submenu_list, LV_OBJ_FLAG_SCROLLABLE);
}

/* =========================================================
 * arex_screen_create 鈥?鍏紑鍏ュ彛
 * ========================================================= */
void arex_screen_create(void)
{
    styles_init();

    s_scr = lv_obj_create(NULL);
    lv_obj_add_style(s_scr, &s_style_screen, 0);

    /* 瀹夊叏鍖哄鍣?(鐩稿浜?s_scr 灞呬腑瀹氫綅) */
    s_safe_zone = lv_obj_create(s_scr);
    lv_obj_set_size(s_safe_zone, g_sys_config.safe_zone_w, g_sys_config.safe_zone_h);
    lv_obj_align(s_safe_zone, LV_ALIGN_CENTER,
                 g_sys_config.offset_x, g_sys_config.offset_y);
    lv_obj_set_style_bg_opa(s_safe_zone, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_safe_zone, 0, 0);
    lv_obj_set_style_pad_all(s_safe_zone, 0, 0);
    lv_obj_clear_flag(s_safe_zone, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_border_color(s_safe_zone, AREX_DARK, 0);
    lv_obj_set_style_border_width(s_safe_zone, AREX_DEBUG_BORDERS ? 1 : 0, 0);

    left_anchor_create();
    right_panel_create();
    wall_create();
    submenu_layer_create();
    modal_create();
    restore_brightness_overlay_state();

    lv_scr_load(s_scr);
}

/* =========================================================
 * Tileview 瀵艰埅
 * ========================================================= */
void arex_screen_scroll_to_card(uint8_t tile_pos)
{
    /* 銆愰棶棰樹笁淇銆憇_tileview 鍙兘涓?NULL锛堝竷灞€閲嶅缓鏈熼棿锛?*/
    if (!s_tileview) return;

    if (tile_pos >= arex_card_count())
    {
        return;
    }
    lv_obj_t *tile = s_tile_objs[tile_pos];
    if (!tile)
    {
        return;
    }

    if (tile_pos == 0)
    {
        lv_anim_del(s_tileview, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_obj_set_y(s_tileview, 0);
    }

    lv_obj_set_tile(s_tileview, tile, AREX_TILE_ANIM_ENABLED ? LV_ANIM_ON : LV_ANIM_OFF);

    /* 棣栧睆/閲嶅缓鍚庨娆¤繘鍗℃椂锛屽綋鍓?tile 鍙兘娌℃湁鍚庣画鑴忔暟鎹┍鍔ㄥ埛鏂般€?
     * 杩欓噷涓诲姩琛ヤ竴娆″綋鍓嶅彲瑙侀〉鐨勫竷灞€鍜岄噸缁橈紝閬垮厤蹇呴』绛夌敤鎴锋棆閽氦浜掑悗鎵嶅畬鏁存樉绀恒€?*/
    lv_obj_update_layout(tile);
    lv_obj_invalidate(tile);

    if (g_sys_card_order(tile_pos) == CARD_ID_COMPASS)
    {
#if BLE_COMPASS_DIAG_LOG_ENABLED
        ble_sensor_debug_note_ui_force_refresh(g_sensor_data.heading);
#if BLE_COMPASS_DIAG_SYSTEM_LOG_ENABLED
#endif
#endif
        if (s_heading_val_lbl)
        {
            lv_label_set_text_fmt(s_heading_val_lbl, "%03d", g_sensor_data.heading);
        }
        if (s_heading_hint_lbl)
        {
            if (g_sensor_data.heading_locked)
            {
                lv_label_set_text_fmt(s_heading_hint_lbl, "[ TARGET LOCKED: %03d掳 ]", g_sensor_data.heading_target);
            }
            else
            {
                lv_label_set_text(s_heading_hint_lbl, "[ ENTER ] mark heading");
            }
        }
        if (s_compass_tape_obj)
        {
            lv_obj_invalidate(s_compass_tape_obj);
        }
    }

    /* SETUP(鏈€鍚庝竴椤? 涓嶆樉绀?dots锛屽彧鏈?DASH 鍔ㄦ€佸崱鐗囨墠鏇存柊 */
    if (tile_pos >= CARD_POS_DYNAMIC_FIRST && tile_pos < arex_setup_display_pos())
    {
        /* 璁＄畻閫昏緫绱㈠紩锛氫粠 DYNAMIC_FIRST 鍒?tile_pos 涔嬮棿鏈夊灏戜釜鏈夋晥鍗＄墖 */
        uint8_t active_idx = 0;
        for (uint8_t pos = CARD_POS_DYNAMIC_FIRST; pos < tile_pos; pos++)
        {
            uint8_t card_id = g_sys_card_order(pos);
            if (card_id != CARD_ID_UNUSED && card_id != CARD_ID_BLANK)
            {
                active_idx++;
            }
        }
        arex_screen_update_scroll_dots(active_idx, true);
    }
    else
    {
        arex_screen_update_scroll_dots(0, false);
    }
}

/* =========================================================
 * 宸︿晶闈㈡澘鍒锋柊 (浣跨敤 arex_widget_set_value API)
 *
 * 褰诲簳濮旀墭缁?arex_widget_set_value()锛岄€氳繃 user_data 鐑欏嵃鑷姩瀹氫綅
 * 宸︿晶閿氱偣 + 5F 缃戞牸涓墍鏈夋墦浜嗙儥鍗扮殑缁勪欢骞舵洿鏂版暟鍊笺€?
 * 涓嶅啀鐩存帴鎿嶄綔 s_lbl_* 鍙ユ焺锛屽交搴曡В鑰︼紒
 *
 * 2x7 缃戞牸甯冨眬:
 *   Row 0: NDL      | (2x1)
 *   Row 1-2: DEPTH  | (2x2, 甯?sudu 閫熺巼鍥炬爣)
 *   Row 3: POD1     | POD2   (鍚?1x1)
 *   Row 4: TIME     | (2x1, 娼滄按鏃堕棿 MM:SS)
 *   Row 5: GAS      | (2x1, 褰撳墠姘斾綋鍚嶇О)
 *   Row 6: SYS      | (2x1, 绯荤粺鏃堕棿)
 * ========================================================= */
/* =========================================================
 * 宸︿晶闈㈡澘鍒锋柊 鈥?鏁版嵁婧愯嚜鍔ㄦ帹瀵煎紩鎿?
 *
 * 閾佸緥锛氬彧璇?g_sys_config.left_widgets[] 鏁扮粍锛屾牴鎹?widget_id 璺敱鍒板搴旂殑 g_sensor_data 瀛楁銆?
 * 姣忔淇敼甯冨眬锛堣皟鏁?g_left_widgets[] 椤哄簭/绫诲瀷锛夊悗锛屾鍑芥暟鑷姩鍚屾锛屾棤闇€鎵嬪姩缁存姢銆?
 * ========================================================= */
/* =========================================================
 * 鍏ㄥ睆缁勪欢鏁版嵁鍚屾鎺ュ彛
 *
 * 鍚屾椂鍒锋柊宸︿晶閿氱偣鍜屽彸渚?5F 鑷畾涔夌綉鏍肩殑鎵€鏈夌粍浠躲€?
 * 鍐呴儴璋冪敤 arex_widget_sync_data() 璺敱鍒嗗彂鍣紝瀹炵幇鍏ㄩ噺鏁版嵁鑷姩瀵归綈銆?
 * ========================================================= */
void arex_screen_refresh_all_widgets(void)
{
    /* 1. 鍚屾宸︿晶鍥哄畾鍖洪厤缃?*/
    for (uint8_t i = 0; i < g_sys_config.left_widget_count; i++)
    {
        if (g_sys_config.left_widgets[i].widget_id != WIDGET_EMPTY)
        {
            arex_widget_sync_data(g_sys_config.left_widgets[i].widget_id);
        }
    }

    /* 2. 鍚屾鍙充晶鍏ㄩ儴鑷畾涔夊崱鐗囬厤缃?*/
    for (uint8_t card_idx = 0;
            card_idx < g_sys_config.custom_card_count && card_idx < AREX_MAX_CUSTOM_CARDS;
            card_idx++)
    {
        for (uint8_t i = 0; i < g_sys_config.custom_cards[card_idx].widget_count; i++)
        {
            if (g_sys_config.custom_cards[card_idx].widgets[i].widget_id != WIDGET_EMPTY)
            {
                arex_widget_sync_data(g_sys_config.custom_cards[card_idx].widgets[i].widget_id);
            }
        }
    }
}

/* =========================================================
 * 鍏煎鏃ф帴鍙ｏ細浠呭埛鏂板乏渚ч潰鏉?
 * 鍐呴儴濮旀墭缁?arex_widget_sync_data()锛屾秷闄ゅ啑浣?switch-case
 * ========================================================= */
void arex_screen_refresh_left_panel(void)
{
    for (uint8_t i = 0; i < g_sys_config.left_widget_count; i++)
    {
        if (g_sys_config.left_widgets[i].widget_id != WIDGET_EMPTY)
        {
            arex_widget_sync_data(g_sys_config.left_widgets[i].widget_id);
        }
    }
}

/* =========================================================
 * Wall indicators
 * ========================================================= */
static const char *charge_blocks[] =
{
    "[   ]   [   ]   [   ]",
    "[ \xE2\x96\xA0 ]   [   ]   [   ]",
    "[ \xE2\x96\xA0 ]   [ \xE2\x96\xA0 ]   [   ]",
    "[ \xE2\x96\xA0 ]   [ \xE2\x96\xA0 ]   [ \xE2\x96\xA0 ]",
};

static void wall_nudge_tileview(lv_coord_t offset_y)
{
    /* 銆愰棶棰樹笁淇銆憇_tileview 鍙兘涓?NULL锛堝竷灞€閲嶅缓鏈熼棿锛?*/
    if (!s_tileview) return;

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
    /* 銆愰棶棰樹笁淇銆憇_tileview 鍙兘涓?NULL锛堝竷灞€閲嶅缓鏈熼棿锛?*/
    if (!s_tileview) return;

    if (charge > 3) charge = 3;

    lv_obj_t *wall    = (side == WALL_TOP) ? s_wall_top    : s_wall_bottom;
    lv_obj_t *txt_lbl = (side == WALL_TOP) ? s_wall_text_top    : s_wall_text_bottom;
    lv_obj_t *blk_lbl = (side == WALL_TOP) ? s_wall_blocks_top  : s_wall_blocks_bottom;
    if (!wall || !txt_lbl || !blk_lbl) return;

    lv_label_set_text(txt_lbl, text);
    lv_label_set_text(blk_lbl, charge_blocks[charge]);
    lv_obj_clear_flag(wall, LV_OBJ_FLAG_HIDDEN);

    lv_coord_t nudge = (lv_coord_t)(charge * 20);
    wall_nudge_tileview(side == WALL_TOP ? nudge : -nudge);
}

void arex_screen_hide_walls(void)
{
    /* 銆愰棶棰樹笁淇銆憇_tileview 鍙兘涓?NULL锛堝竷灞€閲嶅缓鏈熼棿锛?*/
    if (!s_tileview) return;
    if (!s_wall_top || !s_wall_bottom) return;

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
    /* 銆愰棶棰樹笁淇銆憇_tileview 鍙兘涓?NULL锛堝竷灞€閲嶅缓鏈熼棿锛?*/
    if (!s_tileview) return;
    if (!s_wall_top || !s_wall_bottom) return;

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
    uint8_t visible_dash = arex_visible_dash_count();

    printf("[DOTS] update: active=%u, visible=%d, state=%d, dots_pos=%d, visible_dash=%u, dot_cont_children=%d\r\n",
           active_idx, visible, g_ui.state, g_sys_config.dots_position, visible_dash,
           s_dot_cont ? lv_obj_get_child_cnt(s_dot_cont) : -1);

    for (uint8_t i = 0; i < AREX_DASH_CARD_COUNT; i++)
    {
        if (!s_scroll_dots[i])
        {
            if (i < visible_dash)
            {
                printf("[DOTS] WARN: s_scroll_dots[%u] is NULL but visible_dash=%u!\r\n", i, visible_dash);
            }
            continue;
        }
        bool show = visible && in_dash_or_edit && dots_enabled && (i < visible_dash);
        if (!show)
        {
            lv_obj_add_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(s_scroll_dots[i], LV_OBJ_FLAG_HIDDEN);
        if (i == active_idx)
        {
            lv_obj_set_style_bg_color(s_scroll_dots[i], AREX_GREEN, 0);
            lv_obj_set_style_shadow_width(s_scroll_dots[i], 8, 0);
            lv_obj_set_style_shadow_color(s_scroll_dots[i], AREX_GREEN, 0);
            lv_obj_set_style_shadow_opa(s_scroll_dots[i], 255, 0);
        }
        else
        {
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
    for (uint32_t i = 0; i < cnt; i++)
    {
        lv_obj_t *item = lv_obj_get_child(s_info_list, i);
        lv_obj_t *lbl  = lv_obj_get_child(item, 0);
        if (i == idx)
        {
            lv_obj_set_style_bg_color(item, AREX_GREEN, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            if (lbl) lv_obj_set_style_text_color(lbl, AREX_BLACK, 0);
        }
        else
        {
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
    for (uint32_t i = 0; i < cnt; i++)
    {
        lv_obj_t *item  = lv_obj_get_child(s_setup_list, i);
        lv_obj_t *lbl   = lv_obj_get_child(item, 0);
        lv_obj_t *badge = lv_obj_get_child(item, 1);
        if (i == idx)
        {
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, AREX_GREEN, 0);
            lv_obj_set_style_border_width(item, AREX_INNER_BORDER_W + 2, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);
                lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
            }
            if (badge)
            {
                lv_obj_set_style_text_color(badge, AREX_LIGHT, 0);
                lv_obj_set_style_text_font(badge, arex_get_font(AREX_FONT_ID_TITLE), 0);
            }
        }
        else
        {
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, AREX_DARK, 0);
            lv_obj_set_style_border_width(item, AREX_INNER_BORDER_W, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
                lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
            }
            if (badge)
            {
                lv_obj_set_style_text_color(badge, AREX_LIGHT, 0);
                lv_obj_set_style_text_font(badge, arex_get_font(AREX_FONT_ID_SMALL), 0);
            }
        }
    }
}

uint8_t arex_screen_setup_item_count(void)
{
    if (!s_setup_list) return 0;
    return (uint8_t)lv_obj_get_child_cnt(s_setup_list);
}

void arex_screen_register_info_list(lv_obj_t *list)
{
    s_info_list  = list;
}
void arex_screen_register_setup_list(lv_obj_t *list)
{
    s_setup_list = list;
}

/* =========================================================
 * Sub-menu layer
 * ========================================================= */
static void submenu_slide_in(void)
{
    if (!s_submenu_layer) return;
    uint16_t right_w_fallback = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                                - g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t slide_w = s_cached_right_w > 0 ? s_cached_right_w : right_w_fallback;

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
    uint16_t right_w_fallback = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                                - g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t slide_w = s_cached_right_w > 0 ? s_cached_right_w : right_w_fallback;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_submenu_layer);
    lv_anim_set_values(&a, 0, slide_w);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_time(&a, 250);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);
}

static void submenu_populate(const char *title, const char **items, uint8_t count)
{
    if (!s_submenu_title || !s_submenu_list) return;

    lv_label_set_text(s_submenu_title, title);
    lv_obj_clean(s_submenu_list);
    s_light_status_lbl = NULL;  /* 閲嶇疆 LIGHT 鐘舵€佹爣绛?*/

    /* right_w 浠庣紦瀛樿鍙栵紝fallback = safe_zone_w - left_anchor_w - panel_gap */
    uint16_t right_w = (s_cached_right_w > 0)
                       ? s_cached_right_w
                       : (g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - g_sys_config.panel_gap_u * AREX_BASE_U);
    uint16_t sub_w = right_w;
    int item_h = (int)(g_sys_config.h_menu_item * AREX_BASE_U);  /* 5U=50px */
    int item_w = (int)sub_w - 15;
    int gap_y  = (int)(g_sys_config.gap_menu * AREX_BASE_U);   /* 1U=10px */
    int current_y = 0;

    for (uint8_t i = 0; i < count; i++)
    {
        lv_obj_t *item = lv_obj_create(s_submenu_list);
        lv_obj_set_pos(item, 0, current_y);
        lv_obj_set_size(item, item_w, item_h);
        lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, AREX_DARK, 0);
        lv_obj_set_style_border_width(item, AREX_INNER_BORDER_W, LV_PART_MAIN);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        /* LIGHT CONTROL 鐗规畩甯冨眬: LIGHT 宸? ON/OFF 鍙?*/
        if (strcmp(title, "LIGHT CONTROL") == 0 && i == 0)
        {
            /* "LIGHT" 鏍囩鍦ㄥ乏渚?*/
            lv_obj_t *lbl_light = lv_label_create(item);
            lv_obj_set_style_text_color(lbl_light, AREX_GREEN, 0);
            lv_obj_set_style_text_font(lbl_light, arex_get_font(AREX_FONT_ID_TITLE), 0);
            lv_obj_set_size(lbl_light, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(lbl_light, LV_ALIGN_LEFT_MID, 12, 0);
            lv_obj_set_style_text_align(lbl_light, LV_TEXT_ALIGN_LEFT, 0);
            lv_label_set_text(lbl_light, "LIGHT");

            /* "ON"/"OFF" 鏍囩鍦ㄥ彸渚?*/
            lv_obj_t *lbl_status = lv_label_create(item);
            lv_obj_set_style_text_color(lbl_status, g_light_power_state ? AREX_GREEN : AREX_LIGHT, 0);
            lv_obj_set_style_text_font(lbl_status, arex_get_font(AREX_FONT_ID_TITLE), 0);
            lv_obj_set_size(lbl_status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(lbl_status, LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(lbl_status, g_light_power_state ? "ON" : "OFF");

            /* 淇濆瓨鐘舵€佹爣绛惧紩鐢紝鐢ㄤ簬鐐瑰嚮鏃舵洿鏂?*/
            s_light_status_lbl = lbl_status;
            current_y += item_h + gap_y;
            continue;
        }

        /* 鏅€氳彍鍗曢」 */
        lv_obj_t *lbl = lv_label_create(item);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
        lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
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
    uint32_t cnt = lv_obj_get_child_cnt(s_submenu_list);
    for (uint32_t i = 0; i < cnt; i++)
    {
        lv_obj_t *item = lv_obj_get_child(s_submenu_list, i);
        lv_obj_t *lbl  = lv_obj_get_child(item, 0);
        /* 姝ｅ湪缂栬緫鐨?item 鐢?begin_edit_value 鍗曠嫭绠＄悊锛屼笉鍙備笌閫変腑鎬佸埛鏂?*/
        if (g_ui.edit_ctx.active && (uint8_t)i == g_ui.edit_ctx.item_index) continue;
        if (i == idx)
        {
            lv_obj_add_state(item, LV_STATE_FOCUSED);  // HOTFIX: Clear LVGL states to fix bold residue.
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, AREX_GREEN, 0);
            lv_obj_set_style_border_width(item, AREX_INNER_BORDER_W + 2, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);
                lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
            }
            /* LIGHT CONTROL second column uses the same selected emphasis. */
            lv_obj_t *lbl2 = lv_obj_get_child(item, 1);
            if (lbl2)
            {
                lv_obj_set_style_text_color(lbl2, AREX_LIGHT, 0);
                lv_obj_set_style_text_font(lbl2, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
            }
        }
        else
        {
            lv_obj_clear_state(item, LV_STATE_FOCUSED | LV_STATE_EDITED | LV_STATE_CHECKED);  // HOTFIX: Clear LVGL states to fix bold residue.
            if (lbl) lv_obj_clear_state(lbl, LV_STATE_ANY);  // HOTFIX: Clear LVGL states to fix bold residue.
            lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(item, AREX_DARK, 0);
            lv_obj_set_style_border_width(item, AREX_INNER_BORDER_W, 0);
            if (lbl)
            {
                lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
                lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
            }
            /* LIGHT CONTROL 鐗规畩澶勭悊锛氱浜屽垪锛圤N/OFF锛夋仮澶嶇姸鎬佽壊 */
            lv_obj_t *lbl2 = lv_obj_get_child(item, 1);
            if (lbl2)
            {
                lv_obj_set_style_text_color(lbl2, g_light_power_state ? AREX_GREEN : AREX_LIGHT, 0);
                lv_obj_set_style_text_font(lbl2, arex_get_font(AREX_FONT_ID_TITLE), 0);
            }
        }
    }
}

/* INFO sub-menu */
static const char *s_info_titles[] =
{
    "> LAST DIVE", "> DIVE PLAN", "> TISSUE & TOX", "> GAS & CALC", "> SENSOR & DEVICE"
};

static char s_info_str[5][5][32];
static const char *s_info_dyn[5][6];

// HOTFIX: Removed soft BACK buttons.
static void build_info_submenu(uint8_t idx)
{
    uint8_t n = 0;
    switch (idx)
    {
    case 0:
        snprintf(s_info_str[0][0], 32, "MAX DEPTH: %dm", (int)g_sensor_data.depth);
        snprintf(s_info_str[0][1], 32, "DIVE TIME: %dm", (int)(g_sensor_data.dive_time_s / 60));
        s_info_dyn[0][n++] = s_info_str[0][0];
        s_info_dyn[0][n++] = s_info_str[0][1];
        s_info_dyn[0][n++] = "SURFACE INT: 2h 10m";
        break;
    case 1:
        s_info_dyn[1][n++] = "VIEW PROFILE";
        s_info_dyn[1][n++] = "RECALCULATE";
        break;
    case 2:
        snprintf(s_info_str[2][0], 32, "GF: %d/%d", 30, 70);
        snprintf(s_info_str[2][1], 32, "CNS: %d%%", g_sensor_data.cns_pct);
        snprintf(s_info_str[2][2], 32, "OTU: %d", g_sensor_data.otu);
        s_info_dyn[2][n++] = "VIEW BAR GRAPH";
        s_info_dyn[2][n++] = s_info_str[2][0];
        s_info_dyn[2][n++] = s_info_str[2][1];
        s_info_dyn[2][n++] = s_info_str[2][2];
        break;
    case 3:
        snprintf(s_info_str[3][0], 32, "GAS 1: %s", g_sensor_data.gas_name);
        s_info_dyn[3][n++] = s_info_str[3][0];
        s_info_dyn[3][n++] = "ALGO: ZHL-16C";
        break;
    case 4:
        if (g_sensor_data.pod1_bar <= 0.0f)
            snprintf(s_info_str[4][0], 32, "POD 1: -- BAR");
        else
            snprintf(s_info_str[4][0], 32, "POD 1: %.0f BAR", g_sensor_data.pod1_bar);
        if (g_sensor_data.pod2_bar <= 0.0f)
            snprintf(s_info_str[4][1], 32, "POD 2: -- BAR");
        else
            snprintf(s_info_str[4][1], 32, "POD 2: %.0f BAR", g_sensor_data.pod2_bar);
        float battery_pct = g_sensor_data.battery_pct;
        if (battery_pct < 0.0f)
        {
            battery_pct = 0.0f;
        }
        else if (battery_pct > 100.0f)
        {
            battery_pct = 100.0f;
        }
        snprintf(s_info_str[4][2], 32, "BATTERY: %.0f%%", battery_pct);
        snprintf(s_info_str[4][3], 32, "TEMP: 24C");
        s_info_dyn[4][n++] = s_info_str[4][0];
        s_info_dyn[4][n++] = s_info_str[4][1];
        s_info_dyn[4][n++] = s_info_str[4][2];
        s_info_dyn[4][n++] = s_info_str[4][3];
        break;
    default:
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
// Remove 'SELECT ' prefix
static const char *s_setup_sub[][7] =
{
    { "AIR", "NX 32", "TX 18/45", "O2 100%", NULL },
    { "LOW (GF 40/85)", "MED (GF 30/70)", "HIGH (GF 20/65)", "GF 50/70", NULL },
    { "LOW", "ECO", "MED", "HIGH", "MAX", "SUN", NULL },
    { "AUTO CAL: AUTO", "RESET AUTO CAL", NULL },
    { "LIGHT ON/OFF", "RED COLOR", "GREEN COLOR", "BLUE COLOR", "WHITE COLOR", NULL },
    { "VERSION: " AREX_SYSTEM_VERSION, "MODE SETUP", "DIVE MENU", "AI SETUP", "ALERTS SETUP", "DISPLAY" },
};

static const char *s_setup_titles[] =
{
    "GAS SWITCH", "CONSERVATISM", "BRIGHTNESS", "COMPASS CAL", "LIGHT CONTROL", "SYSTEMS SETUP"
};

static const char *s_nested_red[]    = { "10%", "30%", "50%", "70%", "100%", NULL };
static const char *s_nested_green[]  = { "10%", "30%", "50%", "70%", "100%", NULL };
static const char *s_nested_blue[]   = { "10%", "30%", "50%", "70%", "100%", NULL };
static const char *s_nested_white[]  = { "10%", "30%", "50%", "70%", "100%", NULL };
static char s_compass_cal_status_str[24];
static const char *s_compass_cal_items[] = { s_compass_cal_status_str, "RESET AUTO CAL", NULL };

static const char *compass_cal_status_text(void)
{
    arex_compass_cal_ui_state_t st = arex_get_compass_calibration_ui_state();
    if (st == AREX_COMPASS_CAL_RUNNING) return "LEARN";
    if (st == AREX_COMPASS_CAL_READY) return "OK";
    return "AUTO";
}

static const char **build_compass_cal_submenu(uint8_t *out_count)
{
    lv_snprintf(s_compass_cal_status_str,
                sizeof(s_compass_cal_status_str),
                "AUTO CAL: %s",
                compass_cal_status_text());
    if (out_count)
    {
        *out_count = 2;
    }
    return s_compass_cal_items;
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
    const char **items = build_compass_cal_submenu(&count);
    if (count == 0)
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
    if (item_idx >= 6) return;
    uint8_t count = 0;
    const char **items = s_setup_sub[item_idx];
    if (strcmp(s_setup_titles[item_idx], "COMPASS CAL") == 0)
    {
        items = build_compass_cal_submenu(&count);
    }
    else
    {
        while (count < 7 && items[count]) count++;
    }
    submenu_populate(s_setup_titles[item_idx], items, count);
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

/* Nested sub-menus */
static const char *s_nested_mode_setup[]   = { "AIR", "NITROX", "3 GAS NX", "GAUGE", NULL };
static const char *s_nested_ai_setup[]     = { "PAIR T1", "PAIR T2", "GTR MODE: ON", NULL };
static const char *s_nested_alerts_setup[] = { "DEPTH ALARM: 40m", "TIME ALARM: 60m", "LOW NDL: 5m", "TEST VIBRATION", NULL };
static const char *s_nested_display_sys[]  = { "UNITS: METRIC", "DATE & CLOCK", "LOG RATE: 10s", "BLUETOOTH: OFF", "RESET DEFAULTS", NULL };

static char s_modppo2_str[20];
static const char *s_nested_dive_setup[5];

static void build_nested_dive_setup(void)
{
    extern arex_sensor_data_t g_sensor_data;
    (void)g_sensor_data;
    snprintf(s_modppo2_str, sizeof(s_modppo2_str), "MOD PO2: %.1f", 1.4f);
    s_nested_dive_setup[0] = "SALINITY: FRESH";
    s_nested_dive_setup[1] = s_modppo2_str;
    s_nested_dive_setup[2] = "SAFETY STOP: 3 MIN";
    s_nested_dive_setup[3] = "ALTITUDE: AUTO";
    s_nested_dive_setup[4] = NULL;
}

static const char **nested_items_for(const char *title, uint8_t *out_count)
{
    const char **tbl = NULL;
    if      (strcmp(title, "MODE SETUP")    == 0) tbl = s_nested_mode_setup;
    else if (strcmp(title, "DIVE MENU")    == 0)
    {
        build_nested_dive_setup();
        tbl = s_nested_dive_setup;
    }
    else if (strcmp(title, "AI SETUP")      == 0) tbl = s_nested_ai_setup;
    else if (strcmp(title, "ALERTS SETUP")  == 0) tbl = s_nested_alerts_setup;
    else if (strcmp(title, "DISPLAY") == 0) tbl = s_nested_display_sys;
    else if (strcmp(title, "RED")    == 0) tbl = s_nested_red;
    else if (strcmp(title, "GREEN")  == 0) tbl = s_nested_green;
    else if (strcmp(title, "BLUE")   == 0) tbl = s_nested_blue;
    else if (strcmp(title, "WHITE")  == 0) tbl = s_nested_white;

    if (tbl && out_count)
    {
        *out_count = 0;
        while (*out_count < 8 && tbl[*out_count]) (*out_count)++;
    }
    return tbl;
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

    // HOTFIX: Block action for Info items.
    if (strcmp(cur_title, "LAST DIVE") == 0 ||
            strcmp(cur_title, "TISSUE & TOX") == 0 ||
            strcmp(cur_title, "GAS & CALC") == 0 ||
            strcmp(cur_title, "SENSOR & DEVICE") == 0)
    {
        return;
    }

    /* LIGHT CONTROL 棰滆壊閫夐」澶勭悊锛堝繀椤诲湪閫氱敤 > 澶勭悊涔嬪墠锛?*/
    if (strcmp(cur_title, "LIGHT CONTROL") == 0 && strstr(text, "COLOR") != NULL)
    {
        /* 浠?"RED COLOR >" 鎻愬彇棰滆壊鍚?*/
        char color_name[20] = {0};
        if (strncmp(text, "RED", 3) == 0) strcpy(color_name, "RED");
        else if (strncmp(text, "GREEN", 5) == 0) strcpy(color_name, "GREEN");
        else if (strncmp(text, "BLUE", 4) == 0) strcpy(color_name, "BLUE");
        else if (strncmp(text, "WHITE", 5) == 0) strcpy(color_name, "WHITE");

        /* 閫氳繃 nested_items_for 鑾峰彇棰滆壊浜害閫夐」锛堜笓闂ㄧ殑浜岀骇宓屽鑿滃崟锛?*/
        uint8_t ncnt = 0;
        const char **color_items = nested_items_for(color_name, &ncnt);
        if (color_items && ncnt > 0)
        {
            arex_screen_open_nested_submenu(color_name, color_items, ncnt);
        }
        return;
    }

    /* LIGHT CONTROL 寮€鍏冲鐞嗭紙绗竴椤癸紝鐐瑰嚮鍒囨崲 ON/OFF 鐘舵€侊級 */
    if (strcmp(cur_title, "LIGHT CONTROL") == 0 && item_idx == 0)
    {
        extern void arex_bus_set_light_power(bool on);
        g_light_power_state = !g_light_power_state;
        arex_bus_set_light_power(g_light_power_state);

        /* 鏇存柊鐘舵€佹爣绛?*/
        if (s_light_status_lbl)
        {
            lv_label_set_text(s_light_status_lbl, g_light_power_state ? "ON" : "OFF");
        }
        /* 閲嶆柊璁剧疆閫変腑鎬侊紝纭繚 ON/OFF 鍙橀粦鑹?*/
        arex_screen_set_submenu_selection(g_ui.sub_menu_idx);
        return;
    }

    if (text[strlen(text) - 1] == '>')
    {
        char nested_name[40] = {0};
        size_t len = strlen(text);
        size_t copy_len = (len >= 2) ? len - 2 : 0;
        if (copy_len >= sizeof(nested_name)) copy_len = sizeof(nested_name) - 1;
        memcpy(nested_name, text, copy_len);
        while (copy_len > 0 && nested_name[copy_len - 1] == ' ')
        {
            nested_name[--copy_len] = '\0';
        }
        uint8_t ncnt = 0;
        const char **nitems = nested_items_for(nested_name, &ncnt);
        if (nitems && ncnt > 0)
        {
            arex_screen_open_nested_submenu(nested_name, nitems, ncnt);
        }
        return;
    }

    if (strcmp(cur_title, "GAS SWITCH") == 0)
    {
        const char *gas_name = text;
        if (strncmp(text, "SELECT ", 7) == 0) gas_name = text + 7;
        extern const char *AREX_GAS_NAMES[4];
        for (uint8_t i = 0; i < 4; i++)
        {
            if (strcmp(AREX_GAS_NAMES[i], gas_name) == 0)
            {
                // HOTFIX: Route gas switch to safety modal.
                g_ui.gas_cursor = i;
                g_ui.gas_modal_from_submenu = true;  // HOTFIX: Route GAS modal exit based on context.
                arex_screen_show_modal_gas();
                g_ui.state = UI_MODAL_GAS;
                return;
            }
        }
        return;
    }

    if (strcmp(cur_title, "CONSERVATISM") == 0)
    {
        if (strcmp(text, "< BACK") != 0)
        {
            /* 瑙ｆ瀽 conservatism 绾у埆锛歀OW/MED/HIGH/GF 50/70 */
            uint8_t level = 1;  /* 榛樿 MED */
            if (strncmp(text, "LOW", 3) == 0) level = 0;
            else if (strncmp(text, "MED", 3) == 0) level = 1;
            else if (strncmp(text, "HIGH", 4) == 0) level = 2;
            else if (strncmp(text, "GF 50/70", 8) == 0) level = 3;

            /* 璋冪敤澶栭儴涓氬姟灞傚洖璋冿紙weak 瀹炵幇浼氳皟鐢ㄥ唴閮?bus锛?*/
            extern void arex_ui_on_conservatism_set(uint8_t level);
            arex_ui_on_conservatism_set(level);

            arex_screen_refresh_setup_menu();
        }
        arex_screen_update_setup_badge(1, text);
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
            /* 璁剧疆浜害骞舵洿鏂?badge */
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
            /* 瀹為檯璁剧疆灞忓箷浜害锛堟ā鎷熷櫒鐗堟湰锛?*/
            arex_set_brightness(g_sys_config.brightness);
        }
        arex_screen_update_setup_badge(2, text);
        arex_screen_close_submenu();
        return;
    }

    /* 棰滆壊浜害璋冭妭瀛愯彍鍗曞鐞嗭紙RED/GREEN/BLUE/WHITE锛?*/
    if (strcmp(cur_title, "RED") == 0 || strcmp(cur_title, "GREEN") == 0 ||
            strcmp(cur_title, "BLUE") == 0 || strcmp(cur_title, "WHITE") == 0)
    {
        /* 鐢ㄦ埛閫夋嫨浜嗕寒搴︾櫨鍒嗘瘮 */
        if (strcmp(text, "< BACK") != 0)
        {
            /* 鍥炶皟缁欎笟鍔″眰澶勭悊 */
            extern void arex_ui_on_light_color_set(const char *color, const char *level);
            arex_ui_on_light_color_set(cur_title, text);

        }
        /* 鐩存帴鍏抽棴瀛愯彍鍗曡繑鍥炰笂绾э紝涓嶅脊绐?*/
        arex_screen_close_submenu();
        return;
    }

    if (strcmp(cur_title, "DIVE MENU") == 0)
    {
        if (strncmp(text, "MOD PO2:", 8) == 0 || strncmp(text, "MOD PO2 ", 8) == 0)
        {
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
        for (uint8_t i = 0; i < 5 && !found; i++)
        {
            const char *setup_title_stripped = s_setup_titles[i];
            if (setup_title_stripped[0] == '>' && setup_title_stripped[1] == ' ')
                setup_title_stripped += 2;
            if (strcmp(prev_title, setup_title_stripped) == 0)
            {
                uint8_t cnt = 0;
                const char **items = s_setup_sub[i];
                if (strcmp(setup_title_stripped, "COMPASS CAL") == 0)
                {
                    items = build_compass_cal_submenu(&cnt);
                }
                else
                {
                    while (cnt < 6 && items[cnt]) cnt++;
                }
                submenu_populate(s_setup_titles[i], items, cnt);
                g_ui.sub_item_count = cnt;
                g_ui.sub_menu_idx   = h->idx;
                if (cnt > 0 && g_ui.sub_menu_idx >= cnt)
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
            const char **nitems = nested_items_for(prev_title, &ncnt);
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
    if (g_ui.state == UI_MODAL_ACT)
    {
        g_ui.state = (g_ui.sub_item_count > 0) ? UI_SUB_MENU : UI_DASH;
    }
    lv_timer_del(t);
}

static void modal_set_content(const char *title, const char *body, const char *hint)
{
    if (!s_modal_box) return;
    lv_obj_clean(s_modal_box);

    lv_obj_t *t = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(t, AREX_GREEN, 0);
    lv_obj_set_style_text_font(t, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_label_set_text(t, title);
    lv_obj_set_pos(t, 0, 0);

    lv_obj_t *b = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(b, AREX_GREEN, 0);
    lv_obj_set_style_text_font(b, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
    lv_label_set_text(b, body);
    lv_obj_set_pos(b, 0, 40);

    lv_obj_t *h = lv_label_create(s_modal_box);
    lv_obj_set_style_text_color(h, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(h, arex_get_font(AREX_FONT_ID_SMALL), 0);
    lv_label_set_text(h, hint);
    lv_obj_set_pos(h, 0, 100);
}

void arex_screen_show_modal_act(const char *action_text)
{
    if (!s_modal) return;
    modal_set_content("ACTION", action_text ? action_text : "", "[ ESC TO BACK ]");
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
    g_ui.state = UI_MODAL_ACT;
    lv_timer_create(modal_act_timer_cb, 1000, NULL);
}

void arex_screen_show_modal_gas(void)
{
    if (!s_modal) return;
    uint8_t ci = g_ui.gas_cursor;
    char body[32];
    const char *gas_name = g_sensor_data.gas_slot_name[ci][0]
                           ? g_sensor_data.gas_slot_name[ci]
                           : AREX_GAS_NAMES[ci];
    float mod_m = g_sensor_data.gas_slot_mod_m[ci] > 0.0f
                  ? g_sensor_data.gas_slot_mod_m[ci]
                  : (float)AREX_GAS_MOD_M[ci];
    snprintf(body, sizeof(body), "%s\nMOD: %.0fm", gas_name, (double)mod_m);

    const char *hint = (g_sensor_data.depth > mod_m)
                       ? "[ FATAL: OVER MOD ]"
                       : "[ ENTER CONFIRM ]  [ ESC CANCEL ]";

    modal_set_content("CONFIRM GAS", body, hint);
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void arex_screen_show_modal_compass(void)
{
    if (!s_modal) return;
    modal_set_content("CLEAR TARGET?", "REMOVE HEADING MARKER?",
                      "[ ENTER CONFIRM ]  [ ESC CANCEL ]");
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void arex_screen_pulse_modal(void)
{
    if (!s_modal_box) return;
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
    if (!s_modal) return;
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

/* =========================================================
 * Compass / Gas / Edit callbacks
 * ========================================================= */
void arex_screen_refresh_compass_target(void)
{
    arex_card_t *c = arex_card_get_by_id(CARD_ID_COMPASS);
    if (c && c->update_cb) c->update_cb();
}

void arex_screen_refresh_gas_menu(void)
{
    arex_card_t *c = arex_card_get_by_id(CARD_ID_GAS);
    if (c && c->update_cb) c->update_cb();
}

void arex_screen_refresh_setup_menu(void)
{
    arex_card_t *c = arex_card_get_by_id(CARD_ID_SETUP);
    if (c && c->update_cb) c->update_cb();
}

static void edit_flash_timer_cb(lv_timer_t *t)
{
    (void)t;
    s_edit_flash_on = !s_edit_flash_on;
    if (s_edit_flash_val_lbl)
    {
        /* 鏂囧瓧棰滆壊鍦ㄧ豢/鏆楃豢涔嬮棿闂儊锛屾棤鑳屾櫙鑹插垏鎹?*/
        lv_color_t fg = s_edit_flash_on ? AREX_GREEN : AREX_DARK;
        lv_obj_set_style_text_color(s_edit_flash_val_lbl, fg, 0);
    }
}

static void edit_flash_stop(void)
{
    if (s_edit_flash_timer)
    {
        lv_timer_del(s_edit_flash_timer);
        s_edit_flash_timer = NULL;
    }
    s_edit_flash_badge   = NULL;
    s_edit_flash_val_lbl = NULL;
}

static void edit_flash_start(void)
{
    if (s_edit_flash_timer)
    {
        lv_timer_del(s_edit_flash_timer);
        s_edit_flash_timer = NULL;
    }
    s_edit_flash_on = true;
    s_edit_flash_timer = lv_timer_create(edit_flash_timer_cb, 350, NULL);
}

static void edit_value_cleanup(lv_obj_t *item);

void arex_screen_refresh_edit_value(void)
{
    if (!g_ui.edit_ctx.active || !s_edit_flash_val_lbl || !s_submenu_list) return;
    static float last_drawn = -9999.f;
    float cur = g_ui.edit_ctx.value;
    if (cur == last_drawn) return;   /* dirty check锛氬€兼湭鍙樺垯璺宠繃锛屼笉瑙﹀彂閲嶇粯 */
    last_drawn = cur;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f ^v", cur);
    lv_label_set_text(s_edit_flash_val_lbl, buf);
}

void arex_screen_begin_edit_value(uint8_t item_idx, float value,
                                  float min, float max, float step)
{
    if (!s_submenu_list) return;

    g_ui.edit_ctx.value      = value;
    g_ui.edit_ctx.original   = value;
    g_ui.edit_ctx.min        = min;
    g_ui.edit_ctx.max        = max;
    g_ui.edit_ctx.step       = step;
    g_ui.edit_ctx.item_index = item_idx;
    g_ui.edit_ctx.active     = true;
    g_ui.state = UI_EDIT_VALUE;

    lv_obj_t *item = lv_obj_get_child(s_submenu_list, item_idx);
    if (!item)
    {
        g_ui.edit_ctx.active = false;
        g_ui.state = UI_SUB_MENU;
        return;
    }

    /* 浠庨€変腑鎬佸垏鎹㈠埌缂栬緫鎬侊細缁垮簳鈫掗粦搴曠豢妗嗭紝title 鏂囧瓧鎭㈠缁胯壊 */
    lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(item, AREX_GREEN, 0);
    lv_obj_set_style_border_width(item, 2, 0);

    /* 澶嶇敤 child 0 浣滀负宸︿晶鏍囩锛屾仮澶嶇豢鑹叉枃瀛?*/
    lv_obj_t *prefix_lbl = lv_obj_get_child(item, 0);
    if (prefix_lbl)
    {
        lv_label_set_text(prefix_lbl, "MOD PO2:");
        lv_obj_set_style_text_color(prefix_lbl, AREX_GREEN, 0);
        lv_obj_set_size(prefix_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(prefix_lbl, LV_ALIGN_LEFT_MID, 12, 0);
    }

    /* child 1 鏄?badge label锛圕ONSERVATISM 绛夋棤 badge 鏃朵负 NULL锛夛紝鎭㈠棰滆壊 */
    lv_obj_t *old_badge = lv_obj_get_child(item, 1);
    if (old_badge) lv_obj_set_style_text_color(old_badge, AREX_GREEN, 0);

    /* 鍒涘缓鍙充晶鏁板€?+ 绠ご label锛岄€忔槑鑳屾櫙锛岄潬鍙冲眳涓?*/
    lv_obj_t *val_lbl = lv_label_create(item);
    lv_obj_set_style_text_color(val_lbl, AREX_GREEN, 0);
    lv_obj_set_style_text_font(val_lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_style_bg_opa(val_lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_size(val_lbl, 120, LV_SIZE_CONTENT);
    lv_obj_align(val_lbl, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_text_align(val_lbl, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(val_lbl, LV_LABEL_LONG_DOT);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f ^v", value);
    lv_label_set_text(val_lbl, buf);

    s_edit_flash_badge    = val_lbl;   /* 澶嶇敤鎸囬拡鐢ㄤ簬闂儊 */
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
    if (!s_submenu_list)
    {
        edit_flash_stop();
        g_ui.edit_ctx.active = false;
        return;
    }
    lv_obj_t *item = lv_obj_get_child(s_submenu_list, g_ui.edit_ctx.item_index);
    if (!item)
    {
        edit_flash_stop();
        g_ui.edit_ctx.active = false;
        return;
    }
    edit_value_cleanup(item);
    lv_obj_t *lbl = lv_obj_get_child(item, 0);
    if (lbl)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "MOD PO2: %.1f", g_ui.edit_ctx.value);
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
    }
    g_ui.edit_ctx.active = false;
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
    uint16_t right_w_fallback = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                                - g_sys_config.panel_gap_u * AREX_BASE_U;
    uint16_t right_w = (s_cached_right_w > 0) ? s_cached_right_w : right_w_fallback;

    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);
    lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_pos(lbl, 16, 8);
    lv_obj_set_size(lbl, right_w - 32, 40);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl, text);

    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, right_w - 32, 2);
    lv_obj_set_pos(line, 16, 48);
    lv_obj_set_style_bg_color(line, AREX_DARK, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);

    return lbl;
}

/* =========================================================
 * Light control callbacks (渚涗笟鍔″眰瀵规帴)
 *
 * 浠ヤ笅涓や釜鍑芥暟鏄?UI 灞備笌涓氬姟灞?纭欢灞傜殑瀵规帴鍏ュ彛锛?
 *   - arex_bus_set_light_power()  : 鐢变笟鍔″眰瀹炵幇锛屾帶鍒剁伅鍏夊紑鍏?
 *   - arex_ui_on_light_color_set(): 鐢变笟鍔″眰瀹炵幇锛岃缃鑹蹭寒搴?
 * ========================================================= */

/**
 * 鐏厜寮€鍏虫帶鍒跺洖璋?
 *
 * 璋冪敤鏃舵満锛氬綋鐢ㄦ埛鍦?SETUP > LIGHT CONTROL > LIGHT ON/OFF 鐐瑰嚮鏃惰Е鍙?
 * 璋冪敤鏂瑰悜锛歛rex_screen.c -> 涓氬姟灞?
 *
 * @param on  true=寮€鐏? false=鍏崇伅
 *
 * 銆愪笟鍔″眰瀵规帴鏂瑰紡銆?
 * 鍦ㄤ笟鍔″眰閲嶆柊瀹氫箟姝ゅ嚱鏁帮紝鎺у埗 GPIO锛?
 *
 *   void arex_bus_set_light_power(bool on) {
 *       if (on) {
 *           GPIO_SetBits(PORT_LIGHT_EN, PIN_LIGHT_EN);
 *       } else {
 *           GPIO_ResetBits(PORT_LIGHT_EN, PIN_LIGHT_EN);
 *       }
 *   }
 */
#ifdef PC_SIMULATOR
#else
__attribute__((weak))    //杩欎釜鍦ㄧ湡鏈洪渶瑕佹墦寮€锛岃繖涓槸鐢ㄦ潵寮卞畾涔夌殑
#endif
void arex_bus_set_light_power(bool on)
{
    /* TODO: 涓氬姟灞傚疄鐜?
     *
     * 绀轰緥浼唬鐮侊細
     *   extern void hw_light_set_power(bool state);
     *   hw_light_set_power(on);
     *
     * 姝ゅ浠呮墦鍗版棩蹇椾緵璋冭瘯
     */
    printf("[LIGHT] Power: %s\n", on ? "ON" : "OFF");
}

/**
 * 鐏厜棰滆壊浜害璁剧疆鍥炶皟
 *
 * 璋冪敤鏃舵満锛氬綋鐢ㄦ埛鍦?SETUP > LIGHT CONTROL > [COLOR] > [LEVEL] 鐐瑰嚮鏃惰Е鍙?
 * 璋冪敤鏂瑰悜锛歛rex_screen.c -> 涓氬姟灞?
 *
 * @param color  棰滆壊鍚嶇О: "RED", "GREEN", "BLUE", "WHITE"
 * @param level 浜害绾у埆: "10%", "30%", "50%", "70%", "100%"
 *
 * 銆愪笟鍔″眰瀵规帴鏂瑰紡銆?
 * 鍦ㄤ笟鍔″眰瀹炵幇姝ゅ嚱鏁帮紝澶勭悊 RGBW PWM 鎺у埗锛?
 *
 *   void arex_ui_on_light_color_set(const char *color, const char *level) {
 *       uint8_t duty = 0;
 *       if (strncmp(level, "10", 2) == 0) duty = 25;
 *       else if (strncmp(level, "30", 2) == 0) duty = 76;
 *       else if (strncmp(level, "50", 2) == 0) duty = 127;
 *       else if (strncmp(level, "70", 2) == 0) duty = 178;
 *       else duty = 255;
 *       if (strncmp(color, "RED", 3) == 0) set_pwm(CH_RED, duty);
 *       else if (strncmp(color, "GREEN", 5) == 0) set_pwm(CH_GREEN, duty);
 *       else if (strncmp(color, "BLUE", 4) == 0) set_pwm(CH_BLUE, duty);
 *       else if (strncmp(color, "WHITE", 5) == 0) set_pwm(CH_WHITE, duty);
 *   }
 */
#ifdef PC_SIMULATOR
#else
__attribute__((weak))    //杩欎釜鍦ㄧ湡鏈洪渶瑕佹墦寮€锛岃繖涓槸鐢ㄦ潵寮卞畾涔夌殑
#endif
void arex_ui_on_light_color_set(const char *color, const char *level)
{
    /* TODO: 涓氬姟灞傚疄鐜?
     *
     * 姝ゅ浠呮墦鍗版棩蹇椾緵璋冭瘯
     */
    printf("[LIGHT] Color: %s, Level: %s\n", color, level);
}

/**
 * 灞忓箷浜害璁剧疆鍥炶皟
 *
 * 调用时机：当用户在 SETUP > BRIGHTNESS 选择六档亮度时触发
 * 调用方向：arex_screen.c -> 业务层
 *
 * @param level 亮度级别: 0=LOW, 1=ECO, 2=MED, 3=HIGH, 4=MAX, 5=SUN
 *
 * 銆愪笟鍔″眰瀵规帴鏂瑰紡銆?
 * 鍦ㄤ笟鍔″眰瀹炵幇姝ゅ嚱鏁帮紝鎺у埗灞忓箷鑳屽厜 PWM锛?
 *
 *   void arex_set_brightness(uint8_t level) {
 *       static const uint8_t brightness_map[6] = {25, 40, 60, 80, 100, 120};
 *       uint8_t duty = brightness_map[level < 6 ? level : 0];
 *       set_pwm(BACKLIGHT_CHANNEL, duty);
 *   }
 */
#ifdef PC_SIMULATOR
#else
__attribute__((weak))    //杩欎釜鍦ㄧ湡鏈洪渶瑕佹墦寮€锛岃繖涓槸鐢ㄦ潵寮卞畾涔夌殑
#endif
void arex_apply_software_brightness(uint8_t level)
{
    /* 当前正式策略：面板固定在安全硬件亮度，UI 侧只做温和遮罩。
     * 低档首先保证可读，避免再次出现 “LOW 基本看不见” 的问题。 */
    static const lv_opa_t brightness_opa[6] = {120, 150, 185, 215, 238, 255};
    lv_opa_t opa = brightness_opa[(level < 6) ? level : 0];
    lv_opa_t overlay_opa = (lv_opa_t)(255 - opa);

    if (s_scr == NULL)
    {
        return;
    }

    if (s_brightness_overlay == NULL)
    {
        s_brightness_overlay = lv_obj_create(s_scr);
        lv_obj_remove_style_all(s_brightness_overlay);
        lv_obj_set_size(s_brightness_overlay, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(s_brightness_overlay, lv_color_black(), 0);
        lv_obj_set_style_border_width(s_brightness_overlay, 0, 0);
        lv_obj_clear_flag(s_brightness_overlay, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_brightness_overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_clear_flag(s_brightness_overlay, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_move_foreground(s_brightness_overlay);
    }

    if (!s_software_brightness_enabled || overlay_opa == 0)
    {
        lv_obj_add_flag(s_brightness_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_set_style_bg_opa(s_brightness_overlay, overlay_opa, 0);
        lv_obj_clear_flag(s_brightness_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_brightness_overlay);
    }

    printf("[BRIGHTNESS] Level: %d (OPA: %d overlay=%d)\n", level, opa, overlay_opa);
}

void arex_set_software_brightness_enabled(bool enabled)
{
    s_software_brightness_enabled = enabled;

    if (s_brightness_overlay != NULL)
    {
        if (enabled)
        {
            lv_obj_clear_flag(s_brightness_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(s_brightness_overlay, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

#ifdef PC_SIMULATOR
#else
__attribute__((weak))
#endif
void arex_set_brightness(uint8_t level)
{
    arex_apply_software_brightness(level);
}

/**
 * CONSERVATISM 淇濆畧搴﹁缃洖璋?
 *
 * 璋冪敤鏃舵満锛氬綋鐢ㄦ埛鍦?SETUP > CONSERVATISM 閫夋嫨 LOW/MED/HIGH 鏃惰Е鍙?
 * 璋冪敤鏂瑰悜锛歛rex_screen.c -> 涓氬姟灞?
 *
 * @param level 淇濆畧搴︾骇鍒? 0=LOW, 1=MED, 2=HIGH
 *
 * 銆愪笟鍔″眰瀵规帴鏂瑰紡銆?
 * 鍦ㄤ笟鍔″眰瀹炵幇姝ゅ嚱鏁帮紝鎺у埗鍑忓帇绠楁硶鐨勪繚瀹堝害鍙傛暟锛?
 *
 *   void arex_ui_on_conservatism_set(uint8_t level) {
 *       g_sys_config.conservatism = level;
 *       recalculate_deco_plan();  // 閲嶆柊璁＄畻鍑忓帇璁″垝
 *   }
 */
#ifdef PC_SIMULATOR
#else
__attribute__((weak))
#endif
void arex_ui_on_conservatism_set(uint8_t level)
{
    /* Strong target implementation is provided by buhlmann_task.cpp. */
    (void)level;
}

/* 鑾峰彇 Safe Zone 瀹瑰櫒瀵硅薄锛堜緵鍛婅妯箙浣跨敤锛?*/
lv_obj_t *arex_get_safe_zone(void)
{
    return s_safe_zone;
}
