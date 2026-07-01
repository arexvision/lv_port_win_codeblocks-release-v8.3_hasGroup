# AREX runtime current-stop 时间跳变问题记录

## 测试条件

- 测试时间：2026-07-01
- 日志来源：`C:\Users\admin\.codex\attachments\077ae7dc-89d9-4fe8-8af8-20f6e33c1829\pasted-text.txt`
- 测试过程：先通过 TCP 调试执行 `goto 40`，初始 `speed 40`，为观察细节后续降低到 `speed 10`。
- 气体：`slot0 AIR`
- 深度状态：日志中主要观察阶段为 `depth=40.0m` 停留。

说明：当前日志未直接打印 TCP 命令文本，因此 `goto 40 / speed 40 / speed 10` 为测试操作记录；问题证据来自 `[AREX_PLAN]`、`[AREX_RUNTIME_STOP]`、`[AREX_UI_PLAN]` 三类日志。

## 复核修正

此前把 `6m` 的 selector held 倒计时和 raw plan 中的 `6m` 停站时长混在一起解读，结论不准确。`6m` 片段里的 raw plan 时长整体是递增的，不能作为“raw 6m 非单调跳变”的主要证据。

本次复核后，主要异常证据应集中在 `12m` 和部分 `9m` 当前站：这些位置更能说明 selector 输出的主屏当前站时间会受 raw schedule 拓扑切换影响，出现用户可见的分钟回跳。

## 当前显示链路

当前模拟器侧已经按 AREX 0.0.27 的 selector 方案接入：

| 用途 | 当前数据来源 | 说明 |
|---|---|---|
| 主屏 `DECO xxm xxmin` | `arex_deco_select_runtime_stop()` 输出的 `ArexDecoRuntimeStop` | selector 只返回一个当前站，不返回完整列表 |
| PLAN 图 | raw `ArexDecoSchedule.stops[]`，但从 runtime current stop 对应站点开始裁剪 | 裁剪时会校验 `source_raw_index` 是否仍匹配当前 schedule 中的 depth/gas，不匹配则按 depth/gas 重找 |
| `[AREX_UI_PLAN]` | 与 PLAN 图同口径的过滤后显示列表 | 会统计 `hidden_before_current / hidden_suppressed / hidden_kind / hidden_zero` |
| TTS | raw `schedule.tts_seconds` | 不使用过滤后的显示列表重算 |
| raw 日志 / 调试 | 完整 raw `schedule.stops[]` | 不删除 waypoint / route / suppressed stop |

当前固件/模拟器侧的过滤规则：

| 规则 | 当前行为 |
|---|---|
| 主屏当前站 | 只相信 selector 输出的 `depth_m / remaining_seconds / total_seconds` |
| `DISPLAY_SUPPRESSED` | 不进 PLAN 图和 `[AREX_UI_PLAN]` |
| `ROUTE_WAYPOINT` / `GAS_SWITCH` | 不进 PLAN 图和 `[AREX_UI_PLAN]` |
| PLAN 图起点 | 从 selector 当前站匹配到的 raw stop 开始 |
| 分钟显示 | 正秒数向上取整，所以 `61s -> 2min`，`60s/59s -> 1min` |

## 现象摘要

主屏时间仍然会出现明显来回变化，表现类似：

```text
3min -> 2min -> 1min -> 2min
```

从日志看，这不是单纯的 UI 分钟向上取整造成的。UI 当前使用 selector 输出；而 selector 输出本身的 `remaining_seconds / total_seconds` 会随着 raw schedule 拓扑变化出现非单调跳变。

## 复核片段：6m raw plan 基本递增，不作为主要异常证据

| 日志行 | selector 当前站 | selector 剩余 | raw plan 中 6m 时长 | 备注 |
|---|---:|---:|---:|---|
| L106 | 6m | 81s / total 82s | 12s | selector 仍持有此前 `total=82s` 的当前站语义 |
| L110 | 6m | 62s / total 82s | 16s | raw 6m 递增，selector held 倒计时递减 |
| L115 | 6m | 51s / total 82s | 23s | raw 6m 继续递增 |
| L124 | 6m | 21s / total 82s | 28s | raw 拓扑变为 4 站，但 raw 6m 仍递增 |
| L128 | 6m | 37s / total 37s | 37s | selector 重新绑定到当前 raw 6m |
| L133 | 6m | 54s / total 54s | 54s | 绑定后 selector 与 raw 6m 同步增加 |
| L137 | 6m | 62s / total 62s | 62s | 继续增加 |
| L155 | 6m | 93s / total 93s | 93s | 继续增加 |

修正结论：`6m` raw plan 片段实际更接近 `12s -> 16s -> 23s -> 28s -> 37s -> 54s -> 62s -> 93s` 的递增趋势。此前写的 `82s -> 21s -> 37s -> 93s` 混合了 selector held 倒计时和 raw 6m plan 时长，不能作为主要问题描述。

这个片段仍然有价值，但它说明的是 selector 的两种语义在日志中会交错出现：

| 阶段 | 表现 | 含义 |
|---|---|---|
| held 阶段 | selector `total=82s`，`remaining` 递减；raw 6m 从 `12s` 增到 `28s` | selector 当前站沿用上一轮显示状态，不等于当前 raw 6m 时长 |
| rebind 阶段 | selector 变为 `total=37s`，之后 `54s/62s/93s` | selector 重新绑定到当前 raw 6m，之后与 raw plan 同步 |

