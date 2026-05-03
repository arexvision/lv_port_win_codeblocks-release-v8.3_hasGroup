# 2026-05-03 代码修改同步记录

## 修改文件总览

| 文件 | 修改类型 | 说明 |
|------|----------|------|
| `src/arex_ui/arex_ui_state.c` | LOG删除 | 移除所有 LOG 宏调用 |
| `src/arex_ui/arex_ui_engine.c` | 清理 + 功能修改 | 删除未使用代码、深度小数、告警逻辑、速率图标 |
| `src/arex_ui/arex_data.c` | 告警阈值修改 | 上升告警阈值 18→10 m/min |
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

### 2.1 删除的未使用代码
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

### 2.2 深度小数位修复

**问题**：深度12.32m 显示成 12.-3m

**原因**：计算 `(depth - (int)depth) * 10` 在负数时结果为负

**修复**：
```c
// 修改前
lv_label_set_text_fmt(dec_lbl, ".%d", (int)((g_sensor_data.depth - (int)g_sensor_data.depth) * 10 + 0.5f));

// 修改后
lv_label_set_text_fmt(dec_lbl, ".%d", (int)(fabsf(g_sensor_data.depth - (int)g_sensor_data.depth) * 10 + 0.5f));
```

### 2.3 告警系统重构

**新增变量**：
```c
/* 告警显示计时器：控制告警最少显示 5 秒 */
static uint32_t s_alarm_start_tick = 0;
#define ALARM_MIN_DISPLAY_MS  5000   /* 告警最少显示 5 秒 */

/* 告警活跃标记：触发告警后持续闪烁，直到速度降到安全范围 */
static bool s_alarm_active = false;
```

**arex_trigger_alarm() 修改**：
```c
void arex_trigger_alarm(...)
{
    /* 如果已有活跃告警且未达到最小显示时间，不覆盖 */
    if (s_alarm_active && g_current_alarm_level != AREX_ALARM_NONE) {
        uint32_t elapsed = lv_tick_elaps(s_alarm_start_tick);
        if (elapsed < ALARM_MIN_DISPLAY_MS) {
            return;  /* 仍在最短显示期内，忽略新告警 */
        }
    }

    arex_show_alarm_banner(level, eng_text);
    g_current_alarm_target = target_id;
    g_current_alarm_level = level;
    g_ui.alarm_pending_click = true;
    s_alarm_start_tick = lv_tick_get();
    s_alarm_active = true;  /* 新增：标记告警活跃 */
}
```

**arex_clear_all_alarm_styles() 修改**：
```c
void arex_clear_all_alarm_styles(void)
{
    /* 检查是否满足最小显示时间（5秒） */
    uint32_t elapsed = lv_tick_elaps(s_alarm_start_tick);
    if (elapsed < ALARM_MIN_DISPLAY_MS) {
        return;  /* 未达到最短显示期，不清除 */
    }

    if (s_alarm_banner) {
        lv_obj_add_flag(s_alarm_banner, LV_OBJ_FLAG_HIDDEN);
    }

    g_current_alarm_target = WIDGET_EMPTY;
    g_current_alarm_level = 0;
    s_alarm_active = false;  /* 新增：标记告警已清除 */
}
```

### 2.4 速率图标闪烁逻辑修复

**问题**：停止运动后图标仍在 level3 闪烁

**修复**：心跳引擎添加 `s_alarm_active` 条件，速率图标逻辑区分静止/移动状态

**心跳引擎修改**：
```c
/* 修改前：只在速度>=3m/min时刷新 */
if (fabsf(rate) >= AREX_RATE_STILL_THRESHOLD) {
    g_sensor_data.dirty_mask |= DIRTY_DEPTH;
}

/* 修改后：有告警或速度>=3m/min时都刷新 */
if (s_alarm_active || fabsf(rate) >= AREX_RATE_STILL_THRESHOLD) {
    g_sensor_data.dirty_mask |= DIRTY_DEPTH;
}
```

**速率图标更新逻辑修改**：
```c
/* 速度降到安全范围后自动清除告警（但最少显示5秒） */
if (!is_moving && s_alarm_active) {
    arex_clear_all_alarm_styles();
}

/* 静止时强制显示 level0，不闪烁 */
if (!is_moving) {
    int8_t effective_dir = s_last_direction;
    target_img_src = (effective_dir > 0) ? &sudo_up_level0 :
                    (effective_dir < 0) ? &sudo_down_level0 : &sudo_up_level0;
}
/* 移动中 且 非方向切换期 且 闪烁亮相位 → 显示真实等级 */
else if (!direction_changed && current_flash_state == true) {
    // 显示 level1/2 闪烁
}
```

---

## 3. arex_data.c

### 告警阈值修改

**问题**：上升告警阈值 18m/min 过高

**修复**：改为 10m/min

```c
// 修改前
if (rate < -18.0f) {
    g_alarm_pending = true;
    g_pending_alarm_text = "ASCENT RATE FAST";
}

// 修改后
if (rate < -10.0f) {
    g_alarm_pending = true;
    g_pending_alarm_text = "ASCENT RATE FAST";
}
```

---

## 4. arex_screen.c

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

## 8. 功能需求汇总

### 8.1 深度小数位显示
- **需求**：显示 12.3m 而非 12.-3m
- **实现**：使用 `fabsf()` 绝对值计算

### 8.2 上升告警阈值
- **需求**：超过 10m/min 触发告警（原来 18m/min）
- **实现**：修改 `arex_data.c` 中的阈值

### 8.3 告警自动清除
- **需求**：速度降到安全范围后清除告警，但最少显示 5 秒
- **实现**：
  - 添加计时器 `s_alarm_start_tick` 和常量 `ALARM_MIN_DISPLAY_MS`
  - 添加标记 `s_alarm_active` 追踪告警状态
  - `arex_clear_all_alarm_styles()` 检查时间和状态

### 8.4 速率图标静止闪烁修复
- **需求**：停止运动后图标不再闪烁，显示 level0
- **实现**：
  - 静止时直接显示 level0，不执行闪烁逻辑
  - 告警活跃时保持心跳刷新以显示闪烁

---

## 9. 编译错误修复

| 错误 | 修复 |
|------|------|
| 隐式函数声明 | 添加前向声明 |
| 悬空指针 | 添加清零逻辑 |
| 函数名不一致 | 统一函数名 |
| `lv_tick_elapsed` 不存在 | 改为 `lv_tick_elaps`（LVGL v8.3 API） |

---

## 10. 从"同时的新架构"同步的关键修改

本次修改的核心是将 `同时的新架构/` 目录下的正确实现同步到 `src/arex_ui/`。

### 主要同步点

1. **函数声明前向声明** - 新架构在文件开头正确声明了 static 函数
2. **滚动指示器动态计算** - 使用 `arex_visible_dash_count()` 替代固定值
3. **rebuild 函数完整性** - 新架构包含完整的指针清零逻辑
4. **LOG 清理** - 新架构已移除所有调试 LOG
