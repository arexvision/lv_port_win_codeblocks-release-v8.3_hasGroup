# AREX Pro LVGL 渲染修复记录

> 记录从 v0.10 原型对照仿真截图，逐步修复到像素级对齐的全部改动。  
> 文件涉及：`arex_screen.c` / `arex_ui_engine.c` / `card_info.c` / `card_setup.c`

---

## 一、核心铁律（贯穿全部修复）

| 铁律 | 说明 |
|------|------|
| `pad_all = 0` | 所有容器必须显式清零，LVGL 默认 theme padding 会偏移绝对坐标 |
| `border_width = 0` | 容器边框必须显式关闭，否则影响内容区域 |
| `LV_OBJ_FLAG_SCROLLABLE` 关闭 | 所有展示用容器禁止滚动 |
| `lv_obj_set_size` + `LV_LABEL_LONG_DOT` | 所有 label 必须锁死尺寸，禁止 `LV_SIZE_CONTENT` 高度无限扩张 |
| 绝对坐标不依赖 Flex | 左侧锚点全部用 `lv_obj_set_pos`，右侧列表用 Flex Column 但 item 内部绝对定位 |

---

## 二、左侧锚点（Left Anchor）修复

### 2.1 `s_style_anchor_bg` 缺少 pad/radius 清零

**文件**：`arex_screen.c` → `styles_init()`

**问题**：样式只设了颜色和边框，未清零 padding，LVGL 默认 theme padding（8px）导致所有子组件坐标整体右下偏移 8px，累加后越到下方越偏。

**修复**：
```c
lv_style_set_pad_all(&s_style_anchor_bg, 0);
lv_style_set_radius(&s_style_anchor_bg, 0);
```

### 2.2 `s_left_anchor` 对象本身缺少 pad 兜底清零

**文件**：`arex_screen.c` → `left_anchor_create()`

**修复**：
```c
lv_obj_set_style_pad_all(s_left_anchor, 0, 0);   // 兜底，确保 theme 默认 padding 不干扰
```

### 2.3 双拼组件（split=2 右块）x 坐标全部为 0 导致重叠

**文件**：`arex_screen.c` → `left_anchor_create()` 循环内

**问题**：`title_zone` 和 `val_zone` 的 x 坐标硬编码为 0，NDL/TTS、POD1/POD2、BATT/W.TIME 的右半块叠在左半块上，显示为 "WATTME"、"210195" 等重叠字符串。

**修复**：
```c
lv_coord_t comp_x = (c->split == 2) ? (lv_coord_t)(AREX_LEFT_ANCHOR_W / 2) : 0;
lv_obj_set_pos(title_zone, comp_x, c->y);
lv_obj_set_pos(val_zone,   comp_x, c->y + c->title_h);
```

### 2.4 对齐方向统一管理（标题与数值必须同向）

**文件**：`arex_screen.c` → `left_anchor_create()` 循环内

**问题**：`lbl_title` 对齐未设置（默认左对齐），`lbl_val` 的右对齐在循环末尾后补，导致标题与数值对齐方向不一致（如 POD 2 标题靠左、数值靠右）。

**修复**：在创建两个 zone 之前统一计算 `comp_align`，`lbl_title` 和 `lbl_val` 均使用同一变量：
```c
lv_text_align_t comp_align;
if (g_sys_config.split_outward && c->split == 2)      comp_align = LV_TEXT_ALIGN_RIGHT;
else if (g_sys_config.split_outward && c->split == 1) comp_align = LV_TEXT_ALIGN_LEFT;
else                                                   comp_align = arex_align_to_lv(g_sys_config.align_med);
```
同时删除循环末尾原有的"双拼对齐引擎"后补代码。

### 2.5 `lbl_title` 缺少尺寸锁定

**文件**：`arex_screen.c` → `left_anchor_create()` 循环内

**问题**：`lbl_title` 只设了 `set_pos`，高度使用 `LV_SIZE_CONTENT`，会溢出 `title_zone`。

