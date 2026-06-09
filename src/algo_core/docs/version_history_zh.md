# AREX Deco Core 版本变更历史

本文档记录 core API、ABI、算法行为和 WASM 绑定的版本变化。

## 版本规则

版本号来自：

- `AREX_DECO_API_VERSION_MAJOR`
- `AREX_DECO_API_VERSION_MINOR`
- `AREX_DECO_API_VERSION_PATCH`

版本嵌入在所有带 `api_version` 的 POD 结构体中，并由 WASM helper 暴露：

- `_arex_deco_wasm_version_major()`
- `_arex_deco_wasm_version_minor()`
- `_arex_deco_wasm_version_patch()`

任何结构体字段、字段含义、ABI offset、算法输出语义发生变化时，必须更新版本号，并同步：

- core 头文件和 ABI static assert
- tests
- WASM adapter offset
- `simulator_web/public/wasm/arex_deco_core.js`
- `simulator_web/public/wasm/arex_deco_core.wasm`
- smoke test
- 文档

## 0.0.15

### 摘要

本次版本重构组织舱百分比查询接口，移除调用方需要自行传入参考深度 / GF 的旧 margin API，改为由 core 在当前环境压力下直接输出 16 仓 absolute / relative GF 百分比。

### 行为与 ABI 变更

- 删除 `ArexDecoTissueMarginMetrics` 和 `arex_deco_calculate_tissue_margin()`。
- 新增 `ArexDecoTissueGradientMetrics` 和 `arex_deco_calculate_tissue_gradients()`。
- `absolute_gf_percent[16]` 使用当前环境压力下的绝对 M-value 口径；`GF99` 现在与该数组共用同一套逐仓计算逻辑，并取其最大值。
- `relative_gf_percent[16] = absolute_gf_percent[16] / current_target_gf`，返回值已经是百分比数值，例如 `100.0f` 表示达到当前 target GF limit。
- `current_target_gf` 先按 `gf_high` ceiling 判定是否存在强制减压义务；NDL 状态直接使用 `gf_high`，只有进入强制减压后才基于首停和当前深度使用 `gf_low` 或线性插值。
- WASM 导出移除 `_arex_deco_calculate_tissue_margin`，新增 `_arex_deco_calculate_tissue_gradients` 和 `_arex_deco_wasm_sizeof_tissue_gradient_metrics()`。
- `ArexDecoTissueGradientMetrics` 大小为 132 字节；API patch 版本升至 `0.0.15`。

## 0.0.14

### 摘要

本次版本修复浅水减压预测中的 GF anchor 过早绑定问题。旧 planner 会在找到最深候选停站时立即把该深度作为 GF 插值锚点，即使该候选站实际停留时间为 0 s；当潜水员继续上升并在真正需要停留的浅一档重新调用 `arex_deco_plan()` 时，GF anchor 会从较深候选站切到实质停留站，导致同一首停深度的显示停留时间突增。

### 行为变更

- GF anchor 不再由初始数学首停或 0 s 候选站确立。
- Planner 会先模拟经过候选网格站；只有当某站实际产生 `>= AREX_DECO_STOP_TIME_GRANULARITY_SECONDS` 的停留时，才把该站锁定为本次计划的 first stop GF anchor。
- 锚点一旦锁定，后续主计划和 staged alt-plan 都继承同一个锚点，不再在到站重算时漂移。
- 新增 `scripts/reproduce_deco_time_jump.sh`，可用 `--verify-anchor` 复现并验证 12 m 候选站 / 9 m 实质停留站场景。
- ABI 字节布局无变化；由于 planner 输出语义变化，API patch 版本升至 `0.0.14`。

## 0.0.13

### 摘要

本次版本修复实时上升过程中重复调用 `arex_deco_plan()` 时，首个减压停站可能被当前深度夹逼成非配置网格深度的问题。Planner 现在会在当前深度允许的情况下，选择“不深于当前深度”的最深合法网格停站，保持停站深度继续落在 `last_stop_m + k * deco_step_m` 网格上。

### 行为变更

