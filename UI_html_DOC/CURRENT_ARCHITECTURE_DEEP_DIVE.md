# 当前 UI 架构深度讲解

本文档按“代码如何运行起来、数据如何流动、界面如何创建和刷新、菜单和告警如何协作”的顺序解释当前架构。它比 `UI_MODULE_MAP.md` 更偏向完整逻辑和改代码时的思考路径。

## 1. 一句话总览

这是一个基于 LVGL v8.3 的潜水电脑 UI。PC 版通过 Windows GDI 模拟 640x480 屏幕，真实设备侧计划对接 RT-Thread 和硬件/算法后端。

当前核心设计可以概括为：

```text
输入/算法/传感器只写 Data Bus
UI 定时任务消费 dirty mask
Router 把 dirty mask 翻译成 VM
Screen/Layout/Component/Card 把 VM 渲染成 LVGL 对象
Menu/Alarm 通过独立状态机管理交互和提醒
```

这意味着上游业务层不应该直接操作 LVGL 控件，UI 层也不应该直接改传感器全局数据。两边通过 `bus_set_*()`、`bus_get_*()` 和 dirty mask 解耦。

## 2. 目录分层

```mermaid
flowchart TB
    Root["src/ui"]
    Root --> Core["core<br/>全局配置、数据总线、UI 状态机、VM、刷新路由"]
    Root --> Screen["screen<br/>根屏、safe zone、tileview、布局重建、覆盖层、编辑态"]
    Root --> Comp["comp<br/>可复用 widget 工厂、样式字典、组件数据同步"]
    Root --> Views["views<br/>子菜单、菜单定义/运行时/动作、弹窗、Dive Plan 子状态"]
    Root --> Alarm["alarm<br/>告警事件引擎和告警视觉"]
    Root --> Cards["cards<br/>右侧业务卡片，如 compass/deco/gas/plan/custom/blank"]
    Root --> Menus["menus<br/>右侧顶层 INFO MENU 和 DIVE MENU"]
    Root --> Fonts["fonts<br/>字体资源和字体 ID 映射"]
    Root --> Picture["picture<br/>图片资源，如升降速率、气瓶、手电图标"]
```

PC 模拟器和算法模拟层在 `src/hal_sim`、`src/algo_sim`。这些模块是 PC 专用能力，共享 UI/core 代码引用它们时必须有 `#ifdef PC_SIMULATOR` 边界。

## 3. 启动流程

入口从 Windows 程序开始：

```mermaid
sequenceDiagram
    participant Main as main.c<br/>WinMain
    participant LVGL as LVGL/Win32 Driver
    participant UI as ui_main.c<br/>UI_main
    participant Engine as core/ui_engine.c
    participant Screen as screen/screen.c
    participant State as core/ui_state.c
    participant Input as hal_sim/input_pc.c
    participant Sim as hal_sim/sim_data.c

    Main->>LVGL: lv_init()
    Main->>LVGL: lv_win32_init(640, 480)
    Main->>UI: UI_main()
    UI->>Engine: ui_init()
    Engine->>Engine: sys_config_defaults() when needed
    Engine->>Engine: data_init()
    Engine->>Engine: alarm_init()
    UI->>Screen: screen_create()
    UI->>State: ui_state_init()
    UI->>State: UI_DASH + PAGE_POS_DYNAMIC_FIRST
    UI->>Engine: ui_bootstrap_force_first_paint()
    UI->>Input: input_init(scr) if PC_SIMULATOR
    UI->>Screen: screen_scroll_to_page(first dynamic page)
    UI->>LVGL: lv_timer_create(ui_update_task, 50ms)
    UI->>Sim: sim_data_start() if PC_SIMULATOR
    Main->>LVGL: loop lv_task_handler()
```

启动时有一个重要细节：`ui_bootstrap_force_first_paint()` 会主动重排一次布局并补一轮数据脏标记。这样 UI 不依赖传感器首帧才显示出来。即使硬件数据还没有到，屏幕对象树、左侧组件和当前页也会先有完整结构。

