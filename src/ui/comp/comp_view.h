#ifndef COMP_VIEW_H
#define COMP_VIEW_H

#include "../core/ui_engine.h"
#include "../core/vm/ui_vm_dashboard_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void reset_widget_render_state(void);

lv_obj_t *render_widget_by_id(lv_obj_t *parent,
                              comp_id_t w_id,
                              int16_t abs_x, int16_t abs_y,
                              uint16_t abs_w, uint16_t abs_h,
                              uint8_t span_w, uint8_t span_h,
                              font_id_t cfg_font_id);

void comp_refresh_sys_vm(const ui_vm_sys_t *vm, uint32_t dirty_mask);
void comp_refresh_sys(uint32_t dirty_mask);
void comp_refresh_ndl_stop_vm(const ui_vm_ndl_stop_t *vm, uint32_t dirty_mask);
void comp_refresh_ndl_stop(uint32_t dirty_mask);
void comp_refresh_ascent_icons(const ui_vm_ascent_t *vm);

#ifdef __cplusplus
}
#endif

#endif /* COMP_VIEW_H */
