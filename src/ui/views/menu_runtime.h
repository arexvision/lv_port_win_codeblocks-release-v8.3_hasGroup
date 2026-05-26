#ifndef MENU_RUNTIME_H
#define MENU_RUNTIME_H

#include "menu_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

void menu_runtime_reset(void);
bool menu_runtime_open_info(uint8_t index);
bool menu_runtime_open_setup(uint8_t index);
bool menu_runtime_open_child(menu_id_t child_id, menu_item_id_t source_item);
bool menu_runtime_back(void);
void menu_runtime_refresh(void);

menu_id_t menu_runtime_current_id(void);
const char *menu_runtime_current_title(void);
const menu_row_t *menu_runtime_current_rows(uint8_t *out_count);
const menu_row_t *menu_runtime_row_at(uint8_t index);
bool menu_runtime_is_dive_plan(void);
bool menu_runtime_is_dive_plan_result(void);
bool menu_runtime_is_nested(void);
uint8_t menu_runtime_default_selection(void);

#ifdef __cplusplus
}
#endif

#endif /* MENU_RUNTIME_H */
