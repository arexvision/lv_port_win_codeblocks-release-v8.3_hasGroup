#ifndef ALARM_VIEW_H
#define ALARM_VIEW_H

#include "../core/ui_engine.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    lv_obj_t *safe_zone;
    lv_obj_t *left_anchor;
    lv_obj_t **custom_cards;
    uint8_t custom_card_count;
    uint8_t max_custom_cards;
    uint8_t layout_order;
    uint16_t safe_zone_w;
    uint16_t left_anchor_w;
    uint16_t panel_gap_px;
    bool *alarm_pending_click;
} alarm_view_context_t;

void alarm_view_tick(const alarm_view_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* ALARM_VIEW_H */
