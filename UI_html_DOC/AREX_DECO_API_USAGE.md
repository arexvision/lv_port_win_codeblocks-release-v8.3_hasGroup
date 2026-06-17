# Arex Deco API 与当前接入逻辑说明

本文说明当前项目如何使用 `src/algo_core` 中的 Arex 减压算法库。重点不是解释 Bühlmann 数学模型，而是解释 API 的输入输出、我们当前适配层怎么调用，以及为什么 UI 会看到非 3m 倍数的减压站。

相关代码：

- 算法公共头：`src/algo_core/include/arex_deco/api.h`
- 算法结构体：`src/algo_core/include/arex_deco/types.h`
- 算法常量：`src/algo_core/include/arex_deco/defaults.h`、`model_constants.h`
- 当前 UI 适配层：`src/algo_sim/deco_core.cpp`

## 一句话总览

Arex 算法库提供的是一个“状态推进 + 当前状态重新规划”的接口：

1. UI/模拟器把当前深度、经过时间、当前气体传给 `arex_deco_step()`。
2. 算法返回新的组织舱状态和实时指标，例如 NDL、GF99、ceiling。
3. 适配层再用新的状态调用 `arex_deco_plan()`。
4. `arex_deco_plan()` 返回从当前状态升水所需的计划，包括 TTS 和停站列表。
5. 适配层把这些结果写入 UI bus，UI 只显示 bus 里的数据。

所以 UI 右上角的 `NDL / SAFE / DECO` 并不是 UI 自己算的，而是 `deco_core.cpp` 把算法输出转成 `bus_update_deco()` 后显示出来的。

## API 分层

### 1. 构造与校验

常用构造函数：

```c
arex_deco_make_default_config(ArexDecoConfig *config);
arex_deco_make_default_gas_plan(const ArexDecoConfig *config, ArexDecoGasPlan *gas_plan);
arex_deco_make_initial_dive_state(ArexDecoDiveState *state);
arex_deco_reset_tissue_to_surface(const ArexDecoConfig *config, const ArexDecoGas *surface_gas, ArexDecoTissueState *tissue);
```

当前项目在 `ensure_initialized()` 中使用：

1. `arex_deco_make_initial_dive_state(&s_state)` 创建初始潜水状态。
2. `apply_current_ui_config()` 把 UI 设置转换成算法 config 和 gas plan。
3. `arex_deco_reset_tissue_to_surface()` 把组织舱初始化到水面空气/当前气体平衡状态。

初始化后的全局状态保存在 `deco_core.cpp` 的：

```c
static ArexDecoDiveState s_state;
static ArexDecoRuntimeMetrics s_metrics;
```

### 2. 配置 `ArexDecoConfig`

核心配置结构体是 `ArexDecoConfig`。当前项目主要写这些字段：

```c
config->gf_low
config->gf_high
config->last_stop_m
config->deco_step_m
config->water_type
config->water_meters_per_bar
config->safety_stop_seconds
config->safety_stop_enabled
```

当前映射在 `fill_config_from_ui()`：

| UI/项目设置 | 写入算法字段 | 当前逻辑 |
|---|---|---|
| GF low/high | `gf_low` / `gf_high` | UI 百分比转 0.0-1.0 |
| LAST DECO 3m/6m | `last_stop_m` | 只允许 3 或 6 |
| 停站间隔 | `deco_step_m` | 当前固定 `3.0f` |
| SALINITY | `water_type` / `water_meters_per_bar` | fresh 使用 10.3m/bar，其它按 salt 10.0m/bar |
| SAFETY STOP | `safety_stop_enabled` / `safety_stop_seconds` | OFF 关闭，否则 180/240/300s |

注意：`ArexDecoWaterType` 头文件里目前只有 `SALT` 和 `FRESH`，并且有 `TODO: EN13319`。所以 UI 里的 EN13319 如果要严格接入，还需要算法库或适配层补一套明确映射。

### 3. 气体计划 `ArexDecoGasPlan`

算法气体结构是：

```c
typedef struct ArexDecoGas {
    float oxygen_fraction;
    float helium_fraction;
    float nitrogen_fraction;
    float min_depth_m;
    float max_depth_m;
    float max_ppo2_bar;
    uint8_t enabled;
    ArexDecoGasRole role;
} ArexDecoGas;
```

