#ifndef SUBMENU_DIVE_PLAN_STATE_H
#define SUBMENU_DIVE_PLAN_STATE_H

#include "submenu_dive_plan_types.h"
#include "menu_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

void submenu_dive_plan_get_snapshot(submenu_dive_plan_snapshot_t *out_snapshot);
void submenu_dive_plan_reset(void);
void submenu_dive_plan_set_result_snapshot(const dive_plan_result_snapshot_t *snapshot);
bool submenu_dive_plan_handle_rotate(int8_t dir);
bool submenu_dive_plan_is_result_page(void);
bool submenu_dive_plan_handle_action(menu_item_id_t item_id,
                                     bool *out_close_submenu,
                                     uint8_t *out_keep_index);

void submenu_dive_plan_set_page(dive_plan_page_t page);
dive_plan_page_t submenu_dive_plan_get_page(void);
uint8_t submenu_dive_plan_get_result_page_index(void);
uint8_t submenu_dive_plan_get_result_total_pages(void);
void submenu_dive_plan_set_depth_m(float value);
void submenu_dive_plan_set_time_min(float value);
void submenu_dive_plan_set_rmv_lpm(float value);

#ifdef __cplusplus
}
#endif

#endif /* SUBMENU_DIVE_PLAN_STATE_H */
