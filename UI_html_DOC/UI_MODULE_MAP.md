# UI 模块地图

本文档说明 `src/ui/` 当前拆分后的文件职责、调用边界和主要数据流，偏向“现在看代码时应该先看哪里”。

## 目录分层

```text
src/ui/
├─ core/      数据总线、全局状态、UI 状态机、刷新路由、业务回调
├─ screen/    屏幕门面、布局计算、页面注册表
├─ comp/      可复用组件工厂、组件刷新、组件样式
├─ views/     弹窗、子菜单抽屉、菜单定义/运行时/动作层
├─ alarm/     告警事件引擎与告警视图
├─ cards/     右侧业务卡片
├─ menus/     右侧顶层菜单页
├─ fonts/     字体资源
└─ picture/   图标资源
```

## 总览

```mermaid
flowchart TD
    UI["ui_main.c<br/>UI 入口"] --> Engine["ui_engine.c/h<br/>全局配置与主循环"]
    Engine --> Data["data.c/h<br/>数据总线写入 API"]
    Engine --> State["ui_state.c/h<br/>输入与 UI 状态机"]
    Engine --> UpdateRouter["update_router.c/h<br/>dirty mask 刷新分发"]
    Engine --> Screen["screen.c/h<br/>屏幕框架与公共门面"]

    Screen --> Layout["layout_view.c/h<br/>布局计算与网格渲染"]
    Screen --> WidgetView["comp_view.c/h<br/>组件工厂"]
    Screen --> Modal["modal_view.c/h<br/>弹窗视图"]
    Screen --> SubmenuView["submenu_view.c/h<br/>子菜单抽屉视图"]
    SubmenuView --> MenuRuntime["menu_runtime.c/h<br/>当前菜单与父级栈"]
    MenuRuntime --> MenuDefs["menu_defs.c/h<br/>菜单 ID 与显示定义"]
    SubmenuView --> MenuActions["menu_actions.c/h<br/>菜单动作分发"]
    MenuRuntime --> SubmenuModel["submenu_model.c/h<br/>动态文案与复杂编辑状态"]
    MenuActions --> SubmenuModel

    UpdateRouter --> WidgetUpdate["comp_update.c/h<br/>组件数据刷新"]
    UpdateRouter --> AlarmView["alarm_view.c/h<br/>告警横幅与靶向闪烁"]
    Data --> AlarmEngine["alarm.c/h<br/>告警事件引擎"]
    AlarmView --> AlarmEngine

    Screen --> Registry["page_registry.c/h<br/>右侧页面注册表"]
    Registry --> Cards["cards/card_*.c<br/>右侧业务卡片"]
    Registry --> Menus["menus/menu_*.c<br/>右侧顶层菜单页"]
    WidgetView --> Style["comp_style.c/h<br/>组件样式应用"]
    Style --> StyleTypes["comp_style_types.h<br/>样式枚举与配置结构"]
```

## 启动与刷新链路

```mermaid
flowchart LR
    Sim["模拟器 / BLE / 业务层"] --> Bus["bus_set_*<br/>数据写入"]
    Bus --> Sensor["g_sensor_data<br/>dirty_mask"]
    Sensor --> Tick["ui_update_task"]
    Tick --> Router["ui_update_router_dispatch(mask)"]
    Router --> Widgets["comp_update<br/>更新左侧与 5F 组件"]
    Router --> Cards["card_*_update / menu_*_update<br/>更新右侧页面"]
    Router --> AlarmView["alarm_view_tick<br/>告警横幅和靶向闪烁"]
    Router --> ScreenRebuild["screen_rebuild_*<br/>布局或卡片重建"]
```

## 屏幕拆分

`screen.c` 现在更像一个“屏幕门面”：保留根屏、safe zone、左右区域、wall、dots、亮度遮罩、公共刷新入口。具体 UI 子系统尽量拆到独立模块。

