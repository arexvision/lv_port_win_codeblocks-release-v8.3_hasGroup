# AREX Pro Dive Computer UI — 架构解读文档

> 基于 `UI_html/arex_ui_test_0.10.html` 原型，移植到 LVGL v8.3 (Windows/CodeBlocks)  
> 入口：`UI_main()` in `src/arex_ui/UI_main.c`

---

## 1. 整体目录结构

```
src/arex_ui/
├── UI_main.c               # 入口，初始化序列 + 仿真 tick 定时器
├── arex_data.h/c           # 全局状态数据模型（g_arex）
├── arex_ui_state.h/c       # 状态机核心（g_ui），三个输入处理函数
├── arex_screen.h/c         # LVGL 控件树创建 + 所有屏幕操作 API
├── arex_input.h/c          # 输入事件捕获（键盘/编码器 → 状态机）
├── arex_card_registry.h/c  # 卡片注册表（ID、title、create/update 回调）
└── cards/
    ├── card_info.c          # 0F: INFO MENU（5 条静态列表）
    ├── card_compass.c       # 1F: NAV COMPASS（canvas 绘制航向卷尺）
    ├── card_deco.c          # 2F: TISSUES & DECO（16 隔室柱状图 + GF/CNS/OTU）
    ├── card_gas.c           # 3F: GAS SWITCH（4 种气体，MOD 校验）
    ├── card_plan.c          # 4F: DIVE PLAN TRACK（canvas 绘制潜水剖面图）
    └── card_setup.c         # 5F: DIVE SETUP（5 条静态列表）
```

---

## 2. 启动序列：`UI_main()`

```
UI_main()
  │
  ├─ arex_data_init()          → 用 demo 值填充 g_arex（深度45.2m，航向265°…）
  ├─ arex_ui_state_init()      → 将 g_ui 清零，state=UI_DASH，dash_card=0
  ├─ arex_screen_create()      → 创建整个 LVGL 控件树（左面板 + tileview + 弹窗 + 子菜单层）
  │    ├── left_panel_create()
  │    ├── right_panel_create()  → 创建 tileview，按 card_order 调用 card_*_create()
  │    ├── wall_create()
  │    ├── modal_create()
  │    └── submenu_layer_create()
  ├─ arex_input_init(scr)      → 注册键盘/编码器事件回调
  ├─ arex_screen_refresh_left_panel()   → 左侧面板初始值填充
  ├─ arex_screen_scroll_to_card(0)      → 跳到 tile 0（INFO 卡）
  ├─ arex_screen_set_info_selection(0)  → 高亮第一条 LAST DIVE
  └─ lv_timer_create(sim_tick_cb, 1000ms)  → 每秒仿真 tick
```

**仿真 tick (`sim_tick_cb`) 每秒做：**
1. `compass.heading += 1° % 360`（航向缓慢漂移）
2. `dive.dive_time_s++`（潜水时间递增）
3. `arex_screen_refresh_left_panel()` → 刷新左面板数值
4. `arex_ui_refresh_all()` → 调用所有已注册卡片的 `update_cb()`

---

## 3. 数据模型：`arex_data.h/c`

### 全局状态实例：`arex_state_t g_arex`

```c
typedef struct {
    dive_data_t     dive;       // 深度/NDL/TTS/停留点/气瓶压力/潜水时间
    compass_data_t  compass;    // 航向/是否标记/目标航向/罗盘风格
    deco_data_t     deco;       // 16 隔室饱和度/GF99/SurfGF/CNS/OTU
    gas_data_t      gas;        // 当前气体index/ppo2[3]
    settings_data_t settings;   // mod_ppo2/保守度/亮度/card_order[6]
} arex_state_t;
```

### 气体表（静态，4 种）

| Index | name | MOD(m) |
|-------|------|--------|
| 0 | AIR | 56 |
| 1 | NX 32 | 34 |
| 2 | TX 18/45 | 68 |
| 3 | O2 100% | 6 |

### 关键设计点

- `settings.card_order[6]`：控制 tileview 中卡片的显示顺序（indirection 层），默认 `{0,1,2,3,4,5}`
- `deco.surf_gf > 100` 时，SurfGF 数值使用 HTML `.highlight-invert`：绿底黑字（非红色闪烁）；组织条 `≥90%` 使用与 HTML `.t-fill.danger` 相同的 `flashInvert` 节奏（300ms，对应 `--flash-speed 0.3s`）
- `dive.depth` 是浮点数，左面板每秒更新

---

## 4. 状态机：`arex_ui_state.h/c`

### 状态枚举

```
UI_DASH         (0)  — 主卡片滚动模式
UI_INFO         (1)  — INFO 菜单列表激活
UI_SETUP        (2)  — SETUP 菜单列表激活
UI_EDIT_GAS     (3)  — 3F 气体选择光标移动中
UI_MODAL_GAS    (4)  — 气体切换确认弹窗已打开
UI_MODAL_COMPASS(5)  — 清除罗盘目标确认弹窗
UI_SUB_MENU     (6)  — 子菜单层已弹出（从右侧滑入）
UI_MODAL_ACT    (7)  — 通用动作弹窗（1秒自动关闭）
UI_EDIT_VALUE   (8)  — 数值内联编辑（例如 MOD PO2）
```

### 全局 UI 上下文：`arex_ui_ctx_t g_ui`