## 4. 全局数据和配置

当前 UI 有两个关键全局单例：

| 对象 | 定义位置 | 责任 |
|---|---|---|
| `g_sys_config` | `core/ui_engine.c` | 布局、卡片顺序、用户设置、默认气体、亮度、告警阈值等配置 |
| `g_sensor_data` | `core/ui_engine.c` | 深度、NDL、TTS、气体、电量、温度、组织、轨迹等运行时数据，以及 `dirty_mask` |

外部模块不要直接写这两个全局变量。正确入口是：

```text
硬件/算法/模拟器 -> bus_set_*()
UI/组件/VM       -> bus_get_*()
刷新任务          -> bus_take_dirty()
```

原因是 `bus_set_*()` 不只是赋值，它还负责：

| 动作 | 例子 |
|---|---|
| 变化过滤 | `bus_set_depth()` 对显示深度做 0.05m 防抖 |
| 派生统计 | 深度更新时同步最大深度、平均深度 |
| dirty mask | 深度变化打 `DIRTY_DEPTH`，气体变化打 `DIRTY_GAS` |
| 临界区保护 | 组织数组、减压站数组等大块数据写入会关中断保护 |
| 算法同步 | PC 模拟下气体/GF/盐度等会同步给 `buhlmann_debug` |

注意：告警不是 Data Bus 的隐式副作用。`bus_set_*()` 只更新数据和脏标记；告警应由算法、传感器、平台或调试层显式调用 `alarm_set_active()`、`alarm_raise_custom()`、`alarm_clear_custom()` 或 `alarm_clear_all()`。

## 5. 数据刷新主链路

```mermaid
flowchart LR
    Producer["传感器 / 算法 / BLE / PC 模拟器"] --> BusSet["bus_set_*()"]
    BusSet --> Sensor["g_sensor_data<br/>数据 + dirty_mask"]
    Sensor --> Timer["ui_update_task()<br/>50ms LVGL timer"]
    Timer --> AlarmView["alarm_view_tick()<br/>每帧推进告警视觉"]
    Timer --> Async["submenu_dive_plan_poll_async()<br/>计划计算轮询"]
    Timer --> Take["bus_take_dirty()"]
    Take --> Router["ui_update_router_dispatch(mask)"]
    Router --> VM["ui_vm_*_update()<br/>把原始数据转成显示模型"]
    VM --> Comp["comp_refresh_* / comp_sync_data"]
    VM --> Cards["card_*_update / page_registry_update_*"]
    Comp --> LVGL["lv_label_set_text / lv_obj_invalidate"]
    Cards --> LVGL
```

刷新的关键是 dirty mask：

| dirty bit | 典型刷新对象 |
|---|---|
| `DIRTY_DEPTH` | 深度组件、最大/平均深度、DECO 卡、信息子菜单 |
| `DIRTY_NDL` / `DIRTY_NDL_STOP` | NDL/安全停留/减压停留复合组件 |
| `DIRTY_ASCENT` | 升降速率图标和速率文本 |
| `DIRTY_GAS` | 左侧 GAS、GAS 卡片、PPO2/MOD 相关组件、信息子菜单 |
| `DIRTY_TRAJECTORY` | PLAN 卡片轨迹和减压站图 |
| `DIRTY_TISSUES` | 组织柱图和 DECO 卡 |
| `DIRTY_UI_LAYOUT` | 全屏重建：tileview、左侧锚点、右侧卡片、子菜单/弹窗层 |

## 6. VM 层是什么

VM 是 View Model。它把 `g_sensor_data`、`g_sys_config`、`ui_state` 里的原始状态转成“界面刚好需要的结构”。

```mermaid
flowchart TB
    Raw["原始数据<br/>bus_get_* / g_sys_config / ui_state"] --> VM["core/vm/ui_vm_*.c"]
    VM --> Text["格式化文本<br/>如 21.3, 45%, 01:24"]
    VM --> Flags["显示状态<br/>如 low battery, in stop zone, flash_on"]
    VM --> Layout["菜单/弹窗数据<br/>标题、行、badge、编辑规格"]
    Text --> View["comp/card/menu/modal"]
    Flags --> View
    Layout --> View
```

