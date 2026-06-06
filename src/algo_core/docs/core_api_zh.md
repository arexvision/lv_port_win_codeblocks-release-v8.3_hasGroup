# AREX Deco Core API 文档

本文档描述当前 core API。当前 API 版本为 `0.0.13`。

## 适用场景

- 嵌入式 / 原生 C/C++：直接包含 `arex_deco/arex_deco.h` 调用 C ABI。
- WASM / Web：通过 `bindings/wasm` 导出的同名 C ABI 函数和固定 POD 结构体内存布局调用。

core 只负责减压算法、组织舱状态、氧暴露、计划输出和禁飞时间估算。气瓶压力估算、UI 标注、日志导入、持久化等属于产品层。

## 版本与容量

版本宏：

- `AREX_DECO_API_VERSION_MAJOR = 0`
- `AREX_DECO_API_VERSION_MINOR = 0`
- `AREX_DECO_API_VERSION_PATCH = 13`

固定容量：

- `AREX_DECO_COMPARTMENT_COUNT = 16`
- `AREX_DECO_MAX_GAS_COUNT = 6`
- `AREX_DECO_MAX_DECO_STOP_COUNT = 40`

默认参数：

- `surface_pressure_bar = 1.01325`
- `water_vapor_pressure_bar = 0.0627`
- `water_meters_per_bar = 10.0` 海水默认
- `gf_low = 0.50`
- `gf_high = 0.70`
- `ascent_rate_m_per_min = 10.0`
- `ascent_rate_shallow_m_per_min = 3.0`
- `ascent_rate_shallow_start_m = 10.0`
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
| `ascent_rate_m_per_min` | `float` | m/min | 深水段上升速率 |
| `ascent_rate_shallow_m_per_min` | `float` | m/min | 浅水段上升速率 |
| `ascent_rate_shallow_start_m` | `float` | m | 浅水段切换深度 |
| `deco_step_m` | `float` | m | 中间减压站跨度 |
| `last_stop_m` | `float` | m | 最后减压站深度。Planner 到达该深度后不再生成更浅停站，而是在该站延长时间直到可升水 |
| `safety_stop_seconds` | `uint32_t` | s | 非强制减压时的安全停留时长 |
| `gas_switch_penalty_seconds` | `uint32_t` | s | 切换气体时附加的同深度停留 |
| `water_type` | `ArexDecoWaterType` | - | `SALT=0`，`FRESH=1` |
| `safety_stop_enabled` | `uint8_t` | - | 非强制减压安全停留开关，`1` 启用，`0` 关闭 |

校验规则：

- `surface_pressure_bar > 0`
- `0 <= water_vapor_pressure_bar < surface_pressure_bar`
- `water_meters_per_bar > 0`
- `0 < gf_low <= 1`
- `gf_low <= gf_high <= 1`
- `ascent_rate_m_per_min > 0`
- `ascent_rate_shallow_m_per_min > 0`
- `ascent_rate_m_per_min >= ascent_rate_shallow_m_per_min`
- `ascent_rate_shallow_start_m >= 0`
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
| `elapsed_seconds` | `uint32_t` | s | 已经过时间 |
| `was_deco_dive` | `uint8_t` | - | 本次潜水是否曾触发减压义务（latched bit）。一旦 `metrics->ceiling_depth_m > 0` 则置 1，并保持到 `arex_deco_reset_tissue_to_surface` / `arex_deco_make_initial_dive_state` 重置。**该字段服务过去式语义**（影响 nofly 等下限），不要用作 UI 实时 "DECO NOW" 指示——实时义务请读 `metrics->ceiling_depth_m`。0.0.4 起字段名由 `in_deco` 重命名为 `was_deco_dive` 以消除时态歧义；内存布局不变 |

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

计划时的气体切换建议。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `available` | `uint8_t` | - | 1 表示有推荐切换 |
| `recommended_gas_index` | `int8_t` | - | 推荐气体索引 |
| `active_gas_index` | `int8_t` | - | 当前气体索引 |
| `depth_m` | `float` | m | 评估深度 |
| `ppo2_bar` | `float` | bar | 推荐气体在该深度的 PO2 |

### `ArexDecoStop`

