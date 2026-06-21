# AREX 多气体减压计划语义沟通记录

日期：2026-06-21  
目的：整理模拟器中多气体、推荐切气、忽略切气、减压站显示之间的语义问题，便于后续向算法工程师确认。

## 1. 当前接口理解

当前产品层使用 AREX 算法时，至少涉及两类调用：

- `arex_deco_step()`：实时推进组织状态。输入 `ArexDecoStepInput.gas_index`，表示当前这段时间潜水员实际呼吸的气体。
- `arex_deco_plan()`：基于当前 `ArexDecoDiveState` 生成未来减压计划。输入状态里包含 `ArexDecoGasPlan`，其中有 `active_gas_index` 和多个 enabled gas。
- `arex_deco_recommend_gas()`：基于当前状态推荐更优气体，返回 `recommended_gas_index`。

从现有日志看，`step_gas` / `plan_active` 和计划站点里的 `gas=` 不是同一层语义：

```text
[AREX_CALL] ... step_gas=0(slot0 AIR) ... plan_active=0(slot0 AIR)
[AREX_PLAN] ... #1 3.00m dur=195s hold=195s sw=0s gas=2(slot2 O2 100%)
```

这里表示：

- 实时组织推进仍按 AIR。
- plan 当前 active gas 也是 AIR。
- 但未来计划站点使用 O2。

因此，`AREX_PLAN` 中 stop 的 `gas=2` 表示算法规划“未来可使用的气体”，不表示当前已经切换到该气体。

## 2. 关键字段说明

### `DECO_GAS_MAP`

示例：

```text
[DECO_GAS_MAP] count=3 active=0 ignored=0x0 | idx0<-slot0 AIR 21/0 mod=58.2m maxpo2=1.40 en=1 | idx1<-slot1 EAN32 32/0 mod=34.6m maxpo2=1.40 en=1 | idx2<-slot2 O2 100% 100/0 mod=4.0m maxpo2=1.40 en=1
```

字段含义：

- `count=3`：传给算法的 gas 数量。
- `active=0`：算法 gas plan 中当前 active gas index。
- `ignored=0x0`：产品层运行时忽略气体 bitmask。
- `idx0<-slot0`：算法内部 gas index 0 对应 UI/产品层 slot0。
- `mod=4.0m maxpo2=1.40`：该气体按 max PPO2 1.40 计算 MOD。

注意：如果 O2 100% 使用 `maxpo2=1.40`，它的 MOD 是约 4.0m；如果产品预期 O2 在 6m 可用，则应传入 `maxpo2=1.60`。

### `AREX_CALL`

示例：

```text
[AREX_CALL] tick depth=3.0m step=OK(0) step_gas=0(slot0 AIR) plan=OK(0) plan_active=0(slot0 AIR) ndl=0s/0min ceiling=0.83m stops=1 tts=215s cv=0
```

字段含义：

- `tick`：周期推进路径。
- `depth=3.0m`：当前深度。
- `step=OK(0)`：`arex_deco_step()` 返回状态。
- `step_gas=0(slot0 AIR)`：本次实时组织推进使用的气体。
- `plan=OK(0)`：`arex_deco_plan()` 返回状态。
- `plan_active=0(slot0 AIR)`：调用 plan 时 gas plan 的 active gas。
- `ndl=0s/0min`：当前 NDL。
- `ceiling=0.83m`：当前 ceiling。
- `stops=1`：算法返回的计划站数量。
- `tts=215s`：算法返回的总上升/减压时间。
- `cv=0`：ceiling violation 标志。

### `AREX_PLAN`

示例：

```text
[AREX_PLAN] depth=3.0m ceiling=0.83m cv=0 tts=215s stops=1 | #1 3.00m dur=195s hold=195s sw=0s gas=2(slot2 O2 100%) gf=0.70
```

字段含义：

- `#1 3.00m`：第 1 个计划站，深度 3m。
- `dur=195s`：算法返回的站点总时长。
- `hold=195s`：物理停留减压时间。
- `sw=0s`：同深度切气惩罚时间。
- `gas=2(slot2 O2 100%)`：该计划站使用 O2。
- `gf=0.70`：该站目标 GF。

算法头文件中 `ArexDecoStop` 的注释语义为：

- `duration_seconds`：physical hold + gas switch penalty。
- `hold_seconds`：仅物理停留时间。
- `switch_penalty_seconds`：planner-only 的同深度切气延迟。

## 3. 已观察到的两个日志现象

### 3.1 日志 A：切气惩罚为 60s 时，出现纯切气站

较早日志中，O2 未切换，计划中出现：

