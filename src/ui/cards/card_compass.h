#ifndef CARD_COMPASS_H
#define CARD_COMPASS_H

#include <stdbool.h>

#include "lvgl/lvgl.h"
#include "../core/vm/ui_vm_dashboard_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void card_compass_create(lv_obj_t *parent);
void card_compass_update(void);
void card_compass_refresh_heading_vm(const ui_vm_compass_t *vm, bool force_refresh);
void card_compass_refresh_heading(bool force_refresh);

#ifdef __cplusplus
}
#endif

#endif /* CARD_COMPASS_H */
