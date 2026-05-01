我已经仔细检查并修复了文档中（特别是第 29 节）出现的乱码字符。文档结构、格式及所有技术参数均严格保持原样。

> **2026-05-01 更新 v2**：Section 29 进一步优化——移除 `arex_left_widget_t` 中的 `w/h` 字段（BLE 帧由 172 字节缩减为 148 字节）。APP 只下发 `widget_id + x + y` 三字段（3 字节），**跨度信息完全由 MCU 从 `arex_get_widget_style()` 样式表自动查表获取，彻底消除数据冗余**。
>
> **2026-05-01 更新**：Section 29 协议结构体由 `arex_custom_widget_cfg_t` 回归为 `arex_left_widget_t`，移除 protobuf 下发的 `font_id`（字号改由 MCU 根据 `span_w/span_h` 自动推导）；`render_widget_by_id()` 函数签名增加 `cfg_font_id` 参数以支持强制字号覆盖；新增 **8-bit 元素开关掩码** `ELEM_TITLE|ELEM_VALUE|ELEM_UNIT|ELEM_BAR|ELEM_EXTRA`，字典中每个组件按 PRD 约束"按需勾选"，通用流水线改为掩码驱动的按需装配逻辑。

---

# AREX Pro Dive Computer UI — 架构解读文档

> 基于 `UI_html/arex_ui_test_0.10.html` 原型，移植到 LVGL v8.3 (Windows/CodeBlocks)  
> 入口：`UI_main()` in `src/arex_ui/UI_main.c`

---

## 1. 整体目录结构

```
src/arex_ui/
├── UI_main.c                # 入口，初始化序列 + 仿真 tick 定时器
├── arex_ui_engine.h/c       # 全局状态数据模型（g_sys_config + g_sensor_data）
├── arex_ui_state.h/c        # 状态机核心（g_ui），三个输入处理函数
├── arex_screen.h/c          # LVGL 控件树创建 + 所有屏幕操作 API
├── arex_input.h/c           # 输入事件捕获（键盘/编码器 → 状态机）
├── arex_card_registry.h/c # 卡片注册表（ID、title、create/update 回调）
├── arex_data.h/c            # 数据总线头文件存根（arex_ui_engine.h 包含一切）
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
  ├─ arex_ui_init()             → 加载默认配置到 g_sys_config + 初始化 g_sensor_data 演示数据
  ├─ arex_screen_create()        → 创建整个 LVGL 控件树（左面板 + tileview + 弹窗 + 子菜单层）
  │    ├── left_panel_create()
  │    ├── right_panel_create()  → 创建 tileview，按 card_order 调用 card_*_create()
  │    ├── wall_create()
  │    ├── modal_create()
  │    └── submenu_layer_create()
  ├─ arex_input_init(scr)        → 注册键盘/编码器事件回调
  ├─ arex_screen_refresh_left_panel()   → 左侧面板初始值填充
  ├─ arex_screen_scroll_to_card(0)      → 跳到 tile 0（INFO 卡）
  ├─ arex_screen_set_info_selection(0)  → 高亮第一条 LAST DIVE
  ├─ arex_ui_state_init()       → 将 g_ui 清零，state=UI_INFO，dash_card=1
  └─ lv_timer_create(sim_tick_cb, 1000ms)  → 每秒仿真 tick
```

**仿真 tick (`sim_tick_cb`) 每秒做：**
1. `g_sensor_data.heading += 1° % 360`（航向缓慢漂移）+ `g_sensor_data.dive_time_s++`
2. `arex_screen_refresh_left_panel()` → 刷新左面板数值
3. `arex_ui_refresh_all()` → 遍历注册表，调用每个卡片的 `update_cb()`

---

## 3. 数据模型：`arex_ui_engine.h/c`

### 核心数据结构体（两总线分离）

详见 Section 16 `arex_sys_config_t` 和 `arex_sensor_data_t` 定义。
- 实时数据总线：`g_sensor_data` — UI 控件每 tick 读取
- 配置总线：`g_sys_config` — 布局参数 + 用户设置（APP 可同步）

### 气体表（静态，4 种）

| Index | name | MOD(m) |
|-------|------|--------|
| 0 | AIR | 56 |
| 1 | NX 32 | 34 |
| 2 | TX 18/45 | 68 |
| 3 | O2 100% | 6 |

### 关键设计点

- `g_sys_config.card_order[pos] = card_id`：控制 tileview 中卡片的显示顺序
- **位置枚举 `arex_card_pos_t`**：`CARD_POS_INFO`=0（固定 tile 0）、`CARD_POS_1`~`CARD_POS_6`（中间 6 个可重排）、`CARD_POS_SETUP`=7（固定 tile 7）
- **卡片 ID 枚举 `arex_card_id_t`**：`CARD_ID_INFO` ~ `CARD_ID_SETUP`，表示卡片固有身份
- `g_sys_card_order(pos)`：统一入口，通过 `card_order[pos]` 查询卡片 ID
- 用枚举显式赋值：`cfg->card_order[CARD_POS_INFO] = CARD_ID_INFO`（INFO/SETUP 固定，中间可重排）
- 左侧锚点通过 `left_layout[]` 行配置驱动，任意两模块可自由双拼
- 气体常量 `AREX_GAS_NAMES[]` / `AREX_GAS_MOD_M[]` 定义于 `arex_ui_engine.c`
- `g_sensor_data.tissue_pct[]` 原始值由减压引擎计算，UI 层按百分比渲染

---

## 4. 状态机：`arex_ui_state.h/c`

### 状态枚举

```
UI_DASH          (0)  — 主卡片滚动模式
UI_INFO          (1)  — INFO 菜单列表激活
UI_SETUP         (2)  — SETUP 菜单列表激活
UI_EDIT_GAS      (3)  — 3F 气体选择光标移动中
UI_MODAL_GAS     (4)  — 气体切换确认弹窗已打开
UI_MODAL_COMPASS (5)  — 清除罗盘目标确认弹窗
UI_SUB_MENU      (6)  — 子菜单层已弹出（从右侧滑入）
UI_MODAL_ACT     (7)  — 通用动作弹窗（1秒自动关闭）
UI_EDIT_VALUE    (8)  — 数值内联编辑（例如 MOD PO2）
```

### 全局 UI 上下文：`arex_ui_ctx_t g_ui`

```c
typedef struct {
    arex_ui_state_t  state;           // 当前状态（初始 UI_INFO）
    uint8_t  dash_card;               // 当前卡片索引（初始 1，COMPASS）
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
    uint8_t      sub_item_count;
    arex_ui_state_t sub_parent;       // 进入子菜单时的父状态
} arex_ui_ctx_t;
```

> **启动行为：** `arex_ui_state_init()` 将 `state=UI_INFO`，`dash_card=1`，`menu_info_idx=0`，`wall_charge=0`。启动后直接显示 INFO 菜单（tile 0），等待用户操作。

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
- `dash_card` = card 在 `card_order[]` 中的位置（0~7）
- `dash_card=0` → INFO（仅 wall-charge 可进）
- `dash_card=1` → COMPASS，`dash_card=2` → DECO，`dash_card=3` → PLAN，`dash_card=4` → GAS，`dash_card=5` → CUSTOM_GRID，`dash_card=6` → BLANK
- `dash_card=7` → SETUP（仅 wall-charge 可进）

```
card_order 布局：[0]=INFO  [1]=COMPASS  [2]=DECO  [3]=PLAN  [4]=GAS  [5]=CUSTOM_GRID  [6]=BLANK  [7]=SETUP
                 ↑ wall-charge 才能进                                                           ↑ wall-charge 才能进

DASH 可滚动范围：dash_card ∈ [1, AREX_CARD_COUNT-2]（即 tile 1~6）

在 UI_DASH 下：
  dash_card==1 且继续向上 → wall_charge++，显示顶部墙 "[#][ ][ ]"
                            → tileview 向下偏移 charge×20px 然后弹回（橡皮筋感）
  连续3次 → 穿越到 UI_INFO（滚动到 tile_pos=0），wall_charge 清零

  dash_card==AREX_CARD_COUNT-2（即 tile 6，BLANK）且继续向下 → wall_charge++，显示底部墙
                            → tileview 向上偏移 charge×20px 然后弹回
  连续3次 → 穿越到 UI_SETUP（滚动到 tile_pos=AREX_CARD_COUNT-1，即 tile 7）

  任何中途改变方向 → wall_charge = 0，墙UI隐藏，tileview 立即归位
```

**橡皮筋动画实现（`wall_nudge_tileview`）：**
对 `s_tileview` 做 `lv_obj_set_y` 动画：350ms ease-out 平滑推到 `charge×20px`，停在那里直到 wall 清零。
`arex_screen_hide_walls` 时立即 `set_y(0)` 归位。
对应 HTML 的 `transition: 0.35s cubic-bezier(0.2,0.8,0.2,1)` + `updateElevator(wallCharge * 20)`，无回弹。

UI_INFO 退出（wall-charge 或 ESC）→ 返回 DASH，dash_card=1（COMPASS）
UI_SETUP 退出（wall-charge 或 ESC）→ 返回 DASH，dash_card=AREX_CARD_COUNT-2（BLANK，即 tile 6）

> **启动行为说明：** 启动直接进入 `UI_INFO` 状态，显示 INFO 卡（tile 0），光标聚焦第一条 LAST DIVE。
> 在 INFO 菜单底部 wall-charge（连续3次向下）→ 进入 `UI_DASH`，从 COMPASS（tile 1）开始。
> 在 DASH 顶部 wall-charge（COMPASS 处连续3次向上）→ 返回 `UI_INFO`。

### 5.2 气体切换流程（3F 卡片）

> **【重点 · CONFIG GAS 退出规则】** > **气体在当前深度不可用**（`dive.depth >` 该气体的 **MOD**，不适用深度）时，**不能**用确认键完成切换：弹窗内 **Enter/点击** 仅触发 `arex_screen_pulse_modal()` 震动，**不会**改 `active_idx`、**不会**回到仪表盘。  
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
  CLICK  → 提交：g_sys_config.mod_ppo2 = edit_ctx.value
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
│  DEPTH  45.2       │  │  Tileview（垂直8卡片）     │   │
│  NDL 0  TTS 24'    │  │  card_order[0] → tile 0  │   │
│  NEXT STOP 21m 3'  │  │  card_order[1] → tile 1  │   │
│  POD1 210  POD2195 │  │  ...                     │   │
│  GAS TX18/45       │  │  card_order[6] → tile 6  │   │
│  PO2 1.2|1.2|1.3   │  │  card_order[7] → tile 7  │   │
│  TIME 38:14        │  └──────────────────────────┘   │
│                    │  [墙UI top/bottom 隐藏叠加]     │
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
    arex_card_id_t     id;             // 稳定ID（0~7）
    const char      *title;          // 卡片标题，英文
    arex_card_engine_t engine_type;   // 引擎类型
    const void      *config_data;    // 引擎配置数据（如 MENU 引擎的菜单配置）
    lv_obj_t       *tile_obj;       // create 后填入（NULL 直到 create 回调执行）
    void (*create_cb)(lv_obj_t *parent);   // 一次性建控件
    void (*update_cb)(void);               // 每 tick 刷新数据
    void (*on_enter_cb)(void);             // 滚动到此卡时（可选，NULL 表示不处理）
} arex_card_t;
```

### 数量常量

```c
AREX_CARD_COUNT        = 8   // INFO+COMPASS+DECO+GAS+PLAN+CUSTOM_GRID+BLANK+SETUP
AREX_DASH_CARD_COUNT   = 6   // DASH 可滑动范围（排除首尾 INFO/SETUP）
```

### 8张卡片一览

| ID | 文件 | 标题 | 核心实现 |
|----|------|------|----------|
| 0 CARD_ID_INFO | card_info.c | INFO MENU | `arex_render_dynamic_menu()` 渲染，`arex_menu_item_cfg_t[]` 配置数据 |
| 1 CARD_ID_COMPASS | card_compass.c | NAV COMPASS | 420×380 canvas，`draw_tape()` 每帧重绘，目标航向黄线 |
| 2 CARD_ID_DECO | card_deco.c | TISSUES & DECO | 16 竖条贴卡片底（对齐 HTML `margin-top:auto`）；条槽 `AREX_DARK` 半透明；填充绿 / `>70%` 浅绿 / `≥90%` 反色闪烁；SurfGF>100 绿底黑字 |
| 3 CARD_ID_GAS | card_gas.c | GAS SWITCH | 4行 `lv_obj` 容器，光标/激活/超MOD三态颜色，实时 PPO2 计算 |
| 4 CARD_ID_PLAN | card_plan.c | DIVE PLAN TRACK | 380×280 canvas，网格+折线+当前位置黄点 |
| 5 CARD_ID_CUSTOM_GRID | card_custom_grid.c | 5F: CUSTOM WIDGETS | `arex_render_5f_custom_grid()` 渲染 APP 下发的组件网格 |
| 6 CARD_ID_BLANK | card_blank.c | BLANK | 纯黑背景空白卡片 |
| 7 CARD_ID_SETUP | card_setup.c | DIVE SETUP | `arex_render_dynamic_menu()` 渲染，`arex_menu_item_cfg_t[]` 含 badge 徽章 |

### `arex_card_get(pos)` vs `arex_card_get_by_id(id)`

```c
// 通过显示位置取（走 card_order[] 间接层）
arex_card_get(CARD_POS_1)  →  g_cards[ card_order[CARD_POS_1] ] = g_cards[CARD_ID_COMPASS]

// 通过稳定ID取（不走间接层）
arex_card_get_by_id(CARD_ID_GAS)  →  g_cards[CARD_ID_GAS]

// 卡片数量（统一入口）
arex_card_count()  →  AREX_CARD_COUNT (=8)
```

> **修改卡片顺序**（仅限中间 6 个，INFO/BLANK/SETUP 固定）：
> ```c
> cfg->card_order[CARD_POS_INFO]  = CARD_ID_INFO;     // 固定 tile 0
> cfg->card_order[CARD_POS_1]     = CARD_ID_COMPASS; // 可重排
> cfg->card_order[CARD_POS_2]     = CARD_ID_DECO;    // 可重排
> cfg->card_order[CARD_POS_3]     = CARD_ID_PLAN;    // 可重排
> cfg->card_order[CARD_POS_4]     = CARD_ID_GAS;     // 可重排
> cfg->card_order[CARD_POS_5]     = CARD_ID_CUSTOM_GRID; // 可重排
> cfg->card_order[CARD_POS_6]     = CARD_ID_BLANK;   // 固定 tile 6
> cfg->card_order[CARD_POS_SETUP] = CARD_ID_SETUP;   // 固定 tile 7
> ```
> `card_order[pos] = card_id`，pos 用 `CARD_POS_*`，card_id 用 `CARD_ID_*`，含义清晰、不易填反。

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
  LV_KEY_ENTER                → ui_handle_click()
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
- 当前位置：黄色圆点，动态由 `g_sensor_data.dive_time_s` 和 `g_sensor_data.depth` 计算，带 "NOW" 标签

---

## 10. 颜色常量与字体

```c
/* 颜色 */
AREX_GREEN  = #00FF00   // 主色（文字、指针、激活态）
AREX_LIGHT  = #55FF55   // 副色（标签、辅助文字、次要数值）
AREX_DARK   = #003300   // 边框、刻度线、非激活背景
AREX_BLACK  = #000000   // 卡片背景
AREX_BG      = #050505   // 屏幕根背景
```

> **字体系统已全面重构，请参见 Section 17 字体 ID 映射引擎。**
> 旧版 `AREX_FONT_*` 宏定义仅在 `arex_screen.h` 中保留兼容层，**禁止在新代码中使用**。

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
数量: 6个（对应 AREX_DASH_CARD_COUNT，即 tile 1~6）
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
| `.card`（8个 div） | 8个 tileview tile + card_*_create |
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
            ├─ 更新 g_sensor_data（heading++, dive_time_s++, depth浮动）
            ├─ arex_screen_refresh_left_panel()   [读 g_sensor_data → 写 s_lbl_*]
            └─ arex_ui_refresh_all()
                 for i in 0..arex_card_count()-1:
                   card = arex_card_get(i)        // 通过 g_sys_card_order(i) 查 ID
                   if (card->update_cb) card->update_cb()

用户输入（键盘/编码器）
  └─ key_event_cb() / enc_click_cb()
       └─ ui_handle_rotate / click / back
            ├─ 修改 g_ui.state / g_ui.dash_card / g_ui.gas_cursor / …
            ├─ 修改 g_sensor_data / g_sys_config / …
            └─ 调用 arex_screen_* 函数更新控件外观
```

---

## 13. 重要设计约定

1. **卡片不直接写状态**：card_*.c 只读 `g_sensor_data` 和 `g_ui`，不修改它们。状态修改统一在 `arex_ui_state.c` 中进行。

2. **screen 层是哑的**：`arex_screen.c` 的函数只负责操作控件，业务判断（如气体深度校验）在状态机里完成。

3. **card_order 间接层**：tileview 的物理顺序在创建时固定，但用户可以通过修改 `g_sys_config.card_order[]` 改变各卡片的逻辑位置，`arex_card_get(pos)` 通过 `g_sys_card_order(pos)` 负责解引用。

4. **注册回调**：card_info.c 和 card_setup.c 通过 `arex_screen_register_info_list()` / `arex_screen_register_setup_list()` 把它们内部创建的列表对象告知 screen 层，避免在 arex_screen.c 中重复创建控件。

5. **Wall-charge 防误触**：连续3次才穿越边界，防止单次抖动误触进入菜单。

6. **Modal 震动反馈**：气体切换超 MOD 时，`arex_screen_pulse_modal()` 用 lv_anim 做左右 ±6px 抖动（2次重复，80ms），对应 HTML 的 `scale(1.05)` 弹跳。

7. **数据总线归一化**：`arex_data.h` 仅作存根（`#include "arex_ui_engine.h"`），所有数据总线（`g_sys_config`、`g_sensor_data`）、枚举、宏均统一在 `arex_ui_engine.h` 中定义，消除跨文件依赖。

---

## 14. 子菜单动作路由（v0.10 新增）

`arex_screen_handle_submenu_select()` 在 `arex_screen.c` 中全面实现，按 `cur_title` 分支路由：

### 14.1 动作路由表

| 当前子菜单标题 | 选中项规则 | 执行动作 |
|---|---|---|
| `GAS SWITCH` | `SELECT XXX` | 更新 `g_sensor_data.gas_active_idx`，刷新 gas 卡和左面板，关闭子菜单 |
| `CONSERVATISM` | `LOW/MED/HIGH` 开头 | 调用 `arex_ui_on_conservatism_set()` 外部业务层回调，更新 SETUP 菜单 badge，关闭子菜单 |
| `BRIGHTNESS` | 精确匹配 `LOW/MED/HIGH/MAX` | 更新 `g_sys_config.brightness`，更新 SETUP 菜单 badge，关闭子菜单 |
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
           │                    └─ MOD PO2 → UI_EDIT_VALUE（内联数值编辑）
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
`card_setup_update()` 每 tick 从 `g_sys_config` 同步 CONSERVATISM / BRIGHTNESS 的 badge 文字。

### 14.4 INFO 子菜单动态数据

`arex_screen_open_info_submenu()` 在打开前调用 `build_info_submenu(idx)` 从 `g_sensor_data` 动态构建字符串：

| 子菜单 | 动态字段来源 |
|--------|-------------|
| LAST DIVE | `g_sensor_data.depth`，`g_sensor_data.dive_time_s` |
| TISSUE & TOX | `g_sensor_data.cns_pct`，`g_sensor_data.otu` |
| GAS & CALC | `AREX_GAS_TABLE[g_sensor_data.gas_active_idx].name` |
| SENSOR & DEVICE | `g_sensor_data.pod1_bar`，`g_sensor_data.pod2_bar` |

### 14.5 DIVE SETUP 嵌套菜单中 MOD PO2 实时值

`build_nested_dive_setup()` 在每次打开该子菜单前调用，将 `g_sys_config.mod_ppo2` 格式化进 `s_modppo2_str[]`，保证显示最新值。编辑提交后 `arex_screen_commit_edit_value()` 直接更新同一 label。

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
| 2026-04-23 | `arex_ui_engine.h` | 删除 `left_order[]` 和 `left_mod_*[]`，替换为 `arex_left_row_cfg_t left_layout[8]` 行配置结构体 |
| 2026-04-23 | `arex_ui_engine.h` | 新增 `AREX_MODULE_EMPTY=0`（替换旧 NONE），新增 `AREX_MAX_LEFT_ROWS=8`、`AREX_ROW_MAX_SLOTS=2`、`ANCHOR_COMP_COUNT=16` |
| 2026-04-23 | `arex_ui_engine.c` | `arex_calc_anchor_layout()` 完全重写：遍历 `left_layout[]` 而非 `left_order[]`，单栏独占全宽(160px)，双拼各半宽(80px)，零双拼硬编码 |
| 2026-04-23 | `arex_ui_engine.c` | `arex_sys_config_defaults()` 填充 `left_layout[]` 默认行配置（DEPTH/GA + NDL+TTS + POD1+POD2 + BATT+WTM + GAS + TIME） |
| 2026-04-23 | `arex_screen.c` | `left_anchor_create()` 循环改为 `for (i < comp_count)`，标题和数值文字改为 `switch (c->module)` 枚举驱动 |
| 2026-04-23 | `arex_screen.c` | `left_anchor_rebuild()` 同步更新为空模块检查和对齐处理 |
| 2026-04-23 | `AREX_ARCH.md` | Section 16 全面升级：新增 16.3 `arex_left_row_cfg_t` 结构体、16.4 默认行布局、16.9 渲染流程图 |
| 2026-04-23 | `arex_ui_engine.h` | 新增 `arex_font_id_t` 枚举字典、`arex_get_font()` 声明；删除废弃的 `align_title/huge/med` 字段 |
| 2026-04-23 | `arex_ui_engine.c` | 实现 `arex_get_font()` 字体映射器；删除废弃的 `font_sz_*` 默认值；更新 `def_layout[]` 使用 `AREX_FONT_ID_*` 枚举 |
| 2026-04-23 | `arex_screen.c` | 全部 `AREX_FONT_*` 宏替换为 `arex_get_font(id)`；删除 `font_cat[]` 中间层数组（两处）；`left_anchor_rebuild()` 增加 `title_font`/`title_align` 填充 |
| 2026-04-23 | `cards/*.c` | 全部 `AREX_FONT_*` 宏替换为 `arex_get_font(id)`（compass/setup/gas/deco/info/plan） |
| 2026-04-23 | `arex_screen.h` | 旧 `AREX_FONT_*` 宏标记为废弃，附正确用法注释 |
| 2026-04-23 | `AREX_ARCH.md` | 新增 Section 17 字体映射引擎文档；更新 Section 10/16 引用 |
| 2026-04-23 | `arex_ui_engine.h` | 新增 `arex_menu_item_cfg_t` 及 `arex_render_dynamic_menu()` 声明 |
| 2026-04-23 | `arex_ui_engine.c` | 实现工厂函数，height/gap 全程从 `g_sys_config` 推算 |
| 2026-04-23 | `card_info.c` | 完全重构：配置数组 + 1 行工厂调用 |
| 2026-04-23 | `card_setup.c` | 完全重构：badge 刷新逻辑保留，句柄数组改为工厂输出 |
| 2026-04-23 | `card_gas.c` | `GAS_ROW_GAP` 宏删除，`gap_y` 改为配置推算 |
| 2026-04-23 | `AREX_ARCH.md` | 新增 Section 19/20 右侧卡片动态菜单引擎文档 |
| 2026-04-23 | `arex_card_registry.c` | 重写：指定初始化器、`tile_obj=NULL` 初始化、`g_sys_card_order()` 间接查询、`arex_card_count()` API |
| 2026-04-23 | `arex_card_registry.h` | 新增 `AREX_CARD_COUNT`、`AREX_DASH_CARD_COUNT`；`arex_card_reg_t` 新增 `on_enter_cb` |
| 2026-04-23 | `arex_ui_engine.c` | 新增 `g_sys_card_order(pos)` 函数封装；`arex_sys_config_defaults()` 填充默认 `card_order[]` |
| 2026-04-23 | `arex_ui_engine.h` | 新增 `g_sys_card_order()` 声明；新增 `arex_ui_update_data()` 空钩子 |
| 2026-04-23 | `arex_ui_state.c` | `arex_ui_refresh_all()` 改为 `arex_card_count()` 循环；`AREX_CARD_COUNT - 2` 替换为 `AREX_DASH_CARD_COUNT` |
| 2026-04-23 | `arex_data.h/c` | 新建数据总线头文件存根（`#include "arex_ui_engine.h"`），所有定义保留在 engine |
| 2026-04-23 | `card_info.c` | 改为 `arex_get_font()` + `arex_data.h`；空 update 回调 |
| 2026-04-23 | `card_setup.c` | 改为 `arex_get_font()` + dirty check badge 更新；badge 子 label 索引修正 |
| 2026-04-23 | `UI_main.c` | 移除 `lv_timer_create`（已移至 `arex_screen_create`）；启动直接进入 INFO 卡 |
| 2026-04-23 | `AREX_ARCH.md` | 新增 Section 18 重构变更日志；更新 Section 1/3/4/7/12/13 |
|| 2026-04-23 | `arex_ui_engine.h` | 新增 `AREX_DEBUG_BORDER` 宏(0=关闭/1=开启)，统一控制 title_zone/val_zone 调试边框；`RENDER_FIXES.md` Section 7 同步更新 |

