/*
 * 文件: src/app_ui/ui/comp/comp_view.h
 * 作用: 该文件属于公共组件模块，负责复用样式、通用控件、局部刷新逻辑或组件级显示封装。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时应重点检查布局尺寸、刷新频率与复用接口是否一致，避免局部样式或坐标调整影响同类页面的对齐和更新节奏。
 */

#ifndef COMP_VIEW_H
#define COMP_VIEW_H

#include "../core/ui_engine.h"
#include "../core/vm/ui_vm_dashboard_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void reset_widget_render_state(void);

/* 根据组件 ID 创建对应的可视对象，并按布局参数放置到父容器内。 */
lv_obj_t *render_widget_by_id(lv_obj_t *parent,
                              comp_id_t w_id,
                              int16_t abs_x, int16_t abs_y,
                              uint16_t abs_w, uint16_t abs_h,
                              uint8_t span_w, uint8_t span_h,
                              font_id_t cfg_font_id);

/* 下列刷新接口只负责把数据灌进现有组件，不负责重新创建布局。 */
void comp_refresh_sys(dirty_mask_t dirty_mask);
void comp_refresh_ndl_stop_vm(const ui_vm_ndl_stop_t *vm, dirty_mask_t dirty_mask);
void comp_refresh_ndl_stop(dirty_mask_t dirty_mask);
void comp_refresh_ascent_icons(const ui_vm_ascent_t *vm);
void comp_refresh_tissue_widgets(const ui_vm_deco_t *vm, dirty_mask_t dirty_mask);

#ifdef __cplusplus
}
#endif

#endif /* COMP_VIEW_H */
