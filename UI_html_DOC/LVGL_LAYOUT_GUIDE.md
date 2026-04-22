# AREX Pro AR-HUD: LVGL 绝对坐标引擎与像素级排版落地指南

> 基于最新 UI 规格表（物理界限、响应式区域、排版约束），确立"数据驱动 (Data-Driven)"与"纯数学绝对坐标 (Pure Math Absolute Layout)"的开发标准。
>
> 状态：**已生效** | 最后同步：2026-04-22

---

## 零号铁律：文档同步更新协议

改代码必改文档。每次修改了 C 代码、调整了任何 UI 布局参数，或者在 `g_sys_config` 中新增了配置项，**必须立刻同步更新本项目的 Markdown 文档**。

---

## 一、屏幕与 UI 基础参数

绝不允许在 C 代码中使用百分比或 Flex 适配，所有界限必须基于以下物理单位（1U = 10px）进行硬计算：

| 参数名称 | 设定值 | 换算 px | 说明 |
|----------|--------|---------|------|
| `PHYSICAL_RES` | 640×480 | 640×480 | 微显示屏物理极限分辨率，底层 `lv_disp` 锁死，不可逾越 |
| `GRID_BASE_UNIT` | 1U | 10px | 核心基准变量，所有宽高、间距必须是 10 的倍数 |
| `MARGIN_TOP` | 2U | 20px | 顶部光学视区留白 |
| `MARGIN_BOTTOM` | 4U | 40px | 底部防干涉留白（抵消面镜入水浮力上抬盲区） |
| `MARGIN_X_AXIS` | 3U | 30px | 左右各留 30px，为瞳距 (IPD) 左右平移校准预留空间 |
| `SAFE_ZONE` | 58U×40U | 580×400 | 实际可用 UI 安全画布，所有 LVGL 组件必须挂载于此 |

**开发铁律**：创建全局主容器 `ui_safe_zone`，大小锁定 580×400。利用 `lv_obj_align` 结合配置中的 `offset_x/y` 进行物理位移。其内部所有子组件的坐标起算点 `(0,0)` 即为安全区左上角。

### 当前代码中的宏定义

```c
// arex_ui_engine.h
#define AREX_BASE_U          10   /* 物理基准单位 1U = 10px */
#define AREX_PHYSICAL_W    640  /* 硬件屏幕极限宽 */
#define AREX_PHYSICAL_H    480  /* 硬件屏幕极限高 */
#define AREX_LEFT_ANCHOR_W   160  /* 左侧锚点固定宽度 16U */
```

**安全区宽高**通过 `g_sys_config.safe_zone_w/h` 动态配置（默认 580×400）。

---

## 二、响应式架构与区域划分 (Tech Mode)

在 Tech（左右宽屏）模式下，安全区 (580×400) 被严格划分为左、右两块及一个全局间距：

| 区域 | 尺寸 | 说明 |
|------|------|------|
| `LEFT_ANCHOR`（固定数据区） | 宽 16U (160px)，高满载 40U (400px) | 自上而下绝对推算 |
| `RIGHT_CANVAS`（动态卡片区） | 宽 40U (400px)，高满载 40U (400px) | tileview 承载 6 张卡片 |
| `GLOBAL_GAP` | 2U (20px) | 隔离左锚点与右画布 |

**布局推算公式**：

```c
uint16_t gap = g_sys_config.gap_u * AREX_BASE_U;        // 20px
uint16_t right_w = g_sys_config.safe_zone_w             // 580
                 - AREX_LEFT_ANCHOR_W                    // 160
                 - gap;                                  // 20
// right_w = 400px
```

**翻转行为**：系统执行 Layout Order 翻转时，仅需在 C 代码中交换 `LEFT_ANCHOR` 和 `RIGHT_CANVAS` 的计算起点的 X 坐标，**绝对禁止使用 Flex Reverse**。

---

## 三、字体系统与排版约束

| 字体级别 | 预编译字号 | 适用场景 | 高度约束 |
|----------|-----------|----------|----------|
| `FONT_HUGE` | 58px（实际使用 48px LVGL 内置上限） | 深度等最核心数值 | 高度分配必须 ≥ 8U (80px) |
| `FONT_MEDIUM` | 28px | 气瓶压、时间、常规模块数值 | 高度分配必须 ≥ 5U (50px) |
| `FONT_SMALL` | 14px | 所有模块标题 (Title) 与单位 (Unit) | 高度分配必须 ≥ 2U (20px) |

**当前代码中的字体映射**（`arex_ui_engine.h`）：

```c
uint8_t font_sz_huge = 58;   // 规范目标，实际 LVGL 内置最大 48px
uint8_t font_sz_med  = 28;   // FONT_MEDIUM
uint8_t font_sz_small= 14;   // FONT_SMALL
```