**修复**：
```c
lv_obj_set_size(lbl_title, c->w - 8, c->title_h);
lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_DOT);
```

### 2.6 `lbl_val` 缺少尺寸锁定

**文件**：`arex_screen.c` → `left_anchor_create()` 循环内

**修复**：
```c
lv_obj_set_size(lbl_val, c->w - 8, c->val_h);
lv_label_set_long_mode(lbl_val, LV_LABEL_LONG_DOT);
```

### 2.7 各模块字体与高度的数学自洽

**文件**：`arex_screen.c` → `left_anchor_create()` / `arex_ui_engine.c` → `arex_sys_config_defaults()`

**问题**：所有模块均使用 `AREX_FONT_MEDIUM`（28px），但 BATT/W.TIME/TIME 的 val_h 只有 20px（h=4U，title 占 2U，剩余 2U=20px），28px 字体必然溢出被截断。

**修复 — 字体映射**：

| 索引 | 名称 | val_h | 字体 |
|------|------|-------|------|
| 0 | DEPTH | 60px | HUGE (48px) |
| 1,2 | NDL/TTS | 40px | MEDIUM (28px) |
| 3,4 | POD1/POD2 | 40px | TITLE (20px) |
| 5,6 | BATT/W.TIME | 30px | SMALL (14px) |
| 7 | GAS | 40px | MEDIUM (28px) |
| 8 | TIME | 30px | SMALL (14px) |

**修复 — 高度配置**：将 `h_batt` 和 `h_time` 从 4U 改为 5U（val_h 从 20px 增至 30px）：
```c
cfg->h_batt = 5;   /* 50px → val_h = 30px */
cfg->h_time = 5;   /* 50px → val_h = 30px */
```

**同时删除**循环后的冗余字号降级代码（POD1/POD2 后补 TITLE 字体），改为在 `val_fonts[]` 数组初始化时统一设定。

### 2.8 TTS 反色绿底去除

**文件**：`arex_screen.c` → `left_anchor_create()` i==2 分支

**问题**：代码主动给 TTS 数值 label 设置了绿色背景，属于旧版遗留的警报高亮逻辑，当前非报警状态下不应显示。

**修复**：删除以下三行：
```c
// 已删除：
lv_obj_set_style_bg_color(lbl_val, AREX_GREEN, 0);
lv_obj_set_style_bg_opa(lbl_val, LV_OPA_COVER, 0);
lv_obj_set_style_text_color(lbl_val, AREX_BLACK, 0);
lv_obj_set_style_pad_hor(lbl_val, 4, 0);
```

### 2.9 NEXT STOP / PO2 游离元素隐藏

**文件**：`arex_screen.c` → `left_anchor_create()` i==0 分支

**问题**：NEXT STOP（y=132/148）和 PO2（y=192/210）使用旧设计的硬编码坐标，直接挂在 `s_left_anchor` 上，与当前 10U 动态布局的 NDL/POD 区重叠，形成"幽灵元素"。

**修复**：保留代码，全部加 `LV_OBJ_FLAG_HIDDEN`，待按正确的动态坐标重新设计后再启用。

---

## 三、右侧区域通用修复

### 3.1 `arex_screen_make_card_title` — 标题 label 尺寸锁定

**文件**：`arex_screen.c`

**修复**：
```c
lv_obj_set_size(lbl, (s_cached_right_w > 0 ? s_cached_right_w : 420) - 32, 28);
lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
```

---

## 四、列表菜单修复（card_info / card_setup / submenu）

### 4.1 列表宽度硬编码 428px → `LV_PCT(100)`

**文件**：`card_info.c`、`card_setup.c`

**问题**：`s_list` 宽度写死 428px，超出父容器（右侧宽度随配置变化）。

**修复**：
```c
lv_obj_set_size(s_list, LV_PCT(100), ...);
lv_obj_set_pos(s_list, 0, 50);   // 偏移从 16 改为 0，由 label 内缩提供间距
```

### 4.2 菜单项 `pad_ver=12, pad_hor=15` → `pad_all=0` + label 内缩

