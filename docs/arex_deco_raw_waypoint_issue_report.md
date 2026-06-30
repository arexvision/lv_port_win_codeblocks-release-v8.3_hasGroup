# AREX 减压站 raw waypoint 突变问题记录

## 结论摘要

当前日志显示，UI 上看到的 `9m` 停留时间突变，并不是 UI 自己把同一个 stop 算短了，而是算法 core 在相邻两次 plan 中返回了不同形态的 raw schedule。

前一帧 raw plan 首站是有效停留：

| 深度 | raw duration | UI 展示 |
|---|---:|---:|
| 9m | 122s | 122s |
| 6m | 64s | 64s |
| 3m | 161s | 161s |

下一帧 raw plan 变成：

| 深度 | raw duration | UI 当前过滤结果 |
|---|---:|---:|
| 15m | 1s | 被 leading waypoint filter 隐藏 |
| 12m | 1s | 被 leading waypoint filter 隐藏 |
| 9m | 1s | 漏出，被 UI 当作首站展示 |
| 6m | 111s | 展示 |
| 3m | 206s | 展示 |

因此用户可见的异常是：首站看起来从 `9m/122s` 突然跳成 `9m/1s`。从日志看，`9m/1s` 是 core raw schedule 真实返回的站点，不是 UI 计算出来的。

## 固件 runtime/UI 侧边界

当前固件 runtime 已经不是裸显 raw schedule，会做用户可见层过滤：

| 过滤项 | 当前行为 |
|---|---|
| 纯切气站 | `duration_seconds > 0 && hold_seconds == 0 && switch_penalty_seconds > 0` 时不展示 |
| runtime 展示时长 | 使用 `hold_seconds` |
| leading short waypoint | 只过滤 raw schedule 开头的短 waypoint |
| leading waypoint 数量上限 | 当前最多隐藏 2 个 |
| TTS | 始终使用 raw `schedule.tts_seconds`，不按过滤后列表重算 |

这次 UI 暴露 `9m/1s` 的直接原因是 runtime 侧 `leading waypoint` 最多隐藏 2 个，但日志里出现了连续 3 个 leading 1s：`15m/1s, 12m/1s, 9m/1s`。前两个被隐藏，第三个漏出。

不过，这个 runtime 过滤不足只是“用户可见假 stop”的触发点；更上游的现象是 core raw schedule 里 1s waypoint 明显变多，并且会在相邻 planner 输出中改变 stop list 形态。

## 关键日志 1：9m/122s 到 9m/1s 的相邻帧跳变

日志来源：

```text
C:\Users\admin\.codex\attachments\57170364-242c-4a40-8695-a2dd56144332\pasted-text.txt
```

### 前一帧：raw 首站为 9m/122s

```text
[AREX_CALL] tick depth=39.0m step=OK(0) step_gas=0(slot0 AIR) plan=OK(0) plan_active=0(slot0 AIR) ndl=0s/0min ceiling=4.67m stops=3 tts=607s cv=0
[AREX_PLAN] depth=39.0m ceiling=4.67m cv=0 tts=607s stops=3 | #1 9.00m dur=122s hold=122s sw=0s gas=0(slot0 AIR) gf=0.43 | #2 6.00m dur=64s hold=64s sw=0s gas=0(slot0 AIR) gf=0.57 | #3 3.00m dur=161s hold=161s sw=0s gas=0(slot0 AIR) gf=0.70
[AREX_UI_PLAN] tts_raw=607s raw_stops=3 | #1 raw#1 9.00m 122s/3min gas=0(slot0 AIR) gf=0.43 | #2 raw#2 6.00m 64s/2min gas=0(slot0 AIR) gf=0.57 | #3 raw#3 3.00m 161s/3min gas=0(slot0 AIR) gf=0.70 | display=3 hidden_leading=0 hidden_switch=0 hidden_zero=0
```

### 下一帧：raw 出现 15m/1s、12m/1s、9m/1s

```text
[AREX_CALL] tick depth=39.0m step=OK(0) step_gas=0(slot0 AIR) plan=OK(0) plan_active=0(slot0 AIR) ndl=0s/0min ceiling=4.76m stops=5 tts=580s cv=0
[AREX_PLAN] depth=39.0m ceiling=4.76m cv=0 tts=580s stops=5 | #1 15.00m dur=1s hold=1s sw=0s gas=0(slot0 AIR) gf=0.38 | #2 12.00m dur=1s hold=1s sw=0s gas=0(slot0 AIR) gf=0.46 | #3 9.00m dur=1s hold=1s sw=0s gas=0(slot0 AIR) gf=0.54 | #4 6.00m dur=111s hold=111s sw=0s gas=0(slot0 AIR) gf=0.62 | #5 3.00m dur=206s hold=206s sw=0s gas=0(slot0 AIR) gf=0.70
[AREX_UI_PLAN] tts_raw=580s raw_stops=5 | #1 raw#3 9.00m 1s/1min gas=0(slot0 AIR) gf=0.54 | #2 raw#4 6.00m 111s/2min gas=0(slot0 AIR) gf=0.62 | #3 raw#5 3.00m 206s/4min gas=0(slot0 AIR) gf=0.70 | display=3 hidden_leading=2 hidden_switch=0 hidden_zero=0
```