因此，`6m` 片段不适合用来证明 raw plan 非单调；真正要发给算法侧的问题应放在下面的 `9m/12m` 片段。

## 关键证据 1：9m 当前站从 2min 掉到 0，再回到 2min

| 日志行 | selector 当前站 | selector 剩余 | UI 分钟 | raw plan 片段 | 备注 |
|---|---:|---:|---:|---|---|
| L163 | 9m | 111s / total 111s | 2min | `9m/110s, 6m/64s, 3m/146s` | 9m 成为当前站 |
| L167 | 9m | 40s / total 40s | 1min | `12m/2s, 9m/40s, 6m/96s, 3m/183s` | raw 拓扑变化后，9m 剩余大幅降低 |
| L171 | 9m | 22s / total 40s | 1min | `15m/1s, 12m/1s, 9m/8s, 6m/113s` | selector 保持 9m，但 raw 9m 只有 8s |
| L180 | 9m | 0s / total 40s | 0min | `15m/1s, 12m/1s, 9m/24s, 6m/113s` | 当前站倒到 0，但 raw 9m 仍有 24s |
| L185 | 9m | 38s / total 38s | 1min | `15m/1s, 12m/1s, 9m/38s, 6m/113s` | total 被重新绑定到 raw 9m |
| L198 | 9m | 64s / total 64s | 2min | `15m/1s, 12m/1s, 9m/64s, 6m/114s` | 又从 1min 回到 2min |

结论：`9m` 当前站不是平滑倒计时，而是 `111s -> 40s -> 22s -> 0s -> 38s -> 64s`。这会直接造成主屏 `2min -> 1min -> 0/1min -> 2min` 的观感。

## 关键证据 2：12m 当前站出现 3min -> 2min -> 1min -> 2min

| 日志行 | selector 当前站 | selector 剩余 | UI 分钟 | raw plan 片段 | 备注 |
|---|---:|---:|---:|---|---|
| L239 | 12m | 125s / total 125s | 3min | `12m/125s, 9m/82s, 6m/195s, 3m/363s` | 当前站为 12m，显示 3min |
| L244 | 12m | 136s / total 136s | 3min | `12m/136s, 9m/82s, 6m/205s, 3m/394s` | 仍为 3min |
| L249 | 12m | 119s / total 137s | 2min | `18m/1s, 15m/1s, 12m/20s, 9m/110s` | raw 拓扑切到包含 18m/15m waypoint，selector 保持 12m 但开始递减 |
| L258 | 12m | 30s / total 30s | 1min | `18m/1s, 15m/1s, 12m/30s, 9m/110s` | total 从 137s 重置到 30s |
| L282 | 12m | 61s / total 61s | 2min | `18m/1s, 15m/1s, 12m/61s, 9m/125s` | 又回到 2min |
| L287 | 12m | 69s / total 69s | 2min | `18m/1s, 15m/1s, 12m/69s, 9m/136s` | 持续增加 |

结论：这里最贴近肉眼看到的异常：`12m` 当前站从 `3min -> 2min -> 1min -> 2min`。其中 `119s -> 2min` 是分钟取整，但 `136s -> 119s -> 30s -> 61s` 不是一个稳定 current-stop 倒计时应有的单调趋势。

## 初步判断

当前问题不像是固件显示过滤造成的，原因如下：

1. 主屏显示来自 `ArexDecoRuntimeStop`，即 selector 输出，不直接取 raw `stops[0]`。
2. `[AREX_UI_PLAN]` 已经从 selector current stop 起点裁剪，并隐藏 `hidden_before_current`。
3. 日志中跳变发生在 `[AREX_RUNTIME_STOP]` 本身，例如 `12m rem=136s -> 119s -> 30s -> 61s`。
4. raw schedule 在同一深度附近会发生拓扑切换，例如从 `12m/136s` 切到 `18m/1s, 15m/1s, 12m/20s`，随后 selector 输出也出现重绑定或 total 重置。

因此当前更像是算法 core selector / raw plan 拓扑交互问题，而不是 UI 层只显示错了。

## 需要算法侧确认的问题

1. 在潜水员仍停在 40m、没有实际上升到 stop zone 时，runtime current stop 的 `remaining_seconds` 是否应该允许非单调下降后再上升？
2. selector 在 `HELD_PREVIOUS` 和 raw schedule 拓扑变化时，是否应该保持 displayed stop 的 total/remaining 连续性？
3. 当 raw schedule 从 `12m/136s` 变成 `18m/1s, 15m/1s, 12m/20s` 时，selector 是否应该把当前 `12m` 的 total 从 `137s` 重置为 `30s/39s/61s`？
4. 当前 `source_raw_index` 与 raw schedule 拓扑变化后的 stop identity 是否有额外语义约束？固件侧已经改为校验 depth/gas 后再使用 index，但 selector 输出时间仍会跳变。
5. 对用户主屏而言，是否需要 selector 保证同一 displayed stop 在远深处不会出现 `3min -> 2min -> 1min -> 2min` 这类回跳？

## 固件侧当前不建议做的兜底

固件可以强行做 UI hysteresis，例如同一 depth 下不允许分钟减少后再增加，或者不允许 remaining_seconds 增加。但这会改变 selector 的官方语义，并可能掩盖算法实际状态变化。

建议优先由算法侧确认 selector 对 `remaining_seconds / total_seconds` 的稳定性承诺。如果算法侧认为当前输出符合预期，固件侧再单独定义产品级显示平滑策略。
