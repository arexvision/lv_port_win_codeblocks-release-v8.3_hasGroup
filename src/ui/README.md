# UI 目录说明

`ui` 是潜水电脑 UI 的业务层。它不负责 Windows 窗口初始化，也不直接代表传感器算法层；它负责把数据、交互状态和 LVGL 对象组织成屏幕上看到的界面。

## 主链路

```text
硬件 / BLE / PC 仿真数据
  -> core/data.c: bus_set_*()
  -> g_sensor_data / g_sys_config + dirty_mask
  -> core/ui_engine.c: ui_update_task()
  -> core/update_router.c
  -> screen / comp / cards / menus / alarm / views
```

## 目录分工

| 目录 | 主要职责 | 常见入口 |
|---|---|---|
| `core/` | 数据总线、全局配置、UI 状态机、刷新路由 | 新增传感器字段、dirty mask、输入状态 |
| `screen/` | 屏幕骨架、网格布局、tileview 页面登记 | 改左右区域、布局、页面顺序映射 |
| `comp/` | 左侧固定区和 5F 中复用组件 | 改 DEPTH/NDL/POD 等组件结构、数值刷新、样式 |
| `cards/` | 右侧独立业务卡片，不含 INFO/DIVE MENU | 改指南针、气体、减压、轨迹等业务页面 |
| `menus/` | 右侧顶层菜单页 | 改 INFO MENU / DIVE MENU 顶层入口展示 |
| `views/` | 子菜单抽屉、菜单定义/运行时/动作、弹窗 | 改菜单 ID、设置项动作、确认流程、抽屉和弹窗文案 |
| `alarm/` | 告警事件和告警显示 | 改告警规则、优先级、闪烁和面向高亮 |
| `fonts/` | 字体资源 | 增加字体或调整字体映射 |
| `picture/` | 图片资源 | 增加组件/卡片使用的 LVGL 图标 |

## 边界约束

- 外部数据写入统一走 `core/data.c` 的 `bus_set_*()`，不要在其他模块直接写 `g_sensor_data` 或 `g_sys_config`。
- 屏幕级容器和导航放在 `screen/`；具体小组件内容放在 `comp/`；右侧独立业务页面放在 `cards/`；右侧顶层菜单入口放在 `menus/`。
- 右侧 tileview 页面创建和刷新仍由 `screen/card_registry.c` 统一登记。这里的 `CARD_ID_INFO` / `CARD_ID_SETUP` 是历史页面 ID，不代表 INFO/DIVE MENU 的源码还属于 `cards/`。
- 字体和图片资源通常只提供 LVGL 资源定义，不放交互业务逻辑。
- 菜单选择逻辑必须使用 `menu_id_t` / `menu_item_id_t`，显示字符串只用于 LVGL label。
- 简单设置项优先在 `views/menu_defs.c`、`views/menu_runtime.c`、`views/menu_actions.c` 收敛，不要再把业务判断写回 `submenu_view.c`。
