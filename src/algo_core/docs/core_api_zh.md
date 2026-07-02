# AREX Deco Core API 文档

本文档描述当前 core API。当前 API 版本为 `0.0.29`。

## 适用场景

- 嵌入式 / 原生 C/C++：直接包含 `arex_deco/arex_deco.h` 调用 C ABI。
- WASM / Web：通过 `bindings/wasm` 导出的同名 C ABI 函数和固定 POD 结构体内存布局调用。

core 只负责减压算法、组织舱状态、氧暴露、计划输出和禁飞时间估算。气瓶压力估算、UI 标注、日志导入、持久化等属于产品层。

## 版本与容量

版本宏：

- `AREX_DECO_API_VERSION_MAJOR = 0`
- `AREX_DECO_API_VERSION_MINOR = 0`
- `AREX_DECO_API_VERSION_PATCH = 29`

固定容量：

- `AREX_DECO_COMPARTMENT_COUNT = 16`
- `AREX_DECO_MAX_GAS_COUNT = 6`
- `AREX_DECO_MAX_DECO_STOP_COUNT = 40`
- `AREX_DECO_INVALID_STOP_INDEX = 255`，用于 `uint8_t` raw stop index 字段的无效哨兵，
  必须始终大于等于 `AREX_DECO_MAX_DECO_STOP_COUNT`

常量归类：

- `abi_constants.h`：API 版本、固定容量和跨语言 ABI 哨兵。
- `defaults.h`：默认配置模板、默认气体策略和 runtime selector 默认策略。
- `model_constants.h`：算法模型固定规则、物理常数、数值容差和安全停留/禁飞/CNS 机制常量。

默认参数：

- `surface_pressure_bar = 1.01325`
- `water_vapor_pressure_bar = 0.0627`
- `water_meters_per_bar = 10.0` 海水默认
- `gf_low = 0.50`
- `gf_high = 0.70`
- `ascent_rate.rate_75_percent_m_per_min = 9.0`
- `ascent_rate.rate_50_percent_m_per_min = 9.0`
- `ascent_rate.rate_stops_m_per_min = 9.0`
- `ascent_rate.rate_last_6m_m_per_min = 9.0`
- `deco_step_m = 3.0`
- `last_stop_m = 3.0`
- `safety_stop_enabled = 1`
- `safety_stop_seconds = 180`
- `gas_switch_penalty_seconds = 60`

固定安全停留策略：

- `AREX_DECO_SAFETY_STOP_TRIGGER_DEPTH_M = 10.0`
- `AREX_DECO_SAFETY_STOP_DEPTH_M = 5.0`

core 所有深度策略均使用米制；英制展示由产品层自行从米换算。

CNS 衰减（0.0.3 起）：

- `AREX_DECO_CNS_HALFLIFE_SECONDS = 5400.0`（90 min）
- `AREX_DECO_CNS_DECAY_PPO2_THRESHOLD_BAR = 0.5`
- 当 step 段平均 PPO2 < 0.5 bar 时，`cns_percent` 按 90 min 半衰期指数衰减；否则按 NOAA 表加载。

气体物理常数与 ppO2 红线（0.0.4 起）：

- `AREX_DECO_AIR_OXYGEN_FRACTION = 0.21`
- `AREX_DECO_AIR_NITROGEN_FRACTION = 0.79`
- `AREX_DECO_DEFAULT_BOTTOM_PPO2_BAR = 1.4`（底部气工作 ppO2 默认）
- `AREX_DECO_DEFAULT_DECO_PPO2_BAR = 1.6`（减压气工作 ppO2 默认）
- `AREX_DECO_MAX_ALLOWABLE_PPO2_BAR = 2.0`（绝对生理硬顶——`validate_gas` 会拒绝高于此值的配置；行业策略限值 1.4 / 1.6 应在 UI 层提示）

runtime current-stop selector 默认策略（0.0.28 起）：

- `AREX_DECO_DEFAULT_RUNTIME_STOP_ZONE_HALF_WIDTH_M = 1.5`
- `AREX_DECO_DEFAULT_RUNTIME_STOP_PROMOTE_MIN_SECONDS = 30`
- `AREX_DECO_DEFAULT_RUNTIME_STOP_STABLE_SECONDS = 2`

## 状态码

| 状态码 | 数值 | 含义 |
|---|---:|---|
| `AREX_DECO_STATUS_OK` | 0 | 成功 |
| `AREX_DECO_STATUS_INVALID_ARGUMENT` | 1 | 空指针、非法参数或越界输入 |
| `AREX_DECO_STATUS_UNSUPPORTED_VERSION` | 2 | 版本不支持，当前预留 |
| `AREX_DECO_STATUS_INSUFFICIENT_CAPACITY` | 3 | 输出容量不足，当前预留 |
| `AREX_DECO_STATUS_INVALID_STATE` | 4 | 状态不一致或算法无法继续 |
| `AREX_DECO_STATUS_NOT_IMPLEMENTED` | 5 | 功能未实现 |

调用方必须检查返回状态码。不要在失败状态下继续读取输出结构体作为有效结果。

## 核心结构体

所有结构体是固定布局 POD。`reserved` 字段必须写 0，调用方不要使用。

### `ArexDecoVersion`

| 字段 | 类型 | 说明 |
|---|---|---|
| `major` | `uint16_t` | 主版本 |
| `minor` | `uint16_t` | 次版本 |
| `patch` | `uint16_t` | 补丁版本 |

### `ArexDecoConfig`

算法配置。作为 `ArexDecoDiveState.config` 的一部分参与 `step`、`plan`、`nofly`。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `surface_pressure_bar` | `float` | bar | 水面绝对压强 |
| `water_vapor_pressure_bar` | `float` | bar | 水蒸气压 |
| `water_meters_per_bar` | `float` | m/bar | 水深到压强换算 |
| `gf_low` | `float` | 0-1 | 低 GF |
| `gf_high` | `float` | 0-1 | 高 GF，必须大于等于 `gf_low` |
| `ascent_rate` | `ArexDecoAscentRate` | m/min | 上升速率 profile，按 Subsurface core planner 的四段规则选择速率 |
| `deco_step_m` | `float` | m | 中间减压站跨度 |
| `last_stop_m` | `float` | m | 最后减压站深度。Planner 到达该深度后不再生成更浅停站，而是在该站延长时间直到可升水 |
| `safety_stop_seconds` | `uint32_t` | s | 非强制减压时的安全停留时长 |
| `gas_switch_penalty_seconds` | `uint32_t` | s | 计划中预测切换气体时附加的同深度延迟；penalty 期间按新气体推进组织舱，并计入停站 `duration_seconds` 和 TTS |
| `water_type` | `ArexDecoWaterType` | - | `SALT=0`，`FRESH=1` |
| `safety_stop_enabled` | `uint8_t` | - | 非强制减压安全停留开关，`1` 启用，`0` 关闭 |

`ArexDecoAscentRate` 字段：

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `rate_75_percent_m_per_min` | `float` | m/min | 当前深度大于平均深度 75% 时的上升速率 |
| `rate_50_percent_m_per_min` | `float` | m/min | 当前深度大于平均深度 50%、且不在 75% 段时的上升速率 |
| `rate_stops_m_per_min` | `float` | m/min | 当前深度不大于平均深度 50%、且深于 6m 时的上升速率 |
| `rate_last_6m_m_per_min` | `float` | m/min | 6m 及以内的最终上升速率 |

平均深度由 `ArexDecoDiveState.depth_time_m_seconds / elapsed_seconds` 得出，
`arex_deco_step()` 会按线性深度段自动累计该积分。

校验规则：

- `surface_pressure_bar > 0`
- `0 <= water_vapor_pressure_bar < surface_pressure_bar`
- `water_meters_per_bar > 0`
- `0 < gf_low <= 1`
- `gf_low <= gf_high <= 1`
- `ascent_rate.rate_75_percent_m_per_min > 0`
- `ascent_rate.rate_50_percent_m_per_min > 0`
- `ascent_rate.rate_stops_m_per_min > 0`
- `ascent_rate.rate_last_6m_m_per_min > 0`
- `deco_step_m > 0`
- `last_stop_m > 0`
- `safety_stop_enabled` 必须为 `0` 或 `1`
- 当 `safety_stop_enabled == 1` 时，`safety_stop_seconds > 0`

### `ArexDecoGas`

单个气体定义。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `oxygen_fraction` | `float` | 0-1 | 氧气比例 |
| `helium_fraction` | `float` | 0-1 | 氦气比例 |
| `nitrogen_fraction` | `float` | 0-1 | 氮气比例 |
| `min_depth_m` | `float` | m | 允许使用的最浅深度 |
| `max_depth_m` | `float` | m | 允许使用的最深深度，通常由 MOD 或切换深度决定 |
| `max_ppo2_bar` | `float` | bar | 最大 PO2 |
| `enabled` | `uint8_t` | - | 1 启用，0 禁用 |
| `role` | `ArexDecoGasRole` | - | `BOTTOM=0`，`TRAVEL=1`，`DECO=2` |

