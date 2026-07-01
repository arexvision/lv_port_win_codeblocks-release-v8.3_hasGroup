# AREX Ceiling 稳定性最终测试日志分析

## 1. 数据来源

- 日志文件：`C:\Users\admin\.codex\attachments\7ab180f3-7614-4ec3-afda-6df5c99ddde3\pasted-text.txt`
- 测试日期：2026-07-01
- 场景特征：AIR，GF `30/70`，深度快速进入并停留在 `40.0m`，观察 NDL 归零后 ceiling / TTS / stop list / display filter 的变化。
- 固件日志版本：已包含 `ceiling / gf99 / surfgf / kind / flags / hidden_suppressed`。

## 2. 日志字段语义

| 日志 | 含义 | 注意 |
|---|---|---|
| `AREX_CALL` | 一次 runtime 调用摘要，包含 `arex_deco_step()` 结果和随后 `arex_deco_plan()` 摘要 | 受独立节流控制，不保证和下一行 `AREX_PLAN` 一一对应 |
| `AREX_PLAN` | 某次 `arex_deco_plan()` 返回的 raw schedule 明细 | 受独立节流控制，用于看 raw stop list |
| `AREX_UI_PLAN` | 固件 display filter 后的用户可见计划 | 当前只跳过 `flags & DISPLAY_SUPPRESSED` 的 raw stop |

重要：`AREX_CALL` 和 `AREX_PLAN` 各自有打印节流，因此相邻两行不一定来自同一次 plan。判断 stop list 时以 `AREX_PLAN + AREX_UI_PLAN` 成对观察更可靠；判断连续 ceiling 走势时优先看同一类日志序列。

## 3. 总体结论

| 观察项 | 结论 |
|---|---|
| ceiling 稳定性 | 在本日志中单调上升：`0.00m -> 8.88m`，未观察到回落、震荡或跳负 |
| NDL 到 DECO 切换 | `ndl=10s` 后下一次进入 `ndl=0s`，ceiling 从 `0.00m` 到 `0.62m` |
| SurfaceGF | 单调上升：`0.0% -> 146.8%`，与停底累积趋势一致 |
| GF99 | 全程打印为 `0.0%`，这可能是“当前 40m 环境压力下组织未超过环境压力”的结果；如果预期 GF99 非 0，需要算法侧确认 GF99 口径 |
| TTS | 从 safety stop 阶段的 `446s`，进入强制减压瞬间变为 `284s`，后续随 ceiling 上升逐步增加到 `1897s` |
| raw stop list | 不稳定，停站数量在 `1 / 2 / 3 / 4 / 5 / 6` 之间切换 |
| UI display stop | 仍会出现 `9m/1s`、`12m/1s`、`15m/1s`、`18m/1s` 作为可见首站，原因是这些 stop 的 `flags=0x00` |
| suppressed filter | 当 core 设置 `flags=0x01` 时，固件能正确隐藏 route waypoint |

## 4. Ceiling 走势摘要

`AREX_CALL` 序列中，40m 停底后的 ceiling 变化如下：

| 阶段 | NDL | Ceiling | SurfaceGF | TTS | 说明 |
|---|---:|---:|---:|---:|---|
| NDL 充足 | `331s -> 10s` | `0.00m` | `0.0% -> 68.2%` | `446s` | 未进入强制减压，仍有 safety stop 计划 |
| 进入 DECO | `0s` | `0.62m` | `74.8%` | `284s` | 首次出现 ceiling，raw stop 为 `6m/17s` |
| 早期 ceiling 增长 | `0s` | `1.40m -> 4.82m` | `80.7% -> 106.9%` | `312s -> 581s` | ceiling 单调上升，但 raw stop list 频繁变形 |
| 中期 ceiling 增长 | `0s` | `5.23m -> 6.79m` | `110.0% -> 126.2%` | `635s -> 1043s` | 出现 `15m/1s` route waypoint 显示/隐藏切换 |
| 后期 ceiling 增长 | `0s` | `7.02m -> 8.88m` | `129.3% -> 146.8%` | `1064s -> 1897s` | `18m/1s` route waypoint 从隐藏切到显示 |

