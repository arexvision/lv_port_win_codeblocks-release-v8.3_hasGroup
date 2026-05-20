#ifndef AREX_WIDGET_UPDATE_H
#define AREX_WIDGET_UPDATE_H

#include "arex_ui_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

void arex_widget_set_value(arex_widget_id_t id, float value);
void arex_widget_set_text(arex_widget_id_t id, const char *text);
void arex_widget_sync_data(arex_widget_id_t w_id);

#ifdef __cplusplus
}
#endif

#endif /* AREX_WIDGET_UPDATE_H */