- 当理论首停深于当前深度时，planner 不再直接把首停钳制为当前深度。
- 若当前深度所在的最深网格站仍满足 `gf_high` ceiling 约束，则首停使用该网格站。
- 只有当当前网格站无法满足硬 ceiling 时，才退回当前深度作为首停，以保持安全约束。
- 新增回归测试覆盖从 9 m 停站向 6 m 停站过渡时的实时 replanning，确保输出停站仍在配置网格上。
- ABI 字节布局无变化；由于 planner 输出语义变化，API patch 版本升至 `0.0.13`。

## 0.0.12

### 摘要

本次版本把非强制减压安全停留深度从 `ArexDecoConfig` 中收回为 core 固定策略常量。安全停留开关和时长仍由 config 控制；停留深度固定为 `AREX_DECO_SAFETY_STOP_DEPTH_M = 5.0f`。core 保持米制物理输入输出，英制展示由产品层自行换算。

### 行为与 ABI 变更

- `ArexDecoConfig` 移除 `float safety_stop_depth_m`；原 offset `48` 的 4 字节改为 `reserved_config`，后续字段 offset 保持不变，`ArexDecoConfig` 总大小仍为 68 字节。
- Planner 生成安全停留时使用 `AREX_DECO_SAFETY_STOP_DEPTH_M`，不再读取调用方配置的停留深度。
- `arex_deco_validate_config()` 不再校验安全停留深度字段。
- core 不提供英制安全停留深度常量；UI / App / 设备端需要英制展示时，应从 `AREX_DECO_SAFETY_STOP_DEPTH_M` 自行换算。
- API patch 版本升至 `0.0.12`。

## 0.0.11

### 摘要

本次版本把非强制减压安全停留从“用 `safety_stop_seconds > 0` 隐式开启”改为显式 ABI 开关。`ArexDecoConfig` 新增 `safety_stop_enabled` 字段，占用原 reserved byte，因此 `ArexDecoConfig` 总大小仍为 68 字节，但字段语义和 offset 表发生变化，API patch 版本升至 `0.0.11`。

### 行为与 ABI 变更

- `ArexDecoConfig` 新增 `uint8_t safety_stop_enabled`，offset 为 `64`；`reserved` 从 4 字节缩减为 3 字节。
- 默认配置中 `safety_stop_enabled = 1`，默认安全停留仍为 5 m / 180 s。
- Planner 判定安全停留时使用显式开关：没有强制减压、`safety_stop_enabled == 1`、最大深度超过 10 m、当前深度深于当时 config 中的 `safety_stop_depth_m` 时，生成安全停留。
- `safety_stop_seconds` 回归为纯粹的时长参数，不再承担开启/关闭语义；调用方可配置 180 s、300 s 等不同安全停留时长。
- `arex_deco_validate_config()` 要求 `safety_stop_enabled` 只能为 `0` 或 `1`；当开关启用时，`safety_stop_seconds` 必须大于 0。
- 关闭安全停留应写 `safety_stop_enabled = 0`，不要再依赖把 `safety_stop_seconds` 写成 0 的隐式行为。
- 公共常量头文件按职责拆分为 `abi_constants.h`、`defaults.h` 和 `model_constants.h`；`constants.h` 保留为兼容聚合入口。

## 0.0.10

### 摘要

本次版本把 `arex_deco_plan()` 输出的减压停留恢复到秒级：核心算法层不再把正时长停站向上取整到整分钟。分钟级展示或执行策略应由 UI / 产品层在 core 输出之外处理。

### 行为变更

- Planner 移除主路径和 staged alt-plan 中的整分钟停站取整。
- 单站求解仍使用 6 h 上限、24 次二分迭代，并在输出前按严格上界取到整数秒。
- Planner 用该秒级停留时间继续推进组织舱、氧暴露和后续停站求解；因此 `duration_seconds`、`tts_seconds`、CNS / OTU 预测和静态 profile 快照均随之变化。
- `AREX_DECO_STOP_TIME_GRANULARITY_SECONDS` 从 `60` 改为 `1`，表达当前 core 输出粒度。
- ABI 字节布局无变化；由于算法输出语义变化，API patch 版本升至 `0.0.10`。

## 0.0.9

### 摘要

本次版本为计划输出增加出水时氧暴露预测，并新增组织舱基于水面/参考深度的物理余量查询接口。

### 行为与 ABI 变更

