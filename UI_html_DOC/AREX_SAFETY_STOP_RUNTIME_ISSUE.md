# AREX Safety Stop Runtime Issue

## Purpose

本文档用于给算法工程师同步当前 PC 模拟器发现的 safety stop 显示问题。重点不是 UI 样式，而是 `arex_deco_plan()` 输出的“未来计划”能否承担主界面“当前正在执行的安全停留状态”。

## Current UI Call Chain

当前 PC 模拟器每个算法 tick 的主链路如下：

```text
deco_core_tick(depth, temp, dt)
  -> arex_deco_step(...)
  -> arex_deco_plan(&s_state, &schedule, ...)
  -> sync_stop_data(&schedule)
  -> bus_update_deco(ndl, stop_type, stop_depth, total_s, left_s, in_stop_zone)
  -> ui_vm_ndl_stop_update()
  -> comp_refresh_ndl_stop_vm()
```

右上角 `NDL / SAFE / DECO` 模块不直接调用算法，也不自行判断安全停留。它只消费 `bus_update_deco()` 写入的数据：

```c
stop_type_t stop_type;
float stop_depth_m;
uint16_t stop_time_total_s;
uint16_t stop_time_left_s;
bool in_stop_zone;
```

UI 显示规则：

```text
STOP_NONE   -> NDL
STOP_SAFETY -> SAFE Xm + countdown
STOP_DECO   -> DECO Xm + countdown
```

## Current Adapter Logic

`sync_stop_data()` 当前只从算法 schedule 中取第一个有效 runtime stop：

```c
const ArexDecoStop *runtime_stop = first_runtime_stop(schedule);

if (runtime_stop != NULL && s_metrics.ceiling_depth_m > DECO_CEILING_ACTIVE_M) {
    stop_type = STOP_DECO;
} else if (runtime_stop != NULL) {
    stop_type = STOP_SAFETY;
} else {
    stop_type = STOP_NONE;
}
```

之前 PC 适配层曾经有一段本地 fallback：

```text
safety_stop_enabled
&& max_depth > 10m
&& current_depth >= safety zone shallow side
&& ndl <= 0
```

这段 fallback 已经删除。现在右上角 `SAFE` 只来自算法 `schedule.stops[]`，以便验证算法侧是否能独立提供稳定的安全停留状态。

## Algorithm Safety Stop Fields

当前算法文档和头文件中已知 safety stop 相关输入/常量：

```c
#define AREX_DECO_SAFETY_STOP_TRIGGER_DEPTH_M 10.0f
#define AREX_DECO_SAFETY_STOP_DEPTH_M 5.0f
```

```c
ArexDecoConfig.safety_stop_enabled
ArexDecoConfig.safety_stop_seconds
```

当前适配层会把 UI 设置写入：

```c
config->safety_stop_seconds = safety_stop_seconds_from_mode(...);
config->safety_stop_enabled = safety_enabled;
```

## Observed Behavior

复现方式：

```text
speed 40
goto 40
上升回安全停留范围
```

观察到的现象：

1. 本次最大深度超过 10m 后，右上角可以正常出现 `SAFE 5m`。
2. 继续上升，进入安全停留范围附近后，右上角有时突然退回普通 `NDL` 样式。
3. 退回 `NDL` 时，UI 侧收到的是 `STOP_NONE`，不是 `STOP_SAFETY`。

这说明该帧 `sync_stop_data()` 没有拿到有效 `runtime_stop`。由于适配层 fallback 已删除，最直接的推论是：该帧 `arex_deco_plan()` 返回的 `schedule.stops[]` 中没有可用于 runtime 显示的 safety stop。

## Suspected Root Cause

当前算法文档描述中，安全停留生成条件包含：

```text
没有强制减压
safety_stop_enabled == 1
本次最大深度超过 AREX_DECO_SAFETY_STOP_TRIGGER_DEPTH_M
当前深度深于 AREX_DECO_SAFETY_STOP_DEPTH_M
```