**文件**：`card_info.c`、`card_setup.c`、`arex_screen.c`（`submenu_populate`）

**问题**：item 的 padding 直接撑高容器，导致"幽灵内边距"问题，48px 固定高度失效。

**修复**：item 清零 padding，改用 `clip_corner=true` 强制裁剪：
```c
lv_obj_set_style_pad_all(item, 0, 0);
lv_obj_set_style_clip_corner(item, true, 0);
```
label 改用 `lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 12, 0)` 提供 12px 左侧呼吸空间。

### 4.3 菜单 label 垂直居中

**文件**：`card_info.c`、`card_setup.c`、`arex_screen.c`（`submenu_populate`）

**问题**：label 高度锁死为 48px（与 item 等高），`LV_ALIGN_LEFT_MID` 无法产生偏移效果，文字贴顶。

**修复**：label 高度改为 `LV_SIZE_CONTENT`，让 LVGL 自动计算文字高度后居中：
```c
lv_obj_set_size(lbl, LV_PCT(100), LV_SIZE_CONTENT);
lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 12, 0);
```

### 4.4 `card_setup` badge 文字被截断 / 贴死右边框

**文件**：`card_setup.c`

**问题**：badge label 无宽度约束，`LV_TEXT_ALIGN_RIGHT` 的文字右边缘贴死父容器边框（0px 间距），且与 title label 重叠。

**修复**：title label 宽度限制为 280px，badge 宽度 80px 靠右对齐并留 12px 呼吸空间：
```c
lv_obj_set_size(badge, 80, LV_SIZE_CONTENT);
lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -12, 0);
```

### 4.5 `set_setup_selection` 未同步修改 badge 颜色

**文件**：`arex_screen.c`

**问题**：选中状态切换时只改 title label 颜色，badge label（"MED"/"HIGH"）保持浅绿色，在绿底背景上几乎不可见。

**修复**：
```c
lv_obj_t *badge = lv_obj_get_child(item, 1);
// 选中时：
if (badge) lv_obj_set_style_text_color(badge, AREX_BLACK, 0);
// 未选中时：
if (badge) lv_obj_set_style_text_color(badge, AREX_LIGHT, 0);
```

---

## 五、MOD PO2 编辑模式修复

### 5.1 编辑 badge 去除绿色背景容器

**文件**：`arex_screen.c` → `arex_screen_begin_edit_value()`

**问题**：原实现动态创建了一个绿底 `lv_obj_create` badge 容器包裹数值，外观突兀；同时 `lv_obj_set_layout(item, LV_LAYOUT_FLEX)` 触发布局重算导致闪烁。

**修复**：去除 badge 容器和 Flex 布局，直接在 item 上创建透明背景 val_lbl，用 `LV_ALIGN_RIGHT_MID` 靠右定位：
```c
lv_obj_t *val_lbl = lv_label_create(item);
lv_obj_set_style_bg_opa(val_lbl, LV_OPA_TRANSP, 0);
lv_obj_set_size(val_lbl, 120, LV_SIZE_CONTENT);
lv_obj_align(val_lbl, LV_ALIGN_RIGHT_MID, -12, 0);
```

### 5.2 编辑态三态颜色规范

**文件**：`arex_screen.c`

**三态定义**：

| 状态 | 背景 | 边框 | 文字 |
|------|------|------|------|
| Inactive（默认） | 黑色 | AREX_DARK | 绿色 / 浅绿 badge |
| Active（选中） | 实心绿 `LV_OPA_COVER` | 绿色 | 黑色（title + badge） |
| Editing（编辑） | 黑色 | 绿色（2px） | 绿色，右侧数值闪烁 |

**修复**：`begin_edit_value` 进入时主动从选中态切换到编辑态（绿底→黑底绿框，文字从黑恢复为绿）：
```c
lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
lv_obj_set_style_border_color(item, AREX_GREEN, 0);
// 恢复 title/badge 文字为绿色
```

### 5.3 `set_submenu_selection` 跳过编辑中的 item

**文件**：`arex_screen.c`

