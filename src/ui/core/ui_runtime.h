#ifndef UI_RUNTIME_H
#define UI_RUNTIME_H

#include "ui_defs.h"
#include "ui_types.h"
#include "../comp/comp_style_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_init(void);
void ui_apply_config(void);
void ui_update_data(void);
void sys_config_defaults(sys_config_t *cfg);
lv_text_align_t align_to_lv(uint8_t align);
lv_align_t align_to_lv_align(uint8_t align);
const lv_font_t *get_font(uint8_t font_id);
void trigger_alarm(alarm_level_t level, const char *eng_text, comp_id_t target_id);
void clear_all_alarm_styles(void);
bool alarm_mark_clear_requested(void);
const char *comp_get_name(comp_id_t id);
void ui_update_task(lv_timer_t *timer);

extern lv_obj_t *g_left_anchor_obj;
extern lv_obj_t *g_card_custom_objs[MAX_CUSTOM_CARDS];
extern uint8_t   g_card_custom_obj_count;

#ifdef __cplusplus
}
#endif

#endif /* UI_RUNTIME_H */