```mermaid
flowchart TB
    Screen["screen.c<br/>屏幕门面"] --> Root["根屏 / Safe Zone"]
    Screen --> Left["左侧固定锚点"]
    Screen --> Right["右侧 TileView 容器"]
    Screen --> Walls["顶部/底部 Wall 提示"]
    Screen --> Dots["滚动点指示器"]
    Screen --> Brightness["软件亮度遮罩"]

    Left --> LayoutGrid["layout_view<br/>2x7 / 5F 网格定位"]
    LayoutGrid --> WidgetFactory["comp_view<br/>组件创建"]
    WidgetFactory --> WidgetStyle["comp_style<br/>样式应用"]

    Right --> Registry["page_registry<br/>页面顺序与 tile_obj"]
    Registry --> Info["menu_info"]
    Registry --> Compass["card_compass"]
    Registry --> Deco["card_deco"]
    Registry --> Gas["card_gas"]
    Registry --> Plan["card_plan"]
    Registry --> Setup["menu_setup"]
    Registry --> Blank["card_blank"]

    Right --> Modal["modal_view<br/>确认弹窗"]
    Right --> Submenu["submenu_view<br/>子菜单抽屉"]
```

## 子菜单系统

`submenu` 是右侧菜单的二级/三级抽屉。现在它分为定义、运行时、动作和视图四层：显示文字只用于 LVGL label，业务选择必须通过 `menu_id_t` / `menu_item_id_t`。

```mermaid
sequenceDiagram
    participant Input as 输入状态机<br/>ui_state
    participant View as 抽屉视图<br/>submenu_view
    participant Runtime as 菜单运行时<br/>menu_runtime
    participant Defs as 菜单定义<br/>menu_defs
    participant Actions as 动作分发<br/>menu_actions
    participant Model as 动态文案/复杂编辑状态<br/>submenu_model
    participant Screen as 屏幕门面<br/>screen
    participant Callback as 业务回调<br/>callbacks

    Input->>View: open_info/setup_submenu(index)
    View->>Runtime: open_info/open_setup(index)
    Runtime->>Defs: index -> menu_id / item_id
    Runtime->>Model: build dynamic labels when needed
    Runtime-->>View: title + menu_row_t[]
    View->>View: render rows + slide_in

    Input->>View: handle_submenu_select(index)
    View->>Runtime: row_at(index)
    View->>Actions: handle_select(row.id)
    Actions->>Runtime: open_child/back/refresh when needed
    Actions->>Callback: apply setting / light / gas / compass
    Actions->>Screen: show modal / refresh setup badge
```

规则：`menu_runtime_current_title()` 和 `menu_runtime_current_rows()` 只服务渲染，不能再参与业务判断；禁止在选择路径里用 `strcmp(title/text)` 分发。

## 告警系统

```mermaid
flowchart TD
    DataLayer["data.c<br/>数据变化与阈值判定"] --> AlarmEngine["alarm.c<br/>21 个事件注册与 active 表"]
    Compat["bus_raise_alarm<br/>兼容临时告警入口"] --> AlarmEngine
    AlarmEngine --> Display["alarm_display_t<br/>当前最高优先级横幅"]
    AlarmEngine --> Targets["active targets<br/>所有同级靶向组件"]
    UpdateRouter["ui_update_router_tick"] --> AlarmView["alarm_view.c<br/>渲染横幅和组件闪烁"]
    AlarmView --> Display
    AlarmView --> Targets
    AlarmView --> ScreenObjects["safe_zone / left_anchor / custom_cards"]
```

## 文件职责

### 核心与数据

