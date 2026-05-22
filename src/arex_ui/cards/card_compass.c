#include "../screen/arex_screen.h"
#include "../core/arex_data.h"
#include "../core/arex_ui_engine.h"
#include "../core/arex_ui_state.h"
#include "../screen/arex_layout_view.h"
#include "card_compass.h"
#include "lvgl/lvgl.h"
#include "../fonts/arex_fonts.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

extern void rt_kprintf(const char *fmt, ...);

/* ============================================================
 * 1F: NAV COMPASS — 零内存数学绘制引擎
 *
 * 采用 LV_EVENT_DRAW_MAIN 回调实现纯数学渲染，内存占用为 0。
 * 完美复刻 HTML 原型中顺滑的"战术卷尺（Tactical Tape）"效果。
 *
 * 规范参数：
 *   卷尺高度：60px
 *   像素/度比例：3.0（刻度更密集紧凑）
 *   每 3 度一根密集短线，每 15 度一根中线，每 45 度显示纯方位字母
 *   上下极简边框（OPA_30），中心瞄准基线 3px 绿色
 * ============================================================ */

/* ============================================================
 * 罗盘卷尺绘制参数
 * ============================================================ */
#define COMPASS_TAPE_H      60      /* 卷尺高度 60px */
#define PX_PER_DEGREE       3.0f   /* 像素/度比例 */
#define TAPE_TOP_OFFSET     10      /* 卷尺距标题区顶部的偏移 */

/* ============================================================
 * 罗盘卷尺底层数学绘制引擎 (0 RAM 开销)
 *
 * 该回调在 LVGL 重绘时自动调用，仅渲染可见窗口内的刻度。
 * 360° 和 0° 交界处完美循环。
 *
 * 视觉规范（HTML 原型复刻）：
 *   - 每 3 度画一根密集短线
 *   - 每 15 度画一根中长线
 *   - 每 45 度画最长的线并显示 N/NE/E/SE/S/SW/W/NW
 *   - 上下两根若隐若现的边框线（OPA_30）
 *   - 正中央 3px 宽绿色瞄准基线
 *   - 目标锁定时底部显示绿色游标块
 * ============================================================ */
