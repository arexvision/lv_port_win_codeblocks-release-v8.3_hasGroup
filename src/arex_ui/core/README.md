# core

`core` 放 UI 的核心数据和控制逻辑。

主要职责：

- 持有全局配置 `g_sys_config` 和实时数据 `g_sensor_data`。
- 提供 `arex_bus_set_*()` 数据写入接口，并维护 dirty mask。
- 管理 UI 状态机和输入路由。
- 通过 `arex_ui_update_task()` 和 `arex_ui_update_router_*()` 分发刷新。
- 提供 UI 调业务层的默认回调实现。

新增传感器字段、dirty 标记、数据写入接口或状态机行为时，优先从这里开始看。
