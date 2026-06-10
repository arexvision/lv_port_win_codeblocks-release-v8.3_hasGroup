#ifndef UI_TEST_FLAGS_H
#define UI_TEST_FLAGS_H

/*
 * UI-only test switches.
 *
 * Keep these switches local to app_ui so temporary optical/font/display
 * diagnostics do not leak into app/service/driver feature flags.
 *
 * 1: boot directly into the optical ghosting test pattern.
 * 0: boot normal production UI.
 */
#define UI_OPTICAL_GHOST_TEST_ENABLED 0

/*
 * LVGL page update stress test.
 *
 * Purpose:
 * - Simulate 10 dashboard pages with repeated widgets.
 * - Compare "update active page only" against "update all hidden pages too".
 * - Use existing [LVGL_PERF] and [LCD_FLUSH] aggregate logs as the result.
 *
 * Usage:
 * - Set UI_LVGL_PAGE_STRESS_TEST_ENABLED to 1 and rebuild.
 * - Keep UI_LVGL_PAGE_STRESS_UPDATE_ALL_PAGES at 0 for the optimized path.
 * - Set UI_LVGL_PAGE_STRESS_UPDATE_ALL_PAGES to 1 to reproduce the old
 *   worst case where hidden pages are still updated.
 */
#define UI_LVGL_PAGE_STRESS_TEST_ENABLED 0
#define UI_LVGL_PAGE_STRESS_UPDATE_ALL_PAGES 0
#define UI_LVGL_PAGE_STRESS_FORCE_FULL_INVALIDATE 0

#define UI_LVGL_PAGE_STRESS_PAGE_COUNT 10
#define UI_LVGL_PAGE_STRESS_ROWS_PER_PAGE 12
#define UI_LVGL_PAGE_STRESS_UPDATE_PERIOD_MS 100
#define UI_LVGL_PAGE_STRESS_SWITCH_PERIOD_MS 3000

#if (UI_OPTICAL_GHOST_TEST_ENABLED != 0) && (UI_OPTICAL_GHOST_TEST_ENABLED != 1)
#error "UI_OPTICAL_GHOST_TEST_ENABLED must be 0 or 1"
#endif

#if (UI_LVGL_PAGE_STRESS_TEST_ENABLED != 0) && (UI_LVGL_PAGE_STRESS_TEST_ENABLED != 1)
#error "UI_LVGL_PAGE_STRESS_TEST_ENABLED must be 0 or 1"
#endif

#if (UI_LVGL_PAGE_STRESS_UPDATE_ALL_PAGES != 0) && (UI_LVGL_PAGE_STRESS_UPDATE_ALL_PAGES != 1)
#error "UI_LVGL_PAGE_STRESS_UPDATE_ALL_PAGES must be 0 or 1"
#endif

#if (UI_LVGL_PAGE_STRESS_FORCE_FULL_INVALIDATE != 0) && (UI_LVGL_PAGE_STRESS_FORCE_FULL_INVALIDATE != 1)
#error "UI_LVGL_PAGE_STRESS_FORCE_FULL_INVALIDATE must be 0 or 1"
#endif

#if UI_LVGL_PAGE_STRESS_PAGE_COUNT < 2
#error "UI_LVGL_PAGE_STRESS_PAGE_COUNT must be at least 2"
#endif

#if UI_LVGL_PAGE_STRESS_ROWS_PER_PAGE < 1
#error "UI_LVGL_PAGE_STRESS_ROWS_PER_PAGE must be at least 1"
#endif

#endif /* UI_TEST_FLAGS_H */
