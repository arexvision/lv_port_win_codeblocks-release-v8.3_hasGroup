# 2026-05-03 代码修改同步记录

## 修改文件总览

| 文件 | 修改类型 | 说明 |
|------|----------|------|
| `src/arex_ui/arex_ui_state.c` | LOG删除 | 移除所有 LOG 宏调用 |
| `src/arex_ui/arex_ui_engine.c` | 清理 | 删除未使用的变量和函数 |
| `src/arex_ui/arex_screen.c` | 多项修复 | LOG删除、函数声明、dots数量修复 |
| `src/arex_ui/arex_ui_engine.h` | 清理 | 移除重复注释 |

---

## 1. arex_ui_state.c

**移除的头部：**
```c
#define LOG_TAG "arex_ui_state"
#include "log.h"
```

**arex_ui_go_to_card() 简化：**
```c
void arex_ui_go_to_card(uint8_t tile_pos)
{
    g_ui.dash_card = tile_pos;
    arex_screen_scroll_to_card(tile_pos);
}
```

**ui_handle_rotate() 移除的 LOG：**
- `LOG_I("[ROTATE] state=DASH dir=%d...")`
- `LOG_I("[ROTATE] hit_top_wall charge=%u")`
- `LOG_I("[ROTATE] hit_bottom_wall charge=%u")`
- `LOG_I("[ROTATE] advance dir=%d...")`

---

## 2. arex_ui_engine.c

**删除的未使用代码：**
```c
// 删除前
#define MAX_WIDGET_HANDLES 16
#define MAX_WIDGETS  41
static lv_obj_t *s_widget_handles[MAX_WIDGETS];
static uint8_t   s_widget_handle_count = 0;

static lv_obj_t *find_widget_in_container(lv_obj_t *container, arex_widget_id_t w_id)
{
    // ...
}
```

**render_custom_card_widgets() 移除的 LOG：**
```c
// 删除前
LOG_I("[CUSTOM_RENDER] card=%u tile=%p size=%ux%u widget_count=%u",
      custom_card_idx, card_custom, parent_w, parent_h, count);
```

---

## 3. arex_screen.c

**移除的头部：**
```c
#define LOG_TAG "arex_screen"
#include "log.h"
```

**前向函数声明（新增）：**
```c
static void wall_create(void);
static void modal_create(void);
static void submenu_layer_create(void);
```

**滚动指示器 dots 数量修复（核心修复）：**

修改前：
```c
uint16_t dot_cont_h = AREX_DASH_CARD_COUNT * 14;  // 固定12个
```

修改后：
```c
uint16_t dot_cont_h = arex_visible_dash_count() * 14;  // 实际可见数量
```

**arex_screen_update_scroll_dots() 修改：**
```c
// 修改前
bool show = visible && in_dash_or_edit && dots_enabled;

// 修改后
uint8_t visible_dash = arex_visible_dash_count();
bool show = visible && in_dash_or_edit && dots_enabled && (i < visible_dash);
```

**arex_screen_rebuild_tileview() 完整性修复：**
```c
// 新增
memset(g_card_custom_objs, 0, sizeof(g_card_custom_objs));
g_card_custom_obj_count = 0;
```

**arex_screen_rebuild_layout() 函数名修复：**
```c
// 修改前
arex_5f_grid_rebuild();

// 修改后
arex_5f_grid_rebuild_all();
```

**arex_screen_scroll_to_card() 移除的 LOG：**
```c
// 删除前
LOG_W("[SCROLL] ignore invalid tile_pos=%u", tile_pos);
LOG_W("[SCROLL] tile_pos=%u has no tile object", tile_pos);
LOG_I("[SCROLL] tile_pos=%u card=%s(%u) tile=%p", ...);
```

**删除的未使用代码：**
- `card_id_name()` 函数
- `line_delete_cb()` 回调
- 循环中未使用的 `card_id` 变量

---

## 4. arex_ui_engine.h

**移除重复注释：**
```c
// 删除前（重复行）
/* 5F 网格坐标推算：支持 title_zone_h 避让偏移，确保网格落在标题区下方 */
void arex_calc_widget_grid(...);

/* 5F 网格坐标推算：支持 title_zone_h 避让偏移，确保网格落在标题区下方 */  // 重复
```

---

## 5. Scroll Dots 问题分析与修复

### 问题现象
指示器显示非常多个 dots（实际只有几个可见卡片）

### 根本原因
- `AREX_DASH_CARD_COUNT = AREX_MAX_DYNAMIC_SLOTS = 12`（固定最大值）
- 原代码固定创建 12 个 dots 对象的容器和对象
- `arex_screen_update_scroll_dots()` 没有过滤 `i >= visible_dash` 的 dots

### 修复方案
- dot 容器高度：`arex_visible_dash_count() * 14`（根据实际可见数量动态计算）
- 显示逻辑添加：`i < visible_dash` 条件检查

---

## 6. 编译错误修复

| 错误类型 | 修复方式 |
|----------|----------|
| 隐式函数声明 | 添加 `wall_create()`, `modal_create()`, `submenu_layer_create()` 前向声明 |
| 悬空指针 | `arex_screen_rebuild_tileview()` 添加清零 `g_card_custom_objs[]` |
| 函数名不一致 | `arex_5f_grid_rebuild()` → `arex_5f_grid_rebuild_all()` |

---

## 7. 从"同时的新架构"同步的关键修改

本次修改的核心是将 `同时的新架构/` 目录下的正确实现同步到 `src/arex_ui/`。

### 主要同步点

1. **函数声明前向声明** - 新架构在文件开头正确声明了 static 函数
2. **滚动指示器动态计算** - 使用 `arex_visible_dash_count()` 替代固定值
3. **rebuild 函数完整性** - 新架构包含完整的指针清零逻辑
4. **LOG 清理** - 新架构已移除所有调试 LOG
