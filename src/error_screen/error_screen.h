#ifndef ERROR_SCREEN_H
#define ERROR_SCREEN_H

#include <stdbool.h>

#define ERROR_SCREEN_ENABLED 0
#define ERROR_SCREEN_TITLE "ERROR"
#define ERROR_SCREEN_MESSAGE "SYSTEM ERROR"

#if (ERROR_SCREEN_ENABLED != 0) && (ERROR_SCREEN_ENABLED != 1)
#error "ERROR_SCREEN_ENABLED must be 0 or 1"
#endif

bool error_screen_try_start(void);

#endif /* ERROR_SCREEN_H */
