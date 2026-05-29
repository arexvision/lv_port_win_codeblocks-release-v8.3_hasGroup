/*
 * 文件: src/app_ui/ui/alarm/alarm_view.c
 * 作用: 该文件属于闹钟界面模块，负责闹钟数据、视图构建、交互刷新或与上层 UI 状态之间的衔接。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时建议先理解该文件在 app_ui 流程中的位置，再修改注释所描述的职责边界，避免把数据准备、状态切换和视图渲染职责混杂到一起。
 */

#include "alarm_view.h"
#include "alarm.h"

#include <stdio.h>

#define ALARM_L1_ANIM_MS    220U
#define ALARM_L1_SLIDE_PX   16

static lv_obj_t *s_alarm_banner;
static lv_obj_t *s_alarm_banner_lbl;

static lv_color_t alarm_view_level_color(alarm_level_t level)
{
    /* 当前实现下告警条统一使用绿色系，等级差异主要通过样式和闪烁节奏体现。 */
    (void)level;
    return GREEN;
}

static lv_color_t alarm_view_dim_green(uint8_t percent)
{
    /* 生成低亮度绿色，用于 WARN 状态下的弱背景强调。 */
    uint8_t channel = (uint8_t)((255U * (uint16_t)percent) / 100U);
    return lv_color_make(0x00, channel, 0x00);
}

static void alarm_view_banner_set_opa(void *obj, int32_t opa)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)opa, 0);
}

static void alarm_view_banner_hide_ready(lv_anim_t *anim)
{
    if (anim && anim->var)
    {
        lv_obj_add_flag((lv_obj_t *)anim->var, LV_OBJ_FLAG_HIDDEN);
    }
}

static void alarm_view_banner_cancel_anim(void)
{
    /* 重新进入 banner 显示前，先清掉旧动画，避免状态切换时残留位移或透明度过渡。 */
    if (!s_alarm_banner)
    {
        return;
    }

    lv_anim_del(s_alarm_banner, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_del(s_alarm_banner, alarm_view_banner_set_opa);
}

static void alarm_view_banner_anim_y(lv_coord_t start_y, lv_coord_t end_y, uint16_t time_ms)
{
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, s_alarm_banner);
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&anim, time_ms);
    lv_anim_set_values(&anim, start_y, end_y);
    lv_anim_start(&anim);
}

static void alarm_view_banner_anim_opa(lv_opa_t start_opa,
                                       lv_opa_t end_opa,
                                       uint16_t time_ms,
                                       lv_anim_ready_cb_t ready_cb)
{
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, s_alarm_banner);
    lv_anim_set_exec_cb(&anim, alarm_view_banner_set_opa);
    lv_anim_set_time(&anim, time_ms);
    lv_anim_set_values(&anim, start_opa, end_opa);
    if (ready_cb)
    {
        lv_anim_set_ready_cb(&anim, ready_cb);
    }
    lv_anim_start(&anim);
}

static void alarm_view_banner_animate_in(void)
{
    /* 进入动画通过“位移 + 透明度”双通道完成，增强告警进入感。 */
    if (!s_alarm_banner)
    {
        return;
    }

    alarm_view_banner_cancel_anim();

    lv_coord_t end_y = lv_obj_get_y(s_alarm_banner);
    lv_obj_set_y(s_alarm_banner, end_y - ALARM_L1_SLIDE_PX);
    lv_obj_set_style_opa(s_alarm_banner, LV_OPA_TRANSP, 0);

    alarm_view_banner_anim_y(end_y - ALARM_L1_SLIDE_PX,
                             end_y,
                             ALARM_L1_ANIM_MS);
    alarm_view_banner_anim_opa(LV_OPA_TRANSP,
                               LV_OPA_COVER,
                               ALARM_L1_ANIM_MS,
                               NULL);
}