VM 层的好处：

| 没有 VM 时 | 有 VM 后 |
|---|---|
| 每个卡片自己读数据、自己格式化 | 统一格式化，卡片只负责显示 |
| 同一个值在不同地方显示格式可能不一致 | `ui_vm_value_text_update()` 统一文本规则 |
| Router 里塞满字符串拼接和 UI 细节 | Router 只决定刷新谁，VM 决定显示成什么 |
| 菜单、弹窗、组件各自理解业务状态 | 复杂状态先汇总成 VM，再交给视图 |

常见 VM 文件：

| 文件 | 作用 |
|---|---|
| `core/vm/ui_vm_dashboard.*` | 仪表盘组件、罗盘、气体、DECO、SYS、NDL_STOP、升降速率、通用文本 |
| `core/vm/ui_vm_menu.*` | 顶层菜单、气体切换菜单、设置菜单、编辑规格、弹窗数据 |
| `core/vm/ui_vm_plan_chart.*` | PLAN 卡片轨迹和减压站曲线数据 |
| `core/vm/ui_vm_plan_view.*` | Dive Plan 子菜单页面的输入和结果 |
| `core/vm/ui_vm_system_view.*` | 子菜单容器、软件亮度、左侧辅助温度槽 |
| `core/vm/ui_vm_info.*` | 信息页显示模型 |

## 7. 屏幕对象树

当前屏幕不是一堆固定坐标控件，而是一个“安全区 + 左锚点 + 右 tileview + 覆盖层”的对象树。

```mermaid
flowchart TB
    Root["s_scr<br/>LVGL root screen"] --> Safe["s_safe_zone<br/>580x420 默认安全区"]
    Root --> Bright["s_brightness_overlay<br/>软件亮度遮罩"]
    Safe --> Left["s_left_anchor<br/>固定信息锚点"]
    Safe --> Right["s_right_cont<br/>右侧内容裁剪容器"]
    Safe --> Dots["s_dot_cont<br/>滚动点"]
    Right --> Tileview["s_tileview<br/>垂直 tileview"]
    Tileview --> Info["display pos 0<br/>INFO MENU"]
    Tileview --> Dynamic["display pos 1..N<br/>动态业务页"]
    Tileview --> Setup["last display pos<br/>DIVE MENU"]
    Right --> WallTop["wall top<br/>边界蓄力提示"]
    Right --> WallBottom["wall bottom<br/>边界蓄力提示"]
    Right --> Submenu["submenu_view<br/>子菜单抽屉"]
    Right --> Modal["modal_view<br/>确认弹窗"]
```

主要文件拆分：

| 文件 | 责任 |
|---|---|
| `screen/screen.c` | 屏幕门面、根对象、样式、导航、wall、菜单选中态、通用刷新入口 |
| `screen/screen_layout.c` | 左锚点、右面板、tileview 创建，布局重建和整页重建 |
| `screen/screen_dots.c` | 滚动点显示与高亮 |
| `screen/screen_edit.c` | 数值编辑态显示、提交、取消 |
| `screen/screen_overlay.c` | 软件亮度遮罩和 safe zone 获取 |
| `screen/layout_view.c` | 几何计算、左侧 2x7 网格、右侧 5F 网格、菜单行渲染辅助 |
| `screen/page_registry.c` | 页面注册表、显示位置和存储位置映射 |

## 8. 页面注册和动态顺序

右侧页面不直接按数组下标硬编码。`g_sys_config.card_order[]` 存“配置槽位”，`page_registry` 把它压缩成 tileview 的“显示位置”。