- `ArexDecoSchedule` 新增 `end_of_dive_exposure`，表示严格按 schedule 升水后的预测 CNS / OTU。
- `ArexDecoSchedule` 大小从 1468 增加到 1500；`stops` offset 从 28 后移到 60。
- 新增 `ArexDecoTissueMarginMetrics` 和 `arex_deco_calculate_tissue_margin()`，surface 参考面使用 `gf_high` limit，reference 参考面的 limit GF 由调用方显式传入，避免把任意参考深度错误绑定到 `gf_low`。
- WASM 导出新增 `_arex_deco_calculate_tissue_margin`，JS adapter 已同步 schedule offset。

## 0.0.8

### 摘要

本次版本修复 `arex_deco_step()` / `arex_deco_step_pressure()` 在
`duration_seconds == 0` 时返回空 runtime metrics 的问题。0 秒 step 现在可作为
当前状态快照查询：不推进时间、不演化组织，但会基于当前 tissue、深度和气体输出合法
NDL、GF99、SurfGF、ceiling 和 leading compartment。

### 行为变更

- `duration_seconds == 0` 不再把 `ArexDecoRuntimeMetrics` 清零。
- 0 秒 step 的 `next_state.elapsed_seconds` 保持不变。
- 0 秒 step 仍会根据输入的 end pressure / end depth 更新输出 state 的当前深度和
  active gas，用于表达“当前采样点”的 snapshot。
- ABI 布局无变化；由于输出语义变化，API patch 版本升至 `0.0.8`。

## 0.0.7

### 摘要

本次版本主要更新 `arex_deco_plan()` 的计划生成语义：planner 现在会把 `last_stop_m` 视为网格锚点而不是简单的截止深度，并在对齐网格时使用 staged alt-plan 约束最后一站下界，避免较深停站被低估。同步补充了停站离散化、切气惩罚、6 h 单站上限相关说明，以便 API 使用者和 WASM 适配层按同一口径实现。

### 行为变更

- 首停深度不再只由 ceiling 和 `deco_step_m` 决定，而是锚定到 `last_stop_m + k * deco_step_m` 网格。
- 当 `last_stop_m` 不是 `deco_step_m` 的整数倍时，planner 仍保持完整 `deco_step_m` 间距，避免最后几站“挤压”。
- 当 `last_stop_m` 与标准 step 网格对齐时，planner 会用 staged alt-plan 估算最后一站的最小保守停留时间。
- 若 staged alt-plan 在 6 h 单站上限内仍饱和，`arex_deco_plan()` 直接返回 `AREX_DECO_STATUS_INVALID_STATE`。
- 切气惩罚继续计入停站时长，并按新气体推进组织舱。
- 文档补充了二分迭代次数、离散化闭环和网格锚定规则，便于跨语言移植。

## 0.0.6

### 摘要

本次版本把 planner 从“单一匀速上升 + 纯强制减压”扩展为“深浅两段上升 + 安全停留 + 气体切换惩罚”的组合模型，并同步更新了 core ABI 文档与 Web WASM 适配层。`ArexDecoConfig` 新增的字段仍保持 68 字节总大小，但字段语义和 offset 已变化，因此版本必须上调。

### 行为变更

- Planner 现在使用两段上升速率：深水段和浅水段，默认在 10 m 处切换到更慢的浅水速率。
- 当本次潜水没有强制减压义务，但最大深度超过 10 m 时，planner 会默认插入 5 m / 180 s 的安全停留。
- 气体切换会附加默认 60 s 的同深度惩罚，并计入对应停站时长和 TTS。
- `ArexDecoConfig` 新增字段：浅水上升速率、浅水切换深度、安全停留深度、安全停留时长、气体切换惩罚。
- WASM adapter 的 `ArexDecoConfig` 偏移表同步更新，JS 侧需要重新打包。

## 0.0.5

### 摘要

本次版本补强两处安全相关行为：`arex_deco_nofly()` 统一按地表空气计算禁飞时间，不再受最后活跃气体影响；`gas_best_index()` 在氧气比例相同的情况下优先选择氦气更高的气体，并在没有任何可安全使用气体时向 `ArexDecoGasRecommendation` 暴露紧急标志。

### 行为变更

#### `arex_deco_nofly()` 强制使用地表空气

禁飞时间计算不再复用 `state->gas_plan.active_gas_index` 对应的气体，而是内部构造默认空气作为 surface gas。

原因：水面上默认呼吸介质应视为标准空气，禁飞时间不能被最后一次减压气体“污染”。