当前项目在 `fill_gas_plan_from_ui()` 中从 bus 读取气体槽：

- `bus_get_gas_slot_count()`
- `bus_get_gas_slot_o2_pct(i)`
- `bus_get_gas_slot_he_pct(i)`
- `bus_get_gas_slot_max_ppo2(i)`
- `bus_get_gas_active_idx()`

然后转换成算法 gas：

- O2/He 百分比转 fraction。
- N2 = 1 - O2 - He。
- 第一个有效气体 role 设置为 `BOTTOM`，后续设置为 `DECO`。
- `max_ppo2_bar` 优先使用每个气体槽自己的最大 PO2；旧 TCP/旧数据没有该字段时才 fallback 到全局 `MOD PO2`。
- `max_depth_m` 使用 UI gas profile 已按算法接口计算出的 MOD；如果旧数据没有 MOD，则由算法接口按 `max_ppo2_bar` 和当前 config 计算。

如果 UI 没有有效气体，适配层会退回 `arex_deco_make_default_gas_plan()`，即默认空气。

### 4. 状态推进 `arex_deco_step()`

实时潜水中最重要的 API 是：

```c
ArexDecoStatus arex_deco_step(
    const ArexDecoDiveState *state,
    const ArexDecoStepInput *input,
    ArexDecoDiveState *next_state,
    ArexDecoRuntimeMetrics *metrics);
```

输入 `ArexDecoStepInput`：

```c
input.start_depth_m = s_state.current_depth_m;
input.end_depth_m = depth_m;
input.duration_seconds = delta_time_s;
input.gas_index = s_state.gas_plan.active_gas_index;
```

当前项目在 `deco_core_tick(depth_m, temperature_c, delta_time_s)` 里调用它。PC 模拟器 TCP 倍速时，本质是每个真实 tick 里循环多次 `deco_core_tick(..., 1U)`，所以算法时间、深度移动、组织舱推进都会同步快进。

`arex_deco_step()` 输出：

- `next_state`：新的潜水状态，包含组织舱、当前深度、elapsed time、氧暴露等。
- `metrics`：实时指标，包括：
  - `gf99_percent`
  - `surface_gf_percent`
  - `ceiling_depth_m`
  - `ndl_seconds`
  - `leading_compartment`

当前项目调用成功后会执行：

```c
s_state = next_state;
```

也就是说 `s_state` 始终是算法内部“当前潜水状态”的单一来源。

### 5. 当前状态规划 `arex_deco_plan()`

每次 step 后，当前项目马上调用：

```c
ArexDecoSchedule schedule;
ArexDecoStatus plan_status = arex_deco_plan(&s_state, &schedule, NULL);
```

`ArexDecoSchedule` 主要字段：

```c
uint8_t stop_count;
uint8_t ceiling_violated;
uint32_t tts_seconds;
ArexDecoOxygenExposure end_of_dive_exposure;
ArexDecoStop stops[AREX_DECO_MAX_DECO_STOP_COUNT];
```

每个 `ArexDecoStop` 包含：

```c
float depth_m;
uint32_t duration_seconds;
int8_t gas_index;
float target_gf;
uint32_t hold_seconds;
uint32_t switch_penalty_seconds;
```

注意几个点：

1. `duration_seconds` 是秒级输出，算法不会自动取整到整分钟。
2. `depth_m` 是算法返回的停站深度；当前 UI 适配层没有对它做 3m 网格吸附。
3. `duration_seconds` 是计划总时长，包含物理停留和同深度预测切气 penalty。
4. `hold_seconds` 是物理停留时间，不包含切气 penalty。
5. `switch_penalty_seconds` 是 planner 预测用的同深度切气延迟。
6. `ceiling_violated == 1` 表示 plan 时当前深度已经浅于 GF-high ceiling；即使 `stops/tts` 为 0，产品层也必须提示违规风险。

## 当前项目的实时输出链路

实时链路在 `sync_core_data()` 中汇总：

```c
sync_tissue_data();
bus_set_cns(...);
bus_set_otu(...);
bus_set_gf99(...);
bus_set_surf_gf(...);
bus_set_ceiling(s_metrics.ceiling_depth_m);
alarm_set_active(ALARM_ID_CRIT_CEIL_BROKEN, schedule != NULL && schedule->ceiling_violated != 0);
bus_set_tts(schedule != NULL ? round_up_minutes(schedule->tts_seconds) : 0U);
sync_gas_data();
sync_stop_data(schedule);
sync_deco_plan_data(schedule);
```