```mermaid
flowchart LR
    Config["g_sys_config.card_order[]<br/>存储槽位"] --> Registry["page_registry"]
    Registry --> Visible["display_pos<br/>连续显示位置"]
    Visible --> Tile["s_tile_objs[display_pos]"]
    Registry --> Page["page_t<br/>id/title/engine/create/update"]
    Page --> EngineType{"engine_type"}
    EngineType --> Grid["PAGE_ENGINE_GRID<br/>render_5f_custom_grid"]
    EngineType --> Menu["PAGE_ENGINE_MENU<br/>menu_info/menu_setup"]
    EngineType --> Custom["PAGE_ENGINE_CUSTOM<br/>card_compass/card_deco/card_gas/card_plan"]
```

几个重要概念：

| 概念 | 含义 |
|---|---|
| `page_id_t` | 页面身份，如 `PAGE_ID_COMPASS`、`PAGE_ID_PLAN` |
| `storage_pos` | `card_order[]` 中的原始槽位，可能包含 `UNUSED` 或 `BLANK` |
| `display_pos` | tileview 中实际显示的连续序号 |
| `PAGE_POS_INFO` | INFO MENU 固定在首位 |
| `PAGE_POS_SETUP` | DIVE MENU 固定在配置末位，但显示位置会跟随动态页数量变化 |
| `custom_card_slot[]` | 某个 CUSTOM_GRID 槽位映射到哪一张 `custom_cards[]` |

因此，状态机和屏幕层需要查页面时应调用 `page_id_at(display_pos)`，不要直接拿 `display_pos` 当 `card_order[]` 下标。

## 9. 布局系统

当前默认是 TECH 布局：

```text
safe zone 580x420
left anchor 160x420
right content 410x420 左右
中间 panel_gap 默认 10px
```

左侧固定信息区使用 `left_widgets[]` 配置：

```mermaid
flowchart TB
    Config["g_sys_config.left_widgets[]<br/>widget_id + x/y"] --> Layout["render_left_anchor_grid()"]
    Layout --> Style["comp_get_style(widget_id)<br/>span_w/span_h/font/elements"]
    Style --> Factory["render_widget_by_id()"]
    Factory --> LVGL["LVGL widget object<br/>user_data 烙印 comp_id"]
```

右侧 CUSTOM_GRID 使用 `custom_cards[].widgets[]` 配置：

```mermaid
flowchart TB
    Slot["card_order[pos] = PAGE_ID_CUSTOM_GRID"] --> Map["custom_card_slot[pos]"]
    Map --> Card["g_sys_config.custom_cards[index]"]
    Card --> Widgets["widgets[]<br/>widget_id + x/y"]
    Widgets --> Grid["render_5f_custom_grid()"]
    Grid --> Factory["render_widget_by_id()"]
```

布局和刷新是分开的。布局负责“这个 widget 放在哪里、尺寸多大”，刷新负责“这个 widget 显示什么数据”。

## 10. 组件系统

组件系统由三块组成：

| 模块 | 作用 |
|---|---|
| `comp_style.*` | 样式字典：组件 span、字体、标题、单位、元素掩码、特殊布局参数 |
| `comp_view.*` | 组件工厂：按 `comp_id_t` 创建 LVGL 对象并打 `user_data` 烙印 |
| `comp_update.*` | 组件刷新：按 `comp_id_t` 查找对象并写入 VM 格式化后的文本或专用状态 |

核心机制是“烙印”：

```mermaid
sequenceDiagram
    participant Layout as layout_view
    participant Factory as comp_view<br/>render_widget_by_id
    participant Obj as LVGL object
    participant Update as comp_update

    Layout->>Factory: widget_id + abs rect + span
    Factory->>Obj: create container and labels
    Factory->>Obj: lv_obj_set_user_data(comp_id_t)
    Update->>Obj: traverse left/custom containers
    Update->>Obj: find user_data == comp_id_t
    Update->>Obj: lv_label_set_text()
```

复杂组件有专门路径：