static void compass_tape_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t *area = &obj->coords;

    int box_w = lv_area_get_width(area);
    int center_x = area->x1 + (box_w / 2);  /* 屏幕视口正中心 */

    float heading = (float)g_sensor_data.heading;  /* 当前航向 */

    /* ---- 1. 初始化画笔 ---- */
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);

    lv_draw_label_dsc_t lbl_dsc;
    lv_draw_label_dsc_init(&lbl_dsc);
    lbl_dsc.color = GREEN;
    lbl_dsc.font = arex_get_font(FONT_ID_SMALL);  /* 14px 小字 */
    lbl_dsc.align = LV_TEXT_ALIGN_CENTER;

    /* ---- 2. 绘制上下轨道线 (极淡的透明度) ---- */
    line_dsc.color = GREEN;
    line_dsc.width = 1;
    line_dsc.opa = 76;  /* 约 30% 透明度 */
    lv_point_t top_line[] = {{area->x1, area->y1}, {area->x2, area->y1}};
    lv_point_t bot_line[] = {{area->x1, area->y2}, {area->x2, area->y2}};
    lv_draw_line(draw_ctx, &line_dsc, &top_line[0], &top_line[1]);
    lv_draw_line(draw_ctx, &line_dsc, &bot_line[0], &bot_line[1]);

    /* ---- 3. 动态计算视口左右边缘对应的"度数"范围 ---- */
    int fov_deg = (int)(box_w / PX_PER_DEGREE);
    int start_deg = (int)heading - (fov_deg / 2) - 10;
    int end_deg = (int)heading + (fov_deg / 2) + 10;

    /* ---- 4. 绘制精密刻度 ---- */
    line_dsc.opa = LV_OPA_COVER;  /* 刻度线全亮 */

    for (int i = start_deg; i <= end_deg; i++)
    {
        /* 每 3 度一根密集短线 */
        if (i % 3 != 0) continue;

        /* 数学推演：该度数在屏幕上的 X 坐标 */
        int dx = (int)((i - heading) * PX_PER_DEGREE);
        int x = center_x + dx;

        /* 越界裁剪保护 */
        if (x < area->x1 || x > area->x2) continue;

        /* 梯级刻度设计：短线 5px，中线 9px，长线 14px */
        int tick_h = 5;
        line_dsc.width = 1;

        /* 逢 15 度画中长线 */
        if (i % 15 == 0)
        {
            tick_h = 9;
            line_dsc.width = 2;
        }

        /* 逢 45 度画最长的线，并绘制 N, NE 等纯正方位字母 */
        if (i % 45 == 0)
        {
            tick_h = 14;

            /* 处理 360 度循环取模 (比如 -45度 变成 315度) */
            int display_deg = ((i % 360) + 360) % 360;
            char buf[8] = "";

            if (display_deg == 0) strcpy(buf, "N");
            else if (display_deg == 45) strcpy(buf, "NE");
            else if (display_deg == 90) strcpy(buf, "E");
            else if (display_deg == 135) strcpy(buf, "SE");
            else if (display_deg == 180) strcpy(buf, "S");
            else if (display_deg == 225) strcpy(buf, "SW");
            else if (display_deg == 270) strcpy(buf, "W");
            else if (display_deg == 315) strcpy(buf, "NW");

            /* 文本下压，避免与刻度线粘连 */
            lv_area_t txt_area = {x - 20, area->y1 + 18, x + 20, area->y1 + 38};
            lv_draw_label(draw_ctx, &lbl_dsc, &txt_area, buf, NULL);
        }

        /* 刻度线必须统一从顶部 (y1) 往下垂直画 */
        lv_point_t pts[] = {{x, area->y1}, {x, area->y1 + tick_h}};
        lv_draw_line(draw_ctx, &line_dsc, &pts[0], &pts[1]);
    }

    /* ---- 5. 贯穿整个卷尺的中心准星 (高亮加粗) ---- */
    line_dsc.color = GREEN;
    line_dsc.width = 3;
    line_dsc.opa = LV_OPA_COVER;
    lv_point_t center_pts[] = {{center_x, area->y1}, {center_x, area->y2}};
    lv_draw_line(draw_ctx, &line_dsc, &center_pts[0], &center_pts[1]);

    /* ---- 6. 目标锁定游标 (如果用户锁定了航向) ---- */
    if (g_sensor_data.heading_locked)
    {
        float target_dx = (float)(g_sensor_data.heading_target - heading);

        /* 处理捷径逻辑 (如当前350，目标10，应向右走20度而不是向左走340度) */
        if (target_dx > 180.0f) target_dx -= 360.0f;
        if (target_dx < -180.0f) target_dx += 360.0f;

        int tx = center_x + (int)(target_dx * PX_PER_DEGREE);

        if (tx >= area->x1 && tx <= area->x2)
        {
            /* 在目标方位画一个向上的小实心绿块 */
            lv_draw_rect_dsc_t rect_dsc;
            lv_draw_rect_dsc_init(&rect_dsc);
            rect_dsc.bg_color = GREEN;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_area_t t_area = {tx - 4, area->y2 - 8, tx + 4, area->y2};
            lv_draw_rect(draw_ctx, &rect_dsc, &t_area);
        }
    }
}

/* ============================================================
 * 静态句柄声明（供外部 arex_ui_engine.c 引用）
 * ============================================================ */
static lv_obj_t *s_compass_tape_obj = NULL;   /* 卷尺绘制对象 */
static lv_obj_t *s_heading_val_lbl = NULL;    /* 巨型航向文本 */
static lv_obj_t *s_heading_hint_lbl = NULL;   /* 顶部操作提示 */

/* ============================================================
 * 罗盘卡片工厂渲染函数
 *
 * 天地反转布局（完美对齐 HTML 原型）：
 *   1. 顶部操作提示区
 *   2. 居中巨型度数（视觉中心）
 *   3. 底部钉死卷尺
 * ============================================================ */
