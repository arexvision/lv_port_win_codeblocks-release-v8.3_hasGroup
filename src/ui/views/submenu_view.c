/*
 * 文件: src/app_ui/ui/views/submenu_view.c
 * 作用: 该文件属于视图表现模块，负责菜单、子菜单、模态框或特定视图状态的展示与交互联动。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "submenu_view.h"

#include "../core/callbacks.h"
#include "../core/data.h"
#include "../screen/screen.h"
#include "menu_actions.h"
#include "menu_runtime.h"
#include "../core/vm/ui_vm_plan_view.h"
#include "../core/vm/ui_vm_system_view.h"
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
    /* 潜水计划页虽然复用子菜单层，但绘制方式和普通列表完全不同。 */
    return menu_runtime_is_dive_plan();
}

static uint16_t submenu_right_width(void)
{
    return (s_submenu_width > 0U)
           ? s_submenu_width
           : (uint16_t)(ui_safe_zone_w_get() - LEFT_ANCHOR_W - ui_panel_gap_px_get());
}

static bool submenu_light_power_on(void)
{
    ui_vm_submenu_view_t vm;

    ui_vm_submenu_view_update(&vm);
    return (vm.light_power_on != 0U);
}

void submenu_view_reset(void)
{
    /* 布局重建后，旧的子菜单对象和菜单运行时都需要一起清空。 */
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
    /* 子菜单层初始放在屏幕右侧外面，通过 slide_in 动画滑入可视区域。 */
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
    /* 打开子菜单时从右侧滑入。 */
    if (!s_submenu_layer) return;
    uint16_t slide_w = submenu_right_width();

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
    /* 关闭子菜单时滑回右侧屏幕外。 */
    if (!s_submenu_layer) return;
    uint16_t slide_w = submenu_right_width();

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
    /* 潜水计划页的文本控件都通过这个轻量工厂统一创建。 */
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
    /* 潜水计划页底部按钮的统一样式工厂。 */
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
    /* 绘制潜水计划页顶部摘要区，展示输入参数和当前气体。 */
    ui_vm_dive_plan_view_t vm;

    ui_vm_dive_plan_view_update(&vm);

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

    plan_make_label(parent,
                    (vm.page == (uint8_t)DIVE_PLAN_PAGE_DEPTH) ? "---" : vm.depth_value,
                    FONT_ID_SMALL,
                    LIGHT,
                    70,
                    34,
                    70,
                    18,
                    LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent,
                    (vm.page <= (uint8_t)DIVE_PLAN_PAGE_TIME) ? "--" : vm.time_value,
                    FONT_ID_SMALL,
                    LIGHT,
                    155,
                    34,
                    70,
                    18,
                    LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent,
                    (vm.page <= (uint8_t)DIVE_PLAN_PAGE_RMV) ? "--" : vm.rmv_value,
                    FONT_ID_SMALL,
                    LIGHT,
                    240,
                    34,
                    70,
                    18,
                    LV_TEXT_ALIGN_CENTER);

    if (vm.header_gas_o2 != 0U)
    {
        {
            char buf[8];
            lv_snprintf(buf, sizeof(buf), "%u", (unsigned)vm.header_gas_o2);
            plan_make_label(parent, buf, FONT_ID_SMALL, LIGHT, w - 74, 12, 54, 18, LV_TEXT_ALIGN_CENTER);
        }
    }
    else
    {
        plan_make_label(parent, "---", FONT_ID_SMALL, LIGHT, w - 74, 12, 54, 18, LV_TEXT_ALIGN_CENTER);
    }
}

static void plan_draw_bottom_line(lv_obj_t *parent, int w)
{
    /* 底部分隔线用于把输入/结果区和动作按钮区分开。 */
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, 0, (int)s_submenu_height - 42);
    lv_obj_set_size(line, w, 1);
    lv_obj_set_style_bg_color(line, GREEN, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
}