| 组件 | 特殊点 |
|---|---|
| `COMP_DEPTH_1612` | 大深度整数、小数、单位、升降速率图标分开排版 |
| `COMP_NDL_STOP_1606` | 同一个组件在 NDL、安全停留、减压停留三种状态间切换 |
| `COMP_TISSUE_RAW_4012` / `COMP_TISSUE_GF_4012` | 通过 LVGL draw callback 绘制 16 根组织柱 |
| `COMP_POD_0806` | 单模具复用，渲染顺序轮转成 POD1/POD2，并用 1033/2033 tag 区分 |
| `COMP_SYS_1606` | 电量和温度使用静态句柄快速刷新 |

## 11. 输入和 UI 状态机

PC 输入由 `hal_sim/input_pc.c` 转成三类事件：

```text
rotate(+1/-1)
click
back
```

状态机在 `core/ui_state.c`。

```mermaid
stateDiagram-v2
    [*] --> UI_DASH
    UI_DASH --> UI_INFO: 左边界旋转蓄力 3 次
    UI_DASH --> UI_SETUP: 右边界旋转蓄力 3 次
    UI_DASH --> UI_EDIT_GAS: GAS 页 click
    UI_DASH --> UI_MODAL_COMPASS: COMPASS 页二次 click
    UI_INFO --> UI_SUB_MENU: click item
    UI_SETUP --> UI_SUB_MENU: click item
    UI_SUB_MENU --> UI_EDIT_VALUE: select editable item
    UI_SUB_MENU --> UI_MODAL_SETUP_CONFIRM: select confirm item
    UI_SUB_MENU --> UI_MODAL_GAS: select gas slot
    UI_EDIT_GAS --> UI_MODAL_GAS: click selected gas
    UI_MODAL_GAS --> UI_DASH: confirm gas switch from card
    UI_MODAL_GAS --> UI_SUB_MENU: cancel/finish from submenu context
    UI_EDIT_VALUE --> UI_SUB_MENU: click commit or back cancel
    UI_SUB_MENU --> UI_INFO: back to info parent
    UI_SUB_MENU --> UI_SETUP: back to setup parent
    UI_INFO --> UI_DASH: back or bottom wall
    UI_SETUP --> UI_DASH: back or top wall
```

同一个输入在不同状态下含义不同：

| 状态 | rotate | click | back |
|---|---|---|---|
| `UI_DASH` | 翻动态页，边界蓄力进菜单 | 特定卡片动作，如 GAS/COMPASS | 通常无动作 |
| `UI_INFO` | 移动 INFO 菜单光标 | 打开信息子菜单 | 回 DASH |
| `UI_SETUP` | 移动 DIVE MENU 光标 | 打开设置子菜单 | 回 DASH |
| `UI_SUB_MENU` | 移动子菜单光标或 Dive Plan 内部编辑 | 执行 row action | 返回父级 |
| `UI_EDIT_VALUE` | 按 step 改数值 | 提交 | 取消 |
| `UI_MODAL_*` | 通常不动或 pulse | 确认 | 取消/关闭 |

## 12. 菜单系统

菜单分为四层：

```mermaid
flowchart TB
    Defs["menu_defs<br/>稳定 ID、标题、入口表、child 映射"] --> Runtime["menu_runtime<br/>当前菜单、父级栈、当前 rows"]
    Model["submenu_model<br/>动态文案、设置值、编辑规格、复杂菜单模型"] --> Runtime
    Runtime --> View["submenu_view<br/>渲染抽屉、焦点、动画"]
    View --> Actions["menu_actions<br/>按 menu_item_id_t 分发业务动作"]
    Actions --> State["ui_state<br/>切换状态/编辑上下文"]
    Actions --> Bus["bus_set_* / callbacks / requests"]
    Actions --> Screen["screen facade<br/>弹窗、刷新、关闭"]
```

菜单选择路径禁止依赖字符串判断。显示字符串只用于 LVGL label；业务逻辑必须基于：

```text
menu_id_t
menu_item_id_t
menu_row_type_t
submenu_setting_kind_t
```

典型打开流程：

