#ifndef OTA_UPDATE_VIEW_H
#define OTA_UPDATE_VIEW_H

#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t active;
    uint8_t phase;
    uint8_t progress_pct;
    uint16_t error_code;
    uint32_t detail;
    char reason[64];
} ota_update_view_status_t;

void ota_update_view_refresh(const ota_update_view_status_t *status);
void ota_update_view_hide(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_UPDATE_VIEW_H */
