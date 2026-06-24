/*
 * 文件: src/app_ui/ui/alarm/alarm_view.c
 * 作用: 该文件属于闹钟界面模块，负责闹钟数据、视图构建、交互刷新或与上层 UI 状态之间的衔接。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时建议先理解该文件在 app_ui 流程中的位置，再修改注释所描述的职责边界，避免把数据准备、状态切换和视图渲染职责混杂到一起。
 */

#include "alarm_view.h"
#include "alarm.h"
#include "../core/data.h"
#include "../screen/screen.h"

#include <stdio.h>

#define ALARM_L1_ANIM_MS    220U
#define ALARM_L1_SLIDE_PX   16
#define ALARM_WARN_BORDER_W_ON   3U
#define ALARM_WARN_BORDER_W_OFF  1U
#define ALARM_WARN_BANNER_BORDER_W  3U
#define ALARM_TARGET_USE_OVERLAY  1U  /* 告警边框用覆盖层绘制，避免改父组件边框导致内容抖动 */
#define ALARM_TARGET_OVERLAY_TAG  ((uintptr_t)0xA11A0001U)  /* 告警目标覆盖层标记 */
#define LOW_POWER_SHUTDOWN_OVERLAY_W 220
#define LOW_POWER_SHUTDOWN_OVERLAY_H 112

