# AREX UI 模块地图

本文档说明 `src/arex_ui/` 当前拆分后的文件职责、调用边界和主要数据流。更完整的历史架构说明仍以 `UI_html_DOC/AREX_ARCH.md` 为准；本文偏向“现在看代码时应该先看哪里”。

## 目录分层

```text
src/arex_ui/
├─ core/      数据总线、全局状态、UI 状态机、刷新路由、业务回调
├─ screen/    屏幕门面、布局计算、卡片注册表
├─ widgets/   可复用组件工厂、组件刷新、组件样式
├─ views/     弹窗与子菜单抽屉/模型
├─ alarm/     告警事件引擎与告警视图
├─ cards/     右侧业务卡片
├─ fonts/     字体资源
└─ picture/   图标资源
```

## 总览

```mermaid
flowchart TD
    UI["UI_main.c<br/>AREX UI 入口"] --> Engine["arex_ui_engine.c/h<br/>全局配置与主循环"]
    Engine --> Data["arex_data.c/h<br/>数据总线写入 API"]
    Engine --> State["arex_ui_state.c/h<br/>输入与 UI 状态机"]
    Engine --> UpdateRouter["arex_ui_update_router.c/h<br/>dirty mask 刷新分发"]
    Engine --> Screen["arex_screen.c/h<br/>屏幕框架与公共门面"]

    Screen --> Layout["arex_layout_view.c/h<br/>布局计算与网格渲染"]
    Screen --> WidgetView["arex_widget_view.c/h<br/>组件工厂"]
    Screen --> Modal["arex_modal_view.c/h<br/>弹窗视图"]
    Screen --> SubmenuView["arex_submenu_view.c/h<br/>子菜单抽屉视图"]
    SubmenuView --> SubmenuModel["arex_submenu_model.c/h<br/>子菜单数据表"]

    UpdateRouter --> WidgetUpdate["arex_widget_update.c/h<br/>组件数据刷新"]
    UpdateRouter --> AlarmView["arex_alarm_view.c/h<br/>告警横幅与靶向闪烁"]
    Data --> AlarmEngine["arex_alarm.c/h<br/>告警事件引擎"]
    AlarmView --> AlarmEngine

    Screen --> Registry["arex_card_registry.c/h<br/>卡片注册表"]
    Registry --> Cards["cards/card_*.c<br/>右侧业务卡片"]
    WidgetView --> Style["arex_widget_style.c/h<br/>组件样式应用"]
    Style --> StyleTypes["arex_widget_style_types.h<br/>样式枚举与配置结构"]
```

## 启动与刷新链路

```mermaid
flowchart LR
    Sim["模拟器 / BLE / 业务层"] --> Bus["arex_bus_set_*<br/>数据写入"]
    Bus --> Sensor["g_sensor_data<br/>dirty_mask"]
    Sensor --> Tick["arex_ui_update_task"]
    Tick --> Router["arex_ui_update_router_dispatch(mask)"]
    Router --> Widgets["arex_widget_update<br/>更新左侧与 5F 组件"]
    Router --> Cards["card_*_update<br/>更新右侧卡片"]
    Router --> AlarmView["arex_alarm_view_tick<br/>告警横幅和靶向闪烁"]
    Router --> ScreenRebuild["arex_screen_rebuild_*<br/>布局或卡片重建"]
```

## 屏幕拆分

`arex_screen.c` 现在更像一个“屏幕门面”：保留根屏、safe zone、左右区域、wall、dots、亮度遮罩、公共刷新入口。具体 UI 子系统尽量拆到独立模块。

```mermaid
flowchart TB
    Screen["arex_screen.c<br/>屏幕门面"] --> Root["根屏 / Safe Zone"]
    Screen --> Left["左侧固定锚点"]
    Screen --> Right["右侧 TileView 容器"]
    Screen --> Walls["顶部/底部 Wall 提示"]
    Screen --> Dots["滚动点指示器"]
    Screen --> Brightness["软件亮度遮罩"]

    Left --> LayoutGrid["arex_layout_view<br/>2x7 / 5F 网格定位"]
    LayoutGrid --> WidgetFactory["arex_widget_view<br/>组件创建"]
    WidgetFactory --> WidgetStyle["arex_widget_style<br/>样式应用"]

    Right --> Registry["arex_card_registry<br/>卡片顺序与 tile_obj"]
    Registry --> Info["card_info"]
    Registry --> Compass["card_compass"]
    Registry --> Deco["card_deco"]
    Registry --> Gas["card_gas"]
    Registry --> Plan["card_plan"]
    Registry --> Setup["card_setup"]
    Registry --> Blank["card_blank"]

    Right --> Modal["arex_modal_view<br/>确认弹窗"]
    Right --> Submenu["arex_submenu_view<br/>子菜单抽屉"]
```

