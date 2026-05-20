#include "arex_ui_engine.h"
#include "fonts/arex_fonts.h"

#include <math.h>
#include <stdio.h>

/* ============================================================
 * 速率指示器图片资源（6级动态箭头）
 * ============================================================ */
LV_IMG_DECLARE(sudo_up_level0);
LV_IMG_DECLARE(sudo_up_level1);
LV_IMG_DECLARE(sudo_up_level2);
LV_IMG_DECLARE(sudo_down_level0);
LV_IMG_DECLARE(sudo_down_level1);
LV_IMG_DECLARE(sudo_down_level2);

/* ============================================================
 * 速率图标指针阵列（支持多DEPTH 模块同时存在
 * 最多支持屏幕上出现 MAX_ASCENT_ICONS 个深度模
 * (左侧锚点 1 + 5F 自定义网格多
 * ============================================================ */

/* ============================================================
 * NDL_STOP 多形态组件句柄（160x60 极限空间内的"变形金刚"
 * 支持屏幕上多NDL 模块（左侧锚1 + 5F 多个
 * 三种状 NDL常/ Safety停留 / Deco停留
 * ============================================================ */
lv_obj_t *s_img_ascent_rate[MAX_ASCENT_ICONS];
uint8_t  s_ascent_icon_count = 0;
ndl_handle_t s_ndl_handles[MAX_NDL_ICONS];
uint8_t      s_ndl_handle_count = 0;

static uint8_t arex_ui_clamp_battery_pct(float pct)
{
    if (pct <= 0.0f)
    {
        return 0U;
    }
    if (pct >= 100.0f)
    {
        return 100U;
    }
    return (uint8_t)pct;
}

/* =========================================================
 * POD 单模具轮转分配状态机
 *
 * 架构：WIDGET_POD_0806 (33) 是全局唯一真实存在的气瓶模具
 * APP 下发同一POD_0806 可以出现多次（如左侧锚点POD1+POD2，或 5F 中的多个）
 * MCU 通过渲染计数s_pod_render_count 自动分配身份
 *
 * 渲染时拦WIDGET_POD_0806，根据计数器判断
 *   - 次遇(count=1, 奇数) 分配POD1
 *   - 次遇(count=2, 偶数) 分配POD2
 *
 * user_data 烙印使用高位掩码区分
 *   - POD1: 1000 + WIDGET_POD_0806 = 1033
 *   - POD2: 2000 + WIDGET_POD_0806 = 2033
 * ========================================================= */
static uint8_t s_pod_render_count = 0;  /* POD 渲染计数*/

#define POD_TAG_BASE  1000  /* POD 标签基准偏移 */
#define POD1_TAG      (POD_TAG_BASE + WIDGET_POD_0806)  /* 1033 */
#define POD2_TAG      (2 * POD_TAG_BASE + WIDGET_POD_0806)  /* 2033 */

/* =========================================================
 * SYS 模块全局静态指针（O(1) 直接访问，零遍历
 * ========================================================= */
static lv_obj_t *s_sys_batt_lbl = NULL;      /* 电量百分*/
static lv_obj_t *s_sys_temp_lbl = NULL;      /* 温度 */
static lv_obj_t *s_sys_strobe_img = NULL;    /* 留转灯图*/
static lv_obj_t *s_sys_flash_img = NULL;     /* 手电筒图*/
static lv_obj_t *s_sys_cyl_lbl = NULL;      /* 气瓶数量文本 "x0" */

/* =========================================================
 * 获取 POD 标签（根据当前渲染计数器返回值）
 * 返回 POD1_TAG POD2_TAG，用于烙印到 user_data
 *
 * 注意：s_pod_render_count 已在 render_widget_by_id 中先递增
 * 所count=1 时为个POD，count=2 时为个POD
 * ========================================================= */
static uintptr_t arex_get_pod_tag(void)
{
    /* 次调count=1，奇 POD1_TAG
     * 次调count=2，偶 POD2_TAG */
    return (s_pod_render_count % 2 == 1) ? POD1_TAG : POD2_TAG;
}

/* =========================================================
 * 获取 POD 编号（返1 2
 * ========================================================= */
static uint8_t arex_get_pod_index(void)
{
    /* 次调count=1，奇 POD1
     * 次调count=2，偶 POD2 */
    return (s_pod_render_count % 2 == 1) ? 1 : 2;
}

