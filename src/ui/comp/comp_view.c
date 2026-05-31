/*
 * 文件: src/app_ui/ui/comp/comp_view.c
 * 作用: 该文件属于公共组件模块，负责复用样式、通用控件、局部刷新逻辑或组件级显示封装。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时应重点检查布局尺寸、刷新频率与复用接口是否一致，避免局部样式或坐标调整影响同类页面的对齐和更新节奏。
 */

#include "../core/ui_engine.h"
#include "../core/data.h"
#include "../core/vm/ui_vm_dashboard.h"
#include "comp_view.h"
#include "comp_style.h"
#include "../fonts/fonts.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ============================================================
 * 速率指示器图片资源（6级动态箭头）
 * ============================================================ */
LV_IMG_DECLARE(sudo_up_level0);
LV_IMG_DECLARE(sudo_up_level1);
LV_IMG_DECLARE(sudo_up_level2);
LV_IMG_DECLARE(sudo_down_level0);
LV_IMG_DECLARE(sudo_down_level1);
LV_IMG_DECLARE(sudo_down_level2);

#define MAX_ASCENT_ICONS  12
#define MAX_NDL_ICONS     4
#define MAX_TISSUE_WIDGETS 4

typedef struct
{
    lv_obj_t *comp;
    lv_obj_t *horiz_bg;
    lv_obj_t *main_val;
    lv_obj_t *title_top;
    lv_obj_t *sub_bot;
} ndl_handle_t;

typedef struct
{
    lv_obj_t *chart;
    lv_obj_t *placeholder;
    comp_id_t widget_id;
    ui_vm_deco_t vm;
} tissue_handle_t;

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
static lv_obj_t *s_img_ascent_rate[MAX_ASCENT_ICONS];
static uint8_t  s_ascent_icon_count = 0;
static ndl_handle_t s_ndl_handles[MAX_NDL_ICONS];
static uint8_t      s_ndl_handle_count = 0;
static ui_vm_ndl_stop_t s_ndl_draw_vm[MAX_NDL_ICONS];
static tissue_handle_t s_tissue_handles[MAX_TISSUE_WIDGETS];
static uint8_t s_tissue_handle_count;

static uint8_t ui_battery_draw_pct(float pct)
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

static int16_t comp_title_edge_offset_x(lv_align_t align, int16_t offset_x)
{
    switch (align)
    {
    case LV_ALIGN_TOP_LEFT:
    case LV_ALIGN_LEFT_MID:
    case LV_ALIGN_BOTTOM_LEFT:
        return (int16_t)(offset_x - COMP_TITLE_EDGE_NUDGE_PX);

    case LV_ALIGN_TOP_RIGHT:
    case LV_ALIGN_RIGHT_MID:
    case LV_ALIGN_BOTTOM_RIGHT:
        return (int16_t)(offset_x + COMP_TITLE_EDGE_NUDGE_PX);

    default:
        return offset_x;
    }
}

/* =========================================================
 * POD 单模具轮转分配状态机
 *
 * 架构：COMP_POD_0806 (33) 是全局唯一真实存在的气瓶模具
 * APP 下发同一POD_0806 可以出现多次（如左侧锚点POD1+POD2，或 5F 中的多个）
 * MCU 通过渲染计数s_pod_render_count 自动分配身份
 *
 * 渲染时拦COMP_POD_0806，根据计数器判断
 *   - 次遇(count=1, 奇数) 分配POD1
 *   - 次遇(count=2, 偶数) 分配POD2
 *
 * user_data 烙印使用高位掩码区分
 *   - POD1: 1000 + COMP_POD_0806 = 1033
 *   - POD2: 2000 + COMP_POD_0806 = 2033
 * ========================================================= */
static uint8_t s_pod_render_count = 0;  /* POD 渲染计数*/

#define POD_TAG_BASE  1000  /* POD 标签基准偏移 */
#define POD1_TAG      (POD_TAG_BASE + COMP_POD_0806)  /* 1033 */
#define POD2_TAG      (2 * POD_TAG_BASE + COMP_POD_0806)  /* 2033 */

/* =========================================================
 * SYS 模块全局静态指针（O(1) 直接访问，零遍历
 * ========================================================= */
static lv_obj_t *s_sys_batt_lbl = NULL;      /* 电量百分*/
static lv_obj_t *s_sys_temp_lbl = NULL;      /* 温度 */
static lv_obj_t *s_sys_strobe_img = NULL;    /* 留转灯图*/
static lv_obj_t *s_sys_flash_img = NULL;     /* 手电筒图*/
static lv_obj_t *s_sys_cyl_lbl = NULL;      /* 气瓶数量文本 "x0" */

