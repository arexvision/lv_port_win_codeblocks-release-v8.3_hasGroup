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
- `deco.surf_gf > 100` 时，card_deco.c 会创建 500ms 闪烁定时器（红色警告）
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

**DASH 的可滚动范围是 index 1~4（card_order 中 COMPASS 到 PLAN），index 0（INFO）和 index 5（SETUP）仅通过 wall-charge 进入。**

```
card_order 布局：[0]=INFO  [1]=COMPASS  [2]=DECO  [3]=GAS  [4]=PLAN  [5]=SETUP
                  ↑ wall-charge 才能进                            ↑ wall-charge 才能进

DASH 可滚动范围：dash_card ∈ [1, 4]

在 UI_DASH 下：
  dash_card==1 且继续向上 → wall_charge++，显示顶部墙 "[#][ ][ ]"
                            → tileview 向下偏移 charge×20px 然后弹回（橡皮筋感）
  连续3次 → 穿越到 UI_INFO（滚动到 index=0），wall_charge 清零

  dash_card==4 且继续向下 → wall_charge++，显示底部墙
                            → tileview 向上偏移 charge×20px 然后弹回
  连续3次 → 穿越到 UI_SETUP（滚动到 index=5）

  任何中途改变方向 → wall_charge = 0，墙UI隐藏，tileview 立即归位
```

**橡皮筋动画实现（`wall_nudge_tileview`）：**  
对 `s_tileview` 做 `lv_obj_set_y` 动画：350ms ease-out 平滑推到 `charge×20px`，停在那里直到 wall 清零。  
`arex_screen_hide_walls` 时立即 `set_y(0)` 归位。  
对应 HTML 的 `transition: 0.35s cubic-bezier(0.2,0.8,0.2,1)` + `updateElevator(wallCharge * 20)`，无回弹。

UI_INFO 退出（wall-charge 或 ESC）→ 返回 DASH，dash_card=1（COMPASS）  
UI_SETUP 退出（wall-charge 或 ESC）→ 返回 DASH，dash_card=4（PLAN）

> **启动行为说明：** 启动直接进入 `UI_INFO` 状态，显示 INFO 卡（tile 0），光标聚焦第一条 LAST DIVE。
> 在 INFO 菜单底部 wall-charge（连续3次向下）→ 进入 `UI_DASH`，从 COMPASS（tile 1）开始。
> 在 DASH 顶部 wall-charge（COMPASS 处连续3次向上）→ 返回 `UI_INFO`。

### 5.2 气体切换流程（3F 卡片）

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
  └─ 深度 > MOD → CLICK 无效：modal 震动动画（arex_screen_pulse_modal）
  
  ESC（任意时）:
    UI_MODAL_GAS → UI_EDIT_GAS
    UI_EDIT_GAS  → UI_DASH
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
UI_SUB_MENU（DIVE SETUP 子菜单）
  │
  CLICK "MOD PO2: X.X" → UI_EDIT_VALUE
  edit_ctx = {value=当前值, min=1.0, max=1.6, step=0.1, original=旧值}
  子菜单项显示 "MOD PO2: X.X ▲▼"（黑字绿底 blink 提示）
  │
  ROTATE → edit_ctx.value ± step（clamp 到 min/max）
            arex_screen_refresh_edit_value() 更新文字
  CLICK  → 提交：g_arex.settings.mod_ppo2 = edit_ctx.value，返回 UI_SUB_MENU
  ESC    → 取消：恢复 edit_ctx.original，返回 UI_SUB_MENU
```

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
| 2 CARD_ID_DECO | card_deco.c | TISSUES & DECO | 16个 `lv_bar`（竖向），GF99/SurfGF/CNS/OTU 标签，SurfGF>100触发500ms闪烁定时器 |
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

- 使用 `lv_canvas`（420×380，静态像素缓冲 `s_cbuf`）
- `draw_tape(heading)` 每次完整清屏重绘：
  - 以 heading 为中心，±60° 范围画刻度线
  - major（每45°）画高刻度+方位字母（N/NE/E/SE/S/SW/W/NW）
  - minor（每15°）画中刻度；其余画短刻度
  - 中央画绿色竖线（指针）
  - 大字号显示当前航向数字（如 `265°`）
  - 若 `compass.marked`：在目标航向对应位置画黄色竖线，下方显示 `TARGET 265°`
- 每秒通过 `card_compass_update()` 触发重绘

### 9.2 card_deco.c（2F）

- 顶部4列：GF99 / SURF GF / CNS / OTU（`lv_label`）
- 16个 `lv_bar`（范围 0~110，代表0~110%，M值线在100%处）
- 颜色规则：
  - `pct ≤ 80` → 绿色 `#00FF00`
  - `80 < pct ≤ 100` → 橙色 `#FFAA00`
  - `pct > 100` → 红色 `#FF0000`
- M值线：`BAR_H * (1 - 100/110)` 处画红色横线
- `surf_gf > 100` 时启动 500ms 定时器交替变色（闪烁）；低于100时自动销毁定时器

### 9.3 card_gas.c（3F）

- 4行 `lv_obj` 容器（400×72px，间距86px）
- 三态颜色逻辑（`card_gas_update` 每帧评估）：
  - 光标行（`UI_EDIT_GAS && gas_cursor==i`）：绿底黑字
  - 激活行（`active_idx==i`）：深绿底绿字
  - 普通行：黑底绿字
- 超 MOD：边框变红（`depth > MOD`）
- PPO2 简化计算：`depth/10 * 0.21`（使用当前深度）

### 9.4 card_plan.c（4F）

- 380×280 canvas，静态像素缓冲
- 固定 demo 剖面数据（13个坐标点）
- 网格：深度0~50m（步进10m）、时间0~55min（步进10min）
- 折线：绿色实线连接 profile 点
- 当前位置：黄色圆点，坐标由 `g_arex.dive.dive_time_s` 和 `g_arex.dive.depth` 实时计算

---

## 10. 颜色常量

```c
AREX_GREEN  = #00FF00   // 主色（文字、指针、激活态）
AREX_LIGHT  = #55FF55   // 副色（标签、辅助文字、次要数值）
AREX_DARK   = #003300   // 边框、刻度线、非激活背景
AREX_BLACK  = #000000   // 卡片背景
AREX_BG     = #050505   // 屏幕根背景（接近纯黑，略有区分）
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
| `#scroll-indicator` | `s_scroll_dots[6]` |
| JS `STATE` 对象 | `arex_ui_ctx_t g_ui` |
| JS `gasData[]` | `AREX_GAS_TABLE[]` |
| JS `setInterval 150ms`（罗盘） | `sim_tick_cb` 1000ms + `card_compass_update` |
| JS `flashInvert` 动画 | `flash_timer_cb` 500ms（card_deco.c） |
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