```c
typedef struct {
    arex_ui_state_t  state;           // 当前状态
    uint8_t  dash_card;               // 当前卡片索引（0~5）
    uint8_t  menu_info_idx;           // INFO 菜单光标
    uint8_t  menu_setup_idx;          // SETUP 菜单光标
    uint8_t  sub_menu_idx;            // 子菜单光标
    uint8_t  gas_cursor;              // 气体列表光标（UI_EDIT_GAS 期间）
    uint8_t  wall_charge;             // 边界碰撞计数（0~3，到3才穿越）
    int8_t   wall_dir;                // +1=底部  -1=顶部
    arex_sub_history_t sub_history[4];// 子菜单导航栈
    uint8_t  sub_history_depth;
    struct { float value,min,max,step,original; uint8_t item_index; bool active; } edit_ctx;
    const char *sub_title;
    const char *sub_items[8];
    uint8_t     sub_item_count;
    arex_ui_state_t sub_parent;       // 进入子菜单时的父状态
} arex_ui_ctx_t;
```

### 三个公开输入处理函数

| 函数 | 触发 | 作用 |
|------|------|------|
| `ui_handle_rotate(int8_t dir)` | 上下键/编码器旋转 | 卡片滚动、菜单移动、数值调整 |
| `ui_handle_click()` | Enter/编码器按下 | 确认选择、进入子菜单、标记航向 |
| `ui_handle_back()` | ESC/Backspace | 取消/退出/关闭弹窗 |

---

## 5. 核心交互流程

### 5.1 Wall-Charge 边界穿越机制

**`dash_card` 语义（与 HTML 一致）：**
- `dash_card` = card 在 `card_order[]` 中的位置（0~5）
- `dash_card=0` → INFO（仅 wall-charge 可进），`dash_card=1` → COMPASS，`dash_card=2` → DECO，`dash_card=3` → GAS，`dash_card=4` → PLAN，`dash_card=5` → SETUP（仅 wall-charge 可进）

```
card_order 布局：[0]=INFO  [1]=COMPASS  [2]=DECO  [3]=GAS  [4]=PLAN  [5]=SETUP
                  ↑ wall-charge 才能进                            ↑ wall-charge 才能进

DASH 可滚动范围：dash_card ∈ [1, AREX_CARD_COUNT-2]（可配置）

在 UI_DASH 下：
  dash_card==1 且继续向上 → wall_charge++，显示顶部墙 "[#][ ][ ]"
                            → tileview 向下偏移 charge×20px 然后弹回（橡皮筋感）
  连续3次 → 穿越到 UI_INFO（滚动到 tile_pos=0），wall_charge 清零

  dash_card==AREX_CARD_COUNT-2 且继续向下 → wall_charge++，显示底部墙
                            → tileview 向上偏移 charge×20px 然后弹回
  连续3次 → 穿越到 UI_SETUP（滚动到 tile_pos=AREX_CARD_COUNT-1）

  任何中途改变方向 → wall_charge = 0，墙UI隐藏，tileview 立即归位
```

**橡皮筋动画实现（`wall_nudge_tileview`）：**
对 `s_tileview` 做 `lv_obj_set_y` 动画：350ms ease-out 平滑推到 `charge×20px`，停在那里直到 wall 清零。
`arex_screen_hide_walls` 时立即 `set_y(0)` 归位。
对应 HTML 的 `transition: 0.35s cubic-bezier(0.2,0.8,0.2,1)` + `updateElevator(wallCharge * 20)`，无回弹。

UI_INFO 退出（wall-charge 或 ESC）→ 返回 DASH，dash_card=1（COMPASS）
UI_SETUP 退出（wall-charge 或 ESC）→ 返回 DASH，dash_card=AREX_CARD_COUNT-2（PLAN）

> **启动行为说明：** 启动直接进入 `UI_INFO` 状态，显示 INFO 卡（tile 0），光标聚焦第一条 LAST DIVE。
> 在 INFO 菜单底部 wall-charge（连续3次向下）→ 进入 `UI_DASH`，从 COMPASS（tile 1）开始。
> 在 DASH 顶部 wall-charge（COMPASS 处连续3次向上）→ 返回 `UI_INFO`。

### 5.2 气体切换流程（3F 卡片）

> **【重点 · CONFIG GAS 退出规则】**  
> **气体在当前深度不可用**（`dive.depth >` 该气体的 **MOD**，不适用深度）时，**不能**用确认键完成切换：弹窗内 **Enter/点击** 仅触发 `arex_screen_pulse_modal()` 震动，**不会**改 `active_idx`、**不会**回到仪表盘。  
> 此时**必须**通过 **返回键（Back / ESC）** 退出 CONFIG GAS：`UI_MODAL_GAS` 先回到 `UI_EDIT_GAS`，再按一次返回才回到 `UI_DASH`。  
> **不可用气体时，不得以「确认切换」的方式离开气体配置流程。**

```
UI_DASH（当前卡片为 GAS，card_order index=3）
  │
  CLICK → UI_EDIT_GAS，gas_cursor = active_idx
  │        card_gas_update() 高亮当前光标行
  │
  ROTATE → gas_cursor 循环移动（0→1→2→3→0…）
  │        card_gas_update() 重绘
  │
  CLICK → UI_MODAL_GAS，arex_screen_show_modal_gas()
  │
  ├─ 深度 ≤ MOD → CLICK 确认：active_idx = gas_cursor，返回 UI_DASH
  └─ 深度 > MOD（气体不可用）→ CLICK 无效：modal 震动；仅能通过 BACK 退出（见上文重点）
  
  BACK / ESC:
    UI_MODAL_GAS → UI_EDIT_GAS（关弹窗，留在气体编辑）
    UI_EDIT_GAS  → UI_DASH（退出 CONFIG GAS）
```

### 5.3 罗盘标记流程（1F 卡片）

```
UI_DASH（当前卡片为 COMPASS）
  │
  CLICK（未标记）→ compass.marked=true，target=heading，canvas 画黄色竖线
  │
  CLICK（已标记）→ UI_MODAL_COMPASS 弹窗
  │
  CLICK 确认 → compass.marked=false，清除标记，返回 UI_DASH
  ESC      → 取消，返回 UI_DASH
```

