#ifndef ERROR_SCREEN_H
#define ERROR_SCREEN_H

#include <stdbool.h>

#ifndef ERROR_SCREEN_FORCE_SHOW
#define ERROR_SCREEN_FORCE_SHOW 0
#endif

#define ERROR_SCREEN_TITLE "ERROR"
#define ERROR_SCREEN_MESSAGE "SYSTEM ERROR"

#if (ERROR_SCREEN_FORCE_SHOW != 0) && (ERROR_SCREEN_FORCE_SHOW != 1)
#error "ERROR_SCREEN_FORCE_SHOW must be 0 or 1"
#endif

void error_screen_set_boot_error(bool active);
bool error_screen_has_boot_error(void);
bool error_screen_try_start(void);

#endif /* ERROR_SCREEN_H */