```mermaid
sequenceDiagram
    participant State as ui_state
    participant Screen as screen facade
    participant View as submenu_view
    participant Runtime as menu_runtime
    participant Defs as menu_defs
    participant Model as submenu_model

    State->>Screen: screen_open_setup_submenu(menu_setup_idx)
    Screen->>View: submenu open
    View->>Runtime: menu_runtime_open_setup(index)
    Runtime->>Defs: index -> menu_id_t
    Runtime->>Model: build dynamic rows when needed
    Runtime-->>View: title + menu_row_t[]
    View->>View: render title/rows/selection
    Screen->>State: UI_SUB_MENU
```

典型选择流程：

```mermaid
sequenceDiagram
    participant State as ui_state
    participant Screen as screen facade
    participant View as submenu_view
    participant Runtime as menu_runtime
    participant Actions as menu_actions
    participant Bus as data/callback/request

    State->>Screen: screen_handle_submenu_select(row_index)
    Screen->>View: forward select
    View->>Runtime: row_at(row_index)
    View->>Actions: menu_actions_handle_select(row.id, row.type)
    Actions->>Bus: apply setting or request command
    Actions-->>View: MENU_ACTION_*
    View->>Screen: open child / begin edit / show modal / refresh / close
```

## 13. 告警系统

告警分为引擎和视图：

| 模块 | 责任 |
|---|---|
| `alarm/alarm.c` | 告警定义表、active 状态、优先级、ACK、自动超时、横幅轮播 |
| `alarm/alarm_view.c` | 横幅创建、颜色/动画、靶向组件闪烁、样式恢复 |

```mermaid
flowchart LR
    Owner["算法 / 传感器 / 平台 / 调试层"] --> API["alarm_set_active()<br/>alarm_raise_custom()<br/>alarm_clear_*()"]
    API --> Engine["alarm.c<br/>active table + priority + display"]
    Timer["ui_update_task() 每 50ms"] --> View["alarm_view_tick(ctx)"]
    View --> Engine
    Engine --> Display["alarm_display_t<br/>当前横幅"]
    Engine --> Targets["target comp_id_t[]"]
    View --> Banner["safe zone 横幅"]
    View --> Blink["左锚点和 5F 目标组件闪烁"]
```

告警等级和策略：

| 等级 | 行为 |
|---|---|
| `ALARM_CRIT` | 最高优先级，通常条件解除前强制驻留 |
| `ALARM_WARN` | 可 ACK 隐藏，但条件解除前不重新触发同一个提示 |
| `ALARM_INFO` | 信息提示，可自动超时 |

靶向闪烁依赖组件创建时的 `user_data` 烙印。比如 `ALARM_ID_WARN_NDL_LOW` 的 target 是 `COMP_NDL_STOP_1606`，告警视图会在左侧锚点和所有 CUSTOM_GRID 容器中找这个 comp id，然后统一闪烁。

## 14. PC 模拟器边界

PC 专用模块包括：

| 模块 | 作用 |
|---|---|
| `src/hal_sim/input_pc.*` | 键盘/旋钮输入模拟 |
| `src/hal_sim/sim_data.*` | 定时模拟传感器和潜水数据 |
| `src/hal_sim/debug_link_pc.*` | PC 调试链路 |
| `src/algo_sim/buhlmann_debug.*` | PC 侧 Buhlmann 调试算法后端 |

共享 UI/core 代码不能无保护地 include 这些模块。需要 PC 钩子时，应像当前 `ui_main.c` 和 `data.c` 一样：

```c
#ifdef PC_SIMULATOR
#include "hal_sim/input_pc.h"
#endif
```

DIVE PLAN 不是 PC-only。UI 侧应调用中性的 `dive_plan_backend_calculate()` 后端接口；PC 模拟可以由 `buhlmann_debug` 实现，嵌入式构建应替换为真实算法实现。

## 15. 例子 1：深度从算法层刷新到屏幕

假设算法或模拟器得到深度 `21.3m`。

