#ifndef SCREEN_EDIT_H
#define SCREEN_EDIT_H

#include "screen_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

void screen_refresh_edit_value(void);
void screen_begin_edit_value(uint8_t item_idx, const submenu_edit_spec_t *spec);
void screen_commit_edit_value(void);
void screen_cancel_edit_value(void);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_EDIT_H */