/* =========================================================
 * 渲染计数器归零（每次网格重建/重绘前必须调用）
 * arex_screen_rebuild_layout() left_anchor_create() 调用
 * ========================================================= */
void arex_reset_widget_render_state(void)
{
    s_pod_render_count = 0;

    /* 归零底部 SystemData 静态句柄，防止 lv_timer 访问死内*/
    s_sys_batt_lbl     = NULL;
    s_sys_temp_lbl     = NULL;
    s_sys_strobe_img   = NULL;
    s_sys_flash_img    = NULL;
    s_sys_cyl_lbl      = NULL;
}

/* =========================================================
 * NDL 底部横向 10 宫格进度条绘制回(0 RAM)
 * 数学推演：容器宽abs_w - 16，两边各8px 边距
 * 10个块 + 9px间隙 = 137px（完美填满）
 * ========================================================= */
static void ndl_horiz_bar_draw_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    lv_draw_ctx_t * draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t * area = &obj->coords;

    int total_w = lv_area_get_width(area);
    int gap = 3;
    int block_w = (total_w - 9 * gap) / 10;
    if (block_w < 1) block_w = 1;

    /* 计算总体百分比：
     * - 常态：NDL/99 显示9 视为满格
     * - 安全停留：未进站前仍NDL；进站后按停留剩余时间缩
     * - 减压停留：未进站前保持满格；进站后按当前减压站剩余时间缩*/
    float pct = 0.0f;
    if (g_sensor_data.stop_type == AREX_STOP_NONE)
    {
        pct = (float)g_sensor_data.ndl / 99.0f;
    }
    else if (g_sensor_data.stop_type == AREX_STOP_SAFETY)
    {
        if (!g_sensor_data.in_stop_zone)
        {
            pct = (float)g_sensor_data.ndl / 99.0f;
        }
        else if (g_sensor_data.stop_time_total_s > 0)
        {
            pct = (float)g_sensor_data.stop_time_left_s / g_sensor_data.stop_time_total_s;
        }
        else
        {
            pct = 1.0f;
        }
    }
    else if (g_sensor_data.stop_type == AREX_STOP_DECO)
    {
        if (!g_sensor_data.in_stop_zone)
        {
            pct = 1.0f;
        }
        else if (g_sensor_data.stop_time_total_s > 0)
        {
            pct = (float)g_sensor_data.stop_time_left_s / g_sensor_data.stop_time_total_s;
        }
    }
    if (pct > 1.0f) pct = 1.0f;
    if (pct < 0.0f) pct = 0.0f;

    int active_blocks = (int)(pct * 10.0f);
    float remainder = (pct * 10.0f) - active_blocks;

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.radius = 0; /* 纯直*/

    for (int i = 0; i < 10; i++)
    {
        int x1 = area->x1 + i * (block_w + gap);
        int x2 = x1 + block_w - 1;
        lv_area_t block_area = {x1, area->y1, x2, area->y2};

        if (i < active_blocks)
        {
            /* 全亮格子 */
            rect_dsc.bg_color = AREX_GREEN;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_draw_rect(draw_ctx, &rect_dsc, &block_area);
        }
        else if (i == active_blocks && remainder > 0.05f)
        {
            /* 半亮格子 (先画暗底，再盖亮 */
            rect_dsc.bg_color = AREX_DARK;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_draw_rect(draw_ctx, &rect_dsc, &block_area);

            int partial_w = (int)(block_w * remainder);
            if (partial_w > 0)
            {
                lv_area_t partial_area = {x1, area->y1, x1 + partial_w - 1, area->y2};
                rect_dsc.bg_color = AREX_GREEN;
                lv_draw_rect(draw_ctx, &rect_dsc, &partial_area);
            }
        }
        else
        {
            /* 未激活的暗格 */
            rect_dsc.bg_color = AREX_DARK;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_draw_rect(draw_ctx, &rect_dsc, &block_area);
        }
    }
}