稳定性判断：只看 ceiling，本次日志没有看到异常回落；它是逐步增大的。但 stop list 和 UI 首站有明显跳变，这会让用户感知为“不稳定”。

## 5. 关键时间点对比

| 日志行 | Ceiling | Raw stops | UI display | 判断 |
|---:|---:|---|---|---|
| 46-47 | `0.62m` | `6m/17s kind=0 flags=0x00` | `6m/17s` | 强制减压刚出现 |
| 49-50 | `1.40m` | `9m/1s kind=1 flags=0x00`; `6m/1s kind=1 flags=0x00`; `3m/43s` | `9m/1s` 起显 | route waypoint 未 suppressed，UI 会显示 1s 首站 |
| 52-53 | `2.11m` | `6m/79s`; `3m/19s` | `6m/79s` | stop list 变回实质停站 |
| 58-59 | `3.65m` | `12m/1s kind=1 flags=0x00`; `9m/1s kind=1 flags=0x00`; `6m/60s`; `3m/119s` | `12m/1s` 起显 | route waypoint 未 suppressed |
| 65-66 | `4.82m` | `15m/1s flags=0x01`; `12m/1s flags=0x01`; `9m/1s flags=0x01`; `6m/111s`; `3m/200s` | `6m/111s` 起显 | core suppress 生效，固件过滤正确 |
| 68-69 | `5.25m` | `15m/1s flags=0x01`; `12m/1s flags=0x01`; `9m/29s`; `6m/113s`; `3m/226s` | `9m/29s` 起显 | 9m 已变成实质停站 |
| 72-73 | `5.94m` | `15m/1s kind=1 flags=0x00`; `12m/1s kind=1 flags=0x00`; `9m/79s`; `6m/130s`; `3m/294s` | `15m/1s` 起显 | suppress 取消，UI 首站跳深 |
| 81-82 | `6.79m` | `12m/128s`; `9m/82s`; `6m/197s`; `3m/369s` | `12m/128s` 起显 | stop list 重新收敛为实质停站 |
| 84-85 | `7.02m` | `18m/1s flags=0x01`; `15m/1s flags=0x01`; `12m/23s`; `9m/110s`; `6m/219s`; `3m/443s` | `12m/23s` 起显 | 18m/15m 被隐藏，符合 display filter |
| 96-97 | `8.30m` | `18m/1s flags=0x01`; `15m/1s kind=0 flags=0x00`; `12m/99s`; `9m/174s`; `6m/312s`; `3m/724s` | `15m/1s` 起显 | 15m 变为 kind=0 但仍只有 1s |
| 99-100 | `8.56m` | `18m/1s kind=1 flags=0x00`; `15m/16s`; `12m/99s`; `9m/192s`; `6m/337s`; `3m/809s` | `18m/1s` 起显 | 18m route waypoint 未 suppressed，UI 显示 |
| 103-104 | `8.88m` | `18m/1s kind=1 flags=0x00`; `15m/35s`; `12m/99s`; `9m/214s`; `6m/368s`; `3m/913s` | `18m/1s` 起显 | 18m/1s 继续可见 |

## 6. Ceiling 本身是否稳定

从 `AREX_CALL` 抽取到的 40m 停底 ceiling 序列：

```text
0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00,
0.62, 1.40, 2.11, 2.76, 3.35, 3.88, 4.37, 4.82, 5.23, 5.60,
5.94, 6.25, 6.53, 6.79, 7.02, 7.23, 7.52, 7.82, 8.11, 8.38,
8.64, 8.88
```

结论：

- 没有出现下降点。
- 没有出现 `ceiling > 0` 后又回到 `0`。
- 没有出现明显单点尖峰。
- 增长大致连续，最大可见步进来自日志打印间隔，不一定是算法每秒真实步进。

因此，“ceiling 数值稳定性”本身目前看是好的；用户感知到的不稳定更可能来自 plan stop list / display stop 的切换。

## 7. 主要异常/疑点

### 7.1 进入 DECO 瞬间 TTS 从 446s 降到 284s

在 NDL 阶段：

```text
ceiling=0.00m, stops=1, tts=446s, stop=5m/180s kind=SAFETY
```

