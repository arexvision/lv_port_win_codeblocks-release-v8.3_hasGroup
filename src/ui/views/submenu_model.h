#ifndef SUBMENU_MODEL_H
#define SUBMENU_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "menu_defs.h"
#include "submenu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *submenu_info_title(uint8_t index);
const char **submenu_build_info_items(uint8_t index, uint8_t *out_count);

const char *submenu_setup_title(uint8_t index);
int8_t submenu_setup_index_for_title(const char *title);
const char **submenu_build_setup_items(uint8_t index, uint8_t *out_count);
const setting_option_t *submenu_conservatism_option(uint8_t index);
const char *submenu_conservatism_badge(uint8_t level);
const brightness_option_t *submenu_brightness_option(uint8_t index);
const char *submenu_brightness_badge(uint8_t level);
uint8_t submenu_brightness_visible_opa(uint8_t level);

const char **submenu_build_compass_cal_items(uint8_t *out_count);
const char **submenu_nested_items_for(const char *title, uint8_t *out_count);
const char **submenu_child_items_for(const char *current_title,
                                          uint8_t item_index,
                                          const char *item_text,
                                          char *out_title,
                                          uint8_t out_title_size,
                                          uint8_t *out_count);

bool submenu_is_readonly_info_title(const char *title);
bool submenu_setting_from_selection(const char *current_title,
                                         uint8_t item_index,
                                         const char *item_text,
                                         submenu_setting_confirm_t *out_setting);
bool submenu_direct_setting_from_selection(const char *current_title,
                                                uint8_t item_index,
                                                const char *item_text,
                                                submenu_setting_confirm_t *out_setting);
bool submenu_edit_spec_from_selection(const char *current_title,
                                           uint8_t item_index,
                                           const char *item_text,
                                           submenu_edit_spec_t *out_spec);
bool submenu_setting_from_ids(menu_id_t current_menu,
                              menu_item_id_t item_id,
                              submenu_setting_confirm_t *out_setting);
bool submenu_direct_setting_from_ids(menu_id_t current_menu,
                                     menu_item_id_t item_id,
                                     submenu_setting_confirm_t *out_setting);
bool submenu_edit_spec_from_ids(menu_id_t current_menu,
                                menu_item_id_t item_id,
                                submenu_edit_spec_t *out_spec);
void submenu_prepare_oc_tech_child(menu_item_id_t item_id,
                                   char *out_title,
                                   uint8_t out_title_size);
void submenu_apply_setting(submenu_setting_kind_t kind, uint8_t arg, uint16_t value);
void submenu_apply_edit_value(submenu_setting_kind_t kind, uint8_t arg, float value);
uint8_t submenu_safety_stop_depth_m(uint8_t value);
#ifdef __cplusplus
}
#endif

#endif /* SUBMENU_MODEL_H */