```mermaid
sequenceDiagram
    participant Sim as sim/algo
    participant Bus as data.c
    participant Timer as ui_update_task
    participant Router as update_router
    participant VM as ui_vm_dashboard
    participant Comp as comp_update/comp_view
    participant Card as card_deco
    participant LVGL as LVGL objects

    Sim->>Bus: bus_set_depth(21.3)
    Bus->>Bus: update depth/max/avg
    Bus->>Bus: dirty_mask |= DIRTY_DEPTH
    Timer->>Bus: bus_take_dirty()
    Bus-->>Timer: DIRTY_DEPTH
    Timer->>Router: ui_update_router_dispatch(DIRTY_DEPTH)
    Router->>VM: ui_vm_deco_update()
    Router->>Card: card_deco_update()
    Router->>VM: ui_vm_ascent_update()
    Router->>Comp: comp_refresh_ascent_icons()
    Router->>VM: ui_vm_value_text_update(COMP_DEPTH_MAX_0806)
    Router->>Comp: comp_set_text()
    Router->>Comp: screen_refresh_all_widgets()
    Comp->>LVGL: update DEPTH label and related widgets
```

这条链路里没有任何模拟器代码直接碰 LVGL。数据写入和 UI 刷新通过 dirty mask 延迟解耦。

## 16. 例子 2：用户在 GAS 卡切换气体

流程不是“点一下立刻改气体”，而是先进入选择态，再二次确认。

```mermaid
sequenceDiagram
    participant User as 用户
    participant State as ui_state
    participant Screen as screen
    participant Modal as modal_view
    participant Algo as 算法/任务层
    participant Bus as data.c
    participant Router as update_router

    User->>State: GAS page click
    State->>State: UI_DASH -> UI_EDIT_GAS
    State->>Screen: screen_refresh_gas_menu()
    User->>State: rotate
    State->>State: gas_cursor changes
    State->>Screen: screen_refresh_gas_menu()
    User->>State: click
    State->>Modal: screen_show_modal_gas()
    User->>State: confirm
    State->>State: request_gas_switch(ci)
    Algo->>State: has_pending_gas_switch()
    Algo->>Bus: bus_set_gas(new_idx, name)
    Bus->>Bus: dirty_mask |= DIRTY_GAS
    Router->>Screen: screen_refresh_gas_menu()
    Router->>Screen: screen_refresh_all_widgets()
```

注意：UI 只提交“请求切到哪一路气体”。真正气体状态由算法/任务层执行后再通过 Data Bus 反向更新 UI。

## 17. 例子 3：APP/BLE 下发布局配置

假设 APP 下发新 `card_order[]` 或左侧 widget 配置。

```mermaid
sequenceDiagram
    participant BLE as BLE/App
    participant Bus as data.c
    participant Timer as ui_update_task
    participant Router as update_router
    participant Screen as screen_layout
    participant Registry as page_registry
    participant Layout as layout_view
    participant Comp as comp_view/update

    BLE->>Bus: bus_set_ui_layout(payload)
    Bus->>Bus: validate version
    Bus->>Bus: update g_sys_config.card_order/left_widgets/custom_cards
    Bus->>Bus: dirty_mask |= DIRTY_UI_LAYOUT
    Timer->>Router: dispatch(DIRTY_UI_LAYOUT)
    Router->>Screen: screen_rebuild_full()
    Screen->>Registry: page_count/page_get/page_storage_pos
    Screen->>Screen: rebuild tileview/right panel/submenu/modal
    Screen->>Layout: render_left_anchor_grid()
    Screen->>Layout: render_5f_custom_grid()
    Layout->>Comp: render_widget_by_id()
    Screen->>Comp: screen_refresh_all_widgets()
    Router->>Bus: requeue remaining dirty bits
```

`DIRTY_UI_LAYOUT` 会优先处理，因为布局重建可能销毁旧 LVGL 对象。如果先刷新数据再重建，刷新路径可能访问已经失效的对象。

## 18. 例子 4：修改一个简单设置项

以亮度菜单为例，用户在 DIVE MENU 中进入 `BRIGHTNESS`，选择 `HIGH`。