校验规则：

- O2 / He / N2 都在 `[0, 1]`
- 三者总和在 `[0.999, 1.001]`
- `min_depth_m >= 0`
- `max_depth_m >= min_depth_m`
- `max_ppo2_bar > 0` 且 `<= AREX_DECO_MAX_ALLOWABLE_PPO2_BAR`（0.0.4 起）
- `max_depth_m <= MOD(config, gas)`（0.0.4 起）。MOD 由 `gas_pressure_to_depth_m(config, max_ppo2_bar / oxygen_fraction)` 计算。允许调用方设置比 MOD 更保守（更浅）的 `max_depth_m`，但禁止超过 MOD。

### `ArexDecoGasPlan`

气体列表和当前气体。

| 字段 | 类型 | 说明 |
|---|---|---|
| `api_version` | `ArexDecoVersion` | API 版本 |
| `gas_count` | `uint8_t` | 气体数量，范围 `1..6` |
| `active_gas_index` | `int8_t` | 当前气体索引 |
| `gases` | `ArexDecoGas[6]` | 固定容量气体数组 |

### `ArexDecoTissueState`

16 个组织舱的惰性气体压强。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `nitrogen_bar[16]` | `float[]` | bar | 各组织舱氮气压 |
| `helium_bar[16]` | `float[]` | bar | 各组织舱氦气压 |

### `ArexDecoOxygenExposure`

氧暴露累计。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `cns_percent` | `float` | % | CNS 氧中毒百分比 |
| `otu` | `float` | OTU | Oxygen Toxicity Units |

### `ArexDecoDiveState`

完整潜水状态，是实时模拟和计划计算的核心输入。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `config` | `ArexDecoConfig` | - | 算法配置 |
| `gas_plan` | `ArexDecoGasPlan` | - | 气体计划 |
| `tissue` | `ArexDecoTissueState` | - | 组织舱状态 |
| `oxygen_exposure` | `ArexDecoOxygenExposure` | - | 氧暴露 |
| `current_depth_m` | `float` | m | 当前深度 |
| `max_depth_m` | `float` | m | 本次潜水最大深度 |
| `depth_time_m_seconds` | `float` | m*s | 深度时间积分，由 `arex_deco_step()` 按线性深度段自动累计，用于计算平均深度 |
| `elapsed_seconds` | `uint32_t` | s | 已经过时间 |
| `was_deco_dive` | `uint8_t` | - | 本次潜水是否曾触发减压义务（latched bit）。一旦 `metrics->ceiling_depth_m > 0` 则置 1，并保持到新潜水状态初始化。**该字段服务过去式语义**（影响 nofly 等下限），不要用作 UI 实时 "DECO NOW" 指示——实时义务请读 `metrics->ceiling_depth_m`。0.0.4 起字段名由 `in_deco` 重命名为 `was_deco_dive` 以消除时态歧义；内存布局不变 |
| `safety_stop_required` | `uint8_t` | - | 安全停留执行状态：本次潜水是否已触发安全停留 |
| `safety_stop_completed` | `uint8_t` | - | 安全停留执行状态：是否已完成 |
| `safety_stop_missed` | `uint8_t` | - | 安全停留执行状态：是否已错过 |
| `safety_stop_elapsed_seconds` | `uint32_t` | s | 安全停留累计有效计时 |
| `gf_anchor_depth_m` | `float` | m | GF Low 深端锚点：本次潜水进入强制减压后，历史上出现过的最深有效 GF-low first-stop grid depth。它用于稳定 GF Low → GF High 插值斜率；不是 current stop、current ceiling、max depth 或 safety stop |
| `gf_anchor_valid` | `uint8_t` | - | `gf_anchor_depth_m` 是否有效。完整 profile replay 会由 `arex_deco_step()` 自动重建；若宿主从中间快照恢复，则快照必须保存该字段和 `gf_anchor_depth_m` |

`gf_anchor_depth_m` / `gf_anchor_valid` 是 public `ArexDecoDiveState` 的算法状态，
不是调试字段。宿主若序列化、恢复或跨 WASM/native 边界传递完整潜水状态，必须按
ABI 布局保存和还原它们；内部验证用的 `GfAnchorUpdateInfo` 不属于 C API。

`arex_deco_plan()` 会校验 `current_depth_m`、`max_depth_m`、
`depth_time_m_seconds` 和 `elapsed_seconds` 的基本一致性。若调用方手工拼接
或篡改 `ArexDecoDiveState`，导致例如 `current_depth_m > max_depth_m`、
0 秒状态带有非零深度、或 `depth_time_m_seconds > max_depth_m * elapsed_seconds`
等不可能状态，planner 返回 `AREX_DECO_STATUS_INVALID_STATE`。

### `ArexDecoStepInput`

按深度推进一步。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `start_depth_m` | `float` | m | 起始深度 |
| `end_depth_m` | `float` | m | 结束深度 |
| `duration_seconds` | `uint32_t` | s | 该段持续时间 |
| `gas_index` | `int8_t` | - | 使用气体索引 |

### `ArexDecoPressureStepInput`

按绝对压强推进一步。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `start_pressure_bar` | `float` | bar | 起始绝对压强 |
| `end_pressure_bar` | `float` | bar | 结束绝对压强 |
| `duration_seconds` | `uint32_t` | s | 该段持续时间 |
| `gas_index` | `int8_t` | - | 使用气体索引 |

### `ArexDecoRuntimeMetrics`

单步计算输出指标。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `gf99_percent` | `float` | % | 当前深度下 GF99 |
| `surface_gf_percent` | `float` | % | 直接升水时的 Surface GF |
| `ceiling_depth_m` | `float` | m | 当前组织舱天花板深度 |
| `ndl_seconds` | `int32_t` | s | 免减压剩余时间。进入强制减压时通常为 0 |
| `leading_compartment` | `int8_t` | - | 主导组织舱索引，0 基 |

### `ArexDecoGasRecommendation`

当前深度下的最佳安全可用气体推荐。它只回答“按当前 `ArexDecoDiveState`
和气体表，是否存在比 active gas 更优的安全气体”，不直接决定产品 UI 是否弹窗；
`arex_deco_plan()` 的 `gas_rec` 参数仅作为兼容/便利输出，实时侧推荐直接调用
`arex_deco_recommend_gas()` 并叠加产品层展示门控。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `available` | `uint8_t` | - | 1 表示当前深度存在比 active gas 更优的安全可用气体 |
| `recommended_gas_index` | `int8_t` | - | 推荐气体索引 |
| `active_gas_index` | `int8_t` | - | 当前气体索引 |
| `is_emergency_no_safe_gas` | `uint8_t` | - | 1 表示当前没有任何安全可用气体 |
| `depth_m` | `float` | m | 评估深度 |
| `ppo2_bar` | `float` | bar | 推荐气体在该深度的 PO2 |

### `ArexDecoStop`

单个减压停站。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `depth_m` | `float` | m | 停站深度 |
| `duration_seconds` | `uint32_t` | s | 停站预测总时长，等于 `hold_seconds + switch_penalty_seconds`。适合静态计划、TTS、气量估算，也适合作为 runtime 当前站主倒计时 |
| `gas_index` | `int8_t` | - | 停站使用气体 |
| `target_gf` | `float` | 0-1 | 该停站目标 GF |
| `hold_seconds` | `uint32_t` | s | 扣除预测切气延迟后的停留求解时间。强制减压站由 `solve_stop_time_seconds()` 从 penalty 推进后的组织状态起算；安全停留站使用 `safety_stop_seconds` |
| `switch_penalty_seconds` | `uint32_t` | s | planner 预测用的同深度切气延迟；期间按新气体推进组织舱，并已包含在 `duration_seconds` 内 |
| `kind` | `uint8_t` | - | algorithm stop kind，描述 raw stop 在 planner 路径中的算法身份：`MANDATORY=0`、`ROUTE_WAYPOINT=1`、`GAS_SWITCH=2`、`SAFETY=3` |
| `flags` | `uint8_t` | - | runtime display hint flags，不参与 tissue / TTS / ceiling 安全判定；`AREX_DECO_STOP_FLAG_DISPLAY_SUPPRESSED` 表示该 raw stop 不应作为 runtime 当前实质停站展示 |

`hold_seconds` 依赖 `switch_penalty_seconds` 已按新气体推进组织舱这一前提，
两者不可被视为两个互不相关的倒计时。静态计划、TTS 和气量估算应继续使用
`duration_seconds`；用户主屏当前停站应调用
`arex_deco_select_runtime_stop()` 获取稳定 runtime current-stop 语义。

`kind` 是算法事实：它说明该 raw stop 是强制减压停站、GF anchor 下的网格路径
waypoint、纯切气预测站，还是安全停留。runtime current stop projection 只从
`MANDATORY` 中选择；`ROUTE_WAYPOINT` 是 planner route representation，不是 user stop
obligation。