```text
[AREX_CALL] tick depth=3.3m step=OK(0) step_gas=1(slot1 EAN32) plan=OK(0) plan_active=1(slot1 EAN32) ndl=0s/0min ceiling=0.38m stops=1 tts=82s cv=0
[AREX_PLAN] depth=3.3m ceiling=0.38m cv=0 tts=82s stops=1 | #1 3.00m dur=60s hold=0s sw=60s gas=2(slot2 O2 100%) gf=0.70
```

这说明：

- 当前实际呼吸气体是 EAN32。
- plan 仍然认为 O2 是未来可用气体。
- 3m 站没有实际 hold，只剩 `sw=60s` 切气惩罚。

产品层之前用 `DECO_HIDE_SWITCH_ONLY_STOPS` 隐藏纯切气预测站，避免 UI 显示一个没有真实 hold 的减压站。后来确认算法配置中已有 `gas_switch_penalty_seconds`，因此当前更合理的做法是直接传：

```c
config->gas_switch_penalty_seconds = 0U;
```

这样算法本身不再产生 `sw=60s`，UI 不需要额外把 `duration` 改成 `hold`。

当前分支已按这个方向处理：`DECO_GAS_SWITCH_PENALTY_SECONDS = 0U`，并写入 `ArexDecoConfig.gas_switch_penalty_seconds`。

### 3.2 日志 B：当前仍是 AIR，但 plan 站点已经使用 O2

新日志中，在 3m 位置，用户尚未切到 O2 前出现：

```text
[AREX_CALL] tick depth=3.0m step=OK(0) step_gas=0(slot0 AIR) plan=OK(0) plan_active=0(slot0 AIR) ndl=0s/0min ceiling=0.83m stops=1 tts=215s cv=0
[AREX_PLAN] depth=3.0m ceiling=0.83m cv=0 tts=215s stops=1 | #1 3.00m dur=195s hold=195s sw=0s gas=2(slot2 O2 100%) gf=0.70
```

随后用户切到 O2 后：

```text
[AREX_CALL] tick depth=3.0m step=OK(0) step_gas=2(slot2 O2 100%) plan=OK(0) plan_active=2(slot2 O2 100%) ndl=0s/0min ceiling=0.80m stops=1 tts=210s cv=0
[AREX_PLAN] depth=3.0m ceiling=0.80m cv=0 tts=210s stops=1 | #1 3.00m dur=190s hold=190s sw=0s gas=2(slot2 O2 100%) gf=0.70
```

这说明：

- 切气前，组织推进仍是 AIR，但 plan 已经按 O2 规划未来站点。
- 切气后，组织推进和计划站点都使用 O2。
- 切气前显示 `195s`，其语义不是“继续吸 AIR 还剩 195s”，而是“如果采用计划中的 O2，则该站 hold 约 195s”。

这个现象不一定是算法错误，但对 UI 用户语义有明显误导风险。

## 4. 当前产品层策略回顾

### 4.1 推荐气体

产品层希望：

- 当前处于 DECO 场景。
- 当进入某个更优气体的 MOD 范围时，显示 `BETTER GAS AVAILABLE`。
- 例如 EAN32 MOD 34.6m，则应在 34.6m 左右进入推荐条件，而不是超过 MOD 1m 后才推荐。

### 4.2 忽略气体

产品层希望：

- 如果用户没有确认切气，并继续上升到“推荐深度更浅 1m”之后，则认为用户忽略该气体。
- 例如 EAN32 MOD 34.6m，若上升到约 33.6m 仍未切，则可将 EAN32 标记 ignored。
- ignored 气体应从后续 plan 输入里禁用或移除。
- 如果用户之后手动切到该气体，则恢复该气体可用。
- 多气体场景下应支持同时 ignored 多个气体，例如 3 gas 中忽略 EAN32 和 O2。

当前运行时 ignored 使用 bitmask 表达，理论上支持多个 ignored gas。

### 4.3 O2 MOD 配置影响

日志中 O2 为：

```text
O2 100% ... mod=4.0m maxpo2=1.40
```

因此：

- O2 推荐范围从约 4.0m 开始，而不是 6.0m。
- 如果使用“超过推荐深度 1m 后忽略”，O2 需要到约 3.0m 才会被自动忽略。
- 在 3.3m 或 3.0m 附近，是否已经算“忽略 O2”，取决于产品边界规则和深度误差处理。

这点需要和算法工程师/产品规则一起确认：O2 的 max PPO2 是按 1.40 还是 1.60 进入算法。

## 5. 不同调用方式及结果差异

下面按可能的产品调用策略进行拆分。

### 方式 A：单次 plan，传入所有 enabled gas

做法：

- `arex_deco_step()` 使用当前 active gas 更新组织。
- `arex_deco_plan()` 的 `gas_plan` 包含所有配置且未 ignored 的气体。
- `active_gas_index` 是当前气体，但其他气体仍 enabled。