---

## 16. APP 同步就绪：全动态布局引擎（v2026-04-23）

### 16.1 三大核心引擎状态

| 引擎 | 状态 | 说明 |
|------|------|------|
| 左侧锚点自由双拼 | ✅ 已实现 | `left_layout[]` 行配置，任意两模块可双拼或独占全宽 |
| 右侧卡片顺序 | ✅ 已实现 | `card_order[]` + `arex_card_get()` 双射映射 |
| U 单位零硬编码 | ✅ 已实现 | 所有坐标基于 `× AREX_BASE_U`，无残留像素常数 |

### 16.2 左侧锚点模块枚举

```c
typedef enum {
    AREX_MODULE_EMPTY  = 0,  /* 空槽位：不渲染任何模块 */
    AREX_MODULE_DEPTH  = 1,  /* DEPTH 大数字（独立一行，全宽） */
    AREX_MODULE_NDL    = 2,  /* NDL 免减压时间 */
    AREX_MODULE_TTS    = 3,  /* TTS 回到水面时间 */
    AREX_MODULE_POD1  = 4,  /* POD1 气瓶1压力 */
    AREX_MODULE_POD2  = 5,  /* POD2 气瓶2压力 */
    AREX_MODULE_BATT  = 6,  /* BATT 电池 */
    AREX_MODULE_WTM   = 7,  /* W.TIME 潜水总时间 */
    AREX_MODULE_GAS   = 8,  /* GAS 当前气体 */
    AREX_MODULE_TIME  = 9,  /* TIME 独立计时 */
} arex_left_module_t;
```

### 16.3 左侧行配置结构体（APP 同步核心）

```c
#define AREX_MAX_LEFT_ROWS  8   /* 最大行数 */
#define AREX_ROW_MAX_SLOTS  2   /* 每行最多 2 个模块槽 */

typedef struct {
    uint8_t left_module;   /* 左侧模块枚举 (AREX_MODULE_*) */
    uint8_t right_module;  /* 右侧模块枚举 (AREX_MODULE_EMPTY=独占全宽) */
    uint8_t h_u;           /* 该行总高度（单位 U，默认 0=查模块默认值） */
    uint8_t title_h_u;     /* 标题区高度（默认 0=用全局 title_h_u） */
    uint8_t title_font;    /* 标题字号: arex_font_id_t (0~3) */
    uint8_t val_font;      /* 数值字号: arex_font_id_t (0~3) */
    uint8_t val_align;     /* 数值对齐: 0=LEFT 1=CENTER 2=RIGHT */
    uint8_t sep_style;     /* 分割线样式: 0=NONE 1=SOLID 2=DASHED 3=DOTTED */
    uint8_t sep_thick;     /* 分割线粗细 px（0=用全局 sep_thick） */
} arex_left_row_cfg_t;

// g_sys_config.left_layout[] — APP 实际下发此数组
arex_left_row_cfg_t left_layout[AREX_MAX_LEFT_ROWS];
```

### 16.4 默认行布局（初始值）

> **字号 arex_font_id_t**: `0=SMALL(14px)` `1=TITLE(20px)` `2=MEDIUM(28px)` `3=HUGE(48px)`

```c
/* row 0: DEPTH 单栏全宽 */
{ AREX_MODULE_DEPTH, AREX_MODULE_EMPTY, 8, 2,
  AREX_FONT_ID_SMALL,  AREX_FONT_ID_HUGE,   AREX_ALIGN_LEFT, AREX_SEP_DASHED, 0 },
/* row 1: NDL + TTS 双拼 */
{ AREX_MODULE_NDL,  AREX_MODULE_TTS,  6, 2,
  AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_LEFT, AREX_SEP_DASHED, 0 },
/* row 2: POD1 + POD2 双拼 */
{ AREX_MODULE_POD1, AREX_MODULE_POD2, 6, 2,
  AREX_FONT_ID_SMALL,  AREX_FONT_ID_TITLE,  AREX_ALIGN_LEFT, AREX_SEP_DASHED, 0 },
/* row 3: BATT + WTM 双拼 */
{ AREX_MODULE_BATT, AREX_MODULE_WTM,  5, 2,
  AREX_FONT_ID_SMALL,  AREX_FONT_ID_SMALL,  AREX_ALIGN_LEFT, AREX_SEP_DASHED, 0 },
/* row 4: GAS 单栏全宽 */
{ AREX_MODULE_GAS,  AREX_MODULE_EMPTY, 6, 2,
  AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_LEFT, AREX_SEP_DASHED, 0 },
/* row 5: TIME 单栏全宽 */
{ AREX_MODULE_TIME, AREX_MODULE_EMPTY, 5, 2,
  AREX_FONT_ID_SMALL,  AREX_FONT_ID_SMALL,  AREX_ALIGN_LEFT, AREX_SEP_DASHED, 0 },
/* row 6-7: EMPTY */
{ AREX_MODULE_EMPTY, AREX_MODULE_EMPTY, 0, 0,
  0, 0, AREX_ALIGN_LEFT, AREX_SEP_NONE, 0 },
{ AREX_MODULE_EMPTY, AREX_MODULE_EMPTY, 0, 0,
  0, 0, AREX_ALIGN_LEFT, AREX_SEP_NONE, 0 },
```

> **【零号铁律】LVGL 8.3 分割线实现规范**
>
> LVGL 8.3 的原生 `border`（边框）属性**不支持虚线**！绝对禁止使用 `lv_obj_set_style_border_*` 来画分割线。
>
> 分割线渲染规则（`arex_screen.c` 工厂函数）：
>
> | `sep_style` | 渲染对象 | 实现方式 |
> |---|---|---|
> | `AREX_SEP_NONE` (0) | 无 | 不创建任何对象 |
> | `AREX_SEP_SOLID` (1) | `lv_obj` 实线色块 | 纯色矩形，背景色 + bg_opa |
> | `AREX_SEP_DASHED` (2) | `lv_line` + 原生虚线引擎 | `line_dash_width=6` + `line_dash_gap=4` |
> | `AREX_SEP_DOTTED` (3) | `lv_line` + 原生点线引擎 | `line_dash_width=thick` + `line_dash_gap=thick*2` |
>
> **内存管理铁律**：`lv_line_set_points()` 传入的 `lv_point_t[]` 指针由调用方托管，LVGL 不会自动释放。每次重建分割线时：
> 1. 先通过 `lv_obj_get_user_data()` 取出旧 pts，调用 `lv_mem_free()` 释放
> 2. `lv_line_create()` 时通过 `lv_obj_add_event_cb(sep, line_delete_cb, LV_EVENT_DELETE, pts)` 将 pts 绑定到事件回调
> 3. `line_delete_cb` 在对象销毁时自动释放 pts，防止内存泄漏
>
> **关键数据结构**：`arex_anchor_comp_t` 新增 `sep_style`/`sep_thick` 字段，由 `arex_calc_anchor_layout()` 从 `left_layout[]` 填充。

**APP 自由双拼示例**：将 BATT 和 GAS 拼在同一行：
```json
{
  "left_layout": [
    { "left_module": 1, "right_module": 0, "h_u": 8, ... },  // DEPTH
    { "left_module": 2, "right_module": 3, "h_u": 6, ... },  // NDL+TTS
    { "left_module": 6, "right_module": 8, "h_u": 5, ... },  // BATT+GAS ← 自由双拼！
    ...
  ]
}
```
单片机收到后调用 `arex_ui_apply_config()`，整个左侧面板自动重排，无需改任何 C 代码。

### 16.5 布局引擎函数一览

| 函数 | 文件 | 作用 |
|------|------|------|
| `arex_calc_anchor_layout()` | arex_ui_engine.c | 遍历 `left_layout[]`，填 `arex_anchor_comp_t[]`（单栏1入口，双拼2入口），返回实际 count |
| `arex_calc_tech_layout()` | arex_ui_engine.c | Tech 模式左右分区坐标：左锚点固定 160px，右区域 `= safe_zone_w - 160 - gap` |
| `arex_calc_classic_layout()` | arex_ui_engine.c | Classic 模式上下分区，最小高度 `AREX_MIN_CLASSIC_TOP_H=200px` |
| `arex_calc_widget_cell()` | arex_ui_engine.c | 5x6 网格单元格坐标，`unit_w = parent_w/5`，`unit_h = parent_h/6` |
| `arex_calc_tissue_bars()` | arex_ui_engine.c | 16 柱组织图 X 坐标，`col_w = total_w/16` |
| `left_anchor_create()` | arex_screen.c | 首次创建，按 `c->module` 枚举驱动，无索引硬编码 |
| `left_anchor_rebuild()` | arex_screen.c | 配置变更后重建，数据驱动字体/对齐刷新 |
| `right_panel_create()` | arex_screen.c | 创建 tileview，按 `card_order[]` 顺序挂载卡片 |

### 16.6 关键命名常量（防止硬编码）

| 常量 | 值 | 用途 |
|------|-----|------|
| `AREX_BASE_U` | 10px | 所有 U 单位乘数 |
| `AREX_MIN_CLASSIC_TOP_H` | 200px | Classic 模式最小上区高度 |
| `AREX_MASK_EDGE_GUARD` | 80px | 面镜盲区掩膜底部警戒阈值 |
| `AREX_LEFT_ANCHOR_W` | 160px | 左侧锚点固定宽度 |
| `ANCHOR_COMP_COUNT` | 16 | 左侧组件最大句柄数（布局输出缓冲） |
| `AREX_MAX_LEFT_ROWS` | 8 | 左侧最大行配置数 |

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

### 16.9 渲染流程（自由双拼版）

```
arex_calc_anchor_layout()
  for each row in left_layout[]:
    left_mod  = left_layout[row].left_module
    right_mod = left_layout[row].right_module
    if left_mod == EMPTY: continue
    if right_mod == EMPTY:
      → 单栏: 填充 comps[out_idx++] (w=160px, split=0)
    else:
      → 双拼: 填充 comps[out_idx++] (w=80px, split=1)  // 左块
              填充 comps[out_idx++] (w=80px, split=2)  // 右块
    cur_y += h_px + gap

left_anchor_create() / left_anchor_rebuild()
  for i in 0..count-1:
    c = comps[i]
    switch (c->module):
      case DEPTH:  render "DEPTH", "45.2" ...
      case NDL:    render "NDL", "5" ...
      case TTS:    render "TTS", "24'" ...
      ...
      // 零双拼硬编码：任意模块均可出现在任意行！
```

---

## 17. 字体系统：ID 映射引擎（v2026-04-23）

### 17.1 核心铁律

> **零号铁律**：所有配置结构体（`arex_left_row_cfg_t`、`arex_anchor_comp_t` 等）中只允许保存字体 ID（枚举值），禁止保存 `lv_font_t*` 指针！APP 只能下发数字 ID，渲染引擎通过 `arex_get_font(id)` 统一映射。

### 17.2 字体 ID 枚举字典

```c
typedef enum {
    AREX_FONT_ID_SMALL  = 0,  /* 14px  标签/单位/Badge */
    AREX_FONT_ID_TITLE,       /* 20px  菜单项/卡片标题 */
    AREX_FONT_ID_MEDIUM,      /* 28px  数据值 */
    AREX_FONT_ID_HUGE,         /* 48px  深度大数字 */
} arex_font_id_t;
```

### 17.3 字体映射表

| ID 枚举 | 像素 | 用途 |
|---------|------|------|
| `AREX_FONT_ID_SMALL`  (0) | 14px | 标签/单位/Status Badge |
| `AREX_FONT_ID_TITLE`  (1) | 20px | 菜单项/卡片标题 |
| `AREX_FONT_ID_MEDIUM` (2) | 28px | 数据值 |
| `AREX_FONT_ID_HUGE`   (3) | 48px | 深度大数字（规范 58px 最近） |

### 17.4 `arex_get_font()` 映射器

字体映射器是全系统中**唯一**允许将字体 ID 转换为真实 `lvgl` 字体指针的地方，位于 `arex_ui_engine.c`：

```c
/* 声明字体资源（由 arex_fonts.h 中的 LV_FONT_DECLARE 提供） */
#include "fonts/arex_fonts.h"

const lv_font_t *arex_get_font(uint8_t font_id)
{
    switch (font_id) {
        case AREX_FONT_ID_SMALL:  return AREX_FONT_SMALL;   /* 14px */
        case AREX_FONT_ID_TITLE:  return AREX_FONT_TITLE;   /* 20px */
        case AREX_FONT_ID_MEDIUM: return AREX_FONT_MEDIUM;  /* 28px */
        case AREX_FONT_ID_HUGE:   return AREX_FONT_HUGE;   /* 48px */
        default:                   return AREX_FONT_SMALL;   /* 永不为 NULL */
    }
}
```

### 17.5 正确用法 vs 错误用法

```c
/* 正确：传 ID，运行时映射 */
lv_obj_set_style_text_font(obj, arex_get_font(AREX_FONT_ID_HUGE), 0);
lv_obj_set_style_text_font(obj, arex_get_font(row->val_font), 0);

/* 错误：直接传指针（无法被 APP 同步） */
lv_obj_set_style_text_font(obj, &lv_font_courier_48, 0);
lv_obj_set_style_text_font(obj, AREX_FONT_HUGE, 0);
```

### 17.6 默认行配置中的字体 ID

```c
/* row 0: DEPTH — 标题 SMALL(0)，数值 HUGE(3) */
{ AREX_MODULE_DEPTH, ..., AREX_FONT_ID_SMALL,  AREX_FONT_ID_HUGE,   ... }
/* row 1: NDL+TTS — 标题 SMALL(0)，数值 MEDIUM(2) */
{ AREX_MODULE_NDL,  ..., AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, ... }
/* row 2: POD1+POD2 — 标题 SMALL(0)，数值 TITLE(1) */
{ AREX_MODULE_POD1, ..., AREX_FONT_ID_SMALL,  AREX_FONT_ID_TITLE,  ... }
/* row 3: BATT+WTM — 标题 SMALL(0)，数值 SMALL(0) */
{ AREX_MODULE_BATT, ..., AREX_FONT_ID_SMALL,  AREX_FONT_ID_SMALL,  ... }
```

### 17.7 字体声明来源

所有字体资源声明统一在 `fonts/arex_fonts.h` 中：

```c
#include "lvgl/lvgl.h"

#ifndef LV_FONT_COURIER_14
#define LV_FONT_COURIER_14 1
#endif
/* LV_FONT_COURIER_20/28/48 同理 */

LV_FONT_DECLARE(lv_font_courier_14)
LV_FONT_DECLARE(lv_font_courier_20)
LV_FONT_DECLARE(lv_font_courier_28)
LV_FONT_DECLARE(lv_font_courier_48)

/* 角色别名（仅在 arex_ui_engine.c 内部映射器引用） */
#define AREX_FONT_SMALL    (&lv_font_courier_14)
#define AREX_FONT_TITLE    (&lv_font_courier_20)
#define AREX_FONT_MEDIUM    (&lv_font_courier_28)
#define AREX_FONT_HUGE     (&lv_font_courier_48)
```

### 17.8 废弃宏（仅兼容旧代码）

`arex_screen.h` 中保留了 4 个旧的 `AREX_FONT_*` 宏作为兼容层，**新代码禁止使用**：

```c
/* arex_screen.h — 已废弃，仅兼容旧代码 */
#define AREX_FONT_HUGE    (&lv_font_courier_48)
#define AREX_FONT_MEDIUM  (&lv_font_courier_28)
#define AREX_FONT_SMALL   (&lv_font_courier_14)
#define AREX_FONT_TITLE   (&lv_font_courier_20)
```

### 17.9 字体系统变更日志

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-23 | `arex_ui_engine.h` | 新增 `arex_font_id_t` 枚举字典；`arex_get_font()` 声明 |
| 2026-04-23 | `arex_ui_engine.c` | 实现 `arex_get_font()` 映射器；引入 `fonts/arex_fonts.h`；删除废弃的 `font_sz_*` 字段和默认值 |
| 2026-04-23 | `arex_ui_engine.h` | 删除 `arex_sys_config_t` 中废弃的 `align_title/huge/med` 字段 |
| 2026-04-23 | `arex_screen.c` | 所有 `AREX_FONT_*` 宏替换为 `arex_get_font(id)`；删除 `font_cat[]` 中间层数组（两处） |
| 2026-04-23 | `cards/*.c` | 所有卡片中 `AREX_FONT_*` 宏替换为 `arex_get_font(id)` |
| 2026-04-23 | `arex_screen.h` | 旧宏标记为废弃，附正确用法注释 |
| 2026-04-23 | `AREX_ARCH.md` | 新增 Section 17 字体映射引擎文档 |

---

## 18. 重构变更日志（v2026-04-23 第二阶段）

### 18.1 文件变更总览

| 文件 | 操作 | 说明 |
|------|------|------|
| `arex_card_registry.c` | 重写 | 卡片注册表重构：新增 `arex_card_count()`、`g_sys_card_order()` 间接查询、`tile_obj` 初始化流程 |
| `arex_card_registry.h` | 重写 | 新增 `arex_card_pos_t` 位置枚举（INFO/SETUP 固定，中间 4 个可重排）；`arex_card_reg_t` 新增 `on_enter_cb`；`tile_obj` 初始为 NULL |
| `arex_ui_engine.c` | 重写 | `arex_sys_config_defaults()` 用 `CARD_POS_*` / `CARD_ID_*` 枚举显式赋值 `card_order[]`，替代旧的 `for` 循环赋值 |
| `arex_ui_engine.h` | 新增 | 新增 `g_sys_card_order()` 声明；新增 `arex_ui_update_data()` 空钩子 |
| `arex_ui_state.c` | 重写 | `arex_ui_refresh_all()` 改为 `arex_card_count()` 循环；`ui_handle_rotate()` 中 `AREX_CARD_COUNT - 2` 改为 `AREX_DASH_CARD_COUNT` |
| `arex_data.h/c` | 新建 | 数据总线头文件存根，所有定义保留在 `arex_ui_engine.h` |
| `card_info.c` | 重写 | 改为 `arex_get_font()` + `arex_data.h`；引入 `arex_render_dynamic_menu()` 动态菜单工厂 |
| `card_setup.c` | 重写 | badge 刷新逻辑保留，句柄数组改为工厂输出 |
| `card_gas.c` | 清理 | `GAS_ROW_GAP` 删除，`gap_y` 改为 `gap_menu` 配置推算 |
| `UI_main.c` | 重写 | 移除 `lv_timer_create`（已在 `arex_screen_create` 中创建）；启动直接进入 INFO 卡 |
| `AREX_ARCH.md` | 更新 | 本次重构写入文档 |

### 18.2 `arex_card_registry.c` 要点

#### 静态注册表使用指定初始化器

```c
static arex_card_t g_cards[AREX_CARD_COUNT] = {
    [CARD_ID_INFO] = {
        .id          = CARD_ID_INFO,
        .title       = "INFO MENU",
        .engine_type = CARD_ENGINE_MENU,
        .config_data = &info_menu_cfg,
        .tile_obj    = NULL,          // create 后才填入
        .create_cb   = card_info_create,
        .update_cb   = card_info_update,
        .on_enter_cb = NULL,
    },
    [CARD_ID_COMPASS] = { ... },
    // ... 共 8 张卡片
};
```

#### `arex_card_get()` 通过 `g_sys_card_order()` 间接映射

```c
arex_card_t *arex_card_get(uint8_t order_pos)
{
    if (order_pos >= AREX_CARD_COUNT) return NULL;
    uint8_t id = g_sys_card_order(order_pos);   // 查 card_order[]
    if (id >= AREX_CARD_COUNT) return NULL;
    return &g_cards[id];
}
```

#### `arex_ui_refresh_all()` 用 `arex_card_count()` 替代硬编码

```c
void arex_ui_refresh_all(void)
{
    for (uint8_t i = 0; i < arex_card_count(); i++) {
        arex_card_t *c = arex_card_get(i);
        if (c && c->update_cb) c->update_cb();
    }
}
```

### 18.3 启动流程

```
arex_ui_state_init() -> state=UI_INFO, dash_card=1, menu_info_idx=0
UI_main() -> arex_screen_scroll_to_card(0), arex_screen_set_info_selection(0)
```
启动直接进入 INFO 菜单（tile 0），等待用户操作。模拟定时器在 `UI_main()` 中创建。

### 18.4 `tile_obj` 生命周期

```
create 阶段:
  arex_card_registry.c: s_registry[i].tile_obj = NULL（初始化）
  card_*.c: create_cb() 创建 tile 控件 -> 返回 parent
  right_panel_create(): 捕获 tile 对象 -> 填入 registry
    -> registry[i].tile_obj = tile_obj;

update 阶段:
  任意模块通过 arex_card_get_by_id(id)->tile_obj 访问
```


### 18.5 新增 API / 枚举速查

| 枚举 / 宏 | 文件 | 说明 |
|------|------|------|
| `arex_card_pos_t` | arex_card_registry.h | 位置枚举：CARD_POS_INFO=0(固定), CARD_POS_1~4(可重排), CARD_POS_SETUP=5(固定) |
| `arex_card_id_t` | arex_card_registry.h | 卡片固有身份枚举：CARD_ID_INFO ~ CARD_ID_SETUP |
| `AREX_CARD_COUNT` | arex_card_registry.h | 卡片总数 = 8 |
| `AREX_DASH_CARD_COUNT` | arex_card_registry.h | DASH 可滑动数 = 6 |

