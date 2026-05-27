#ifndef UI_VM_INFO_TYPES_H
#define UI_VM_INFO_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t count;
    char lines[6][32];
} ui_vm_info_page_t;

#ifdef __cplusplus
}
#endif

#endif /* UI_VM_INFO_TYPES_H */