**双拼模块对称引擎 (Symmetry Override)**：在 NDL/TTS 或 POD 1/2 等 80×60 双拼模块中，若开启 `split_outward`，必须在创建 `lv_label` 时无视全局对齐，强行覆写：左块 `LV_TEXT_ALIGN_LEFT`，右块 `LV_TEXT_ALIGN_RIGHT`。

---

## 四、"裸对象"工厂标准 (The Naked Factory)

针对早期仿真中出现的"菜单按钮极度肥大、右侧文字被无情截断、左侧大字号往下溢出堆叠"三大灾难，必须全面接管 LVGL 渲染权。

### 核心铁律（所有对象创建必须遵守）

```c
// 1. 零边距 — 铲除幽灵内边距
lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
lv_obj_set_style_pad_all(obj, 0, LV_PART_SCROLLBAR);

// 2. 零边框 — 容器边框必须显式关闭
lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);

// 3. 禁止滚动 — 所有展示用容器
lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

// 4. 尺寸死锁 — 所有 label 必须锁死宽高
lv_obj_set_size(lbl, width, height);
lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);

// 5. 强制裁剪 — 防止子元素溢出父容器
lv_obj_set_style_clip_corner(obj, true, LV_PART_MAIN);
```

### 标准组件工厂函数模板

```c
/**
 * @brief AREX 标准模块/菜单生成工厂
 *        (零边距、强制裁剪、防溢出)
 *
 * 使用规范：
 *   - 所有宽高基于 1U=10px 硬计算
 *   - title 区固定分配 20px (2U) 高度
 *   - val 区吃满剩余高度
 *   - 内部文字左右各留 4px 呼吸空间
 */
lv_obj_t* create_arex_component(lv_obj_t* parent,
                                int x, int y, int w, int h,
                                const char* title, const char* val,
                                const lv_font_t* val_font,
                                lv_text_align_t align)
{
    // 1. 创建坐标与尺寸绝对死锁的底框
    lv_obj_t* comp = lv_obj_create(parent);
    lv_obj_set_pos(comp, x, y);
    lv_obj_set_size(comp, w, h);

    // 2. 铲除幽灵内边距 (解决菜单/模块异常肥大)
    lv_obj_set_style_pad_all(comp, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(comp, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(comp, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(comp, LV_OBJ_FLAG_SCROLLABLE);

    // 3. 强制溢出边界裁剪 (解决大字号往下溢出导致重叠)
    lv_obj_set_style_clip_corner(comp, true, LV_PART_MAIN);

    // 4. 组装标题区 (固定分配 20px / 2U 高度)
    lv_obj_t* title_lbl = lv_label_create(comp);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_lbl, AREX_LIGHT, 0);
    lv_obj_set_pos(title_lbl, 4, 0);
    lv_obj_set_size(title_lbl, w - 8, 20);  // 高度死锁 20px
    lv_obj_set_style_text_align(title_lbl, align, 0);
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);

    // 5. 组装数值区 (吃满剩余高)
    int val_y = 20;
    int val_h = h - val_y;

    if (val_h > 0 && val != NULL) {
        lv_obj_t* val_lbl = lv_label_create(comp);
        lv_label_set_text(val_lbl, val);
        lv_obj_set_style_text_font(val_lbl, val_font, 0);
        lv_obj_set_style_text_color(val_lbl, AREX_GREEN, 0);
        lv_obj_set_pos(val_lbl, 4, val_y);
        lv_obj_set_size(val_lbl, w - 8, val_h);
        lv_obj_set_style_text_align(val_lbl, align, 0);
        lv_label_set_long_mode(val_lbl, LV_LABEL_LONG_DOT);
    }

    return comp;
}
```

---

## 五、数据驱动的瀑布流推演 (Data-Driven Flow)

### 左侧锚点 Tech 模式渲染模板

