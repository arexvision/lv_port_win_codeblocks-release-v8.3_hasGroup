#ifndef UI_ENGINE_H
#define UI_ENGINE_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#include "ui_runtime.h"
#include "data.h"
#include "ui_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PC_SIMULATOR
#define SYSTEM_VERSION  "20260101.01"
#else
#include "../../config/app_version_auto.h"
#define SYSTEM_VERSION  APP_VERSION_SEMVER
#endif

extern uint8_t g_sys_page_order(uint8_t pos);
#define g_sys_card_order g_sys_page_order

#ifdef __cplusplus
}
#endif

#endif /* UI_ENGINE_H */
