#include "ota_update_view.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define OTA_VIEW_SPINNER_MS 16U
#define OTA_VIEW_SPINNER_STEP_DEG 4U
#define OTA_VIEW_W          454
#define OTA_VIEW_H          454

static lv_obj_t *s_ota_layer;
static lv_obj_t *s_ota_arc;
static lv_obj_t *s_ota_title;
static lv_obj_t *s_ota_stage;
static lv_obj_t *s_ota_percent;
static lv_obj_t *s_ota_bar;
static lv_obj_t *s_ota_detail;
static lv_timer_t *s_ota_timer;
static uint16_t s_ota_spin_deg;
static uint8_t s_ota_visible;
static uint8_t s_last_phase = 0xFFU;
static uint8_t s_last_progress = 0xFFU;
static uint16_t s_last_error_code = 0xFFFFU;
static uint32_t s_last_detail = 0xFFFFFFFFUL;
static char s_last_reason[64];

static const char *ota_view_phase_text(uint8_t phase)
{
    switch (phase)
    {
    case 1: return "PREPARING";
    case 2: return "RECEIVING";
    case 3: return "VERIFYING";
    case 4: return "INSTALLING";
    case 5: return "REBOOTING";
    case 6: return "OTA ERROR";
    default: return "WAITING";
    }
}

static lv_color_t ota_view_phase_color(uint8_t phase)
{
    if (phase == 6U) {
        return lv_color_make(255, 80, 64);
    }
    if (phase == 5U) {
        return lv_color_make(64, 220, 180);
    }
    return lv_color_make(0, 220, 120);
}

static void ota_view_spinner_cb(lv_timer_t *timer)
{
    (void)timer;

    if ((s_ota_arc == NULL) || (s_ota_visible == 0U)) {
        return;
    }

    s_ota_spin_deg = (uint16_t)((s_ota_spin_deg + OTA_VIEW_SPINNER_STEP_DEG) % 360U);
    lv_arc_set_angles(s_ota_arc, s_ota_spin_deg, (uint16_t)(s_ota_spin_deg + 96U));
}

static lv_obj_t *ota_view_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return label;
}

static void ota_view_create(void)
{
    if (s_ota_layer != NULL && lv_obj_is_valid(s_ota_layer)) {
        return;
    }

    s_ota_layer = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_ota_layer);
    lv_obj_set_size(s_ota_layer, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_ota_layer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ota_layer, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_ota_layer, 0, 0);
    lv_obj_clear_flag(s_ota_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_ota_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ota_layer, LV_OBJ_FLAG_IGNORE_LAYOUT);

    s_ota_arc = lv_arc_create(s_ota_layer);
    lv_obj_set_size(s_ota_arc, 156, 156);
    lv_obj_align(s_ota_arc, LV_ALIGN_CENTER, 0, -82);
    lv_arc_set_bg_angles(s_ota_arc, 0, 360);
    lv_arc_set_angles(s_ota_arc, 0, 96);
    lv_obj_remove_style(s_ota_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_ota_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s_ota_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_ota_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_ota_arc, lv_color_make(24, 40, 36), LV_PART_MAIN);

    s_ota_percent = ota_view_label(s_ota_layer, LV_FONT_DEFAULT, lv_color_white());
    lv_obj_set_width(s_ota_percent, 132);
    lv_obj_align(s_ota_percent, LV_ALIGN_CENTER, 0, -82);

    s_ota_title = ota_view_label(s_ota_layer, LV_FONT_DEFAULT, lv_color_white());
    lv_obj_set_width(s_ota_title, OTA_VIEW_W - 96);
    lv_obj_align(s_ota_title, LV_ALIGN_CENTER, 0, 26);

    s_ota_stage = ota_view_label(s_ota_layer, LV_FONT_DEFAULT, lv_color_make(0, 220, 120));
    lv_obj_set_width(s_ota_stage, OTA_VIEW_W - 110);
    lv_obj_align(s_ota_stage, LV_ALIGN_CENTER, 0, 58);

    s_ota_bar = lv_bar_create(s_ota_layer);
    lv_obj_set_size(s_ota_bar, 292, 10);
    lv_obj_align(s_ota_bar, LV_ALIGN_CENTER, 0, 94);
    lv_bar_set_range(s_ota_bar, 0, 100);
    lv_obj_set_style_radius(s_ota_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_ota_bar, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_ota_bar, lv_color_make(28, 36, 34), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_ota_bar, LV_OPA_COVER, LV_PART_MAIN);

    s_ota_detail = ota_view_label(s_ota_layer, LV_FONT_DEFAULT, lv_color_make(156, 168, 164));
    lv_obj_set_width(s_ota_detail, OTA_VIEW_W - 116);
    lv_obj_align(s_ota_detail, LV_ALIGN_CENTER, 0, 128);

    s_ota_timer = lv_timer_create(ota_view_spinner_cb, OTA_VIEW_SPINNER_MS, NULL);
    if (s_ota_timer != NULL) {
        lv_timer_pause(s_ota_timer);
    }
}