static void plan_draw_input(lv_obj_t *parent, int w)
{
    /* 输入页根据当前 page 类型切换显示深度、时间或 RMV。 */
    ui_vm_dive_plan_view_t vm;
    char buf[48];

    ui_vm_dive_plan_view_update(&vm);

    if (vm.page == (uint8_t)DIVE_PLAN_PAGE_TIME)
    {
        plan_make_label(parent, vm.time_value, FONT_ID_MEDIUM, LIGHT, 0, 98, w, 42, LV_TEXT_ALIGN_CENTER);
    }
    else if (vm.page == (uint8_t)DIVE_PLAN_PAGE_RMV)
    {
        plan_make_label(parent, vm.rmv_value, FONT_ID_MEDIUM, LIGHT, 0, 98, w, 42, LV_TEXT_ALIGN_CENTER);
    }
    else
    {
        plan_make_label(parent, vm.depth_value, FONT_ID_MEDIUM, LIGHT, 0, 98, w, 42, LV_TEXT_ALIGN_CENTER);
    }

    lv_obj_t *underline = lv_obj_create(parent);
    lv_obj_remove_style_all(underline);
    lv_obj_set_pos(underline, (w - 38) / 2, 132);
    lv_obj_set_size(underline, 38, 4);
    lv_obj_set_style_bg_color(underline, lv_color_make(255, 255, 0), 0);
    lv_obj_set_style_bg_opa(underline, LV_OPA_COVER, 0);

    plan_make_label(parent, vm.input_prompt, FONT_ID_SMALL, LIGHT, 0, 166, w, 22, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, vm.input_unit, FONT_ID_SMALL, LIGHT, 0, 190, w, 22, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, vm.input_min_text, FONT_ID_SMALL, LIGHT, 0, 224, w, 18, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, vm.input_max_text, FONT_ID_SMALL, LIGHT, 0, 246, w, 18, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *spin = lv_obj_create(parent);
    lv_obj_remove_style_all(spin);
    lv_obj_set_pos(spin, (w - 100) / 2, 276);
    lv_obj_set_size(spin, 100, 25);
    lv_obj_set_style_bg_color(spin, LIGHT, 0);
    lv_obj_set_style_bg_opa(spin, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(spin, DARK, 0);
    lv_obj_set_style_border_width(spin, 1, 0);
    if (vm.page == (uint8_t)DIVE_PLAN_PAGE_TIME)
    {
        lv_snprintf(buf, sizeof(buf), "%s", vm.time_value);
    }
    else if (vm.page == (uint8_t)DIVE_PLAN_PAGE_RMV)
    {
        lv_snprintf(buf, sizeof(buf), "%s", vm.rmv_value);
    }
    else
    {
        lv_snprintf(buf, sizeof(buf), "%s", vm.depth_value);
    }
    plan_make_label(spin, buf, FONT_ID_SMALL, BLACK, 6, 2, 78, 20, LV_TEXT_ALIGN_RIGHT);
    plan_make_label(spin, "^", FONT_ID_SMALL, DARK, 84, 0, 14, 12, LV_TEXT_ALIGN_CENTER);
    plan_make_label(spin, "v", FONT_ID_SMALL, DARK, 84, 12, 14, 12, LV_TEXT_ALIGN_CENTER);
}

static void plan_draw_ready(lv_obj_t *parent, int w)
{
    /* READY 页用于展示“可以开始计算”的摘要状态。 */
    ui_vm_dive_plan_view_t vm;

    ui_vm_dive_plan_view_update(&vm);

    plan_make_label(parent, "Ready to Plan Dive", FONT_ID_SMALL, LIGHT, 0, 106, w, 24, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, vm.ready_gf_text, FONT_ID_SMALL, LIGHT, 96, 168, w - 192, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, vm.ready_last_stop_text, FONT_ID_SMALL, LIGHT, 96, 200, w - 192, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, vm.ready_start_cns_text, FONT_ID_SMALL, LIGHT, 96, 232, w - 192, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, vm.gas_summary, FONT_ID_SMALL, LIGHT, 96, 264, w - 192, 22, LV_TEXT_ALIGN_LEFT);
}

static void plan_draw_result_summary(lv_obj_t *parent, int w, const ui_vm_dive_plan_view_t *vm)
{
    plan_make_label(parent, "SUMMARY", FONT_ID_SMALL, GREEN, 0, 76, w, 22, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, vm->result_runtime_text, FONT_ID_SMALL, LIGHT, 92, 126, w - 184, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, vm->result_deco_text, FONT_ID_SMALL, LIGHT, 92, 164, w - 184, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, vm->result_gas_text, FONT_ID_SMALL, LIGHT, 92, 202, w - 184, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, vm->result_cns_text, FONT_ID_SMALL, LIGHT, 92, 240, w - 184, 22, LV_TEXT_ALIGN_LEFT);
    plan_make_label(parent, vm->result_otu_text, FONT_ID_SMALL, LIGHT, 92, 278, w - 184, 22, LV_TEXT_ALIGN_LEFT);
}

static void plan_draw_result(lv_obj_t *parent, int w)
{
    ui_vm_dive_plan_view_t vm;
    static const int col_x[] = { 20, 88, 166, 244, 334 };
    static const int col_w[] = { 64, 72, 72, 82, 72 };

    ui_vm_dive_plan_view_update(&vm);

    if (vm.result_summary_page != 0U)
    {
        plan_draw_result_summary(parent, w, &vm);
        plan_make_label(parent, vm.result_page_text, FONT_ID_SMALL, GREEN, 0, (int)s_submenu_height - 72, w, 18, LV_TEXT_ALIGN_CENTER);
        return;
    }

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
    for (uint8_t i = 0U; i < 8U; i++)
    {
        if (vm.rows[i].valid == 0U)
        {
            continue;
        }
        plan_make_label(parent, vm.rows[i].depth_text, FONT_ID_SMALL, LIGHT, col_x[0], y, col_w[0], 18, LV_TEXT_ALIGN_CENTER);
        plan_make_label(parent, vm.rows[i].time_text, FONT_ID_SMALL, LIGHT, col_x[1], y, col_w[1], 18, LV_TEXT_ALIGN_CENTER);
        plan_make_label(parent, vm.rows[i].run_text, FONT_ID_SMALL, LIGHT, col_x[2], y, col_w[2], 18, LV_TEXT_ALIGN_CENTER);
        plan_make_label(parent, vm.rows[i].gas_text, FONT_ID_SMALL, LIGHT, col_x[3], y, col_w[3], 18, LV_TEXT_ALIGN_CENTER);
        plan_make_label(parent, vm.rows[i].qty_text, FONT_ID_SMALL, LIGHT, col_x[4], y, col_w[4], 18, LV_TEXT_ALIGN_RIGHT);
        y += 26;
    }

    plan_make_label(parent, vm.result_page_text, FONT_ID_SMALL, GREEN, 0, (int)s_submenu_height - 72, w, 18, LV_TEXT_ALIGN_CENTER);
}

static void plan_draw_error(lv_obj_t *parent, int w)
{
    ui_vm_dive_plan_view_t vm;

    ui_vm_dive_plan_view_update(&vm);

    plan_make_label(parent, vm.error_title, FONT_ID_MEDIUM, LIGHT, 0, 118, w, 40, LV_TEXT_ALIGN_CENTER);
    plan_make_label(parent, vm.error_hint, FONT_ID_SMALL, LIGHT, 0, 176, w, 24, LV_TEXT_ALIGN_CENTER);
}

static void submenu_populate_dive_plan(const menu_row_t *rows, uint8_t count)
{
    ui_vm_dive_plan_view_t vm;

    if (!s_submenu_list) return;

    ui_vm_dive_plan_view_update(&vm);

    int w = (int)submenu_right_width();

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
    switch ((dive_plan_page_t)vm.page)
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
        /* DIVE PLAN 不是普通“纵向菜单列表”，而是一个借用子菜单层承载的独立页面。
         * 因此标题栏、列表尺寸和选中态逻辑都要切到专用分支。 */
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
    uint16_t right_w = submenu_right_width();
    uint16_t sub_w = right_w;
    int item_h = (int)ui_menu_item_h_px_get();
    int item_w = (int)sub_w - 15;
    int gap_y  = (int)ui_menu_gap_px_get();
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
            /* 灯光总开关这一行是双列布局：
             * 左边固定显示 "LIGHT"，右边动态显示 ON/OFF。
             * 单独保留右侧 label 引用，便于点击后只更新状态文字。 */
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
            lv_obj_set_style_text_color(lbl_status, submenu_light_power_on() ? GREEN : LIGHT, 0);
            lv_obj_set_style_text_font(lbl_status, get_font(FONT_ID_TITLE), 0);
            lv_obj_set_size(lbl_status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(lbl_status, LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_text(lbl_status, submenu_light_power_on() ? "ON" : "OFF");

            /* 保存状态标签引用，用于点击时更新 */
            s_light_status_lbl = lbl_status;
            current_y += item_h + gap_y;
            continue;
        }

        /* 普通菜单项 */
        /* 普通项只渲染标题文字，不在 view 层处理点击逻辑。
         * 选中、进入子菜单、编辑数值等动作统一交给 menu_actions / ui_state。 */
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
    ui_state_set_sub_item_count(count);
}

void screen_set_submenu_selection(uint8_t idx)
{
    if (!s_submenu_list) return;
    if (submenu_is_dive_plan_visible())
    {
        uint32_t cnt = lv_obj_get_child_cnt(s_submenu_list);
        uint32_t action_count = ui_state_get_sub_item_count();
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
        if (ui_state_get_edit_active() && ((uint8_t)i == ui_state_get_edit_item_index())) continue;
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
                lv_obj_set_style_text_color(lbl2, submenu_light_power_on() ? GREEN : LIGHT, 0);
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
    ui_state_set_sub_item_count(count);
    ui_state_set_sub_menu_idx(menu_runtime_default_selection());
    ui_state_set_sub_parent(UI_INFO);
    ui_state_set_state(UI_SUB_MENU);
    ui_state_set_sub_history_depth(0U);
    screen_set_submenu_selection(ui_state_get_sub_menu_idx());
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
    ui_state_set_sub_item_count(count);
    if (keep_idx >= count)
    {
        keep_idx = (uint8_t)(count - 1U);
    }
    ui_state_set_sub_menu_idx(keep_idx);
    screen_set_submenu_selection(keep_idx);
}

void screen_refresh_info_submenu_if_open(void)
{
    if (!s_submenu_title || !s_submenu_list)
    {
        return;
    }
    if ((ui_state_get_state() != UI_SUB_MENU) ||
        (ui_state_get_sub_parent() != UI_INFO) ||
        (ui_state_get_sub_history_depth() != 0U))
    {
        return;
    }

    refresh_info_submenu_page(ui_state_get_sub_menu_idx());
}

bool screen_handle_dive_plan_rotate(int8_t dir)
{
    if ((ui_state_get_state() != UI_SUB_MENU) ||
        (ui_state_get_sub_parent() != UI_INFO) ||
        !submenu_is_dive_plan_visible())
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
    ui_state_set_sub_item_count(count);
    if (ui_state_get_sub_menu_idx() >= count)
    {
        ui_state_set_sub_menu_idx((uint8_t)(count - 1U));
    }
    screen_set_submenu_selection(ui_state_get_sub_menu_idx());
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
    ui_state_set_sub_item_count(count);
    ui_state_set_sub_menu_idx(0U);
    ui_state_set_sub_parent(UI_SETUP);
    ui_state_set_state(UI_SUB_MENU);
    ui_state_set_sub_history_depth(0U);
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
    ui_state_set_sub_item_count(count);
    ui_state_set_sub_menu_idx(0U);
    ui_state_set_state(UI_SUB_MENU);
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
    ui_state_set_sub_item_count(count);
    if (keep_idx >= count)
    {
        keep_idx = (uint8_t)(count - 1);
    }
    ui_state_set_sub_menu_idx(keep_idx);
    screen_set_submenu_selection(keep_idx);
}

void screen_handle_submenu_select(uint8_t item_idx)
{
    menu_action_t action;
    const menu_row_t *row;
    if (!s_submenu_list || !s_submenu_title) return;
    if (item_idx >= ui_state_get_sub_item_count()) return;
    row = menu_runtime_row_at(item_idx);
    if (!menu_actions_handle_select(item_idx, row, &action))
    {
        return;
    }
    /* 这里是子菜单层的“动作分发口”：
     * menu_runtime 决定当前有哪些行；
     * menu_actions 决定选中某一行后应该产生什么动作；
     * submenu_view 只负责把动作翻译成界面行为或状态切换。 */

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
            ui_state_set_sub_menu_idx(0U);
            screen_set_submenu_selection(ui_state_get_sub_menu_idx());
        }
        break;
    case MENU_ACTION_REFRESH:
        refresh_current_submenu_page(action.keep_index);
        break;
    case MENU_ACTION_SHOW_CONFIRM:
        screen_show_modal_setup_confirm(action.modal_text);
        ui_state_set_state(UI_MODAL_SETUP_CONFIRM);
        break;
    case MENU_ACTION_BEGIN_EDIT:
        screen_begin_edit_value(item_idx, &action.edit_spec);
        break;
    case MENU_ACTION_SHOW_GAS_MODAL:
        screen_show_modal_gas();
        ui_state_set_state(UI_MODAL_GAS);
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
        ui_state_set_sub_history_depth(0U);
        ui_state_set_sub_item_count(0U);
        ui_state_set_sub_menu_idx(0U);
        ui_state_set_edit_active(false);
        ui_state_set_gas_modal_from_submenu(false);
        ui_state_set_state(ui_state_get_sub_parent());
        return;
    }

    if (menu_runtime_back())
    {
        uint8_t count = 0;
        (void)menu_runtime_current_rows(&count);
        if (count > 0)
        {
            submenu_populate_current();
            ui_state_set_sub_item_count(count);
            if (ui_state_get_sub_menu_idx() >= count)
            {
                ui_state_set_sub_menu_idx((uint8_t)(count - 1U));
            }
            screen_set_submenu_selection(ui_state_get_sub_menu_idx());
        }
        ui_state_set_state(UI_SUB_MENU);
        return;
    }
    submenu_slide_out();
    menu_runtime_reset();
    menu_actions_clear_pending();
    ui_state_set_sub_history_depth(0U);
    ui_state_set_sub_item_count(0U);
    ui_state_set_sub_menu_idx(0U);
    ui_state_set_state(ui_state_get_sub_parent());
}

void screen_confirm_submenu_setting(void)
{
    bool close_extra_mode_layer = false;
    bool return_dash_after_apply = false;
    if (!menu_actions_confirm_pending(&close_extra_mode_layer, &return_dash_after_apply))
    {
        screen_hide_modal();
        ui_state_set_state(UI_SUB_MENU);
        return;
    }
    screen_hide_modal();
    if (return_dash_after_apply)
    {
        ui_state_set_sub_history_depth(0U);
        ui_state_set_edit_active(false);
        menu_runtime_reset();
        menu_actions_clear_pending();
        submenu_slide_out();
        ui_state_set_state(UI_DASH);
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
    ui_state_set_state(UI_SUB_MENU);
}