### 5.4 子菜单流程（INFO/SETUP 菜单）

```
UI_INFO / UI_SETUP
  │
  CLICK → arex_screen_open_info/setup_submenu(item_idx)
           - 用内置字符串表填充 sub_items[]
           - submenu_slide_in()：从右侧滑入（lv_anim 250ms ease-out）
           - state → UI_SUB_MENU，sub_parent = UI_INFO/UI_SETUP
  │
  UI_SUB_MENU
    ROTATE → sub_menu_idx 移动
    CLICK → arex_screen_handle_submenu_select()
             "< BACK" → arex_screen_close_submenu()（slide out，恢复 sub_parent 状态）
             其他     → （扩展实现中）
    ESC → arex_screen_close_submenu()
```

### 5.5 数值内联编辑（MOD PO2）

```
UI_SUB_MENU（DIVE SETUP 子菜单，"MOD PO2: X.X" 行高亮）
  │
  CLICK "MOD PO2: X.X" → arex_screen_begin_edit_value()
    - edit_ctx = {value=当前值, min=1.0, max=1.6, step=0.1, original=旧值}
    - UI 变化：行从绿底黑字恢复为黑底绿字 + 绿边框
    - 布局：与 HTML `.menu-item` 一致 — `display:flex; justify-content:space-between; align-items:center`（LVGL：`LV_LAYOUT_FLEX` + `LV_FLEX_ALIGN_SPACE_BETWEEN`），三列：`MOD PO2: ` | 绿底数值 | `^ v`
    - value badge：绿底(AREX_GREEN) + 黑字(AREX_BLACK)
    - ▲▼ 箭头：AREX_LIGHT 灰色，贴右
    - 启动 600ms 定时器 toggle 闪烁
  │
  ROTATE → edit_ctx.value ± step（clamp 到 min/max）
            arex_screen_refresh_edit_value() 更新 badge 内数值
            闪烁不中断（定时器继续）
  │
  CLICK  → 提交：g_arex.settings.mod_ppo2 = edit_ctx.value
            停止闪烁，清理 badge/arrows，恢复完整标签
            返回 UI_SUB_MENU（该行恢复选中态）
  ESC    → 取消：恢复 edit_ctx.original
            停止闪烁，清理 badge/arrows，恢复旧值
            返回 UI_SUB_MENU
```

> **实现细节：** `s_edit_flash_timer`（`lv_timer`，600ms）持续切换 badge 背景色（绿↔黑）与数值 label 文字色（黑↔绿），`edit_flash_start()` 不清空 `s_edit_flash_badge`/`s_edit_flash_val_lbl`，确保定时器回调有效。

---

## 6. 屏幕布局：`arex_screen.h/c`

### 整体布局（640×480px）

```
┌──────────────────────────────────────────────────────┐
│  Left Panel 180px  │      Right Canvas 460px         │
│                    │  ┌──────────────────────────┐   │
│  DEPTH  45.2       │  │  Tileview（垂直6卡片）    │   │
│  NDL 0  TTS 24'    │  │  card_order[0] → tile 0  │   │
│  NEXT STOP 21m 3'  │  │  card_order[1] → tile 1  │   │
│  POD1 210  POD2195 │  │  ...                     │   │
│  GAS TX18/45       │  │  card_order[5] → tile 5  │   │
│  PO2 1.2|1.2|1.3   │  └──────────────────────────┘   │
│  TIME 38:14        │  [墙UI top/bottom 隐藏叠加]     │
│                    │  [子菜单层 从右侧推入]          │
│                    │  [弹窗遮罩层 hidden]            │
│                    │  [scroll dots 右侧 6个点]       │
└──────────────────────────────────────────────────────┘
```

### 内部静态控件句柄（全部 `static` 在 arex_screen.c）

| 句柄 | 说明 |
|------|------|
| `s_scr` | 根 screen 对象 |
| `s_left_panel` | 左面板容器 |
| `s_tileview` | 右侧卡片 tileview |
| `s_lbl_depth/ndl/tts/…` | 左面板各数值标签 |
| `s_wall_top/bottom` | 边界指示墙（默认 HIDDEN） |
| `s_scroll_dots[6]` | 右侧滚动点 |
| `s_modal / s_modal_box` | 弹窗遮罩 + 内容框 |
| `s_submenu_layer` | 子菜单全屏层（默认 x=460 隐藏在右侧屏外）|
| `s_info_list / s_setup_list` | 由 card_info.c/card_setup.c 注册 |

### Tileview 工作方式

- 创建时按 `card_order[]` 顺序逐个调用 `lv_tileview_add_tile()`
- 禁用自身 touch/scroll（`LV_OBJ_FLAG_SCROLLABLE` 已清除）
- 切换由 `arex_screen_scroll_to_card()` 调用 `lv_obj_set_tile()` 带动画

---

## 7. 卡片系统：`arex_card_registry.h/c`

### 卡片描述符

```c
typedef struct {
    arex_card_id_t  id;             // 稳定ID（0~5）
    const char     *title;
    lv_obj_t       *tile_obj;       // create 后填入
    void (*create_cb)(lv_obj_t *parent);   // 一次性建控件
    void (*update_cb)(void);               // 每 tick 刷新数据
    void (*on_enter_cb)(void);             // 滚动到此卡时（可选）
} arex_card_reg_t;
```

### 6张卡片一览

