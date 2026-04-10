# HTML vs LVGL 布局差异对比文档

> 目标：让 LVGL 实现与 HTML 原型在视觉上完全对应  
> 状态：待确认后逐项修改

---

## 0. 说明

HTML 用的是 flex 布局 + CSS，LVGL 用绝对坐标。  
两者的**屏幕尺寸、主色板完全一致**，差异集中在：字体大小、边框样式（实线/虚线）、间距、局部分隔线、card 内部 padding。

---

## 1. 字体大小

| 位置 | HTML | LVGL | 差异 |
|------|------|------|------|
| DEPTH 数值（巨大字） | `58px` (`--font-huge`) | `montserrat_48` (48px) | **−10px** |
| 卡片标题 `.card-title` | `28 × 0.75 = 21px` | `montserrat_14` (14px) | **−7px** |
| 菜单条目 `.menu-item` | `28 × 0.75 = 21px` | `montserrat_20` (20px) | **−1px（可接受）** |
| GAS 名称（左面板） | `28 × 0.80 = 22px` | `montserrat_28` (28px) | **+6px（偏大）** |
| POD 数值（左面板） | `28 × 0.75 = 21px` | `montserrat_28` (28px) | **+7px（偏大）** |
| PO2 数值（左面板） | `28 × 0.70 = 19.6px` | `montserrat_14` (14px) | **−6px（偏小）** |

**结论：**
- DEPTH 需要从 `montserrat_48` 升到 `montserrat_48`（LVGL 最大内置字体，无 58px 可用，保持不变或自定义）
- 卡片标题应从 `montserrat_14` 升到 `montserrat_20`
- POD/GAS 左面板数值应从 `montserrat_28` 降到 `montserrat_20`（≈21px）
- PO2 左面板数值应从 `montserrat_14` 升到 `montserrat_20`

---

## 2. 左面板（#left-anchor）

### 2.1 整体

| 属性 | HTML | LVGL | 差异 |
|------|------|------|------|
| 宽度 | 180px | 180px | ✓ |
| 高度 | 480px | 480px | ✓ |
| 背景色 | `#000000` | AREX_BLACK | ✓ |
| 右边框 | `2px dashed #003300` | `2px solid AREX_DARK`（右侧） | **dashed → solid** |
| padding | `10px 12px`（上下10, 左右12） | `pad_all=10`（四边相同） | **右侧少2px** |
| 布局方式 | `flex-column + justify-content: space-between` | 绝对坐标 | 可接受 |

### 2.2 内部分隔线（LVGL 完全缺失）

HTML 左面板内有 **2条虚线分隔线**：

```
DEPTH
NDL / TTS
NEXT STOP
─ ─ ─ ─ ─ ─ ─ ─  ← border-top: 1px dashed #003300（POD 区上方）
POD 1 / POD 2
─ ─ ─ ─ ─ ─ ─ ─  ← border-top: 1px dashed #003300（GAS 区上方）
GAS
PO2
TIME（底部，justify-content: space-between 推到最底）
```

LVGL 目前没有任何分隔线，是视觉上差距最大的一点。

### 2.3 各数据块位置（绝对坐标）

HTML 是 flex 布局，LVGL 用的是手工计算的固定 y 坐标。  
HTML 中 POD/GAS/TIME 的位置是由 flex `space-between` 动态决定的，LVGL 硬编码了 192/252/440 这些值，大致对应，无需修改。

---

## 3. 右侧区域

### 3.1 Card 内部 padding

| 属性 | HTML | LVGL | 差异 |
|------|------|------|------|
| `.card` padding | `25px`（四边） | 各 card 无统一 padding，tile 本身 pad=0 | **缺少 25px 内边距** |

各卡片内容都贴着边缘，需要在每个 `card_*_create` 的内容起始位置加 25px 偏移（或给 tile 本身设 pad_all=25）。

### 3.2 Card 标题样式

| 属性 | HTML `.card-title` | LVGL | 差异 |
|------|------|------|------|
| 字体大小 | 21px | montserrat_14 | **−7px** |
| 颜色 | `#55FF55`（AREX_LIGHT） | AREX_GREEN（部分 card） | **颜色不一致** |
| 下边框 | `2px solid #003300` | 无 | **缺失** |
| padding-bottom | 8px | 无 | **缺失** |
| margin-bottom | 15px | 无 | **缺失** |

### 3.3 Wall UI

| 属性 | HTML `.wall-ui` | LVGL | 差异 |
|------|------|------|------|
| 边框 | `2px dashed #003300` | `2px solid AREX_DARK` | **dashed → solid** |
| padding | `15px 0`（上下15px，左右0） | `pad_all=8` | **8px → 15px 上下** |
| 文字居中 | `text-align: center` | `lv_obj_center(lbl)` | ✓ |
| charge 块符号 | `[■] [ ] [ ]`（实心方块） | `[#] [ ] [ ]` | **符号不同** |
| charge 块字体大小 | 32px | 继承父级（14px） | **偏小** |
| charge 块 letter-spacing | 10px | 无 | **缺失** |
| 显示/隐藏过渡 | `opacity 0.2s transition` | 直接 HIDDEN flag | **无淡入淡出** |

### 3.4 Scroll Dots（滚动点）

