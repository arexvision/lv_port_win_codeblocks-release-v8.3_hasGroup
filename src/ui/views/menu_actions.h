#ifndef MENU_ACTIONS_H
#define MENU_ACTIONS_H

#include "menu_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

bool menu_actions_handle_select(uint8_t row_index,
                                const menu_row_t *row,
                                menu_action_t *out_action);
void menu_actions_clear_pending(void);
bool menu_actions_confirm_pending(bool *out_close_parent_too,
                                  bool *out_return_dash);

#ifdef __cplusplus
}
#endif

#endif /* MENU_ACTIONS_H */