结果：

- 算法可以返回更优的未来切气计划。
- 即使当前没有切气，未来站点也可能使用 O2/EAN32。
- UI 显示的 `dur/hold/tts` 是“按算法未来最佳气体计划”的时间，不一定是“当前继续吸 active gas”的时间。

优点：

- TTS 更接近最优可执行计划。
- 能体现切气收益。

风险：

- 主 UI 倒计时可能误导用户：用户尚未切气，却看到按 O2 计算的减压时间。
- 如果用户忽略某个推荐气体但产品层尚未将其 ignored，plan 会继续使用它。

适用：

- 适合显示“最佳计划/推荐计划”。
- 不适合直接作为“当前实际呼吸气体下的主倒计时”，除非 UI 明确标注这是推荐计划。

### 方式 B：plan 只传当前 active gas

做法：

- `gas_plan.gas_count = 1`，只保留当前 active gas。
- 或保留数组但禁用所有非 active gas。

结果：

- 计划站点只会使用当前实际气体。
- UI 主倒计时语义最清楚：继续按当前气体潜水/减压，还剩多少。

优点：

- 主 UI 安全语义清晰，不会显示尚未切换气体的乐观时间。

风险：

- 无法直接从同一次 plan 获得“如果切到更优气体会怎样”的优化结果。
- TTS 可能明显偏保守。
- 如果推荐气体逻辑也依赖这份 gas plan，则无法推荐更优气体；所以推荐气体需要另一路调用。

适用：

- 适合主 UI 当前 DECO/NDL 倒计时。
- 需要额外调用 `arex_deco_recommend_gas()` 或另一份 full gas plan 来做推荐。

### 方式 C：plan 传 active gas + 已确认可用 gas

做法：

- 初始传全部 gas。
- 当用户错过/拒绝某气体后，将该气体 disabled/ignored。
- 当用户手动切到某气体后，该气体恢复为可用。
- 对尚未到 MOD 或尚未确认的气体是否 enabled，需要产品定义。

结果：

- 可以保留一定优化能力。
- 被用户忽略的气体不会继续进入未来计划。

优点：

- 接近真实潜水行为：用户明确拒绝或错过的气体不再被算法假设可用。

风险：

- “尚未明确拒绝但也尚未切换”的窗口期仍有歧义。例如 O2 MOD 4m，当前 3m，用户还未按确认，此时 plan 是否可以使用 O2？

适用：

- 适合作为产品层长期策略，但需要明确“未确认气体”的处理规则。

### 方式 D：双 plan / 双语义输出

做法：

- 调用 1：当前实际计划，只传 active gas 或 active + 已确认 gas，用于主 UI 倒计时、进度条、stop display。
- 调用 2：推荐/优化计划，传全部未 ignored gas，用于 `BETTER GAS AVAILABLE`、收益提示、未来计划页。

结果：

- 主 UI 显示真实当前气体语义。
- 仍然可以提示更优气体和优化后收益。

优点：

- 语义最清晰。
- 最不容易误导潜水员。

风险：

- 算法调用成本增加。
- 产品层需要维护两份 plan 的用途边界，避免混用。

适用：

- 适合最终产品形态。
- 需要算法侧确认 repeated plan 调用成本、状态复制方式、是否有推荐 API 可以替代第二次完整 plan。

### 方式 E：使用算法 `gas_switch_penalty_seconds`

做法：

- 如果产品不希望切气动作本身额外增加 60s，设置 `config->gas_switch_penalty_seconds = 0`。
- 如果产品希望保守估计切气耗时，设置为 60s 或其他值。

结果：

- penalty=60 时，可能出现 `hold=0 sw=60 dur=60` 的纯切气站。
- penalty=0 时，`sw=0`，`dur` 通常等于 `hold`。

建议：

- 如果 UI 不希望显示切气站，优先通过算法配置关闭 penalty，而不是 UI 私自把 duration 改成 hold。
- 如果未来需要显示“切气耗时”，应在 UI 上明确标注，不应混入减压 hold 倒计时。

## 6. 当前最核心的语义问题

### 问题 1：plan 的 `duration/hold` 是哪种气体语义？

当出现：

```text
step_gas=0(slot0 AIR)
plan_active=0(slot0 AIR)
stop gas=2(slot2 O2 100%)
```

需要明确：

- `hold=195s` 是不是“按 stop gas=O2 执行该站”的 hold？
- 如果当前继续吸 AIR，是否应该调用另一种 plan 才能得到 AIR 下的 hold？
- 算法是否支持“不自动规划未来气体切换，只按 active gas plan”的参数？

### 问题 2：`active_gas_index` 在 plan 中的作用边界

当前观察显示：

