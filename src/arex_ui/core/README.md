# core

`core` 是 UI 的数据和控制中心。它不直接定义页面长什么样，而是决定数据如何进入 UI、交互当前处于什么状态、哪些 UI 需要刷新。

## 文件职责

| 文件 | 作用 |
|---|---|
| `arex_ui_engine.h` | 全局类型和字典，包括配置结构、传感器结构、`comp_id_t`、dirty mask、尺寸宏和颜色宏。 |
| `arex_ui_engine.c` | 持有 `g_sys_config` 与 `g_sensor_data`，加载默认配置，初始化告警和数据总线，提供 `arex_ui_update_task()`。 |
| `arex_data.h` | 数据总线公开接口、BLE 布局同步帧、配置持久化 weak 接口声明。 |
| `arex_data.c` | `arex_bus_set_*()` 实现，写入实时数据、更新统计值、设置 dirty mask，并触发部分告警判断。 |
| `arex_ui_state.h` | UI 状态枚举、输入上下文、子菜单历史、气体切换与罗盘校准命令结构。 |
| `arex_ui_state.c` | `ui_handle_rotate/click/back()` 的状态机分发，控制 DASH、INFO、SETUP、SUB_MENU、MODAL、EDIT 等流转。 |
| `arex_ui_update_router.h/c` | 50ms UI 心跳与 dirty mask 路由，决定刷新组件、卡片、告警还是重建布局。 |
| `arex_callbacks.h/c` | UI 调业务层的默认回调，例如亮度、灯光、保守度、时间、蓝牙和报警设置。 |

## 典型调用

```text
arex_bus_set_depth()
  -> g_sensor_data.depth + DIRTY_DEPTH
  -> arex_ui_update_task()
  -> arex_ui_update_router_dispatch(DIRTY_DEPTH)

arex_bus_set_ascent_rate()
  -> g_sensor_data.ascent_rate + DIRTY_ASCENT
  -> arex_ui_update_task()
  -> arex_ui_update_router_dispatch(DIRTY_ASCENT)
```

## 修改入口

- 新增传感器字段：先看 `arex_ui_engine.h` 的数据结构，再补 `arex_data.h/c` 写入接口和 dirty 标记。
- 修改旋钮/按键行为：看 `arex_ui_state.c`，不要把输入分支塞进卡片文件。
- 修改刷新频率或 dirty 分发：看 `arex_ui_update_router.c`。
- 对接真机业务回调：看 `arex_callbacks.h/c` 的接口边界。