### 这里的异常点

| 字段 | 前一帧 | 下一帧 |
|---|---:|---:|
| depth | 39.0m | 39.0m |
| ceiling | 4.67m | 4.76m |
| raw stop count | 3 | 5 |
| raw TTS | 607s | 580s |
| raw 首站 | 9m/122s | 15m/1s |
| UI 首站 | 9m/122s | 9m/1s |
| hidden leading | 0 | 2 |

重点：深度和 ceiling 变化都很小，但 raw stop list 形态发生了明显跳变，并且 TTS 从 607s 降到 580s。

## 关键日志 2：进入减压早期已有大量 1s waypoint

同一份日志中，进入减压初期就反复出现 1s waypoint：

```text
[AREX_PLAN] depth=39.0m ceiling=1.34m cv=0 tts=298s stops=3 | #1 9.00m dur=1s hold=1s sw=0s gas=0(slot0 AIR) gf=0.43 | #2 6.00m dur=1s hold=1s sw=0s gas=0(slot0 AIR) gf=0.57 | #3 3.00m dur=36s hold=36s sw=0s gas=0(slot0 AIR) gf=0.70
[AREX_UI_PLAN] tts_raw=298s raw_stops=3 | #1 raw#3 3.00m 36s/1min gas=0(slot0 AIR) gf=0.70 | display=1 hidden_leading=2 hidden_switch=0 hidden_zero=0

[AREX_PLAN] depth=39.0m ceiling=2.90m cv=0 tts=387s stops=4 | #1 12.00m dur=1s hold=1s sw=0s gas=0(slot0 AIR) gf=0.40 | #2 9.00m dur=1s hold=1s sw=0s gas=0(slot0 AIR) gf=0.50 | #3 6.00m dur=13s hold=13s sw=0s gas=0(slot0 AIR) gf=0.60 | #4 3.00m dur=112s hold=112s sw=0s gas=0(slot0 AIR) gf=0.70
[AREX_UI_PLAN] tts_raw=387s raw_stops=4 | #1 raw#3 6.00m 13s/1min gas=0(slot0 AIR) gf=0.60 | #2 raw#4 3.00m 112s/2min gas=0(slot0 AIR) gf=0.70 | display=2 hidden_leading=2 hidden_switch=0 hidden_zero=0

[AREX_PLAN] depth=39.0m ceiling=3.56m cv=0 tts=437s stops=4 | #1 12.00m dur=1s hold=1s sw=0s gas=0(slot0 AIR) gf=0.40 | #2 9.00m dur=1s hold=1s sw=0s gas=0(slot0 AIR) gf=0.50 | #3 6.00m dur=55s hold=55s sw=0s gas=0(slot0 AIR) gf=0.60 | #4 3.00m dur=120s hold=120s sw=0s gas=0(slot0 AIR) gf=0.70
[AREX_UI_PLAN] tts_raw=437s raw_stops=4 | #1 raw#3 6.00m 55s/1min gas=0(slot0 AIR) gf=0.60 | #2 raw#4 3.00m 120s/2min gas=0(slot0 AIR) gf=0.70 | display=2 hidden_leading=2 hidden_switch=0 hidden_zero=0
```

这些 1s waypoint 当前被 runtime 过滤掉，所以未必都能被用户看到。但它们说明 raw schedule 本身已经存在多个非常短的中间点。

## 关键日志 3：另一份日志也能复现高频 1s waypoint

日志来源：

```text
C:\Users\admin\.codex\attachments\e1837917-faec-4608-92dd-48af2bfaec14\pasted-text.txt
```

摘录：