```mermaid
sequenceDiagram
    participant User as 用户
    participant State as ui_state
    participant Screen as screen/submenu facade
    participant Runtime as menu_runtime
    participant Actions as menu_actions
    participant Bus as data.c
    participant Overlay as screen_overlay

    User->>State: click setup item
    State->>Screen: screen_open_setup_submenu(index)
    Screen->>Runtime: open MENU_SETUP_BRIGHTNESS
    Runtime-->>Screen: rows with MENU_ITEM_BRIGHTNESS_*
    User->>State: select HIGH
    State->>Screen: screen_handle_submenu_select(row)
    Screen->>Actions: menu_actions_handle_select()
    Actions->>Bus: bus_set_brightness(BRIGHTNESS_HIGH)
    Actions-->>Screen: MENU_ACTION_CLOSE or REFRESH
    Screen->>Overlay: apply_software_brightness(level)
```

业务分发依据 `MENU_ITEM_BRIGHTNESS_HIGH`，不是 `"HIGH"` 这个显示字符串。

## 19. 例子 5：告警触发和 ACK

假设算法判断 PPO2 超限：

```mermaid
sequenceDiagram
    participant Algo as 算法/传感器层
    participant Alarm as alarm.c
    participant Timer as ui_update_task
    participant View as alarm_view.c
    participant User as 用户
    participant State as ui_state

    Algo->>Alarm: alarm_set_active(ALARM_ID_CRIT_PO2_MAX, true)
    Timer->>View: alarm_view_tick(ctx)
    View->>Alarm: alarm_tick(now)
    View->>Alarm: alarm_get_display()
    Alarm-->>View: level/text/target COMP_PPO2_0806
    View->>View: show banner + blink target widgets
    User->>State: click/back/rotate
    State->>Alarm: alarm_ack_current()
    Alarm-->>State: true/false according to clear policy
    Algo->>Alarm: alarm_set_active(ALARM_ID_CRIT_PO2_MAX, false)
    Timer->>View: next tick restores target styles
```

CRIT 类型通常不是 ACK 后直接消失，而是需要条件解除；WARN 类型可 ACK 隐藏；INFO 类型可自动超时。

## 20. 改代码时的定位方法

| 目标 | 优先看哪里 |
|---|---|
| 程序怎么启动 | `main.c` -> `src/ui_main.c` -> `core/ui_engine.c` |
| 某个传感器值怎么显示 | `data.c bus_set_*` -> `update_router.c` -> `core/vm/*` -> `comp_update.c` |
| 左侧固定区怎么改 | `sys_config_defaults()` 的 `left_widgets[]`、`layout_view.c`、`comp_style.c` |
| 右侧新增卡片 | `cards/card_*.c`、`page_registry.c/h`、必要时 `ui_vm_*` |
| 调整页面顺序 | `g_sys_config.card_order[]` 和 `page_registry` 的 display/storage 映射 |
| 新增通用 widget | `comp_style_types.h`、`comp_style.c`、`comp_view.c`、`comp_update.c`、VM 文本 |
| 修改顶层 INFO/DIVE MENU | `menus/menu_info.c`、`menus/menu_setup.c` |
| 修改子菜单结构 | `views/menu_defs.c`、`views/menu_runtime.c` |
| 修改子菜单动作 | `views/menu_actions.c`、`views/submenu_model.c` |
| 修改数值编辑 | `screen/screen_edit.c`、`submenu_model.c`、`ui_vm_menu.c` |
| 修改告警事件 | `alarm/alarm.c/h` |
| 修改告警视觉 | `alarm/alarm_view.c/h` |
| 修改 PC 输入/模拟数据 | `src/hal_sim`，共享代码引用时加 `#ifdef PC_SIMULATOR` |

## 21. 两条最重要的维护边界

第一，数据边界：

```text
外部输入 -> bus_set_* -> dirty mask -> ui_update_task -> router -> VM -> view
```

不要让传感器、算法或 PC 模拟器直接操作 LVGL 对象。

第二，菜单边界：

```text
menu_id_t / menu_item_id_t / row type -> action
display string -> label only
```

不要用菜单标题或行文本做业务分发。文案以后可能改，ID 才是稳定协议。