`flags` 是 runtime display hint：它不改变 raw schedule、`tts_seconds`、组织舱推进、
ceiling、GF99 或安全判定。`AREX_DECO_STOP_FLAG_DISPLAY_SUPPRESSED` 只告诉固件 / App：
该 raw stop 不应作为主屏当前实质停站、PLAN 图首站或 DLF/APP `next_stop` 展示。

`ROUTE_WAYPOINT` 即使未设置 `DISPLAY_SUPPRESSED`，也不进入 runtime current stop。
它可以保留在 raw schedule、PLAN 图调试和 TTS 路径中，但主屏当前停站应使用
`arex_deco_select_runtime_stop()` 从 first meaningful mandatory obligation 派生。
`DISPLAY_SUPPRESSED` 是 display hint，不是 Bühlmann / GF 安全裕量。

### `ArexDecoSchedule`

计划输出。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `stop_count` | `uint8_t` | - | 有效停站数量 |
| `truncated` | `uint8_t` | - | 1 表示停站数组容量不足被截断 |
| `ceiling_violated` | `uint8_t` | - | 1 表示计划时当前深度浅于 GF-high ceiling，即使 `stops/tts` 为 0 也必须提示违规/风险 |
| `tts_seconds` | `uint32_t` | s | raw Time To Surface，包括完整 planner 路径、上升时间、停站、安全停留和切气惩罚；不应由 UI/display stop 重新计算 |
| `end_of_dive_exposure` | `ArexDecoOxygenExposure` | - | 若严格按此计划升水，出水时预测累计 CNS / OTU |
| `stops[40]` | `ArexDecoStop[]` | - | 固定容量 raw algorithm route 停站数组。它是 planner 的完整原始路径，可包含短 waypoint，不等同于固件 UI 当前显示停站 |

`ArexDecoSchedule.stops[]` 是 core 算法输出，不是产品显示层 API。
`18 m / 1 s`、`15 m / 1 s`、`6 m / 1 s` 这类短 waypoint 可能出现在 raw
路径中，用于保持 GF slope、减压网格路径和实时重复 plan 的连续性。它们会参与
TTS、组织舱和氧暴露递推，因此 core 不会为了贴合 UI first stop 直接删除。
固件 `ArexDecoRuntime` / App display 层应在保留 raw schedule 的同时，把
`schedule`、上一帧 `ArexDecoRuntimeStopSelectorState` 和当前深度传给
`arex_deco_select_runtime_stop()`，用其输出作为主屏当前实质停站和 DLF/APP
`next_stop`。凡带有 `AREX_DECO_STOP_FLAG_DISPLAY_SUPPRESSED` 的 raw stop 不进入
selector 候选；`schedule.tts_seconds` 仍始终来自 raw schedule。

### `ArexDecoRuntimeStopSelectorState`

runtime current-stop selector 的调用方持有状态。core 不在 `arex_deco_plan()` 内保存
隐藏状态；固件、Web 或 App 每帧把上一帧 `next_state` 作为下一次
`previous_state` 传入。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `active` | `uint8_t` | - | 1 表示当前有已激活的 runtime current stop |
| `candidate_active` | `uint8_t` | - | 保留 ABI 字段；当前 selector 不再使用 candidate promotion，输出中保持 0 |
| `displayed_source_raw_index` | `uint8_t` | - | 当前显示停站来源 raw stop index；无效时为 `AREX_DECO_INVALID_STOP_INDEX` |
| `candidate_source_raw_index` | `uint8_t` | - | 保留 ABI 字段；当前 selector 不再使用，输出中为 `AREX_DECO_INVALID_STOP_INDEX` |
| `displayed_depth_m` | `float` | m | 当前 runtime 停站深度 |
| `displayed_remaining_seconds` | `uint32_t` | s | 当前 runtime 停站剩余/预测停留秒数 |
| `displayed_total_seconds` | `uint32_t` | s | 当前 runtime 停站总预测秒数 |
| `displayed_gas_index` | `int8_t` | - | 当前 runtime 停站气体 |
| `displayed_is_short` | `uint8_t` | - | 1 表示当前显示停站低于 promote 门槛 |
| `candidate_gas_index` | `int8_t` | - | 保留 ABI 字段；当前 selector 不再使用，输出中为 -1 |
| `candidate_depth_m` | `float` | m | 保留 ABI 字段；当前 selector 不再使用，输出中为 0 |
| `candidate_seen_seconds` | `uint32_t` | s | 当前仅记录 `HELD_PREVIOUS` 的连续 hold 秒数；没有 hold 时为 0 |
| `last_elapsed_seconds` | `uint32_t` | s | 上一次 selector 调用对应的潜水 elapsed time |

### `ArexDecoRuntimeStopSelectorInput`

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `current_depth_m` | `float` | m | 当前真实深度 |
| `elapsed_seconds` | `uint32_t` | s | 当前潜水 elapsed time |
| `stop_zone_half_width_m` | `float` | m | 当前停站 zone 半宽；传 0 使用 `AREX_DECO_DEFAULT_RUNTIME_STOP_ZONE_HALF_WIDTH_M` |
| `promote_min_seconds` | `uint32_t` | s | 实质停站门槛；传 0 使用 `AREX_DECO_DEFAULT_RUNTIME_STOP_PROMOTE_MIN_SECONDS` |
| `stable_seconds` | `uint32_t` | s | 极薄 hysteresis 秒数；0 表示关闭。非 0 会夹到 1-2 s，上限为 2 s；默认宏为 2 |

### `ArexDecoRuntimeStop`

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `available` | `uint8_t` | - | 1 表示本帧有 runtime current stop |
| `source_raw_index` | `uint8_t` | - | 来源 raw stop index；无效时为 `AREX_DECO_INVALID_STOP_INDEX` |
| `reason` | `uint8_t` | - | `AREX_DECO_RUNTIME_STOP_REASON_*` |
| `is_short` | `uint8_t` | - | 1 表示输出停站低于 promote 门槛 |
| `depth_m` | `float` | m | runtime current stop 深度 |
| `remaining_seconds` | `uint32_t` | s | runtime current stop 剩余/预测秒数 |
| `total_seconds` | `uint32_t` | s | runtime current stop 总预测秒数 |
| `gas_index` | `int8_t` | - | 停站气体 |

`reason` 取值：

| 值 | 名称 | 说明 |
|---:|---|---|
| 0 | `NONE` | 无特殊原因 |
| 1 | `STABLE_CANDIDATE` | 本帧 projection 输出了非 short mandatory stop |
| 2 | `UNIQUE_SHORT` | 本帧 projection 输出了允许显示的 short mandatory stop |
| 3 | `HELD_PREVIOUS` | 极薄 hysteresis 暂时保持上一帧 runtime stop，且来源已在当前 schedule 中重新匹配到有效 mandatory |
| 4 | `DEBOUNCING` | 旧语义保留值；当前 selector 不再使用 10 s debounce，也不会因等待候选稳定而清空输出 |
| 5 | `CLEARED` | 当前无 runtime selector 候选 |

### `ArexDecoTtsForecast`

TTS hold 预测输出。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `hold_seconds` | `uint32_t` | s | 虚拟定深停留秒数 |
| `current_tts_seconds` | `uint32_t` | s | 当前状态下的 TTS |
| `tts_at_hold_seconds` | `uint32_t` | s | 当前深度、当前 active gas 继续停留 `hold_seconds` 后的预测 TTS |
| `tts_delta_hold_seconds` | `int32_t` | s | `tts_at_hold_seconds - current_tts_seconds` |

### `ArexDecoNdlExcursionForecast`

NDL 深度试探预测输出。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `delta_depth_m` | `float` | m | 上下试探深度 |
| `current_ndl_seconds` | `int32_t` | s | 当前深度、当前 active gas 下的 NDL |
| `ndl_up_seconds` | `int32_t` | s | 立即上移 `delta_depth_m` 后的 NDL；深度钳制到 0 m |
| `ndl_down_seconds` | `int32_t` | s | 立即下移 `delta_depth_m` 后的 NDL |

`NDL Δ 3M / 10FT` 是产品显示层语义：固件可根据当前页面模式在
`ndl_up_seconds` 和 `ndl_down_seconds` 之间选择或自行计算相对当前 NDL 的变化量。
core 不返回第三个 delta 字段，避免把 UI 表达固化为算法 ABI。

### `ArexDecoSafetyStopStatus`

安全停留 runtime 状态输出。该结构描述“当前真实状态下安全停留是否需要、是否正在
计时、剩余多少秒”，不替代 `arex_deco_plan()` 的 TTS / 预测路径用途。

`phase` 取值：