**问题**：旋转滚轮时 `set_submenu_selection` 会把编辑态的 item 覆盖回选中态（绿底），破坏编辑状态显示。

**修复**：
```c
if (g_ui.edit_ctx.active && (uint8_t)i == g_ui.edit_ctx.item_index) continue;
```

### 5.4 数值刷新改为原地更新

**文件**：`arex_screen.c` → `arex_screen_refresh_edit_value()`

**修复**：直接通过 `s_edit_flash_val_lbl` 指针原地更新文字，不重建任何对象：
```c
lv_label_set_text(s_edit_flash_val_lbl, buf);
```

### 5.5 闪烁改为文字颜色切换

**文件**：`arex_screen.c` → `edit_flash_timer_cb()`

**问题**：原实现切换 badge 容器背景色（绿/黑交替），现 badge 容器已去除。

**修复**：改为切换 val_lbl 文字颜色（`AREX_GREEN` ↔ `AREX_DARK`）：
```c
lv_color_t fg = s_edit_flash_on ? AREX_GREEN : AREX_DARK;
lv_obj_set_style_text_color(s_edit_flash_val_lbl, fg, 0);
```

---

## 六、配置变更汇总（`arex_ui_engine.c`）

| 参数 | 旧值 | 新值 | 原因 |
|------|------|------|------|
| `h_batt` | 4U (40px) | 5U (50px) | val_h=20px 放不下任何字体 |
| `h_time` | 4U (40px) | 5U (50px) | 同上 |

---

## 七、Debug 边框（统一宏控制）

`AREX_DEBUG_BORDER` 宏定义于 `arex_ui_engine.h`，通过 `#include "arex_ui_engine.h"` 间接被所有源文件引用：

```c
// arex_ui_engine.h
#define AREX_DEBUG_BORDER  0  /* 0=关闭(默认), 1=开启 debug 边框 */
```

| 值 | title_zone 边框 | val_zone 边框 |
|----|-----------------|---------------|
| `0` | 无边框 | 无边框 |
| `1` | 1px AREX_DARK | 1px AREX_DARK |

---

## 八、4F 潜水曲线渲染引擎（`card_plan.c`）

### 8.1 彻底废除硬编码假数据

**文件**：`card_plan.c`

**问题**：原实现使用固定 55 分钟的 `s_profile[]` 静态数组和黄色 NOW 标记，与 HTML 原型逻辑完全不符。

**修复 — 数据总线**（与 HTML `diveLog` / `mockStops` 完全对应）：

```c
typedef struct { float time_min; float depth_m; } arex_dive_pt_t;
typedef struct { float depth_m; float stay_min; } arex_deco_stop_t;

arex_dive_pt_t   g_dive_log[MAX_DIVE_LOG];
uint16_t         g_dive_log_count;

arex_deco_stop_t g_deco_stops[MAX_DECO_STOPS];
uint16_t         g_deco_stop_count;
```

**修复 — 测试数据**：
```c
g_sensor_data.dive_time_s = 45;    // 下水刚 45 秒
g_sensor_data.depth = 13.1f;       // 深度 13.1m
g_dive_log_count = 2;              // 只有 2 个历史点，说明刚下水
g_dive_log[0] = {0.0f, 0.0f};
g_dive_log[1] = {0.5f, 10.0f};    // 0.5min 时在 10m
g_deco_stop_count = 1;
g_deco_stops[0] = {3.0f, 3.0f};  // 3m 停留 3 分钟
```

### 8.2 零内存高阶渲染引擎

**文件**：`card_plan.c` → `plan_chart_draw_cb`

**核心特性**：
| 特性 | 说明 |
|------|------|
| Auto-Scale X轴 | 根据 `max(current_t, predicted_t) * 1.05` 动态推算，≥120min 时按 60 步进 |
| Auto-Scale Y轴 | 初始 60m，超 90% 时按 20 的倍数扩展 |
| 历史轨迹 | 实线 3px，`g_dive_log[]` 数据，`g_sensor_data.depth` 作为 NOW 点 |
| 计划轨迹 | 虚线 dash=6 gap=5，`g_deco_stops[]` + 算法升水推演 |
| NOW 标记 | 绿色实心圆 R=6px + 黑字绿底背景（**绝无黄色**） |
| 减压停留点 | 空心圆 + "Xm Y'" 文字标注 |