void render_compass_custom(lv_obj_t *parent_card)
{
    /* 1. 统一标题 */
    arex_render_card_title(parent_card, "NAV COMPASS");

    /* 计算右侧区域宽度 */
    int right_canvas_w = (int)g_sys_config.safe_zone_w - (int)LEFT_ANCHOR_W
                         - (int)(g_sys_config.gap_u * BASE_U);

    /* ========================================================
     * 1. 顶部操作提示区 (Target Locked / Enter mark)
     * 挂载在大标题下方
     * ======================================================== */
    s_heading_hint_lbl = lv_label_create(parent_card);
    lv_obj_set_style_text_font(s_heading_hint_lbl, arex_get_font(FONT_ID_SMALL), 0);
    lv_obj_set_style_text_color(s_heading_hint_lbl, LIGHT, 0);
    lv_obj_align(s_heading_hint_lbl, LV_ALIGN_TOP_MID, 0, CARD_TITLE_H + 20);

    if (g_sensor_data.heading_locked)
    {
        lv_label_set_text_fmt(s_heading_hint_lbl, "[ TARGET LOCKED: %03d ]", g_sensor_data.heading_target);
    }
    else
    {
        lv_label_set_text(s_heading_hint_lbl, "[ ENTER ] mark heading");
    }

    /* ========================================================
     * 2. 居中巨型当前航向文本 (绝对视觉中心)
     * ======================================================== */
    s_heading_val_lbl = lv_label_create(parent_card);
    lv_obj_set_style_text_font(s_heading_val_lbl, arex_get_font(FONT_ID_HUGE), 0);  /* 48px */
    lv_obj_set_style_text_color(s_heading_val_lbl, GREEN, 0);
    /* 核心修复：绝对居中，并稍微往上抬一点点 */
    lv_obj_align(s_heading_val_lbl, LV_ALIGN_CENTER, 0, -10);
    lv_label_set_text_fmt(s_heading_val_lbl, "%03d", g_sensor_data.heading);

    /* 度数空心小圆圈 */
    lv_obj_t *degree_circle = lv_obj_create(parent_card);
    lv_obj_remove_style_all(degree_circle);
    lv_obj_set_size(degree_circle, 10, 10);
    lv_obj_set_style_border_width(degree_circle, 2, 0);
    lv_obj_set_style_border_color(degree_circle, GREEN, 0);
    lv_obj_set_style_radius(degree_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_align_to(degree_circle, s_heading_val_lbl, LV_ALIGN_OUT_RIGHT_TOP, 4, 8);

    /* ========================================================
     * 3. 零内存罗盘卷尺 (Tape) -> 钉死在最底部！
     * ======================================================== */
    s_compass_tape_obj = lv_obj_create(parent_card);
    lv_obj_remove_style_all(s_compass_tape_obj);

    /* 宽度留边，高度 60px */
    lv_obj_set_size(s_compass_tape_obj, right_canvas_w - 20, COMPASS_TAPE_H);
    /* 核心修复：钉在卡片最底部，往上偏移 20px 防贴边 */
    lv_obj_align(s_compass_tape_obj, LV_ALIGN_BOTTOM_MID, 0, -20);

    /* 挂载数学绘制引擎 */
    lv_obj_add_event_cb(s_compass_tape_obj, compass_tape_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
}

/* ============================================================
 * 卡片创建/更新回调 (保留旧接口兼容)
 * ============================================================ */
void card_compass_create(lv_obj_t *parent)
{
    render_compass_custom(parent);
}

void card_compass_refresh_heading(bool force_refresh)
{
#if BLE_COMPASS_DIAG_LOG_ENABLED
    static uint32_t s_last_compass_ui_log_tick = 0;
    static uint16_t s_last_compass_ui_heading = 0xFFFFU;

    if (force_refresh)
    {
        ble_sensor_debug_note_ui_force_refresh(g_sensor_data.heading);
    }
    else
    {
        uint32_t now_tick = lv_tick_get();
        bool heading_changed = (s_last_compass_ui_heading != g_sensor_data.heading);
        bool heartbeat_due =
            (s_last_compass_ui_log_tick == 0U) ||
            ((now_tick - s_last_compass_ui_log_tick) >= 2000U);

        if (heading_changed || heartbeat_due)
        {
            s_last_compass_ui_log_tick = now_tick;
            s_last_compass_ui_heading = g_sensor_data.heading;
            ble_sensor_debug_note_ui_dirty(g_sensor_data.heading);
#if BLE_COMPASS_DIAG_SYSTEM_LOG_ENABLED
            rt_kprintf("[COMPASS_UI] dirty heading=%u label=%d tape=%d card=%u dash=%u\r\n",
                       g_sensor_data.heading,
                       s_heading_val_lbl ? 1 : 0,
                       s_compass_tape_obj ? 1 : 0,
                       g_sys_config.card_order[g_ui.dash_card],
                       g_ui.dash_card);
#endif
        }
    }
#else
    (void)force_refresh;
#endif

    if (s_heading_val_lbl)
    {
        lv_label_set_text_fmt(s_heading_val_lbl, "%03d", g_sensor_data.heading);
    }
    if (s_compass_tape_obj)
    {
        lv_obj_invalidate(s_compass_tape_obj);
    }
    if (s_heading_hint_lbl)
    {
        if (g_sensor_data.heading_locked)
        {
            lv_label_set_text_fmt(s_heading_hint_lbl, "[ TARGET LOCKED: %03d ]", g_sensor_data.heading_target);
        }
        else
        {
            lv_label_set_text(s_heading_hint_lbl, "[ ENTER ] mark heading");
        }
    }
}

void card_compass_update(void)
{
    /* Keep compass labels and the custom tape draw surface in sync. */
    card_compass_refresh_heading(false);
}