| 函数 | 文件 | 作用 |
|------|------|------|
| `arex_card_count()` | arex_card_registry.c | 返回卡片总数 |
| `arex_card_get(pos)` | arex_card_registry.c | 按位置取卡片（走 card_order[] 间接层） |
| `arex_card_get_by_id(id)` | arex_card_registry.c | 按 ID 取卡片（不走间接层） |
| `g_sys_card_order(pos)` | arex_ui_engine.c | 通过 card_order[] 查询卡片 ID |
| `arex_ui_refresh_all()` | arex_ui_state.c | 遍历所有卡片执行 update 回调 |
| `arex_ui_update_data()` | arex_ui_engine.c | 空钩子，供未来扩展数据更新逻辑 |
| `arex_ui_state_init()` | arex_ui_state.c | 初始化 UI 上下文，启动 state=UI_INFO |
| `arex_screen_register_info_list()` | arex_screen.c | INFO 列表注册（由 card_info.c 调用） |
| `arex_screen_register_setup_list()` | arex_screen.c | SETUP 列表注册（由 card_setup.c 调用） |

---

## 19. 右侧卡片动态菜单引擎（v2026-04-23）

### 19.1 核心目标

彻底消灭 `card_info.c` / `card_setup.c` 中写死的 `lv_obj_create` 循环。所有菜单的选项数量、文本、字体、边框全部解耦至配置结构体数组，由 APP 通过 JSON/结构体动态下发。

### 19.2 配置结构体

**`arex_ui_engine.h`** 中新增：

```c
typedef struct {
    const char *title_text;      /* 左侧主文本 (可为空) */
    const char *value_badge;     /* 右侧数值/状态徽章 (可为空) */
    uint8_t      title_font_id;   /* 标题字体 ID: arex_font_id_t */
    uint8_t      value_font_id;   /* 徽章字体 ID: arex_font_id_t */
    uint8_t      border_width;    /* 边框粗细 px，0=无边框 */
    uint8_t      height_u;        /* 高度 (单位 U，0=用 h_menu_item 默认值) */
} arex_menu_item_cfg_t;

void arex_render_dynamic_menu(lv_obj_t *parent_card,
                              const arex_menu_item_cfg_t *items,
                              uint8_t item_count,
                              int start_y,
                              lv_obj_t **out_item_handles);
```

### 19.3 工厂函数尺寸推算规则

`arex_render_dynamic_menu()` 内部所有尺寸全部从 `g_sys_config` 推算，无硬编码像素：

| 参数 | 来源 |
|------|------|
| `item_h` | `height_u > 0 ? height_u : h_menu_item` × `AREX_BASE_U` |
| `gap_y` | `g_sys_config.gap_menu × AREX_BASE_U` |
| `item_w` | `safe_zone_w - LEFT_ANCHOR_W - gap_u×AREX_BASE_U - 15` |

### 19.4 业务代码重构对照

#### card_info.c

**重构前**：66 行硬编码循环，`item_h=48` 写死。

**重构后**：配置数组 + 1 行工厂调用：

```c
static const arex_menu_item_cfg_t s_info_items[] = {
    { "> LAST DIVE",       NULL, AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> DIVE PLAN",       NULL, AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> TISSUE & TOX",   NULL, AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> GAS & CALC",      NULL, AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
    { "> SENSOR & DEVICE", NULL, AREX_FONT_ID_TITLE, AREX_FONT_ID_SMALL, 2, 0 },
};
#define INFO_ITEM_COUNT (sizeof(s_info_items) / sizeof(s_info_items[0]))

/* 列表总高度从 h_menu_item 和 gap_menu 推算 */
uint16_t item_h_px = (uint16_t)g_sys_config.h_menu_item * AREX_BASE_U;
uint16_t gap_y_px  = (uint16_t)g_sys_config.gap_menu * AREX_BASE_U;
uint16_t list_h = INFO_ITEM_COUNT * item_h_px
                  + (INFO_ITEM_COUNT - 1) * gap_y_px;

arex_render_dynamic_menu(s_list, s_info_items, INFO_ITEM_COUNT, 0, NULL);
```

#### card_setup.c

badge 刷新逻辑完整保留，句柄数组改为工厂输出：

```c
arex_render_dynamic_menu(s_list, s_setup_items, SETUP_ITEM_COUNT, 0, s_setup_item_objs);

for (uint8_t i = 0; i < SETUP_ITEM_COUNT; i++) {
    s_setup_badge_lbls[i] = lv_obj_get_child(s_setup_item_objs[i], 1);
}
```

### 19.5 card_gas.c 特殊处理

GAS 卡片每行包含三个 label（气体名 + MOD + PPO2），超出通用菜单工厂能力范围，保留原有多标签行渲染逻辑。仅将 `GAS_ROW_GAP` 从硬编码 `8` 改为 `g_sys_config.gap_menu × AREX_BASE_U`。

### 19.6 APP 同步协议扩展

APP 动态下发菜单配置示例：

```json
{
  "menus": {
    "info": {
      "items": [
        { "title": "> LAST DIVE", "badge": null, "title_font": 1, "value_font": 0, "border": 2, "height_u": 0 }
      ]
    }
  }
}
```

---

## 20. Section 19 变更日志（v2026-04-23）

|| 日期 | 文件 | 变更 |
||------|------|------|
|| 2026-04-23 | `arex_ui_engine.h` | 新增 `arex_menu_item_cfg_t` 及 `arex_render_dynamic_menu()` 声明 |
|| 2026-04-23 | `arex_ui_engine.c` | 实现工厂函数，height/gap 全程从 `g_sys_config` 推算 |
|| 2026-04-23 | `card_info.c` | 完全重构：配置数组 + 1 行工厂调用 |
|| 2026-04-23 | `card_setup.c` | 完全重构：badge 刷新逻辑保留，句柄数组改为工厂输出 |
|| 2026-04-23 | `card_gas.c` | `GAS_ROW_GAP` 宏删除，`gap_y` 改为配置推算 |
|| 2026-04-23 | `AREX_ARCH.md` | 新增 Section 19/20 |


## 21. 5F 自定义网格引擎 (v2026-04-23)

### 21.1 核心设计原则

**lv_grid**：MCU 资源受限，动态重建 lv_grid 极耗内存 and 性能。F 渲染完全采用"行×列相乘→绝对物理坐标"纯数学映射。

**零硬编码字号**：字号由组件跨度自动决定，渲染器只查元数据表，不靠 `if(w_id==DEPTH)` 判断。

**靶向告警烙印**：每个组件创建时 `lv_obj_set_user_data(obj, (void*)(uintptr_t)widget_id)` 打上全系统唯一身份烙印，告警引擎靠此烙印实现"左侧锚点 + 5F 组件同时反色闪烁"。

### 21.2 配置数据（来自 g_sys_config）

```
uint8_t  widget_count;    /* 当前装填的组件数量 (最大 30) */
uint8_t  widget_ids[30];  /* 组件类型: arex_widget_id_t */
uint8_t  widget_r[30];    /* 起始行 0~5 */
uint8_t  widget_c[30];    /* 起始列 0~4 */
uint8_t  widget_w[30];    /* 列跨度 1~2 */
uint8_t  widget_h[30];    /* 行跨度 1~2 */
```

APP 下发示例：

```json
{
  "widget_count": 6,
  "widget_ids": [1, 2, 3, 4, 5, 6],
  "widget_r":  [0, 0, 0, 1, 1, 2],
  "widget_c":  [0, 2, 3, 0, 2, 2],
  "widget_w":  [2, 1, 2, 2, 2, 2],
  "widget_h":  [2, 1, 1, 1, 1, 1]
}
```

### 21.3 组件 ID 枚举

| ID | 组件 | 数据源 |
|----|------|--------|
| 0 | EMPTY | - |
| 1 | DEPTH | `g_sensor_data.depth` |
| 2 | TEMP | `g_sensor_data.temp` |
| 3 | HEADING | `g_sensor_data.heading` |
| 4 | SAC_RATE | `g_sensor_data.sac_rate` |
| 5 | BATTERY | `g_sensor_data.battery_pct` |
| 6 | NDL | `g_sensor_data.ndl` |
| 7 | TTS | `g_sensor_data.tts` |
| 8 | PPO2 | `g_sensor_data.ppo2[active_gas]` |
| 9 | CNS | `g_sensor_data.cns_pct` |
| 10 | POD1 | `g_sensor_data.pod1_bar` |
| 11 | POD2 | `g_sensor_data.pod2_bar` |
| 12 | W.TIME | `g_sensor_data.dive_time_s` |

### 21.4 纯数学绝对坐标映射（含 40px 标题避让 + 锁定 80x60 基准）

> **重要**：`AREX_CARD_TITLE_H = 40px`（4U），即常规卡片绿色大标题+分割线的占用高度。60px 的区域是**告警横幅悬浮覆盖区**，不属于常规标题区。

```
排版矩阵严格锁定 80x60 基准（完美整数）:
  cell_w = 80px  (tile_w=400 / 5 列)
  cell_h = 60px  ((tile_h=400 - AREX_CARD_TITLE_H=40) / 6 行)

abs_x  = col * 80 + WIDGET_GAP              (WIDGET_GAP=2px 缝隙)
abs_y  = AREX_CARD_TITLE_H + row * 60 + WIDGET_GAP
abs_w  = span_w * 80 - WIDGET_GAP * 2       (留2px制造4px物理留白)
abs_h  = span_h * 60 - WIDGET_GAP * 2
```

**物理尺寸对照**：

| 跨度 | 逻辑尺寸 | 物理尺寸(含留白) |
|------|----------|-----------------|
| 1x1  | 80x60    | 76x56           |
| 2x1  | 160x60   | 156x56          |
| 1x2  | 80x120   | 76x116          |
| 2x2  | 160x120  | 156x116          |

> **关键**：row=0 时 `abs_y = 40 + 0 + 2 = 42px`，确保第一排组件落在标题区（~40px）下方的黑色内容区内。

### 21.5 字号自适应引擎

| 跨度条件 | 选用字体 | 字号 |
|----------|----------|------|
| `span_w>=2 && span_h>=2` | `AREX_FONT_ID_HUGE` | 48px |
| `span_w>=2 || span_h>=2` | `AREX_FONT_ID_MEDIUM` | 28px |
| `span_w==1 && span_h==1` | `AREX_FONT_ID_SMALL` | 14px |

### 21.6 靶向告警同步引擎

当 `arex_trigger_alarm(level, text, target_id)` 调用时：

1. 弹出横幅（纯英文，永不显示图案）
2. 遍历所有容器子节点，`lv_obj_get_user_data()` 匹配 `target_id`
3. 同步闪烁：`CRIT` ~2Hz(500ms) / `WARN` ~1Hz(1000ms) / `INFO` 仅横向
4. 消失时调用 `arex_clear_all_alarm_styles()`

### 21.7 核心 API

| 函数 | 说明 |
|------|------|
| `arex_render_5f_custom_grid()` | 总线渲染器，遍历配置数组渲染所有组件 |
| `arex_widget_set_value()` | 按 ID 更新数据 label，绝不触发重建 |
| `arex_trigger_alarm()` | 靶向告警触发 |
| `arex_clear_all_alarm_styles()` | 清除所有告警样式 |
| `arex_get_widget_name()` | 按 ID 获取显示名称 |
| `arex_calc_widget_grid()` | 网格→绝对坐标（含TITLE_ZONE_H=40px避让偏移，锁定 80x60 基准） |
| `arex_get_widget_name()` | 按 ID 获取显示名称 |
| `arex_calc_widget_grid()` | 网格→绝对坐标（含TITLE_ZONE_H=40px避让偏移，锁定 80x60 基准） |
| `arex_show_alarm_banner()` | 纯英文告警横幅 |

---

## 22. 卡片注册系统 (Card Registry)

### 22.1 设计目标

单张表管理所有卡片信息，新增卡片只需在 `g_cards[]` 加一条，无需修改 `arex_screen.c`。

### 22.2 核心数据结构

```c
/* arex_card_registry.h */

typedef enum {
    CARD_ENGINE_MENU   = 0,   /* create_cb() 完整创建（含 list 注册） */
    CARD_ENGINE_GRID   = 1,   /* arex_render_5f_custom_grid()         */
    CARD_ENGINE_CHART  = 2,   /* 预留                                   */
    CARD_ENGINE_CUSTOM = 3,   /* create_cb() 完整创建                  */
} arex_card_engine_t;

typedef struct {
    arex_card_id_t      id;
    const char         *title;
    arex_card_engine_t  engine_type;
    const void         *config_data;   /* arex_menu_list_cfg_t* for MENU engine */
    lv_obj_t           *tile_obj;      /* filled at runtime by right_panel_create() */
    void (*create_cb)(lv_obj_t *parent);
    void (*update_cb)(void);
    void (*on_enter_cb)(void);
} arex_card_t;
```

```c
/* arex_ui_engine.h */

typedef struct {
    const arex_menu_item_cfg_t *items;
    uint8_t                    count;
} arex_menu_list_cfg_t;
```

### 22.3 引擎分发流程

```
right_panel_create()
  └─ for each card in g_cards[]
       ├─ CARD_ENGINE_GRID → make_title + render_5f_custom_grid()
       └─ 其余             → card->create_cb(tile)
```

### 22.4 文件职责

| 文件 | 职责 |
|------|------|
| `arex_card_registry.h` | `arex_card_engine_t` + `arex_card_t` 类型定义 + API 声明 |
| `arex_card_registry.c` | `g_cards[]` 单张统一表（ROM 字段 + 运行时 tile_obj） |
| `arex_ui_engine.h` | `arex_menu_item_cfg_t` + `arex_menu_list_cfg_t` 配置结构体 |
| `card_info.c` | 暴露 `info_menu_cfg`，`create_cb` 内注册 `s_info_list` |
| `card_setup.c` | 暴露 `setup_menu_cfg`，`create_cb` 内注册 `s_setup_list` |
| `arex_screen.c` | `right_panel_create()` 引擎分发循环 |

### 22.5 当前卡片引擎映射

| 卡片 | engine_type | 说明 |
|------|-------------|------|
| `CARD_ID_INFO` | `CARD_ENGINE_MENU` | `card_info_create()` 完整创建，含 list 注册 |
| `CARD_ID_COMPASS` | `CARD_ENGINE_CUSTOM` | `card_compass_create()` canvas 绘制 |
| `CARD_ID_DECO` | `CARD_ENGINE_CUSTOM` | `card_deco_create()` 柱状图 + GF/CNS/OTU |
| `CARD_ID_GAS` | `CARD_ENGINE_CUSTOM` | `card_gas_create()` 4 行气体行，含静态句柄 |
| `CARD_ID_PLAN` | `CARD_ENGINE_CUSTOM` | `card_plan_create()` canvas 潜水剖面图 |
| `CARD_ID_SETUP` | `CARD_ENGINE_MENU` | `card_setup_create()` 完整创建，含 list 注册 |

> `CARD_ENGINE_GRID`（`arex_render_5f_custom_grid()`）目前由 5F 自定义卡片使用，详见 Section 23。

### 22.6 新增卡片步骤

**扩展配置数据（两步铁律）**：

新增一个 MENU 类型卡片（假设加 `6F: CAMERA`）只需：

```
Step 1: 在 arex_card_configs.c 中定义配置数据
    const arex_menu_item_cfg_t g_camera_items[] = { ... };
    extern const uint8_t g_camera_item_count;

Step 2: 在 arex_ui_engine.c 的 g_card_registry[] 之前加静态配置变量
    static const arex_card_menu_cfg_t s_cfg_camera = { g_camera_item_count, g_camera_items };

Step 3: 在 arex_ui_engine.c 的 g_card_registry[] 中加一行
    [CARD_ID_CAMERA] = {
        .engine_type = CARD_ENGINE_MENU,
        .title      = "6F: CAMERA",
        .config_data = &s_cfg_camera,
        .render_cb  = card_camera_render,
        .update_cb  = card_camera_update,
    },
```

> **注意**：`g_card_registry[]` 是 `const` 静态数组，C 标准不允许在其中直接使用复合字面量作为初始化器（如 `&(arex_card_menu_cfg_t){ ... }`），因为表达式在编译期不可求值。必须先定义 `static const` 桥接变量，再用其地址初始化数组。

---

## 23. 5F 标题区保护与坐标避让修复 (v2026-04-24)

### 23.1 问题描述

**现象**：`CARD_ENGINE_GRID` 卡片渲染时，网格组件（DEPTH、TEMP 等）直接顶到 Y=0，顶部绿色标题文字和分割线完全被黑色网格背景覆盖。

**根因**：`arex_calc_widget_grid()` 中 `out_y = parent_y + row * cell_h + WIDGET_GAP`，当 row=0 时 `out_y=2`。而 `arex_screen_make_card_title()` 渲染的标题 label 在 y=12、标题线在 y=38，整个黑色网格组件（黑色背景）完全覆盖了绿色标题。

### 23.2 修复方案

**增加标题区常量**：

```c
// arex_ui_engine.h
#define AREX_CARD_TITLE_H   40  /* 卡片顶部标题区高度(4U)，包含绿色标题文字+分割线 */
```

> **注意**：视觉规范中 60px 的区域是**告警横幅悬浮覆盖区**，不属于常规卡片标题。常规卡片绿色大标题+分割线严格占用 **40px**。

**重构 `arex_calc_widget_grid()` 签名与逻辑**：

- 移除 `parent_x/parent_y` 参数（不再需要）
- 锁定 80x60 基准网格：`cell_w=80px (400/5)`, `cell_h=60px ((400-40)/6)`
- Y 坐标增加 `AREX_CARD_TITLE_H=40` 偏移量：`out_y = AREX_CARD_TITLE_H + row * 60 + WIDGET_GAP`
- row=0 时 `out_y = 40 + 0 + 2 = 42px`，恰好落在标题区下方

**数学验证**（tile 为 400px）：

```
cell_w = 400 / 5 = 80px
cell_h = (400 - 40) / 6 = 60px
row=0:  out_y = 40 + 0*60 + 2 = 42  → 超过标题线 (y=38)，落入黑色区域 ✓
row=5:  out_y = 40 + 5*60 + 2 = 342, out_h = 60-4 = 56, 底线 = 342+56 = 398 < 400 ✓
```

### 23.3 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-24 | `arex_ui_engine.h` | 新增 `AREX_CARD_TITLE_H` 常量(40px)；新增 `arex_calc_widget_grid()` 公开声明 |
| 2026-04-24 | `arex_ui_engine.c` | 重构 `arex_calc_widget_grid()`：移除 parent_x/y，锁定 80x60 基准，增加 40px 标题避让偏移 |
| 2026-04-24 | `AREX_ARCH.md` | 新增 Section 23 记录本次修复 |

### 23.4 API 签名变更

**旧签名**（已废弃）：
```c
void arex_calc_widget_grid(int16_t parent_x, int16_t parent_y,
                            uint16_t parent_w, uint16_t parent_h,
                            uint8_t row, uint8_t col,
                            uint8_t span_w, uint8_t span_h,
                            int16_t *out_x, int16_t *out_y,
                            uint16_t *out_w, uint16_t *out_h);
```

**新签名**（当前）：
```c
void arex_calc_widget_grid(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t span_w, uint8_t span_h,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h);
```

### 23.5 渲染层级保证

路由到 `CARD_ENGINE_GRID` 分支的调用顺序：
```c
arex_screen_make_card_title(tile, card->title);  // 先画标题（label y=12, 线 y=38）
arex_render_5f_custom_grid(tile, ...);            // 后画网格（row=0 的组件从 y=52 开始）
```

网格组件 Z-Order 在标题之后创建，自然覆盖标题区下方的黑色区域，不会覆盖标题文字和分割线。

---

## 24. 左侧面板数据修复 (v2026-04-24)

### 24.1 问题描述

**问题 1：W.TIME 始终显示 `00:00`**
- `AREX_MODULE_WTM` 的数据 label 读取 `g_sensor_data.surface_time_s`，但 `sim_tick_cb` 中从未递增此字段
- 根因：`UI_main.c` 中 `sim_tick_cb` 只更新了 `dive_time_s`，漏掉了 `surface_time_s`

**问题 2：TIME 标签创建时硬编码 `00:00`**
- `AREX_MODULE_TIME` 的 label 在 `left_anchor_create()` 中创建时直接设置 `"00:00"`
- 虽然 `arex_screen_refresh_left_panel()` 有正确的刷新逻辑，但初始值应为数据总线的当前值

**问题 3：POD1/POD2 初始值显示为 `210` / `195` 而非 `"--"`**
- `arex_ui_engine.c` 中 `arex_ui_init()` 中 `pod1_bar=210.0f`，`pod2_bar=195.0f` 为模拟值
- 下水后 POD 未连接时应显示 `"--"`
- 渲染层直接 `snprintf("%.0f")` 输出数字，无 0 值判断

**问题 4：PO2 1 和 PO2 2 未显示 `"--"`**
- 初始文本已在 `left_anchor_create()` 中改为 `"--"`，刷新函数中也改为固定 `"--"`
- 需确认重编译后生效

### 24.2 修复方案

**修复 1：`UI_main.c` 中 `sim_tick_cb` 中增加 `surface_time_s` 递增**

```c
g_sensor_data.dive_time_s += 1;
g_sensor_data.surface_time_s += 1;  // 新增：水面休息计时同步递增
```

**修复 2：`arex_screen.c` 中 `left_anchor_create()` 中 `AREX_MODULE_TIME` 改用数据总线**

```c
case AREX_MODULE_TIME:
    snprintf(buf, sizeof(buf), "%02d:%02d",
             g_sensor_data.dive_time_s / 60,
             g_sensor_data.dive_time_s % 60);
    lv_label_set_text(lbl_val, buf);
    s_lbl_time = lbl_val;
    break;
```

**修复 3：POD1/POD2 初始值归零，渲染层按 0 = `"--"` 处理**

统一约定：气压传感器 `pod1_bar / pod2_bar` 为 `0.0f` 代表"未连接"，渲染时显示 `"--"`。

```c
// arex_ui_engine.c:513-514 初始值归零
g_sensor_data.pod1_bar = 0.0f;
g_sensor_data.pod2_bar = 0.0f;

// arex_screen.c left_anchor_create() / arex_screen_refresh_left_panel()
// 渲染时判断 0 显示 "--"
if (g_sensor_data.pod1_bar <= 0.0f)
    snprintf(buf, sizeof(buf), "--");
else
    snprintf(buf, sizeof(buf), "%.0f", g_sensor_data.pod1_bar);
```

### 24.3 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-24 | `UI_main.c` | `sim_tick_cb` 增加 `g_sensor_data.surface_time_s += 1` |
| 2026-04-24 | `arex_screen.c` | `AREX_MODULE_TIME` 创建时从硬编码 `"00:00"` 改为 `g_sensor_data.dive_time_s` |
| 2026-04-24 | `arex_ui_engine.c` | `pod1_bar / pod2_bar` 初始值从 `210.0f/195.0f` 改为 `0.0f` |
| 2026-04-24 | `arex_screen.c` | POD1/POD2 渲染（创建+刷新+INFO菜单）加 `0.0f = "--"` 判断 |
| 2026-04-24 | `AREX_ARCH.md` | 新增 Section 24 记录本次修复 |
| 2026-04-24 | `arex_ui_engine.h` | `arex_dive_pt_t.time_min` 改为 `time_s`（统一秒级单位） |
| 2026-04-24 | `card_plan.c` | 重写 `plan_chart_draw_cb` 为秒级坐标引擎；`init_test_data()` 清零 `g_dive_log_count`；新增 `arex_dive_log_append()` |
| 2026-04-24 | `arex_data.h` | 新增 `arex_dive_log_append()` 声明 |
| 2026-04-24 | `UI_main.c` | `sim_tick_cb` 改用 `arex_dive_log_append()` 推流 |
| 2026-04-24 | `AREX_ARCH.md` | 新增 Section 25 |

