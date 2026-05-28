# 警告系统移植使用指南

本文说明当前 UI 警告系统的职责边界、等级、清除策略，以及后续移植到真机时算法层/传感器层应该怎么触发和解除警告。

## 总原则

警告状态不属于 `bus_set_*()` 数据总线。

`bus_set_*()` 只做两件事：

- 写入 `g_sensor_data` / `g_sys_config`。
- 设置 dirty mask，让 UI 刷新。

警告判断由算法、传感器、平台任务或调试层负责。它们判断出“条件触发”或“条件解除”后，显式调用 `alarm_*()` 接口。

```text
算法 / 传感器 / 平台任务 / TCP 调试
        ↓
alarm_set_active() / alarm_raise_custom() / alarm_clear_custom()
        ↓
alarm.c 维护警告状态、等级、轮播、ACK 状态
        ↓
alarm_view.c 只负责显示 banner 和高亮组件
```

不要再写成：

```c
void bus_set_depth(float depth_m)
{
    ...
    if (depth_m > 40.0f) {
        alarm_set_active(ALARM_ID_WARN_DEPTH_LIMIT, true);
    }
}
```

应该写成：

```c
bus_set_depth(depth_m);

if (depth_limit_condition) {
    alarm_set_active(ALARM_ID_WARN_DEPTH_LIMIT, true);
} else {
    alarm_set_active(ALARM_ID_WARN_DEPTH_LIMIT, false);
}
```

## 警告等级

等级定义在 `src/ui/core/ui_defs.h`：

```c
ALARM_NONE = 0
ALARM_INFO = 1
ALARM_WARN = 2
ALARM_CRIT = 3
```

当前显示优先级按等级决定：

- `CRIT` 最高。
- `WARN` 次之。
- `INFO` 最低。
- 同等级多条警告会轮播，轮播周期目前是 3000ms。

显示效果：

- `CRIT`：banner 显示 `CRITICAL: xxx`，快速闪烁；目标组件也闪烁高亮。
- `WARN`：banner 显示 `WARNING: xxx`，边框/目标组件较慢闪烁。
- `INFO`：普通提示 banner，不高亮组件。

注意：`INFO` 目前不会参与目标组件高亮，`alarm_get_active_targets()` 对 `INFO` 直接返回空列表。

## 固定警告和自定义警告

### 固定警告

固定警告 ID 定义在 `src/ui/alarm/alarm.h` 的 `alarm_id_t`。

使用方式：

```c
alarm_set_active(ALARM_ID_WARN_NDL_LOW, true);   /* 触发 */
alarm_set_active(ALARM_ID_WARN_NDL_LOW, false);  /* 解除 */
```

固定警告适合所有有稳定业务含义的场景，例如：

- 上升过快。
- PPO2 超限。
- NDL 过低。
- 电池低。
- 安全停留破坏。
- 气瓶压力低。

真机移植时优先使用固定警告 ID，不要用文本字符串判断警告类型。

### 自定义警告

自定义警告用于临时调试或没有固定 ID 的提示：

```c
alarm_raise_custom(ALARM_WARN, "DEBUG WARNING", COMP_EMPTY);
alarm_clear_custom();
```

当前只有一个 custom slot。再次 `alarm_raise_custom()` 会覆盖上一条 custom 警告。

TCP 调试目前支持：

```text
alarm info hello
alarm warn depth test
alarm crit sensor lost
alarm clear
```

## 清除策略

固定警告表里每条警告都有一个清除策略，定义在 `src/ui/alarm/alarm.c`。

### CONDITION_ONLY

条件解除前强制驻留，按键 ACK 不会隐藏。

适合真正危险、必须等条件恢复的警告：

- `ASCENT TOO FAST`
- `PO2 CRITICAL`
- `CEILING BROKEN`
- `TANK EMPTY`
- `NDL LOW`
- `SAFETY BROKEN`
- `POD LOST`
- `SAFETY STOP ACTIVE`
- `BETTER GAS AVAILABLE`

使用方式：

```c
alarm_set_active(ALARM_ID_CRIT_ASCENT_RATE, ascent_rate_too_fast);
```

只要 `ascent_rate_too_fast` 还是 true，警告就必须存在。用户按键不能把它真正清掉。

### ACK_HIDE

用户确认后可以暂时隐藏，但条件未解除前内部仍然保持 active。

适合提醒类 WARN：

- `HIGH PO2`
- `HIGH CNS`
- `HIGH OTU`
- `TURN PRESSURE`
- `DEPTH LIMIT`
- `TIME LIMIT`
- `BATTERY LOW`

使用方式：

```c
alarm_set_active(ALARM_ID_WARN_BATTERY_LOW, battery_low);
```

如果用户 ACK 了，它会隐藏；但如果 `battery_low` 一直是 true，内部状态仍是 active。等平台任务写入：

```c
alarm_set_active(ALARM_ID_WARN_BATTERY_LOW, false);
```

之后，下次再次触发才会重新显示。

### AUTO_TIMEOUT

