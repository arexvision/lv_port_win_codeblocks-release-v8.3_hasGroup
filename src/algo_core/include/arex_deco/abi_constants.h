#ifndef AREX_DECO_ABI_CONSTANTS_H
#define AREX_DECO_ABI_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

// API version and fixed ABI capacities. These values directly constrain
// public struct layout, static array sizes, and cross-language bindings.
#define AREX_DECO_API_VERSION_MAJOR 0
#define AREX_DECO_API_VERSION_MINOR 0
#define AREX_DECO_API_VERSION_PATCH 15

#define AREX_DECO_COMPARTMENT_COUNT 16
#define AREX_DECO_MAX_GAS_COUNT 6
#define AREX_DECO_MAX_DECO_STOP_COUNT 40

#ifdef __cplusplus
}
#endif

#endif