## 子菜单系统

`submenu` 是右侧菜单的二级/三级抽屉。用户在 `INFO` 或 `SETUP` 页面确认一个条目后，子菜单从右侧滑入，显示更细的详情或设置项。

```mermaid
sequenceDiagram
    participant Input as 输入状态机<br/>arex_ui_state
    participant View as 子菜单视图<br/>arex_submenu_view
    participant Model as 子菜单模型<br/>arex_submenu_model
    participant Screen as 屏幕门面<br/>arex_screen
    participant Callback as 业务回调<br/>arex_callbacks

    Input->>View: open_info_submenu(index)
    View->>Model: build_info_items(index)
    Model-->>View: title + items
    View->>View: populate + slide_in

    Input->>View: handle_submenu_select(index)
    View->>Model: nested_items_for(title)
    Model-->>View: nested items
    View->>View: open_nested_submenu

    View->>Callback: 设置亮度/灯光/保守度
    View->>Screen: 打开确认弹窗或刷新 Setup badge
```

## 告警系统

```mermaid
flowchart TD
    DataLayer["arex_data.c<br/>数据变化与阈值判定"] --> AlarmEngine["arex_alarm.c<br/>21 个事件注册与 active 表"]
    Compat["arex_bus_raise_alarm<br/>兼容临时告警入口"] --> AlarmEngine
    AlarmEngine --> Display["arex_alarm_display_t<br/>当前最高优先级横幅"]
    AlarmEngine --> Targets["active targets<br/>所有同级靶向组件"]
    UpdateRouter["arex_ui_update_router_tick"] --> AlarmView["arex_alarm_view.c<br/>渲染横幅和组件闪烁"]
    AlarmView --> Display
    AlarmView --> Targets
    AlarmView --> ScreenObjects["safe_zone / left_anchor / custom_cards"]
```

## 文件职责

### 核心与数据

| 文件 | 作用 |
|---|---|
| `core/arex_ui_engine.h` | 全局类型、配置结构、传感器结构、dirty mask、公开 UI 总线 API 声明。 |
| `core/arex_ui_engine.c` | UI 初始化、默认配置、主刷新任务入口、全局 `g_sys_config` / `g_sensor_data` 持有者。 |
| `core/arex_data.h` | 数据同步帧、数据写入 API、告警判定入口声明。 |
| `core/arex_data.c` | `arex_bus_set_*()` 数据写入实现，维护 dirty mask，并触发可判定告警条件。 |
| `core/arex_ui_update_router.h` | UI 刷新路由模块公开入口。 |
| `core/arex_ui_update_router.c` | 消费 dirty mask，分发到 widget、card、alarm、layout rebuild 等刷新路径。 |
| `core/arex_ui_state.h` | UI 状态机枚举、输入上下文、编辑上下文、子菜单历史结构。 |
| `core/arex_ui_state.c` | 键盘/旋钮输入路由，控制 DASH、INFO、SETUP、SUB_MENU、MODAL、EDIT 等状态切换。 |
| `core/arex_callbacks.h` | UI 调业务层的回调声明，例如灯光、亮度、保守度。 |
| `core/arex_callbacks.c` | PC 模拟器默认回调实现，真实业务层可替换或对接强实现。 |

### 屏幕、布局与组件

| 文件 | 作用 |
|---|---|
| `screen/arex_screen.h` | 屏幕层公开门面，供状态机、卡片、告警、组件刷新调用。 |
| `screen/arex_screen.c` | 根屏、safe zone、左右容器、tileview、wall、dots、亮度遮罩、公共刷新入口。 |
| `screen/arex_layout_view.h` | 布局计算与网格渲染函数声明。 |
| `screen/arex_layout_view.c` | safe zone 计算、左右布局计算、2x7 固定区、5F 自定义网格定位与渲染调度。 |
| `widgets/arex_widget_view.h` | 组件工厂与组件运行态句柄声明。 |
| `widgets/arex_widget_view.c` | `render_widget_by_id()`，创建 DEPTH、NDL、POD、SYS、GAS 等可复用 widget。 |
| `widgets/arex_widget_update.h` | widget 数据刷新 API 声明。 |
| `widgets/arex_widget_update.c` | 根据 widget id 同步 `g_sensor_data` 到屏幕组件，处理文本/数值更新。 |
| `widgets/arex_widget_style_types.h` | widget 样式枚举、布局元数据、样式配置结构。 |
| `widgets/arex_widget_style.h` | widget 样式应用 API 声明。 |
| `widgets/arex_widget_style.c` | 应用边框、字体、颜色、背景等组件样式。 |

