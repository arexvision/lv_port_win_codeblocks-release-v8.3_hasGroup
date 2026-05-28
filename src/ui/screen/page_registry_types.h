/*
 * 文件: src/app_ui/ui/screen/page_registry_types.h
 * 作用: 该文件属于屏幕或页面编排模块，负责整屏布局、分页切换、覆盖层、编辑态显示或页面注册管理。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#ifndef PAGE_REGISTRY_TYPES_H
#define PAGE_REGISTRY_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const char *title_text;
    const char *value_badge;
    uint8_t     title_font_id;
    uint8_t     value_font_id;
    uint8_t     border_width;
    uint8_t     height_u;
} menu_item_cfg_t;

typedef struct
{
    const menu_item_cfg_t *items;
    uint8_t                count;
} menu_list_cfg_t;

typedef enum
{
    PAGE_ID_INFO        = 0,
    PAGE_ID_COMPASS     = 1,
    PAGE_ID_DECO        = 2,
    PAGE_ID_GAS         = 3,
    PAGE_ID_PLAN        = 4,
    PAGE_ID_CUSTOM_GRID = 5,
    PAGE_ID_BLANK       = 6,
    PAGE_ID_SETUP       = 7,
    PAGE_ID_COUNT
} page_id_t;

#define PAGE_ID_UNUSED 0xFF

#define MAX_DYNAMIC_SLOTS 12

typedef enum
{
    PAGE_POS_INFO          = 0,
    PAGE_POS_DYNAMIC_FIRST = 1,
    PAGE_POS_SETUP         = PAGE_POS_DYNAMIC_FIRST + MAX_DYNAMIC_SLOTS,
    PAGE_POS_COUNT
} page_pos_t;

#define PAGE_POS_1   (PAGE_POS_DYNAMIC_FIRST + 0)
#define PAGE_POS_2   (PAGE_POS_DYNAMIC_FIRST + 1)
#define PAGE_POS_3   (PAGE_POS_DYNAMIC_FIRST + 2)
#define PAGE_POS_4   (PAGE_POS_DYNAMIC_FIRST + 3)
#define PAGE_POS_5   (PAGE_POS_DYNAMIC_FIRST + 4)
#define PAGE_POS_6   (PAGE_POS_DYNAMIC_FIRST + 5)
#define PAGE_POS_7   (PAGE_POS_DYNAMIC_FIRST + 6)
#define PAGE_POS_8   (PAGE_POS_DYNAMIC_FIRST + 7)
#define PAGE_POS_9   (PAGE_POS_DYNAMIC_FIRST + 8)
#define PAGE_POS_10  (PAGE_POS_DYNAMIC_FIRST + 9)
#define PAGE_POS_11  (PAGE_POS_DYNAMIC_FIRST + 10)
#define PAGE_POS_12  (PAGE_POS_DYNAMIC_FIRST + 11)

#define PAGE_COUNT      PAGE_POS_COUNT
#define DASH_PAGE_COUNT MAX_DYNAMIC_SLOTS

#ifdef __cplusplus
}
#endif

#endif /* PAGE_REGISTRY_TYPES_H */