#### `gas_best_index()` 增加氦气二级排序

当多个气体的 `oxygen_fraction` 相同且都可用时，优先返回 `helium_fraction` 更高者，以降低深水氮醉风险。

#### `ArexDecoGasRecommendation.is_emergency_no_safe_gas`

当当前深度没有任何安全气体可用时，推荐结构体显式返回：

- `available = 0`
- `recommended_gas_index = -1`
- `is_emergency_no_safe_gas = 1`

上层 UI 必须把这视为强制警报，而不是“无需切气”。

## 0.0.4

### 摘要

围绕 DDD 单一真相源原则与 "机制 vs 策略" 分层，对气体默认值、校验、状态字段名做了五项破坏性变更。同时修复一处 `duration_seconds == 0` 路径下 `next_state` 未初始化的回归（自 `aafb1da` commit 引入），并在内部把 tissue model 切换为单精度 + `expm1f` 形式，为 MCU 移植做准备。

### API 签名变更（破坏性）

#### `make_default_air_gas` / `make_default_gas_plan` 签名加 `config`

```c
// 0.0.3
ArexDecoStatus arex_deco_make_default_air_gas(ArexDecoGas* gas);
ArexDecoStatus arex_deco_make_default_gas_plan(ArexDecoGasPlan* gas_plan);

// 0.0.4
ArexDecoStatus arex_deco_make_default_air_gas(const ArexDecoConfig* config, ArexDecoGas* gas);
ArexDecoStatus arex_deco_make_default_gas_plan(const ArexDecoConfig* config, ArexDecoGasPlan* gas_plan);
```

动机：`max_depth_m` 之前被硬编码为 60 m，但海水 + 1.4 bar ppO2 下空气真实 MOD 为 ~56.7 m。强制传入 `config` 让 MOD 由 `gas_pressure_to_depth_m(config, max_ppo2_bar / oxygen_fraction)` 自动推导，从源头消除"两个本应耦合的物理量被独立硬编码"的潜在错误。

迁移：调用方先 `arex_deco_make_default_config(&config)`，再把 `&config` 作为第一个参数传入。`arex_deco_make_initial_dive_state` 内部已自动适配，不需要外部调整。

#### `validate_gas` 签名加 `config` + 两条物理硬约束

```c
// 0.0.3
ArexDecoStatus arex_deco_validate_gas(const ArexDecoGas* gas);

// 0.0.4
ArexDecoStatus arex_deco_validate_gas(const ArexDecoConfig* config, const ArexDecoGas* gas);
```

新增校验：

- `max_ppo2_bar <= AREX_DECO_MAX_ALLOWABLE_PPO2_BAR (2.0)` — 绝对生理硬顶。1.4 / 1.6 等行业策略限值不在 core 拦截，由 UI 层提示。
- `max_depth_m <= MOD(config, gas)` — 防止用户手工把 air 的 `max_depth_m` 改成 100 m 等致死配置后通过校验。允许更保守（更浅）。

设计哲学：core 提供机制（拦截绝对致命），策略警告由 UI 层提供。

#### `ArexDecoDiveState.in_deco` → `was_deco_dive`

字段名变更，**内存布局和 latched 语义不变**（仍是 uint8_t @ offset 568）。0.0.3 起该字段就是 latched bit，但 `in_deco` 这个名字读起来像现在时，UI 开发者会误把 "DECO NOW" 红灯接到这个字段上，导致即便潜水员还清减压义务后红灯仍亮。

新名 `was_deco_dive` 显式表达"过去式"语义，与已有的内部参数 `was_deco_dive`（在 `nofly_seconds()` 中）对齐。**实时义务**请读 `metrics->ceiling_depth_m`。

### 默认值常量提取（无破坏性）

新增命名常量替代散落的 magic number：

```c
// 物理常数
#define AREX_DECO_AIR_OXYGEN_FRACTION 0.21f
#define AREX_DECO_AIR_NITROGEN_FRACTION 0.79f

// 用气策略默认 ppO2 红线
#define AREX_DECO_DEFAULT_BOTTOM_PPO2_BAR 1.4f
#define AREX_DECO_DEFAULT_DECO_PPO2_BAR 1.6f

// 绝对生理硬顶
#define AREX_DECO_MAX_ALLOWABLE_PPO2_BAR 2.0f
```