void ota_update_view_refresh(const ota_update_view_status_t *status)
{
    char text[64];
    lv_color_t color;
    uint8_t progress;
    bool content_changed;

    if (status == NULL || status->active == 0U) {
        ota_update_view_hide();
        return;
    }

    ota_view_create();
    if (s_ota_layer == NULL) {
        return;
    }

    progress = (status->progress_pct > 100U) ? 100U : status->progress_pct;
    color = ota_view_phase_color(status->phase);
    const bool becoming_visible = (s_ota_visible == 0U);
    content_changed =
        (s_last_phase != status->phase) ||
        (s_last_progress != progress) ||
        (s_last_error_code != status->error_code) ||
        (s_last_detail != status->detail) ||
        (strncmp(s_last_reason, status->reason, sizeof(s_last_reason)) != 0);

    if (becoming_visible) {
        s_ota_visible = 1U;
        lv_obj_clear_flag(s_ota_layer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_ota_layer);
        if (s_ota_timer != NULL) {
            lv_timer_resume(s_ota_timer);
        }
    }

    if (!content_changed) {
        return;
    }

    s_last_phase = status->phase;
    s_last_progress = progress;
    s_last_error_code = status->error_code;
    s_last_detail = status->detail;
    (void)strncpy(s_last_reason, status->reason, sizeof(s_last_reason) - 1U);
    s_last_reason[sizeof(s_last_reason) - 1U] = '\0';

    lv_obj_set_style_arc_color(s_ota_arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_ota_bar, color, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(s_ota_stage, color, 0);

    (void)snprintf(text, sizeof(text), "%u%%", (unsigned)progress);
    lv_label_set_text(s_ota_percent, text);
    lv_label_set_text(s_ota_title, "OTA UPDATE");
    lv_label_set_text(s_ota_stage, ota_view_phase_text(status->phase));
    lv_bar_set_value(s_ota_bar, progress, LV_ANIM_OFF);

    if (status->phase == 6U) {
        char suffix[32];
        uint16_t current = (uint16_t)((status->detail >> 16) & 0xFFFFU);
        uint16_t total = (uint16_t)(status->detail & 0xFFFFU);

        if (total != 0U) {
            (void)snprintf(suffix, sizeof(suffix), " P%u/%u", (unsigned)current, (unsigned)total);
        } else if (status->detail != 0U) {
            (void)snprintf(suffix, sizeof(suffix), " 0x%08lX", (unsigned long)status->detail);
        } else {
            suffix[0] = '\0';
        }

        if (status->reason[0] != '\0') {
            (void)snprintf(text,
                           sizeof(text),
                           "%s\nERR %u%s",
                           status->reason,
                           (unsigned)status->error_code,
                           suffix);
        } else {
            (void)snprintf(text,
                           sizeof(text),
                           "ERR %u%s",
                           (unsigned)status->error_code,
                           suffix);
        }
    } else if (status->detail != 0U) {
        uint16_t current = (uint16_t)((status->detail >> 16) & 0xFFFFU);
        uint16_t total = (uint16_t)(status->detail & 0xFFFFU);
        if (total != 0U) {
            (void)snprintf(text, sizeof(text), "PKT %u/%u", (unsigned)current, (unsigned)total);
        } else {
            (void)snprintf(text, sizeof(text), "PKT %lu", (unsigned long)status->detail);
        }
    } else {
        text[0] = '\0';
    }
    lv_label_set_text(s_ota_detail, text);
}

void ota_update_view_hide(void)
{
    s_ota_visible = 0U;
    s_last_phase = 0xFFU;
    s_last_progress = 0xFFU;
    s_last_error_code = 0xFFFFU;
    s_last_detail = 0xFFFFFFFFUL;
    s_last_reason[0] = '\0';
    if (s_ota_timer != NULL) {
        lv_timer_pause(s_ota_timer);
    }
    if (s_ota_layer != NULL && lv_obj_is_valid(s_ota_layer)) {
        lv_obj_add_flag(s_ota_layer, LV_OBJ_FLAG_HIDDEN);
    }
}