/* =========================================================
 * 创建单个自定义组件（组件工厂 左侧网格 + 5F 共用
 *
 * 关键：每个组件的 lv_obj_set_user_data() 存储了标签烙印
 * 对于 POD，使用高位掩码区分（1033=POD1, 2033=POD2）
 * 告警引擎靠这个烙印实左侧锚点 + 5F 组件同时闪烁"
 *
 * 架构铁律
 *   - 位置参数 (abs_x/y/w/h, span_w/h) 由调用方传入
 *   - 样式参数 (font, offsets) arex_get_widget_style(w_id) 自动查表
 *   - cfg_font_id != 255 时强制覆盖自动字
 *   - 速率图标由工厂自主查字典决定（根elements & ELEM_BAR
 *   - 专属组件（DEPTH/NDL）走早期返回，内部仍style 参数
 *   - 通用组件elements 掩码装配流水线：TITLE VALUE UNIT BAR
 *
 * POD 单模具轮转分配：
 *   - 函数入口检w_id == WIDGET_POD_0806
 *   - 调用 arex_get_pod_tag() 获得高位掩码标签 (1033/2033)
 *   - 调用 arex_get_pod_index() 获得 POD 编号 (1/2)
 *   - 将标签烙印到容器 user_data
 * ========================================================= */
