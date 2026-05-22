# AREX UI 目录说明

`arex_ui` 是潜水电脑 UI 的业务层，负责把数据总线、状态机、屏幕结构、卡片、组件和告警组织成完整的 LVGL 界面。

当前目录按职责分层：

- `core/`: 数据、配置、状态机、刷新路由和业务回调。
- `screen/`: 屏幕骨架、布局计算和卡片注册表。
- `comp/`: 可复用潜水数据组件的创建、刷新和样式。
- `views/`: 弹窗、子菜单抽屉和子菜单数据模型。
- `alarm/`: 告警事件引擎和告警视觉层。
- `cards/`: 右侧 tileview 里的具体业务卡片。
- `fonts/`: LVGL 字体资源。
- `picture/`: LVGL 图片/图标资源。

外部数据写入应通过 `core/arex_data.c` 里的 `arex_bus_set_*()` 接口进入，不要从其他模块直接写 `g_sensor_data` 或 `g_sys_config`。