### 1. NDL / DECO / SAFE 右上角

右上角状态来自 `sync_stop_data()`：

1. 如果 `s_metrics.ndl_seconds > 0`，适配层把它向上取整成分钟，写成 NDL。
2. 如果 `schedule.stop_count > 0`，适配层从 `schedule.stops[]` 中找第一个 runtime stop。
3. 如果某站满足 `hold_seconds == 0 && switch_penalty_seconds > 0`，它是纯切气预测站，不作为右上角 `DECO xxm` 主倒计时显示。
4. 如果当前存在强制 ceiling，右上角显示 `DECO`，倒计时来自 planner 的第一个有效 runtime stop。
5. 如果没有强制 ceiling，右上角 `SAFE` 读取 `arex_deco_safety_stop()` 的 runtime 状态，使用 `target_depth_m/required_seconds/remaining_seconds/counting` 写入：

```c
bus_update_deco(ndl_min, stop_type, stop_depth_m, stop_total_s, stop_left_s, in_stop_zone);
```

UI 组件 `comp_refresh_ndl_stop_vm()` 再显示：

- `STOP_NONE` -> `NDL`
- `STOP_SAFETY` -> `SAFE Xm`
- `STOP_DECO` -> `DECO Xm`

当前 `STOP_DECO` 判定是：

```c
stop_type = (s_metrics.ceiling_depth_m > 0.01f) ? STOP_DECO : STOP_SAFETY;
```

因此只要实时 ceiling 大于 0，就认为是强制减压停留；否则不再把 schedule stop 自动当安全停留，而是单独读取 `arex_deco_safety_stop()`。

当前适配层不再自行补安全停留 fallback。右上角 `SAFE Xm` 必须来自算法 `arex_deco_safety_stop()` 返回的 runtime 状态；适配层不会再用 `max_depth/current_depth/ndl` 或本地安全区间自己判断“应该显示安全停留”。`arex_deco_plan()` 中的 safety stop 保留给 TTS、计划路径和气量估算。

如果 `arex_deco_step()` 成功但 `arex_deco_plan()` 返回非 OK，适配层不得把它当作 `stops=0` 同步到 UI。此时只同步组织仓、毒性、GF、ceiling、nofly、gas 等非 schedule 数据；TTS、右上角当前停站和实时轨迹停站列表保持上一帧有效 plan 输出。`plan` 失败表示本轮 schedule 不可用，不等价于“没有安全停留/没有减压站”。

0.0.19 后，`schedule.ceiling_violated` 也会直接驱动 `CEILING BROKEN` 告警。这个告警不能只依赖 `stop_count` 或 `tts_seconds`，因为算法明确说明存在“计划为空但当前深度已经浅于 ceiling”的风险场景。

### 2. 停留进度条

UI 的进度条使用：

```c
pct = stop_time_left_s / stop_time_total_s;
```

之前的问题是适配层每次把 total 和 left 都传成算法当前剩余时间，导致进度一直是满的。现在适配层通过 `sync_stop_progress_total()` 缓存当前站第一次进入时的 `total_s`，后续只让 `left_s` 跟随算法变化。

### 3. DIVE PLAN TRACK 图

路径跟踪图使用 `bus_set_deco_plan()` 写入的 `deco_stop_t` 数组。这个数组来自 `sync_deco_plan_data()`：

```c
stops[count].depth_m = schedule->stops[i].depth_m;
stops[count].stay_min = schedule->stops[i].duration_seconds / 60.0f;
```

所以图上显示的停站深度同样来自算法返回的 `schedule.stops[i].depth_m`。

当前实时轨迹图同样会过滤纯切气预测站，避免把 `hold_seconds == 0 && switch_penalty_seconds > 0` 的锚点画成 `0:00` 减压站。真正进入停站列表的时间使用 `duration_seconds`，与 TTS/planner 的总时长口径一致。

这层过滤由 `src/algo_sim/deco_core.cpp` 中的 `DECO_HIDE_SWITCH_ONLY_STOPS` 控制。默认开启，只影响 UI 是否展示纯切气预测站；不改写算法返回的 `schedule`，也不改变 `tts_seconds`、氧暴露预测、气量估算或推荐气体状态。