static lv_obj_t *s_alarm_banner;
static lv_obj_t *s_alarm_banner_lbl;
static lv_coord_t s_alarm_banner_target_y;
static lv_obj_t *s_low_power_shutdown_overlay;
static lv_obj_t *s_low_power_shutdown_title_lbl;
static lv_obj_t *s_low_power_shutdown_time_lbl;
static uint8_t s_low_power_shutdown_last_sec = 0xFFU;

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
        lv_obj_t *banner = (lv_obj_t *)anim->var;
        lv_obj_add_flag(banner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(banner, 0);
        lv_obj_set_style_opa(banner, LV_OPA_COVER, 0);
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

    lv_coord_t end_y = s_alarm_banner_target_y;
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

    lv_coord_t start_y = s_alarm_banner_target_y;
    lv_obj_set_y(s_alarm_banner, start_y);
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

static void alarm_view_reset_low_power_shutdown_if_invalid(lv_obj_t *safe_zone)
{
    if (!s_low_power_shutdown_overlay)
    {
        return;
    }

    if (!lv_obj_is_valid(s_low_power_shutdown_overlay) ||
        lv_obj_get_parent(s_low_power_shutdown_overlay) != safe_zone)
    {
        s_low_power_shutdown_overlay = NULL;
        s_low_power_shutdown_title_lbl = NULL;
        s_low_power_shutdown_time_lbl = NULL;
        s_low_power_shutdown_last_sec = 0xFFU;
    }
}

static void alarm_view_hide_low_power_shutdown_overlay(void)
{
    if (s_low_power_shutdown_overlay)
    {
        lv_obj_add_flag(s_low_power_shutdown_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    s_low_power_shutdown_last_sec = 0xFFU;
}

static void alarm_view_show_low_power_shutdown_overlay(const alarm_view_context_t *ctx,
                                                       uint8_t remaining_sec,
                                                       bool phase_on)
{
    if (!ctx || !ctx->safe_zone)
    {
        return;
    }

    alarm_view_reset_low_power_shutdown_if_invalid(ctx->safe_zone);

    if (!s_low_power_shutdown_overlay)
    {
        s_low_power_shutdown_overlay = lv_obj_create(ctx->safe_zone);
        lv_obj_remove_style_all(s_low_power_shutdown_overlay);
        lv_obj_set_size(s_low_power_shutdown_overlay,
                        LOW_POWER_SHUTDOWN_OVERLAY_W,
                        LOW_POWER_SHUTDOWN_OVERLAY_H);
        lv_obj_clear_flag(s_low_power_shutdown_overlay, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(s_low_power_shutdown_overlay, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(s_low_power_shutdown_overlay, 0, 0);
        lv_obj_set_style_border_width(s_low_power_shutdown_overlay, 2, 0);
        lv_obj_set_style_border_color(s_low_power_shutdown_overlay, GREEN, 0);
        lv_obj_set_style_border_opa(s_low_power_shutdown_overlay, LV_OPA_COVER, 0);

        s_low_power_shutdown_title_lbl = lv_label_create(s_low_power_shutdown_overlay);
        lv_obj_set_width(s_low_power_shutdown_title_lbl, LV_PCT(100));
        lv_obj_set_style_text_font(s_low_power_shutdown_title_lbl, get_font(FONT_ID_MEDIUM), 0);
        lv_obj_set_style_text_align(s_low_power_shutdown_title_lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(s_low_power_shutdown_title_lbl, LV_ALIGN_TOP_MID, 0, 12);
        lv_label_set_text(s_low_power_shutdown_title_lbl, "SHUTDOWN IN");

        s_low_power_shutdown_time_lbl = lv_label_create(s_low_power_shutdown_overlay);
        lv_obj_set_width(s_low_power_shutdown_time_lbl, LV_PCT(100));
        lv_obj_set_style_text_font(s_low_power_shutdown_time_lbl, get_font(FONT_ID_MEDIUM), 0);
        lv_obj_set_style_text_align(s_low_power_shutdown_time_lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(s_low_power_shutdown_time_lbl, LV_ALIGN_BOTTOM_MID, 0, -16);
    }

    lv_obj_set_size(s_low_power_shutdown_overlay,
                    LOW_POWER_SHUTDOWN_OVERLAY_W,
                    LOW_POWER_SHUTDOWN_OVERLAY_H);

    /* 倒计时提示固定在移动内容区中心，避开左/上/下固定栏，避免遮挡固定信息栏。 */
    lv_coord_t overlay_x = (lv_coord_t)ctx->content_x;
    lv_coord_t overlay_y = (lv_coord_t)ctx->content_y;
    if (ctx->content_w > LOW_POWER_SHUTDOWN_OVERLAY_W)
    {
        overlay_x += (lv_coord_t)((ctx->content_w - LOW_POWER_SHUTDOWN_OVERLAY_W) / 2U);
    }
    if (ctx->content_h > LOW_POWER_SHUTDOWN_OVERLAY_H)
    {
        overlay_y += (lv_coord_t)((ctx->content_h - LOW_POWER_SHUTDOWN_OVERLAY_H) / 2U);
    }
    lv_obj_set_pos(s_low_power_shutdown_overlay, overlay_x, overlay_y);
    lv_obj_clear_flag(s_low_power_shutdown_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_low_power_shutdown_overlay);

    lv_color_t bg_color = phase_on ? GREEN : BLACK;
    lv_color_t text_color = phase_on ? BLACK : GREEN;
    lv_obj_set_style_bg_color(s_low_power_shutdown_overlay, bg_color, 0);
    lv_obj_set_style_bg_opa(s_low_power_shutdown_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_low_power_shutdown_title_lbl, text_color, 0);
    lv_obj_set_style_text_color(s_low_power_shutdown_time_lbl, text_color, 0);

    if (remaining_sec != s_low_power_shutdown_last_sec)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%us", (unsigned int)remaining_sec);
        lv_label_set_text(s_low_power_shutdown_time_lbl, buf);
        s_low_power_shutdown_last_sec = remaining_sec;
    }
}

static void alarm_view_banner_rect(const alarm_view_context_t *ctx,
                                   lv_coord_t *out_x,
                                   lv_coord_t *out_y,
                                   lv_coord_t *out_w,
                                   lv_coord_t *out_h)
{
    *out_x = (lv_coord_t)ctx->content_x;
    *out_y = (lv_coord_t)ctx->content_y;
    *out_w = (lv_coord_t)ctx->content_w;
    *out_h = CARD_TITLE_H;

    if (!ctx->vertical_split)
    {
        *out_x = 0;
        *out_w = (lv_coord_t)ctx->safe_zone_w;
        *out_y = (lv_coord_t)ctx->content_y;
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

    lv_coord_t banner_x;
    lv_coord_t banner_y;
    lv_coord_t banner_w;
    lv_coord_t banner_h;
    alarm_view_banner_rect(ctx, &banner_x, &banner_y, &banner_w, &banner_h);
    s_alarm_banner_target_y = banner_y;

    if (!s_alarm_banner)
    {
        /* 首次创建时只搭骨架，后续复用对象减少频繁销毁创建。 */
        s_alarm_banner = lv_obj_create(ctx->safe_zone);
        lv_obj_remove_style_all(s_alarm_banner);
        lv_obj_set_size(s_alarm_banner, banner_w, banner_h);

        s_alarm_banner_lbl = lv_label_create(s_alarm_banner);
        lv_obj_set_style_text_font(s_alarm_banner_lbl, get_font(FONT_ID_MEDIUM), 0);
        lv_obj_align(s_alarm_banner_lbl, LV_ALIGN_LEFT_MID, 20, 0);
    }

    lv_obj_set_size(s_alarm_banner, banner_w, banner_h);
    lv_obj_set_pos(s_alarm_banner, banner_x, banner_y);

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
#if ALARM_TARGET_MATCH_DEPTH_1612
    if ((target == COMP_DEPTH_1606 && raw == (uintptr_t)COMP_DEPTH_1612) ||
        (target == COMP_DEPTH_1612 && raw == (uintptr_t)COMP_DEPTH_1606))
    {
        return true;
    }
#endif
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

#if ALARM_TARGET_USE_OVERLAY
static lv_obj_t *alarm_view_find_target_overlay(lv_obj_t *obj)
{
    if (!obj)
    {
        return NULL;
    }

    int16_t child_count = lv_obj_get_child_cnt(obj);
    for (int16_t i = 0; i < child_count; i++)
    {
        lv_obj_t *child = lv_obj_get_child(obj, i);
        if ((uintptr_t)lv_obj_get_user_data(child) == ALARM_TARGET_OVERLAY_TAG)
        {
            return child;
        }
    }

    return NULL;
}

static lv_obj_t *alarm_view_ensure_target_overlay(lv_obj_t *obj)
{
    lv_obj_t *overlay = alarm_view_find_target_overlay(obj);
    if (overlay)
    {
        return overlay;
    }

    overlay = lv_obj_create(obj);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_user_data(overlay, (void *)ALARM_TARGET_OVERLAY_TAG);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_align(overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    return overlay;
}

static void alarm_view_set_target_overlay(lv_obj_t *obj, lv_color_t color, uint8_t border_width)
{
    lv_obj_t *overlay = alarm_view_ensure_target_overlay(obj);
    if (!overlay)
    {
        return;
    }

    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_align(overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_border_color(overlay, color, 0);
    lv_obj_set_style_border_width(overlay, border_width, 0);
    lv_obj_set_style_border_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(overlay);
}

static void alarm_view_hide_target_overlay(lv_obj_t *obj)
{
    lv_obj_t *overlay = alarm_view_find_target_overlay(obj);
    if (overlay)
    {
        lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    }
}
#endif

static void alarm_view_restore_widget_style(lv_obj_t *obj);

static void alarm_view_apply_widget_style(lv_obj_t *obj,
                                          alarm_target_effect_t effect,
                                          bool phase_on)
{
    /* 告警高亮的核心逻辑：按等级切换底色、边框和文字颜色。 */
    lv_color_t alarm_color = alarm_view_level_color(ALARM_CRIT);
    lv_color_t text_color = GREEN;

    if (effect == ALARM_TARGET_EFFECT_CRIT_FLASH)
    {
        if (phase_on)
        {
            lv_obj_set_style_bg_color(obj, alarm_color, 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
#if ALARM_TARGET_USE_OVERLAY
            alarm_view_set_target_overlay(obj, alarm_color, 2U);
#else
            lv_obj_set_style_border_color(obj, alarm_color, 0);
            lv_obj_set_style_border_width(obj, 2, 0);
#endif
            text_color = BLACK;
        }
        else
        {
            alarm_view_restore_widget_style(obj);
            return;
        }
    }
    else if (effect == ALARM_TARGET_EFFECT_WARN_BREATHE)
    {
        lv_obj_set_style_bg_color(obj, alarm_view_dim_green(15), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
#if ALARM_TARGET_USE_OVERLAY
        alarm_view_set_target_overlay(obj, alarm_color, phase_on ? ALARM_WARN_BORDER_W_ON : ALARM_WARN_BORDER_W_OFF);
#else
        lv_obj_set_style_border_color(obj, alarm_color, 0);
        lv_obj_set_style_border_width(obj, phase_on ? ALARM_WARN_BORDER_W_ON : ALARM_WARN_BORDER_W_OFF, 0);
#endif
        text_color = GREEN;
    }
    else
    {
        alarm_view_restore_widget_style(obj);
        return;
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
#if ALARM_TARGET_USE_OVERLAY
    alarm_view_hide_target_overlay(obj);
#endif
    alarm_view_set_text_color_recursive(obj, GREEN);
}

static void alarm_view_visit_targets(const alarm_view_context_t *ctx,
                                     const comp_id_t *targets,
                                     uint8_t target_count,
                                     alarm_target_effect_t effect,
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
        if (!restore && (c < max_count) && !screen_custom_card_refresh_visible(c))
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
                alarm_view_apply_widget_style(child, effect, phase_on);
            }
        }
    }
}

static void alarm_view_restore_targets(const alarm_view_context_t *ctx,
                                       const comp_id_t *targets,
                                       uint8_t count)
{
    alarm_view_visit_targets(ctx, targets, count, ALARM_TARGET_EFFECT_NONE, false, true);
}

static comp_id_t alarm_view_comp_from_raw(uintptr_t raw)
{
    if (raw >= 1000U)
    {
        raw %= 1000U;
    }
    if (raw == 0U || raw >= 255U)
    {
        return COMP_EMPTY;
    }
    return (comp_id_t)raw;
}

static void alarm_view_visible_target_add(comp_id_t *targets,
                                          uint8_t *count,
                                          comp_id_t target)
{
    if (targets == NULL || count == NULL || target == COMP_EMPTY || *count >= ALARM_VISIBLE_TARGET_MAX)
    {
        return;
    }

    for (uint8_t i = 0U; i < *count; i++)
    {
        if (targets[i] == target)
        {
            return;
        }
    }
    targets[(*count)++] = target;
}

static void alarm_view_collect_visible_targets(const alarm_view_context_t *ctx)
{
    comp_id_t targets[ALARM_VISIBLE_TARGET_MAX];
    uint8_t count = 0U;

    if (!ctx)
    {
        alarm_set_visible_targets(NULL, 0U);
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
        if ((c < max_count) && !screen_custom_card_refresh_visible(c))
        {
            continue;
        }

        int16_t child_count = lv_obj_get_child_cnt(container);
        for (int16_t i = 0; i < child_count; i++)
        {
            lv_obj_t *child = lv_obj_get_child(container, i);
            comp_id_t target = alarm_view_comp_from_raw((uintptr_t)lv_obj_get_user_data(child));
            alarm_view_visible_target_add(targets, &count, target);
        }
    }

    alarm_set_visible_targets(targets, count);
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
    static uint32_t s_last_revision = UINT32_MAX;
    static alarm_level_t s_last_level = ALARM_NONE;
    static bool s_last_phase;
    static bool s_last_visible;

    uint32_t now = lv_tick_get();
    alarm_view_collect_visible_targets(ctx);
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

    bool need_banner_update = (!s_last_visible ||
                               s_last_revision != display->revision ||
                               s_last_level != display->level ||
                               s_last_phase != phase_on);

    if (!display->visible && s_last_visible)
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
    }
    else if (display->visible && need_banner_update)
    {
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
                lv_obj_set_style_border_width(s_alarm_banner, ALARM_WARN_BANNER_BORDER_W, 0);
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
    }

    if (s_prev_target_count > 0U)
    {
        alarm_view_restore_targets(ctx, s_prev_targets, s_prev_target_count);
        s_prev_target_count = 0;
    }

    alarm_target_effect_entry_t effects[ALARM_TARGET_MAX];
    uint8_t effect_count = alarm_get_target_effects(effects, ALARM_TARGET_MAX);
    for (uint8_t i = 0U; i < effect_count; i++)
    {
        bool target_phase = true;
        if (effects[i].effect == ALARM_TARGET_EFFECT_CRIT_FLASH)
        {
            target_phase = ((now / 333U) % 2U) == 0U;
        }
        else if (effects[i].effect == ALARM_TARGET_EFFECT_WARN_BREATHE)
        {
            target_phase = ((now / 500U) % 2U) == 0U;
        }
        alarm_view_visit_targets(ctx, &effects[i].target, 1U, effects[i].effect, target_phase, false);
        if (s_prev_target_count < ALARM_TARGET_MAX)
        {
            s_prev_targets[s_prev_target_count++] = effects[i].target;
        }
    }

    if (bus_get_low_power_shutdown_active())
    {
        uint8_t remaining_sec = bus_get_low_power_shutdown_remaining_sec();
        alarm_view_show_low_power_shutdown_overlay(ctx,
                                                   remaining_sec,
                                                   (remaining_sec % 2U) == 0U);
    }
    else
    {
        alarm_view_hide_low_power_shutdown_overlay();
    }

    s_last_visible = display->visible;
    s_last_level = display->visible ? display->level : ALARM_NONE;
    s_last_phase = phase_on;
    s_last_revision = display->revision;
}
