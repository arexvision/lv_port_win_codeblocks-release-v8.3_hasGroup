/*
 * 文件: src/app_ui/ui/views/submenu_model.h
 * 作用: 该文件属于视图表现模块，负责菜单、子菜单、模态框或特定视图状态的展示与交互联动。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef SUBMENU_MODEL_H
#define SUBMENU_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "menu_defs.h"
#include "submenu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *submenu_info_title(uint8_t index);
/* 根据信息页索引生成标题和条目内容。 */
const char **submenu_build_info_items(uint8_t index, uint8_t *out_count);

const char *submenu_setup_title(uint8_t index);
/* SETUP 菜单的标题和条目都是按索引动态派生的。 */
int8_t submenu_setup_index_for_title(const char *title);
const char **submenu_build_setup_items(uint8_t index, uint8_t *out_count);
const setting_option_t *submenu_conservatism_option(uint8_t index);
const char *submenu_conservatism_badge(uint8_t level);
const brightness_option_t *submenu_brightness_option(uint8_t index);
const char *submenu_brightness_badge(uint8_t level);
uint8_t submenu_brightness_visible_opa(uint8_t level);

const char **submenu_build_compass_cal_items(uint8_t *out_count);
const char **submenu_nested_items_for(const char *title, uint8_t *out_count);
const char **submenu_child_items_for(const char *current_title,
                                          uint8_t item_index,
                                          const char *item_text,
                                          char *out_title,
                                          uint8_t out_title_size,
                                          uint8_t *out_count);

bool submenu_is_readonly_info_title(const char *title);
/* 这些函数把菜单文本/ID 映射成设置项、编辑项或确认项。 */
bool submenu_setting_from_selection(const char *current_title,
                                         uint8_t item_index,
                                         const char *item_text,
                                         submenu_setting_confirm_t *out_setting);
bool submenu_direct_setting_from_selection(const char *current_title,
                                                uint8_t item_index,
                                                const char *item_text,
                                                submenu_setting_confirm_t *out_setting);
bool submenu_edit_spec_from_selection(const char *current_title,
                                           uint8_t item_index,
                                           const char *item_text,
                                           submenu_edit_spec_t *out_spec);
bool submenu_setting_from_ids(menu_id_t current_menu,
                              menu_item_id_t item_id,
                              submenu_setting_confirm_t *out_setting);
bool submenu_direct_setting_from_ids(menu_id_t current_menu,
                                     menu_item_id_t item_id,
                                     submenu_setting_confirm_t *out_setting);
bool submenu_edit_spec_from_ids(menu_id_t current_menu,
                                menu_item_id_t item_id,
                                submenu_edit_spec_t *out_spec);
void submenu_prepare_oc_tech_child(menu_item_id_t item_id,
                                   char *out_title,
                                   uint8_t out_title_size);
/* 应用设置结果时统一落到数据层或业务回调。 */
void submenu_apply_setting(submenu_setting_kind_t kind, uint8_t arg, uint16_t value);
void submenu_apply_edit_value(submenu_setting_kind_t kind, uint8_t arg, float value);
uint8_t submenu_safety_stop_depth_m(uint8_t value);
void submenu_sync_persisted_settings(void);
#ifdef __cplusplus
}
#endif

#endif /* SUBMENU_MODEL_H */