| ID | 文件 | 标题 | 核心实现 |
|----|------|------|----------|
| 0 CARD_ID_INFO | card_info.c | INFO MENU | 5条静态列表，`arex_screen_register_info_list()` 注册到屏幕层 |
| 1 CARD_ID_COMPASS | card_compass.c | NAV COMPASS | 420×380 canvas，`draw_tape()` 每帧重绘，目标航向黄线 |
| 2 CARD_ID_DECO | card_deco.c | TISSUES & DECO | 16 竖条贴卡片底（对齐 HTML `margin-top:auto`）；条槽 `AREX_DARK` 半透明；填充绿 / `>70%` 浅绿 / `≥90%` 反色闪烁；SurfGF>100 绿底黑字 |
| 3 CARD_ID_GAS | card_gas.c | GAS SWITCH | 4行 `lv_obj` 容器，光标/激活/超MOD三态颜色，实时 PPO2 计算 |
| 4 CARD_ID_PLAN | card_plan.c | DIVE PLAN TRACK | 380×280 canvas，网格+折线+当前位置黄点 |
| 5 CARD_ID_SETUP | card_setup.c | DIVE SETUP | 5条静态列表，`arex_screen_register_setup_list()` 注册 |

### `arex_card_get(pos)` vs `arex_card_get_by_id(id)`

```c
// 通过显示位置取（走 card_order 间接层）
arex_card_get(0)  →  s_registry[ g_arex.settings.card_order[0] ]

// 通过稳定ID取（不走间接层）
arex_card_get_by_id(CARD_ID_GAS)  →  s_registry[3]
```

---

## 8. 输入处理：`arex_input.c`

```
arex_input_init()
  │
  ├─ 创建 1×1px 透明 btn（catcher），挂在 scr 上
  ├─ group_kbd：keypad → KEY 事件 → key_event_cb()
  └─ group_enc：encoder，固定 editing=true → enc_diff → LEFT/RIGHT → key_event_cb()
                encoder 按下 → enc_click_cb() → ui_handle_click()

key_event_cb():
  LV_KEY_UP / LV_KEY_LEFT    → ui_handle_rotate(-1)
  LV_KEY_DOWN / LV_KEY_RIGHT → ui_handle_rotate(+1)
  LV_KEY_ENTER               → ui_handle_click()
  LV_KEY_ESC / LV_KEY_BACKSPACE → ui_handle_back()
```

**注意：** 编码器组强制 `editing=true`，这意味着编码器旋转在 LVGL 内部会发出 `LV_KEY_LEFT/RIGHT`（而非 `LV_KEY_UP/DOWN`），与键盘组共用同一个 `key_event_cb`。

---

## 9. 各卡片详细实现

### 9.1 card_compass.c（1F）

- 与规范对齐：Canvas **420×140**px，标题下 y=50：
  - 滑动框(Tape)：高 **60px**，2px `AREX_DARK` 边框
  - 航向数字：最接近规范 46.4px，字库用 `AREX_FONT_HUGE`(48px)，居中
  - 中心游标：**宽 4px**，高 60px，`AREX_GREEN`
- `draw_tape(heading)` 每次完整清屏重绘：
  - 以 heading 为中心，±60° 范围画刻度线，每度 3px
  - major（每45°）画高刻度+方位字母（N/NE/E/SE/S/SW/W/NW）
  - minor（每15°）画中刻度；其余画短刻度
  - **不输出 `°` 字符** — `lv_font_courier_*` 仅含 ASCII `0x20-0x7E`，`U+00B0` 会显示为方框
  - 若 `compass.marked`：在目标航向对应位置画黄色竖线，下方显示 `TARGET 265`（同上无度符号）
- 每秒通过 `card_compass_update()` 触发重绘

### 9.2 card_deco.c（2F）

- 布局：与 `arex_ui_test_0.10.html` 一致 — `.card` 为列 flex，`.tissue-section-title` 使用 `margin-top:auto`，隔室图在卡片**最下方**；LVGL 用 `BARS_Y` 从 `TILE_H` 向上反算；规范 Section Title Y≈250，`BOTTOM_PAD=36`。
- 顶部三行 `.deco-grid`（y=60/107/154）：文案 `SurfGF`、`GF LOW / HIGH` 等与 HTML 一致。
- **SurfGF**：`surf_gf > 100` 时为 `.highlight-invert`（`AREX_GREEN` 底 + `AREX_BLACK` 字 + 水平 padding 4），无红字闪烁。
- 16 个 `lv_bar`（规范：间距 4px，槽 `AREX_DARK`+`LV_OPA_50`，填充≤70%绿/>70%浅绿/≥90%危险闪烁）：
  - 槽道：`.t-bar` 对应 `AREX_DARK` + `LV_OPA_50`。
  - 填充：`≤70%` → `AREX_GREEN`；`>70%` 且 `<90%` → `AREX_LIGHT`（`.t-fill.high`）；`≥90%` → `.t-fill.danger` 式反色闪烁（**300ms** 定时器，绿/黑切换）。
- M 值虚线：容器高度 80px 的 **top 20%**，`LV_OPA_50`；右侧 `M-VALUE` 小字标签。

### 9.3 card_gas.c（3F）

> **【与 §5.2 一致】** 当前深度超过某行 MOD、该气体不可用时，确认切换无效，**须用返回键退出 CONFIG GAS**（见 §5.2 重点说明）。

- 规范：行高 **49px**，间距 8，padding 上下 **12px**，左右 **15px**；字体 `AREX_FONT_TITLE`(20px)。
- 与 HTML `.menu-list` / `.menu-item` / `.static-active` / `.active` / `.hint-text` / `#gas-card-status` 对齐：
  - 气体名：`AREX_GREEN` 左对齐；MOD/PO2：`AREX_LIGHT`，右上/右下对齐。
  - 光标行（`UI_EDIT_GAS && gas_cursor==i`）：`.active` — 绿底、黑字、绿边框。
  - 当前呼吸气（`active_idx==i`，非光标）：`.static-active` — **黑底**、绿边框、气体名 `AREX_GREEN`。
  - 普通行：黑底、绿字、`AREX_DARK` 边框。
  - `#gas-card-status`：右上角 `[EDIT MODE]`（`AREX_FONT_SMALL`，`AREX_GREEN`）。
  - `.hint-text`：底部提示，`AREX_LIGHT` + `LV_OPA_60`；编辑/空闲文案与 HTML 一致。