| 属性 | HTML `.scroll-dot` | LVGL | 差异 |
|------|------|------|------|
| 大小 | 6×6px | 6×6px | ✓ |
| 间距 | `gap: 8px` | flex space_evenly | 近似 ✓ |
| 激活态 box-shadow | `0 0 8px #00FF00` | 无 | **缺失（LVGL不支持）** |
| 激活态颜色 | AREX_GREEN | AREX_GREEN | ✓ |

> box-shadow 在 LVGL 中无法直接实现，可忽略。

### 3.5 Modal

| 属性 | HTML `.modal-box` | LVGL | 差异 |
|------|------|------|------|
| 边框粗细 | `4px` | 3px | **−1px** |
| 宽度 | `400px` | 380px | **−20px** |
| padding | `30px` | 24px | **−6px** |
| 遮罩透明度 | `rgba(0,0,0,0.95)`（95%） | LV_OPA_90（90%） | **−5%** |
| 文字居中 | `text-align: center` | 绝对坐标，未居中 | **缺失** |

### 3.6 Sub-menu Layer

| 属性 | HTML `#sub-menu-layer` | LVGL | 差异 |
|------|------|------|------|
| padding | `25px` | 20px | **−5px** |
| 动画时长 | `300ms` | 250ms | **−50ms** |
| 动画曲线 | `cubic-bezier(0.2, 0.8, 0.2, 1)` | `lv_anim_path_ease_out` | 接近，可接受 |

### 3.7 菜单条目 `.menu-item`

| 属性 | HTML | LVGL | 差异 |
|------|------|------|------|
| padding | `12px 15px`（上下12，左右15） | `pad_all=12` | **左右少3px** |
| 条目间距 | `gap: 8px`（flex gap） | flex column（无显式gap） | **缺少 8px 间距** |
| 字体大小 | 21px | montserrat_20 | ≈ ✓ |
| 激活边框色 | AREX_GREEN | AREX_GREEN | ✓ |

---

## 4. 修改优先级

### 🔴 高优先（视觉差距大）

| # | 项目 | 位置 | 改动 |
|---|------|------|------|
| 1 | 左面板缺少 2 条 dashed 分隔线 | `arex_screen.c` `left_panel_create()` | 在 POD 区上方、GAS 区上方各加 1px dashed 横线对象 |
| 2 | Card 内部缺少 25px padding | 各 `card_*_create()` | tile 设 `pad_all=25` 或内容整体偏移 25px |
| 3 | 卡片标题字体太小（14→20）且缺下边框 | 各 `card_*_create()` | 字体改 montserrat_20，加 border_bottom 2px AREX_DARK，加 padding_bottom=8 |
| 4 | Card 标题颜色统一为 AREX_LIGHT | 各 `card_*_create()` | 部分 card 用了 AREX_GREEN，改为 AREX_LIGHT |

### 🟡 中优先（有差距但不致命）

| # | 项目 | 位置 | 改动 |
|---|------|------|------|
| 5 | 左面板右边框改为 dashed | `arex_screen.c` `styles_init()` | LVGL 不支持原生 dashed border，用虚线 label 模拟或保持 solid |
| 6 | Wall padding 改为上下 15px | `arex_screen.c` `wall_create()` | `pad_top=15, pad_bottom=15, pad_left=0, pad_right=0` |
| 7 | Charge 块字体 32px | `arex_screen.c` `arex_screen_show_wall()` | wall label 字体改 montserrat_28 或自定义 |
| 8 | POD/GAS 左面板字体改为 20px | `arex_screen.c` `left_panel_create()` | `montserrat_28` → `montserrat_20` |
| 9 | PO2 左面板字体改为 20px | `arex_screen.c` `left_panel_create()` | `montserrat_14` → `montserrat_20` |
| 10 | Modal 边框 3→4px，宽度 380→400，padding 24→30 | `arex_screen.c` `modal_create()` | 数值调整 |
| 11 | 菜单条目间距加 8px gap | `card_info.c`, `card_setup.c`, submenu | flex gap 或每个 item margin_bottom=8 |
| 12 | Sub-menu padding 20→25px，动画 250→300ms | `arex_screen.c` | 数值调整 |

### 🟢 低优先（细节/LVGL 限制）

| # | 项目 | 说明 |
|---|------|------|
| 13 | Charge 符号 `#` → `■` | Unicode U+25A0，需确认 LVGL 字体是否包含 |
| 14 | 遮罩透明度 90%→95% | `LV_OPA_90` → `LV_OPA_95` |
| 15 | Scroll dot box-shadow | LVGL 不支持，跳过 |
| 16 | 动画曲线精确匹配 cubic-bezier | LVGL 无自定义 bezier，`ease_out` 已是最接近 |
| 17 | DEPTH 字体 48→58px | 需自定义字体生成，成本高 |

---

## 5. 不需要修改的项（已正确）

- 屏幕尺寸 640×480 ✓
- 左面板/右面板宽度分割 180/460 ✓
- 所有颜色常量（GREEN/LIGHT/DARK/BLACK/BG）✓
- DEPTH/NDL/TTS/NEXT STOP/POD/GAS/PO2/TIME 各 label 的 y 坐标 ✓
- NDL/TTS 的 x 坐标（0 和 90）✓
- TTS 绿底黑字高亮 ✓
- 滚动点位置（右侧垂直居中）✓
- Wall 上下位置（top:0 / bottom:420）✓
- Wall 宽度 460px ✓
- Sub-menu 从右侧推入动画（方向正确）✓
- Modal 边框颜色 AREX_GREEN，背景 AREX_BLACK ✓
- 激活态 绿底黑字 ✓