**坐标映射**（与 HTML 完全一致）：
```c
#define MAP_X(t)  (area->x1 + (lv_coord_t)(45.0f + ((t)/max_t_axis) * w))
#define MAP_Y(d)  (area->y1 + (lv_coord_t)(15.0f + ((d)/max_d_axis) * h))
```

### 8.3 色彩铁律

| 元素 | 颜色 |
|------|------|
| 网格线、文字 | `AREX_GREEN`，opa=38（网格）/191（文字） |
| 历史轨迹 | `AREX_GREEN`，实线 3px |
| 计划轨迹 | `AREX_GREEN`，虚线 dash=6 gap=5 |
| NOW 圆点 | `AREX_GREEN` 实心圆 |
| NOW 文字 | 黑字 `AREX_BLACK`，绿底背景 |
| 停留点圆 | `AREX_BLACK` 填充，`AREX_GREEN` 边框 |
| 停留点文字 | `AREX_GREEN` |

---

## 十、Protobuf 枚举对齐 + 组件更新数据流修复

### 10.1 枚举精简（`arex_ui_engine.h`）

**文件**：`arex_ui_engine.h` → `arex_widget_id_t`

**背景**：与 APP 端 Protobuf 的 `WidgetType` 严格对齐，移除 MCU 端多余 ID。

**修复 — 新枚举定义**：

```c
typedef enum {
    WIDGET_TYPE_UNSPECIFIED = 0,
    WIDGET_EMPTY            = 0,  /* C语言内部别名：空槽位占位符 */

    /* --- 核心驻留区 (强制固定) --- */
    WIDGET_NDL_STOP_1606    = 1,
    WIDGET_DEPTH_1612       = 2,
    WIDGET_DEPTH_1606       = 3,
    WIDGET_DIVE_TIME_1606   = 4,
    WIDGET_GAS_1606         = 5,
    WIDGET_SYS_1606         = 6,

    /* --- 基础组件 (Basic) --- */
    WIDGET_TEMP_0806        = 10,
    WIDGET_TIME_1606        = 11,
    WIDGET_TTS_0806         = 12,
    // ... 13-19 ...

    /* --- 技术潜水 (Tech Dive) --- */
    WIDGET_SURF_GF_0806    = 20,
    // ... 21-31 ...

    /* --- 传感器 (Sensors) --- */
    WIDGET_HEADING_0806     = 32,
    WIDGET_POD_0806         = 33,
    WIDGET_DEPTH_MAX_0806   = 34,
    WIDGET_DEPTH_AVG_0806   = 35,
    WIDGET_TEMP_MIN_0806    = 36,
    WIDGET_TEMP_AVG_0806    = 37
    /* 🚨 以下已移除：38(TEMP_MAX), 39(SAC_RATE), 40(WTIME), 50-52(安全边界组件) */
} arex_widget_id_t;
```

**已移除的废弃 ID**：

| 旧 ID | 名称 | 原因 |
|-------|------|------|
| 38 | `WIDGET_TEMP_MAX_0806` | Protobuf 已移除 |
| 39 | `WIDGET_SAC_RATE_0806` | Protobuf 已移除 |
| 40 | `WIDGET_WTIME_0806` | Protobuf 已移除（替换为 WIDGET_TIME_1606） |
| 50 | `WIDGET_PPO2_SAFE_0806` | Protobuf 已移除 |
| 51 | `WIDGET_NDL_SAFE_0806` | Protobuf 已移除 |
| 52 | `WIDGET_SAC_SAFE_0806` | Protobuf 已移除 |

---

### 10.2 样式表清理（`arex_ui_engine.c`）

**文件**：`arex_ui_engine.c` → `g_widget_styles[]`

**修复**：将废弃 ID 的样式项替换为 `WIDGET_EMPTY`，保持数组索引稳定。