## 为什么会出现非 3m 倍数的站点

按当前适配层配置：

```c
config->last_stop_m = 3.0f 或 6.0f;
config->deco_step_m = 3.0f;
```

从业务预期看，强制减压站应该落在：

- last stop = 3m：3 / 6 / 9 / 12 / 15 ...
- last stop = 6m：6 / 9 / 12 / 15 ...

但是当前 UI 右上角和计划图没有做任何网格修正，而是直接使用：

```c
schedule.stops[0].depth_m
```

因此如果算法库返回 `11.8m`、`10.7m`、`9.9m`，UI 会直接显示成：

- 右上角：`DECO 11m`、`DECO 10m`、`DECO 9m`
- 计划图：对应位置也会画出这些深度

这说明当前看到的非 3m 站点不是 UI 自己生成的，而是算法 `arex_deco_plan()` 输出的 `ArexDecoStop.depth_m` 本身没有按 3m 网格离散。

当前需要明确一个接口口径：

| 概念 | 是否可以连续 | 当前来源 | UI 应该怎么用 |
|---|---:|---|---|
| ceiling | 可以 | `metrics.ceiling_depth_m` | 显示 CEIL / 判断是否强制减压 |
| deco stop depth | 理论上应离散 | `schedule.stops[i].depth_m` | 显示 DECO 站点、画计划图 |
| current depth | 可以 | 传感器/TCP 深度 | 显示当前深度、判断是否在停留区 |

如果算法设计是“schedule depth 已经是离散停站”，那现在的 `.a` 输出与文档/业务预期不一致，需要检查算法库。

如果算法设计是“schedule depth 可能是连续 ceiling 参考点”，那 UI 适配层就必须增加离散化，把连续深度转换到业务停站网格。

## 如果在适配层离散化，应该怎么做

建议把这个处理放在 `src/algo_sim/deco_core.cpp`，不要放到 LVGL 组件里。原因是 bus 层和所有 UI 都应该看到同一个“当前停站”。

推荐规则：

```c
display_stop = last_stop_m + ceil((raw_stop_m - last_stop_m) / deco_step_m) * deco_step_m;
```

含义：

- raw stop 10.7m，last 3m，step 3m -> 12m。
- raw stop 9.9m，last 3m，step 3m -> 12m 或 9m 取决于容差策略。
- raw stop 8.1m，last 3m，step 3m -> 9m。
- raw stop 小于 last stop 时 -> last stop。

需要特别注意：如果只改显示深度，不改算法内部 `duration_seconds`，就可能出现“显示 12m，但时间是算法按 10.7m 算出来的”。这是否可接受要由算法口径决定。

更稳的方案是让算法库直接输出离散站点，因为停站深度和停留时间是耦合的。UI 适配层只消费结果，不再二次猜测。

## 16 组织仓柱状图口径

当前 DECO 卡片主图使用归一化组织图载荷：

- `tissue_bar_permille[16]`：0~1000 的组织条长度，400 表示环境压力线，900 表示 M 值线。
- `tissue_pi_permille`：吸入总惰性气体分压虚线位置，同样使用 0~1000 坐标。
- `tissue_ambient_pressure_bar`：当前环境压力。
- `tissue_inspired_n2_bar`：当前吸入氮气分压。
- `tissue_inspired_he_bar`：当前吸入氦气分压。
- `tissue_n2_bar[16]`：16 仓氮气分压。
- `tissue_he_bar[16]`：16 仓氦气分压。
- `tissue_m_value_bar[16]`：16 仓 combined a/b M 值，来自 `arex_deco_calculate_tissue_pressures()`。
- `tissue_m_gf_bar[16]`：16 仓当前 GF 红线压力，来自 `arex_deco_calculate_tissue_pressures()`。

注意：core 支持 Trimix，组织条长度和 GF 映射都按总惰性气体压力 `PN2 + PHe`；不得只用 `tissue_n2_bar[16]` 重算风险。

前端显示语义：