---

## 25. 4F 曲线时间单位统一与历史轨迹推送 (v2026-04-24)

### 25.1 问题描述

**问题 1：X 轴单位错误**
- 原代码底层以"分钟"为单位，但 X 轴网格标签在 `<120s` 时标 `"%ds"`，导致 20 分钟坐标系里 4 秒只占 0.33%，曲线像悬崖

**问题 2：历史轨迹污染未来预测**
- `g_dive_log[]` 的 `time_min` 字段被旧代码预填充未来大时间值，`g_dive_log_count` 初始化不为 0，导致预测虚线被历史数据实线覆盖

**问题 3：无统一推流接口**
- `UI_main.c` 直接操作 `g_dive_log[]` 数组，不符合数据总线统一管理原则

### 25.2 修复方案

**统一时间单位**：全系统以秒为唯一时间基准，消除分钟/秒混用。

```c
// arex_ui_engine.h
typedef struct { float time_s; float depth_m; } arex_dive_pt_t;  // 改 time_min 为 time_s
```

**历史轨迹推流接口**：

```c
// arex_data.h 声明
void arex_dive_log_append(float current_time_s, float current_depth_m);

// card_plan.c 实现
void arex_dive_log_append(float current_time_s, float current_depth_m)
{
    if (g_dive_log_count < MAX_DIVE_LOG) {
        g_dive_log[g_dive_log_count].time_s   = current_time_s;
        g_dive_log[g_dive_log_count].depth_m  = current_depth_m;
        g_dive_log_count++;
    }
}
```

**清零启动**：`init_test_data()` 中 `g_dive_log_count = 0`，轨迹从零生成。

**秒级坐标引擎**：`plan_chart_draw_cb` 核心改为：
- 当前时间 `current_t_sec = g_sensor_data.dive_time_s`（秒级）
- 升水速度 `6.0f` 米/分（对应 10m/min）
- X 轴最小锁定 20 秒视口，`fmaxf` 动态扩展
- 映射公式 `MAP_X(t_sec) = pad_x + (t_sec / max_t_axis_sec) * w`

### 25.3 X 轴秒级步长表

| 时间范围 | X 轴最大刻度 | 步长 |
|---------|-------------|------|
| 0~20s | 20s | 10s |
| 20~60s | ceil/10*10 | 10s |
| 60~120s | ceil/60*60 | 15s |
| 2~5min | ceil/60*60 | 30s |
| 5~10min | ceil/60*60 | 60s |
| 10~20min | ceil/60*60 | 120s |
| 20~60min | ceil/60*60 | 300s |

### 25.4 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-24 | `arex_ui_engine.h` | `arex_dive_pt_t.time_min` 改为 `time_s` |
| 2026-04-24 | `card_plan.c` | 完全重写 `plan_chart_draw_cb` 为秒级引擎；清零 `g_dive_log_count` |
| 2026-04-24 | `card_plan.c` | 新增 `arex_dive_log_append()` |
| 2026-04-24 | `arex_data.h` | 新增 `arex_dive_log_append()` 声明 |
| 2026-04-24 | `UI_main.c` | `sim_tick_cb` 改用 `arex_dive_log_append()` |
| 2026-04-24 | `AREX_ARCH.md` | 新增 Section 25 |
| 2026-04-24 | `arex_ui_engine.h` | `arex_sensor_data_t` 增加 `dirty_mask` 字段；新增 `arex_dirty_bit_t` 脏标记枚举；声明 Data Bus Setter + UI Consumer |
| 2026-04-24 | `arex_data.h` | 彻底重构为 Data Bus 硬件写入接口层；`arex_bus_set_*()` 系列声明 |
| 2026-04-24 | `arex_data.c` | 新建文件；实现全部 `arex_bus_set_*()` Setter，含防抖阈值 |
| 2026-04-24 | `arex_ui_engine.c` | 实现 `arex_ui_update_task()` 集中消费任务；`arex_screen.h` include |
| 2026-04-24 | `UI_main.c` | `sim_tick_cb` 全部改用 `arex_bus_set_*()`；撤销直接 `g_sensor_data` 写入；撤销 `arex_ui_refresh_all()` |
| 2026-04-24 | `UI_main.c` | `arex_ui_update_task(50ms)` 驱动 UI 渲染；`sim_tick_cb(1Hz)` 驱动数据写入 |
| 2026-04-24 | `arex_card_registry.h` | 新增所有卡片 update forward 声明 |
| 2026-04-24 | `AREX_ARCH.md` | 新增 Section 26 |

---

## 26. Data Bus 架构：硬件写入接口与 UI 消费任务 (v2026-04-24)

### 26.1 架构铁律

```
硬件工程  ──arex_bus_set_*()──→  g_sensor_data (dirty_mask)
                                          │
                              arex_ui_update_task() (50ms lv_timer)
                                          │
                                按脏标记按需刷新 UI
```

- **硬件工程层**：只能调用 `arex_bus_set_*()` 系列函数。禁止直接写 `g_sensor_data`，禁止包含任何 LVGL 代码头
- **UI 工程层**：只能修改 `arex_ui_update_task()` 消费者函数。禁止绕过消费任务直接操作 LVGL
- **两者通过 `g_sensor_data.dirty_mask` 完全解耦**。

### 26.2 脏标记位枚举

| 位 | 名 | 含义 |
|----|----|------|
| 0 | `DIRTY_DEPTH` | 深度数据 |
| 1 | `DIRTY_NDL` | 免减压时间 |
| 2 | `DIRTY_TTS` | 回到水面时间 |
| 3 | `DIRTY_POD` | 气瓶压力（pod1/pod2） |
| 4 | `DIRTY_BATT` | 电池电量 |
| 5 | `DIRTY_HEADING` | 罗盘航向 |
| 6 | `DIRTY_TIME` | 潜水时间 / W.TIME |
| 7 | `DIRTY_PPO2` | PO2 数据 |
| 8 | `DIRTY_GAS` | 气体切换 |
| 9 | `DIRTY_ALARM` | 告警状态 |
| 10 | `DIRTY_DECO` | 减压站序列 + 站点时间（临界区保护） |
| 11 | `DIRTY_TEMP` | 温度数据 |
| 12 | `DIRTY_DEVICES` | 外设状态（灯、气瓶数量） |
| 13 | `DIRTY_TISSUES` | **16 组织舱饱和度数组**（临界区保护） |
| 14 | `DIRTY_CNS` | CNS 氧中毒百分比 |
| 15 | `DIRTY_OTU` | OTU 氧中毒剂量单位 |

> **临界区铁律**：>`32bit` 的数组拷贝必须包在 `rt_hw_interrupt_disable/enable` 临界区里，防止多线程数据撕裂。PC 仿真器下 `rt_hw_interrupt_*` 宏替换为空操作，真机 RT-Thread 下触发真实关中断。

### 26.3 Data Bus Setter 接口（`arex_data.h / arex_data.c`）

```c
void arex_bus_set_depth(float depth_m);         // 防抖阈值 0.05m
void arex_bus_set_ndl(int16_t ndl_min);
void arex_bus_set_tts(uint16_t tts_min);
void arex_bus_set_pod(uint8_t pod_idx, float bar); // pod_idx: 0=pod1, 1=pod2
void arex_bus_set_battery(float pct);
void arex_bus_set_heading(uint16_t heading_deg);
void arex_bus_set_dive_time(uint32_t dive_s);   // 触发 DIRTY_TIME
void arex_bus_set_surface_time(uint32_t surface_s);
void arex_bus_set_ppo2(uint8_t sensor_idx, float ppo2_val);
void arex_bus_set_gas(uint8_t gas_idx, const char *gas_name);
void arex_bus_set_deco(int16_t stop_m, uint8_t stop_min);
void arex_bus_set_cns(uint8_t cns_pct);
void arex_bus_set_otu(uint16_t otu_val);

/* 临界区保护的数组写入（>32bit，必须包 rt_hw_interrupt_disable/enable） */
void arex_bus_set_tissues(const uint8_t tissue_pct[16]);           // DIRTY_TISSUES
void arex_bus_set_deco_plan(const arex_deco_stop_t *stops, uint8_t count); // DIRTY_DECO

void arex_bus_clear_all_dirty(void);
```

### 26.4 UI 消费任务（`arex_ui_engine.c`）

```c
// 由 lv_timer 驱动，50ms 周期，~20 FPS
void arex_ui_update_task(lv_timer_t *timer)
{
    uint32_t mask = g_sensor_data.dirty_mask;
    if (mask == DIRTY_NONE) return;

    if (mask & (DIRTY_DEPTH | DIRTY_NDL | DIRTY_TTS | DIRTY_TISSUES)) {
        arex_screen_refresh_left_panel();
        card_deco_update();
    }
    if (mask & DIRTY_POD)    { arex_screen_refresh_left_panel(); }
    if (mask & DIRTY_BATT)   { arex_screen_refresh_left_panel(); arex_screen_refresh_system_data(); }
    if (mask & DIRTY_HEADING){ arex_screen_refresh_compass_target(); }
    if (mask & DIRTY_TIME)   { arex_screen_refresh_left_panel(); }
    if (mask & DIRTY_PPO2)   { arex_screen_refresh_left_panel(); }
    if (mask & DIRTY_GAS)    { arex_screen_refresh_gas_menu(); arex_screen_refresh_left_panel(); }
    if (mask & DIRTY_DECO)   { card_plan_update(); }
    if (mask & DIRTY_CNS)    { card_deco_update(); }
    if (mask & DIRTY_OTU)    { card_deco_update(); }
    if (mask & DIRTY_TEMP)   { arex_screen_refresh_system_data(); }
    if (mask & DIRTY_DEVICES){ arex_screen_refresh_system_data(); }

    arex_bus_clear_all_dirty();
}
```

### 26.5 定时器分工

| 定时器 | 周期 | 职责 |
|-------|------|------|
| `sim_tick_cb` | 1Hz | 硬件数据写入，通过 `arex_bus_set_*()` 打脏标记 |
| `arex_ui_update_task` | 50ms | UI 消费任务，按脏标记按需刷新 LVGL |

### 26.6 Placeholder 占位符机制

所有 widget 初始化时显示 `"--"` 占位符，等硬件数据首次推送后替换为真实值。

#### 统一开关

```c
// arex_ui_engine.h
#define AREX_SHOW_PLACEHOLDER_ON_INIT  1  // 1=显示"--"，0=直接显示初始值
```

- 设为 `1`：`render_widget_by_id()` 中所有数值 label 初始化为 `"--"`
- 设为 `0`：`render_widget_by_id()` 中所有数值 label 用 `g_sensor_data` 初始值填充

#### 受控位置（`arex_ui_engine.c`）

| 位置 | widget | 宏控制 |
|------|--------|--------|
| DEPTH 专属分支 | DEPTH | ✅ |
| NDL 专属分支 | NDL | ✅ |
| 通用渲染分支 | TTS/HEADING/SAC/BATTERY/PPO2/CNS/POD1/POD2/WTIME/TEMP | ✅ |

#### 补充铁律：防抖阈值 + 首次强制打脏

带阈值防抖的 Setter（`battery`、`temperature`）首次调用时即便值相同也会打脏标记，保证 placeholder 能被替换：

```c
// arex_data.c — 铁律模板
if (fabsf(g_sensor_data.xxx - val) > threshold
    || (g_sensor_data.dirty_mask & DIRTY_XXX) == 0) {
    g_sensor_data.xxx = val;
    g_sensor_data.dirty_mask |= DIRTY_XXX;
}
```

> 后续新增带阈值的 Setter 时，必须追加 `|| (dirty_mask & DIRTY_XXX) == 0` 条件。

### 26.7 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-24 | `arex_ui_engine.h` | `arex_sensor_data_t` 增加 `dirty_mask`；新增 `arex_dirty_bit_t` 枚举；声明 Setter + Consumer |
| 2026-04-24 | `arex_data.h` | 彻底重构为 Data Bus 接口层头文件 |
| 2026-04-24 | `arex_data.c` | 新建；全部 Setter 实现，含防抖逻辑 |
| 2026-04-24 | `arex_ui_engine.c` | 实现 `arex_ui_update_task()` 集中消费任务 |
| 2026-04-24 | `UI_main.c` | `sim_tick_cb` 全部改用 Setter；撤销直接写入；分离两定时器 |
| 2026-04-24 | `arex_card_registry.h` | 卡片 update forward 声明 |
| 2026-04-24 | `AREX_ARCH.md` | 新增 Section 26 |
| 2026-04-28 | `arex_data.c` | `arex_bus_set_battery/temperature` 加首次强制打脏逻辑，防止 placeholder "--" 无法被替换 |
| 2026-04-28 | `arex_ui_engine.h` | `AREX_SHOW_PLACEHOLDER_ON_INIT` 宏声明 |
| 2026-04-28 | `arex_ui_engine.c` | `render_widget_by_id` 中 DEPTH/NDL/通用渲染分支全部加 `AREX_SHOW_PLACEHOLDER_ON_INIT` 统一控制 |


## 27. 标题区宏统一 + 图表坐标精修 (v2026-04-25)

### 27.1 问题描述

**问题 1：标题区高度魔法数字散落**
- `arex_screen_make_card_title()` 中标题文字 Y=12，分割线 Y=38，文字高度28均为硬编码
- `card_info.c` / `card_setup.c` 中列表起始 Y=50
- `arex_ui_engine.c` 中 5F 网格分割线 Y=38
- 子菜单标题 Y=12，列表 Y=50
- `card_plan.c` 中图表 Y=50

**问题 2：图表坐标轴带冗余单位**
- Y 轴数字如 `10m`、`20m` 带 `m` 后缀，网格拥挤凌乱
- X 轴数字如 `10s`、`2min`、`2.5min` 带 `s`/`min` 后缀

### 27.2 修复方案

**统一宏 `AREX_CARD_TITLE_H`**（定义在 `arex_ui_engine.h`，值为 40px）：

```
标题文字: Y = 8px
标题高度: AREX_CARD_TITLE_H - 10 = 30px
分割线:   Y = AREX_CARD_TITLE_H - 2 = 38px
```

**受影响文件/位置**：

| 文件 | 变更 |
|------|------|
| `arex_ui_engine.h` | 注释更新为"右侧卡片全局标题区高度分配" |
| `arex_screen.c` | `arex_screen_make_card_title()` 标题 Y=8, h=30, 分割线 Y=`AREX_CARD_TITLE_H-2` |
| `arex_screen.c` | 子菜单标题 Y=8, 分割线 Y=`AREX_CARD_TITLE_H-2`, 列表 Y=`AREX_CARD_TITLE_H` |
| `card_info.c` | 列表起始 Y=`AREX_CARD_TITLE_H`（原 50） |
| `card_setup.c` | 列表起始 Y=`AREX_CARD_TITLE_H`（原 50） |
| `card_compass.c` | Canvas Y=`AREX_CARD_TITLE_H`（原 50） |
| `arex_ui_engine.c` | 5F 网格标题文字 Y=8, h=`AREX_CARD_TITLE_H-10`, 分割线 Y=`AREX_CARD_TITLE_H-2`（原 38）；更新注释 |
| `card_plan.c` | `CHART_Y` 改为 `AREX_CARD_TITLE_H`（原 50） |

**图表坐标轴纯数字化**：

| 位置 | 变更 |
|------|------|
| Y 轴标签 | `"%dm"` → `"%d"`（纯数字） |
| X 轴标签 (<120s) | `"%ds"` → `"%d"` |
| X 轴标签 (>=120s) | `"%dmin"` / `"%.1fmin"` → `"%d"` / `"%.1f"` |

**新增左下角统一单位标识**：

在 X/Y 轴网格绘制完毕后，在图表原点位置绘制 `m/min` 单位文字（AREX_LIGHT, opa=191）。

### 27.3 关键代码变更

**`arex_screen_make_card_title()`**:

```c
lv_obj_set_pos(lbl, 16, 8);                    // 原 Y=12
lv_obj_set_size(lbl, right_w - 32, AREX_CARD_TITLE_H - 10);  // 原 28
lv_obj_set_pos(line, 16, AREX_CARD_TITLE_H - 2);  // 原 38
```

**`card_plan.c` 图表坐标轴**:

```c
// Y 轴: 纯数字
snprintf(buf, sizeof(buf), "%d", d);

// X 轴: 纯数字
if (max_t_axis_sec >= 120.0f) {
    if (t % 60 == 0) snprintf(buf, sizeof(buf), "%d", t / 60);
    else snprintf(buf, sizeof(buf), "%.1f", (float)t / 60.0f);
} else {
    snprintf(buf, sizeof(buf), "%d", t);
}

// 左下角单位标识
lv_draw_label_dsc_t unit_dsc;
lv_draw_label_dsc_init(&unit_dsc);
unit_dsc.font = arex_get_font(AREX_FONT_ID_SMALL);
unit_dsc.color = AREX_LIGHT;
unit_dsc.opa = 191;
lv_area_t unit_area = {area->x1 + 2, area->y2 - 14, area->x1 + 60, area->y2 + 10};
lv_draw_label(draw_ctx, &unit_dsc, &unit_area, "m/min", NULL);
```

### 27.4 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-25 | `arex_ui_engine.h` | `AREX_CARD_TITLE_H` 注释更新 |
| 2026-04-25 | `arex_screen.c` | 标题区全部 Y 值改用宏 |
| 2026-04-25 | `card_info.c` | 列表 Y 从 50 改为 `AREX_CARD_TITLE_H` |
| 2026-04-25 | `card_setup.c` | 列表 Y 从 50 改为 `AREX_CARD_TITLE_H` |
| 2026-04-25 | `card_compass.c` | Canvas Y 从 50 改为 `AREX_CARD_TITLE_H` |
| 2026-04-25 | `arex_ui_engine.c` | 5F 网格分割线 Y 改用宏 |
| 2026-04-25 | `card_plan.c` | `CHART_Y` 改用宏；Y/X 轴标签去单位；新增 `m/min` 标识 |
| 2026-04-25 | `card_deco.c` | 使用相对偏移（y+16/y+40），无需修改 |
| 2026-04-25 | `card_gas.c` | 使用相对偏移，无需修改 |
| 2026-04-25 | `AREX_ARCH.md` | 新增 Section 27 |

---

## 28. 4U 标题高度规范与全组件自适应降维 (v2026-04-26)

### 28.1 核心设计原则

**"彻底分离 + 高度自适应"**：
- 右侧卡片区域的标题高度固定为 **4U (40px)**，由 `AREX_CARD_TITLE_H` 宏统一定义
- 标题区域作为一个独立的盒子，它占用 40px，下方内容区自动捡剩余空间
- 所有组件（菜单、网格、图表）的 Y=0 起点统一为 `AREX_CARD_TITLE_H`
- 绝不允许任何硬编码的魔法数字 Y 坐标

### 28.2 标题区宏定义

```c
// arex_ui_engine.h
#define AREX_CARD_TITLE_H  40   /* 4U 标题高度：文字(5px) + 分割线(2px) + 留白(33px) = 40px */
```

**内部布局**：
```
标题区 (0 ~ 40px):
  Y=5:      绿色标题文字
  Y=38:      2px 分割线 (AREX_DARK 色)
内容区 (40px ~ 400px):
  Y=40:      内容区 Y=0 起点（各组件自适应）
```

### 28.3 通用卡片标题渲染器

```c
// arex_ui_engine.h 声明
void arex_render_card_title(lv_obj_t *parent_card, const char *title_text);

// arex_ui_engine.c 实现
void arex_render_card_title(lv_obj_t *parent_card, const char *title_text)
{
    uint16_t right_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                      - ((int)g_sys_config.gap_u * AREX_BASE_U);

    // 标题文字: Y=5, h=30, AREX_LIGHT 色
    lv_obj_t *lbl = lv_label_create(parent_card);
    lv_obj_set_pos(lbl, 16, 5);
    lv_obj_set_size(lbl, right_w - 32, AREX_CARD_TITLE_H - 10);
    lv_label_set_text(lbl, title_text);

    // 分割线: Y=38 (AREX_CARD_TITLE_H - 2), 2px, AREX_DARK 色
    lv_obj_t *line = lv_obj_create(parent_card);
    lv_obj_set_size(line, right_w - 32, 2);
    lv_obj_set_pos(line, 16, AREX_CARD_TITLE_H - 2);
    lv_obj_set_style_bg_color(line, AREX_DARK, 0);
}
```

### 28.4 全组件自适应公式

#### 5F 自定义网格 (动态 cell_h)

```c
void arex_calc_widget_grid(...)
{
    // 锁定 5 列，动态计算 cell_h
    uint16_t cell_w = parent_w / 5;
    uint16_t cell_h = (parent_h > AREX_CARD_TITLE_H)
                      ? ((parent_h - AREX_CARD_TITLE_H) / 6)
                      : 60;  // fallback

    *out_x = col * cell_w;
    *out_y = AREX_CARD_TITLE_H + row * cell_h;  // 标题避让偏移
    *out_w = span_w * cell_w;
    *out_h = span_h * cell_h;
}
```

数学验证（tile=400px）：
- `cell_h = (400 - 40) / 6 = 60px`
- Row 0: `out_y = 40 + 0*60 = 40px` → 超过标题线 (38px)，落入内容区
- Row 5: `out_y = 40 + 5*60 = 340px`，底线 `340 + 60 = 400px` → 刚好填满

如果标题改为 60px：
- `cell_h = (400 - 60) / 6 = 56px`
- 网格自动重新切分，无需修改任何组件代码

#### 动态菜单

```c
// 内容区 Y=0 = AREX_CARD_TITLE_H
lv_obj_set_pos(menu_list, 0, AREX_CARD_TITLE_H);
// 菜单内部 start_y=0，从标题下方开始排列
arex_render_dynamic_menu(list, items, count, 0, NULL);
```

#### 4F 图表

```c
int tile_h  = g_sys_config.safe_zone_h;          // 400px
int chart_y = AREX_CARD_TITLE_H;                  // 40px
int chart_h = tile_h - AREX_CARD_TITLE_H - PAD; // 剩余空间自适应
int chart_w = right_w - PAD * 2;                // 宽度自适应
```

#### 2F 组织图

```c
#define DECO_CONTENT_Y  (AREX_CARD_TITLE_H + 20)
#define DECO_ROW2_Y    (AREX_CARD_TITLE_H + 67)
#define DECO_ROW3_Y    (AREX_CARD_TITLE_H + 114)

int bars_y    = DECO_ROW3_Y + 40;                          // 动态定位
int bar_max_h = g_sys_config.safe_zone_h - bars_y - 30;   // 自适应剩余高度
```

