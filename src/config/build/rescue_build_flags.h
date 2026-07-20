#ifndef RESCUE_BUILD_FLAGS_H
#define RESCUE_BUILD_FLAGS_H

/* Rescue/error-screen standalone build switch. Keep disabled in normal UI builds. */
#ifndef ERROR_SCREEN_FORCE_SHOW
#define ERROR_SCREEN_FORCE_SHOW 0
#endif

#if (ERROR_SCREEN_FORCE_SHOW != 0) && (ERROR_SCREEN_FORCE_SHOW != 1)
#error "ERROR_SCREEN_FORCE_SHOW must be 0 or 1"
#endif

#endif /* RESCUE_BUILD_FLAGS_H */