- **超 MOD**（`depth > MOD`）：红边框；若该行同时为编辑光标，边框保持绿色。
- PPO2 简化计算：`depth/10 * 0.21`，文案 `PO2 %.2f`。

### 9.4 card_plan.c（4F）

- 规范参数（已对齐）：
  - 外壳：2px 实线 `AREX_DARK`，Padding 10px
  - 画布：**400×320px**（`CHART_W`/`CHART_H`）
  - 背景网格：横间距按 CHART_W/53px 步进，纵间距按 CHART_H/64px 步进；线宽 1px，`LV_OPA_51`（透明度 20%）
  - 坐标轴文字：`AREX_FONT_SMALL`(14px)，`LV_OPA_191`（透明度 75%）
  - 走势线：实线，粗细 **4px**，`AREX_GREEN`
  - 停留点：半径 **6px**，填充黑，描边 2px `AREX_GREEN`（仅水平段且停留≥3min 时绘制）
- 当前位置：黄色圆点，动态由 `g_arex.dive.dive_time_s` 和 `g_arex.dive.depth` 计算，带 "NOW" 标签

---

## 10. 颜色常量与字体

```c
/* 颜色 */
AREX_GREEN  = #00FF00   // 主色（文字、指针、激活态）
AREX_LIGHT  = #55FF55   // 副色（标签、辅助文字、次要数值）
AREX_DARK   = #003300   // 边框、刻度线、非激活背景
AREX_BLACK  = #000000   // 卡片背景
AREX_BG     = #050505   // 屏幕根背景

/* 字体（Courier New Bold）*/
AREX_FONT_SMALL    = 14px   // 标签/单位/Status Badge
AREX_FONT_TITLE    = 20px   // 菜单项/卡片标题（规范21px最接近）
AREX_FONT_MEDIUM   = 28px   // 数据值
AREX_FONT_HUGE     = 48px   // 深度大数字（规范58px）
AREX_FONT_DERIVED  = 20px   // 21px派生（规范0.75x≈21px）
```

## 10.1 弹窗参数（规范值）

```c
modal_overlay:  bg #000000, opacity 95% (LVGL opa=242)
modal_box:      400×? px, bg #000000, border 4px #00FF00, padding 30px
```

## 10.2 滚动指示器（规范值）

```c
位置: 右侧 8px，垂直居中（LV_ALIGN_RIGHT_MID）
大小: 6×6px（border-radius:0 → 正方形）
间距: 纵向 gap 8px
默认: #003300 (AREX_DARK)
激活: #00FF00 (AREX_GREEN) + shadow_width=8, shadow_color=#00FF00
```

## 10.3 左侧面板 PO2（规范值）

```c
PO2 标签: AREX_FONT_SMALL(14px), AREX_LIGHT
三个值段 + 两个 | 分隔符，x=30/66/102, 间距≈28px
| 分隔符: 透明度 30% (LV_OPA_30)
DEPTH 大数字: AREX_FONT_HUGE(48px), AREX_GREEN, 字间距 -2px, y=24
DEPTH 标签:  AREX_FONT_SMALL(14px), AREX_LIGHT, y=10
```

---

## 11. HTML 原型 → LVGL 对应关系

| HTML 元素 | LVGL 实现 |
|-----------|-----------|
| `#left-anchor` | `s_left_panel`（180px 绝对定位容器） |
| `#elevator-track`（translateY 动画） | `s_tileview`（`lv_obj_set_tile` 带动画） |
| `.card`（6个 div） | 6个 tileview tile + card_*_create |
| `#top-wall-ui / #bottom-wall-ui` | `s_wall_top / s_wall_bottom`（HIDDENFlag 切换） |
| `#canvas-modal` | `s_modal + s_modal_box` |
| `#sub-menu-layer`（translateX 动画）| `s_submenu_layer`（lv_anim 水平滑入/滑出） |
| `#scroll-indicator` | `s_scroll_dots[6]`（激活时 shadow_width=8, shadow_color=AREX_GREEN） |
| `.menu-list`/`.menu-item`/`.static-active`/`.active` | `card_gas.c`（行 428×49、间距8、padding 12/15px、三态颜色） |
| JS `gasData[]` | `AREX_GAS_TABLE[]` |
| JS `setInterval 150ms`（罗盘） | `sim_tick_cb` 1000ms + `card_compass_update` |
| JS `flashInvert`（`.t-fill.danger`） | `tissue_danger_flash_cb` 300ms（card_deco.c，仅 `pct≥90` 的条） |
| JS `renderModal('GAS')` MOD 警告 | `arex_screen_show_modal_gas()` hint 字符串切换 |

---

## 12. 数据流总览

```
main.c (WinMain)
  └─ UI_main()
       ├─ 初始化序列（见第2节）
       └─ lv_timer(sim_tick_cb, 1000ms)
            │
            ├─ 更新 g_arex（heading++, dive_time_s++）
            ├─ arex_screen_refresh_left_panel()   [读 g_arex → 写 s_lbl_*]
            └─ arex_ui_refresh_all()
                 └─ for each card: card->update_cb()
                      └─ 读 g_arex / g_ui → 写 LVGL 控件

用户输入（键盘/编码器）
  └─ key_event_cb() / enc_click_cb()
       └─ ui_handle_rotate / click / back
            ├─ 修改 g_ui.state / g_ui.dash_card / g_ui.gas_cursor / …
            ├─ 修改 g_arex.gas.active_idx / compass.marked / settings.mod_ppo2 / …
            └─ 调用 arex_screen_* 函数更新控件外观
```