如果算法 plan 的语义是“从当前状态到未来水面的计划”，那么当潜水员已经到达或浅于 `AREX_DECO_SAFETY_STOP_DEPTH_M = 5m` 时，planner 可能认为未来不需要再插入 5m safety stop，于是 `schedule.stops[]` 为空。

但主界面需要的是“当前正在执行的 safety stop 状态”：

```text
本次潜水已经触发 safety stop
潜水员进入 safety stop zone
安全停留尚未完成
=> 右上角应继续显示 SAFE 5m，并显示剩余时间/进度
```

这两种语义不同：

| Concept | Meaning | Current Source |
|---|---|---|
| Future plan stop | 从当前状态继续上升，未来还需要停哪里 | `arex_deco_plan().schedule.stops[]` |
| Runtime current stop | 当前正在执行/应保持显示的停留状态 | 目前也临时用 `schedule.stops[0]` 推导 |

这个差异会导致：planner 认为“未来不用再插入安全停留”，但产品 UI 仍然需要“当前安全停留正在进行”。

## Expected Product Behavior

产品期望：

1. 安全停留开启时，本次潜水最大深度超过触发深度后，应产生 safety stop obligation。
2. 在完成 safety stop 前，右上角应保持 `SAFE 5m`，不应因为当前已到达 5m 附近而退回 `NDL`。
3. 倒计时只应在有效安全停留区内推进。
4. 偏离安全停留区时，可以暂停/提示 broken，但 obligation 不应立刻消失。
5. 安全停留完成后，才切回普通 `NDL`，并触发产品层 `STOP DONE`。
6. 出水或新一轮潜水生命周期开始时，重置本次 safety stop obligation。

## Questions For Algorithm Side

请算法侧确认以下接口语义：

1. `arex_deco_plan()` 返回的 safety stop 是否只表示“未来计划中的停站”？
2. 当当前深度已经等于或浅于 `AREX_DECO_SAFETY_STOP_DEPTH_M` 时，planner 是否会故意不再返回 safety stop？
3. 算法 core 内部是否已经维护“本次潜水 safety stop obligation / completed / remaining seconds”状态？
4. 如果有，是否可以通过 runtime metrics 或新接口输出以下字段？

建议字段：

```c
bool safety_stop_required;
bool safety_stop_active;
bool safety_stop_completed;
float safety_stop_depth_m;
uint32_t safety_stop_total_seconds;
uint32_t safety_stop_remaining_seconds;
bool safety_stop_in_zone;
```

5. 如果算法侧不维护 runtime safety stop 状态，是否建议产品层/平台层维护一个 latch？如果需要产品层维护，请明确算法侧 plan 的边界，避免 UI 误以为 `schedule.stops[]` 就是 runtime stop 状态。

## Proposed Direction

优先建议算法侧提供 runtime safety stop 状态，因为：

- safety stop obligation 与组织舱/潜水生命周期强相关。
- UI 不应复刻触发深度、完成条件、倒计时推进、偏离区间等算法策略。
- 真机和模拟器需要同一口径。

如果算法侧暂不提供 runtime 字段，产品层只能增加一个很薄的 latch：

```text
if max_depth > trigger && safety enabled:
    required = true

if required && in safety zone:
    decrement remaining

if remaining == 0:
    completed = true
    required = false

if surfaced/new dive:
    reset
```

但这会让 safety stop 策略分散到产品层，不符合当前“算法已有接口则不在 UI 复刻计算”的项目规则。

## Debug Data Needed

建议在问题复现时同时打印：

```text
current_depth
max_depth
ndl_seconds
ceiling_depth_m
schedule.stop_count
schedule.tts_seconds
schedule.ceiling_violated
config.safety_stop_enabled
config.safety_stop_seconds
AREX_DECO_SAFETY_STOP_DEPTH_M
first stop depth/duration/hold/switch
```

如果进入 5m 附近后 `schedule.stop_count == 0`，即可确认是 planner 计划语义导致的 safety stop runtime 状态缺失。