进入强制减压后：

```text
ceiling=0.62m, stops=1, tts=284s, stop=6m/17s kind=MANDATORY
```

这表示 safety stop 计划和 mandatory deco 计划在 TTS 语义上发生切换。它可能是算法预期行为，但产品显示上会表现为：刚进入 DECO 时 TTS 反而变短。建议算法侧确认 `safety stop` 与 `mandatory deco` 切换时 TTS 是否期望平滑。

### 7.2 `ROUTE_WAYPOINT + flags=0x00` 会直接进入 UI

固件当前规则是：只隐藏 `flags & DISPLAY_SUPPRESSED` 的 stop。因此下面这种 raw stop 会显示：

```text
15m/1s kind=1 flags=0x00
18m/1s kind=1 flags=0x00
```

这符合 0.0.26 文档里的协议，但会导致主屏/PLAN 首站显示 `15m/1s` 或 `18m/1s`。如果这些仍是“路径连续性 waypoint”，core 应继续设置 `DISPLAY_SUPPRESSED`；如果 core 认为它们贴近当前 ceiling，应该显示，则需要产品侧确认这种显示风格是否接受。

### 7.3 suppress 状态存在阶段性切换

同类 route waypoint 在不同 ceiling 段的 `flags` 会变化：

| Ceiling 段 | Raw route waypoint | flags | UI 结果 |
|---:|---|---|---|
| `4.82m` | `15m/1s`, `12m/1s`, `9m/1s` | `0x01` | 全部隐藏，UI 从 `6m/111s` 开始 |
| `5.94m` | `15m/1s`, `12m/1s` | `0x00` | UI 从 `15m/1s` 开始 |
| `7.02m - 8.30m` | `18m/1s`, `15m/1s` | 前段 `0x01` | UI 从 `12m` 或 `15m` 开始 |
| `8.56m+` | `18m/1s` | `0x00` | UI 从 `18m/1s` 开始 |

这说明固件过滤逻辑已经按协议执行，但 core 的 display classification 会改变用户可见首站。

## 8. 给算法工程师的问题描述

本次 40m 停底日志中，`ceiling` 自身表现稳定：进入强制减压后从 `0.62m` 单调增长到 `8.88m`，未观察到回落或震荡。

但 raw schedule / display schedule 有明显用户可见跳变：

- `ceiling=1.40m` 时 UI 显示 `9m/1s` 和 `6m/1s`，两者都是 `kind=ROUTE_WAYPOINT` 且 `flags=0x00`。
- `ceiling=4.82m` 时 `15m/1s, 12m/1s, 9m/1s` 均带 `flags=0x01`，固件正确隐藏，UI 从 `6m/111s` 开始。
- `ceiling=5.94m` 时 `15m/1s, 12m/1s` 变为 `flags=0x00`，UI 首站跳到 `15m/1s`。
- `ceiling=8.56m` 后 `18m/1s kind=ROUTE_WAYPOINT flags=0x00`，UI 首站跳到 `18m/1s`。

需要算法侧确认：

- `ROUTE_WAYPOINT + 1s + flags=0x00` 是否确实代表“应该给用户显示的短首停”。
- 如果这些点只是 raw 路径 waypoint，是否应保持 `DISPLAY_SUPPRESSED`，避免主屏/PLAN/DLF 显示 `15m/1s`、`18m/1s`。
- Safety stop 到 mandatory deco 切换时，TTS 从 `446s` 降到 `284s` 是否符合预期。
- `GF99` 在 40m 停底全程为 `0.0%` 是否符合当前 runtime metrics 的定义。

## 9. 固件侧当前结论

- 固件没有再使用旧的“最多隐藏几个 1s / leading waypoint”启发式。
- 固件过滤只看 `AREX_DECO_STOP_FLAG_DISPLAY_SUPPRESSED`。
- 本日志中所有 `flags=0x01` 的 raw stop 都被正确隐藏。
- 所有 `flags=0x00` 的 raw stop 都会按协议进入 UI display schedule。
- 因此，当前用户可见 `1s` 首站不是固件过滤漏掉，而是 core 没有给该 stop 设置 `DISPLAY_SUPPRESSED`。

