#ifndef COMP_UPDATE_H
#define COMP_UPDATE_H

#include "arex_ui_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

void comp_set_value(comp_id_t id, float value);
void comp_set_text(comp_id_t id, const char *text);
void comp_sync_data(comp_id_t w_id);

#ifdef __cplusplus
}
#endif

#endif /* COMP_UPDATE_H */