### 28.5 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-26 | `arex_ui_engine.h` | 新增 `arex_render_card_title()` 声明 |
| 2026-04-26 | `arex_ui_engine.c` | 实现 `arex_render_card_title()`；重构 `arex_calc_widget_grid()` 动态 cell_h |
| 2026-04-26 | `arex_ui_engine.c` | `arex_render_5f_custom_grid()` 改用 `arex_render_card_title()` |
| 2026-04-26 | `card_info.c` | 改用 `arex_render_card_title()` |
| 2026-04-26 | `card_setup.c` | 改用 `arex_render_card_title()` |
| 2026-04-26 | `card_compass.c` | 改用 `arex_render_card_title()` |
| 2026-04-26 | `card_deco.c` | 改用 `arex_render_card_title()`；新增 `DECO_CONTENT_Y/ROW2/ROW3` 宏；bars 动态高度 |
| 2026-04-26 | `card_gas.c` | 改用 `arex_render_card_title()`；气体行 Y 改用 `AREX_CARD_TITLE_H` 基准 |
| 2026-04-26 | `card_plan.c` | 移除 `CHART_W/H/X/Y` 硬编码；改用 `arex_render_card_title()`；chart_w/h 动态计算；`plan_chart_draw_cb` 使用 `area->coords` 实际尺寸 |
| 2026-04-26 | `AREX_ARCH.md` | 新增 Section 28 |
---

## 29. 左侧 2x7 绝对网格布局重构 (v2026-05-01)

### 29.1 核心设计原则

**彻底废弃 `current_y` 累加排版，改为 `x*y*w*h` 纯数学坐标推演**

左侧 160x420 区域被严格划分为 **2 (80px) x 7 (60px)** 的绝对网格矩阵，彻底与右侧 `arex_widget_id_t` 体系共享同一数据源。底部 60px SystemData 独立渲染（电池/温度/设备图标），不参与网格。

```
Grid Layout:
  Row 0:  NDL           |           (2x1 -> 160x60)
  Row 1-2: DEPTH (x2)  |           (2x2 -> 160x120, 含速度小图标)
  Row 3:  POD1         | POD2     (1x1 -> 80x60, 双拼)
  Row 4:  TIME          |           (2x1 -> 160x60, 潜水时间 MM:SS)
  Row 5:  GAS           |           (2x1 -> 160x60, 当前气体名称)
  Row 6:  SYS           |           (2x1 -> 160x60, 系统时间，可配置)
  [底部 60px SystemData 独立渲染：电池/温度/设备图标]
```

### 29.2 新增数据结构

**`arex_widget_id_t` 枚举扩展**（`arex_ui_engine.h`）：

```c
typedef enum {
    AREX_WIDGET_EMPTY      = 0,   /* 空槽位 */
    AREX_WIDGET_DEPTH      = 1,   /* DEPTH 深度 — 数据源: g_sensor_data.depth */
    AREX_WIDGET_TEMP       = 2,   /* TEMP 水温 — 数据源: g_sensor_data.temp */
    AREX_WIDGET_HEADING    = 3,   /* HEADING 航向 — 数据源: g_sensor_data.heading */
    AREX_WIDGET_SAC_RATE   = 4,   /* SAC 呼吸速率 — 数据源: g_sensor_data.sac_rate */
    AREX_WIDGET_BATTERY    = 5,   /* BATTERY 电池 — 数据源: g_sensor_data.battery_pct */
    AREX_WIDGET_NDL        = 6,   /* NDL 免减压 — 数据源: g_sensor_data.ndl */
    AREX_WIDGET_TTS        = 7,   /* TTS 回到水面 — 数据源: g_sensor_data.tts */
    AREX_WIDGET_PPO2       = 8,   /* PPO2 — 数据源: g_sensor_data.ppo2[active_gas] */
    AREX_WIDGET_CNS        = 9,   /* CNS — 数据源: g_sensor_data.cns_pct */
    AREX_WIDGET_POD1       = 10,  /* POD1 气瓶1 — 数据源: g_sensor_data.pod1_bar */
    AREX_WIDGET_POD2       = 11,  /* POD2 气瓶2 — 数据源: g_sensor_data.pod2_bar */
    AREX_WIDGET_WTIME      = 12,  /* TIME 潜水总时 — 数据源: g_sensor_data.dive_time_s */
    AREX_WIDGET_GAS        = 13,  /* GAS 当前气体 — 数据源: g_sensor_data.gas_name */
    AREX_WIDGET_COUNT
} arex_widget_id_t;
```

**`arex_left_widget_t`**（定义于 `arex_ui_engine.h`）：

```c
#define AREX_LEFT_COLS   2
#define AREX_LEFT_ROWS   7
#define AREX_LEFT_CELL_W 80   /* 80px */
#define AREX_LEFT_CELL_H 60   /* 60px */
#define AREX_LEFT_GRID_W (AREX_LEFT_COLS * AREX_LEFT_CELL_W)  /* 160px */
#define AREX_LEFT_GRID_H (AREX_LEFT_ROWS * AREX_LEFT_CELL_H)  /* 420px */

/* APP 下发数据结构（只含位置，3字节） */
typedef struct {
    arex_widget_id_t widget_id;  /* 组件类型 ID（枚举） */
    uint8_t x;                   /* 列索引 0~1 */
    uint8_t y;                   /* 行索引 0~6 */
} arex_left_widget_t;

/* 左侧网格组件数组声明（最多 14 个组件覆盖 2x7 网格） */
#define AREX_LEFT_MAX_WIDGETS 14
extern arex_left_widget_t g_left_widgets[AREX_LEFT_MAX_WIDGETS];
extern uint8_t g_left_widget_count;
```

> **架构铁律**：APP 只下发 `widget_id + x + y` 三字段（3 字节），不包含跨度信息。**跨度（span_w/span_h）由 MCU 根据 `widget_id` 从 `arex_get_widget_style()` 样式表自动查表获取**，彻底消除数据冗余。

### 29.3 默认网格配置

**`arex_ui_engine.c` 中 `arex_sys_config_defaults()` 填充**：

```c
g_left_widget_count = 7;
g_left_widgets[0] = (arex_left_widget_t){ WIDGET_NDL_STOP_1606,  0, 0 };
g_left_widgets[1] = (arex_left_widget_t){ WIDGET_DEPTH_1612,    0, 1 };
g_left_widgets[2] = (arex_left_widget_t){ WIDGET_POD_0806,      0, 3 };
g_left_widgets[3] = (arex_left_widget_t){ WIDGET_POD_0806,      1, 3 };
g_left_widgets[4] = (arex_left_widget_t){ WIDGET_WTIME_0806,    0, 4 };
g_left_widgets[5] = (arex_left_widget_t){ WIDGET_GAS_1606,     0, 5 };
g_left_widgets[6] = (arex_left_widget_t){ WIDGET_SYS_1606,     0, 6 };
```

### 29.4 网格渲染引擎

**`arex_render_left_anchor_grid(lv_obj_t *left_anchor)`**（`arex_ui_engine.c`）：

```
MCU 渲染流程（统一从样式表查表获取跨度）：
  1. 从 arex_get_widget_style(widget_id) 查表获取 span_w/span_h
  2. 纯数学 cell_w * cell_h 坐标推演：
     abs_x = cfg->x * 80
     abs_y = cfg->y * 60
     abs_w = span_w * 80
     abs_h = span_h * 60

内部调用 render_widget_by_id(parent, w_id, abs_x, abs_y, abs_w, abs_h, span_w, span_h, is_depth_icon, (arex_font_id_t)255)
```

### 29.5 通用组件工厂 `render_widget_by_id()`

**左侧网格 + 5F 网格共用**（原 `create_custom_widget()` 重命名）：

```c
lv_obj_t *render_widget_by_id(lv_obj_t *parent,
                               arex_widget_id_t w_id,
                               int16_t abs_x, int16_t abs_y,
                               uint16_t abs_w, uint16_t abs_h,
                               uint8_t span_w, uint8_t span_h,
                               bool is_depth_icon,
                               arex_font_id_t cfg_font_id)
```

> `cfg_font_id` 为强制字号覆盖参数：传 `(arex_font_id_t)255` 表示不覆盖，由函数内根据 `span_w/span_h` 自动推导。

**字号自适应**（已内联到函数内）：

| 跨度条件 | 选用字体 |
|----------|----------|
| `span_w>=2 && span_h>=2` | `AREX_FONT_ID_HUGE` (48px) |
| `span_w>=2` | `AREX_FONT_ID_MEDIUM` (28px) |
| 其他 | `AREX_FONT_ID_SMALL` (14px) |

**DEPTH 特殊处理**：`is_depth_icon==true` 时挂载速度小图标（`LV_IMG_DECLARE(sudu)`）。

**流水线架构（三段式）**：

```
阶段一：专属早期分支（立即返回）
  ├─ WIDGET_DEPTH_1612 (2x2)：int_lbl + dec_lbl + unit_lbl + sudu_icon
  └─ WIDGET_NDL (2x1)：vert_bar + horiz_bar + main_val + title_top + sub_bot

阶段二：通用掩码流水线（ELEM_* 开关驱动）
  ├─ ELEM_TITLE  → 创建标题 label，按 title_offset 定位
  ├─ ELEM_VALUE  → 创建数值 label，数据源 switch 分发各 widget
  ├─ ELEM_UNIT   → 创建单位 label（NULL 单位自动跳过）
  ├─ ELEM_BAR    → DEPTH(icon) / ASCENT_0812(icon) / COMPASS(tape) / TISSUE(chart) / SYS(battery)
  └─ ELEM_EXTRA  → POD(POD1/POD2 专属 ID 标签)

阶段三：返回容器句柄（供 update 循环使用）
```

**元素开关掩码示例（严格对照 PRD 约束表）**：

| Widget ID | elements 值 | PRD 约束 |
|-----------|------------|---------|
| `WIDGET_DEPTH_1612` | `ELEM_VALUE \| ELEM_UNIT \| ELEM_BAR` | 无 title，靠 spec.depth 做 int/dec/unit 分离 |
| `WIDGET_DEPTH_1606` | `ELEM_TITLE \| ELEM_VALUE \| ELEM_UNIT` | 有 title，无 bar |
| `WIDGET_NDL_STOP_1606` | `ELEM_TITLE \| ELEM_VALUE \| ELEM_UNIT \| ELEM_BAR` | 有 bar（多形态进度条） |
| `WIDGET_DIVE_TIME_1606` | `ELEM_TITLE \| ELEM_VALUE \| ELEM_BAR` | 有 title，无 unit |
| `WIDGET_GAS_1606` | `ELEM_TITLE \| ELEM_VALUE` | 有 title，无 unit |
| `WIDGET_SYS_1606` | `ELEM_VALUE \| ELEM_BAR` | 无 title，电池条走 ELEM_BAR |
| `WIDGET_ASCENT_0806` | `ELEM_TITLE \| ELEM_VALUE \| ELEM_UNIT` | 有 title/unit，无 bar |
| `WIDGET_ASCENT_0812` | `ELEM_TITLE \| ELEM_VALUE \| ELEM_UNIT \| ELEM_BAR` | 1x2 带 sudu 速率箭头图标 |
| `WIDGET_COMPASS_1612` | `ELEM_VALUE \| ELEM_BAR` | 无 title，靠 spec.compass 做 tape/val 分离 |
| `WIDGET_SURF_GF_0806` | `ELEM_TITLE \| ELEM_VALUE` | 有 title，无 unit |
| `WIDGET_GF99_0806` | `ELEM_TITLE \| ELEM_VALUE \| ELEM_UNIT` | 有 title + unit |
| `WIDGET_TISSUE_GF_4012` | `ELEM_TITLE \| ELEM_BAR` | title(Med) + tissue chart |
| `WIDGET_POD_0806` | `ELEM_TITLE \| ELEM_VALUE \| ELEM_UNIT \| ELEM_EXTRA` | ELEM_EXTRA → POD1/POD2 ID 标签 |
| `WIDGET_TEMP_0806` | `ELEM_TITLE \| ELEM_VALUE \| ELEM_UNIT` | 常规小块，无 bar |

### 29.6 数据更新函数

**`arex_widget_set_value(arex_widget_id_t id, float value)`**：

双向容器搜索，同时搜索 5F 卡片和左侧锚点，命中即更新 label。

**格式化规则**（按 widget_id 分支）：

| Widget ID | 格式化 | 示例 |
|-----------|--------|------|
| `DEPTH` / `TEMP` | `%.1f` | `45.2` |
| `PPO2` | `%.2f` | `1.20` |
| `POD1` / `POD2` | `%.0f` | `210` |
| `WTIME` | `%02d:%02d` | `05:32` |
| `BATTERY` | `%.0f%%` | `85%` |
| `TTS` / `NDL` | `%d` | `24` |
| 其他 | `%.0f` | - |

**`arex_widget_set_text(arex_widget_id_t id, const char *text)`**：

用于非数值组件（如 GAS 气体名称）。同样双向搜索容器，命中即更新 label 文字。

### 29.7 左侧面板刷新 — 数据源自动推导

**`arex_screen_refresh_left_panel()`（`arex_screen.c`）**：

铁律：只读 `g_left_widgets[]` 数组，根据 `widget_id` 路由到对应的 `g_sensor_data` 字段。**每次修改布局后此函数自动同步，无需手动维护。**

```c
void arex_screen_refresh_left_panel(void)
{
    for (uint8_t i = 0; i < g_left_widget_count; i++) {
        arex_widget_id_t w_id = g_left_widgets[i].widget_id;
        switch (w_id) {
            case WIDGET_NDL_STOP_1606:
                arex_widget_set_value(WIDGET_NDL_STOP_1606, (float)g_sensor_data.ndl); break;
            case WIDGET_DEPTH_1612:
                arex_widget_set_value(WIDGET_DEPTH_1612, g_sensor_data.depth); break;
            case WIDGET_WTIME_0806:
                arex_widget_set_value(WIDGET_WTIME_0806, (float)g_sensor_data.dive_time_s); break;
            case WIDGET_GAS_1606:
                arex_widget_set_text(WIDGET_GAS_1606, g_sensor_data.gas_name); break;
            case WIDGET_SYS_1606:
                arex_widget_set_text(WIDGET_SYS_1606, g_sensor_data.sys_time_str); break;
            case WIDGET_POD_0806:
                arex_widget_set_value(WIDGET_POD_0806, g_sensor_data.pod1_bar); break;
            // ... 其他 widget 同样处理
            default: break;
        }
    }
}
```

### 29.8 屏幕层变更

**双向容器搜索**：同时搜索 5F 卡片和左侧锚点，命中即更新 label。

```c
lv_obj_t *containers[2] = { g_card_custom_obj, g_left_anchor_obj };
for (c = 0; c < 2; c++) {
    // 遍历所有子节点，user_data 匹配 widget_id
    // 找到后遍历子-label，格式化并更新文本
}
```

**格式化规则**（按 widget_id 分支）：

| Widget ID | 格式化 | 示例 |
|-----------|--------|------|
| `DEPTH` / `TEMP` | `%.1f` | `45.2` |
| `PPO2` | `%.2f` | `1.20` |
| `POD1` / `POD2` | `%.0f` | `210` |
| `WTIME` | `%02d:%02d` | `05:32` |
| `BATTERY` | `%.0f%%` | `85%` |
| `TTS` / `NDL` | `%d` | `24` |
| 其他 | `%.0f` | - |

### 29.7 屏幕层变更

**`arex_screen.c` 中 `left_anchor_create()` 与 `left_anchor_rebuild()` 全面重构**：

* `left_anchor_create()`：保留容器创建，删除所有组件创建循环。
* `left_anchor_rebuild()`：清空容器 -> 调用 `arex_render_left_anchor_grid()` -> 调用 `arex_render_system_data()`。

**旧版 `arex_calc_anchor_layout()` + 累加 `current_y` 方式已废弃**，不再创建 `title_zone` / `val_zone` / `sep` 句柄数组。

### 29.8 APP 同步协议

```json
{
  "left_widgets": [
    { "widget_id": 6,  "x": 0, "y": 0, "w": 2, "h": 1 },
    { "widget_id": 1,  "x": 0, "y": 1, "w": 2, "h": 2 },
    { "widget_id": 12, "x": 0, "y": 3, "w": 2, "h": 1 },
    { "widget_id": 13, "x": 0, "y": 4, "w": 2, "h": 1 },
    { "widget_id": 10, "x": 0, "y": 5, "w": 1, "h": 1 },
    { "widget_id": 11, "x": 1, "y": 5, "w": 1, "h": 1 }
  ]
}
```

### 29.9 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-28 | `arex_ui_engine.h` | 新增 `AREX_LEFT_COLS/ROWS/CELL_W/CELL_H/GRID_W/GRID_H` 宏；新增 `arex_custom_widget_cfg_t` 结构体；新增 `arex_render_left_anchor_grid()` 与 `render_widget_by_id()` 声明 |
| 2026-04-28 | `arex_ui_engine.c` | 全局 `g_left_widgets[]` / `g_left_widget_count` 定义；`arex_sys_config_defaults()` 填充默认网格配置；`arex_render_left_anchor_grid()` 实现；`render_widget_by_id()` 重写（含字号自适应内联）；`arex_widget_set_value()` 改为双容器搜索；新增 `arex_widget_set_text()` 字符串刷新；新增 `arex_render_system_data()` 底部 60px SystemData 渲染 |
| 2026-04-28 | `arex_screen.c` | `left_anchor_create()` 删除组件创建循环；`left_anchor_rebuild()` 简化为清空 + 调用两个渲染函数；`arex_screen_refresh_left_panel()` 改为遍历 `g_left_widgets[]` 自动推导数据源；删除 `s_anchor_titles[]` / `s_anchor_vals[]` / `s_anchor_seps[]` / `s_anchor_mod_seps[]` 相关逻辑 |
| 2026-04-28 | `AREX_ARCH.md` | 新增 Section 29 |
| 2026-04-28 | `arex_ui_engine.h` | `AREX_WIDGET_GAS = 13` 新增；`AREX_WIDGET_WTIME` 注释改为 TIME，数据源改为 `dive_time_s` |
| 2026-04-28 | `arex_ui_engine.c` | `s_widget_meta[]` 新增 GAS 条目；WTIME title 改为 "TIME" |
| 2026-04-28 | `arex_screen.c` | `arex_screen_refresh_left_panel()` 重构为 switch-case 路由，遍历 `g_left_widgets[]` 自动推导数据源；移除 TTS；新增 GAS 字符串刷新 |
| 2026-04-28 | `arex_ui_engine.c` | `render_widget_by_id()` 新增三大改进：(1) `LV_OBJ_FLAG_SCROLLABLE` + `LV_SCROLLBAR_MODE_OFF` 封杀滚动条；(2) `AREX_WIDGET_DEPTH` 专属渲染：整数(HUGE)+小数(MEDIUM)+单位(SMALL)+箭头四层分离排版；(3) `AREX_WIDGET_NDL` 专属渲染：电池型 Bar(左)+巨型数值(中)+NDL标签(右)横向布局 |
| 2026-05-01 | `arex_data.h` | `ble_sync_left_widget_t` 删除 `w/h` 字段；BLE 帧总大小由 172 字节缩减为 148 字节 |
| 2026-05-01 v2 | `arex_ui_engine.h` | `arex_left_widget_t` 移除 `w/h` 字段，统一由 `arex_get_widget_style()` 查表获取 |
| 2026-05-01 v2 | `arex_ui_engine.c` | `arex_render_left_anchor_grid()` 改为从样式表查表获取 span_w/span_h |
| 2026-05-01 | `arex_ui_engine.h` | `arex_left_widget_t` 移除 `font_id` 字段（与 protobuf 侧对齐）；`render_widget_by_id()` 函数签名增加 `arex_font_id_t cfg_font_id` 参数 |
| 2026-05-01 | `arex_ui_engine.c` | `g_left_widgets[]` 初始化简化为 5 字段；`arex_render_5f_custom_grid()` 调用 `render_widget_by_id()` 删除多余参数 |
| 2026-05-01 | `arex_data.c` | `arex_bus_set_ui_layout()` 删除 `font_id` 赋值行 |
| 2026-05-01 | `UI_main.c` | `arex_test_set_ui_layout()` 测试函数中 `left_def`/`left_rev` 数组移除 `font_id` 列（6→5 字段），对应 `ble_sync_left_widget_t` 结构体对齐 |
| 2026-05-01 | `arex_ui_engine.h` | 新增 `ELEM_TITLE|ELEM_VALUE|ELEM_UNIT|ELEM_BAR|ELEM_EXTRA` 8-bit 元素开关掩码宏（ELEM_EXTRA 用于 POD 专属 ID 标签）；`arex_widget_style_t` 结构体新增 `uint8_t elements` 字段 |
| 2026-05-01 | `arex_ui_engine.c` | `g_widget_styles[]` 全部 30+ 组件条目新增 `.elements` 字段（严格按 PRD 约束表按需勾选）；`render_widget_by_id()` 重构为三段式流水线：专属早期分支（DEPTH/NDL）+ 通用掩码装配（ELEM_TITLE→ELEM_VALUE→ELEM_UNIT→ELEM_BAR→ELEM_EXTRA）+ 早期返回；删除废弃的旧通用渲染分支代码 |

---

## 30. 硬件移植指南 (v2026-04-28)

### 30.1 平台兼容层 — 两行代码切换真机/仿真

所有跨平台代码集中在 `arex_data.h` 顶部：

```c
// =========================================================
// 平台兼容层 — 必须在所有 include 之前
//
// 真机 (RT-Thread): 使用 rt_hw_interrupt_disable/enable 临界区
// PC 仿真器:        替换为空操作，防止编译报错
// =========================================================
#define PC_SIMULATOR  //移植硬件后需要注释此行！！！
#ifdef PC_SIMULATOR
    typedef int rt_base_t;
    #define rt_hw_interrupt_disable()   ((rt_base_t)0)  //假代码
    #define rt_hw_interrupt_enable(lvl) ((void)(lvl))   //假代码
#else
    #include <rtthread.h>
#endif
```

**移植步骤**：将 `arex_data.h` 中 `#define PC_SIMULATOR` 整行注释或删除，然后引入 `#include <rtthread.h>`，即可切换到真机模式。`arex_data.c` 和所有调用方**无需修改任何代码**，临界区自动启用。

---

### 30.2 硬件工程师的 API — 只能调用这 18 个函数

硬件工程师（传感器驱动、潜水算法）**绝对禁止**直接写入 `g_sensor_data`，**绝对禁止**调用任何 LVGL 函数。唯一合法入口是以下 `arex_bus_set_*()` 系列：

| 函数 | 参数 | 触发脏标记 | 备注 |
|------|------|-----------|------|
| `arex_bus_set_depth(float)` | 深度 m | `DIRTY_DEPTH \| DIRTY_DECO` | 内置 0.05m 防抖 |
| `arex_bus_set_ndl(int16_t)` | NDL 分钟 | `DIRTY_NDL` | — |
| `arex_bus_set_tts(uint16_t)` | TTS 分钟 | `DIRTY_TTS` | — |
| `arex_bus_set_pod(uint8_t idx, float)` | idx=0/1, 气压 bar | `DIRTY_POD` | — |
| `arex_bus_set_battery(float)` | 百分比 | `DIRTY_BATT` | 内置 0.1f 防抖 |
| `arex_bus_set_heading(uint16_t)` | 航向 0~359° | `DIRTY_HEADING` | — |
| `arex_bus_set_dive_time(uint32_t)` | 潜水秒数 | `DIRTY_TIME` | — |
| `arex_bus_set_surface_time(uint32_t)` | 水面休息秒数 | `DIRTY_TIME` | — |
| `arex_bus_set_ppo2(uint8_t idx, float)` | idx=0~2, PO2 值 | `DIRTY_PPO2` | — |
| `arex_bus_set_gas(uint8_t, const char*)` | 气体索引+名称 | `DIRTY_GAS` | — |
| `arex_bus_set_deco(int16_t, uint8_t)` | 下一站深度/分钟 | `DIRTY_DECO` | — |
| `arex_bus_set_cns(uint8_t)` | CNS 百分比 | `DIRTY_CNS` | — |
| `arex_bus_set_otu(uint16_t)` | OTU 值 | `DIRTY_OTU` | — |
| `arex_bus_set_tissues(const uint8_t[16])` | 16 隔室饱和度数组 | `DIRTY_TISSUES` | **临界区保护** |
| `arex_bus_set_deco_plan(const arex_deco_stop_t*, uint8_t)` | 减压站序列 | `DIRTY_DECO` | **临界区保护** |
| `arex_bus_set_temperature(float)` | 温度 °C | `DIRTY_TEMP` | 内置 0.1f 防抖 |
| `arex_bus_set_device_status(bool, bool, uint8_t)` | 频闪/手电/气瓶数 | `DIRTY_DEVICES` | — |
| `arex_bus_clear_all_dirty(void)` | — | 清零 dirty_mask | 仅由 UI 层调用 |

