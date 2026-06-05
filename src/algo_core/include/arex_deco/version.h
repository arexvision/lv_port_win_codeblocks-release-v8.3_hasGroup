#ifndef AREX_DECO_VERSION_H
#define AREX_DECO_VERSION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ArexDecoVersion {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
} ArexDecoVersion;

ArexDecoVersion arex_deco_get_api_version(void);

#ifdef __cplusplus
}
#endif

#endif
