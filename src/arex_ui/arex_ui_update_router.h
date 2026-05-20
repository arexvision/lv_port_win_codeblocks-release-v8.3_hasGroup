#ifndef AREX_UI_UPDATE_ROUTER_H
#define AREX_UI_UPDATE_ROUTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void arex_ui_update_router_tick(void);
void arex_ui_update_router_dispatch(uint32_t dirty_mask);

#ifdef __cplusplus
}
#endif

#endif /* AREX_UI_UPDATE_ROUTER_H */