lv_obj_t *render_widget_by_id(lv_obj_t *parent,
                              arex_widget_id_t w_id,
                              int16_t abs_x, int16_t abs_y,
                              uint16_t abs_w, uint16_t abs_h,
                              uint8_t span_w, uint8_t span_h,
                              arex_font_id_t cfg_font_id)
{
    /* ===== POD 单模具拦截：提前消耗计数器 ===== */
    bool is_pod_mold = (w_id == WIDGET_POD_0806);
    uint8_t pod_index = 0;        /* POD number 1 or 2 */
    uintptr_t pod_tag = 0;        /* POD tag 1033 or 2033 */
    if (is_pod_mold)
    {
        s_pod_render_count++;     /* Increment first, then get current value */
        pod_index = arex_get_pod_index();
        pod_tag = arex_get_pod_tag();
    }

    const arex_widget_style_t *style = arex_get_widget_style(w_id);
    if (!style) return NULL;

    /* 字号选择逻辑
     *   cfg_font_id != 255 强制覆盖（运行时指定
     *   DEPTH 系列 自动适配尺寸（HUGE/MEDIUM/SMALL
     *   其他组件 直接使用字典 font_id */
    arex_font_id_t val_font_id;
    if (cfg_font_id != (arex_font_id_t)255)
    {
        val_font_id = cfg_font_id;  /* 强制覆盖（运行时指定*/
    }
    else if (w_id == WIDGET_DEPTH_1612 || w_id == WIDGET_DEPTH_1606)
    {
        /* DEPTH 组件：自动适配尺寸 */
        if (span_w >= 2 && span_h >= 2)
        {
            val_font_id = AREX_FONT_ID_HUGE;
        }
        else if (span_w >= 2)
        {
            val_font_id = AREX_FONT_ID_MEDIUM;
        }
        else
        {
            val_font_id = AREX_FONT_ID_SMALL;
        }
    }
    else
    {
        /* 其他组件：直接使用字font_id */
        val_font_id = style->font_id;
    }

    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, abs_x, abs_y);
    lv_obj_set_size(obj, abs_w, abs_h);
    lv_obj_set_style_bg_color(obj, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, AREX_DARK, 0);
    lv_obj_set_style_border_width(obj, AREX_DEBUG_BORDERS ? 1 : 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 2, 0);

    /* 封杀所有滚动条 */
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);

    /* ===== 靶向告警烙印 =====
     * POD uses high-bit mask tags (1033/2033), others use raw w_id */
    if (is_pod_mold)
    {
        lv_obj_set_user_data(obj, (void *)pod_tag);
    }
    else
    {
        lv_obj_set_user_data(obj, (void *)(uintptr_t)w_id);
    }

    if (w_id == WIDGET_EMPTY) return obj;

    /* ===== DEPTH 2x2 专属渲染（整小数+单位分离===== */
    bool is_2x2 = (span_w >= 2 && span_h >= 2);
    if (w_id == WIDGET_DEPTH_1612 && is_2x2)
    {
        /* 样式参数来自 arex_widget_style_t */
        const arex_style_depth_t *s = &style->spec.depth;

        /* ==========================================
         * 1. 超大号整-> 宽度必须紧密包裹
         * ========================================== */
        lv_obj_t *int_lbl = lv_label_create(obj);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT) lv_label_set_text(int_lbl, "--");
        else lv_label_set_text_fmt(int_lbl, "%d", (int)g_sensor_data.depth);
        // 字体从字典读取（font_id = HUGE 58px
        lv_obj_set_style_text_font(int_lbl, arex_get_font(style->font_id), 0);
        lv_obj_set_style_text_color(int_lbl, AREX_GREEN, 0);

        // 绝杀技：必须设CONTENT！这样无论变"6" 还是 "45"
        // Label 的右边缘都会死死包住个位数，绝不留一丝缝隙！
        lv_obj_set_size(int_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

        // 读取字典中的 RIGHT_MID -45，把右边缘焊死在这堵墙上
        lv_obj_align(int_lbl, (lv_align_t)s->int_align, s->int_offset_x, s->int_offset_y);

        /* ==========================================
         * 2. 中号小数 -> 紧贴整数的右边界
         * ========================================== */
        lv_obj_t *dec_lbl = lv_label_create(obj);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT) lv_label_set_text(dec_lbl, ".-");
        else
        {
            /* 提取小数部分：只保留一位小数，范围 0-9 */
            float decimal_part = fabsf(g_sensor_data.depth - (int)g_sensor_data.depth);
            int dd = (int)(decimal_part * 10 + 0.5f);
            if (dd > 9) dd = 9;  /* 防止浮点精度问题导致多位*/
            lv_label_set_text_fmt(dec_lbl, ".%d", dd);
        }
        // 字体从字典读取（title_font_id = MEDIUM 28px，小数比整数小）
        lv_obj_set_style_text_font(dec_lbl, arex_get_font(style->title_font_id), 0);
        lv_obj_set_style_text_color(dec_lbl, AREX_GREEN, 0);
        lv_obj_set_size(dec_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

        // 因为整数的右边缘(个位被焊死了，小数挂在它右边，自然就永远贴紧个位数！
        lv_obj_align_to(dec_lbl, int_lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, s->dec_offset_x, s->dec_offset_y);

        /* ==========================================
         * 3. 小号单位 (m) -> 紧贴小数正下
         * ========================================== */
        if (style->elements & ELEM_UNIT)
        {
            lv_obj_t *unit_lbl = lv_label_create(obj);
            lv_label_set_text(unit_lbl, style->unit ? style->unit : "");
            // 单位固定用小号字
            lv_obj_set_style_text_font(unit_lbl, arex_get_font(AREX_FONT_ID_SMALL), 0);
            lv_obj_set_style_text_color(unit_lbl, AREX_LIGHT, 0);
            lv_obj_set_size(unit_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align_to(unit_lbl, dec_lbl, LV_ALIGN_OUT_BOTTOM_MID, s->unit_offset_x, s->unit_offset_y);
        }

        /* 速率图标：工厂自主查字典判断是否需要绘*/
        bool needs_bar_icon = (style->elements & ELEM_BAR) != 0;
        if (needs_bar_icon)
        {
            lv_obj_t *sudu_img = lv_img_create(obj);
            lv_img_set_src(sudu_img, &sudo_up_level0);
            lv_obj_align(sudu_img, (lv_align_t)s->icon_align, s->icon_offset_x, s->icon_offset_y);
            if (s_ascent_icon_count < MAX_ASCENT_ICONS)
                s_img_ascent_rate[s_ascent_icon_count++] = sudu_img;
        }
        return obj;
    }
    else if (w_id == WIDGET_NDL_STOP_1606)
    {
        /* NDL 变形金刚：从 style->spec.ndl_stop 读取所有位置参*/
        if (s_ndl_handle_count >= MAX_NDL_ICONS) return obj;
        ndl_handle_t *h = &s_ndl_handles[s_ndl_handle_count++];
        h->comp = obj;

        const arex_style_ndl_stop_t *s = &style->spec.ndl_stop;

        /* 创建 10 宫格的底层透明画板 */
        h->horiz_bg = lv_obj_create(obj);
        lv_obj_remove_style_all(h->horiz_bg);
        /* 🚨 宽度填满减去两边留白：abs_w - 16，两边各8px */
        lv_obj_set_size(h->horiz_bg, abs_w - 16, 10);
        /* 贴紧底部，略微上4px */
        lv_obj_align(h->horiz_bg, LV_ALIGN_BOTTOM_MID, 0, -4);
        lv_obj_add_event_cb(h->horiz_bg, ndl_horiz_bar_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
        lv_obj_add_flag(h->horiz_bg, LV_OBJ_FLAG_HIDDEN);

        /* 顶部标题（默认隐藏，停留态时显示*/
        h->title_top = lv_label_create(obj);
        lv_obj_set_style_text_font(h->title_top, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(h->title_top, AREX_GREEN, 0);
        lv_label_set_text(h->title_top, "");
        lv_obj_add_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);

        /* 主数(22, 3:00) - 使用48px字体 */
        h->main_val = lv_label_create(obj);
        lv_obj_set_style_text_color(h->main_val, AREX_GREEN, 0);
        lv_obj_set_style_text_font(h->main_val, arex_get_font(AREX_FONT_ID_NDL), 0);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(h->main_val, "--");
        else
            lv_label_set_text_fmt(h->main_val, "%d", g_sensor_data.ndl);

        /* 底部标题 (NDL 45) */
        h->sub_bot = lv_label_create(obj);
        lv_obj_set_style_text_font(h->sub_bot, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(h->sub_bot, AREX_GREEN, 0);
        lv_obj_add_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN);
        return obj;
    }
    else if (w_id == WIDGET_SYS_1606)
    {
        /* ===== SYS 模块：电+ 温度横向排列 ===== */

        /* 左侧：电Label */
        s_sys_batt_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(s_sys_batt_lbl, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
        lv_obj_set_style_text_color(s_sys_batt_lbl, AREX_GREEN, 0);
        lv_obj_align(s_sys_batt_lbl, LV_ALIGN_LEFT_MID, 4, 0);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(s_sys_batt_lbl, "--%");
        else
            lv_label_set_text_fmt(s_sys_batt_lbl, "%u%%", arex_ui_clamp_battery_pct(g_sensor_data.battery_pct));

        /* 右侧：温Label */
        s_sys_temp_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(s_sys_temp_lbl, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
        lv_obj_set_style_text_color(s_sys_temp_lbl, AREX_GREEN, 0);
        lv_obj_align(s_sys_temp_lbl, LV_ALIGN_RIGHT_MID, -4, 0);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(s_sys_temp_lbl, "-- C");
        else
        {
            int t_int = (int)g_sensor_data.temperature_c;
            int t_dec = (int)(fabsf(g_sensor_data.temperature_c - t_int) * 10);
            lv_label_set_text_fmt(s_sys_temp_lbl, "%d.%d C", t_int, t_dec);
        }

        return obj;
    }

    /* ===== 通用流水线：elements 掩码按需装配零件 =====
     * POD1/POD2/WTIME 及所1x1/2x1 通用组件走此路径
     * ELEM_TITLE ELEM_VALUE ELEM_UNIT ELEM_BAR
     *
     * 样式参数全部来自 arex_get_widget_style(w_id) 查表结果
     * title 文本和数值数据源依赖 w_id switch 分发 */

    /* --- 零件 1：标--- */
    if ((style->elements & ELEM_TITLE) && style->title)
    {
        lv_obj_t *title_lbl = lv_label_create(obj);
        /* POD 单模具：根据 pod_index 动态决定标题文*/
        if (is_pod_mold)
        {
            lv_label_set_text_fmt(title_lbl, "POD %d", pod_index);
        }
        else
        {
            lv_label_set_text(title_lbl, style->title);
        }
        lv_obj_set_style_text_font(title_lbl, arex_get_font(style->title_font_id), 0);
        lv_obj_set_style_text_color(title_lbl, AREX_LIGHT, 0);
        lv_obj_set_size(title_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(title_lbl, (lv_align_t)style->title_align,
                     style->title_offset_x, style->title_offset_y);
        lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
    }

    /* --- 零件 2：主数--- */
    lv_obj_t *val_lbl = NULL;
    if (style->elements & ELEM_VALUE)
    {
        val_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(val_lbl, arex_get_font(val_font_id), 0);
        lv_obj_set_style_text_color(val_lbl, AREX_GREEN, 0);

        if (AREX_SHOW_PLACEHOLDER_ON_INIT)
        {
            /* 通用占位*/
            lv_label_set_text(val_lbl, "--");
        }
        else
        {
            char buf[48] = "--";
            switch (w_id)
            {
            case WIDGET_DEPTH_1612:
            case WIDGET_DEPTH_1606:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.depth);
                break;
            case WIDGET_NDL_STOP_1606:
                snprintf(buf, sizeof(buf), "%d", g_sensor_data.ndl_stop_value);
                break;
            case WIDGET_DIVE_TIME_1606:
                snprintf(buf, sizeof(buf), "%02d:%02d", g_sensor_data.dive_time_s/60, g_sensor_data.dive_time_s%60);
                break;
            case WIDGET_GAS_1606:
                snprintf(buf, sizeof(buf), "%s", g_sensor_data.gas_name);
                break;
            case WIDGET_SYS_1606:
                snprintf(buf, sizeof(buf), "%02d:%02d", g_sensor_data.sys_time_h, g_sensor_data.sys_time_m);
                break;
            case WIDGET_TEMP_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.temperature_c);
                break;
            case WIDGET_TIME_1606:
                snprintf(buf, sizeof(buf), "%02d:%02d", g_sensor_data.sys_time_h, g_sensor_data.sys_time_m);
                break;
            case WIDGET_TTS_0806:
                snprintf(buf, sizeof(buf), "%d", g_sensor_data.tts);
                break;
            case WIDGET_ASCENT_0806:
            case WIDGET_ASCENT_0812:
                snprintf(buf, sizeof(buf), "%+.1f", (double)g_sensor_data.ascent_rate);
                break;
            case WIDGET_COMPASS_1612:
                snprintf(buf, sizeof(buf), "%03d", g_sensor_data.heading);
                break;
            case WIDGET_BATTERY_0806:
                snprintf(buf, sizeof(buf), "%u", arex_ui_clamp_battery_pct(g_sensor_data.battery_pct));
                break;
            case WIDGET_STOP_DEPTH_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.stop_depth_m);
                break;
            case WIDGET_STOP_TIME_1606:
                snprintf(buf, sizeof(buf), "%d", g_sensor_data.stop_time_left_s);
                break;
            case WIDGET_PPO2_0806:
                snprintf(buf, sizeof(buf), "%.2f", (double)g_sensor_data.ppo2[g_sensor_data.gas_active_idx]);
                break;
            case WIDGET_SURF_GF_0806:
                snprintf(buf, sizeof(buf), "%.2f", (double)g_sensor_data.surf_gf);
                break;
            case WIDGET_GF99_0806:
                snprintf(buf, sizeof(buf), "%.0f", (double)g_sensor_data.gf99);
                break;
            case WIDGET_CNS_0806:
                snprintf(buf, sizeof(buf), "%d", g_sensor_data.cns_pct);
                break;
            case WIDGET_OTU_0806:
                snprintf(buf, sizeof(buf), "%d", g_sensor_data.otu);
                break;
            case WIDGET_GF_0806:
                snprintf(buf, sizeof(buf), "%d/%d", g_sensor_data.gf_low, g_sensor_data.gf_high);
                break;
            case WIDGET_MOD_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.mod_m);
                break;
            case WIDGET_CEILING_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.ceiling_m);
                break;
            case WIDGET_GAS_MIX_1606:
                snprintf(buf, sizeof(buf), "%d/%d", g_sensor_data.gas_o2_pct, g_sensor_data.gas_he_pct);
                break;
            case WIDGET_GAS_DENS_0806:
                snprintf(buf, sizeof(buf), "%.2f", (double)g_sensor_data.gas_density);
                break;
            case WIDGET_FIO2_0806:
                snprintf(buf, sizeof(buf), "%.0f%%", (double)g_sensor_data.fio2_pct);
                break;
            case WIDGET_HEADING_0806:
                snprintf(buf, sizeof(buf), "%03d", g_sensor_data.heading);
                break;
            /* ===== POD 单模具：数据源根pod_index 动态分===== */
            case WIDGET_POD_0806:
                if (is_pod_mold)
                {
                    if (pod_index == 1)
                    {
                        snprintf(buf, sizeof(buf), "%.0f", (double)g_sensor_data.pod1_bar);
                    }
                    else
                    {
                        snprintf(buf, sizeof(buf), "%.0f", (double)g_sensor_data.pod2_bar);
                    }
                }
                else
                {
                    snprintf(buf, sizeof(buf), "--");
                }
                break;
            case WIDGET_DEPTH_MAX_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.max_depth);
                break;
            case WIDGET_DEPTH_AVG_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.avg_depth);
                break;
            case WIDGET_TEMP_MIN_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.min_temp);
                break;
            case WIDGET_TEMP_AVG_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.avg_temp);
                break;
            /* 🚨 以下已废弃，Protobuf 已移除对ID
            case WIDGET_WTIME_0806: {
                uint32_t t = g_sensor_data.surface_time_s;
                snprintf(buf, sizeof(buf), "%02d:%02d", t / 60, t % 60);
            break;
            }
            case WIDGET_TEMP_MAX_0806: snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.max_temp); break;
            case WIDGET_SAC_RATE_0806:  snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.sac_rate); break;
            case WIDGET_PPO2_SAFE_0806: snprintf(buf, sizeof(buf), "%.2f", 1.4); break;
            case WIDGET_NDL_SAFE_0806:  snprintf(buf, sizeof(buf), "%d", 5); break;
            case WIDGET_SAC_SAFE_0806:  snprintf(buf, sizeof(buf), "%.1f", 25.0); break;
            */
            default:
                snprintf(buf, sizeof(buf), "--");
                break;
            }
            lv_label_set_text(val_lbl, buf);
        }
        /* 所有使ELEM_VALUE widget 都使spec.basic.value_align */
        lv_obj_align(val_lbl, (lv_align_t)style->spec.basic.value_align,
                     style->spec.basic.value_offset_x, style->spec.basic.value_offset_y);
        lv_obj_set_user_data(val_lbl, (void *)(uintptr_t)w_id);
    }

    /* --- 零件 3：单--- */
    if ((style->elements & ELEM_UNIT) && style->unit)
    {
        lv_obj_t *unit_lbl = lv_label_create(obj);
        lv_label_set_text(unit_lbl, style->unit);
        lv_obj_set_style_text_font(unit_lbl, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(unit_lbl, AREX_LIGHT, 0);
        /* 单位位于数值右侧（对于 2x1 等窄组件*/
        if ((style->elements & ELEM_VALUE) && (val_lbl != NULL))
        {
            /* 挂在数label 右侧 */
            lv_obj_align_to(unit_lbl, val_lbl, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
        }
        else
        {
            lv_obj_align(unit_lbl, (lv_align_t)style->title_align,
                         style->title_offset_x, style->title_offset_y);
        }
    }

    /* --- 零件 4：特BAR --- */
    if (style->elements & ELEM_BAR)
    {
        if (w_id == WIDGET_DEPTH_1612)
        {
            const arex_style_depth_t *s = &style->spec.depth;
            lv_obj_t *sudu_img = lv_img_create(obj);
            lv_img_set_src(sudu_img, &sudo_up_level0);
            lv_obj_align(sudu_img, (lv_align_t)s->icon_align, s->icon_offset_x, s->icon_offset_y);
            if (s_ascent_icon_count < MAX_ASCENT_ICONS)
                s_img_ascent_rate[s_ascent_icon_count++] = sudu_img;
        }
        else if (w_id == WIDGET_ASCENT_0812)
        {
            /* ASCENT_0812 (1x2)：绘制上升速率方向箭头图标（工厂自主查字典决定*/
            lv_obj_t *sudu_img = lv_img_create(obj);
            lv_img_set_src(sudu_img, &sudo_up_level0);
            lv_obj_align(sudu_img, LV_ALIGN_CENTER, 0, 0);
            if (s_ascent_icon_count < MAX_ASCENT_ICONS)
                s_img_ascent_rate[s_ascent_icon_count++] = sudu_img;
        }
        else if (w_id == WIDGET_COMPASS_1612)
        {
            /* COMPASS_1612 (2x2)：卷tape 在早期分支里，ELEM_BAR 标记spec.compass 驱动 */
        }
        else if (w_id == WIDGET_TISSUE_GF_4012 || w_id == WIDGET_TISSUE_RAW_4012)
        {
            /* TISSUE (4x2)6 柱组织图，ELEM_BAR 标记spec.tissue 驱动 */
        }
        else if (w_id == WIDGET_SYS_1606)
        {
            /* SYS 电池+ 外设图标（系统状态栏*/
            lv_obj_t *bat_bg = lv_obj_create(obj);
            lv_obj_remove_style_all(bat_bg);
            lv_obj_set_size(bat_bg, 60, 14);
            lv_obj_align(bat_bg, LV_ALIGN_BOTTOM_LEFT, 4, -4);
            lv_obj_set_style_border_width(bat_bg, 1, 0);
            lv_obj_set_style_border_color(bat_bg, AREX_GREEN, 0);
            lv_obj_set_style_radius(bat_bg, 2, 0);

            uint8_t pct = arex_ui_clamp_battery_pct(g_sensor_data.battery_pct);
            lv_obj_t *bat_fill = lv_obj_create(bat_bg);
            lv_obj_remove_style_all(bat_fill);
            lv_obj_set_size(bat_fill, LV_PCT(pct > 20 ? 100 : pct), LV_PCT(100));
            lv_obj_align(bat_fill, LV_ALIGN_LEFT_MID, 0, 0);
            lv_obj_set_style_bg_color(bat_fill, pct > 20 ? AREX_GREEN : AREX_LIGHT, 0);
            lv_obj_set_style_bg_opa(bat_fill, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(bat_fill, 1, 0);
            (void)bat_fill;
        }
    }

    return obj;
}

void arex_widget_refresh_sys(uint32_t dirty_mask)
{
    if ((dirty_mask & DIRTY_BATT) && s_sys_batt_lbl)
    {
        lv_label_set_text_fmt(s_sys_batt_lbl, "%u%%", arex_ui_clamp_battery_pct(g_sensor_data.battery_pct));
    }
    if ((dirty_mask & DIRTY_TEMP) && s_sys_temp_lbl)
    {
        int t_int = (int)g_sensor_data.temperature_c;
        int t_dec = (int)(fabsf(g_sensor_data.temperature_c - t_int) * 10);
        lv_label_set_text_fmt(s_sys_temp_lbl, "%d.%d C", t_int, t_dec);
    }
}

void arex_widget_refresh_ascent_icons(float rate)
{
    static int8_t s_last_direction = 0;  /* 0=still, 1=up, -1=down */

    if (s_ascent_icon_count == 0)
    {
        return;
    }

    bool is_moving = (fabsf(rate) >= AREX_RATE_STILL_THRESHOLD);
    bool current_flash_state = (lv_tick_get() / 500) % 2 == 0;

    int8_t current_direction = 0;
    if (rate > 0.0f)
    {
        current_direction = 1;
    }
    else if (rate < 0.0f)
    {
        current_direction = -1;
    }

    const void *target_img_src = &sudo_up_level0;

    if (!is_moving)
    {
        target_img_src = (s_last_direction > 0) ? &sudo_up_level0 :
                         (s_last_direction < 0) ? &sudo_down_level0 : &sudo_up_level0;
    }
    else if (current_direction > 0)
    {
        if (rate >= AREX_RATE_LEVEL2_THRESHOLD)
        {
            target_img_src = current_flash_state ? &sudo_up_level2 : &sudo_up_level0;
        }
        else if (rate >= AREX_RATE_LEVEL1_THRESHOLD)
        {
            target_img_src = current_flash_state ? &sudo_up_level1 : &sudo_up_level0;
        }
        else
        {
            target_img_src = &sudo_up_level0;
        }
    }
    else
    {
        if (rate <= -AREX_RATE_LEVEL2_THRESHOLD)
        {
            target_img_src = current_flash_state ? &sudo_down_level2 : &sudo_down_level0;
        }
        else if (rate <= -AREX_RATE_LEVEL1_THRESHOLD)
        {
            target_img_src = current_flash_state ? &sudo_down_level1 : &sudo_down_level0;
        }
        else
        {
            target_img_src = &sudo_down_level0;
        }
    }

    if (current_direction != 0)
    {
        s_last_direction = current_direction;
    }

    for (int i = 0; i < s_ascent_icon_count; i++)
    {
        if (s_img_ascent_rate[i] != NULL)
        {
            lv_img_set_src(s_img_ascent_rate[i], target_img_src);
        }
    }
}