```text
[AREX_PLAN] depth=40.0m ceiling=1.64m cv=0 tts=325s stops=3 | #1 9.00m dur=1s hold=1s sw=0s gas=0(slot0 AIR) gf=0.43 | #2 6.00m dur=1s hold=1s sw=0s gas=0(slot0 AIR) gf=0.57 | #3 3.00m dur=56s hold=56s sw=0s gas=0(slot0 AIR) gf=0.70
[AREX_UI_PLAN] tts_raw=325s raw_stops=3 | #1 raw#3 3.00m 56s/1min gas=0(slot0 AIR) gf=0.70 | display=1 hidden_leading=2 hidden_switch=0 hidden_zero=0

[AREX_PLAN] depth=40.0m ceiling=2.95m cv=0 tts=399s stops=4 | #1 12.00m dur=1s hold=1s sw=0s gas=0(slot0 AIR) gf=0.40 | #2 9.00m dur=1s hold=1s sw=0s gas=0(slot0 AIR) gf=0.50 | #3 6.00m dur=17s hold=17s sw=0s gas=0(slot0 AIR) gf=0.60 | #4 3.00m dur=113s hold=113s sw=0s gas=0(slot0 AIR) gf=0.70
[AREX_UI_PLAN] tts_raw=399s raw_stops=4 | #1 raw#3 6.00m 17s/1min gas=0(slot0 AIR) gf=0.60 | #2 raw#4 3.00m 113s/2min gas=0(slot0 AIR) gf=0.70 | display=2 hidden_leading=2 hidden_switch=0 hidden_zero=0

[AREX_PLAN] depth=40.0m ceiling=4.96m cv=0 tts=600s stops=5 | #1 15.00m dur=1s hold=1s sw=0s gas=0(slot0 AIR) gf=0.38 | #2 12.00m dur=1s hold=1s sw=0s gas=0(slot0 AIR) gf=0.46 | #3 9.00m dur=9s hold=9s sw=0s gas=0(slot0 AIR) gf=0.54 | #4 6.00m dur=113s hold=113s sw=0s gas=0(slot0 AIR) gf=0.62 | #5 3.00m dur=209s hold=209s sw=0s gas=0(slot0 AIR) gf=0.70
[AREX_UI_PLAN] tts_raw=600s raw_stops=5 | #1 raw#3 9.00m 9s/1min gas=0(slot0 AIR) gf=0.54 | #2 raw#4 6.00m 113s/2min gas=0(slot0 AIR) gf=0.62 | #3 raw#5 3.00m 209s/4min gas=0(slot0 AIR) gf=0.70 | display=3 hidden_leading=2 hidden_switch=0 hidden_zero=0
```

统计结果：

| 日志文件 | `dur=1s` 出现次数 |
|---|---:|
| `e1837917.../pasted-text.txt` | 30 |
| `57170364.../pasted-text.txt` | 38 |

注：这里统计的是 `dur=1s` 的出现次数，不是包含 `dur=1s` 的行数。单行 raw plan 里可能有多个 1s stop。

## 问题描述，建议发给算法工程师

我们在新版本 AREX 算法接入后观察到 raw schedule 中 `1s` 减压站/waypoint 明显增多，且这些 1s waypoint 会出现在多个深度层级，例如 `9m/1s`、`12m/1s`、`15m/1s`、`18m/1s`。

目前 firmware runtime 会把 raw schedule 解释成用户可见 stop，并且会过滤开头的短 waypoint。但日志里出现了连续三个 leading 1s waypoint：`15m/1s, 12m/1s, 9m/1s`。固件当前只隐藏前两个，导致第三个 `9m/1s` 漏到 UI，形成用户看到的异常：上一帧显示 `9m/122s`，下一帧显示 `9m/1s`。

从 raw 日志看，这个 `9m/1s` 是 core 返回的真实 raw stop，不是 UI 自己生成或计算出来的。固件侧可以继续增强 display filter，避免把这类 waypoint 暴露给用户；但需要算法侧确认这些 1s waypoint 是否符合新版本 planner 预期，尤其是下面几个现象：

| 待确认点 | 说明 |
|---|---|
| 1s waypoint 数量是否符合预期 | 两份日志中 `dur=1s` 出现 30 次和 38 次，感觉明显多于以前版本 |
| 相邻帧 stop list 形态跳变是否符合预期 | `9/122,6/64,3/161` 下一帧变成 `15/1,12/1,9/1,6/111,3/206` |
| TTS 跳变是否符合预期 | 上述相邻帧 raw TTS 从 607s 降到 580s |
| GF 分布是否符合预期 | 同样 9m 站，上一帧 gf=0.43，下一帧 9m/1s gf=0.54 |
| 这些 1s stop 是 raw path waypoint 还是用户有效停留站 | 如果只是 planner 过渡点，建议算法文档明确 raw/display 语义边界 |

## 固件侧后续处理建议

固件 runtime/display 层建议继续把 raw schedule 和用户可见 schedule 分开：

| 层级 | 建议 |
|---|---|
| core raw schedule | 保留完整 raw route，不删除 waypoint，不影响 TTS |
| runtime display schedule | 隐藏所有连续 leading 1s waypoint，只要后面还有有效展示 stop |
| PLAN 图 | 使用 runtime display schedule，避免和主屏 current stop 不一致 |
| debug 日志 | 同时输出 raw plan 和 UI filtered plan，方便定位 |

这只能解决“用户看到假 stop”的问题；raw schedule 中 1s waypoint 变多、相邻帧 stop list 形态跳变的问题，仍建议算法侧一起确认。