| 文件 | 作用 |
|---|---|
| `core/ui_engine.h` | 全局类型、配置结构、传感器结构、dirty mask、公开 UI 总线 API 声明。 |
| `core/ui_engine.c` | UI 初始化、默认配置、主刷新任务入口、全局 `g_sys_config` / `g_sensor_data` 持有者。 |
| `core/data.h` | 数据同步帧、数据写入 API、告警判定入口声明。 |
| `core/data.c` | `bus_set_*()` 数据写入实现，维护 dirty mask，并触发可判定告警条件。 |
| `core/update_router.h` | UI 刷新路由模块公开入口。 |
| `core/update_router.c` | 消费 dirty mask，分发到 widget、card、alarm、layout rebuild 等刷新路径。 |
| `core/ui_state.h` | UI 状态机枚举、输入上下文、编辑上下文、子菜单历史结构。 |
| `core/ui_state.c` | 键盘/旋钮输入路由，控制 DASH、INFO、SETUP、SUB_MENU、MODAL、EDIT 等状态切换。 |
| `core/callbacks.h` | UI 调业务层的回调声明，例如灯光、亮度、保守度。 |
| `core/callbacks.c` | PC 模拟器默认回调实现，真实业务层可替换或对接强实现。 |

### 屏幕、布局与组件

| 文件 | 作用 |
|---|---|
| `screen/screen.h` | 屏幕层公开门面，供状态机、卡片、告警、组件刷新调用。 |
| `screen/screen.c` | 根屏、safe zone、左右容器、tileview、wall、dots、亮度遮罩、公共刷新入口。 |
| `screen/layout_view.h` | 布局计算与网格渲染函数声明。 |
| `screen/layout_view.c` | safe zone 计算、左右布局计算、2x7 固定区、5F 自定义网格定位与渲染调度。 |
| `comp/comp_view.h` | 组件工厂与组件运行态句柄声明。 |
| `comp/comp_view.c` | `render_widget_by_id()`，创建 DEPTH、NDL、POD、SYS、GAS 等可复用 widget。 |
| `comp/comp_update.h` | widget 数据刷新 API 声明。 |
| `comp/comp_update.c` | 根据 widget id 同步 `g_sensor_data` 到屏幕组件，处理文本/数值更新。 |
| `comp/comp_style_types.h` | widget 样式枚举、布局元数据、样式配置结构。 |
| `comp/comp_style.h` | widget 样式应用 API 声明。 |
| `comp/comp_style.c` | 应用边框、字体、颜色、背景等组件样式。 |

### 子菜单与弹窗

| 文件 | 作用 |
|---|---|
| `views/submenu_view.h` | 子菜单抽屉创建、重置、列表句柄获取 API。 |
| `views/submenu_view.c` | 子菜单抽屉 LVGL 对象、滑入/滑出、row 渲染、焦点样式和选择事件转交；不做业务字符串判断。 |
| `views/menu_defs.h/c` | 菜单定义层：稳定 `menu_id_t` / `menu_item_id_t`、顶层 INFO/SETUP 定义表、标题和子菜单映射。 |
| `views/menu_runtime.h/c` | 菜单运行时层：当前菜单、父级栈、当前 `menu_row_t[]`、默认选中项和刷新。 |
| `views/menu_actions.h/c` | 菜单动作层：按 row ID 执行打开子菜单、设置项、确认弹窗、GAS/LIGHT/COMPASS/DIVE PLAN 入口。 |
| `views/submenu_model.h` | 菜单模型辅助 API，提供动态文案、内联编辑规格、DIVE PLAN 数据和设置值应用接口。 |
| `views/submenu_model.c` | INFO/SETUP 动态文案、复杂编辑流程与 DIVE PLAN 状态；不再按标题/行文本解析菜单业务动作。 |
| `views/modal_view.h` | 弹窗创建、显示、隐藏、pulse、上下文恢复 API。 |
| `views/modal_view.c` | GAS / COMPASS / ACT 等确认弹窗的 LVGL 对象管理与动画。 |

### 告警

| 文件 | 作用 |
|---|---|
| `alarm/alarm.h` | 21 个告警事件 ID、active/display API、target 查询接口。 |
| `alarm/alarm.c` | 告警定义表、active 状态表、优先级选择、FIFO 轮播、ACK 与清除规则。 |
| `alarm/alarm_view.h` | 告警视图上下文和 tick 渲染 API。 |
| `alarm/alarm_view.c` | 横幅创建、L1/L2/L3 视觉节拍、组件靶向闪烁和样式恢复。 |

### 卡片系统

