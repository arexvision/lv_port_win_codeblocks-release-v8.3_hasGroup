#ifndef UI_TEST_H
#define UI_TEST_H

#include <stdbool.h>
#include "ui_test_flags.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(UI_TEST_IMPLEMENTATION) && !UI_LVGL_PAGE_STRESS_TEST_ENABLED && !UI_OPTICAL_GHOST_TEST_ENABLED
static inline bool ui_test_try_start(void)
{
    return false;
}
#else
bool ui_test_try_start(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* UI_TEST_H */
