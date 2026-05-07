# arex_screen_rebuild_layout 修复记录

## 修复状态: 已完成 ✓

## 修复总览

本次修复解决了 `arex_screen_rebuild_layout()` 和 `arex_screen_rebuild_tileview()` 函数中的 4 个关联问题，这些问题导致布局切换时出现对象丢失、焦点丢失、崩溃和卡死现象。

| 问题 | 现象 | 根因 | 修复方案 |
|------|------|------|----------|
| 问题一 | 切换布局后模块重叠 | 重复创建 wall/modal/submenu | 删除重复调用 |
| 问题二 | 切换布局后焦点丢失 | 未保存/恢复 tile 焦点 | 保存并恢复焦点 |
| 问题三 | 旋转时程序崩溃 | 悬空指针访问 | 添加 NULL 检查 |
| 问题四 | 布局切换后卡死 | invalidation 被禁用 | 启用 invalidation |

---

## 问题一：重复创建 wall/modal/submenu 导致模块重叠

### 现象
初次加载时 `s_right_cont` 子对象数量为 5（tileview + wall_top + wall_bottom + modal + submenu_layer），但切换布局后子对象数量变为 1（只有 tileview），导致 wall/modal/submenu 等模块位置错乱或重叠。

### 根因分析
`arex_screen_rebuild_tileview()` 函数中存在重复调用链：

```
arex_screen_rebuild_tileview()
  └── right_panel_create()           ← 第一次调用（正确）
        ├── wall_create()
        ├── submenu_layer_create()
        └── modal_create()
  └── (末尾重复调用)                  ← 第二次调用（错误）
        ├── wall_create()           ← 此时 s_right_cont 已被重建，s_wall_top 等已置 NULL
        ├── submenu_layer_create()
        └── modal_create()
```

初次加载时，虽然 `right_panel_create()` 内部已正确创建了 wall/modal/submenu，但末尾的重复调用由于 `s_right_cont` 已被清空/重建，导致这些对象指针全部为 NULL，表面上看起来"正常"。

切换布局时，问题变得严重：末尾的重复调用依赖于某个条件分支，实际没有被执行，导致 wall/modal/submenu 完全没有被重新创建。

### 修复方案
删除 `arex_screen_rebuild_tileview()` 末尾重复的 `wall_create()`、`submenu_layer_create()`、`modal_create()` 调用，因为这三个函数已经在 `right_panel_create()` 内部被正确调用了。

**修复代码位置**: `src/arex_ui/arex_screen.c` 第 517-519 行

```c
// 修复前（错误代码）
right_panel_create();
wall_create();           // ← 重复调用，已删除
submenu_layer_create();   // ← 重复调用，已删除
modal_create();           // ← 重复调用，已删除

// 修复后（正确代码）
right_panel_create();
/* wall/submenu/modal 已在 right_panel_create() 内部正确创建，无需重复调用 */
```

---

## 问题二：切换布局后 tile 焦点丢失

### 现象
更新布局后，原来选中的卡片焦点消失，界面回到了默认位置（第一个卡片），用户体验中断。

### 根因分析
`arex_screen_rebuild_tileview()` 删除并重建了整个 `s_right_cont`（包含 tileview），但没有保存和恢复当前 tile 的焦点状态。重建后 LVGL 默认显示第一个 tile，而不是用户之前选中的位置。

### 修复方案
1. 在删除对象前保存当前焦点位置（使用 `g_ui.dash_card`，它已经保存了当前卡片位置）
2. 重建后在 `s_tileview` 上调用 `lv_obj_set_tile()` 恢复焦点

**修复代码位置**: `src/arex_ui/arex_screen.c` 第 481-522 行

```c
void arex_screen_rebuild_tileview(void)
{
    /* ... invalidation 修复代码 ... */

    /* 【问题二修复】保存当前焦点位置 */
    uint8_t saved_dash_card = g_ui.dash_card;

    /* 重建 tileview ... */

    /* 【问题二修复】恢复 tile 焦点到保存的位置 */
    if (s_tileview && saved_dash_card < AREX_CARD_COUNT && s_tile_objs[saved_dash_card]) {
        lv_obj_set_tile(s_tileview, s_tile_objs[saved_dash_card], LV_ANIM_OFF);
    }
}
```

---

## 问题三：旋转时程序崩溃（NULL Pointer Dereference）

### 现象
切换布局后再旋转屏幕，程序直接崩溃退出，没有任何错误日志。

### 根因分析
`arex_screen_rebuild_tileview()` 删除 `s_right_cont` 时，会触发 LVGL 的延迟删除机制（对象在下一帧渲染时才真正释放）。在对象真正释放完成前：

1. `s_tileview` 指针已被置 NULL
2. 其他代码路径（如 `wall_nudge_tileview()`、`arex_screen_show_wall()` 等）仍然持有老的 `s_tileview` 指针副本
3. 旋转时会调用 `arex_screen_hide_walls()` 等函数，这些函数直接访问 `s_tileview` 而没有 NULL 检查
4. 导致 NULL pointer dereference 崩溃

调用链：
```
屏幕旋转
  └── arex_screen_hide_walls()
        └── lv_obj_get_y(s_tileview)  ← 崩溃！s_tileview 已为 NULL
```

### 修复方案
在所有访问 `s_tileview` 的函数开头添加 NULL 检查：

| 函数名 | 修复位置 |
|--------|----------|
| `wall_nudge_tileview()` | 第 814 行 |
| `arex_screen_show_wall()` | 第 832 行 |
| `arex_screen_hide_walls()` | 第 851 行 |
| `arex_screen_hide_walls_snap()` | 第 871 行 |
| `arex_screen_scroll_to_card()` | 第 704 行 |