static bool ui_obj_is_valid(lv_obj_t **obj_ref)
{
    /* 如果对象已经被 LVGL 销毁，就立刻把缓存指针清空。 */
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

/* =========================================================
 * 获取 POD 标签（根据当前渲染计数器返回值）
 * 返回 POD1_TAG POD2_TAG，用于烙印到 user_data
 *
 * 注意：s_pod_render_count 已在 render_widget_by_id 中先递增
 * 所count=1 时为个POD，count=2 时为个POD
 * ========================================================= */
static uintptr_t get_pod_tag(void)
{
    /* 奇数次渲染视为 POD1，偶数次渲染视为 POD2。 */
    /* 次调count=1，奇 POD1_TAG
     * 次调count=2，偶 POD2_TAG */
    return (s_pod_render_count % 2 == 1) ? POD1_TAG : POD2_TAG;
}

/* =========================================================
 * 获取 POD 编号（返1 2
 * ========================================================= */
static uint8_t get_pod_index(void)
{
    /* 与 POD tag 同步，返回逻辑上的 1 号或 2 号气瓶。 */
    /* 次调count=1，奇 POD1
     * 次调count=2，偶 POD2 */
    return (s_pod_render_count % 2 == 1) ? 1 : 2;
}

/* =========================================================
 * 渲染计数器归零（每次网格重建/重绘前必须调用）
 * screen_rebuild_layout() left_anchor_create() 调用
 * ========================================================= */
void reset_widget_render_state(void)
{
    /* 布局重建前必须把所有缓存句柄和轮转计数器清空。 */
    memset(s_img_ascent_rate, 0, sizeof(s_img_ascent_rate));
    s_ascent_icon_count = 0;
    memset(s_ndl_handles, 0, sizeof(s_ndl_handles));
    s_ndl_handle_count = 0;
    memset(s_ndl_draw_vm, 0, sizeof(s_ndl_draw_vm));
    memset(s_tissue_handles, 0, sizeof(s_tissue_handles));
    s_tissue_handle_count = 0;

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
    /* 这个回调负责按当前 VM 进度绘制 NDL/停留进度条。 */
    lv_obj_t * obj = lv_event_get_target(e);
    lv_draw_ctx_t * draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t * area = &obj->coords;
    const ui_vm_ndl_stop_t *vm = (const ui_vm_ndl_stop_t *)lv_event_get_user_data(e);

    int total_w = lv_area_get_width(area);
    int gap = 3;
    int block_w = (total_w - 9 * gap) / 10;
    if (block_w < 1) block_w = 1;

    float pct = 0.0f;
    if (vm != NULL)
    {
        if (vm->stop_type == STOP_NONE)
        {
            if (vm->ndl_bar_pct <= 100U)
            {
                pct = (float)vm->ndl_bar_pct / 100.0f;
            }
            else
            {
                pct = (float)vm->ndl / 99.0f;
            }
        }
        else if (vm->stop_type == STOP_SAFETY)
        {
            if (vm->stop_time_total_s > 0U)
            {
                pct = (float)vm->stop_time_left_s / (float)vm->stop_time_total_s;
            }
            else
            {
                pct = 1.0f;
            }
        }
        else if (vm->stop_type == STOP_DECO)
        {
            if (vm->in_stop_zone == 0U)
            {
                pct = 1.0f;
            }
            else if (vm->stop_time_total_s > 0U)
            {
                pct = (float)vm->stop_time_left_s / (float)vm->stop_time_total_s;
            }
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
            rect_dsc.bg_color = GREEN;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_draw_rect(draw_ctx, &rect_dsc, &block_area);
        }
        else if (i == active_blocks && remainder > 0.05f)
        {
            /* 半亮格子 (先画暗底，再盖亮 */
            rect_dsc.bg_color = DARK;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_draw_rect(draw_ctx, &rect_dsc, &block_area);

            int partial_w = (int)(block_w * remainder);
            if (partial_w > 0)
            {
                lv_area_t partial_area = {x1, area->y1, x1 + partial_w - 1, area->y2};
                rect_dsc.bg_color = GREEN;
                lv_draw_rect(draw_ctx, &rect_dsc, &partial_area);
            }
        }
        else
        {
            /* 未激活的暗格 */
            rect_dsc.bg_color = DARK;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_draw_rect(draw_ctx, &rect_dsc, &block_area);
        }
    }
}

static void tissue_chart_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t *area = &obj->coords;
    const tissue_handle_t *h = (const tissue_handle_t *)lv_event_get_user_data(e);
    const ui_vm_deco_t *vm = (h != NULL) ? &h->vm : NULL;

    if (vm == NULL)
    {
        return;
    }

    int total_w = lv_area_get_width(area);
    int total_h = lv_area_get_height(area);
    int gap = 3;
    int bar_w = (total_w - gap * 15) / 16;
    if (bar_w < 2)
    {
        bar_w = 2;
        gap = 2;
    }

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.radius = 0;

    for (uint8_t i = 0U; i < 16U; i++)
    {
        uint8_t pct = (h->widget_id == COMP_TISSUE_RAW_4012) ?
                      vm->tissue_raw_pct[i] :
                      vm->tissue_gf_pct[i];
        if (pct > 100U)
        {
            pct = 100U;
        }

        int x1 = area->x1 + (int)i * (bar_w + gap);
        int x2 = x1 + bar_w - 1;
        lv_area_t bg = { x1, area->y1, x2, area->y2 };

        rect_dsc.bg_color = DARK;
        rect_dsc.bg_opa = LV_OPA_COVER;
        lv_draw_rect(draw_ctx, &rect_dsc, &bg);

        if (pct > 0U)
        {
            int fill_h = (total_h * (int)pct) / 100;
            if (fill_h < 1)
            {
                fill_h = 1;
            }
            lv_area_t fill = { x1, area->y2 - fill_h + 1, x2, area->y2 };
            rect_dsc.bg_color = GREEN;
            lv_draw_rect(draw_ctx, &rect_dsc, &fill);
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
 *   - 样式参数 (font, offsets) comp_get_style(w_id) 自动查表
 *   - cfg_font_id != 255 时强制覆盖自动字
 *   - 速率图标由工厂自主查字典决定（根elements & ELEM_BAR
 *   - 专属组件（DEPTH/NDL）走早期返回，内部仍style 参数
 *   - 通用组件elements 掩码装配流水线：TITLE VALUE UNIT BAR
 *
 * POD 单模具轮转分配：
 *   - 函数入口检w_id == COMP_POD_0806
 *   - 调用 get_pod_tag() 获得高位掩码标签 (1033/2033)
 *   - 调用 get_pod_index() 获得 POD 编号 (1/2)
 *   - 将标签烙印到容器 user_data
 * ========================================================= */
lv_obj_t *render_widget_by_id(lv_obj_t *parent,
                              comp_id_t w_id,
                              int16_t abs_x, int16_t abs_y,
                              uint16_t abs_w, uint16_t abs_h,
                              uint8_t span_w, uint8_t span_h,
                              font_id_t cfg_font_id)
{
    /* 这是组件工厂的统一入口：同一套 ID 可以同时服务左锚点和 5F 自定义网格。 */
    /* 这个函数承担三层职责：
     * 1. 根据 comp_id_t 找到样式字典
     * 2. 创建 LVGL 对象并完成 user_data 烙印
     * 3. 对复杂组件走专属装配，对普通组件走通用流水线
     * 因此它是整个 UI 组件体系的“总装线”。 */
    /* ===== POD 单模具拦截：提前消耗计数器 ===== */
    bool is_pod_mold = (w_id == COMP_POD_0806);
    uint8_t pod_index = 0;        /* POD number 1 or 2 */
    uintptr_t pod_tag = 0;        /* POD tag 1033 or 2033 */
    if (is_pod_mold)
    {
        /* POD 是单模具复用组件，所以先决定当前这次渲染扮演 POD1 还是 POD2。 */
        /* 这套“轮转分配”依赖布局渲染顺序稳定。
         * 所以前面 screen/layout 重建时必须先 reset_widget_render_state()，
         * 否则 POD1/POD2 身份会错位。 */
        s_pod_render_count++;     /* Increment first, then get current value */
        pod_index = get_pod_index();
        pod_tag = get_pod_tag();
    }

    const comp_style_t *style = comp_get_style(w_id);
    if (!style) return NULL;

    /* 字号选择逻辑
     *   cfg_font_id != 255 强制覆盖（运行时指定
     *   DEPTH 系列 自动适配尺寸（HUGE/MEDIUM/SMALL
     *   其他组件 直接使用字典 font_id */
    font_id_t val_font_id;
    if (cfg_font_id != (font_id_t)255)
    {
        val_font_id = cfg_font_id;  /* 强制覆盖（运行时指定*/
    }
    else if (w_id == COMP_DEPTH_1612 || w_id == COMP_DEPTH_1606)
    {
        /* DEPTH 组件：自动适配尺寸 */
        if (span_w >= 2 && span_h >= 2)
        {
            val_font_id = FONT_ID_HUGE;
        }
        else if (span_w >= 2)
        {
            val_font_id = FONT_ID_MEDIUM;
        }
        else
        {
            val_font_id = FONT_ID_SMALL;
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
    lv_obj_set_style_bg_color(obj, BLACK, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, DARK, 0);
    lv_obj_set_style_border_width(obj, DEBUG_BORDERS ? 1 : 0, 0);
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

    if (w_id == COMP_EMPTY) return obj;

    /* ===== DEPTH 2x2 专属渲染（整小数+单位分离===== */
    bool is_2x2 = (span_w >= 2 && span_h >= 2);
    if (w_id == COMP_DEPTH_1612 && is_2x2)
    {
        /* 样式参数来自 comp_style_t */
        /* DEPTH 大组件单独分支的原因是它的排版过于特殊：
         * 整数、小数、单位、速率图标都不是标准“标题+数值”模型能覆盖的。 */
        const style_depth_t *s = &style->spec.depth;
        ui_vm_depth_t depth_vm;

        ui_vm_depth_update(&depth_vm, NULL);

        /* ==========================================
         * 1. 超大号整-> 宽度必须紧密包裹
         * ========================================== */
        lv_obj_t *int_lbl = lv_label_create(obj);
        if (SHOW_PLACEHOLDER_ON_INIT) lv_label_set_text(int_lbl, "--");
        else lv_label_set_text_fmt(int_lbl, "%d", (int)depth_vm.int_part);
        // 字体从字典读取（font_id = HUGE 58px
        lv_obj_set_style_text_font(int_lbl, get_font(style->font_id), 0);
        lv_obj_set_style_text_color(int_lbl, GREEN, 0);

        // 绝杀技：必须设CONTENT！这样无论变"6" 还是 "45"
        // Label 的右边缘都会死死包住个位数，绝不留一丝缝隙！
        lv_obj_set_size(int_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

        // 读取字典中的 RIGHT_MID -45，把右边缘焊死在这堵墙上
        lv_obj_align(int_lbl, (lv_align_t)s->int_align, s->int_offset_x, s->int_offset_y);

        /* ==========================================
         * 2. 中号小数 -> 紧贴整数的右边界
         * ========================================== */
        lv_obj_t *dec_lbl = lv_label_create(obj);
        if (SHOW_PLACEHOLDER_ON_INIT) lv_label_set_text(dec_lbl, ".-");
        else lv_label_set_text_fmt(dec_lbl, ".%u", (unsigned)depth_vm.dec_part);
        // 字体从字典读取（title_font_id = MEDIUM 28px，小数比整数小）
        lv_obj_set_style_text_font(dec_lbl, get_font(style->title_font_id), 0);
        lv_obj_set_style_text_color(dec_lbl, GREEN, 0);
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
            lv_obj_set_style_text_font(unit_lbl, get_font(FONT_ID_SMALL), 0);
            lv_obj_set_style_text_color(unit_lbl, LIGHT, 0);
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
    else if (w_id == COMP_NDL_STOP_1606)
    {
        /* NDL 变形金刚：从 style->spec.ndl_stop 读取所有位置参*/
        /* NDL/SAFE/DECO 三种状态共用同一个物理组件容器，
         * 后续通过 comp_refresh_ndl_stop_vm() 动态切换文字、进度条和布局。 */
        if (s_ndl_handle_count >= MAX_NDL_ICONS) return obj;
        ndl_handle_t *h = &s_ndl_handles[s_ndl_handle_count++];
        ui_vm_ndl_stop_t *draw_vm = &s_ndl_draw_vm[s_ndl_handle_count - 1U];
        h->comp = obj;
        /* 创建 10 宫格的底层透明画板 */
        h->horiz_bg = lv_obj_create(obj);
        lv_obj_remove_style_all(h->horiz_bg);
        /* 🚨 宽度填满减去两边留白：abs_w - 16，两边各8px */
        lv_obj_set_size(h->horiz_bg, abs_w - 16, 10);
        /* 贴紧底部，略微上4px */
        lv_obj_align(h->horiz_bg, LV_ALIGN_BOTTOM_MID, 0, -4);
        memset(draw_vm, 0, sizeof(*draw_vm));
        lv_obj_add_event_cb(h->horiz_bg, ndl_horiz_bar_draw_cb, LV_EVENT_DRAW_MAIN, draw_vm);
        lv_obj_add_flag(h->horiz_bg, LV_OBJ_FLAG_HIDDEN);

        /* 顶部标题（默认隐藏，停留态时显示*/
        h->title_top = lv_label_create(obj);
        lv_obj_set_style_text_font(h->title_top, get_font(FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(h->title_top, GREEN, 0);
        lv_label_set_text(h->title_top, "");
        lv_obj_add_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);

        /* 主数(22, 3:00) - 使用48px字体 */
        h->main_val = lv_label_create(obj);
        lv_obj_set_style_text_color(h->main_val, GREEN, 0);
        lv_obj_set_style_text_font(h->main_val, get_font(FONT_ID_NDL), 0);
        if (SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(h->main_val, "--");
        else
        {
            ui_vm_ndl_stop_t ndl_vm;
            ui_vm_ndl_stop_update(&ndl_vm, NULL);
            lv_label_set_text_fmt(h->main_val, "%d", ndl_vm.ndl_stop_value);
        }

        /* 底部标题 (NDL 45) */
        h->sub_bot = lv_label_create(obj);
        lv_obj_set_style_text_font(h->sub_bot, get_font(FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(h->sub_bot, GREEN, 0);
        lv_obj_add_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN);
        return obj;
    }
    else if (w_id == COMP_SYS_1606)
    {
        /* ===== SYS 模块：电+ 温度横向排列 ===== */
        /* SYS 组件走静态句柄缓存，是因为它刷新频率高、结构固定，
         * 直接 O(1) 更新比每次遍历整棵对象树更省。 */

        /* 左侧：电Label */
        s_sys_batt_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(s_sys_batt_lbl, get_font(FONT_ID_MEDIUM), 0);
        lv_obj_set_style_text_color(s_sys_batt_lbl, GREEN, 0);
        lv_obj_align(s_sys_batt_lbl, LV_ALIGN_LEFT_MID, 4, 0);
        if (SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(s_sys_batt_lbl, "--%");
        else
        {
            lv_label_set_text_fmt(s_sys_batt_lbl, "%u%%", (unsigned)ui_battery_draw_pct(bus_get_battery_pct()));
        }

        /* 右侧：温Label */
        s_sys_temp_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(s_sys_temp_lbl, get_font(FONT_ID_MEDIUM), 0);
        lv_obj_set_style_text_color(s_sys_temp_lbl, GREEN, 0);
        lv_obj_align(s_sys_temp_lbl, LV_ALIGN_RIGHT_MID, -4, 0);
        if (SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(s_sys_temp_lbl, "-- C");
        else
        {
            float temp_c = bus_get_temperature();
            int16_t temp_int = (int16_t)temp_c;
            uint8_t temp_dec = (uint8_t)(fabsf(temp_c - (float)temp_int) * 10.0f);
            lv_label_set_text_fmt(s_sys_temp_lbl, "%d.%u C", (int)temp_int, (unsigned)temp_dec);
        }

        return obj;
    }

    /* ===== 通用流水线：elements 掩码按需装配零件 =====
     * POD1/POD2/WTIME 及所1x1/2x1 通用组件走此路径
     * ELEM_TITLE ELEM_VALUE ELEM_UNIT ELEM_BAR
     *
     * 样式参数全部来自 comp_get_style(w_id) 查表结果
     * title 文本和数值数据源依赖 w_id switch 分发 */

    /* --- 零件 1：标--- */
    if ((style->elements & ELEM_TITLE) && style->title)
    {
        /* 标题层只负责静态语义标签，真正的动态值统一放到 value 层。
         * 这样 title 字体和 value 字体可以完全独立调。 */
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
        lv_obj_set_style_text_font(title_lbl, get_font(style->title_font_id), 0);
        lv_obj_set_style_text_color(title_lbl, LIGHT, 0);
        lv_obj_set_size(title_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(title_lbl, (lv_align_t)style->title_align,
                     comp_title_edge_offset_x((lv_align_t)style->title_align, style->title_offset_x),
                     style->title_offset_y);
        lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
    }

    /* --- 零件 2：主数--- */
    lv_obj_t *val_lbl = NULL;
    if (style->elements & ELEM_VALUE)
    {
        /* 通用 value 初始化也统一走 VM 文本接口。
         * 这样无论是深度、POD、温度还是 PPO2，创建时就能拿到一份符合 UI 规范的初始文本。 */
        val_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(val_lbl, get_font(val_font_id), 0);
        lv_obj_set_style_text_color(val_lbl, GREEN, 0);

        if (SHOW_PLACEHOLDER_ON_INIT)
        {
            /* 通用占位*/
            lv_label_set_text(val_lbl, "--");
        }
        else
        {
            char buf[48] = "--";
            switch (w_id)
            {
            case COMP_DEPTH_1612:
            case COMP_NDL_STOP_1606:
            case COMP_TEMP_0806:
            case COMP_BATTERY_0806:
            case COMP_STOP_TIME_1606:
            case COMP_POD_0806:
            default:
            {
                ui_vm_value_text_t value_vm;
                ui_vm_value_text_update(&value_vm, w_id, pod_index);
                snprintf(buf, sizeof(buf), "%s", value_vm.text);
                break;
            }
            /* 历史旧 ID 已移除，展示文本统一走 ui_vm_value_text_update()。 */
            }
            lv_label_set_text(val_lbl, buf);
        }
        /* 所有使ELEM_VALUE widget 都使spec.basic.value_align */
        lv_obj_align(val_lbl, (lv_align_t)style->spec.basic.value_align,
                     style->spec.basic.value_offset_x, style->spec.basic.value_offset_y);
        lv_obj_set_user_data(val_lbl, (void *)(uintptr_t)w_id);
    }

    /* --- 零件 3：单--- */
    if ((style->elements & ELEM_UNIT) && style->unit && (style->unit[0] != '\0'))
    {
        lv_obj_t *unit_lbl = lv_label_create(obj);
        lv_label_set_text(unit_lbl, style->unit);
        lv_obj_set_style_text_font(unit_lbl, get_font(FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(unit_lbl, LIGHT, 0);
        if ((style->elements & ELEM_VALUE) && (val_lbl != NULL))
        {
            /* 单位贴在组件内部右侧，数值再贴到单位左侧，避免单位被右边框裁掉。 */
            lv_obj_align(unit_lbl, (lv_align_t)style->spec.basic.value_align,
                         style->spec.basic.value_offset_x, style->spec.basic.value_offset_y);
            lv_obj_align_to(val_lbl, unit_lbl, LV_ALIGN_OUT_LEFT_MID, -2, 0);
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
        if (w_id == COMP_DEPTH_1612)
        {
            const style_depth_t *s = &style->spec.depth;
            lv_obj_t *sudu_img = lv_img_create(obj);
            lv_img_set_src(sudu_img, &sudo_up_level0);
            lv_obj_align(sudu_img, (lv_align_t)s->icon_align, s->icon_offset_x, s->icon_offset_y);
            if (s_ascent_icon_count < MAX_ASCENT_ICONS)
                s_img_ascent_rate[s_ascent_icon_count++] = sudu_img;
        }
        else if (w_id == COMP_ASCENT_0812)
        {
            /* ASCENT_0812 (1x2)：绘制上升速率方向箭头图标（工厂自主查字典决定*/
            lv_obj_t *sudu_img = lv_img_create(obj);
            lv_img_set_src(sudu_img, &sudo_up_level0);
            lv_obj_align(sudu_img, LV_ALIGN_CENTER, 0, 0);
            if (s_ascent_icon_count < MAX_ASCENT_ICONS)
                s_img_ascent_rate[s_ascent_icon_count++] = sudu_img;
        }
        else if (w_id == COMP_COMPASS_1612)
        {
            /* COMPASS_1612 (2x2)：卷tape 在早期分支里，ELEM_BAR 标记spec.compass 驱动 */
        }
        else if (w_id == COMP_TISSUE_GF_4012 || w_id == COMP_TISSUE_RAW_4012)
        {
            if (s_tissue_handle_count < MAX_TISSUE_WIDGETS)
            {
                tissue_handle_t *h = &s_tissue_handles[s_tissue_handle_count++];
                memset(h, 0, sizeof(*h));
                h->widget_id = w_id;

                h->chart = lv_obj_create(obj);
                lv_obj_remove_style_all(h->chart);
                lv_obj_set_size(h->chart, abs_w - 10U, abs_h - 42U);
                lv_obj_align(h->chart, LV_ALIGN_BOTTOM_MID, 0, -8);
                lv_obj_add_event_cb(h->chart, tissue_chart_draw_cb, LV_EVENT_DRAW_MAIN, h);
                lv_obj_add_flag(h->chart, LV_OBJ_FLAG_HIDDEN);

                h->placeholder = lv_label_create(obj);
                lv_obj_set_style_text_font(h->placeholder, get_font(FONT_ID_MEDIUM), 0);
                lv_obj_set_style_text_color(h->placeholder, GREEN, 0);
                lv_label_set_text(h->placeholder, "--");
                lv_obj_align(h->placeholder, LV_ALIGN_CENTER, 0, 6);
            }
        }
        else if (w_id == COMP_SYS_1606)
        {
            /* SYS 电池+ 外设图标（系统状态栏*/
            lv_obj_t *bat_bg = lv_obj_create(obj);
            lv_obj_remove_style_all(bat_bg);
            lv_obj_set_size(bat_bg, 60, 14);
            lv_obj_align(bat_bg, LV_ALIGN_BOTTOM_LEFT, 4, -4);
            lv_obj_set_style_border_width(bat_bg, 1, 0);
            lv_obj_set_style_border_color(bat_bg, GREEN, 0);
            lv_obj_set_style_radius(bat_bg, 2, 0);

            uint8_t pct = ui_battery_draw_pct(bus_get_battery_pct());
            bool battery_low = pct <= 40U;
            lv_obj_t *bat_fill = lv_obj_create(bat_bg);
            lv_obj_remove_style_all(bat_fill);
            lv_obj_set_size(bat_fill, LV_PCT(battery_low ? pct : 100U), LV_PCT(100));
            lv_obj_align(bat_fill, LV_ALIGN_LEFT_MID, 0, 0);
            lv_obj_set_style_bg_color(bat_fill, battery_low ? LIGHT : GREEN, 0);
            lv_obj_set_style_bg_opa(bat_fill, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(bat_fill, 1, 0);
            (void)bat_fill;
        }
    }

    return obj;
}

void comp_refresh_tissue_widgets(const ui_vm_deco_t *vm, uint32_t dirty_mask)
{
    if (vm == NULL)
    {
        return;
    }
    if (s_tissue_handle_count == 0U)
    {
        return;
    }
    if ((dirty_mask & DIRTY_TISSUES) == 0U)
    {
        return;
    }

    for (uint8_t i = 0U; i < s_tissue_handle_count; i++)
    {
        tissue_handle_t *h = &s_tissue_handles[i];

        if (!ui_obj_is_valid(&h->chart) ||
                !ui_obj_is_valid(&h->placeholder))
        {
            memset(h, 0, sizeof(*h));
            continue;
        }

        memcpy(&h->vm, vm, sizeof(h->vm));
        lv_obj_add_flag(h->placeholder, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(h->chart, LV_OBJ_FLAG_HIDDEN);
        lv_obj_invalidate(h->chart);
    }
}

void comp_refresh_ndl_stop_vm(const ui_vm_ndl_stop_t *vm, uint32_t dirty_mask)
{
    if (vm == NULL)
    {
        return;
    }
    if (s_ndl_handle_count == 0)
    {
        return;
    }
    if ((dirty_mask & (DIRTY_NDL_STOP | DIRTY_DEPTH | DIRTY_NDL)) == 0)
    {
        return;
    }
    /* NDL 组件不是“文本刷新”那么简单：
     * 它既要更新主值，又要切换 STOP_NONE / SAFETY / DECO 三种版式，
     * 还要重绘底部 10 格进度条，所以必须走专用刷新器。 */

    const comp_style_t *style = comp_get_style(COMP_NDL_STOP_1606);
    if (!style)
    {
        return;
    }
    (void)style;

    for (int i = 0; i < s_ndl_handle_count; i++)
    {
        ndl_handle_t *h = &s_ndl_handles[i];
        ui_vm_ndl_stop_t *draw_vm = &s_ndl_draw_vm[i];

        if (!ui_obj_is_valid(&h->comp) ||
            !ui_obj_is_valid(&h->horiz_bg) ||
            !ui_obj_is_valid(&h->main_val) ||
            !ui_obj_is_valid(&h->title_top) ||
            !ui_obj_is_valid(&h->sub_bot))
        {
            memset(h, 0, sizeof(*h));
            memset(draw_vm, 0, sizeof(*draw_vm));
            continue;
        }

        memcpy(draw_vm, vm, sizeof(*draw_vm));

        lv_obj_clear_flag(h->horiz_bg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_invalidate(h->horiz_bg);

        if (vm->stop_type == STOP_NONE)
        {
            /* 普通 NDL 态：主值显示剩余免减压时间，底部显示 NDL 标签。 */
            lv_obj_add_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN);

            lv_label_set_text(h->sub_bot, "NDL");
            lv_obj_align(h->sub_bot, LV_ALIGN_LEFT_MID, 8, -6);

            lv_obj_set_style_text_font(h->main_val, get_font(FONT_ID_NDL), 0);
            lv_label_set_text_fmt(h->main_val, "%d", vm->ndl);
            lv_obj_align(h->main_val, LV_ALIGN_CENTER, 0, -8);
        }
        else if (vm->stop_type == STOP_SAFETY)
        {
            /* 安全停留态：顶部显示 SAFE 深度，主值改成倒计时，底部显示 IN STOP 或 NDL 参考。 */
            lv_obj_clear_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN);

            lv_label_set_text_fmt(h->title_top, "SAFE %dm", (int)vm->stop_depth_m);
            lv_obj_align(h->title_top, LV_ALIGN_TOP_LEFT,
                         comp_title_edge_offset_x(LV_ALIGN_TOP_LEFT, 8), 2);

            if (vm->in_stop_zone != 0U)
            {
                lv_label_set_text(h->sub_bot, "IN STOP");
            }
            else
            {
                lv_label_set_text_fmt(h->sub_bot, "NDL %d", vm->ndl);
            }
            lv_obj_align(h->sub_bot, LV_ALIGN_BOTTOM_LEFT, 8, -16);

            int m = vm->stop_time_left_s / 60;
            int s = vm->stop_time_left_s % 60;
            lv_obj_set_style_text_font(h->main_val, get_font(FONT_ID_MEDIUM), 0);
            lv_label_set_text_fmt(h->main_val, "%d:%02d", m, s);
            lv_obj_align(h->main_val, LV_ALIGN_RIGHT_MID, -4, -6);
        }
        else if (vm->stop_type == STOP_DECO)
        {
            /* 减压停留态：逻辑上比 SAFETY 更强制，所以只保留 DECO 深度和剩余时间。 */
            lv_obj_clear_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN);

            lv_label_set_text_fmt(h->title_top, "DECO %dm", (int)vm->stop_depth_m);
            lv_obj_align(h->title_top, LV_ALIGN_TOP_LEFT,
                         comp_title_edge_offset_x(LV_ALIGN_TOP_LEFT, 8), 2);

            int m = vm->stop_time_left_s / 60;
            int s = vm->stop_time_left_s % 60;
            lv_obj_set_style_text_font(h->main_val, get_font(FONT_ID_MEDIUM), 0);
            lv_label_set_text_fmt(h->main_val, "%d:%02d", m, s);
            lv_obj_align(h->main_val, LV_ALIGN_RIGHT_MID, -4, -6);
        }
    }
}

void comp_refresh_ndl_stop(uint32_t dirty_mask)
{
    ui_vm_ndl_stop_t vm;
    ui_vm_ndl_stop_update(&vm, NULL);
    comp_refresh_ndl_stop_vm(&vm, dirty_mask);
}

void comp_refresh_sys(uint32_t dirty_mask)
{
    if ((dirty_mask & DIRTY_BATT) && ui_obj_is_valid(&s_sys_batt_lbl))
    {
        lv_label_set_text_fmt(s_sys_batt_lbl, "%u%%", (unsigned)ui_battery_draw_pct(bus_get_battery_pct()));
    }
    if ((dirty_mask & DIRTY_TEMP) && ui_obj_is_valid(&s_sys_temp_lbl))
    {
        float temp_c = bus_get_temperature();
        int16_t temp_int = (int16_t)temp_c;
        uint8_t temp_dec = (uint8_t)(fabsf(temp_c - (float)temp_int) * 10.0f);
        lv_label_set_text_fmt(s_sys_temp_lbl, "%d.%u C", (int)temp_int, (unsigned)temp_dec);
    }
}

void comp_refresh_ascent_icons(const ui_vm_ascent_t *vm)
{
    static int8_t s_last_direction = 0;  /* 0=still, 1=up, -1=down */

    if ((vm == NULL) || (s_ascent_icon_count == 0))
    {
        return;
    }

    bool is_moving = (vm->is_moving != 0U);
    bool current_flash_state = (vm->flash_on != 0U);
    int8_t current_direction = vm->direction;

    const void *target_img_src = &sudo_up_level0;
    /* 速率图标的决策维度有三个：
     * 1. 是否在运动
     * 2. 方向是上升还是下降
     * 3. 当前处于 level0/1/2 哪一档，并结合 flash_on 做闪烁 */

    if (!is_moving)
    {
        /* 静止时仍保留上一次方向的 level0 图标，避免箭头方向来回丢失。 */
        target_img_src = (s_last_direction > 0) ? &sudo_up_level0 :
                         (s_last_direction < 0) ? &sudo_down_level0 : &sudo_up_level0;
    }
    else if (current_direction > 0)
    {
        if (vm->rate >= RATE_LEVEL2_THRESHOLD)
        {
            target_img_src = current_flash_state ? &sudo_up_level2 : &sudo_up_level0;
        }
        else if (vm->rate >= RATE_LEVEL1_THRESHOLD)
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
        if (vm->rate <= -RATE_LEVEL2_THRESHOLD)
        {
            target_img_src = current_flash_state ? &sudo_down_level2 : &sudo_down_level0;
        }
        else if (vm->rate <= -RATE_LEVEL1_THRESHOLD)
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
        /* 一次速率变化可能要同步多个 DEPTH/ASCENT 组件实例，所以这里广播到全部缓存图标。 */
        if (ui_obj_is_valid(&s_img_ascent_rate[i]))
        {
            lv_img_set_src(s_img_ascent_rate[i], target_img_src);
        }
    }
}