`make_default_air_gas` 现在使用这些常量；UI 设置界面可直接引用同一份定义作为输入边界。

### 算法内部重构（无 API 变化）

#### Tissue model 单精度化 + 数值稳定形

`core/src/internal/tissue_model.cpp` 完整重写：

- Haldane 等压方程：`1 - exp(-kt)` 改为 `-expm1f(-kt)`，避免慢舱（半衰期 635 min）+ 短步长下尾数精度损失。
- Schreiner 变压方程：从教科书形 `P_alv0 + R(t-1/k) - (P_alv0 - P0 - R/k)·e^(-kt)` 等价变形为 `P0 + R·t + (P0 - P_alv0 + R/k)·expm1(-kt)`。新形式在 `t → 0`、`R/k → 大` 的高频采样场景下消除灾难性消去。
- 全程 `float`、`expm1f`、`std::exp` 走单精度路径——MCU 上单精度 FPU 直通，不再退化为软浮点 double `exp`。

`Zhl16cCoefficients` 新增 `n2_k_per_sec_f[]` / `he_k_per_sec_f[]` 字段，由初始化阶段一次下转得到，热路径循环零 cast。

数值等价性：`tests/data/profiles/l2_static_profiles.json` 的所有快照在 `1e-5 bar` 容差内通过。

#### 指针别名守卫

`tissue_apply_constant` / `tissue_apply_schreiner` / `arex_deco_step_from_pressure` 在 `memcpy(out, in, ...)` 处加 `if (in != out)` 守卫。允许 `next_state == state` 原位更新而不触发 `-O3` 下 `memcpy(restrict)` 的 UB 假设。

### Bug 修复

#### `duration_seconds == 0` 早返回路径未初始化 `next_state`（回归）

0.0.3 期间 `aafb1da` commit 在 `arex_deco_step_from_pressure` 中加入 `duration_seconds == 0` 早返回优化（避免无意义地走 Schreiner），但**遗漏了把 `state` 复制到 `next_state`**。结果：调用方在 `duration=0` 路径下读到的 `next_state` 是栈上未初始化值（`tissue.nitrogen_bar[*]=0`、`elapsed_seconds=随机`）。

`StepTest.ZeroDurationAllowed` 实际上一直在这条路径下挂掉，但因为版本号未 bump、CMake 配置缓存导致 ctest 跑的是旧二进制，长期没有暴露。

修复：早返回路径加上 `if (next_state != state) memcpy(next_state, state, sizeof(*next_state))`。

### 测试覆盖

新增测试（共 2 个）：

- `ApiValidationTest.GasRejectsPpo2AboveAbsoluteCeiling`
- `ApiValidationTest.GasRejectsMaxDepthExceedingMOD`

重命名测试（与字段名同步）：

- `StepTest.InDecoLatchesOnceTriggered` → `WasDecoDiveLatchesOnceTriggered`
- `NoflyTest.InDecoLatchedAcrossAscent` → `WasDecoDiveLatchedAcrossAscent`

更新 fixture：`tests/data/profiles/l2_static_profiles.json` 中 4 处空气 gas 的 `max_depth_m: 60.0` → `56.0`。这是新校验暴露的真实物理违规（air @ 1.4 bar MOD ≈ 56.7 m），dive profile 实际深度从未触达该值，输出快照不变。

JSON 字段重命名：`expected.in_deco` → `expected.was_deco_dive`。

验证结果：93/93 通过。

### WASM / 跨端影响

无 ABI 字节大小变化。`simulator_web/src/lib/wasmCoreAdapter.ts` 中 `inDeco: 568` offset 键名同步重命名为 `wasDecoDive: 568`（值不变）；公共 `DiveState` TS 接口本来就没暴露这个字段。

WASM 二进制（`simulator_web/public/wasm/arex_deco_core.{js,wasm}`）需要在 Docker 中重新编译——签名变更会影响导出符号的参数计数。流程见 `bindings/wasm/README.md`。

### 升级清单（嵌入式 / 集成方）

