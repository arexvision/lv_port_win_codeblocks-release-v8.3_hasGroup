#ifndef UI_LOGBOOK_IO_PORT_H
#define UI_LOGBOOK_IO_PORT_H

#include "../core/ui_types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 记录键由平台实现生成；UI 只保存并原样回传，不解释其内容。 */
typedef uint32_t logbook_record_key_t;

#define LOGBOOK_INVALID_RECORD_KEY ((logbook_record_key_t)0U)

typedef enum
{
    LOGBOOK_IO_READY = 0,
    LOGBOOK_IO_PROFILE_UNAVAILABLE,
    LOGBOOK_IO_FAILED,
} logbook_io_status_t;

typedef struct
{
    uint32_t request_id;
    logbook_record_key_t record_key;
    logbook_io_status_t status;
    logbook_entry_t entry;
    const dive_pt_t *points;
    uint16_t point_count;
} logbook_io_result_t;

bool logbook_io_get_summary(uint16_t index,
                            logbook_entry_t *out_entry,
                            logbook_record_key_t *out_record_key);

bool logbook_io_request_detail(logbook_record_key_t record_key,
                               uint32_t *out_request_id);
bool logbook_io_take_detail(uint32_t request_id, logbook_io_result_t *out_result);
void logbook_io_cancel_detail(uint32_t request_id);

void logbook_io_release_points(const dive_pt_t *points);

#ifdef __cplusplus
}
#endif

#endif /* UI_LOGBOOK_IO_PORT_H */