| 值 | 说明 |
|---:|---|
| `0` | `NOT_REQUIRED`，未触发或安全停留关闭 |
| `1` | `PENDING`，预留阶段；当前实现用 `PAUSED_TOO_DEEP` / `PAUSED_TOO_SHALLOW` 表达未计时原因 |
| `2` | `COUNTING`，位于有效区间并正在计时 |
| `3` | `PAUSED_TOO_DEEP`，深于有效区间，暂停计时 |
| `4` | `PAUSED_TOO_SHALLOW`，浅于有效区间但未到 missed 阈值，暂停计时 |
| `5` | `MISSED_TOO_SHALLOW`，浅于 missed 阈值；本次安全停留终止，不再继续倒计时 |
| `6` | `COMPLETE`，安全停留已完成 |
| `7` | `SUPPRESSED_BY_DECO`，当前已有 GF-high 强制减压 ceiling，安全停留被强制减压义务覆盖 |

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `required` | `uint8_t` | - | 1 表示当前仍需执行安全停留 |
| `counting` | `uint8_t` | - | 1 表示本状态处于有效计时区间 |
| `phase` | `ArexDecoSafetyStopPhase` | - | 当前安全停留阶段 |
| `completed` | `uint8_t` | - | 1 表示本次安全停留已完成 |
| `missed` | `uint8_t` | - | 1 表示本次安全停留已因过浅 missed |
| `target_depth_m` | `float` | m | 目标安全停留深度，当前固定 5 m |
| `zone_min_depth_m` | `float` | m | 有效计时区间最浅深度，当前固定 2.9 m |
| `zone_max_depth_m` | `float` | m | 有效计时区间最深深度，当前固定 6 m |
| `too_shallow_depth_m` | `float` | m | missed too shallow 阈值，当前固定 2 m |
| `trigger_depth_m` | `float` | m | 触发最大深度阈值，当前固定 10 m |
| `required_seconds` | `uint32_t` | s | 配置的安全停留总时长 |
| `elapsed_seconds` | `uint32_t` | s | 已在有效区间累计计时的秒数 |
| `remaining_seconds` | `uint32_t` | s | 剩余秒数 |

## C API

### `arex_deco_get_api_version`

```c
ArexDecoVersion arex_deco_get_api_version(void);
```

返回当前库编译时使用的 API 版本，字段对应
`AREX_DECO_API_VERSION_MAJOR`、`AREX_DECO_API_VERSION_MINOR` 和
`AREX_DECO_API_VERSION_PATCH`。

该函数不需要输入参数，也不会返回状态码。集成方可在启动时调用它，并
与头文件期望版本或交付包 `MANIFEST.txt` 中记录的版本比对；不一致时应
停止使用该库，避免头文件和静态库来自不同版本导致 ABI 假设失效。

### `arex_deco_make_default_config`

```c
ArexDecoStatus arex_deco_make_default_config(ArexDecoConfig* config);
```

输出默认配置到 `config`。

### `arex_deco_make_default_air_gas`

```c
ArexDecoStatus arex_deco_make_default_air_gas(const ArexDecoConfig* config, ArexDecoGas* gas);
```

输出默认空气气体：O2 21%，N2 79%，`max_ppo2_bar = AREX_DECO_DEFAULT_BOTTOM_PPO2_BAR (1.4)`。`max_depth_m` 由 `(max_ppo2_bar / oxygen_fraction)` 折算得到的 MOD 自动推导，因此**必须传入有效 `config`**——海水/淡水、水面气压会改变换算结果。0.0.4 前 `max_depth_m` 硬编码为 60 m，违反物理一致性（air @ 1.4 bar 实际 MOD ≈ 56.7 m）。

### `arex_deco_make_default_gas_plan`

```c
ArexDecoStatus arex_deco_make_default_gas_plan(const ArexDecoConfig* config, ArexDecoGasPlan* gas_plan);
```

输出默认气体计划：1 个空气气体（通过 `make_default_air_gas` 推导），active gas 为 0。同样必须传入有效 `config`。

### `arex_deco_reset_tissue_to_surface`

```c
ArexDecoStatus arex_deco_reset_tissue_to_surface(
    const ArexDecoConfig* config,
    const ArexDecoGas* surface_gas,
    ArexDecoTissueState* tissue);
```

按水面压强和水面气体初始化组织舱。

输入：

- `config`
- `surface_gas`

输出：

- `tissue`

### `arex_deco_make_initial_dive_state`

```c
ArexDecoStatus arex_deco_make_initial_dive_state(ArexDecoDiveState* state);
```

创建完整初始状态，包括默认配置、默认空气气体计划、水面组织舱、当前深度 0。

### `arex_deco_validate_config`

```c
ArexDecoStatus arex_deco_validate_config(const ArexDecoConfig* config);
```

校验配置是否合法。

### `arex_deco_validate_gas`

```c
ArexDecoStatus arex_deco_validate_gas(const ArexDecoConfig* config, const ArexDecoGas* gas);
```

校验气体比例、深度范围、最大 PO2 与 MOD 物理约束。0.0.4 起新增 `config` 参数：

- `max_ppo2_bar` 拒绝高于 `AREX_DECO_MAX_ALLOWABLE_PPO2_BAR (2.0)`——绝对生理硬顶。1.4 / 1.6 等行业策略限值不在 core 拦截，由 UI 层提示。
- `max_depth_m` 拒绝超过 `MOD(config, gas)`——禁止用户手工把 air 的 max depth 改到 100 m 等致死配置通过校验。允许更保守（更浅）。

### `arex_deco_calculate_gas_mod`

```c
ArexDecoStatus arex_deco_calculate_gas_mod(
    const ArexDecoConfig* config,
    const ArexDecoGas* gas,
    float* mod_m);
```

按 core 当前压力模型计算气体 MOD，公式等价于
`(gas.max_ppo2_bar / gas.oxygen_fraction - config.surface_pressure_bar) * config.water_meters_per_bar`，
并在结果小于 0 时钳制为 0。该接口用于外部 UI 展示 MOD、限制切换深度输入，并与
`validate_gas()` / 切气推荐保持同一口径。

该函数会校验 `config`、气体比例、`oxygen_fraction > 0` 以及
`max_ppo2_bar <= AREX_DECO_MAX_ALLOWABLE_PPO2_BAR`，但不会要求
`gas.max_depth_m <= MOD`。调用方可以先用它获得 MOD，再把 `max_depth_m` 或产品层
切换深度设置为不超过该值。

### `arex_deco_calculate_gas_density`

```c
ArexDecoStatus arex_deco_calculate_gas_density(
    const ArexDecoConfig* config,
    const ArexDecoGas* gas,
    float depth_m,
    float temperature_kelvin,
    float compressibility_z,
    float* density_g_per_l);
```

按实际气体状态方程计算当前深度的气体密度，单位为 `g/L`。`temperature_kelvin`
和 `compressibility_z` 由调用方传入；core 只返回物理数值，不做预警分类。

### `arex_deco_step`

```c
ArexDecoStatus arex_deco_step(
    const ArexDecoDiveState* state,
    const ArexDecoStepInput* input,
    ArexDecoDiveState* next_state,
    ArexDecoRuntimeMetrics* metrics);
```

按深度段推进组织舱和氧暴露。

输入：

- `state`：当前完整潜水状态
- `input`：起始深度、结束深度、持续时间、气体索引

输出：

- `next_state`：推进后的完整潜水状态
- `metrics`：该状态下的实时指标

约束：

- `input.gas_index` 必须在 `state.gas_plan.gas_count` 范围内
- 深度不能为负
- 持续时间可以为 0（仅刷新 metrics，不推进组织舱、不累计时间）

### `arex_deco_step_pressure`

```c
ArexDecoStatus arex_deco_step_pressure(
    const ArexDecoDiveState* state,
    const ArexDecoPressureStepInput* input,
    ArexDecoDiveState* next_state,
    ArexDecoRuntimeMetrics* metrics);
```

按绝对压强段推进组织舱和氧暴露。适合嵌入式设备直接从压力传感器输入。

输入输出与 `arex_deco_step` 类似，但使用 `start_pressure_bar` / `end_pressure_bar`。

### `arex_deco_plan`

```c
ArexDecoStatus arex_deco_plan(
    const ArexDecoDiveState* state,
    ArexDecoSchedule* schedule,
    ArexDecoGasRecommendation* gas_rec);
```

从当前状态计算减压计划。

输入：

- `state`：当前完整潜水状态，包含配置、气体、组织舱、当前深度
- `gas_rec` 可传 `NULL`，表示不需要气体推荐

输出：

- `schedule`：停站列表和 TTS
- `gas_rec`：如果不为 `NULL`，输出当前深度的最佳安全可用气体推荐

Planner 行为：