```c
void render_left_anchor_tech(void)
{
    int current_y = 0;
    int anchor_w = 16U * GRID_BASE_UNIT;  // 160px
    lv_text_align_t global_align = (lv_text_align_t)g_sys_config.align_huge;

    // 1. 深度大通栏
    int depth_h = g_sys_config.h_depth * 10;  // 8U = 80px
    create_arex_component(s_left_anchor, 0, current_y, anchor_w, depth_h,
                          "DEPTH", "45.2", &lv_font_montserrat_48, global_align);
    current_y += depth_h + (g_sys_config.gap_u * 10);

    // 2. NDL / TTS 双拼模块
    int split_h = g_sys_config.h_ndl * 10;     // 6U = 60px
    int split_w = anchor_w / 2;                 // 80px

    lv_text_align_t left_align  = g_sys_config.split_outward ? LV_TEXT_ALIGN_LEFT  : global_align;
    lv_text_align_t right_align = g_sys_config.split_outward ? LV_TEXT_ALIGN_RIGHT : global_align;

    create_arex_component(s_left_anchor, 0,        current_y, split_w, split_h,
                          "NDL", "0", &lv_font_montserrat_28, left_align);
    create_arex_component(s_left_anchor, split_w, current_y, split_w, split_h,
                          "TTS", "24'", &lv_font_montserrat_28, right_align);
    current_y += split_h + (g_sys_config.gap_u * 10);

    // 3. POD 1 / POD 2 双拼模块
    split_h = g_sys_config.h_pod * 10;  // 6U = 60px
    create_arex_component(s_left_anchor, 0,        current_y, split_w, split_h,
                          "POD 1", "210 BAR", &lv_font_montserrat_20, left_align);
    create_arex_component(s_left_anchor, split_w, current_y, split_w, split_h,
                          "POD 2", "195 BAR", &lv_font_montserrat_20, right_align);
    current_y += split_h + (g_sys_config.gap_u * 10);

    // 4. BATT / W.TIME 双拼模块
    split_h = g_sys_config.h_batt * 10;  // 5U = 50px
    create_arex_component(s_left_anchor, 0,        current_y, split_w, split_h,
                          "BATT", "85%", &lv_font_montserrat_14, left_align);
    create_arex_component(s_left_anchor, split_w, current_y, split_w, split_h,
                          "W.TIME", "10:45", &lv_font_montserrat_14, right_align);
    current_y += split_h + (g_sys_config.gap_u * 10);

    // 5. GAS 中通栏
    int gas_h = g_sys_config.h_gas * 10;  // 6U = 60px
    create_arex_component(s_left_anchor, 0, current_y, anchor_w, gas_h,
                          "GAS", "TX 18/45", &lv_font_montserrat_28, global_align);
    current_y += gas_h + (g_sys_config.gap_u * 10);

    // 6. DIVE TIME 底部
    int time_h = g_sys_config.h_time * 10;  // 5U = 50px
    create_arex_component(s_left_anchor, 0, current_y, anchor_w, time_h,
                          "DIVE TIME", "38:14", &lv_font_montserrat_14, global_align);
}
```

### 右侧卡片区渲染模板

```c
void render_right_cards(void)
{
    int right_canvas_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                         - (g_sys_config.gap_u * AREX_BASE_U);  // 400px

    // 菜单项标准尺寸（与 INFO MENU 对齐）
    int item_h = 48;   // h_menu_item = 5U → 锁死 48px
    int item_w = right_canvas_w - 15;  // 右侧留 15px 呼吸
    int gap_y  = 8;    // gap_menu = 1U → 锁死 8px

    // card_gas.c 中的关键修复：
    // 气体选项 item 必须从 x=0 开始，与 INFO MENU 完全对齐
    for (int i = 0; i < AREX_GAS_COUNT; i++) {
        lv_coord_t row_y = 50 + i * (GAS_ROW_H + GAS_ROW_GAP);

        lv_obj_t* row = lv_obj_create(parent);
        lv_obj_set_size(row, item_w, GAS_ROW_H);   // 锁死尺寸
        lv_obj_set_pos(row, 0, row_y);              // x=0 对齐 INFO MENU
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        // ...
    }
}
```

---

## 六、自查表（每次修改 UI 必须确认）

| # | 检查项 | 通过标准 |
|---|--------|----------|
| 1 | `lv_obj_set_style_pad_all` 是否设置为 0 | 所有容器和 item 必须零边距 |
| 2 | 组件尺寸是否基于 1U=10px 乘以 config | 禁止硬编码 px 值（常量宏除外） |
| 3 | `lv_label` 是否设定绝对长宽 + `LV_LABEL_LONG_DOT` | 禁止 `LV_SIZE_CONTENT` |
| 4 | 菜单项 `x` 坐标是否从 0 开始 | 与 INFO MENU 对齐，不允许 `x=16` |
| 5 | 双拼模块是否正确应用 Symmetry Override | `split_outward` 时左/右 label 对齐方向相反 |
| 6 | 是否已同步更新本文档 | MD 文档与 C 代码同步更新 |

---

## 七、关键变更日志

| 日期 | 变更内容 | 影响文件 |
|------|----------|----------|
| 2026-04-22 | 新建本指南，建立绝对坐标排版标准 | 新增 `LVGL_LAYOUT_GUIDE.md` |
| 2026-04-22 | 修复 `card_gas.c` 气体选项宽度溢出：行 `x=16` → `x=0`，与 INFO MENU 对齐 | `card_gas.c` |
| 2026-04-22 | 统一 `right_canvas_w` 推算公式：`safe_zone_w - LEFT_ANCHOR_W - gap_u×10` | 所有 `card_*.c` |
