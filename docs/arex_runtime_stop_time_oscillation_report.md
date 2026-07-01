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

## 日志打印含义

| 日志 tag | 数据层级 | 主要含义 |
|---|---|---|
| `[AREX_CALL]` | runtime 调用汇总 | 每次 `step/plan` 后的摘要，包含当前深度、step/plan 返回值、NDL、ceiling、GF99、SurfGF、raw stop 数量和 raw TTS。它是本帧算法调用总览，不是 UI 过滤后的显示列表。 |
| `[AREX_PLAN]` | core raw schedule | `arex_deco_plan()` 返回的原始 `ArexDecoSchedule.stops[]`。这里保留所有 raw stop，包括 route waypoint、suppressed stop、1s stop 等。 |
| `[AREX_RUNTIME_STOP]` | 0.0.27 selector 输出 | `arex_deco_select_runtime_stop()` 返回的用户当前站。主屏 `DECO xxm xxmin` 使用这里的 `depth/rem/total`。selector 只返回一个当前站，不返回完整 plan 列表。 |
| `[AREX_UI_PLAN]` | 固件/UI 展示列表 | PLAN 图和 UI 调试使用的过滤后列表。当前逻辑从 selector current stop 匹配到的 raw stop 开始，隐藏 current stop 之前的 raw stop，并继续过滤 suppressed/kind/zero stop。 |

常用字段说明：

| 字段 | 出现位置 | 含义 |
|---|---|---|
| `depth` | `CALL/PLAN/RUNTIME_STOP` | 当前水深或 selector 当前站深度。 |
| `ceiling` | `CALL/PLAN` | core 返回的当前 ceiling。 |
| `gf99` / `surfgf` | `CALL/PLAN` | core 返回的 GF99 / SurfGF。 |
| `tts` / `tts_raw` | `CALL/PLAN/UI_PLAN` | raw `schedule.tts_seconds`，UI 不按过滤后 stop 重算。 |
| `stops` / `raw_stops` | `CALL/PLAN/UI_PLAN` | raw schedule stop 数量。 |
| `dur` | `PLAN` | raw stop 的 `duration_seconds`。 |
| `hold` | `PLAN` | 普通减压站展示用停留秒数。 |
| `sw` | `PLAN` | switch penalty seconds，纯切气站相关。 |
| `kind` | `PLAN/UI_PLAN` | stop 类型；当前日志中 `0` 为 mandatory，`1` 为 route waypoint，`3` 为 safety。 |
| `flags` | `PLAN/UI_PLAN` | `0x01` 表示 `DISPLAY_SUPPRESSED`，不应作为用户当前实质站展示；`0x00` 表示未 suppressed。 |
| `avail` | `RUNTIME_STOP` | selector 是否返回可用当前站。 |
| `raw#` | `RUNTIME_STOP/UI_PLAN` | 对应 raw schedule stop 序号；`raw#0` 表示无可用 raw stop。 |
| `rem` / `total` | `RUNTIME_STOP` | selector 当前站剩余秒数 / 总秒数，主屏时间直接来自这里。 |
| `reason` | `RUNTIME_STOP` | selector 选择原因枚举值，当前文档只用它区分 selector 输出状态，不按具体枚举名判断。 |
| `active` / `cand` / `cand_seen` | `RUNTIME_STOP` | selector 内部 displayed stop 与 candidate debounce 状态。 |
| `display` | `UI_PLAN` | UI/PLAN 图最终展示的 stop 数量。 |
| `hidden_before_current` | `UI_PLAN` | 因 selector current stop 起点裁剪而隐藏的前置 raw stop 数量。 |
| `hidden_suppressed` / `hidden_kind` / `hidden_zero` | `UI_PLAN` | 分别表示因 suppressed flag、stop kind、0 秒时长被隐藏的数量。 |

`[AREX_CALL]` 和 `[AREX_PLAN]` 的对应关系：`CALL` 是算法调用摘要，打印频率可能高于 `PLAN` 详细 stop 列表；`PLAN` 是某次 raw schedule 详细展开。看当前站显示问题时，应同时看同一段附近的 `[AREX_PLAN]`、`[AREX_RUNTIME_STOP]` 和 `[AREX_UI_PLAN]`。

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

## 关键原始日志摘录

以下为关键字段摘录，完整日志来源见本文开头路径。为降低噪声，重复的 `gas/gf/flags` 等字段在部分行中省略，但保留了判断问题所需的 `depth / ceiling / stops / dur / rem / total / display`。

### 6m 复核片段：raw plan 递增，selector 先 held 后 rebind