**数据流铁律**：

```
硬件传感器/潜水算法 ──arex_bus_set_*()──▶ g_sensor_data (dirty_mask)
                                              │
                              arex_ui_update_task() (50ms lv_timer)
                                              │
                                    按脏标记按需刷新 LVGL UI
```

---

### 30.3 临界区保护 — 为什么数组写入需要包关中断

`tissue_pct[16]` 和 `deco_stops[]` 每次写入 16~320 字节，超过单条汇编指令的 32bit 原子边界。在多线程/RTOS 环境下，不包临界区会导致 UI 消费任务读到撕裂数据（一半新一半旧）。

```c
void arex_bus_set_tissues(const uint8_t tissue_pct[16])
{
    rt_base_t level = rt_hw_interrupt_disable();  // 真机：关中断；仿真：无操作
    memcpy(g_sensor_data.tissue_pct, tissue_pct, 16);
    g_sensor_data.dirty_mask |= DIRTY_TISSUES;
    rt_hw_interrupt_enable(level);               // 真机：开中断；仿真：无操作
}
```

---

### 30.4 两定时器架构 — 生产者与消费者彻底分离

真机上有两个定时器，各自独立，**绝不互相调用**：

| 定时器 | 来源 | 周期 | 职责 | 唯一合法操作 |
|--------|------|------|------|-------------|
| 硬件数据写入 | RT-Thread 定时器线程 / 传感器 DMA 中断 | **1Hz** | 读取传感器，计算潜水算法，推送数据 | 调用 `arex_bus_set_*()` |
| UI 消费任务 | LVGL `lv_timer_create` | **50ms（20FPS）** | 按 dirty_mask 刷新 LVGL 界面 | 调用 `lv_label_set_text()` 等 |

```
┌─────────────────┐       arex_bus_set_*()        ┌─────────────────┐
│  硬件传感器 ISR   │ ─────────────────────────▶ │  g_sensor_data  │
│  1Hz 定时器线程  │        (写数据+打脏标记)    │   (RAM 数据总线) │
└─────────────────┘                               └────────┬────────┘
                                                           │ dirty_mask
                                                           ▼
                                                  ┌─────────────────┐
                                                  │ arex_ui_update_  │
                                                  │ task() lv_timer  │
                                                  │ 50ms            │
                                                  └────────┬────────┘
                                                           │ lv_label_set_text()
                                                           ▼
                                                  ┌─────────────────┐
                                                  │   LVGL 界面     │
                                                  │   (绝不反向触碰)  │
                                                  └─────────────────┘
```

---

### 30.5 移植检查清单

**Step 1：切换平台宏**（`arex_data.h`）
- [ ] 注释掉 `#define PC_SIMULATOR`
- [ ] 确认 `#include <rtthread.h>` 正常引入

**Step 2：确保 rtthread 环境提供以下符号**
- [ ] `rt_base_t` 类型定义
- [ ] `rt_hw_interrupt_disable()` 函数
- [ ] `rt_hw_interrupt_enable(rt_base_t)` 函数

**Step 3：挂接传感器驱动**
- [ ] 在传感器数据就绪的 ISR 或 DMA callback 中，调用对应的 `arex_bus_set_*()`
- [ ] 组织饱和度（16 隔室）数组写入 `arex_bus_set_tissues()`
- [ ] 减压站序列写入 `arex_bus_set_deco_plan()`

**Step 4：挂接 1Hz 定时器**（可选，传感器可自行触发写入）
- [ ] 创建 RT-Thread 定时器，周期 1000ms
- [ ] 定时器回调中调用 `arex_bus_set_dive_time()`、`arex_bus_set_depth()` 等

**Step 5：确认 UI 消费任务正常运行**
- [ ] `lv_timer_create(arex_ui_update_task, 50, NULL)` 在 `UI_main()` 中已启动（已实现）
- [ ] `AREX_SHOW_PLACEHOLDER_ON_INIT` 行为符合预期（上电显示 "--" 等传感器数据）

**Step 6：验证临界区**
- [ ] 用示波器/调试器确认 `tissue_pct[]` 读取时无撕裂
- [ ] 确认 `rt_hw_interrupt_disable()` 耗时 < 0.1us（RT-Thread CPSR 操作极快）

---

### 30.6 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-28 | `AREX_ARCH.md` | 新增 Section 30 — 硬件移植指南 |

---

## 31. 1F 罗盘零内存数学绘制引擎重构 (v2026-04-30)

### 31.1 问题描述

**原版罗盘的高内存占用问题**：
- 使用 `lv_canvas` + 静态 buffer `s_cbuf[640 * 140]`，占用约 **89KB RAM**
- 每帧重绘需要完整的画布操作，效率低下
- 航向变化时需要调用 `draw_tape()` 重新渲染整个画布

### 31.2 修复方案

**废弃 lv_canvas，改用 LV_EVENT_DRAW_MAIN 纯数学绘制引擎**：

```
┌─────────────────────────────────────────────────────────────┐
│  LV_EVENT_DRAW_MAIN 回调架构                                 │
├─────────────────────────────────────────────────────────────┤
│  LVGL 重绘周期 ──▶ compass_tape_draw_cb() ──▶ 数学计算 ──▶ 渲染 │
│                        │                                    │
│                   每次航向变化触发 lv_obj_invalidate()       │
│                   仅计算可见窗口内的刻度（0~N 循环判断）        │
│                   无需任何 buffer，内存占用 = 0               │
└─────────────────────────────────────────────────────────────┘
```

### 31.3 核心绘制参数

```c
#define COMPASS_TAPE_H      50      /* 卷尺高度 50px */
#define COMPASS_TAPE_BORDER 2       /* 卷尺边框 2px */
#define PX_PER_DEGREE       3.0f   /* 像素/度比例（越大刻度越稀疏） */
#define TAPE_TOP_OFFSET     20      /* 卷尺距标题区的偏移 */
```

### 31.4 数学绘制回调 `compass_tape_draw_cb()`

核心算法逻辑：

```c
// 1. 计算视口中心 X 坐标
int center_x = area->x1 + (box_w / 2);

// 2. 计算可见度数范围
int fov_deg = (int)(box_w / PX_PER_DEGREE);
int start_deg = heading - (fov_deg / 2) - 10;
int end_deg   = heading + (fov_deg / 2) + 10;

// 3. 循环渲染可见刻度（仅画 5° 倍数）
for (int i = start_deg; i <= end_deg; i++) {
    if (i % 5 != 0) continue;

    // 数学映射：该度数在屏幕上的 X 坐标
    float dx = (i - heading) * PX_PER_DEGREE;
    int x = center_x + (int)dx;

    // 越界裁剪
    if (x < area->x1 || x > area->x2) continue;

    // 刻度高度：15° 短线(6px)，45° 长线(12px)
    int tick_h = (i % 15 == 0) ? 12 : 6;
    lv_draw_line(draw_ctx, &line_dsc, ...);
}

// 4. 绘制中心瞄准线（4px 粗绿线）
// 5. 绘制目标锁定游标（黄色）
```

### 31.5 关键优化点

| 优化项 | 原版 | 新版 |
|--------|------|------|
| 内存占用 | ~89KB (640×140 buffer) | **0 bytes** |
| 绘制触发 | 每帧重绘整个画布 | **invalidate 触发局部重绘** |
| 刻度计算 | 固定 -60°~+60° | **动态视口计算** |
| 360° 循环 | 无 | **完美处理交界处** |

### 31.6 UI 消费任务对接

在 `arex_ui_engine.c` 的 `arex_ui_update_task()` 中：

```c
/* 罗盘航向 — 零内存数学引擎，触发 invalidate + 更新标签 */
if (mask & DIRTY_HEADING) {
    /* 更新巨型航向文字 */
    if (s_heading_val_lbl) {
        lv_label_set_text_fmt(s_heading_val_lbl, "%03d", g_sensor_data.heading);
    }
    /* 触发卷尺画板的底层数学重绘（极其轻量） */
    if (s_compass_tape_obj) {
        lv_obj_invalidate(s_compass_tape_obj);
    }
    /* 更新提示文本 */
    if (s_heading_hint_lbl) {
        if (g_sensor_data.heading_locked) {
            lv_label_set_text_fmt(s_heading_hint_lbl, "[ ENTER ] clear  %03d",
                                  g_sensor_data.heading_target);
        } else {
            lv_label_set_text(s_heading_hint_lbl, "[ ENTER ] mark heading");
        }
    }
}
```

### 31.7 外部声明（`arex_ui_engine.c`）

```c
/* 罗盘卡片静态句柄（由 card_compass.c 持有） */
extern lv_obj_t *s_compass_tape_obj;
extern lv_obj_t *s_heading_val_lbl;
extern lv_obj_t *s_heading_hint_lbl;
```

### 31.8 任务验收

- ✅ 罗盘代码内存占用从 ~89KB 降为 **0 bytes**
- ✅ 根据 `PX_PER_DEGREE` 设定，航向变化时卷尺以 **60 FPS** 极限丝滑滑动
- ✅ 消除旧版数十个 Label 同时重绘带来的撕裂感
- ✅ 360° 和 0° 交界处完美循环渲染

### 31.9 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-30 | `card_compass.c` | 完全重写：删除 lv_canvas + buffer，改为 `compass_tape_draw_cb()` 数学绘制引擎 |
| 2026-04-30 | `card_compass.c` | 新增 `render_compass_custom()` 工厂函数 |
| 2026-04-30 | `arex_ui_engine.c` | 添加罗盘句柄外部声明 |
| 2026-04-30 | `arex_ui_engine.c` | `arex_ui_update_task()` 中 DIRTY_HEADING 处理改为 invalidate + 标签更新 |
| 2026-04-30 | `AREX_ARCH.md` | 新增 Section 31 — 1F 罗盘零内存重构 |

---

## 33. 灯光控制子系统 (v2026-04-30)

### 33.1 功能概述

在 **DIVE SETUP** 卡片中新增 **LIGHT CONTROL** 选项，支持：
- 灯光开关控制（ON/OFF）
- RGBW 四色亮度调节（RED / GREEN / BLUE / WHITE）
- 每种颜色支持 5 档亮度：10% / 30% / 50% / 70% / 100%
- **自动开灯**：当灯光关闭时，点击任意颜色选项会自动先开灯

### 33.2 菜单结构

```
DIVE SETUP
  ├─ > LIGHT CONTROL          ← 新增入口
  │     ├─ LIGHT ON/OFF      ← 开关切换
  │     ├─ RED COLOR >       ─┐
  │     ├─ GREEN COLOR >     ─┼─ 颜色子菜单（嵌套）
  │     ├─ BLUE COLOR >     ─┤
  │     ├─ WHITE COLOR >     ─┘
  │     └─ < BACK
  │
  └─ > SYSTEM SETUP
```

点击颜色（如 RED COLOR >）后进入**专门的二级嵌套菜单**：
```
RED (二级嵌套菜单)
  ├─ 10%
  ├─ 30%
  ├─ 50%
  ├─ 70%
  ├─ 100%
  └─ < BACK
```

### 33.3 状态管理

```c
/* arex_screen.c - 全局灯光状态（供 LIGHT CONTROL 子菜单共享） */
bool g_light_power_state = false;  /* 灯光开关状态 */
```

### 33.4 二级嵌套菜单实现

颜色亮度菜单通过统一的 `nested_items_for()` 路由系统实现：

```c
/* arex_screen.c */
static const char *s_nested_red[]   = { "10%", "30%", "50%", "70%", "100%", "< BACK" };
static const char *s_nested_green[] = { "10%", "30%", "50%", "70%", "100%", "< BACK" };
static const char *s_nested_blue[]  = { "10%", "30%", "50%", "70%", "100%", "< BACK" };
static const char *s_nested_white[] = { "10%", "30%", "50%", "70%", "100%", "< BACK" };

static const char **nested_items_for(const char *title, uint8_t *out_count)
{
    // ... existing routes ...
    else if (strcmp(title, "RED")   == 0) tbl = s_nested_red;
    else if (strcmp(title, "GREEN") == 0) tbl = s_nested_green;
    else if (strcmp(title, "BLUE")  == 0) tbl = s_nested_blue;
    else if (strcmp(title, "WHITE") == 0) tbl = s_nested_white;
    // ...
}
```

### 33.5 自动开灯逻辑

当用户点击颜色选项时，如果灯处于关闭状态，系统会自动先开灯：

```c
/* arex_screen_handle_submenu_select() - LIGHT CONTROL 分支 */
if (strstr(text, "COLOR >") != NULL) {
    // 【自动开灯】如果灯是关闭状态，先自动打开
    if (!g_light_power_state) {
        g_light_power_state = true;
        arex_bus_set_light_power(true);
    }
    // 通过 nested_items_for 获取颜色亮度选项（专门的二级嵌套菜单）
    uint8_t ncnt = 0;
    const char **color_items = nested_items_for(color_name, &ncnt);
    if (color_items && ncnt > 0) {
        arex_screen_open_nested_submenu(color_name, color_items, ncnt);
    }
    return;
}
```

### 33.6 回调函数对接

#### 回调 1：灯光开关

**函数原型**：
```c
void arex_bus_set_light_power(bool on);
```

**调用时机**：用户在 `SETUP > LIGHT CONTROL > LIGHT ON/OFF` 点击时触发

**业务层对接示例**：
```c
/* 在业务层重新定义此函数 */
void arex_bus_set_light_power(bool on) {
    if (on) {
        GPIO_SetBits(PORT_LIGHT_EN, PIN_LIGHT_EN);
    } else {
        GPIO_ResetBits(PORT_LIGHT_EN, PIN_LIGHT_EN);
    }
}
```

#### 回调 2：颜色亮度设置

**函数原型**：
```c
void arex_ui_on_light_color_set(const char *color, const char *level);
```

**参数**：
| 参数 | 值 | 说明 |
|------|-----|------|
| `color` | `"RED"`, `"GREEN"`, `"BLUE"`, `"WHITE"` | 颜色名称 |
| `level` | `"10%"`, `"30%"`, `"50%"`, `"70%"`, `"100%"` | 亮度级别 |

**业务层对接示例**：
```c
void arex_ui_on_light_color_set(const char *color, const char *level) {
    uint8_t duty = 0;
    /* 解析亮度百分比 */
    if (strncmp(level, "10", 2) == 0) duty = 25;   // 10%
    else if (strncmp(level, "30", 2) == 0) duty = 76;  // 30%
    else if (strncmp(level, "50", 2) == 0) duty = 127;  // 50%
    else if (strncmp(level, "70", 2) == 0) duty = 178;  // 70%
    else duty = 255;  // 100%

    /* 根据颜色设置对应 PWM 通道 */
    if (strncmp(color, "RED", 3) == 0) {
        set_pwm_channel(PWM_CH_RED, duty);
    } else if (strncmp(color, "GREEN", 5) == 0) {
        set_pwm_channel(PWM_CH_GREEN, duty);
    } else if (strncmp(color, "BLUE", 4) == 0) {
        set_pwm_channel(PWM_CH_BLUE, duty);
    } else if (strncmp(color, "WHITE", 5) == 0) {
        set_pwm_channel(PWM_CH_WHITE, duty);
    }
}
```

### 33.7 文件变更

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-04-30 | `card_setup.c` | 新增 `"> LIGHT CONTROL"` 菜单项（索引 4） |
| 2026-04-30 | `arex_screen.c` | `s_setup_sub[]` 新增 LIGHT 子菜单配置 |
| 2026-04-30 | `arex_screen.c` | `s_setup_titles[]` 新增 `"> LIGHT CONTROL"` |
| 2026-04-30 | `arex_screen.c` | `arex_screen_handle_submenu_select()` 新增 LIGHT 处理分支 |
| 2026-04-30 | `arex_screen.c` | 新增 `arex_bus_set_light_power()` 回调（TODO 实现） |
| 2026-04-30 | `arex_screen.c` | 新增 `arex_ui_on_light_color_set()` 回调（TODO 实现） |
| 2026-04-30 | `arex_screen.h` | 新增回调函数声明 |
| 2026-04-30 | `arex_screen.c` | `g_light_power_state` 全局变量管理灯光状态 |
| 2026-04-30 | `arex_screen.c` | 颜色子菜单通过 `nested_items_for()` 路由实现 |
| 2026-04-30 | `arex_screen.c` | 点击颜色选项时自动开灯逻辑 |
| 2026-04-30 | `AREX_ARCH.md` | 新增 Section 33 — 灯光控制子系统 |

---

## 34. 速率图标多例指针阵列与动效闪烁引擎 (v2026-05-01)

### 34.1 问题描述

**问题 1：单例指针被覆盖**
- 原代码使用 `static lv_obj_t *s_img_ascent_rate = NULL` 单例指针
- 当屏幕上同时存在多个 DEPTH 模块时（如左侧锚点 1 个 + 5F 卡片多个），后创建的指针会覆盖先创建的
- 导致左侧锚点的速度图标失去响应

**问题 2：缺乏动感**
- 原代码只在 `DIRTY_DEPTH` 脏标记触发时更新图标
- 如果速度匀速不变（没有新的脏标记），图标静止不动
- 上升/下降时缺乏"呼吸效果"

### 34.2 修复方案

**第一步：单例升级为指针阵列**

```c
// arex_ui_engine.h/c 顶部
#define MAX_ASCENT_ICONS 4
static lv_obj_t *s_img_ascent_rate[MAX_ASCENT_ICONS];
static uint8_t s_ascent_icon_count = 0;
```

**第二步：工厂函数收集指针**

在 `render_widget_by_id()` 创建 DEPTH 图标时，将指针存入数组：

```c
lv_obj_t *sudu_img = lv_img_create(obj);
lv_img_set_src(sudu_img, &sudo_up_level0);
lv_obj_align(sudu_img, ico_al, ico_x, ico_y);

/* 核心修复：将生成的图片指针存入数组 */
if (s_ascent_icon_count < MAX_ASCENT_ICONS) {
    s_img_ascent_rate[s_ascent_icon_count++] = sudu_img;
}
```

**重建时清空计数器**

在 `arex_render_5f_custom_grid()` 和 `arex_render_left_anchor_grid()` 最开头清空数组：

```c
/* 清空速率图标阵列（防止内存溢出/指针残留） */
memset(s_img_ascent_rate, 0, sizeof(s_img_ascent_rate));
s_ascent_icon_count = 0;
```

### 34.3 速率计算与防抖（arex_data.c）

`arex_bus_set_depth()` 负责速率计算与防抖：

```c
void arex_bus_set_depth(float depth_m)
{
    /* 速率计算移到防抖之前，每次调用都计算最新值 */
    uint32_t now_ms = lv_tick_get();
    if (_last_depth_tick_ms > 0) {
        uint32_t dt_ms = now_ms - _last_depth_tick_ms;
        if (dt_ms > 0) {
            float dt_min = dt_ms / 60000.0f;
            float rate = (depth_m - _prev_depth) / dt_min;  /* m/min */
            g_sensor_data.ascent_rate = -rate;  /* 取反：正=上升，负=下降 */
        }
    }
    _last_depth_tick_ms = now_ms;
    _prev_depth = depth_m;

    /* 防抖：深度变化 > 0.05m 才触发 DIRTY_DEPTH */
    if (fabsf(g_sensor_data.depth - depth_m) > 0.05f) {
        g_sensor_data.depth = depth_m;
        g_sensor_data.dirty_mask |= DIRTY_DEPTH;
    } else {
        /* 深度未变 → 速率必为0，确保停留时图标显示 level0 不闪烁 */
        g_sensor_data.ascent_rate = 0.0f;
    }
}
```

**关键点：**
- 速率计算在防抖之前执行，确保每次调用都更新 `ascent_rate`
- 停留时（深度不变）主动清零 `ascent_rate = 0.0f`，防止残留非零值导致图标继续闪烁
- 防抖阈值 0.05m 避免 UI 频繁刷新

### 34.4 500ms 心跳闪烁引擎（arex_ui_engine.c）

在 `arex_ui_update_task()` 中注入闪烁逻辑，核心状态机：

```c
{
    static int8_t s_last_direction = 0;  /* 0=静止, 1=上升, -1=下降 */
    float rate = g_sensor_data.ascent_rate;
    bool is_moving = (fabsf(rate) >= AREX_RATE_STILL_THRESHOLD);  /* 3.0 m/min */
    bool current_flash_state = (lv_tick_get() / 500) % 2 == 0;   /* 500ms 节拍 */
    const void *target_img_src = &sudo_up_level0;

    /* 判断当前运动方向: 1=上升(positive), -1=下降(negative), 0=静止 */
    int8_t current_direction = 0;
    if (rate > 0.0f)      current_direction = 1;
    else if (rate < 0.0f) current_direction = -1;

    /* A. 静止悬停 或 方向切换过渡期 或 闪烁灭相位 → 强制显示 level0
     * 方向根据最后的运动方向决定（静止时保持最后的方向图标） */
    bool direction_changed = (current_direction != 0 && s_last_direction != 0
                              && current_direction != s_last_direction);
    if (!is_moving || direction_changed || current_flash_state == false) {
        int8_t effective_dir = is_moving ? current_direction : s_last_direction;
        target_img_src = (effective_dir > 0) ? &sudo_up_level0 : &sudo_down_level0;
    }
    /* B. 移动中 且 非方向切换期 且 闪烁亮相位 → 显示真实等级 */
    else {
        if (rate >= AREX_RATE_LEVEL2_THRESHOLD)       target_img_src = &sudo_up_level2;
        else if (rate >= AREX_RATE_LEVEL1_THRESHOLD) target_img_src = &sudo_up_level1;
        else if (rate > -AREX_RATE_LEVEL1_THRESHOLD) target_img_src = &sudo_down_level0;
        else if (rate > -AREX_RATE_LEVEL2_THRESHOLD) target_img_src = &sudo_down_level1;
        else                                          target_img_src = &sudo_down_level2;
    }

    /* 更新方向状态（仅在非静止时更新，静止时保持最后方向） */
    if (current_direction != 0) {
        s_last_direction = current_direction;
    }

    /* C. 循环遍历数组，让所有速率图标同步刷新 */
    for (int i = 0; i < s_ascent_icon_count; i++) {
        if (s_img_ascent_rate[i] != NULL) {
            lv_img_set_src(s_img_ascent_rate[i], target_img_src);
        }
    }
}
```

### 34.5 核心设计原理