---

## 13. 重要设计约定

1. **卡片不直接写状态**：card_*.c 只读 `g_arex` 和 `g_ui`，不修改它们。状态修改统一在 `arex_ui_state.c` 中进行。

2. **screen 层是哑的**：`arex_screen.c` 的函数只负责操作控件，业务判断（如气体深度校验）在状态机里完成。

3. **card_order 间接层**：tileview 的物理顺序在创建时固定，但用户可以通过修改 `g_arex.settings.card_order[]` 改变各卡片的逻辑位置，`arex_card_get(pos)` 负责解引用。

4. **注册回调**：card_info.c 和 card_setup.c 通过 `arex_screen_register_info_list()` / `arex_screen_register_setup_list()` 把它们内部创建的列表对象告知 screen 层，避免在 arex_screen.c 中重复创建控件。

5. **Wall-charge 防误触**：连续3次才穿越边界，防止单次抖动误触进入菜单。

6. **Modal 震动反馈**：气体切换超 MOD 时，`arex_screen_pulse_modal()` 用 lv_anim 做左右 ±6px 抖动（2次重复，80ms），对应 HTML 的 `scale(1.05)` 弹跳。

---

## 14. 子菜单动作路由（v0.10 新增）

`arex_screen_handle_submenu_select()` 在 `arex_screen.c` 中全面实现，按 `cur_title` 分支路由：

### 14.1 动作路由表

| 当前子菜单标题 | 选中项规则 | 执行动作 |
|---|---|---|
| `GAS SWITCH` | `SELECT XXX` | 更新 `g_arex.gas.active_idx`，刷新 gas 卡和左面板，关闭子菜单 |
| `CONSERVATISM` | `LOW/MED/HIGH` 开头 | 更新 `g_arex.settings.conservatism`，更新 SETUP 菜单 badge，关闭子菜单 |
| `BRIGHTNESS` | 精确匹配 `LOW/MED/HIGH/MAX` | 更新 `g_arex.settings.brightness`，更新 SETUP 菜单 badge，关闭子菜单 |
| `DIVE SETUP`（嵌套） | `MOD PO2:` 开头 | 调用 `arex_screen_begin_edit_value()` → `UI_EDIT_VALUE` |
| `DIVE SETUP`（嵌套） | 其他项 | `arex_screen_show_modal_act(text)` 通用动作弹窗 |
| 任意标题 | 末尾含 `>` | 解析标题，调用 `arex_screen_open_nested_submenu()` 进入下一级 |
| 任意标题 | `< BACK` | `arex_screen_close_submenu()` 退出/回上级 |
| 其他所有 | 任意 | `arex_screen_show_modal_act(text)` 通用动作弹窗（1秒自动关闭） |

### 14.2 嵌套子菜单（三级）

```
SETUP → SYSTEM SETUP（二级）
           ├─ MODE SETUP >   → [AIR / NITROX / 3 GAS NX / GAUGE]
           ├─ DIVE SETUP >   → [SALINITY / MOD PO2 / SAFETY STOP / ALTITUDE]
           │                      └─ MOD PO2 → UI_EDIT_VALUE（内联数值编辑）
           ├─ AI SETUP >     → [PAIR T1 / PAIR T2 / GTR MODE: ON]
           ├─ ALERTS SETUP > → [DEPTH ALARM / TIME ALARM / LOW NDL / TEST VIB]
           └─ DISPLAY / SYS >→ [UNITS / DATE & CLOCK / LOG RATE / BLUETOOTH / RESET]
```

**导航栈** (`g_ui.sub_history[]`, 最深 4 层)：
- 进入嵌套时 `submenu_history_push()` 保存当前标题和光标位置
- `arex_screen_close_submenu()` 检测 `sub_history_depth > 0` 时执行 pop，重新 populate 上一级内容
- `sub_history_depth == 0` 时执行 `submenu_slide_out()`，返回 `sub_parent` 状态

### 14.3 SETUP badge 更新

`card_setup.c` 每个菜单项有两个子 label：
- child 0：标题文字（`> GAS SWITCH` 等）
- child 1：右侧 badge（`MED` / `HIGH` / 空）

`arex_screen_update_setup_badge(item_idx, value)` 通过 `s_setup_list` 直接写 child 1。  
`card_setup_update()` 每 tick 从 `g_arex.settings` 同步 CONSERVATISM / BRIGHTNESS 的 badge 文字。

### 14.4 INFO 子菜单动态数据

`arex_screen_open_info_submenu()` 在打开前调用 `build_info_submenu(idx)` 从 `g_arex` 动态构建字符串：

| 子菜单 | 动态字段来源 |
|--------|-------------|
| LAST DIVE | `g_arex.dive.depth`，`g_arex.dive.dive_time_s` |
| TISSUE & TOX | `g_arex.deco.cns_pct`，`g_arex.deco.otu` |
| GAS & CALC | `AREX_GAS_TABLE[g_arex.gas.active_idx].name` |
| SENSOR & DEVICE | `g_arex.dive.pod1_bar`，`g_arex.dive.pod2_bar` |

### 14.5 DIVE SETUP 嵌套菜单中 MOD PO2 实时值

`build_nested_dive_setup()` 在每次打开该子菜单前调用，将 `g_arex.settings.mod_ppo2` 格式化进 `s_modppo2_str[]`，保证显示最新值。编辑提交后 `arex_screen_commit_edit_value()` 直接更新同一 label。

### 14.6 新增 arex_screen.h 公开 API