static void alarm_view_banner_animate_out(void)
{
    /* 退出动画与进入动画对称，结束后再隐藏对象，防止对象瞬间消失。 */
    if (!s_alarm_banner)
    {
        return;
    }

    alarm_view_banner_cancel_anim();

    lv_coord_t start_y = lv_obj_get_y(s_alarm_banner);
    alarm_view_banner_anim_y(start_y,
                             start_y - ALARM_L1_SLIDE_PX,
                             ALARM_L1_ANIM_MS);
    alarm_view_banner_anim_opa(LV_OPA_COVER,
                               LV_OPA_TRANSP,
                               ALARM_L1_ANIM_MS,
                               alarm_view_banner_hide_ready);
}

static void alarm_view_reset_banner_if_invalid(lv_obj_t *safe_zone)
{
    /* 如果安全区重建了，旧 banner 指针就失效，必须强制丢弃。 */
    if (!s_alarm_banner)
    {
        return;
    }

    if (!lv_obj_is_valid(s_alarm_banner) || lv_obj_get_parent(s_alarm_banner) != safe_zone)
    {
        s_alarm_banner = NULL;
        s_alarm_banner_lbl = NULL;
    }
}

static void alarm_view_show_banner(const alarm_view_context_t *ctx,
                                   alarm_level_t level,
                                   const char *text)
{
    /* banner 是闹钟模块对外可见的最直接 UI 反馈。 */
    if (!ctx || !ctx->safe_zone)
    {
        return;
    }

    alarm_view_reset_banner_if_invalid(ctx->safe_zone);

    int card_canvas_w = (int)ctx->safe_zone_w - (int)ctx->left_anchor_w - (int)ctx->panel_gap_px;
    if (card_canvas_w < 0)
    {
        card_canvas_w = 0;
    }

    if (!s_alarm_banner)
    {
        /* 首次创建时只搭骨架，后续复用对象减少频繁销毁创建。 */
        s_alarm_banner = lv_obj_create(ctx->safe_zone);
        lv_obj_remove_style_all(s_alarm_banner);
        lv_obj_set_size(s_alarm_banner, card_canvas_w, 60);

        s_alarm_banner_lbl = lv_label_create(s_alarm_banner);
        lv_obj_set_style_text_font(s_alarm_banner_lbl, get_font(FONT_ID_MEDIUM), 0);
        lv_obj_align(s_alarm_banner_lbl, LV_ALIGN_LEFT_MID, 20, 0);
    }

    lv_obj_set_size(s_alarm_banner, card_canvas_w, 60);
    if (ctx->layout_order == ORDER_NORMAL)
    {
        lv_obj_align(s_alarm_banner, LV_ALIGN_TOP_RIGHT, 0, 0);
    }
    else
    {
        lv_obj_align(s_alarm_banner, LV_ALIGN_TOP_LEFT, 0, 0);
    }

    lv_obj_move_foreground(s_alarm_banner);
    lv_obj_clear_flag(s_alarm_banner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(s_alarm_banner, BLACK, 0);
    lv_obj_set_style_bg_opa(s_alarm_banner, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_alarm_banner_lbl, GREEN, 0);

    (void)level;
    lv_label_set_text(s_alarm_banner_lbl, text ? text : "");
}

static bool alarm_view_target_match(uintptr_t raw, comp_id_t target)
{
    /* 这里同时兼容完整 ID 和 POD 这类做过压缩编码的目标。 */
    if (raw == (uintptr_t)target)
    {
        return true;
    }
    if (target == COMP_POD_0806)
    {
        return (raw % 1000U) == (uintptr_t)COMP_POD_0806;
    }
    return false;
}

static void alarm_view_set_text_color_recursive(lv_obj_t *obj, lv_color_t color)
{
    /* 递归遍历子树，确保 banner 内部所有 label 都同步变色。 */
    if (!obj)
    {
        return;
    }
    if (lv_obj_check_type(obj, &lv_label_class))
    {
        lv_obj_set_style_text_color(obj, color, 0);
    }
    int16_t child_count = lv_obj_get_child_cnt(obj);
    for (int16_t i = 0; i < child_count; i++)
    {
        alarm_view_set_text_color_recursive(lv_obj_get_child(obj, i), color);
    }
}

static void alarm_view_restore_widget_style(lv_obj_t *obj);

static void alarm_view_apply_widget_style(lv_obj_t *obj,
                                          alarm_level_t level,
                                          bool phase_on)
{
    /* 告警高亮的核心逻辑：按等级切换底色、边框和文字颜色。 */
    lv_color_t alarm_color = alarm_view_level_color(level);
    lv_color_t text_color = GREEN;

    if (level >= ALARM_CRIT)
    {
        if (phase_on)
        {
            lv_obj_set_style_bg_color(obj, alarm_color, 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(obj, alarm_color, 0);
            lv_obj_set_style_border_width(obj, 2, 0);
            text_color = BLACK;
        }
        else
        {
            alarm_view_restore_widget_style(obj);
            return;
        }
    }
    else if (level == ALARM_WARN)
    {
        lv_obj_set_style_bg_color(obj, alarm_view_dim_green(15), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(obj, alarm_color, 0);
        lv_obj_set_style_border_width(obj, phase_on ? 2 : 1, 0);
        text_color = GREEN;
    }

    alarm_view_set_text_color_recursive(obj, text_color);
}

static void alarm_view_restore_widget_style(lv_obj_t *obj)
{
    if (!obj)
    {
        return;
    }
    lv_obj_set_style_bg_color(obj, BLACK, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, DARK, 0);
    lv_obj_set_style_border_width(obj, DEBUG_BORDERS ? 1 : 0, 0);
    alarm_view_set_text_color_recursive(obj, GREEN);
}

static void alarm_view_visit_targets(const alarm_view_context_t *ctx,
                                     const comp_id_t *targets,
                                     uint8_t target_count,
                                     alarm_level_t level,
                                     bool phase_on,
                                     bool restore)
{
    if (!ctx)
    {
        return;
    }

    uint8_t max_count = (ctx->custom_card_count < ctx->max_custom_cards)
                        ? ctx->custom_card_count : ctx->max_custom_cards;

    for (uint8_t c = 0; c <= max_count; c++)
    {
        lv_obj_t *container = (c < max_count && ctx->custom_cards) ? ctx->custom_cards[c] : ctx->left_anchor;
        if (!container)
        {
            continue;
        }

        int16_t child_count = lv_obj_get_child_cnt(container);
        for (int16_t i = 0; i < child_count; i++)
        {
            lv_obj_t *child = lv_obj_get_child(container, i);
            uintptr_t raw = (uintptr_t)lv_obj_get_user_data(child);
            bool matched = false;

            for (uint8_t t = 0; t < target_count; t++)
            {
                if (alarm_view_target_match(raw, targets[t]))
                {
                    matched = true;
                    break;
                }
            }

            if (!matched)
            {
                continue;
            }

            if (restore)
            {
                alarm_view_restore_widget_style(child);
            }
            else
            {
                alarm_view_apply_widget_style(child, level, phase_on);
            }
        }
    }
}

static void alarm_view_restore_targets(const alarm_view_context_t *ctx,
                                       const comp_id_t *targets,
                                       uint8_t count)
{
    alarm_view_visit_targets(ctx, targets, count, ALARM_NONE, false, true);
}

static void alarm_view_format_banner(const alarm_display_t *display,
                                     char *buf,
                                     size_t buf_size)
{
    const char *text = display->text ? display->text : "";

    if (display->level >= ALARM_CRIT)
    {
        snprintf(buf, buf_size, "CRITICAL: %s", text);
    }
    else if (display->level == ALARM_WARN)
    {
        snprintf(buf, buf_size, "WARNING: %s", text);
    }
    else
    {
        snprintf(buf, buf_size, "%s", text);
    }
}

void alarm_view_tick(const alarm_view_context_t *ctx)
{
    static comp_id_t s_prev_targets[ALARM_TARGET_MAX];
    static uint8_t s_prev_target_count;
    static uint32_t s_last_revision = 0xFFFFFFFFU;
    static alarm_level_t s_last_level = ALARM_NONE;
    static bool s_last_phase;
    static bool s_last_visible;

    uint32_t now = lv_tick_get();
    alarm_tick(now);

    const alarm_display_t *display = alarm_get_display();
    bool phase_on = true;

    if (display->level >= ALARM_CRIT)
    {
        phase_on = ((now / 333U) % 2U) == 0U;
    }
    else if (display->level == ALARM_WARN)
    {
        phase_on = ((now / 500U) % 2U) == 0U;
    }

    if (!display->visible)
    {
        if (s_last_visible)
        {
            if (s_alarm_banner)
            {
                if (s_last_level == ALARM_INFO)
                {
                    alarm_view_banner_animate_out();
                }
                else
                {
                    alarm_view_banner_cancel_anim();
                    lv_obj_set_style_opa(s_alarm_banner, LV_OPA_COVER, 0);
                    lv_obj_add_flag(s_alarm_banner, LV_OBJ_FLAG_HIDDEN);
                }
            }
            alarm_view_restore_targets(ctx, s_prev_targets, s_prev_target_count);
            s_prev_target_count = 0;
        }

        s_last_visible = false;
        s_last_level = ALARM_NONE;
        s_last_revision = display->revision;
        return;
    }

    bool need_update = (!s_last_visible ||
                        s_last_revision != display->revision ||
                        s_last_level != display->level ||
                        s_last_phase != phase_on);
    if (!need_update)
    {
        return;
    }

    if (s_prev_target_count > 0U)
    {
        alarm_view_restore_targets(ctx, s_prev_targets, s_prev_target_count);
        s_prev_target_count = 0;
    }

    char banner_text[128];
    bool was_visible = s_last_visible;
    alarm_level_t prev_level = s_last_level;
    uint32_t prev_revision = s_last_revision;

    alarm_view_format_banner(display, banner_text, sizeof(banner_text));
    alarm_view_show_banner(ctx, display->level, banner_text);

    if (s_alarm_banner && s_alarm_banner_lbl)
    {
        lv_color_t alarm_color = alarm_view_level_color(display->level);

        if (display->level >= ALARM_CRIT)
        {
            alarm_view_banner_cancel_anim();
            lv_obj_set_style_opa(s_alarm_banner, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(s_alarm_banner, phase_on ? alarm_color : BLACK, 0);
            lv_obj_set_style_bg_opa(s_alarm_banner, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(s_alarm_banner, alarm_color, 0);
            lv_obj_set_style_border_width(s_alarm_banner, 2, 0);
            lv_obj_set_style_text_color(s_alarm_banner_lbl, phase_on ? BLACK : alarm_color, 0);
        }
        else if (display->level == ALARM_WARN)
        {
            alarm_view_banner_cancel_anim();
            lv_obj_set_style_opa(s_alarm_banner, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(s_alarm_banner, alarm_view_dim_green(20), 0);
            lv_obj_set_style_bg_opa(s_alarm_banner, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(s_alarm_banner, alarm_color, 0);
            lv_obj_set_style_border_width(s_alarm_banner, phase_on ? 4 : 1, 0);
            lv_obj_set_style_text_color(s_alarm_banner_lbl, alarm_color, 0);
        }
        else
        {
            lv_obj_set_style_bg_color(s_alarm_banner, alarm_view_dim_green(10), 0);
            lv_obj_set_style_bg_opa(s_alarm_banner, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(s_alarm_banner, alarm_color, 0);
            lv_obj_set_style_border_width(s_alarm_banner, 1, 0);
            lv_obj_set_style_text_color(s_alarm_banner_lbl, alarm_color, 0);

            if (!was_visible ||
                prev_level != display->level ||
                prev_revision != display->revision)
            {
                alarm_view_banner_animate_in();
            }
        }
    }

    s_prev_target_count = alarm_get_active_targets(display->level,
                                                        s_prev_targets,
                                                        ALARM_TARGET_MAX);
    if (s_prev_target_count > 0U)
    {
        alarm_view_visit_targets(ctx, s_prev_targets, s_prev_target_count,
                                 display->level, phase_on, false);
    }

    s_last_visible = true;
    s_last_level = display->level;
    s_last_phase = phase_on;
    s_last_revision = display->revision;
}