```text
L105 [AREX_PLAN] ceiling=2.18m tts=356s stops=3 | #1 9m dur=1s | #2 6m dur=12s | #3 3m dur=76s
L106 [AREX_RUNTIME_STOP] avail=1 raw#1 depth=6.00m rem=81s total=82s reason=3 active=1
L107 [AREX_UI_PLAN] #1 raw#2 6.00m 12s/1min | #2 raw#3 3.00m 76s/2min | display=2 hidden_before_current=1

L109 [AREX_PLAN] ceiling=2.49m tts=377s stops=3 | #1 9m dur=11s | #2 6m dur=16s | #3 3m dur=83s
L110 [AREX_RUNTIME_STOP] avail=1 raw#1 depth=6.00m rem=62s total=82s reason=3 active=1

L123 [AREX_PLAN] ceiling=3.12m tts=409s stops=4 | #1 12m dur=1s | #2 9m dur=1s | #3 6m dur=28s | #4 3m dur=112s
L124 [AREX_RUNTIME_STOP] avail=1 raw#1 depth=6.00m rem=21s total=82s reason=3 active=1

L127 [AREX_PLAN] ceiling=3.26m tts=418s stops=4 | #1 12m dur=1s | #2 9m dur=1s | #3 6m dur=37s | #4 3m dur=112s
L128 [AREX_RUNTIME_STOP] avail=1 raw#3 depth=6.00m rem=37s total=37s reason=3 active=1

L136 [AREX_PLAN] ceiling=3.67m tts=453s stops=4 | #1 12m dur=1s | #2 9m dur=1s | #3 6m dur=62s | #4 3m dur=122s
L137 [AREX_RUNTIME_STOP] avail=1 raw#3 depth=6.00m rem=62s total=62s reason=3 active=1
```

对应含义：

| 日志 | 含义 |
|---|---|
| `L105/L109/L123/L127/L136 [AREX_PLAN]` | raw 6m 时长从 `12s -> 16s -> 28s -> 37s -> 62s`，整体递增。 |
| `L106/L110/L124 [AREX_RUNTIME_STOP]` | selector 还在 held 之前的 `total=82s` 当前站，所以 `rem` 递减，不应拿它直接和当前 raw 6m dur 比较。 |
| `L128/L137 [AREX_RUNTIME_STOP]` | selector 重新绑定到当前 raw 6m，之后 `rem/total` 与 raw 6m 基本一致。 |

### 9m 片段：selector 当前站发生可见时间回跳

```text
L161 [AREX_PLAN] ceiling=4.55m tts=587s stops=3 | #1 9m dur=110s | #2 6m dur=64s | #3 3m dur=146s
L163 [AREX_RUNTIME_STOP] avail=1 raw#1 depth=9.00m rem=111s total=111s reason=3 active=1
L162 [AREX_UI_PLAN] #1 raw#1 9.00m 110s/2min | #2 raw#2 6.00m 64s/2min | #3 raw#3 3.00m 146s/3min

L166 [AREX_PLAN] ceiling=4.76m tts=588s stops=4 | #1 12m dur=2s | #2 9m dur=40s | #3 6m dur=96s | #4 3m dur=183s
L167 [AREX_RUNTIME_STOP] avail=1 raw#2 depth=9.00m rem=40s total=40s reason=3 active=1
L168 [AREX_UI_PLAN] #1 raw#2 9.00m 40s/1min | #2 raw#3 6.00m 96s/2min | #3 raw#4 3.00m 183s/4min

L170 [AREX_PLAN] ceiling=4.95m tts=599s stops=5 | #1 15m dur=1s | #2 12m dur=1s | #3 9m dur=8s | #4 6m dur=113s | #5 3m dur=209s
L171 [AREX_RUNTIME_STOP] avail=1 raw#2 depth=9.00m rem=22s total=40s reason=3 active=1

L179 [AREX_PLAN] ceiling=5.17m tts=628s stops=5 | #1 15m dur=1s | #2 12m dur=1s | #3 9m dur=24s | #4 6m dur=113s | #5 3m dur=222s
L180 [AREX_RUNTIME_STOP] avail=1 raw#2 depth=9.00m rem=0s total=40s reason=3 active=1

L184 [AREX_PLAN] ceiling=5.36m tts=654s stops=5 | #1 15m dur=1s | #2 12m dur=1s | #3 9m dur=38s | #4 6m dur=113s | #5 3m dur=234s
L185 [AREX_RUNTIME_STOP] avail=1 raw#3 depth=9.00m rem=38s total=38s reason=3 active=1
```

对应含义：