| 函数签名 | 作用 |
|----------|------|
| `arex_screen_open_nested_submenu(title, items, count)` | 把当前状态压栈，原地替换子菜单内容（无滑动动画） |
| `arex_screen_update_setup_badge(item_idx, value)` | 更新 SETUP 菜单行的右侧 badge label |
| `arex_screen_show_modal_act(action_text)` | 显示通用动作弹窗，1秒后自动关闭，状态回 `UI_SUB_MENU` |
| `arex_screen_begin_edit_value(item_idx, value, min, max, step)` | 初始化 `edit_ctx`，进入 `UI_EDIT_VALUE` 状态；UI：行黑底+绿边框；整行 flex `space-between`（对齐 HTML 第 137 行）；数值 `X.X` 在绿底 badge 内居中；箭头 `^ v`；`s_edit_flash_timer` 600ms 切换 badge 背景（绿↔黑）与数值文字颜色（黑↔绿） |
## 15. LVGL 绝对坐标排版标准（v2026-04-22）

> 详细规范见 `UI_html_DOC/LVGL_LAYOUT_GUIDE.md`。本节为快速索引。

### 15.1 物理参数与安全区

| 参数 | 值 | 说明 |
|------|-----|------|
| `AREX_BASE_U` | 10px | 1U = 10px，所有尺寸必须是 10 的倍数 |
| `AREX_PHYSICAL_W/H` | 640x480 | 物理屏幕锁死，不可逾越 |
| `SAFE_ZONE` | 580x400 | 安全画布，offset_x/y 驱动物理位移 |
| `AREX_LEFT_ANCHOR_W` | 160px | 左侧锚点固定宽度 |
| `GLOBAL_GAP` | 20px | 左/右分区隔离间距 |

### 15.2 Tech 模式布局推算

```
uint16_t gap = g_sys_config.gap_u * AREX_BASE_U;  // 20px
uint16_t right_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - gap;  // 400px
```

### 15.3 渲染铁律（自查清单）

| # | 规则 | 错误代码 |
|---|------|----------|
| 1 | `pad_all = 0` 所有容器零边距 | `pad_all = 8` |
| 2 | `border_width = 0` 所有容器零边框 | `border_width = 2` |
| 3 | 禁止 Flex 布局 | `lv_obj_set_layout(obj, LV_LAYOUT_FLEX)` |
| 4 | label 尺寸锁死 | `lv_obj_set_size(lbl, LV_SIZE_CONTENT, ...)` |
| 5 | `LV_LABEL_LONG_DOT` 防截断 | 无 `long_mode` |
| 6 | 菜单项 x=0 与 INFO MENU 对齐 | `x=16`（溢出） |
| 7 | `clip_corner=true` 防止子元素溢出 | 无裁剪 |

### 15.4 气体选项宽度修复记录

**问题**：`card_gas.c` 气体选项宽度溢出，右边缘超出 tile 边界。

**根因**：行 x=16 起始，`row_w = right_canvas_w - 15`，导致右边缘到达 `16 + (right_canvas_w - 15) = right_canvas_w + 1`，超出 tile。

**修复**：将 `lv_obj_set_pos(row, 16, row_y)` 改为 `lv_obj_set_pos(row, 0, row_y)`，与 `card_info.c` 的 x=0 对齐。

```
// 修复前 (card_gas.c:32)
lv_obj_set_pos(row, 16, row_y);  // 溢出

// 修复后
lv_obj_set_pos(row, 0, row_y);   // 与 INFO MENU 对齐
```

### 15.5 关键代码变更日志

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-22 | `card_gas.c` | 气体选项 x=16 到 x=0，与 INFO MENU 对齐 |
| 2026-04-22 | `UI_html_DOC/LVGL_LAYOUT_GUIDE.md` | 新建排版落地指南 |
| 2026-04-23 | `arex_ui_engine.h` | 新增 `arex_left_module_t` 枚举、`left_order[]`、`left_mod_*` 属性表 |
| 2026-04-23 | `arex_ui_engine.h` | 新增命名常量 `AREX_MIN_CLASSIC_TOP_H`、`AREX_MASK_EDGE_GUARD` |
| 2026-04-23 | `arex_ui_engine.h` | 扩展 `arex_anchor_comp_t`：新增 `module`、`title_font`、`val_font`、`title_align`、`val_align` |
| 2026-04-23 | `arex_ui_engine.c` | `arex_sys_config_defaults()` 填充所有新字段（left_order、per-module 属性表） |
| 2026-04-23 | `arex_ui_engine.c` | `arex_calc_anchor_layout()` 完全数据驱动化，支持 left_order 遍历 |
| 2026-04-23 | `arex_screen.c` | 移除所有残留硬编码像素：DEPTH 内 NEXT_STOP/PO2 Y 改为 U×10 计算，wall 高度改为 `h_tissues_chart×U`，submenu item_h 改为 `h_menu_item×U` |
| 2026-04-23 | `arex_screen.c` | `left_anchor_create()` 使用 `comps[i].val_font`/`val_align` 替代 switch 硬编码 |
| 2026-04-23 | `arex_screen.c` | `left_anchor_rebuild()` 同样接入数据驱动字体/对齐刷新 |

---

## 16. APP 同步就绪：全动态布局引擎（v2026-04-23）

### 16.1 三大核心引擎状态

| 引擎 | 状态 | 说明 |
|------|------|------|
| 左侧锚点枚举排序 | ✅ 已实现 | `left_order[]` + `arex_calc_anchor_layout()` 数据驱动遍历 |
| 右侧卡片顺序 | ✅ 已实现 | `card_order[]` + `arex_card_get()` 双射映射 |
| U 单位零硬编码 | ✅ 已实现 | 所有坐标基于 `× AREX_BASE_U`，无残留像素常数 |

### 16.2 左侧锚点模块枚举