| 特性 | 说明 |
|------|------|
| `lv_tick_get() / 500` | 硬件级节拍器，每 500ms 翻转一次，与速度数据无关 |
| `is_moving` | 移动判断，`|rate| >= 3.0 m/min` 视为移动 |
| `s_last_direction` | 记录最后运动方向，静止时用于决定 level0 的方向（上/下箭头） |
| `direction_changed` | 方向切换过渡期，强制显示 level0 防止图标跳变 |
| 灭相位强制 level0 | 动感实现，`current_flash_state == false` 时退回空心箭头 |
| 亮相位显示等级 | 呼吸效果，`current_flash_state == true` 时显示真实等级 |

**视觉效果**：
- 静止时：显示空心箭头（level0），方向由最后运动方向决定，不闪烁
- 上升/下降时：图标一亮一灭（一闪一闪），如"呼吸"般有动感
- 停留阶段（delta=0）：`ascent_rate` 被清零，图标固定显示 level0，方向与停留前的运动方向一致

### 34.6 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-05-01 | `arex_ui_engine.c` | 单例 `s_img_ascent_rate` 升级为 `s_img_ascent_rate[MAX_ASCENT_ICONS]` 指针数组 |
| 2026-05-01 | `arex_ui_engine.c` | `render_widget_by_id()` DEPTH 分支收集图标指针到数组 |
| 2026-05-01 | `arex_ui_engine.c` | `arex_render_5f_custom_grid()` 和 `arex_render_left_anchor_grid()` 清空数组计数器 |
| 2026-05-01 | `arex_ui_engine.c` | `arex_ui_update_task()` 注入 500ms 心跳闪烁引擎 |
| 2026-05-01 | `arex_data.c` | `arex_bus_set_depth()` 速率计算前置；停留时主动清零 `ascent_rate` |
| 2026-05-01 | `arex_ui_engine.c` | 静止/方向切换时 level0 方向由 `s_last_direction` 决定（非 rate>=0） |
| 2026-05-01 | `AREX_ARCH.md` | Section 34 新增 34.3 速率计算与防抖；34.4~34.6 更新 |

---

## 35. NDL_STOP 多形态组件与动态减压状态机 (v2026-05-01)

### 35.1 问题描述

在 160x60 的极限空间内，NDL 组件需要在三种完全不同的视觉状态之间**瞬间无缝切换**：

| 状态 | 视觉表现 |
|------|---------|
| **NDL 常态** | 左侧粗壮垂直进度条 + 右上方巨型数字（如 "22 NDL"） |
| **Safety 停留** | 垂直柱消失，底部出现横向进度条 + 顶部 "SAFETY 3m" 标题 + 缩小倒计时 |
| **Deco 停留** | 同上，但顶部 "DECO 6m" + 无 NDL 文本 + 更长倒计时 |

传统方案（替换整张图片 / 重建对象）会导致**明显的卡顿感**，用户体验极差。

### 35.2 数据总线扩展（arex_ui_engine.h）

**停留状态枚举**：

```c
typedef enum {
    AREX_STOP_NONE = 0,    /* 0: 常态，无停留 */
    AREX_STOP_SAFETY,      /* 1: 安全停留 */
    AREX_STOP_DECO         /* 2: 强制减压停留 */
} arex_stop_type_t;
```

**结构体新增字段**：

```c
/* --- 动态停留状态机 --- */
arex_stop_type_t stop_type;        /* 当前所处的停留模式 */
float            stop_depth_m;     /* 目标停留深度 (如 3.0m 或 6.0m) */
uint16_t         stop_time_total_s;/* 该减压站的总时间 (用于计算横向进度条) */
uint16_t         stop_time_left_s; /* 剩余倒计时 (秒) */
bool             in_stop_zone;     /* 是否在目标深度 ±1.5m 范围内？(决定是否读秒) */
```

### 35.3 静态句柄声明（arex_ui_engine.c 顶部）

```c
/* NDL_STOP 多形态组件句柄（160x60 极限空间内的"变形金刚"）
 * 三种状态: NDL常态 / Safety停留 / Deco停留 */
static lv_obj_t *s_ndl_comp      = NULL;
static lv_obj_t *s_ndl_vert_bg   = NULL;  /* 垂直进度条背景 */
static lv_obj_t *s_ndl_vert_fill = NULL;  /* 垂直进度条填充 */
static lv_obj_t *s_ndl_horiz_bg   = NULL; /* 横向进度条背景 */
static lv_obj_t *s_ndl_horiz_fill = NULL;/* 横向进度条填充 */
static lv_obj_t *s_ndl_main_val  = NULL;  /* 主干数值 (大数字/倒计时) */
static lv_obj_t *s_ndl_title_top = NULL;  /* 顶部标题 (SAFETY 3m / DECO 6m) */
static lv_obj_t *s_ndl_sub_bot   = NULL;  /* 底部副标题 (NDL 文本) */
```

**重建时清空句柄**（防止指针残留）：

```c
s_ndl_comp       = NULL;
s_ndl_vert_bg    = NULL;
s_ndl_vert_fill  = NULL;
s_ndl_horiz_bg   = NULL;
s_ndl_horiz_fill = NULL;
s_ndl_main_val   = NULL;
s_ndl_title_top  = NULL;
s_ndl_sub_bot    = NULL;
```

### 35.4 WIDGET_NDL_STOP_1606 创建拦截（render_widget_by_id）

在 `render_widget_by_id()` 中拦截 `WIDGET_NDL_STOP_1606`，**提前创建所有子对象**，靠显隐/字号切换实现瞬间变形：

```c
} else if (w_id == WIDGET_NDL_STOP_1606) {
    s_ndl_comp = obj;

    /* === 常态: 左侧垂直进度条 (宽14, 高40) === */
    s_ndl_vert_bg = lv_obj_create(obj);
    lv_obj_remove_style_all(s_ndl_vert_bg);
    lv_obj_set_size(s_ndl_vert_bg, 14, 40);
    lv_obj_align(s_ndl_vert_bg, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_border_width(s_ndl_vert_bg, 2, 0);
    lv_obj_set_style_border_color(s_ndl_vert_bg, AREX_GREEN, 0);
    lv_obj_set_style_radius(s_ndl_vert_bg, 4, 0);

    s_ndl_vert_fill = lv_obj_create(s_ndl_vert_bg);
    lv_obj_remove_style_all(s_ndl_vert_fill);
    lv_obj_set_size(s_ndl_vert_fill, LV_PCT(100), LV_PCT(60));
    lv_obj_align(s_ndl_vert_fill, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(s_ndl_vert_fill, AREX_GREEN, 0);
    lv_obj_set_style_bg_opa(s_ndl_vert_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_ndl_vert_fill, 2, 0);

    /* === 停留态: 底部横向进度条 (宽140, 高6) === */
    s_ndl_horiz_bg = lv_obj_create(obj);
    lv_obj_remove_style_all(s_ndl_horiz_bg);
    lv_obj_set_size(s_ndl_horiz_bg, 140, 6);
    lv_obj_align(s_ndl_horiz_bg, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_border_width(s_ndl_horiz_bg, 1, 0);
    lv_obj_set_style_border_color(s_ndl_horiz_bg, AREX_GREEN, 0);
    lv_obj_add_flag(s_ndl_horiz_bg, LV_OBJ_FLAG_HIDDEN);  /* 默认隐藏 */

    s_ndl_horiz_fill = lv_obj_create(s_ndl_horiz_bg);
    lv_obj_remove_style_all(s_ndl_horiz_fill);
    lv_obj_set_size(s_ndl_horiz_fill, LV_PCT(0), LV_PCT(100));
    lv_obj_align(s_ndl_horiz_fill, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(s_ndl_horiz_fill, AREX_GREEN, 0);
    lv_obj_set_style_bg_opa(s_ndl_horiz_fill, LV_OPA_COVER, 0);

    /* === 主干数值 (大数字/倒计时) === */
    s_ndl_main_val = lv_label_create(obj);
    lv_obj_set_style_text_color(s_ndl_main_val, AREX_GREEN, 0);
    lv_obj_set_style_text_font(s_ndl_main_val, arex_get_font(AREX_FONT_ID_HUGE), 0);
    lv_obj_align(s_ndl_main_val, LV_ALIGN_RIGHT_MID, -45, 0);

    /* === 顶部标题 === */
    s_ndl_title_top = lv_label_create(obj);
    lv_obj_set_style_text_font(s_ndl_title_top, arex_get_font(AREX_FONT_ID_SMALL), 0);
    lv_obj_set_style_text_color(s_ndl_title_top, AREX_LIGHT, 0);
    lv_obj_align(s_ndl_title_top, LV_ALIGN_TOP_LEFT, 10, 4);
    lv_obj_add_flag(s_ndl_title_top, LV_OBJ_FLAG_HIDDEN);  /* 默认隐藏 */

    /* === 底部副标题 === */
    s_ndl_sub_bot = lv_label_create(obj);
    lv_obj_set_style_text_font(s_ndl_sub_bot, arex_get_font(AREX_FONT_ID_SMALL), 0);
    lv_obj_set_style_text_color(s_ndl_sub_bot, AREX_GREEN, 0);
    lv_obj_align(s_ndl_sub_bot, LV_ALIGN_BOTTOM_RIGHT, -10, -5);

    return obj;
}
```

### 35.5 状态机切换逻辑（arex_ui_update_task）

在 `arex_ui_update_task()` 中注入状态机，响应 `DIRTY_NDL_STOP` / `DIRTY_DEPTH` / `DIRTY_NDL`：

```c
if (s_ndl_comp != NULL && (mask & (DIRTY_NDL_STOP | DIRTY_DEPTH | DIRTY_NDL))) {

    /* ========== 状态 1: 常规 NDL 模式 ========== */
    if (g_sensor_data.stop_type == AREX_STOP_NONE) {
        /* 显隐控制 */
        lv_obj_clear_flag(s_ndl_vert_bg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ndl_horiz_bg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ndl_title_top, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_ndl_sub_bot, LV_OBJ_FLAG_HIDDEN);

        /* 58px 巨型字体，纯 NDL 数字 */
        lv_obj_set_style_text_font(s_ndl_main_val, arex_get_font(AREX_FONT_ID_HUGE), 0);
        lv_label_set_text_fmt(s_ndl_main_val, "%d", g_sensor_data.ndl);
        lv_obj_align(s_ndl_main_val, LV_ALIGN_RIGHT_MID, -45, 0);

        lv_label_set_text(s_ndl_sub_bot, "NDL");
        lv_obj_align(s_ndl_sub_bot, LV_ALIGN_BOTTOM_RIGHT, -10, -5);

        /* 垂直进度条动态计算 */
        int fill_h = (g_sensor_data.ndl * 40) / 99;
        if (fill_h > 40) fill_h = 40;
        if (fill_h < 1) fill_h = 1;
        lv_obj_set_size(s_ndl_vert_fill, LV_PCT(100), fill_h);
    }
    /* ========== 状态 2 & 3: 停留模式 (Safety / Deco) ========== */
    else {
        /* 显隐控制 */
        lv_obj_add_flag(s_ndl_vert_bg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_ndl_horiz_bg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_ndl_title_top, LV_OBJ_FLAG_HIDDEN);

        /* 缩小主字体为 28px 以腾出空间显示 MM:SS */
        lv_obj_set_style_text_font(s_ndl_main_val, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
        lv_obj_align(s_ndl_main_val, LV_ALIGN_RIGHT_MID, -10, -5);

        /* 判断在 ±1.5m 范围内？(读秒 vs 读分) */
        if (g_sensor_data.in_stop_zone) {
            int m = g_sensor_data.stop_time_left_s / 60;
            int s = g_sensor_data.stop_time_left_s % 60;
            lv_label_set_text_fmt(s_ndl_main_val, "%d:%02d", m, s);
        } else {
            int m = (g_sensor_data.stop_time_left_s + 59) / 60;
            lv_label_set_text_fmt(s_ndl_main_val, "%d'", m);
        }

        /* 标题文本分配 */
        if (g_sensor_data.stop_type == AREX_STOP_SAFETY) {
            lv_label_set_text_fmt(s_ndl_title_top, "SAFETY %.0fm", g_sensor_data.stop_depth_m);
            lv_obj_clear_flag(s_ndl_sub_bot, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(s_ndl_sub_bot, "NDL %d", g_sensor_data.ndl);
            lv_obj_align(s_ndl_sub_bot, LV_ALIGN_BOTTOM_LEFT, 10, -14);
        } else {
            lv_label_set_text_fmt(s_ndl_title_top, "DECO %.0fm", g_sensor_data.stop_depth_m);
            lv_obj_add_flag(s_ndl_sub_bot, LV_OBJ_FLAG_HIDDEN);
        }

        /* 横向进度条动态生长 */
        if (g_sensor_data.stop_time_total_s > 0) {
            int fill_w = ((g_sensor_data.stop_time_total_s - g_sensor_data.stop_time_left_s) * 140)
                         / g_sensor_data.stop_time_total_s;
            if (fill_w > 140) fill_w = 140;
            if (fill_w < 1) fill_w = 1;
            lv_obj_set_size(s_ndl_horiz_fill, fill_w, 6);
        }
    }
}
```

### 35.6 仿真测试逻辑（UI_main.c）

在仿真定时器中注入停留状态机测试代码：

```c
{
    static uint16_t s_ndl_tick = 0;
    s_ndl_tick++;

    /* 1. NDL 递减 */
    if (g_sensor_data.ndl > 0) {
        arex_bus_set_ndl((int16_t)(g_sensor_data.ndl - 1));
    }

    /* 2. 常态: 深度 < 5m 且 NDL > 0 */
    g_sensor_data.stop_type = AREX_STOP_NONE;
    g_sensor_data.in_stop_zone = false;

    /* 3. 安全停留: 深度 5~10m，触发 3m 安全停留 180秒 */
    if (s_sim_depth >= 5.0f && s_sim_depth < 10.0f && g_sensor_data.ndl > 0) {
        g_sensor_data.stop_type = AREX_STOP_SAFETY;
        g_sensor_data.stop_depth_m = 3.0f;
        g_sensor_data.stop_time_total_s = 180;
        g_sensor_data.stop_time_left_s = 180 - (s_ndl_tick % 180);
        g_sensor_data.in_stop_zone = (fabsf(s_sim_depth - 3.0f) <= 1.5f);
    }
    /* 4. 减压停留: 深度 >= 10m 或 NDL 耗尽 */
    else if (s_sim_depth >= 10.0f || g_sensor_data.ndl <= 0) {
        if (g_sensor_data.ndl <= 0) g_sensor_data.ndl = 0;
        g_sensor_data.stop_type = AREX_STOP_DECO;
        g_sensor_data.stop_depth_m = 6.0f;
        g_sensor_data.stop_time_total_s = 300;
        g_sensor_data.stop_time_left_s = 300 - (s_ndl_tick % 300);
        g_sensor_data.in_stop_zone = (fabsf(s_sim_depth - 6.0f) <= 1.5f);
    }

    /* 强制唤醒 UI 更新 */
    g_sensor_data.dirty_mask |= DIRTY_NDL_STOP;
}
```

**仿真效果**：
- 下水初期（0~25s）：深度 < 5m，显示巨型 NDL 数字
- 深度 5~10m（25~50s）：垂直柱消失，底部出现 "SAFETY 3m" + 倒计时横向条
- 深度 > 10m（50s+）：切换 "DECO 6m" + 更长倒计时

### 35.7 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-05-01 | `arex_ui_engine.h` | 新增 `arex_stop_type_t` 枚举；`arex_sensor_data_t` 新增 5 个停留状态字段 |
| 2026-05-01 | `arex_ui_engine.c` | 新增 `s_ndl_*` 静态句柄；`render_widget_by_id()` 拦截 WIDGET_NDL_STOP_1606 创建多形态组件 |
| 2026-05-01 | `arex_ui_engine.c` | `arex_render_5f_custom_grid()` / `arex_render_left_anchor_grid()` 清空 NDL 句柄 |
| 2026-05-01 | `arex_ui_engine.c` | `arex_ui_update_task()` 注入 NDL_STOP 状态机切换逻辑 |
| 2026-05-01 | `UI_main.c` | 仿真定时器新增停留状态机测试逻辑 |
| 2026-05-01 | `AREX_ARCH.md` | 新增 Section 35 - NDL_STOP 多形态组件与动态减压状态机 |

---

## 36. 数组清空时机问题：双渲染区域踩踏 Bug 分析与修复 (v2026-05-01)

### 36.1 问题现象

NDL 状态机和速率图标（Sudo 箭头）在两个渲染区域的表现不一致：

- **5F 自定义卡片区**：NDL_STOP 状态机能正常变形，sudo 图标正常闪烁 ✅
- **左侧锚点固定区**：两者均无变化 ❌

两种表现完全隔离，仿佛是两个互不干扰的子系统。

### 36.2 根本原因：错误的清空时机

#### 架构背景

ARE-X 的 UI 渲染分为**两个独立的渲染区域**，但它们共用同一套 widget 句柄数组：

| 渲染区域 | 调用函数 | 渲染内容 |
|---------|---------|---------|
| 左侧锚点 2x6 网格 | `arex_render_left_anchor_grid()` | DEPTH、NDL、TTS 等固定组件 |
| 5F 自定义网格 | `arex_render_5f_custom_grid()` | 用户配置的 widget（可含 NDL） |

两个渲染函数都通过 `render_widget_by_id()` 工厂创建 NDL 组件，并把句柄**追加到同一个全局数组**：

```c
// arex_ui_engine.c
ndl_handle_t s_ndl_handles[MAX_NDL_ICONS];  // 共用数组
uint8_t      s_ndl_handle_count = 0;          // 追加计数器
```

#### 错误发生的过程

问题出在**渲染顺序和清空时机**。原来的代码在**三个地方**各自清空数组：

```c
// ❌ 错误：多处清空，时机混乱

// 位置1: arex_render_5f_custom_grid() 开头
void arex_render_5f_custom_grid(...) {
    memset(s_ndl_handles, 0, sizeof(s_ndl_handles));  // ← 清空！
    s_ndl_handle_count = 0;
    // ... 渲染 5F widget，追加到数组
}

// 位置2: arex_render_left_anchor_grid() 开头
void arex_render_left_anchor_grid(...) {
    memset(s_ndl_handles, 0, sizeof(s_ndl_handles));  // ← 又清空！
    s_ndl_handle_count = 0;
    // ... 渲染左侧 widget，追加到数组
}

// 位置3: left_anchor_rebuild() 里头
static void left_anchor_rebuild(...) {
    memset(s_ndl_handles, 0, sizeof(s_ndl_handles));  // ← 再次清空！
    s_ndl_handle_count = 0;
    arex_render_left_anchor_grid(...);
}
```

而这两个渲染函数的调用顺序是：

```c
// arex_screen.c
void arex_screen_rebuild_layout(void) {
    left_anchor_rebuild(0);           // ① 先渲染左侧 → 数组：[左侧NDL]
                                       // ② → 调用 render_left_anchor_grid()
                                       // ③ → render_left_anchor_grid 里头又清空一次！
                                       // ④ 结果：数组被清空，变成 []
    card_engine_create(CARD_ID_CUSTOM_GRID);  // ⑤ 渲染 5F
                                                // ⑥ → 调用 render_5f_custom_grid()
                                                // ⑦ → 数组：[5F的NDL]
}
```

最终结果：**只有最后被调用的渲染函数（5F）的 widget 被保留在数组里**。状态机遍历数组时，只能刷新 5F 区域的组件，左侧锚点完全被无视。

#### 速率图标（ascent icon）同样遭殃

同样的问题也影响了 `s_img_ascent_rate[]` 数组。sudo 箭头图标在 DEPTH 模块里被创建并追加到数组，但因为清空时机混乱，只有最后一个渲染区域的图标被保留。

### 36.3 修复方案：统一清空，单点控制

核心思想：**清空数组和渲染 widget 是两件完全不同的事，必须彻底分离**。

引入统一的 `clear_widget_arrays()` helper，在**渲染之前只清空一次**：

```c
// arex_screen.c

/* 清空 ascent/NDL widget 句柄数组（在任何网格渲染之前调用） */
static void clear_widget_arrays(void)
{
    memset(s_img_ascent_rate, 0, sizeof(s_img_ascent_rate));
    s_ascent_icon_count = 0;
    memset(s_ndl_handles, 0, sizeof(s_ndl_handles));
    s_ndl_handle_count = 0;
}
```

然后在两个入口各调用一次：

```c
// 入口A：首次初始化
static void left_anchor_create(void) {
    lv_obj_clean(s_left_anchor);
    clear_widget_arrays();        // ← 统一清空
    arex_render_left_anchor_grid(s_left_anchor);   // 追加左侧
    arex_render_5f_custom_grid(tile, ...);         // 追加 5F
}

// 入口B：布局重建
void arex_screen_rebuild_layout(void) {
    clear_widget_arrays();        // ← 统一清空
    left_anchor_rebuild(0);      // 追加左侧
    card_engine_create(CARD_ID_CUSTOM_GRID);  // 追加 5F
}
```

两个渲染函数改为**纯追加模式**，不再自己清空：

```c
// arex_render_left_anchor_grid()  —— 不再清空！
void arex_render_left_anchor_grid(...) {
    // ... 渲染左侧 widget
    // 自动追加到数组
}

// arex_render_5f_custom_grid()    —— 不再清空！
void arex_render_5f_custom_grid(...) {
    // ... 渲染 5F widget
    // 自动追加到数组
}
```

最终数组状态（正确的顺序）：

```
渲染前：clear_widget_arrays()  →  数组=[]
渲染后：left_anchor  →  数组=[左侧NDL, 左侧DEPTH+sudo]
渲染后：5F_grid      →  数组=[左侧NDL, 左侧DEPTH, 5F的NDL, 5F的DEPTH]
状态机遍历数组  →  所有实例同步刷新 ✅
```

### 36.4 变量作用域问题：C 文件间的 static 隔离

修复过程中还遇到了一个编译错误：

```
error: 's_ndl_handles' undeclared (first use in this function)
```

原因是 `s_ndl_handles[]` 在 `arex_ui_engine.c` 中声明为 `static`，只能在那个文件内部访问。`arex_screen.c` 无法直接访问。

**解决方案**：把这些共享数组的声明从 `static` 改为非 static，并移动到 `arex_ui_engine.h` 头文件中作为 `extern` 声明：

```c
// arex_ui_engine.h（头文件：extern 声明）
#define MAX_NDL_ICONS 4
typedef struct { ... } ndl_handle_t;
#define MAX_ASCENT_ICONS 4
extern lv_obj_t *s_img_ascent_rate[MAX_ASCENT_ICONS];
extern uint8_t  s_ascent_icon_count;
extern ndl_handle_t s_ndl_handles[MAX_NDL_ICONS];
extern uint8_t  s_ndl_handle_count;

// arex_ui_engine.c（源文件：实际定义）
lv_obj_t *s_img_ascent_rate[MAX_ASCENT_ICONS];
uint8_t  s_ascent_icon_count = 0;
ndl_handle_t s_ndl_handles[MAX_NDL_ICONS];
uint8_t  s_ndl_handle_count = 0;

// arex_screen.c（通过 include 访问）
#include "arex_ui_engine.h"  // 隐式获得 extern 声明
clear_widget_arrays();       // ✅ 正确访问
```