通知类，显示一段时间后自动消失。目前超时时间是 3000ms。

适合一次性事件：

- `STOP CLEARED`
- `ALARM_INFO` 级别的 custom alarm

使用方式：

```c
alarm_set_active(ALARM_ID_INFO_STOP_DONE, true);
```

这类警告不需要业务层手动 clear，但业务层重复触发时仍然要注意不要每帧调用，否则会一直刷新。

## 按键 ACK 怎么处理

当前 `alarm.c` 已经实现：

```c
alarm_ack_current();
```

它的含义是“确认当前正在显示的这一条警告”。

但 ACK 的效果取决于清除策略：

- `ACK_HIDE`：会隐藏当前显示项。
- `CONDITION_ONLY`：不会隐藏，必须等业务层 `active=false`。
- `AUTO_TIMEOUT`：通常不用 ACK，自己超时。

当前 UI 状态机里保留了一个历史钩子：

```c
ui_state_set_alarm_pending_click(true);
```

如果该 pending 标志为 true，下一次 `ui_handle_click()` 会调用 `alarm_ack_current()`。

真机建议有两种接法：

1. 保守接法：只有外部任务认为某个警告允许用户确认时，设置 pending，然后用户下一次确认键触发 ACK。
2. 全局接法：如果当前有 `WARN/CRIT` banner，确认键优先调用 `alarm_ack_current()`，再进入普通 UI 点击逻辑。

不建议用 ACK 代替条件解除。比如 ceiling broken、上升过快、PPO2 critical，这些必须由算法判断恢复后调用 `alarm_set_active(id, false)`。

## 真机移植推荐流程

### 1. 初始化

UI 启动时已经在 `ui_init()` 中调用：

```c
data_init();
alarm_init();
```

真机侧一般不需要重复调用 `alarm_init()`，除非开始新潜水时你明确想清空所有历史警告状态。

### 2. 周期判断固定警告

算法或平台任务每个周期判断一次条件，然后写 active 状态：

```c
void dive_alarm_update(void)
{
    alarm_set_active(ALARM_ID_CRIT_ASCENT_RATE, ascent_rate_mpm >= 10.0f);
    alarm_set_active(ALARM_ID_WARN_NDL_LOW, stop_type == STOP_NONE && ndl_min >= 0 && ndl_min < 5);
    alarm_set_active(ALARM_ID_WARN_BATTERY_LOW, battery_pct < 20.0f && battery_pct >= 5.0f);
    alarm_set_active(ALARM_ID_CRIT_BATTERY_DEAD, battery_pct < 5.0f);
}
```

这些阈值只是示例。最终阈值应该来自算法、系统设置或平台配置，不放在 UI bus setter 里。

### 3. 事件型提示

比如安全停留完成：

```c
if (stop_was_active && stop_time_left_s == 0U) {
    alarm_set_active(ALARM_ID_INFO_STOP_DONE, true);
}
```

注意不要每一帧重复触发事件型提示，最好用边沿检测。

### 4. 条件型提示

比如安全停留中：

```c
alarm_set_active(ALARM_ID_INFO_SAFETY_STOP, stop_type == STOP_SAFETY);
```

这类提示进入条件时显示，离开条件时解除。

### 5. 临时调试提示

```c
alarm_raise_custom(ALARM_WARN, "SENSOR DEBUG", COMP_EMPTY);
alarm_clear_custom();
```

真机正式业务不要长期依赖 custom alarm。能稳定命名的警告应加入 `alarm_id_t` 固定表。

## 新增固定警告流程

1. 在 `alarm_id_t` 增加新的 ID，放到对应等级分组中。
2. 在 `s_alarm_defs[]` 增加定义：

```c
{ ALARM_ID_WARN_XXX, ALARM_WARN, "XXX TEXT", COMP_DEPTH_1606, true, ALARM_CLEAR_ACK_HIDE },
```

3. 选择合适的 `target`：

- 有对应 UI 组件就填 `COMP_*`。
- 不需要高亮组件就填 `COMP_EMPTY`。

4. 选择合适的清除策略：

- 危险持续类：`ALARM_CLEAR_CONDITION_ONLY`。
- 可确认提醒类：`ALARM_CLEAR_ACK_HIDE`。
- 一次性通知类：`ALARM_CLEAR_AUTO_TIMEOUT`。

5. 在算法/平台任务里调用：

```c
alarm_set_active(ALARM_ID_WARN_XXX, condition);
```

## 当前需要注意的点

- `alarm.c` 目前还依赖 `bus_requeue_dirty(DIRTY_ALARM)` 通知 UI 刷新，这是显示刷新通道，不是业务判断通道。
- `connected` 字段目前只是定义表字段，当前高亮逻辑没有使用它。
- `INFO` 不高亮目标组件，只显示 banner。
- `alarm_clear_all()` 会清掉所有固定警告和 custom 警告，只适合新潜水、重置或调试，不适合普通 ACK。
- `alarm_clear_custom()` 只清 custom 警告，不影响固定警告。