- 主图是纯黑背景上的横向 16 行组织条，当前完全剥离色相差异，统一使用纯绿 `GREEN`，只通过 opacity 表达状态。
- 400 竖线是环境压力线，使用 `LV_OPA_40`；900 竖线是 M 值线，使用 `LV_OPA_100`；PI 虚线来自 `tissue_pi_permille`，同样使用纯绿低透明度。
- `PI`、`PAMB`、`M` 文字标签由 `card_deco.c` 绘制；文字只用于标注，不影响归一化计算和参考线。
- 每根组织条直接使用 `tissue_bar_permille[i]` 作为长度；400 以下安全段使用 `LV_OPA_40`；400~900 排氮段按接近 M 值的比例在 `LV_OPA_50..LV_OPA_100` 之间线性映射；超过 900 的部分使用纯绿满亮闪烁。

自定义 Tissue 小组件：

- `COMP_TISSUE_RAW_4012` 显示同一套 16 行横向组织仓，条长直接读取 `tissue_bar_permille[16]`。小组件画 400、900 和 PI 参考线，但不画底部 `PI/PAMB/M` 字母。
- `COMP_TISSUE_GF_4012` 也显示 16 行横向组织仓，但 900 线语义切换为当前 target GF 红线。条长在超过 400 后使用 `tissue_gf_pct[i]` 映射：`400 + tissue_gf_pct[i] / 100 * 500`。
- `tissue_raw_pct[16]` / `tissue_gf_pct[16]` 仍保留给自定义组件和信息概览兼容，不驱动 DECO 主图。0.0.23 起，适配层不再反推 M 值，而是直接消费 `arex_deco_calculate_tissue_pressures()`。

## 当前潜水计划页的用法

`deco_core_plan_calculate()` 是给 PLAN 页面使用的离线计划计算，不是实时潜水状态。

流程：

1. 创建新的 `plan_state`。
2. 复制当前 UI config/gas plan。
3. 把组织舱重置到水面。
4. 先模拟下潜到目标深度。
5. 再模拟底部停留 `bottom_time_min`。
6. 调用 `arex_deco_plan()` 得到计划。
7. 把 schedule 转成 `dive_plan_result_snapshot_t`。

这里和实时潜水不同：它不使用当前 `s_state` 的组织舱负荷作为起点，而是用水面初始状态起算。

## 当前接入层的几个注意点

1. `temperature_c` 已用于 `arex_deco_calculate_gas_density()`，适配层把摄氏度转为 Kelvin，并按 `compressibility_z = 1.0f` 调用算法气体密度接口。
2. `s_state.was_deco_dive` 是“本次是否曾经进入过减压”的 latched bit，不能用来判断当前是否 DECO；当前是否有减压义务要看 `s_metrics.ceiling_depth_m`。
3. `arex_deco_plan()` 每次 tick 都重新从当前状态规划，所以 schedule 会随深度、组织舱和气体变化不断变化。
4. `schedule.tts_seconds` 是算法规划的总升水时间，UI 里通过 `round_up_minutes()` 显示成 TTS 分钟。
5. 当前 `sync_stop_data()` 只在强制减压场景取第一个非纯切气预测的 runtime stop 做右上角 `DECO`；免减压安全停留显示读取 `arex_deco_safety_stop()`。
6. 当前适配层没有检查 `schedule.truncated`，如果未来有超长/容量不足计划，需要补 UI 提示。
7. 当前适配层会消费 `schedule.ceiling_violated`，用于触发 `CEILING BROKEN`；真机侧也需要同步该语义。
8. `TTS @ +5min` / `TTS Δ +5min` 使用 `arex_deco_forecast_tts_hold()`，按 5 秒低频刷新；`NDL↑3` / `NDL↓3` 使用 `arex_deco_forecast_ndl_excursion()`。动态紧凑 `NDL3` 根据上升率选择 up/down 预测，预测失败或方向静止时保留上一帧，初始化默认 0。

## 建议后续确认项

1. 确认 Arex `.a` 的 `arex_deco_plan()` 是否承诺 `stops[i].depth_m` 按 `last_stop_m + k * deco_step_m` 输出。
2. 如果承诺离散输出，当前非 3m 站点应作为算法库问题追踪。
3. 如果不承诺离散输出，需要在适配层新增“业务停站网格化”函数，并明确时间是否也需要重新规划。
4. EN13319 水型目前头文件没有枚举值，需要确认算法层是否会增加，还是 UI 层映射到 salt/fresh 的某个 meters-per-bar。
