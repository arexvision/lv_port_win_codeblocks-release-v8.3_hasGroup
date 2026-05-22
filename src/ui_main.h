#ifndef AREX_UI_MAIN_H
#define AREX_UI_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "arex_hal_sim/arex_input_pc.h"
#define PC_SIMULATOR  //移植硬件后需要注释

/**
 * @file ui_main.h
 * @brief AREX UI entry point & simulation HAL
 *
 * Hardware layer entry point. Called once from WinMain() after lv_init() + HAL init.
 * Manages:
 *   - UI engine init (config defaults, data bus zero)
 *   - Screen creation (safe zone, left anchor, cards)
 *   - Input init (PC keyboard/encoder)
 *   - UI consumer task (50ms lv_timer)
 *   - Simulation tick task (1Hz lv_timer)
 */

/* Forward declare — no heavy includes here, keep header lightweight */
void UI_main(void);

#ifdef __cplusplus
}
#endif

#endif /* AREX_UI_MAIN_H */