1. 任何 `arex_deco_make_default_air_gas(&gas)` 改为 `arex_deco_make_default_air_gas(&config, &gas)`，先初始化 config。
2. 任何 `arex_deco_make_default_gas_plan(&plan)` 改为 `arex_deco_make_default_gas_plan(&config, &plan)`。
3. 任何 `arex_deco_validate_gas(&gas)` 改为 `arex_deco_validate_gas(&config, &gas)`。校验失败的 gas 配置可能曾经能通过——重新审阅 UI 表单：用户输入的 `max_depth_m` 是否超过对应气体的 MOD？
4. 任何 `state.in_deco` 改为 `state.was_deco_dive`。**重点**：不要把 UI 的 "DECO NOW" 闪烁灯接到这个字段，应改为 `metrics.ceiling_depth_m > 0`。
5. WASM 调用方期望 API 版本更新到 `0.0.4`，重新生成 WASM 产物。



### 摘要

修复三个影响安全语义的算法问题，清理死代码，对齐文档契约。

### 行为变化

#### CNS 半衰期衰减（A1）

0.0.2 前：CNS 只增不减，水面间隔后 CNS 不下降。

0.0.3 后：当段平均 PPO2 < 0.5 bar 时，CNS 按 90 min 半衰期指数衰减（对齐 Shearwater / Garmin 生产实现）。

新增常量：

```c
#define AREX_DECO_CNS_HALFLIFE_SECONDS 5400.0f
#define AREX_DECO_CNS_DECAY_PPO2_THRESHOLD_BAR 0.5f
```

新增内部函数：

```c
float ox_cns_apply(float current_cns_percent,
                   float ppo2_start_bar, float ppo2_end_bar,
                   float duration_seconds);
```

`arex_deco_step` / `arex_deco_step_pressure` 内部改用 `ox_cns_apply` 替代原来的 `cns_percent += ox_cns_increment_percent(...)`。

#### in_deco latched bit（A2）

0.0.2 前：`in_deco` 在每步 step 后按当前 ceiling 实时清零。

0.0.3 后：`in_deco` 是 latched bit——本次潜水一旦触发减压义务（ceiling > 0），保持 1 直到 `arex_deco_make_initial_dive_state` 重置。瞬时 ceiling 仍通过 `metrics->ceiling_depth_m` 暴露。

影响：`arex_deco_nofly` 使用 `state->in_deco` 判断 12h vs 24h 最低禁飞时间，latching 确保 deco dive 不会因升出 deco 区而误判为 no-deco dive。

#### Planner 单站时长上限提升（A3）

0.0.2 前：二分搜索上限 3600 s（60 min），20 次迭代。

0.0.3 后：上限 21600 s（6 h），24 次迭代。若 6h 后仍无法满足 ceiling 约束，`deco_plan` 返回 `AREX_DECO_STATUS_INVALID_STATE`。

#### Planner 停站时间离散化（A4）

0.0.3 后：`arex_deco_plan` 不再把连续数学求解得到的极短停站直接输出为秒级停站。若某站无需等待即可离开，则该站不写入 `ArexDecoSchedule`；若需要停留，则停站时间向上取整到最近的整数分钟。

该取整属于 core 算法策略，不是 UI 格式化。Planner 会用取整后的停留时间重新推进组织舱和氧暴露，再继续求解下一站。因此 TTS、后续停站和跨语言移植必须复现“求解 -> 取整 -> 重算”的闭环。

#### duration_seconds=0 允许通过（B8）

0.0.2 前：`arex_deco_step` / `arex_deco_step_pressure` 在 `duration_seconds == 0` 时返回 `INVALID_ARGUMENT`。

0.0.3 后：允许 0 时长，组织舱不变，仅输出当前 metrics。与 API 文档对齐。

### 代码清理

- GF 插值：删除死赋值，简化为单一 leaving-GF 分支（B5）。
- `find_first_stop_depth`：ceiling 比较加 epsilon 容差（B6）。
- 删除 `if (stop_time_sec == 0)` 死分支（B10）。

### API / ABI 变化

无结构体字段变化。`ArexDecoConfig` 大小保持 68 字节。`ArexDecoDiveState` 大小保持 600 字节。

`in_deco` 字段语义变更：从"瞬时是否欠债"变为"本次潜水是否曾欠债"。

### 测试覆盖

新增 7 个测试：