- 强制减压义务按 `gf_high` 天花板判断；没有强制减压、`safety_stop_enabled == 1`、本次最大深度超过 `AREX_DECO_SAFETY_STOP_TRIGGER_DEPTH_M`、且当前深度深于 `AREX_DECO_SAFETY_STOP_DEPTH_M` 时，生成安全停留。默认安全停留深度为 5 m；调用方只可通过 `safety_stop_seconds` 参数化停留时长。英制展示由 UI / App 产品层从米换算。
- 上升时间按 Subsurface 风格四段 `ascent_rate` profile 计算。Planner 先由
  `depth_time_m_seconds / elapsed_seconds` 得出平均深度，再按当前深度是否
  大于平均深度 75%、大于平均深度 50%、深于 6 m、6 m 及以内选择对应速率；
  跨越边界的一次上升会被拆成多段分别计算。
- `arex_deco_plan()` 会拒绝明显不一致的 depth-time 状态，并返回
  `AREX_DECO_STATUS_INVALID_STATE`，避免用失真的平均深度参与四段速率选择。
- 首停深度由当前 ceiling、`last_stop_m` 和 `deco_step_m` 计算，并锚定在 `last_stop_m + k * deco_step_m` 网格上。若 `last_stop_m` 不是 `deco_step_m` 的整数倍（例如 4.5 m / 3 m），相邻停站仍保持完整 `deco_step_m` 间距。
- 实时上升过程中重新计划时，若当前深度已浅于理论 ceiling 网格首停但仍可合法停在“不深于当前深度”的最深网格站，planner 会保持首停在配置网格上，避免输出 8.8 m、5.9 m 这类由当前深度夹逼出的非网格停站。
- GF 插值锚点由第一个实质停留站确立：planner 会先模拟经过候选网格站，只有当该站实际需要 `>= AREX_DECO_STOP_TIME_GRANULARITY_SECONDS` 的停留时，才把它锁定为本次计划的 first stop GF anchor；0 s 候选站不会污染后续 GF 斜率。
- 中间停站按 `deco_step_m` 递减。
- 到达 `last_stop_m` 后，不再生成更浅停站；若 `last_stop_m` 深于常规 step 网格（例如 6 m），最后一站会延长到满足直接升水条件。
- 当较深 `last_stop_m` 与标准 step 网格对齐时，planner 会用一条“继续按常规 step 停到底”的 staged alt-plan 估算最后一站下界，避免直接升水求解低估保守停留时间。若该 alt-plan 在 6 h 单站上限内仍不可行，`arex_deco_plan` 返回 `AREX_DECO_STATUS_INVALID_STATE`，而不是输出被饱和值污染的计划。
- 气体切换会计入同深度额外停留，默认 60 s。该值写入停站的 `switch_penalty_seconds`，并计入 `duration_seconds`。
- 气体切换惩罚期间按新气体推进组织舱，且该惩罚计入新气体所在停站的 `duration_seconds`。
- `tts_seconds` 包含停站时间、上升时间、安全停留和气体切换惩罚。
- 若计划时当前深度浅于 GF-high ceiling，`schedule.ceiling_violated` 置 1。
  这种情况下 planner 仍描述“从当前位置到水面”的未来路径；如果潜水员已在水面，
  路径可以为空且 `stops == 0 && tts_seconds == 0`。产品层不得只用
  `stops == 0` 或 `tts_seconds == 0` 判断安全，必须同时确认
  `ceiling_violated == 0`。
- 单站时长上限 6 h（0.0.3 起）。若该上限内仍无法满足 ceiling 约束，返回 `AREX_DECO_STATUS_INVALID_STATE`。

### `arex_deco_select_runtime_stop`

```c
ArexDecoStatus arex_deco_select_runtime_stop(
    const ArexDecoSchedule* schedule,
    const ArexDecoRuntimeStopSelectorState* previous_state,
    const ArexDecoRuntimeStopSelectorInput* input,
    ArexDecoRuntimeStopSelectorState* next_state,
    ArexDecoRuntimeStop* output);
```

从 raw `schedule` 中选择本帧用户应该看到的 runtime current stop。

该接口不修改 `schedule`，不重算 TTS，不推进组织舱，也不改变
`ArexDecoDiveState`。它是显式 state-in/state-out 纯函数：调用方保存
`next_state`，下一帧作为 `previous_state` 传回；`previous_state == NULL` 表示
selector 初始化。

版本和时间语义：

- `input->api_version` 必须等于当前 `arex_deco_get_api_version()`，否则返回
  `AREX_DECO_STATUS_UNSUPPORTED_VERSION`。
- `previous_state->api_version` 不匹配当前版本时，selector 将其视为无上一帧状态，不返回错误。
- `elapsed_seconds` 与上一帧相同表示同一潜水秒内重复调用；如发生极薄 hysteresis hold，
  hold age 不会增加。
- `elapsed_seconds` 小于上一帧表示 replay/潜水时间回退，selector 将上一帧状态视为无效。

候选规则：

- 每帧先从当前 raw schedule 重新计算 projected stop。
- 只考虑 `kind == MANDATORY`、`depth_m > 0`、`hold_seconds > 0` 且
  `runtime_seconds_for_stop > 0` 的停站。
- 带 `AREX_DECO_STOP_FLAG_DISPLAY_SUPPRESSED` 的 raw stop 不进入 projection。
- `ROUTE_WAYPOINT`、`GAS_SWITCH` 和 `SAFETY` 不进入强制减压 current-stop selector。
  安全停留继续使用 `arex_deco_safety_stop()`。
- `hold_seconds >= promote_min_seconds` 的候选为实质候选；更短的 mandatory stop
  是 short candidate。
- first mandatory 若为 short：唯一 mandatory 时可显示；潜水员已在该 stop zone 内时可显示；
  没有实质 mandatory 替代时可显示；否则显示第一个实质 mandatory。

projection / hysteresis 语义：

- 默认策略为 stateless mandatory projection；`stable_seconds = 0` 可完全关闭 hysteresis。
- `stable_seconds > 0` 时只启用 1-2 s 极薄 hysteresis，实际值会夹到最大 2 s。
- hold previous 前必须在当前 schedule 中重新找到同 depth/gas 的有效、非 short
  mandatory candidate；`output.source_raw_index` 使用当前 schedule 中匹配到的 raw index。
- 如果 previous stop 在当前 schedule 中不再是有效 mandatory、变成 short、变成
  `ROUTE_WAYPOINT` 或带 `DISPLAY_SUPPRESSED`，selector 必须立即切到本帧 projection。
- 如果 projected stop 是更深的实质 mandatory，selector 不会长期 hold 旧 stop。
- selector 不再使用 `candidate_active/candidate_seen_seconds` 做 10 s debounce 或 stable promotion。

`output` 是固件主屏当前减压站、bridge current stop 和 DLF/APP `next_stop` 的官方语义。
`schedule.tts_seconds` 仍是唯一 TTS 来源，不得用 selector 输出重算。runtime stop 不是
safety proof layer；安全边界来自 planner schedule、hard ceiling、tissue/GF 重算和违规提示。

安全停留边界：

- `arex_deco_plan()` 中的安全停留站仍是“如果现在开始上升”的预测计划项，用于
  TTS / 气量 / 路径估算。
- Runtime 倒计时、有效区间、过浅暂停和完成状态应读取 `arex_deco_safety_stop()`。
- 是否存在强制减压义务由 core 内部按 GF-high ceiling 判断；调用方不需要自行用
  NDL / ceiling 判断安全停留是否应显示或计时。

纯切气预测站：

- 当某站 `hold_seconds == 0 && switch_penalty_seconds > 0` 时，该站在物理上不需要额外脱气停留，只是 planner 对未来同深度切气动作的预测锚点。
- Runtime UI 不应把纯切气预测站渲染成减压倒计时表盘；应通过 `arex_deco_recommend_gas()` 和产品层门控展示切气确认提示。用户确认后用 0 秒 `arex_deco_step()` 更新 active gas 并重新 plan，该站通常会因 penalty 清零而消失。
- 静态计划、日志、计划详情不应隐藏纯切气预测站的时间，可显示为“切气 +N s -> Gas X”。因为该时间已经计入 `tts_seconds`，隐藏会导致各站时间与 TTS 对不上。

Planning 与 Runtime 语义：

- 静态计划、TTS、CNS / OTU 预测、气量估算和 runtime 当前站主倒计时应使用 `duration_seconds` / `tts_seconds`，因为它们代表“按当前计划执行”的连续预测。
- 实时水下 UI 不应把 `switch_penalty_seconds` 做成独立强制倒计时；它已经包含在当前站 `duration_seconds` 内。当前站可通过 `arex_deco_recommend_gas()` 获取当前深度最佳安全可用气体；产品层确认需要展示切气提示后，由潜水员确认，再调用 `arex_deco_step()` 的 0 秒 step，把 `gas_index` 改成推荐气体，随后重新 plan。
- 确认切气后，新的 `active_gas_index` 已等于推荐气体，当前站的 `switch_penalty_seconds` 会清零；由于切气 penalty 期间原本已按新气体推进组织舱，重新计划后的 `duration_seconds` 通常会比单独显示 `hold_seconds` 更平顺，避免确认切气后当前站倒计时向上跳变。
- 若潜水员拒绝或该气体不可用，产品层应保持 active gas 不变，并在传入 core 的 gas plan 中禁用/移除该气体后重新 plan；core 不在 `ArexDecoDiveState` 内保存“拒绝切气”策略状态。