单个减压停站。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `depth_m` | `float` | m | 停站深度 |
| `duration_seconds` | `uint32_t` | s | 停留时间。`arex_deco_plan()` 输出秒级停站时间；核心算法层不做分钟级取整 |
| `gas_index` | `int8_t` | - | 停站使用气体 |
| `target_gf` | `float` | 0-1 | 该停站目标 GF |

### `ArexDecoSchedule`

计划输出。

| 字段 | 类型 | 单位 | 说明 |
|---|---|---:|---|
| `api_version` | `ArexDecoVersion` | - | API 版本 |
| `stop_count` | `uint8_t` | - | 有效停站数量 |
| `truncated` | `uint8_t` | - | 1 表示停站数组容量不足被截断 |
| `tts_seconds` | `uint32_t` | s | Time To Surface，包括停站和上升时间 |
| `end_of_dive_exposure` | `ArexDecoOxygenExposure` | - | 若严格按此计划升水，出水时预测累计 CNS / OTU |
| `stops[40]` | `ArexDecoStop[]` | - | 固定容量停站数组 |

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
- `gas_rec`：如果不为 `NULL`，输出当前深度的推荐切换气体

Planner 行为：

- 强制减压义务按 `gf_high` 天花板判断；没有强制减压、`safety_stop_enabled == 1`、本次最大深度超过 `AREX_DECO_SAFETY_STOP_TRIGGER_DEPTH_M`、且当前深度深于 `AREX_DECO_SAFETY_STOP_DEPTH_M` 时，生成安全停留。默认安全停留深度为 5 m；调用方只可通过 `safety_stop_seconds` 参数化停留时长。英制展示由 UI / App 产品层从米换算。
- 上升时间按深水段 / 浅水段两段速率计算，默认 10 m/min 到 10 m，10 m 以内 3 m/min。
- 首停深度由当前 ceiling、`last_stop_m` 和 `deco_step_m` 计算，并锚定在 `last_stop_m + k * deco_step_m` 网格上。若 `last_stop_m` 不是 `deco_step_m` 的整数倍（例如 4.5 m / 3 m），相邻停站仍保持完整 `deco_step_m` 间距。
- 实时上升过程中重新计划时，若当前深度已浅于理论 ceiling 网格首停但仍可合法停在“不深于当前深度”的最深网格站，planner 会保持首停在配置网格上，避免输出 8.8 m、5.9 m 这类由当前深度夹逼出的非网格停站。
- 中间停站按 `deco_step_m` 递减。
- 到达 `last_stop_m` 后，不再生成更浅停站；若 `last_stop_m` 深于常规 step 网格（例如 6 m），最后一站会延长到满足直接升水条件。
- 当较深 `last_stop_m` 与标准 step 网格对齐时，planner 会用一条“继续按常规 step 停到底”的 staged alt-plan 估算最后一站下界，避免直接升水求解低估保守停留时间。若该 alt-plan 在 6 h 单站上限内仍不可行，`arex_deco_plan` 返回 `AREX_DECO_STATUS_INVALID_STATE`，而不是输出被饱和值污染的计划。
- 气体切换会计入同深度额外停留，默认 60 s，并合并到该停站的 `duration_seconds`。
- 气体切换惩罚期间按新气体推进组织舱，且该惩罚计入新气体所在停站的 `duration_seconds`。
- `tts_seconds` 包含停站时间、上升时间、安全停留和气体切换惩罚。
- 单站时长上限 6 h（0.0.3 起）。若该上限内仍无法满足 ceiling 约束，返回 `AREX_DECO_STATUS_INVALID_STATE`。

停站时间秒级输出与重算闭环：

- 组织舱推进、ceiling、GF99、SurfGF 等底层计算仍使用连续时间的 Bühlmann ZHL-16C 数学模型。
- Planner 的二分求解器先计算满足“离开当前站到下一站 / 水面”所需的最小数学停留时间。
- 二分求解固定 24 次迭代，6 h 上限下时间分辨率约 1.29 ms，输出前按严格上界取到整数秒。
- 如果 0 s 已满足离开条件，该深度不会写入 `schedule->stops`，但 planner 仍会继续模拟上升到下一深度。
- 如果需要停留，Core 将该停站时间输出为整数秒。例如 12 s 会输出为 12 s，而不会在核心算法层抬到 60 s。
- Core 会用该秒级停留时间继续推进组织舱和氧暴露，再计算后续更浅停站。
- 因此 `tts_seconds`、后续停站时间、气体使用估算和跨语言移植结果都必须基于“求解 -> 秒级 strict-ceil -> 重算”的闭环，不能在 core 层额外套用分钟级取整。

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