| 日志 | 含义 |
|---|---|
| `L161-L163` | selector 当前站为 `9m`，主屏显示约 `2min`。 |
| `L166-L168` | raw schedule 拓扑加入 `12m` 后，9m raw dur 变为 `40s`，selector 也变成 `40s`，主屏会从 `2min` 掉到 `1min`。 |
| `L170-L180` | raw schedule 又加入 `15m/12m` waypoint，selector 仍显示 9m 但 held 到 `0s`，此时 raw 9m 已经从 `8s` 增到 `24s`。 |
| `L184-L185` | selector 重新绑定到 raw#3 9m，`rem/total` 变为 `38s`，随后继续增加。 |

### 12m 片段：最接近用户肉眼看到的 3min -> 2min -> 1min -> 2min

```text
L238 [AREX_PLAN] ceiling=6.75m tts=1032s stops=4 | #1 12m dur=125s | #2 9m dur=82s | #3 6m dur=195s | #4 3m dur=363s
L239 [AREX_RUNTIME_STOP] avail=1 raw#1 depth=12.00m rem=125s total=125s reason=3 active=1

L243 [AREX_PLAN] ceiling=6.87m tts=1084s stops=4 | #1 12m dur=136s | #2 9m dur=82s | #3 6m dur=205s | #4 3m dur=394s
L244 [AREX_RUNTIME_STOP] avail=1 raw#1 depth=12.00m rem=136s total=136s reason=3 active=1

L248 [AREX_PLAN] ceiling=6.99m tts=1051s stops=6 | #1 18m dur=1s | #2 15m dur=1s | #3 12m dur=20s | #4 9m dur=110s | #5 6m dur=217s | #6 3m dur=435s
L249 [AREX_RUNTIME_STOP] avail=1 raw#1 depth=12.00m rem=119s total=137s reason=3 active=1

L256 [AREX_PLAN] ceiling=7.10m tts=1104s stops=6 | #1 18m dur=1s | #2 15m dur=1s | #3 12m dur=30s | #4 9m dur=110s | #5 6m dur=227s | #6 3m dur=468s
L258 [AREX_RUNTIME_STOP] avail=1 raw#3 depth=12.00m rem=30s total=30s reason=3 active=1

L282 [AREX_RUNTIME_STOP] avail=1 raw#3 depth=12.00m rem=61s total=61s reason=3 active=1
L287 [AREX_RUNTIME_STOP] avail=1 raw#3 depth=12.00m rem=69s total=69s reason=3 active=1
```

对应含义：

| 日志 | 含义 |
|---|---|
| `L238-L244` | 当前站 `12m` 从 `125s` 增到 `136s`，主屏显示约 `3min`。 |
| `L248-L249` | raw schedule 拓扑变为 `18m/1s, 15m/1s, 12m/20s...`，selector 仍显示 12m，但 `rem` 开始从 `137s` held 递减到 `119s`，主屏变为 `2min`。 |
| `L256-L258` | selector 重新绑定到 raw#3 12m，`total` 变成 `30s`，主屏变为 `1min`。 |
| `L282-L287` | 同一个 12m 当前站又增长到 `61s/69s`，主屏回到 `2min`。 |

## 需要算法侧确认的问题

1. 在潜水员仍停在 40m、没有实际上升到 stop zone 时，runtime current stop 的 `remaining_seconds` 是否应该允许非单调下降后再上升？
2. selector 在 `HELD_PREVIOUS` 和 raw schedule 拓扑变化时，是否应该保持 displayed stop 的 total/remaining 连续性？
3. 当 raw schedule 从 `12m/136s` 变成 `18m/1s, 15m/1s, 12m/20s` 时，selector 是否应该把当前 `12m` 的 total 从 `137s` 重置为 `30s/39s/61s`？
4. 当前 `source_raw_index` 与 raw schedule 拓扑变化后的 stop identity 是否有额外语义约束？固件侧已经改为校验 depth/gas 后再使用 index，但 selector 输出时间仍会跳变。
5. 对用户主屏而言，是否需要 selector 保证同一 displayed stop 在远深处不会出现 `3min -> 2min -> 1min -> 2min` 这类回跳？

## 固件侧当前不建议做的兜底

固件可以强行做 UI hysteresis，例如同一 depth 下不允许分钟减少后再增加，或者不允许 remaining_seconds 增加。但这会改变 selector 的官方语义，并可能掩盖算法实际状态变化。

建议优先由算法侧确认 selector 对 `remaining_seconds / total_seconds` 的稳定性承诺。如果算法侧认为当前输出符合预期，固件侧再单独定义产品级显示平滑策略。