| 文件 | 作用 |
|---|---|
| `screen/page_registry.h` | 页面 ID、页面结构、注册表 API；保留旧 `CARD_*` 兼容别名。 |
| `screen/page_registry.c` | 页面顺序、页面查找、tile 对象绑定、动态页面数量计算。 |
| `menus/menu_info.c` | INFO 顶层菜单页。 |
| `menus/menu_setup.c` | DIVE MENU 顶层菜单页和 badge 刷新。 |
| `cards/card_compass.h` | 指南针卡片对外刷新接口。 |
| `cards/card_compass.c` | 指南针页面、航向刷新、校准相关 UI 入口。 |
| `cards/card_deco.c` | 减压/组织/毒性相关页面刷新。 |
| `cards/card_gas.c` | 气体列表、气体状态和气体切换相关显示。 |
| `cards/card_plan.c` | 轨迹/计划/曲线类显示。 |
| `cards/card_blank.c` | 空白卡片占位实现。 |

### 字体与图片资源

| 文件 | 作用 |
|---|---|
| `fonts/fonts.h` | 字体 ID 到 LVGL 字体对象的统一入口。 |
| `fonts/FONT_GUIDE.md` | 字体资源说明。 |
| `fonts/lv_font_consola_*.c` | Consola 字体资源，不写业务逻辑。 |
| `fonts/lv_font_courier_*.c` | Courier 字体资源，部分符号显示使用。 |
| `fonts/lv_font_ordinar_*.c` | Ordinar 字体资源。 |
| `picture/sudo_up_level*.c` | 上升速度图标资源。 |
| `picture/sudo_down_level*.c` | 下降速度图标资源。 |
| `picture/qiping.c` | 气瓶图标资源。 |
| `picture/Shoudiantong.c` | 手电筒图标资源。 |
| `picture/liuzhuandeng.c` | 流转灯图标资源。 |

## 推荐阅读顺序

```mermaid
flowchart LR
    A["1. ui_engine.c<br/>看初始化和主循环"] --> B["2. data.c<br/>看数据怎么写入"]
    B --> C["3. update_router.c<br/>看 dirty mask 怎么刷新"]
    C --> D["4. screen.c<br/>看屏幕框架"]
    D --> E["5. layout_view.c<br/>看布局计算"]
    E --> F["6. comp_view/update<br/>看组件创建与刷新"]
    D --> G["7. menu_defs/runtime/actions<br/>看菜单定义、运行时和动作"]
    C --> H["8. alarm.c/view<br/>看告警引擎与视觉"]
    D --> I["9. cards/card_*.c / menus/menu_*.c<br/>看右侧具体页面"]
```

## 维护边界

| 需求 | 优先修改位置 |
|---|---|
| 新增传感器字段或数据写入 | `core/ui_engine.h`、`core/data.c/h` |
| 新增 dirty 刷新策略 | `core/update_router.c` |
| 新增固定区或 5F 组件 | `comp/comp_view.c`、`comp/comp_update.c`、`screen/layout_view.c` |
| 调整组件外观 | `comp/comp_style.c`、`comp/comp_style_types.h` |
| 新增右侧业务页面 | `cards/card_*.c`、`screen/page_registry.c/h` |
| 新增右侧顶层菜单页 | `menus/menu_*.c`、`screen/page_registry.c/h` |
| 修改 INFO/DIVE MENU 顶层菜单页 | `menus/menu_info.c`、`menus/menu_setup.c` |
| 修改子菜单文案、顶层入口或简单设置项 | `views/menu_defs.c`、`views/menu_runtime.c`、`views/menu_actions.c` |
| 修改子菜单动画、抽屉布局或选中态 | `views/submenu_view.c` |
| 修改告警规则或事件 | `alarm/alarm.c/h`、`core/data.c` |
| 修改告警视觉 | `alarm/alarm_view.c/h` |
| 修改弹窗显示 | `views/modal_view.c/h` |

例行源码调整不要修改 `LittlevGL.cbp`；只有明确需要同步 CodeBlocks 工程文件时再改。