### 子菜单与弹窗

| 文件 | 作用 |
|---|---|
| `views/arex_submenu_view.h` | 子菜单抽屉创建、重置、列表句柄获取 API。 |
| `views/arex_submenu_view.c` | 子菜单滑入/滑出、列表渲染、选中态刷新、选择动作处理。 |
| `views/arex_submenu_model.h` | 子菜单数据模型 API，向 view 提供标题和条目。 |
| `views/arex_submenu_model.c` | INFO、SETUP、NESTED 菜单数据表和动态文案构建。 |
| `views/arex_modal_view.h` | 弹窗创建、显示、隐藏、pulse、上下文恢复 API。 |
| `views/arex_modal_view.c` | GAS / COMPASS / ACT 等确认弹窗的 LVGL 对象管理与动画。 |

### 告警

| 文件 | 作用 |
|---|---|
| `alarm/arex_alarm.h` | 21 个告警事件 ID、active/display API、target 查询接口。 |
| `alarm/arex_alarm.c` | 告警定义表、active 状态表、优先级选择、FIFO 轮播、ACK 与清除规则。 |
| `alarm/arex_alarm_view.h` | 告警视图上下文和 tick 渲染 API。 |
| `alarm/arex_alarm_view.c` | 横幅创建、L1/L2/L3 视觉节拍、组件靶向闪烁和样式恢复。 |

### 卡片系统

| 文件 | 作用 |
|---|---|
| `screen/arex_card_registry.h` | 卡片 ID、卡片结构、注册表 API。 |
| `screen/arex_card_registry.c` | 卡片顺序、卡片查找、tile 对象绑定、动态卡片数量计算。 |
| `cards/card_info.c` | INFO 页面主菜单。 |
| `cards/card_setup.c` | SETUP 页面主菜单和 badge 刷新。 |
| `cards/card_compass.h` | 指南针卡片对外刷新接口。 |
| `cards/card_compass.c` | 指南针页面、航向刷新、校准相关 UI 入口。 |
| `cards/card_deco.c` | 减压/组织/毒性相关页面刷新。 |
| `cards/card_gas.c` | 气体列表、气体状态和气体切换相关显示。 |
| `cards/card_plan.c` | 轨迹/计划/曲线类显示。 |
| `cards/card_blank.c` | 空白卡片占位实现。 |

### 字体与图片资源

| 文件 | 作用 |
|---|---|
| `fonts/arex_fonts.h` | 字体 ID 到 LVGL 字体对象的统一入口。 |
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
    A["1. arex_ui_engine.c<br/>看初始化和主循环"] --> B["2. arex_data.c<br/>看数据怎么写入"]
    B --> C["3. arex_ui_update_router.c<br/>看 dirty mask 怎么刷新"]
    C --> D["4. arex_screen.c<br/>看屏幕框架"]
    D --> E["5. arex_layout_view.c<br/>看布局计算"]
    E --> F["6. arex_widget_view/update<br/>看组件创建与刷新"]
    D --> G["7. arex_submenu_view/model<br/>看菜单抽屉"]
    C --> H["8. arex_alarm.c/view<br/>看告警引擎与视觉"]
    D --> I["9. cards/card_*.c<br/>看右侧具体页面"]
```

## 维护边界

| 需求 | 优先修改位置 |
|---|---|
| 新增传感器字段或数据写入 | `core/arex_ui_engine.h`、`core/arex_data.c/h` |
| 新增 dirty 刷新策略 | `core/arex_ui_update_router.c` |
| 新增固定区或 5F 组件 | `widgets/arex_widget_view.c`、`widgets/arex_widget_update.c`、`screen/arex_layout_view.c` |
| 调整组件外观 | `widgets/arex_widget_style.c`、`widgets/arex_widget_style_types.h` |
| 新增右侧页面 | `cards/card_*.c`、`screen/arex_card_registry.c/h` |
| 修改子菜单文案或层级 | `views/arex_submenu_model.c` |
| 修改子菜单动画或选中态 | `views/arex_submenu_view.c` |
| 修改告警规则或事件 | `alarm/arex_alarm.c/h`、`core/arex_data.c` |
| 修改告警视觉 | `alarm/arex_alarm_view.c/h` |
| 修改弹窗显示 | `views/arex_modal_view.c/h` |

例行源码调整不要修改 `LittlevGL.cbp`；只有明确需要同步 CodeBlocks 工程文件时再改。
