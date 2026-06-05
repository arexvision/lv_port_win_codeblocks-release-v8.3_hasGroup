#ifndef AREX_DECO_STATUS_H
#define AREX_DECO_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ArexDecoStatus {
    AREX_DECO_STATUS_OK = 0,
    AREX_DECO_STATUS_INVALID_ARGUMENT = 1,
    AREX_DECO_STATUS_UNSUPPORTED_VERSION = 2,
    AREX_DECO_STATUS_INSUFFICIENT_CAPACITY = 3,
    AREX_DECO_STATUS_INVALID_STATE = 4,
    AREX_DECO_STATUS_NOT_IMPLEMENTED = 5
} ArexDecoStatus;

#ifdef __cplusplus
}
#endif

#endif
