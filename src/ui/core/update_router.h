#ifndef UI_UPDATE_ROUTER_H
#define UI_UPDATE_ROUTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_update_router_dispatch(uint32_t dirty_mask);

#ifdef __cplusplus
}
#endif

#endif /* UI_UPDATE_ROUTER_H */