### 36.5 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-05-01 | `arex_ui_engine.h` | `ndl_handle_t` 结构体 + `s_ndl_handles[]` / `s_img_ascent_rate[]` 改为 extern 声明 |
| 2026-05-01 | `arex_screen.c` | 新增 `clear_widget_arrays()` 统一清空函数 |
| 2026-05-01 | `arex_screen.c` | `arex_screen_rebuild_layout()` 入口调用 `clear_widget_arrays()` |
| 2026-05-01 | `arex_screen.c` | `left_anchor_create()` 入口调用 `clear_widget_arrays()` |
| 2026-05-01 | `arex_ui_engine.c` | `arex_render_left_anchor_grid()` 移除自己的 memset |
| 2026-05-01 | `arex_ui_engine.c` | `arex_render_5f_custom_grid()` 移除自己的 memset |
| 2026-05-01 | `AREX_ARCH.md` | 新增 Section 36 - 双渲染区域踩踏 Bug 分析与修复 |

---

## 37. 协议极简降维与 MCU 本地样式字典 (Union 内存共享版) (v2026-05-01)

### 37.1 问题描述

原有 `arex_custom_widget_cfg_t` 结构体存在严重的"过度耦合"与"内存浪费"：

- APP 下发了 `w` 和 `h`（但组件 ID 已经明确编码了尺寸信息）
- BLE 协议中每个组件占 5 字节，浪费了 2 字节/组件
- 各种奇形怪状模块的专属 offset 造成结构体无限膨胀

### 37.2 架构铁律

**APP 只负责下发"意图和网格坐标" (Intent & Grid Pos)，单片机 (MCU) 负责包揽所有"内部像素级排版" (Local CSS)。**

既然组件 ID（如 `DEPTH_1612`）已经明确代表了物理跨度（2x2），BLE 协议就绝不允许再冗余下发 w 和 h。

### 37.3 第一步：扁平分组枚举（前后端严格对齐）

```c
typedef enum {
    AREX_WIDGET_UNSPECIFIED = 0,

    /* --- 核心驻留区 (强制固定) --- */
    WIDGET_NDL_STOP_1606 = 1,   /* NDL/停留状态机 (2x1) */
    WIDGET_DEPTH_1612    = 2,   /* 深度大通栏 (2x2) */
    WIDGET_DEPTH_1606    = 3,   /* 深度长条 (2x1) */
    WIDGET_DIVE_TIME_1606 = 4,  /* 潜水计时 (2x1) */
    WIDGET_GAS_1606      = 5,  /* 当前气体 (2x1) */
    WIDGET_SYS_1606      = 6,  /* 系统状态栏 (2x1) */

    /* --- 基础组件 (Basic) --- */
    WIDGET_TEMP_0806     = 10,  /* 温度 (1x1) */
    WIDGET_TIME_1606     = 11,  /* 当前时间 (2x1) */
    WIDGET_TTS_0806      = 12,  /* 到水面时间 (1x1) */
    WIDGET_ASCENT_0806   = 13,  /* 上升速率 (1x1) */
    WIDGET_COMPASS_1612  = 15,  /* 罗盘 (2x2) */
    WIDGET_BATTERY_0806  = 16,  /* 电池 (1x1) */
    /* ... 更多组件 ... */

} arex_widget_id_t;
```

**命名规则**: `WIDGET_<TYPE>_<W><H>`，例如 `WIDGET_DEPTH_1612` = 2列×2行。

### 37.4 第二步：极简 BLE 通信帧（仅 3 字节/组件）

```c
#pragma pack(push, 1)
typedef struct {
    arex_widget_id_t widget_id;  /* 组件类型 ID */
    uint8_t x;                    /* 列索引 0~4 */
    uint8_t y;                    /* 行索引 0~5 */
} arex_widget_pos_t;             /* 仅 3 字节！ */
#pragma pack(pop)
```

### 37.5 第三步：MCU 本地样式字典（Union 内存优化）

```c
/* DEPTH 专属样式 */
typedef struct {
    int8_t  int_offset_x, int_offset_y; uint8_t int_align;
    int8_t  dec_offset_x, dec_offset_y;
    int8_t  unit_offset_x, unit_offset_y;
    int8_t  icon_offset_x, icon_offset_y; uint8_t icon_align;
} arex_style_depth_t;

/* NDL_STOP 多形态专属样式 */
typedef struct {
    int8_t  vert_offset_x, vert_offset_y; uint8_t vert_align;
    int8_t  vert_w, vert_h;
    int8_t  horiz_offset_x, horiz_offset_y;
    int8_t  horiz_w, horiz_h;
    int8_t  main_offset_x, main_offset_y; uint8_t main_align;
    int8_t  title_offset_x, title_offset_y; uint8_t title_align;
    int8_t  sub_offset_x, sub_offset_y; uint8_t sub_align;
} arex_style_ndl_stop_t;

/* 通用基础样式 */
typedef struct {
    int8_t value_offset_x, value_offset_y; uint8_t value_align;
} arex_style_basic_t;

/* MCU 本地样式字典（Union 共享内存，大小永远等于最大成员） */
#define AREX_MAX_STYLE_SPEC_SIZE 32
typedef struct {
    arex_widget_id_t widget_id;
    uint8_t span_w, span_h;               /* MCU 本地做主！ */

    /* 核心新增：元素开关掩码（ELEM_TITLE|ELEM_VALUE|ELEM_UNIT|ELEM_BAR） */
    uint8_t elements;

    arex_font_id_t font_id;               /* 主数值字号 */
    arex_font_id_t title_font_id;          /* 标题字号 */
    const char *unit;                      /* 单位字符串（NULL 则无单位） */
    int8_t  title_offset_x, title_offset_y; uint8_t title_align;

    union {                       /* 强制共享内存，防止膨胀 */
        arex_style_depth_t     depth;
        arex_style_ndl_stop_t ndl_stop;
        arex_style_basic_t    basic;
        uint8_t              dummy[AREX_MAX_STYLE_SPEC_SIZE];
    } spec;
} arex_widget_style_t;
```

### 37.6 第四步：只读样式注册表

```c
static const arex_widget_style_t g_widget_styles[] = {
    {
        .widget_id = WIDGET_DEPTH_1612,
        .span_w = 2, .span_h = 2,
        .elements = ELEM_VALUE | ELEM_UNIT | ELEM_BAR,  /* 无 title，走 spec.depth */
        .font_id = AREX_FONT_ID_HUGE,
        .unit = "m",
        .spec.depth = {
            .int_offset_x = 10, .int_offset_y = 0, .int_align = LV_TEXT_ALIGN_LEFT,
            .dec_offset_x = 2,  .dec_offset_y = 5,
            .unit_offset_x = 0, .unit_offset_y = 2,
            .icon_offset_x = -10, .icon_offset_y = 0, .icon_align = LV_ALIGN_RIGHT_MID
        }
    },
    {
        .widget_id = WIDGET_NDL_STOP_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT | ELEM_BAR,
        .font_id = AREX_FONT_ID_HUGE,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "min",
        .title = "NDL",
        .spec.ndl_stop = {
            .vert_offset_x = 10, .vert_offset_y = 0, .vert_align = LV_ALIGN_LEFT_MID,
            .vert_w = 14, .vert_h = 40,
            .horiz_offset_x = 0, .horiz_offset_y = -4, .horiz_w = 140, .horiz_h = 6,
            .main_offset_x = -45, .main_offset_y = 0, .main_align = LV_ALIGN_RIGHT_MID,
            .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_TEXT_ALIGN_LEFT,
            .sub_offset_x = -10, .sub_offset_y = -5, .sub_align = LV_ALIGN_BOTTOM_RIGHT
        }
    },
    /* 35 个组件全部注册，每个条目均有 .elements 字段 ... */
};
#define STYLE_COUNT (int)(sizeof(g_widget_styles) / sizeof(g_widget_styles[0]))

/* 查表函数 */
const arex_widget_style_t* arex_get_widget_style(arex_widget_id_t id)
{
    for (int i = 0; i < STYLE_COUNT; i++) {
        if (g_widget_styles[i].widget_id == id)
            return &g_widget_styles[i];
    }
    return NULL;
}
```

### 37.7 第五步：掩码驱动的按需装配工厂函数

```c
lv_obj_t *render_widget_by_id(..., arex_font_id_t cfg_font_id)
{
    const arex_widget_style_t *style = arex_get_widget_style(w_id);
    /* ... 容器创建代码 ... */

    /* 阶段一：专属早期分支（DEPTH/NDL 走独立渲染路径，立即返回） */
    if (w_id == WIDGET_DEPTH_1612) { /* int_lbl + dec_lbl + unit_lbl + sudu_icon */ return obj; }
    if (w_id == WIDGET_NDL)         { /* vert_bar + horiz_bar + main_val + ... */ return obj; }

    /* 阶段二：通用流水线 — 按 elements 掩码按需装配 */
    if (style->elements & ELEM_TITLE) { /* 创建标题 label */ }
    if (style->elements & ELEM_VALUE)  { /* 创建数值 label，数据源 switch 分发 */ }
    if ((style->elements & ELEM_UNIT) && style->unit) { /* 创建单位 label */ }
    if (style->elements & ELEM_BAR) {
        if (w_id == WIDGET_DEPTH_1612 || w_id == WIDGET_ASCENT_0812)  { /* sudu 图标 */ }
        else if (w_id == WIDGET_COMPASS_1612)                           { /* 卷尺 tape */ }
        else if (w_id == WIDGET_NDL_STOP_1606)                        { /* 多形态进度条 */ }
        else if (w_id == WIDGET_SYS_1606)                              { /* 电池进度条 */ }
        else if (w_id == WIDGET_TISSUE_GF_4012)                       { /* 组织柱状图 */ }
    }
    if (style->elements & ELEM_EXTRA) {
        if (w_id == WIDGET_POD_0806) { /* POD1/POD2 专属 ID 标签 */ }
    }

    /* 阶段三：返回容器句柄 */
    return obj;
}
```

### 37.8 POD1/POD2 共用枚举值的状态机策略

问题：POD1 和 POD2 共用 `WIDGET_POD_0806` (值 33)，MCU 收到 33 时无法区分。

解决：静态渲染计数器，同一渲染批次内按遍历顺序分配。

```c
/* 渲染前调用归零 */
void arex_reset_widget_render_state(void) { s_pod_render_count = 0; }

/* 每次遇到 WIDGET_POD_0806 时调用 */
static bool arex_consume_pod_slot(void) {
    return (s_pod_render_count++ == 0);  /* true=POD1, false=POD2 */
}

/* 渲染时将枚举替换为具体类型 */
arex_widget_id_t render_wid = pos->widget_id;
if (pos->widget_id == WIDGET_POD_0806) {
    /* POD1/POD2 通过渲染槽位轮转计数器区分，不依赖 widget_id */
    render_wid = arex_consume_pod_slot() ? WIDGET_POD_0806 : WIDGET_POD_0806;
}
```

**使用时机**：`arex_render_left_anchor_grid()` / `arex_render_5f_custom_grid()` / `left_anchor_create()` 入口处调用 `arex_reset_widget_render_state()` 归零。

### 37.9 变更文件清单

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-05-01 | `arex_ui_engine.h` | 删除 `/* 向后兼容别名字典 (Legacy Aliases) */` 整块 #define 宏（约50行）；新增别名定义 `#define AREX_WIDGET_ASCENT_0812 WIDGET_ASCENT_0812` 等 |
| 2026-05-01 | `arex_ui_engine.c` | 全局替换全部 AREX_WIDGET_* 别名为 WIDGET_*_<WH> 枚举值（共51处，分布在 ~60行代码中）；修复双后缀污染（`WIDGET_TEMP_0806_0806` → `WIDGET_TEMP_0806` 等24处） |
| 2026-05-01 | `arex_ui_engine.c` | `g_widget_styles[]`（36个组件含 WIDGET_DEPTH_1606）；POD 渲染状态机（`s_pod_render_count`） |
| 2026-05-01 | `arex_ui_engine.c` | `arex_get_widget_style()`、`arex_reset_widget_render_state()` 实现 |
| 2026-05-01 | `arex_screen.c` | 全局替换 AREX_WIDGET_* 别名为 WIDGET_*_<WH>（12处） |
| 2026-05-01 | `UI_main.c` | 全局替换 AREX_WIDGET_* 别名为 WIDGET_*_<WH>（13处） |
| 2026-05-01 | `AREX_ARCH.md` | Section 37.3 删除"向后兼容别名"代码块示例；37.8 更新 POD 槽位伪代码；37.9 新增本条变更记录 |

---

## 38. POD 单模具轮转分配架构 (v2026-05-01)

### 38.1 核心设计原则

**彻底废弃虚拟 ID 方案，改用"单模具 + 渲染计数器"！**

```
思维突破：WIDGET_POD_0806 (33) 只是一个"没有灵魂的皮囊模具"。

它不在乎你绑了什么 ID，它只在乎"我是第几个出现的 POD"。

APP 只需要保证，把绑定为"气瓶1"的组件排在前面；
把"气瓶2"排在后面。

MCU 靠那个小小的 s_pod_render_count，
就能把 UI 模具和物理气瓶完美、不出错地缝合在一起！
```

### 38.2 架构设计

```
┌─────────────────────────────────────────────────────────────────┐
│  APP 端：用户拖拽组件 + 绑定气瓶 ID                                │
├─────────────────────────────────────────────────────────────────┤
│  APP 下发 BLE 布局帧：                                            │
│    widget_ids[0] = 33 (POD_0806)  → 绑定气瓶1 (MAC: A1-B2)     │
│    widget_ids[1] = 33 (POD_0806)  → 绑定气瓶2 (MAC: C3-D4)     │
├─────────────────────────────────────────────────────────────────┤
│  MCU 端：渲染拦截 + 轮转分配                                      │
├─────────────────────────────────────────────────────────────────┤
│  第一次遇到 33 → count=1(奇数) → 分配 POD1 → 烙印 1033          │
│  第二次遇到 33 → count=2(偶数) → 分配 POD2 → 烙印 2033          │
├─────────────────────────────────────────────────────────────────┤
│  数据更新：精确靶向                                                │
├─────────────────────────────────────────────────────────────────┤
│  arex_bus_set_pod(0, 200.0f)  →  触发 DIRTY_POD               │
│  arex_widget_set_value(POD_0806, 200.0f)                      │
│    → 遍历找 user_data==1033 的容器 → 更新 "200"                  │
│  arex_bus_set_pod(1, 180.0f)  →  触发 DIRTY_POD               │
│  arex_widget_set_value(POD_0806, 180.0f)                       │
│    → 遍历找 user_data==2033 的容器 → 更新 "180"                  │
└─────────────────────────────────────────────────────────────────┘
```

### 38.3 枚举定义（简洁无虚拟 ID）

```c
/* arex_ui_engine.h — arex_widget_id_t 枚举 */
typedef enum {
    /* ... 其他组件 ... */
    WIDGET_HEADING_0806  = 32,
    WIDGET_POD_0806      = 33,  /* 全局唯一真实存在的气瓶模具 ID */
    WIDGET_DEPTH_MAX_0806 = 34,
    /* ... 其他组件 ... */
    WIDGET_EMPTY         = 99,
} arex_widget_id_t;
```

### 38.4 标签宏定义

```c
/* arex_ui_engine.c — POD 标签宏 */
#define POD_TAG_BASE  1000  /* POD 标签基准偏移 */
#define POD1_TAG      (POD_TAG_BASE + WIDGET_POD_0806)  /* 1033 */
#define POD2_TAG      (2 * POD_TAG_BASE + WIDGET_POD_0806)  /* 2033 */
```

### 38.5 轮转计数器状态机

```c
/* arex_ui_engine.c */
static uint8_t s_pod_render_count = 0;

/* 渲染计数器归零（每次网格重建前调用） */
void arex_reset_widget_render_state(void)
{
    s_pod_render_count = 0;
}

/* 获取 POD 标签（根据当前渲染计数器返回值） */
static uintptr_t arex_get_pod_tag(void)
{
    /* 偶数次调用 → POD1，奇数次调用 → POD2 */
    return (s_pod_render_count % 2 == 0) ? POD1_TAG : POD2_TAG;
}

/* 获取 POD 编号（返回 1 或 2） */
static uint8_t arex_get_pod_index(void)
{
    /* 偶数次调用 → POD1，奇数次调用 → POD2 */
    return (s_pod_render_count % 2 == 0) ? 1 : 2;
}
```

### 38.6 渲染拦截（render_widget_by_id 内部）

```c
lv_obj_t *render_widget_by_id(lv_obj_t *parent,
                               arex_widget_id_t w_id, ...)
{
    /* ===== POD 单模具拦截：提前消耗计数器 ===== */
    bool is_pod模具 = (w_id == WIDGET_POD_0806);
    uint8_t pod_index = 0;
    uintptr_t pod_tag = 0;
    if (is_pod模具) {
        s_pod_render_count++;     /* 先递增，再获取当前值 */
        pod_index = arex_get_pod_index();
        pod_tag = arex_get_pod_tag();
    }

    /* 创建容器 ... */

    /* ===== 靶向告警烙印 =====
     * POD 使用高位掩码标签（1033/2033），其他使用原始 w_id */
    if (is_pod模具) {
        lv_obj_set_user_data(obj, (void *)pod_tag);
    } else {
        lv_obj_set_user_data(obj, (void *)(uintptr_t)w_id);
    }

    /* 零件 1：标题 */
    if ((style->elements & ELEM_TITLE) && style->title) {
        lv_obj_t *title_lbl = lv_label_create(obj);
        if (is_pod模具) {
            lv_label_set_text_fmt(title_lbl, "POD %d", pod_index);  /* "POD 1" 或 "POD 2" */
        } else {
            lv_label_set_text(title_lbl, style->title);
        }
        /* ... */
    }

    /* 零件 2：数值 */
    if (style->elements & ELEM_VALUE) {
        if (is_pod模具) {
            /* 根据 pod_index 动态决定数据源 */
            if (pod_index == 1) {
                snprintf(buf, sizeof(buf), "%.0f", (double)g_sensor_data.pod1_bar);
            } else {
                snprintf(buf, sizeof(buf), "%.0f", (double)g_sensor_data.pod2_bar);
            }
        }
        /* ... */
    }

    /* 零件 5：EXTRA 附加异构元素 */
    if (style->elements & ELEM_EXTRA) {
        if (is_pod模具) {
            lv_obj_t *pod_id_lbl = lv_label_create(obj);
            lv_label_set_text_fmt(pod_id_lbl, "%d", pod_index);  /* "1" 或 "2" */
            /* ... */
        }
    }

    return obj;
}
```

### 38.7 更新精确靶向（arex_widget_set_value）

```c
void arex_widget_set_value(arex_widget_id_t id, float value)
{
    /* 遍历两个容器（5F 卡片 + 左侧锚点） */
    lv_obj_t *containers[2] = { g_card_custom_obj, g_left_anchor_obj };

    for (uint8_t c = 0; c < 2; c++) {
        lv_obj_t *container = containers[c];
        if (!container) continue;

        int16_t child_cnt = lv_obj_get_child_cnt(container);
        for (int16_t i = 0; i < child_cnt; i++) {
            lv_obj_t *child = lv_obj_get_child(container, i);
            if (!child) continue;

            uintptr_t child_tag = (uintptr_t)lv_obj_get_user_data(child);

            /* ===== POD 单模具：根据标签 1033/2033 精确匹配 ===== */
            if (id == WIDGET_POD_0806) {
                if (child_tag == POD1_TAG || child_tag == POD2_TAG) {
                    /* 遍历子节点，找 user_data 等于容器标签的 label */
                    int16_t sub_cnt = lv_obj_get_child_cnt(child);
                    for (int16_t j = 0; j < sub_cnt; j++) {
                        lv_obj_t *sub = lv_obj_get_child(child, j);
                        if (!sub) continue;
                        if ((uintptr_t)lv_obj_get_user_data(sub) == child_tag) {
                            if (lv_obj_check_type(sub, &lv_label_class)) {
                                char buf[32];
                                snprintf(buf, sizeof(buf), "%.0f", (double)value);
                                lv_label_set_text(sub, buf);
                            }
                            break;
                        }
                    }
                }
                continue;
            }

            /* 其他组件 ... */
        }
    }
}
```

### 38.8 APP 交互全链路解析

#### 用户在手机 APP 上如何操作？

**第一步：先创建组件（放模具）**
用户从组件库里拖了一个【气瓶压力】(POD_0806) 的小方块，放到左上角。
接着，用户又拖了一个【气瓶压力】(POD_0806) 放到右上角。
此时，APP 知道有两个模具，但不知道它们对应哪个真实的物理气瓶。

**第二步：再绑 ID（定身份）**
用户点击左上角的那个气瓶方块，APP 弹出列表："请选择绑定的气瓶发射器"。
用户选择了 `[气瓶发射器 MAC: A1-B2 (呼吸气)]`。
用户点击右上角的方块，选择了 `[气瓶发射器 MAC: C3-D4 (减压气)]`。

#### APP 下发数据

**数据 A：排版布局帧**
```json
{
  "widget_ids": [33, 33],  // 两个 POD_0806
  "widget_positions": [
    { "x": 0, "y": 0, "w": 1, "h": 1 },  // 左上角
    { "x": 1, "y": 0, "w": 1, "h": 1 }   // 右上角
  ]
}
```

**数据 B：传感器绑定协议帧**
APP 偷偷告诉手表底层的蓝牙模块：
"喂，MAC A1-B2 这个发射器，你给我当作通道 1 (POD 1) 处理；
MAC C3-D4 当作通道 2 (POD 2) 处理！"

### 38.9 数据流闭环

```
蓝牙底层连上了 A1-B2，压力变成了 200bar。
硬件写入 g_sensor_data.pod1_bar = 200。

MCU 触发 UI 刷新。
UI 引擎去找被打上 1033 (POD 1) 烙印的 Label，更新文字为 "200"。

这个 Label 恰好就是用户在 APP 左上角放的那个组件！
```

### 38.10 架构优势

| 对比项 | 虚拟 ID 方案 | 单模具+轮转计数器 |
|--------|-------------|------------------|
| 枚举数量 | 需要额外定义 100/101 | 无需额外定义 |
| 样式查表 | 需要重定向逻辑 | 直接查 POD_0806 |
| APP 认知 | 33/100/101 三种 ID | 只有 33 一种 ID |
| 代码复杂度 | 高（多处重定向） | 低（单一模具） |
| 概念清晰度 | 需要理解虚拟 ID | 直观易懂 |

### 38.11 变更记录

| 日期 | 文件 | 变更 |
|------|------|------|
| 2026-05-01 | `arex_ui_engine.h` | 删除 `WIDGET_POD1_VIRTUAL`、`WIDGET_POD2_VIRTUAL` 枚举；整理分组 |
| 2026-05-01 | `arex_ui_engine.c` | 新增 `POD_TAG_BASE`、`POD1_TAG`、`POD2_TAG` 宏定义 |
| 2026-05-01 | `arex_ui_engine.c` | 删除 `arex_get_widget_style()` 中的虚拟 ID 重定向逻辑 |
| 2026-05-01 | `arex_ui_engine.c` | 重构 `render_widget_by_id()` 内部 POD 拦截逻辑 |
| 2026-05-01 | `arex_ui_engine.c` | 重构 `arex_widget_set_value()` POD 精确靶向逻辑 |
| 2026-05-01 | `arex_ui_engine.c` | 删除 `arex_render_widget()` 中的旧版虚拟 ID 路由 |
| 2026-05-01 | `AREX_ARCH.md` | 替换 Section 38.x 为新 POD 单模具架构文档 |