停站时间秒级输出与重算闭环：

- 组织舱推进、ceiling、GF99、SurfGF 等底层计算仍使用连续时间的 Bühlmann ZHL-16C 数学模型。
- Planner 的二分求解器先计算满足“离开当前站到下一站 / 水面”所需的最小数学停留时间。
- 二分求解固定 24 次迭代，6 h 上限下时间分辨率约 1.29 ms，输出前按严格上界取到整数秒。
- 如果 0 s 已满足离开条件，该深度通常不会写入 `schedule->stops`，但 planner 仍会继续模拟上升到下一深度。
- 当较深的实质停站已经确立 GF anchor 后，后续中间网格站若数学停留为 0 s，planner 会先验证在该站停留最小输出粒度后仍满足离开到下一站的 ceiling 约束；验证通过时输出一个 `AREX_DECO_STOP_TIME_GRANULARITY_SECONDS` 的安全 waypoint，避免实时重复调用 `arex_deco_plan()` 时出现 9 m / 3 m 这类跳过 6 m 的断档计划。该 waypoint 会参与 tissue、氧暴露和 TTS 递推，不是纯 UI 展示项。
- UI / adapter 不应直接把带有 `AREX_DECO_STOP_FLAG_DISPLAY_SUPPRESSED` 的
  `18 m / 1 s`、`15 m / 1 s`、`6 m / 1 s` 等 raw stop 当作当前实质停站展示；
  产品层应保留 raw `schedule->stops`，并按 `kind/flags` 派生 effective/display stop。
  不再需要按“前 N 个短站”“小于若干秒”等 heuristic 猜测 waypoint。
- 如果需要停留，Core 将该停站时间输出为整数秒。例如 12 s 会输出为 12 s，而不会在核心算法层抬到 60 s。
- Core 会用该秒级停留时间继续推进组织舱和氧暴露，再计算后续更浅停站。
- 因此 `tts_seconds`、后续停站时间、气体使用估算和跨语言移植结果都必须基于“求解 -> 秒级 strict-ceil -> 重算”的闭环，不能在 core 层额外套用分钟级取整。

### `arex_deco_safety_stop`

```c
ArexDecoStatus arex_deco_safety_stop(
    const ArexDecoDiveState* state,
    ArexDecoSafetyStopStatus* safety_stop);
```

读取当前安全停留 runtime 状态。

该接口不推进组织舱，也不修改 `state`。安全停留累计计时由 `arex_deco_step()` 在
真实时间推进时更新；固件应在每次 tick 后调用本接口读取状态。core 会在内部判断
当前是否存在 GF-high 强制减压 ceiling；若存在，`phase` 返回
`SUPPRESSED_BY_DECO`，调用方不需要自行判断是否为免减压状态。

输入：

- `state`：当前完整潜水状态

输出：

- `safety_stop`：安全停留执行状态

当前固定策略：

- 触发阈值：最大深度深于 10 m
- 目标深度：5 m
- 有效计时区间：2.9-6 m
- 过浅 missed 阈值：浅于 2 m
- 计时时长：`state->config.safety_stop_seconds`
- 一旦浅于 missed 阈值，本次安全停留进入 `MISSED_TOO_SHALLOW` 终态；即使随后重新下潜到有效区间，也不会恢复倒计时。
- 若重新下潜深于触发深度 10 m，安全停留执行状态会重置，并重新要求完整安全停留。
- 若本次潜水已经产生过强制减压义务（`was_deco_dive == 1`）或当前存在 GF-high ceiling，安全停留由 core 报告为 `SUPPRESSED_BY_DECO`。

### `arex_deco_recommend_gas`

```c
ArexDecoStatus arex_deco_recommend_gas(
    const ArexDecoDiveState* state,
    ArexDecoGasRecommendation* gas_rec);
```

按当前真实状态输出最佳安全可用气体推荐，不生成减压计划，也不修改组织舱状态。

该接口不保存隐藏会话状态，也不判断产品生命周期。`available == 1` 只表示
“当前深度存在比 active gas 更优的安全可用气体”，不等价于“UI 必须立即弹窗”。
是否展示 `BETTER GAS AVAILABLE` / "Switch gas?" 应由产品层结合是否已入水、
是否处于潜水中、提示抑制策略、用户是否拒绝该气体等状态决定。

推荐用法：

- 每次 `arex_deco_step()` 后调用一次，用于获得当前深度的最佳安全可用气体。
- `available == 1` 时，产品层可在自己的潜水生命周期和提示策略允许时提示潜水员确认；确认后用 0 秒 `arex_deco_step()` 更新 active gas。
- `available == 0` 且 `is_emergency_no_safe_gas == 0` 表示保持当前气体。
- `is_emergency_no_safe_gas == 1` 表示当前没有安全可用气体，应作为告警处理。
- 输入的 `ArexDecoDiveState` 必须是宿主侧持续 `arex_deco_step()` 推进后的最新真实状态；不要复用旧计划或旧组织舱快照调用该接口，否则切气深度附近的推荐时机可能早出或晚出。

该接口与 `arex_deco_plan()` 的职责不同：

- `arex_deco_recommend_gas()` 回答当前深度下“现在是否有更优且安全的气体”，适合在实时 tick 后高频调用。
- `arex_deco_plan()` 回答“从当前状态开始，如果按可用气体和规划策略上升，未来停站、TTS、氧暴露会怎样”，其中未来切气延迟会进入 `switch_penalty_seconds` 和 `tts_seconds`。
- 如果设备为了性能降低 `arex_deco_plan()` 调用频率，仍可在每次 `arex_deco_step()` 后调用 `arex_deco_recommend_gas()`，避免当前深度的切气推荐被低频计划刷新延迟。

### `arex_deco_nofly`

```c
ArexDecoStatus arex_deco_nofly(
    const ArexDecoDiveState* state,
    uint32_t* nofly_seconds);
```

估算当前状态后的禁飞时间。

输入：

- `state`

输出：

- `nofly_seconds`

### `arex_deco_calculate_tissue_pressures`

```c
ArexDecoStatus arex_deco_calculate_tissue_pressures(
    const ArexDecoDiveState* state,
    ArexDecoTissuePressureMetrics* metrics);
```

输出当前环境压力下的组织压力快照。该接口只返回算法侧物理真值和当前有效 GF，不做 UI 归一化，不推进组织舱，也不修改 `state`。

输入：

- `state`：当前完整潜水状态，包含配置和组织舱惰性气体压强。

输出：

- `metrics->ambient_pressure_bar`：当前环境绝对压力。
- `metrics->inspired_n2_bar` / `metrics->inspired_he_bar`：当前活跃气体的肺泡吸入惰性气体分压，使用 `max(P_amb - water_vapor_pressure_bar, 0)` 口径。
- `metrics->tissue_n2_bar[16]` / `metrics->tissue_he_bar[16]`：当前 16 仓组织惰性气体分压原值。
- `metrics->tissue_m_value_bar[16]`：当前 `P_amb` 下按 combined a/b 计算的 Bühlmann M 值。
- `metrics->tissue_m_gf_bar[16]`：`P_amb + (M_i - P_amb) * current_gf_target`。
- `metrics->current_gf_target`：算法内部判定的当前目标 GF，使用 `0.0f ~ 1.0f` 小数形式。

GF target 语义：

- 当 `gf_high` ceiling 尚未产生强制减压义务时，`current_target_gf = config.gf_high`。
- 只有进入强制减压后，core 才会求首停；当前深度深于或等于首停时使用 `config.gf_low`，浅于首停时在 `gf_low` 和 `gf_high` 之间按深度线性插值。
- `current_gf_target` 与 `tissue_m_gf_bar` 使用同一个值，UI 不重复实现 GF 插值。

UI 显示建议：

- core 只输出物理量，不输出百分比、柱长或屏幕坐标。
- Tissue Graph 可使用固定锚点分段线性映射：`0` 为零压力，`400` 为 `P_amb`，`900` 为当前模式红线，`1000` 为超限边界。
- `limit_i` 表示当前模式下红线分母对应的压力值，必须与红线选择逻辑一致：RAW 模式为 `tissue_m_value_bar[i]`；GF 模式且 UI 有 planner stop 上下文时为 `P_amb + (tissue_m_value_bar[i] - P_amb) * stop.target_gf`；GF 模式且无 stop 上下文时为 `tissue_m_gf_bar[i]`。
- 对每个组织仓，`P_tissue_i = tissue_n2_bar[i] + tissue_he_bar[i]`。当 `P_tissue_i <= P_amb` 时，柱长为 `(P_tissue_i / P_amb) * 400`；当 `P_tissue_i > P_amb` 时，柱长为 `400 + ((P_tissue_i - P_amb) / (limit_i - P_amb)) * 500`，最后截断到 `0..1000`。
- 肺泡惰性气体参考线位置为 `(inspired_n2_bar + inspired_he_bar) / P_amb * 400`。
- Tissue Graph 的 leading compartment 可按当前模式下 `GF_i = 0`（`P_tissue_i <= P_amb`）或 `GF_i = (P_tissue_i - P_amb) / (limit_i - P_amb) * 100`（`P_tissue_i > P_amb`）选最大值；若全部为 0，则选 `P_tissue_i` 最大的仓。

