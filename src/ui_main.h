#ifndef UI_MAIN_H
#define UI_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ui_main.h
 * @brief UI entry point and simulation HAL
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

#endif /* UI_MAIN_H */
