#ifndef AREX_UI_MAIN_H
#define AREX_UI_MAIN_H

/**
 * @file UI_main.h
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

#endif /* AREX_UI_MAIN_H */