| 废弃 ID | 替换为 | 位置 |
|---------|--------|------|
| `WIDGET_TEMP_MAX_0806` | `WIDGET_EMPTY` | g_widget_styles |
| `WIDGET_SAC_RATE_0806` | `WIDGET_EMPTY` | g_widget_styles |
| `WIDGET_WTIME_0806` | `WIDGET_EMPTY` | g_widget_styles |
| `WIDGET_PPO2_SAFE_0806` | `WIDGET_EMPTY` | g_widget_styles |
| `WIDGET_NDL_SAFE_0806` | `WIDGET_EMPTY` | g_widget_styles |
| `WIDGET_SAC_SAFE_0806` | `WIDGET_EMPTY` | g_widget_styles |

---

### 10.3 网格配置清理（`arex_ui_engine.c`）

**文件**：`arex_ui_engine.c` → `g_5f_widgets[]` / `g_left_widgets[]`

**修复 — 5F 自定义网格**：

```c
g_5f_widgets[3] = (arex_5f_widget_t){ WIDGET_EMPTY,         2, 0 };  /* SAC 已移除 */
g_5f_widgets[11] = (arex_5f_widget_t){ WIDGET_EMPTY,         4, 4 };  /* WTIME 已移除 */
g_5f_widgets[12] = (arex_5f_widget_t){ WIDGET_EMPTY,         5, 0 };  /* WTIME 已移除 */
```

**修复 — 左侧锚点网格**：

```c
g_left_widgets[2] = (arex_left_widget_t){ WIDGET_TIME_1606,   0, 3 };  /* 系统时间 */
```

---

### 10.4 渲染工厂 switch 清理（`arex_ui_engine.c`）

**文件**：`arex_ui_engine.c` → `render_widget_by_id()` 内 switch

**修复**：将废弃 ID 的 case 分支注释掉：

```c
/* 🚨 以下已废弃，Protobuf 已移除对应 ID
case WIDGET_WTIME_0806: {
    uint32_t t = g_sensor_data.surface_time_s;
    snprintf(buf, sizeof(buf), "%02d:%02d", t / 60, t % 60);
    break;
}
case WIDGET_TEMP_MAX_0806: snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.max_temp); break;
case WIDGET_SAC_RATE_0806:  snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.sac_rate); break;
case WIDGET_PPO2_SAFE_0806: snprintf(buf, sizeof(buf), "%.2f", 1.4); break;
case WIDGET_NDL_SAFE_0806:  snprintf(buf, sizeof(buf), "%d", 5); break;
case WIDGET_SAC_SAFE_0806:  snprintf(buf, sizeof(buf), "%.1f", 25.0); break;
*/
```

---

### 10.5 刷新面板缺失路由修复（`arex_screen.c`）

**文件**：`arex_screen.c` → `arex_screen_refresh_left_panel()`

**问题**：`WIDGET_DIVE_TIME_1606` 虽已加入左侧网格配置，但刷新函数 switch 中缺少对应分支，导致 `DIRTY_DIVE_TIME` 信号无法路由到组件。

**修复 — 添加 case 分支**：

```c
case WIDGET_DIVE_TIME_1606: {
    /* 直接格式化 MM:SS，避免 arex_widget_set_value 的浮点格式化 */
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d",
             g_sensor_data.dive_time_s / 60,
             g_sensor_data.dive_time_s % 60);
    arex_widget_set_text(WIDGET_DIVE_TIME_1606, buf);
    break;
}
```

**注意**：`arex_widget_set_value` 会将浮点数格式化为 `%.0f`（整数），不适用于 `MM:SS` 时间格式，因此使用 `arex_widget_set_text` 直接设置格式化字符串。

---

### 10.6 测试代码清理（`UI_main.c`）

**文件**：`UI_main.c` → `arex_test_set_ui_layout()`

**修复**：将废弃 ID 替换为有效 ID 或 `WIDGET_EMPTY`：