### `arex_deco_calculate_tissue_margin`

```c
ArexDecoStatus arex_deco_calculate_tissue_margin(
    const ArexDecoDiveState* state,
    float reference_depth_m,
    float reference_limit_gf,
    ArexDecoTissueMarginMetrics* metrics);
```

计算 16 个组织舱在水面参考面和指定参考深度下，相对 GF limit 的占用比例。该接口用于 UI 或外部工具绘制组织舱 margin / limit 图，不推进组织舱，也不修改 `state`。

输入：

- `state`：当前完整潜水状态，包含配置和组织舱惰性气体压强。
- `reference_depth_m`：参考深度，单位 m，必须为有限值且 `>= 0`。例如 planner 首停深度、当前 ceiling 取整深度或用户指定的参考停站深度。
- `reference_limit_gf`：参考深度使用的 limit GF，必须为有限值且在 `(0, 1]` 范围内。调用方应按自己的参考面语义显式传入，例如 `config.gf_low`、某个停站 target GF 或 UI 想比较的自定义 GF。

输出：

- `metrics->surface_limit_ratio[16]`：每个组织舱在水面压强下的 `GF / config.gf_high`。
- `metrics->reference_limit_ratio[16]`：每个组织舱在 `reference_depth_m` 对应环境压强下的 `GF / reference_limit_gf`。

Ratio 语义：

- `1.0` 表示刚好达到该参考面的 limit。
- `< 1.0` 表示仍有余量。
- `> 1.0` 表示超过该参考面的 limit。
- `surface_limit_ratio` 固定使用 `gf_high` 作为水面 limit，避免调用方把水面可浮出判断错误绑定到任意参考 GF。
- `reference_limit_ratio` 的 limit GF 完全由 `reference_limit_gf` 参数决定，避免把任意参考深度错误绑定到 `gf_low`。

约束：

- `state` 和 `metrics` 不能为空。
- `state->config` 必须通过 `arex_deco_validate_config()`。
- `reference_depth_m` 必须为有限非负深度。
- `reference_limit_gf` 必须为有限值，且 `0 < reference_limit_gf <= 1`。

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
| `ArexDecoTissueMarginMetrics` | 128 |
| `ArexDecoGasRecommendation` | 44 |
| `ArexDecoStop` | 36 |
| `ArexDecoSchedule` | 1500 |

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
ArexDecoGasRecommendation gas_rec;
status = arex_deco_plan(&state, &schedule, &gas_rec);
if (status == AREX_DECO_STATUS_OK) {
    for (uint8_t i = 0; i < schedule.stop_count; ++i) {
        const ArexDecoStop* stop = &schedule.stops[i];
        // stop->depth_m, stop->duration_seconds, stop->gas_index
    }
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
| `_arex_deco_wasm_sizeof_schedule()` | `ArexDecoSchedule` 字节大小 |
| `_arex_deco_calculate_tissue_margin()` | 计算组织舱 surface/reference limit ratio；reference limit GF 由调用方传入 |

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
| `ascent_rate_m_per_min` | 28 |
| `ascent_rate_shallow_m_per_min` | 32 |
| `ascent_rate_shallow_start_m` | 36 |
| `deco_step_m` | 40 |
| `last_stop_m` | 44 |
| `reserved_config` | 48 |
| `safety_stop_seconds` | 52 |
| `gas_switch_penalty_seconds` | 56 |
| `water_type` | 60 |
| `safety_stop_enabled` | 64 |

当前 `ArexDecoSchedule` 关键字段偏移：

| 字段 | offset |
|---|---:|
| `stop_count` | 6 |
| `truncated` | 7 |
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

WASM adapter 必须以 sizeof helper 为准。任何 core ABI 变更都必须同步更新 JS offset 和 API 版本。