约束：

- `state` 和 `metrics` 不能为空。
- `state->config` 必须通过 `arex_deco_validate_config()`。
- 当前 active gas index 必须有效，active gas 必须 enabled 且通过 `arex_deco_validate_gas()`。
- `state->current_depth_m` 必须为有限非负深度。
- 若 tissue state 含 NaN、负数、`n2 + he <= 0`，或 M 值约束不成立，返回 `AREX_DECO_STATUS_INVALID_STATE`，且不写出半成品。

### `arex_deco_forecast_tts_hold`

```c
ArexDecoStatus arex_deco_forecast_tts_hold(
    const ArexDecoDiveState* state,
    uint32_t hold_seconds,
    ArexDecoTtsForecast* forecast);
```

生成 TTS hold 动态应急预测。使用当前 active gas 在当前深度前向积分
`hold_seconds` 后重新规划，并返回当前 TTS、预测 TTS 和 delta。前向积分会同时推进
惰性气体 tissue 和 CNS / OTU 氧暴露，避免漏算 hold 段对后续
`end_of_dive_exposure` 的影响。

该接口计算开销高：为了得到当前 TTS 和 hold 后 TTS，会执行当前计划和未来计划两次
planner 路径。宿主固件不应把它放进 1 Hz 或更高频的主实时 tick；建议后台低频刷新，
例如每 5 秒一次，或仅在用户打开应急预测页面时刷新。

### `arex_deco_forecast_ndl_excursion`

```c
ArexDecoStatus arex_deco_forecast_ndl_excursion(
    const ArexDecoDiveState* state,
    float delta_depth_m,
    ArexDecoNdlExcursionForecast* forecast);
```

生成 NDL 深度试探预测。该接口不调用 planner，适合比 TTS hold 更高频刷新。
NDL ±delta 永远使用当前 active gas；上移深度钳制到 0 m；下移深度不因 MOD
超限而被 core 拦截。若当前已有 ceiling，所有 NDL 字段返回 0。`delta_depth_m`
若在 0 的数值容差内略小于 0 会被钳制为 0；明显负值返回 invalid argument。

## 嵌入式数值与 ABI 注意事项

### 浮点精度策略

- core 公共 ABI 全部使用 `float`，方便单精度 FPU 直通。
- 0.0.4 起 `tissue_model` 内部完整切换为单精度 + `expm1f` 形式：避免 `1 - e^(-kt)` 在慢舱 / 短步长下尾数损失，避免 Schreiner 方程教科书形式中 `R/k` 项的灾难性消去（duration → 0、变压速率大时）。
- ZHL-16C 系数表新增单精度镜像字段（`Zhl16cCoefficients::n2_k_per_sec_f[]` / `he_k_per_sec_f[]`），由初始化时下转一次，热路径循环零 cast、纯 float FPU 直通。
- 整数累加路径：`elapsed_seconds`（uint32, ~136 年量级），`oxygen_exposure.cns_percent` / `oxygen_exposure.otu` 在长时间高 ppO2 下用 float 累加误差经实测在临床安全余量内（8h 1.6 bar 暴露漂移 ~0.03%）。

### 指针别名规则

`arex_deco_step` / `arex_deco_step_pressure` 允许 `next_state == state` 做原位更新（0.0.4 起在所有 duration 路径上都有别名守卫）。但出于性能与可读性，建议传入不同地址，让编译器优化更激进。

`tissue_apply_*` 内部同样支持 `in == out`，但作为内部接口该约束不对外保证。

### ABI 大小

| 结构体 | 大小（字节） |
|---|---:|
| `ArexDecoVersion` | 6 |
| `ArexDecoConfig` | 68 |
| `ArexDecoGas` | 48 |
| `ArexDecoGasPlan` | 312 |
| `ArexDecoTissueState` | 136 |
| `ArexDecoOxygenExposure` | 32 |
| `ArexDecoDiveState` | 600 |
| `ArexDecoStepInput` | 36 |
| `ArexDecoPressureStepInput` | 36 |
| `ArexDecoRuntimeMetrics` | 52 |
| `ArexDecoTissuePressureMetrics` | 304 |
| `ArexDecoGasRecommendation` | 44 |
| `ArexDecoStop` | 36 |
| `ArexDecoSchedule` | 1500 |
| `ArexDecoTtsForecast` | 48 |
| `ArexDecoNdlExcursionForecast` | 48 |
| `ArexDecoSafetyStopStatus` | 64 |

所有大小由 `core/src/arex_deco_abi_checks.cpp` 的 `static_assert` 强制保证。0.0.3 → 0.0.4 无 ABI 字节大小变化，只有字段语义/名称变更（`in_deco` → `was_deco_dive`）和函数签名变更。

### 跨平台一致性

- 所有惰性气体压强、ceiling、GF99、SurfaceGF 计算严格按照 ZHL-16C + GF 模型（Subsurface 参考实现对齐）。
- `tests/data/profiles/l2_static_profiles.json` 是冻结的回归快照，浮点容差 `1e-5 bar`。MCU 移植后必须能在该容差内复现。
- Planner 的"求解 → 秒级 strict-ceil → 重算"闭环（见 `arex_deco_plan` 章节）必须按相同顺序复现，否则 TTS 与后续停站会偏移。

## 嵌入式典型调用流程

### 初始化

```c
ArexDecoDiveState state;
ArexDecoStatus status = arex_deco_make_initial_dive_state(&state);
if (status != AREX_DECO_STATUS_OK) {
    // handle error
}

state.config.gf_low = 0.30f;
state.config.gf_high = 0.85f;
state.config.deco_step_m = 3.0f;
state.config.last_stop_m = 6.0f;
```

### 实时推进

```c
ArexDecoStepInput input = {0};
input.start_depth_m = state.current_depth_m;
input.end_depth_m = 30.0f;
input.duration_seconds = 60;
input.gas_index = state.gas_plan.active_gas_index;

ArexDecoDiveState next_state;
ArexDecoRuntimeMetrics metrics;
status = arex_deco_step(&state, &input, &next_state, &metrics);
if (status == AREX_DECO_STATUS_OK) {
    state = next_state;
}
```

### 预测减压计划

```c
ArexDecoSchedule schedule;
status = arex_deco_plan(&state, &schedule, NULL);
if (status == AREX_DECO_STATUS_OK) {
    for (uint8_t i = 0; i < schedule.stop_count; ++i) {
        const ArexDecoStop* stop = &schedule.stops[i];
        // stop->duration_seconds: planning total
        // stop->hold_seconds: physical hold only
        // stop->switch_penalty_seconds: planning-only switch delay
        // stop->kind: algorithm stop kind
        // stop->flags: runtime display hints only
    }
}
```

实时 UI 消费建议：

- tick 内先用 `arex_deco_step()` 推进真实状态，再用 `arex_deco_recommend_gas()` 获取当前深度最佳安全可用气体，最后按产品需要调用 `arex_deco_plan()` 刷新计划。
- TTS 显示使用 `schedule.tts_seconds`。该值包含未来上升、停站、安全停留和预测切气延迟，适合做保守总时间估计。
- 当前站主倒计时应绑定第一个未设置
  `AREX_DECO_STOP_FLAG_DISPLAY_SUPPRESSED` 的 stop 的 `duration_seconds`。不要只绑定
  `hold_seconds`，否则在 pending 切气被确认、`switch_penalty_seconds` 清零后，隐藏在
  penalty 中的新气体脱气时间会回到 `hold_seconds`，可能导致倒计时向上跳变。
- `hold_seconds` 和 `switch_penalty_seconds` 可用于拆分展示、调试或计划详情；若只显示一个当前站剩余时间，应显示 `duration_seconds`。
- 若当前首站 `kind == AREX_DECO_STOP_KIND_GAS_SWITCH` 或设置了
  `AREX_DECO_STOP_FLAG_DISPLAY_SUPPRESSED`，Runtime 主界面应隐藏该站减压倒计时；
  纯切气预测站只保留切气提示，静态计划和日志中可显示为“切气 +N s”。
- 用户确认切气后，用 0 秒 `arex_deco_step()` 更新 active gas，再重新调用 `arex_deco_plan()`。此时当前气体已是推荐气体，对应当前站的 `switch_penalty_seconds` 会按新状态重新计算，主倒计时继续绑定新的 `duration_seconds`。
- 用户拒绝或气体不可用时，产品层应保持 active gas 不变，并在传给 core 的 gas plan 中把该气体 `enabled` 设为 0 或移除该气体后重新 plan；core 不保存拒绝策略状态。