| 行 | 旧值 | 新值 |
|----|------|------|
| 39 | `WIDGET_WTIME_0806` | `WIDGET_TIME_1606` |
| 55 | `WIDGET_SAC_RATE_0806` | `WIDGET_EMPTY` |
| 63 | `WIDGET_WTIME_0806` | `WIDGET_EMPTY` |
| 96 | `WIDGET_WTIME_0806` | `WIDGET_EMPTY` |

---

### 10.7 数据流完整路径

```
UI_main.c: g_sensor_data.dive_time_s++
    ↓
arex_bus_set_dive_time(dive_s)
    → arex_data.c: g_sensor_data.dirty_mask |= DIRTY_DIVE_TIME
    ↓
arex_ui_engine.c: update_task_timer_cb()
    → if (mask & DIRTY_DIVE_TIME) → arex_screen_refresh_left_panel()
    ↓
arex_screen.c: arex_screen_refresh_left_panel()
    → switch(w_id) case WIDGET_DIVE_TIME_1606
    → arex_widget_set_text(WIDGET_DIVE_TIME_1606, "MM:SS")
    ↓
arex_ui_engine.c: arex_widget_set_text()
    → 遍历容器查找 user_data == WIDGET_DIVE_TIME_1606
    → lv_label_set_text(sub, "MM:SS")
```

### 10.8 全量组件数据路由分发器 (Data Router)

**文件**：`arex_ui_engine.c` / `arex_screen.c`

**架构设计**：新增 `arex_widget_sync_data()` 函数作为数据路由总机，根据 `widget_id` 自动从 `g_sensor_data` 取值并刷新界面。

```
调用层次：
  arex_screen_refresh_all_widgets()
      ├─ 遍历 g_left_widgets[] → arex_widget_sync_data(w_id)
      └─ 遍历 g_5f_widgets[]  → arex_widget_sync_data(w_id)

  arex_widget_sync_data(w_id)
      ├─ switch(w_id) 路由到 g_sensor_data 字段
      └─ 调用 arex_widget_set_value() 或 arex_widget_set_text()
```

**覆盖全部 30+ 组件**，包括：

