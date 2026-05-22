# alarm

`alarm` 把告警规则和告警视觉分开。事件是否 active、优先级怎么选由告警引擎决定；横幅怎么画、目标组件怎么闪由告警视图决定。

## 文件职责

| 文件 | 作用 |
|---|---|
| `arex_alarm.h` | 告警 ID、告警级别、display 结构和对外查询接口。 |
| `arex_alarm.c` | 告警定义表、active 状态表、优先级选择、轮播、ACK 和清除规则。 |
| `arex_alarm_view.h` | 告警视图渲染需要的 screen context。 |
| `arex_alarm_view.c` | safe zone 横幅、L1/L2/L3 节拍、靶向组件闪烁和样式恢复。 |

## 数据关系

```text
core/arex_data.c 判定数据条件
  -> alarm engine 更新 active 状态
  -> core/arex_ui_update_router.c 定时 tick
  -> alarm view 渲染横幅和组件高亮
```

## 修改入口

- 新增告警事件、改阈值后要暴露的事件状态：先看 `arex_alarm.h/c` 与 `core/arex_data.c`。
- 改横幅位置、颜色、闪烁节拍、目标恢复样式：看 `arex_alarm_view.c`。
- 临时模拟告警可走 `arex_bus_raise_alarm()`，正式告警应尽量有明确告警 ID。
