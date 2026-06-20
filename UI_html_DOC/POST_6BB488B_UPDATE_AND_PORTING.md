# 6bb488b 之后更新与真机适配说明

本文只记录 `6bb488bd8810e81ad280a453655949d8f858c375` 之后的增量改动，以及真机侧需要如何适配。后续同类算法适配、PC 模拟、生命周期、调试链路变更也统一追加到本文，不再分散写入旧专题文档。

## 变更总览

| 提交/改动 | 主题 | 影响范围 |
|---|---|---|
| `48ba776` | 修正指南针模拟倍速行为 | PC 模拟器 |
| `72ee5b9` | 增加指南针独立倍速指令 | PC TCP 调试 |
| `f26724a` | 让指南针倍速实时刷新 | PC 模拟器 |
| `26087b4` | 修正水面算法按 AIR 恢复 | 算法适配层、PC 生命周期 |
| 本次改动 | BETTER GAS 生命周期门控 | 算法适配层、data bus、PC 生命周期 |

## 指南针模拟更新

旧行为：

- TCP `speed` 同时影响深度/时间/算法推进和 heading 变化。
- heading 只跟随 1 秒主模拟 tick 更新，倍速时看不到连续变化过程。

新行为：

- `speed` 只表示潜水时间、深度脚本、goto、算法 tick 的模拟倍速。
- 新增 `heading_speed` / `hspeed`，单独控制 heading 每秒变化多少度。
- heading 使用 10ms 定时器刷新，因此 `heading_speed 10` 时，能在 1 秒内看到 heading 从 0 到 10 的连续变化。

真机侧适配：

- 真机不需要实现 TCP `heading_speed` 命令。
- 真机 heading 应来自真实罗盘/姿态融合输出，不应被潜水时间倍速影响。
- 如果真机有工程测试注入通道，可以参考 PC 语义：heading 注入速度和潜水时间推进分离。

## 潜水生命周期与水面 AIR

旧行为：

- PC 模拟器主要靠 `in_dive` / `surfacing_pending` 两个布尔值判断生命周期。
- 出水确认旧阈值是 `0.8m / 5s`。
- 已经出水后，实时 `deco_core_tick()` 和后续 `arex_deco_plan()` 仍可能使用 UI 当前 active gas。
- 如果用户处于多气体配置，水面组织恢复和 plan 可能被当前选择气体污染。

新行为：

- PC 模拟器内部使用四态生命周期：
  - `SURFACE_CONFIRMED`
  - `ENTRY_PENDING`
  - `DIVE_ACTIVE`
  - `SURFACING_PENDING`
- 入水确认：`depth >= 1.2m` 连续 `3` 个模拟秒后进入 `DIVE_ACTIVE`。
- 出水确认：`depth <= 0.2m` 连续 `30` 个模拟秒后进入 `SURFACE_CONFIRMED`。
- `speed` 会同步加速入水/出水确认计时，因为生命周期 tick 按模拟秒执行。
- `SURFACE_CONFIRMED` 和 `ENTRY_PENDING` 下，算法适配层按 `0m + AIR` 推进。
- 进入 `DIVE_ACTIVE` 后，算法才使用真实深度和 UI 当前 active gas。
- `rtc_offline` 只允许在 `SURFACE_CONFIRMED` 执行，算法输入固定为 `0m + AIR`。

真机侧适配：

- 真机必须有自己的潜水生命周期状态机，不能只依赖 `dive_time_s > 0` 或单个深度阈值。
- 真机确认水面后，需要调用 `deco_core_set_surface_confirmed(true)` 或等价接口。
- 真机确认进入潜水后，需要调用 `deco_core_set_surface_confirmed(false)` 或等价接口。
- 水面 AIR 只影响算法恢复计算，不能改写 UI 当前气体、气体槽配置或 active gas 显示。
- 真机 `rtc_offline` 必须只在确认水面后执行，并按 `0m + AIR` 计算离线恢复。

## BETTER GAS 生命周期门控

旧行为：

- `BETTER GAS AVAILABLE` 的产品门控只看 `bus_get_dive_time_s() > 0`。
- 刚入水确认后，只要算法推荐有效，就可能立即弹出 `BETTER GAS`。

新行为：

- data bus 新增共享生命周期 phase：
  - `DIVE_LIFECYCLE_SURFACE_CONFIRMED`
  - `DIVE_LIFECYCLE_ENTRY_PENDING`
  - `DIVE_LIFECYCLE_ACTIVE`
  - `DIVE_LIFECYCLE_SURFACING_PENDING`
- PC 模拟器状态机每次切换状态都会同步写入 `bus_set_dive_lifecycle_phase()`。
- `BETTER GAS` 只有在 phase 为 `DIVE_LIFECYCLE_ACTIVE` 或 `DIVE_LIFECYCLE_SURFACING_PENDING` 时才可能提示。
- 除生命周期外，还必须满足以下任一业务场景：
  - 当前 `stop_type == STOP_DECO`
  - `ceiling_m > 0.01m`
  - `TTS > 0`
  - 明显上升：`ascent_rate > DECO_GAS_SWITCH_ASCENT_MIN_MPM`
- `DECO_GAS_SWITCH_ASCENT_MIN_MPM` 默认 `1.0 m/min`。
- 不满足门控时，适配层写 `recommended_gas_idx = -1` 并清除 `INFO_GAS_SWITCH`。

真机侧适配：

- 真机平台需要写入同一个 lifecycle phase，供算法适配、告警和后续 UI 逻辑共用。
- 真机不能继续用 `dive_time_s > 0` 作为 `BETTER GAS` 的唯一提示条件。
- 算法库仍只负责输出推荐气体；是否提示潜水员由产品层按 lifecycle phase、减压义务和上升趋势判断。
- 如果真机有自己的上升率滤波，写入 `bus_set_ascent_rate()` 的值应保持“正数=上升”的语义。

## 验证清单

- `heading_speed 10`：heading 在 1 秒内连续变化约 10 度，`speed` 不再影响 heading。
- `speed 30`：PC 出水确认的 30 个模拟秒约 1 个真实秒完成。
- 多气体模式下刚入水：`ENTRY_PENDING` 不弹 `BETTER GAS`。
- 刚进入 `DIVE_ACTIVE` 且无减压、无明显上升：不弹 `BETTER GAS`。
- `STOP_DECO` / ceiling / TTS 存在且算法推荐有效：弹 `BETTER GAS`。
- 上升率超过 `1.0 m/min` 且算法推荐有效：弹 `BETTER GAS`。
- 回到 `SURFACE_CONFIRMED`：推荐气体清空，`BETTER GAS` 消失。
- `rtc_offline`：只有确认水面后允许执行，并按 `0m + AIR` 计算。