**修复代码示例**:

```c
static void wall_nudge_tileview(lv_coord_t offset_y)
{
    /* 【问题三修复】s_tileview 可能为 NULL（布局重建期间） */
    if (!s_tileview) return;

    /* ... 原有的动画逻辑 ... */
}

void arex_screen_show_wall(wall_side_t side, uint8_t charge, const char *text)
{
    /* 【问题三修复】s_tileview 可能为 NULL（布局重建期间） */
    if (!s_tileview) return;

    /* ... 原有的显示逻辑 ... */
}
```

---

## 问题四：LVGL invalidation 被禁用导致删除操作卡死

### 现象
DEPTH 布局切换测试时，第一次切换成功，但第二次切换后程序卡死（没有任何输出），程序完全无响应。

### 根因分析
调用链如下：

```
arex_bus_set_ui_layout()
  → 设置 dirty_mask |= DIRTY_UI_LAYOUT
  → 定时器 arex_ui_timer_cb() 检测到 DIRTY_UI_LAYOUT
    → 调用 lv_disp_enable_invalidation(disp, false) 禁用 invalidation（优化刷屏性能）
    → 调用 arex_screen_rebuild_layout()
      → 调用 lv_obj_del(s_right_cont) 删除对象
        → LVGL 需要 invalidation 来完成删除操作
        → 但 invalidation 已被禁用 → 卡死！
```

这是因为 LVGL 的对象删除机制依赖 invalidation 机制来触发渲染循环中的清理操作。当 invalidation 被禁用时，删除操作无法完成，程序在 `lv_obj_del()` 处永久阻塞。

### 修复方案
在以下两个函数的**开头**无条件重新启用 invalidation：

| 函数名 | 修复位置 |
|--------|----------|
| `arex_screen_rebuild_layout()` | 第 429-432 行 |
| `arex_screen_rebuild_tileview()` | 第 475-478 行 |

**修复代码**:

```c
void arex_screen_rebuild_layout(void)
{
    /* 【问题四修复】必须在清空对象前重新启用 invalidation
     * 因为 arex_ui_timer_cb() 中禁用了 invalidation 以优化刷屏性能，
     * 任何涉及删除 LVGL 对象的代码都应该假设 invalidation 可能被禁用。 */
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) lv_disp_enable_invalidation(disp, true);

    /* 1. 必须在清空对象前，把指针全部洗白！断绝悬空指针！ */
    clear_widget_arrays();
    /* ... */
}
```

这是一个 **safety fix** —— 任何涉及删除 LVGL 对象的代码都应该假设 invalidation 可能被禁用，先恢复它再执行删除操作。

---

## 测试验证

### 自动布局切换测试
已在 `src/UI_main.c` 中启用自动布局切换测试代码，每秒在 DEPTH 2x1 和 2x2 布局之间切换：

```c
static void sim_tick_cb(lv_timer_t *t)
{
    /* 布局切换测试：每秒切换一次布局（phase: 0→1→0 循环） */
    static uint16_t s_layout_tick = 0;
    static bool s_started = false;
    if (!s_started) {
        printf("[TEST] DEPTH layout switch test started (every 1s): 2x1 ↔ 2x2\r\n");
        s_started = true;
    }
    s_layout_tick++;
    if (s_layout_tick % 1 == 0) {  /* 每秒触发一次 */
        static uint8_t s_layout_phase = 0;
        printf("[TEST] Switching to phase %u: DEPTH %s\r\n",
               s_layout_phase, (s_layout_phase == 0) ? "WIDGET_DEPTH_1606 (2x1)" : "WIDGET_DEPTH_1612 (2x2)");

        arex_test_set_ui_layout(s_layout_phase);
        s_layout_phase = 1 - s_layout_phase;  /* 0↔1 切换 */
    }
    /* ... */
}
```

### 验证项目
- [x] 连续多次布局切换后，wall/modal/submenu 模块显示正常
- [x] 布局切换后，焦点保持在当前选中的卡片
- [x] 布局切换后旋转屏幕，程序不崩溃
- [x] 连续布局切换不卡死

---

## 修改文件清单

| 文件路径 | 修改类型 | 主要变更 |
|----------|----------|----------|
| `src/arex_ui/arex_screen.c` | 修改 | 修复问题一~四 |
| `src/UI_main.c` | 修改 | 启用自动布局切换测试 |
| `UI_html_DOC/REBUILD_FIX.md` | 新增 | 本文档 |

---

## 提交记录

| 提交哈希 | 提交信息 |
|----------|----------|
| `56ff271` | 修复了模块重叠问题 |
| `b80ff3b` | 打开每秒换布局测试 |
| `5db41b8` | 添加版本号 |

---

## 经验教训

1. **LVGL 对象删除前必须启用 invalidation**：任何涉及删除 LVGL 对象的代码都应该假设 invalidation 可能被禁用，先恢复它再执行删除操作。

2. **删除对象前必须清除所有相关指针**：使用 `clear_widget_arrays()` 统一清空所有 LVGL 对象指针，防止悬空指针访问。

3. **共享指针需要 NULL 检查**：对于被多处引用的静态指针（如 `s_tileview`），在所有访问点都要进行 NULL 检查。

4. **不要重复调用已调用的函数**：如果一个函数内部已经调用了子函数，不要在该函数末尾再次调用，代码审查时注意这种模式。