| 分类 | Widget ID | 数据源 | 接口 |
|------|-----------|--------|------|
| 核心 | `WIDGET_DEPTH_1612/1606` | `g_sensor_data.depth` | `arex_widget_set_value` |
| 核心 | `WIDGET_DIVE_TIME_1606` | `g_sensor_data.dive_time_s` | `arex_widget_set_text` (MM:SS) |
| 核心 | `WIDGET_GAS_1606` | `g_sensor_data.gas_name` | `arex_widget_set_text` |
| 基础 | `WIDGET_TEMP_0806` | `g_sensor_data.temperature_c` | `arex_widget_set_value` |
| 基础 | `WIDGET_TIME_1606` | `g_sensor_data.sys_time_h/m` | `arex_widget_set_text` (HH:MM) |
| 基础 | `WIDGET_TTS_0806` | `g_sensor_data.tts` | `arex_widget_set_value` |
| 基础 | `WIDGET_ASCENT_0806/0812` | `g_sensor_data.ascent_rate` | `arex_widget_set_value` |
| 基础 | `WIDGET_BATTERY_0806` | `g_sensor_data.battery_pct` | `arex_widget_set_value` |
| 基础 | `WIDGET_STOP_DEPTH_0806` | `g_sensor_data.stop_depth_m` | `arex_widget_set_value` |
| 基础 | `WIDGET_STOP_TIME_1606` | `g_sensor_data.stop_time_left_s` | `arex_widget_set_text` (MM:SS) |
| 基础 | `WIDGET_PPO2_0806` | `g_sensor_data.ppo2[gas_active_idx]` | `arex_widget_set_value` |
| Tech | `WIDGET_SURF_GF_0806` | `g_sensor_data.surf_gf` | `arex_widget_set_value` |
| Tech | `WIDGET_GF99_0806` | `g_sensor_data.gf99` | `arex_widget_set_value` |
| Tech | `WIDGET_GF_0806` | `g_sensor_data.gf_low/high` | `arex_widget_set_text` (L/H) |
| Tech | `WIDGET_CNS_0806` | `g_sensor_data.cns_pct` | `arex_widget_set_value` |
| Tech | `WIDGET_OTU_0806` | `g_sensor_data.otu` | `arex_widget_set_value` |
| Tech | `WIDGET_MOD_0806` | `g_sensor_data.mod_m` | `arex_widget_set_value` |
| Tech | `WIDGET_CEILING_0806` | `g_sensor_data.ceiling_m` | `arex_widget_set_value` |
| Tech | `WIDGET_GAS_MIX_1606` | `g_sensor_data.gas_o2/he_pct` | `arex_widget_set_text` (O2/He) |
| Tech | `WIDGET_GAS_DENS_0806` | `g_sensor_data.gas_density` | `arex_widget_set_value` |
| Tech | `WIDGET_FIO2_0806` | `g_sensor_data.fio2_pct` | `arex_widget_set_value` |
| 传感 | `WIDGET_HEADING_0806` | `g_sensor_data.heading` | `arex_widget_set_value` |
| 传感 | `WIDGET_POD_0806` | `g_sensor_data.pod1_bar` | `arex_widget_set_value` |
| 传感 | `WIDGET_DEPTH_MAX_0806` | `g_sensor_data.max_depth` | `arex_widget_set_value` |
| 传感 | `WIDGET_DEPTH_AVG_0806` | `g_sensor_data.avg_depth` | `arex_widget_set_value` |
| 传感 | `WIDGET_TEMP_MIN_0806` | `g_sensor_data.min_temp` | `arex_widget_set_value` |
| 传感 | `WIDGET_TEMP_AVG_0806` | `g_sensor_data.avg_temp` | `arex_widget_set_value` |
| 复杂 | `WIDGET_NDL_STOP_1606` | (状态机专属刷新) | - |
| 复杂 | `WIDGET_SYS_1606` | (状态机专属刷新) | - |
| 复杂 | `WIDGET_COMPASS_1612` | (状态机专属刷新) | - |
| 复杂 | `WIDGET_TISSUE_GF/RAW_4012` | (状态机专属刷新) | - |

> **架构铁律**：复杂状态机组件（NDL_STOP/SYS/COMPASS/TISSUE）在 `arex_ui_update_task` 中有专属刷新逻辑，Data Router 仅做兜底处理。

**全屏刷新接口**：

```c
void arex_screen_refresh_all_widgets(void)
{
    /* 1. 同步左侧固定区配置 */
    for (uint8_t i = 0; i < g_left_widget_count; i++) {
        if (g_left_widgets[i].widget_id != WIDGET_EMPTY) {
            arex_widget_sync_data(g_left_widgets[i].widget_id);
        }
    }

    /* 2. 同步右侧 5F 自定义区配置 */
    for (uint8_t i = 0; i < g_5f_widget_count; i++) {
        if (g_5f_widgets[i].widget_id != WIDGET_EMPTY) {
            arex_widget_sync_data(g_5f_widgets[i].widget_id);
        }
    }
}
```

**使用场景**：
- update 任务在收到 `DIRTY_ALL`（全量更新）时调用
- 任何需要同时刷新左右两侧所有组件的场景
- 算法工程师新增指标后，只需在 `arex_widget_sync_data()` 的 switch 中加一行 case

---

## 九、待处理项

| 项目 | 文件 | 说明 |
|------|------|------|
| NEXT STOP 标签坐标重设 | `arex_screen.c` | 当前 HIDDEN，需按 10U 动态坐标重新计算 y |
| PO2 三值标签坐标重设 | `arex_screen.c` | 同上 |
| 左面板右边框 dashed | `arex_screen.c` | LVGL 不支持原生虚线边框，可用点阵 label 模拟 |
| 左面板 2 条虚线分隔线 | `arex_screen.c` | POD 区上方、GAS 区上方各加 1px 横线 |
| 卡片内各类布局细节 | `card_*.c` | 参见 `CARD_LAYOUT_FIXES.md` |