### Runtime 切气确认

```c
ArexDecoGasRecommendation gas_rec;
status = arex_deco_recommend_gas(&state, &gas_rec);
if (status == AREX_DECO_STATUS_OK &&
    gas_rec.available &&
    gas_rec.recommended_gas_index >= 0) {
    // Product layer may prompt the diver here after dive-lifecycle gating.
}

ArexDecoStepInput switch_input = {0};
switch_input.start_depth_m = state.current_depth_m;
switch_input.end_depth_m = state.current_depth_m;
switch_input.duration_seconds = 0;
switch_input.gas_index = gas_rec.recommended_gas_index;

ArexDecoDiveState switched_state;
ArexDecoRuntimeMetrics metrics;
status = arex_deco_step(&state, &switch_input, &switched_state, &metrics);
if (status == AREX_DECO_STATUS_OK) {
    state = switched_state;
}
```

## WASM 使用说明

WASM 构建导出的是同一套 C ABI 符号，外加版本和 sizeof helper。

### 额外导出函数

| 函数 | 说明 |
|---|---|
| `_malloc(size)` | 分配 WASM 内存 |
| `_free(ptr)` | 释放 WASM 内存 |
| `_arex_deco_wasm_version_major()` | WASM core 主版本 |
| `_arex_deco_wasm_version_minor()` | WASM core 次版本 |
| `_arex_deco_wasm_version_patch()` | WASM core patch |
| `_arex_deco_wasm_sizeof_config()` | `ArexDecoConfig` 字节大小 |
| `_arex_deco_wasm_sizeof_gas()` | `ArexDecoGas` 字节大小 |
| `_arex_deco_wasm_sizeof_gas_plan()` | `ArexDecoGasPlan` 字节大小 |
| `_arex_deco_wasm_sizeof_tissue_state()` | `ArexDecoTissueState` 字节大小 |
| `_arex_deco_wasm_sizeof_dive_state()` | `ArexDecoDiveState` 字节大小 |
| `_arex_deco_wasm_sizeof_step_input()` | `ArexDecoStepInput` 字节大小 |
| `_arex_deco_wasm_sizeof_runtime_metrics()` | `ArexDecoRuntimeMetrics` 字节大小 |
| `_arex_deco_wasm_sizeof_tissue_pressure_metrics()` | `ArexDecoTissuePressureMetrics` 字节大小 |
| `_arex_deco_wasm_sizeof_gas_recommendation()` | `ArexDecoGasRecommendation` 字节大小 |
| `_arex_deco_wasm_sizeof_stop()` | `ArexDecoStop` 字节大小 |
| `_arex_deco_wasm_sizeof_schedule()` | `ArexDecoSchedule` 字节大小 |
| `_arex_deco_wasm_sizeof_runtime_stop_selector_state()` | `ArexDecoRuntimeStopSelectorState` 字节大小 |
| `_arex_deco_wasm_sizeof_runtime_stop_selector_input()` | `ArexDecoRuntimeStopSelectorInput` 字节大小 |
| `_arex_deco_wasm_sizeof_runtime_stop()` | `ArexDecoRuntimeStop` 字节大小 |
| `_arex_deco_wasm_sizeof_tts_forecast()` | `ArexDecoTtsForecast` 字节大小 |
| `_arex_deco_wasm_sizeof_ndl_excursion_forecast()` | `ArexDecoNdlExcursionForecast` 字节大小 |
| `_arex_deco_wasm_sizeof_safety_stop_status()` | `ArexDecoSafetyStopStatus` 字节大小 |
| `_arex_deco_calculate_gas_mod()` | 按 core 口径计算气体 MOD |
| `_arex_deco_calculate_gas_density()` | 按 core 口径计算实时气体密度 |
| `_arex_deco_recommend_gas()` | 当前深度的最佳安全可用气体推荐 |
| `_arex_deco_select_runtime_stop()` | 从 raw schedule 中选择稳定 runtime current stop |
| `_arex_deco_calculate_tissue_pressures()` | 输出当前环境压力下 16 仓组织压力、M 值和 GF M 值 |
| `_arex_deco_forecast_tts_hold()` | 计算 TTS hold 动态应急预测 |
| `_arex_deco_forecast_ndl_excursion()` | 计算 NDL 深度试探预测 |
| `_arex_deco_safety_stop()` | 读取安全停留 runtime 状态 |

### WASM 调用规则

1. 加载 `arex_deco_core.js` 和 `arex_deco_core.wasm`。
2. 检查版本必须等于当前 JS adapter 期望版本。
3. 用 sizeof helper 校验 ABI 大小。
4. 用 `_malloc` 分配输入输出结构体内存。
5. 用 `DataView` / `HEAPU8` 按小端写入 POD 字段。
6. 调用 `_arex_deco_*` 函数。
7. 检查返回状态码。
8. 读取输出结构体。
9. 用 `_free` 释放临时内存。

### WASM 当前关键偏移

当前 `ArexDecoConfig` 关键字段偏移：

| 字段 | offset |
|---|---:|
| `api_version.major` | 0 |
| `surface_pressure_bar` | 8 |
| `water_vapor_pressure_bar` | 12 |
| `water_meters_per_bar` | 16 |
| `gf_low` | 20 |
| `gf_high` | 24 |
| `ascent_rate.rate_75_percent_m_per_min` | 28 |
| `ascent_rate.rate_50_percent_m_per_min` | 32 |
| `ascent_rate.rate_stops_m_per_min` | 36 |
| `ascent_rate.rate_last_6m_m_per_min` | 40 |
| `deco_step_m` | 44 |
| `last_stop_m` | 48 |
| `safety_stop_seconds` | 52 |
| `gas_switch_penalty_seconds` | 56 |
| `water_type` | 60 |
| `safety_stop_enabled` | 64 |

当前 `ArexDecoDiveState` 关键字段偏移：

| 字段 | offset |
|---|---:|
| `config` | 8 |
| `gas_plan` | 76 |
| `tissue` | 388 |
| `oxygen_exposure` | 524 |
| `current_depth_m` | 556 |
| `max_depth_m` | 560 |
| `depth_time_m_seconds` | 564 |
| `elapsed_seconds` | 568 |
| `was_deco_dive` | 572 |
| `safety_stop_required` | 573 |
| `safety_stop_completed` | 574 |
| `safety_stop_missed` | 575 |
| `safety_stop_elapsed_seconds` | 576 |
| `gf_anchor_depth_m` | 580 |
| `gf_anchor_valid` | 584 |

当前 `ArexDecoSchedule` 关键字段偏移：

| 字段 | offset |
|---|---:|
| `stop_count` | 6 |
| `truncated` | 7 |
| `ceiling_violated` | 8 |
| `tts_seconds` | 24 |
| `end_of_dive_exposure` | 28 |
| `stops` | 60 |

当前 `ArexDecoStop` 字段偏移：

| 字段 | offset |
|---|---:|
| `depth_m` | 0 |
| `duration_seconds` | 4 |
| `gas_index` | 8 |
| `target_gf` | 12 |
| `hold_seconds` | 16 |
| `switch_penalty_seconds` | 20 |
| `kind` | 24 |
| `flags` | 25 |

当前 `ArexDecoRuntimeStopSelectorState` 关键字段偏移：

| 字段 | offset |
|---|---:|
| `active` | 6 |
| `candidate_active` | 7 |
| `displayed_source_raw_index` | 8 |
| `candidate_source_raw_index` | 9 |
| `displayed_depth_m` | 12 |
| `displayed_remaining_seconds` | 16 |
| `displayed_total_seconds` | 20 |
| `displayed_gas_index` | 24 |
| `displayed_is_short` | 25 |
| `candidate_gas_index` | 26 |
| `candidate_depth_m` | 28 |
| `candidate_seen_seconds` | 32 |
| `last_elapsed_seconds` | 36 |

当前 `ArexDecoRuntimeStopSelectorInput` 关键字段偏移：

| 字段 | offset |
|---|---:|
| `current_depth_m` | 8 |
| `elapsed_seconds` | 12 |
| `stop_zone_half_width_m` | 16 |
| `promote_min_seconds` | 20 |
| `stable_seconds` | 24 |

当前 `ArexDecoRuntimeStop` 关键字段偏移：

| 字段 | offset |
|---|---:|
| `available` | 6 |
| `source_raw_index` | 7 |
| `reason` | 8 |
| `is_short` | 9 |
| `depth_m` | 12 |
| `remaining_seconds` | 16 |
| `total_seconds` | 20 |
| `gas_index` | 24 |

WASM adapter 必须以 sizeof helper 为准。任何 core ABI 变更都必须同步更新 JS offset 和 API 版本。
