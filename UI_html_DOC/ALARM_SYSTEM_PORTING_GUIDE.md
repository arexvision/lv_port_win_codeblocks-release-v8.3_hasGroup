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
alarm.c 维护警告状态、等级、轮播、banner ack / target ack 状态
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

- `CRIT`：banner 显示 `CRITICAL: xxx`，反色闪烁；目标组件也反色闪烁。`back 2` 确认后只隐藏 banner，目标组件不取消闪烁，直到条件解除。
- `WARN`：如果目标组件当前可见，只做原位绿色透明底 + 1Hz 呼吸，不弹 banner；如果目标组件不可见，显示 `WARNING: xxx` banner。`back 2` 确认后只隐藏 banner，原位组件继续呼吸，直到条件解除。
- `INFO`：普通提示 banner，不高亮组件，默认 5s 自动消失。

注意：`INFO_GAS_SWITCH` 是特殊交互提示，不走 5s 自动消失；它通过 `back 2` 确认，确认后自动提交推荐气体切换请求。

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

## 确认与清除策略

告警系统把“条件是否存在”和“用户是否确认过视觉提示”拆成两层：

- 条件状态：业务层通过 `alarm_set_active(id, true/false)` 设置。只有业务层写入 `false`，才表示风险真的解除。
- `banner_ack`：用户通过 `back 2` 确认后隐藏横幅。
- `target_ack`：仅作为内部“已确认过”状态，避免同一条 WARN 重复吞掉 `back 2`；它不取消原位呼吸。`CRIT` 不设置 `target_ack`，确认后原位模块继续闪烁。
- 条件解除后，ack 状态自动重置；下一次重新触发会再次显示。

普通 INFO 事件使用自动超时，显示 5s 后消失。`INFO_STOP_DONE` 文案固定为 `STOP DONE`。

`INFO_GAS_SWITCH` 是特殊确认动作：

- 算法推荐气体有效时，显示 `BETTER GAS AVAILABLE`。
- 弹出瞬间记录当前深度；如果潜水员未确认并继续上浮超过 `ALARM_GAS_SWITCH_PROMPT_EXIT_DELTA_M`，提示自动消失。
- 用户执行 `back 2` 后调用 `alarm_confirm_current()`，内部会读取 `bus_get_recommended_gas_idx()` 并提交 `request_gas_switch()`。
- 普通 ENTER 不再确认告警，也不再执行推荐气体切换。

真机侧只需要把长按 BACK 两秒映射为：

```c
alarm_confirm_current();
```

不要用确认动作代替条件解除。比如 ceiling broken、上升过快、PPO2 critical，这些必须由算法判断恢复后调用 `alarm_set_active(id, false)`。

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
    alarm_set_active(ALARM_ID_WARN_NDL_LOW, ndl_alarm_min > 0 && (stop_type == STOP_NONE || stop_type == STOP_SAFETY) && ndl_min > 0 && ndl_min <= ndl_alarm_min);
    alarm_set_active(ALARM_ID_WARN_BATTERY_LOW, battery_pct < 20.0f && battery_pct >= 5.0f);
    alarm_set_active(ALARM_ID_CRIT_BATTERY_DEAD, battery_pct < 5.0f);
}
```

这些阈值只是示例。最终阈值应该来自算法、系统设置或平台配置，不放在 UI bus setter 里。`ndl_alarm_min == 0` 视为关闭低 NDL 警告。

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
{ ALARM_ID_WARN_XXX, ALARM_WARN, "XXX TEXT", COMP_DEPTH_1606, true, ALARM_MODE_CONDITION },
```

3. 选择合适的 `target`：

- 有对应 UI 组件就填 `COMP_*`。
- 不需要高亮组件就填 `COMP_EMPTY`。

4. 选择合适的模式：

- 持续条件类：`ALARM_MODE_CONDITION`。
- 5s 自动淡出通知：`ALARM_MODE_AUTO_TIMEOUT`。
- 需要 `back 2` 确认并执行动作：`ALARM_MODE_CONFIRM_ACTION`，当前仅用于 `INFO_GAS_SWITCH`。

5. 在算法/平台任务里调用：

```c
alarm_set_active(ALARM_ID_WARN_XXX, condition);
```

## 当前需要注意的点

- `alarm.c` 目前还依赖 `bus_requeue_dirty(DIRTY_ALARM)` 通知 UI 刷新，这是显示刷新通道，不是业务判断通道。
- `connected` 字段目前只是定义表字段，当前高亮逻辑没有使用它。
- `INFO` 不高亮目标组件，只显示 banner。
- `alarm_clear_all()` 会清掉所有固定警告和 custom 警告，只适合新潜水、重置或调试，不适合普通确认。
- `alarm_clear_custom()` 只清 custom 警告，不影响固定警告。