- `active_gas_index` 不等于“plan 只能使用这个气体”。
- 它更像未来计划的起点气体，planner 仍可选择其他 enabled gas。

需要算法侧确认：

- `active_gas_index` 是否只代表当前起始气体？
- planner 是否默认会在合适深度自动选择 enabled gas 中更优的气体？
- 是否有开关禁用这种自动选择？

### 问题 3：用户拒绝/错过推荐气体后，core 是否保存拒绝状态？

目前产品层理解：

- core 不保存“用户拒绝切气”策略状态。
- 如果用户忽略某气体，产品层必须在后续传入 gas plan 时 disabled/removed 该气体。

需要算法侧确认：

- 这个理解是否正确？
- disabled gas 是设置 `enabled=0` 即可，还是需要从 `gas_count` 中移除？
- 如果从 gas_count 移除，原始 gas index 到 UI slot 的映射是否应由产品层维护？

### 问题 4：推荐气体 API 和 plan API 是否应使用同一份 gas plan？

可能策略：

- 推荐气体调用使用全部未 ignored gas。
- 主 UI 当前倒计时 plan 使用当前 active gas 或已确认 gas。

需要算法侧确认：

- `arex_deco_recommend_gas()` 是否足够用于判断 BETTER GAS？
- 是否还需要 full gas plan 才能计算切气收益？
- 如果 recommend 和 plan 使用不同 gas set，算法侧是否认为这符合接口设计？

### 问题 5：O2 MOD / max PPO2 配置

日志中 O2 100% 进入算法时为：

```text
mod=4.0m maxpo2=1.40
```

需要确认：

- O2 100% 减压气是否应该使用 max PPO2 1.60，从而 MOD 约 6m？
- 不同气体是否支持各自独立 max PPO2？
- 当前产品层传入 `max_ppo2_bar` 时是否应直接使用每个 gas slot 的配置，而不是全局默认值？

## 7. 建议的产品层最终方向

当前倾向方案：

1. `arex_deco_step()` 永远使用真实 active gas。
2. `arex_deco_recommend_gas()` 使用全部未 ignored gas，用于 BETTER GAS。
3. 主 UI 的 NDL/DECO stop 倒计时，优先使用“当前实际气体语义”的 plan：
   - 只传 active gas，或
   - 只传 active + 用户已确认可用的 gas。
4. 优化计划页或切气收益提示可使用 full gas plan。
5. 如果用户错过或拒绝推荐气体，产品层将该 gas 标记 ignored，并从后续 plan 输入中禁用。
6. 如果用户手动切到 previously ignored gas，则恢复该 gas。
7. `gas_switch_penalty_seconds` 当前建议设为 0，避免纯切气站混入减压时间。

这个方向的核心是：主 UI 永远展示“按当前用户实际选择行为”的安全语义；更优气体作为推荐信息，不偷偷改变主倒计时。

## 8. 明天需要问算法工程师的问题清单

1. `arex_deco_plan()` 是否默认会自动使用所有 enabled gas 做未来最佳计划？
2. 是否存在参数或模式，让 plan 只使用 `active_gas_index`，不自动切到其他 enabled gas？
3. `ArexDecoGas.enabled = 0` 后，planner 和 recommend 是否都会完全忽略该气体？
4. 如果产品层移除 ignored gas，算法返回的 `gas_index` 是否只按压缩后的 gas plan index 返回？
5. `active_gas_index` 对 planner 的确切语义是什么：起点气体、当前气体、还是约束气体？
6. 推荐气体 API 是否只负责“当前深度最佳气体”，还是也考虑后续减压收益？
7. 如果主 UI 用 active-only plan，推荐气体用 full-gas recommend，这是否符合算法设计预期？
8. `gas_switch_penalty_seconds=0` 是否是关闭切气站/切气惩罚的推荐方式？
9. O2 100% 减压气的 max PPO2 是否建议使用 1.60？算法是否支持每个气体独立 max PPO2？
10. 对于“用户到达 O2 MOD 但尚未确认切气”的窗口期，算法侧建议产品把 O2 继续视为 available，还是必须等用户确认后才纳入主 UI plan？

## 9. 暂定结论

从日志看，当前问题不是简单的 gas index 错乱，也不是 `step` 把当前气体切错了。更准确地说：

- 实时组织推进使用的是 `step_gas`。
- 未来计划站点使用的是 planner 按 enabled gases 选择出的 `stop gas`。
- 当 UI 直接使用 full gas plan 的 stop time 时，可能显示尚未切换气体下的乐观减压时间。
- 当用户忽略/错过某个气体时，产品层必须及时从 plan gas set 中禁用该气体，否则 planner 会继续假设它可用。

因此，需要算法侧确认接口语义后，产品层再决定是否采用“双 plan / 双语义输出”的方式。