- `OxygenExposureTest.CNSDecaysAtSurface`
- `OxygenExposureTest.CNSDoesNotDecayAtElevatedPPO2`
- `OxygenExposureTest.CNSDecayAcrossDiveSession`
- `StepTest.InDecoLatchesOnceTriggered`
- `StepTest.ZeroDurationAllowed`
- `NoflyTest.InDecoLatchedAcrossAscent`
- `DecoPlannerTest.LongDecoObligationDoesNotSaturate`

验证结果：77/77 通过。

### WASM 影响

无 ABI 变化。WASM adapter 需更新期望版本号到 `0.0.3`。行为变化（CNS 衰减、in_deco latching）自动生效。

## 0.0.2

### 摘要

新增 `last_stop_m` 作为 core 算法入参，用于控制最后减压站深度。

### API / ABI 变化

`ArexDecoConfig` 新增字段：

```c
float last_stop_m;
```

字段位置：

- 位于 `deco_step_m` 之后
- 位于 `water_type` 之前

WASM `ArexDecoConfig` 关键 offset：

| 字段 | offset |
|---|---:|
| `deco_step_m` | 32 |
| `last_stop_m` | 36 |
| `water_type` | 40 |

`ArexDecoConfig` 总大小保持 `68` 字节，通过减少 reserved 字段保持外层结构体大小稳定。

### 默认值

新增默认常量：

```c
#define AREX_DECO_DEFAULT_LAST_STOP_M 3.0f
```

`arex_deco_make_default_config()` 会设置：

```c
config->last_stop_m = AREX_DECO_DEFAULT_LAST_STOP_M;
```

### 校验变化

`arex_deco_validate_config()` 新增：

- `last_stop_m > 0`

### Planner 行为变化

0.0.2 前：

- Planner 只使用 `deco_step_m`
- 最浅停站隐含等于 `deco_step_m`
- 例如 `deco_step_m = 3m` 时，最后停站必然是 3m

0.0.2 后：

- `deco_step_m`：中间停站跨度
- `last_stop_m`：最后减压站深度
- Planner 到达 `last_stop_m` 后不再生成更浅停站
- 最后一站会延长到满足直接升水条件
- `last_stop_m` 支持非 `deco_step_m` 整数倍，例如 `4.5m`

示例：

| `deco_step_m` | `last_stop_m` | 行为 |
|---:|---:|---|
| 3m | 3m | 常规 3m 最后停站 |
| 3m | 4.5m | 最后停站为 4.5m |
| 3m | 6m | 跳过 3m，最后停站为 6m |

### 测试覆盖

新增/更新测试：

- 默认初始状态包含 `state.config.last_stop_m`
- `last_stop_m = 6m` 时，schedule 不生成 3m 停站
- `last_stop_m = 4.5m` 时，最后停站为 4.5m，而不是被折算到 3m

验证结果：

- core tests：70/70 通过
- Docker WASM build + smoke：通过
- 前端 build：通过

### WASM 影响

WASM adapter 需要：

- 期望 API 版本更新到 `0.0.2`
- 在 `ArexDecoConfig` offset `36` 写入 `last_stop_m`
- 将 `water_type` offset 从 `36` 更新到 `40`
- 重新构建并复制 WASM 产物

构建和同步流程见：

- `bindings/wasm/README.md`

### 前端影响

前端配置拆分为两个真实 core 参数：

- `停站步长` -> `decoStepM` -> core `deco_step_m`
- `最后减压站` -> `lastStopM` -> core `last_stop_m`

`最后减压站` 改为数值输入，支持 0.5m 步进，不再限制为 `3 / 4.5 / 6` 三个固定选项。

## 0.0.1

### 摘要

初始 core API 和 WASM 绑定版本。

### 主要能力

- 默认配置、默认空气气体、默认气体计划
- 初始化完整潜水状态
- 组织舱水面初始化
- 气体与配置校验
- 深度段 step
- 压强段 step
- runtime metrics 输出
- deco planner 输出停站和 TTS
- gas recommendation
- no-fly time
- WASM 导出 core C ABI 和 sizeof helper

### Planner 行为

- `deco_step_m` 同时控制中间停站跨度和最浅停站深度
- 例如 `deco_step_m = 3m` 时，最后停站固定为 3m

### 已知限制

- 没有独立 `last_stop_m`
- 前端产品层的 `lastStopM` 在 WASM core 路径中不生效
- 停站策略无法表达开放水域涌浪或 6m 纯氧最后停站策略