```c
typedef enum {
    AREX_MODULE_NONE   = 0,  /* 占位/空白 */
    AREX_MODULE_DEPTH  = 1,  /* DEPTH 大通栏 */
    AREX_MODULE_NDL    = 2,  /* NDL 双拼左块 */
    AREX_MODULE_TTS    = 3,  /* TTS 双拼右块 */
    AREX_MODULE_POD1   = 4,  /* POD 1 双拼左块 */
    AREX_MODULE_POD2   = 5,  /* POD 2 双拼右块 */
    AREX_MODULE_BATT   = 6,  /* BATT 双拼左块 */
    AREX_MODULE_WTM    = 7,  /* W.TIME 双拼右块 */
    AREX_MODULE_GAS    = 8,  /* GAS 中通栏 */
    AREX_MODULE_TIME   = 9,  /* DIVE TIME 底部通栏 */
    AREX_MODULE_CUSTOM = 10,  /* 自定义预留 */
} arex_left_module_t;
```

### 16.3 左侧锚点顺序数组

```c
// g_sys_config.left_order[] — 控制模块渲染顺序（默认）
g_sys_config.left_order[0] = AREX_MODULE_DEPTH;  // DEPTH
g_sys_config.left_order[1] = AREX_MODULE_NDL;  // NDL
g_sys_config.left_order[2] = AREX_MODULE_TTS;  // TTS
g_sys_config.left_order[3] = AREX_MODULE_POD1;  // POD1
g_sys_config.left_order[4] = AREX_MODULE_POD2;  // POD2
g_sys_config.left_order[5] = AREX_MODULE_BATT;  // BATT
g_sys_config.left_order[6] = AREX_MODULE_WTM;   // W.TIME
g_sys_config.left_order[7] = AREX_MODULE_GAS;   // GAS
g_sys_config.left_order[8] = AREX_MODULE_TIME;  // TIME
```

**APP 同步示例**：将 DEPTH 移到最后，只需下发：
```json
{ "left_order": [2, 3, 4, 5, 6, 7, 8, 9, 1] }
```
单片机收到后调用 `arex_ui_apply_config()`，整个左侧面板自动重排，无需改任何 C 代码。

### 16.4 per-module 属性映射表

| 字段 | 类型 | 说明 |
|------|------|------|
| `left_mod_split[i]` | `uint8_t` | 0=单栏 1=双拼左 2=双拼右 |
| `left_mod_title_font[i]` | `uint8_t` | 0=SMALL 1=MEDIUM 2=TITLE |
| `left_mod_val_font[i]` | `uint8_t` | 0=SMALL 1=MEDIUM 2=TITLE 3=HUGE |
| `left_mod_title_align[i]` | `uint8_t` | 0=LEFT 1=CENTER 2=RIGHT |
| `left_mod_val_align[i]` | `uint8_t` | 0=LEFT 1=CENTER 2=RIGHT |

字号表（`font_cat[4]`）: `0=AREX_FONT_SMALL(14px)` `1=AREX_FONT_MEDIUM(28px)` `2=AREX_FONT_TITLE(20px)` `3=AREX_FONT_HUGE(48px)`

### 16.5 布局引擎函数一览

| 函数 | 文件 | 作用 |
|------|------|------|
| `arex_calc_anchor_layout()` | arex_ui_engine.c | 遍历 `left_order[]`，填 `arex_anchor_comp_t[9]`，所有尺寸基于 `× AREX_BASE_U` |
| `arex_calc_tech_layout()` | arex_ui_engine.c | Tech 模式左右分区坐标：左锚点固定 160px，右区域 `= safe_zone_w - 160 - gap` |
| `arex_calc_classic_layout()` | arex_ui_engine.c | Classic 模式上下分区，最小高度 `AREX_MIN_CLASSIC_TOP_H=200px` |
| `arex_calc_widget_cell()` | arex_ui_engine.c | 5x6 网格单元格坐标，`unit_w = parent_w/5`，`unit_h = parent_h/6` |
| `arex_calc_tissue_bars()` | arex_ui_engine.c | 16 柱组织图 X 坐标，`col_w = total_w/16` |
| `left_anchor_create()` | arex_screen.c | 首次创建，使用 `comps[i].val_font` / `val_align` 数据驱动 |
| `left_anchor_rebuild()` | arex_screen.c | 配置变更后重建，使用数据驱动字体/对齐刷新 |
| `right_panel_create()` | arex_screen.c | 创建 tileview，按 `card_order[]` 顺序挂载卡片 |

### 16.6 关键命名常量（防止硬编码）

| 常量 | 值 | 用途 |
|------|-----|------|
| `AREX_BASE_U` | 10px | 所有 U 单位乘数 |
| `AREX_MIN_CLASSIC_TOP_H` | 200px | Classic 模式最小上区高度 |
| `AREX_MASK_EDGE_GUARD` | 80px | 面镜盲区掩膜底部警戒阈值 |
| `AREX_LEFT_ANCHOR_W` | 160px | 左侧锚点固定宽度 |
| `ANCHOR_COMP_COUNT` | 9 | 左侧组件固定槽位数 |
| `ANCHOR_LEFT_MODULE_COUNT` | 9 | 左侧模块枚举/顺序数组长度 |

### 16.7 right_w Fallback 公式

所有右侧宽度 fallback 使用以下公式，不再使用硬编码 `420`：

```c
uint16_t right_w_fallback = g_sys_config.safe_zone_w
                          - AREX_LEFT_ANCHOR_W          // 160px
                          - g_sys_config.gap_u * AREX_BASE_U;  // gap
// 例: 580 - 160 - 10 = 410px
```

### 16.8 APP 完整同步协议

1. **APP 下发** JSON 配置（仅包含 `g_sys_config` 字段子集）
2. **单片机解析** → 覆盖 `g_sys_config` 对应字段
3. **调用** `arex_ui_apply_config()` → `left_anchor_rebuild()` + `arex_screen_rebuild_tileview()`
4. **结果**：整个 UI 按新配置重排，无需重启，无需改 C 代码
