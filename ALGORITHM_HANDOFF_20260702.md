# AREX Deco Core Algorithm Handoff - 2026-07-02

本文是本轮 planner/runtime current stop 收敛后的嵌入式交付说明。目标是给今天下水测试使用：
明确当前算法行为、已修复问题、与 Shearwater/Garmin 的已知差异，以及测试时必须记录的字段。

## 交付结论

本轮算法版本可以封板交给嵌入式做下水测试。

本轮没有修改 tissue 方程、ZHL-16C 系数、GF 数学、安全边界、public API 字段或 ABI
布局。为便于固件识别本次算法修复版，API patch 版本推进到 `0.0.29`。核心变化是：

- runtime current stop 已收敛为轻量 mandatory projection，不再让 waypoint/suppressed/1s route node 抢主屏。
- planner 的 GF anchor update 已改为使用与主 planner 同构的路径语义：
  先模拟上升到 first stop，再逐站 solve hold，再逐站上升。
- 修复前，anchor-update 可能用静态 tissue 扫描得到一个与主 planner 路径不一致的 `first_real_stop_m`。
  修复后，anchor 的 first-real 判断与主 planner 的 first mandatory stop 一致。

## 当前版本行为

测试配置：

- Gas: Air
- GF: 40/85
- Safety stop: off
- `deco_step_m = 3m`
- `last_stop_m = 3m`
- runtime selector: stateless mandatory projection + 极薄 hysteresis

### 39.8m replay

`18m/min` 逐秒下潜到 `39.8m` 后 hold：

```text
首个 runtime stop: 3m/1s at 543s
首段 3m: 1s -> 48s
随后切到: 6m/50s
秒级深度序列: 3, 6, 3, 6, 9, 6, 9, 12, 15
```

### 40.8m replay

`18m/min` 逐秒下潜到 `40.8m` 后 hold：

```text
首个 runtime stop: 3m/1s at 521s
首段 3m: 1s -> 48s
随后切到: 6m/50s
秒级深度序列: 3, 6, 3, 6, 9, 6, 9, 12, 15, 18
```

`instant` 到 `40.8m` 后 hold：

```text
NDL/ceiling transition: about 400s
首个 runtime stop: 3m/1s at 453s
首段 3m: 1s -> 48s
随后切到: 6m/50s
```

## Garmin/Shearwater 对齐状态

### 已对齐的现象

Garmin 40.8m 加压观察：

```text
约 6:18 后 NDL 归零
出现 3m stop
3m 停留增长到 48s 后变为 6m
```

AREX 当前也复现了这个关键细节：

```text
3m 从 1s 增长到 48s
下一帧切到 6m/约50s
```

因此，`3m -> 6m at 48s` 不是 planner bug，而是可解释的 current stop transition。

### 仍需下水验证的差异

Garmin/Shearwater 看起来有一种 display time 语义：

```text
每个新的 displayed stop 出现时，时间几乎从 0 开始增长
```

AREX 当前 runtime stop 暴露的是 planner obligation：

```text
新 displayed stop 出现时，remaining_seconds 可能已经是 30s / 50s / 90s / 100s
```

这说明后续若要进一步贴近 Shearwater/Garmin，可能需要在 display 层增加
`display_stop_age` 或 `display_counter` 语义，而不是修改 planner mandatory hold。

当前不可把“新站不是从 0 开始”判断为减压数学错误。

## Safety 边界说明

runtime current stop 不是 safety proof layer。

安全边界仍然来自：

- tissue 重算；
- GF ceiling；
- hard ceiling；
- raw schedule；
- ceiling violation；
- 实际深度与上升控制。

runtime current stop 只是从 raw schedule 中投影出当前主屏可显示的 first meaningful mandatory obligation。

## 嵌入式接入注意事项

1. 必须保存完整 `ArexDecoDiveState`，包括：
   - `gf_anchor_valid`
   - `gf_anchor_depth_m`
   - tissue state
   - elapsed/depth-time/max-depth

2. 每个采样 tick 推荐顺序：

```text
arex_deco_step()
arex_deco_plan()
arex_deco_select_runtime_stop()
```

3. `ArexDecoSchedule.stops[]` 是 raw route nodes，不是 UI current stop list。

4. UI 主屏 current stop 应使用 runtime selector 输出，不要直接使用 raw schedule 第一项。

5. `ROUTE_WAYPOINT`、`DISPLAY_SUPPRESSED`、pure gas switch 不应抢主屏。

## 下水测试必须记录

建议每 1s 或设备可承受的最高频率记录：

- elapsed time
- depth
- active gas
- NDL
- hard ceiling
- runtime current stop depth
- runtime current stop remaining seconds
- TTS
- surface GF
- GF99
- ceiling violation
- raw first 3 stops: depth/time/kind/flags
- `gf_anchor_valid`
- `gf_anchor_depth_m`

与 Shearwater/Garmin 人眼观察至少记录：

- NDL 归零时间；
- 第一次出现 deco stop 的时间、深度、显示时间；
- 每次 current stop depth 变化的时间；
- 每次新 stop 出现时，显示时间是否从 0/1s 开始；
- `3m -> 6m`、`6m -> 9m`、`9m -> 12m` 的切换阈值；
- 到达 15m/18m/21m current stop 的时间；
- 开始上升前的最后 stop、TTS、SurfGF。

## 验证状态

本轮回归结果：

```text
arex_deco_core_tests: 184/184 passed
case_001_air_45m_single_gas: assertions=20 failed=0 raw_failed=0 display_failed=0
```

## 当前最终判断

本版已经解决：

- waypoint/suppressed/1s route stop 抢主屏；
- 强 debounce 状态机导致的长期 hold previous；
- anchor-update 使用与主 planner 不一致路径推导 first-real stop；
- “首站稳定偏深到 6m”的当前重建 profile 问题。

当前仍未解决、需要下水验证：

- Shearwater/Garmin 的 display time 是否是 stop age/display counter，而非 planner remaining obligation；
- 后段 `12/15/18/21m` 推进节奏是否仍与 Shearwater 存在稳定偏差；
- 真实 pressure log 与手动加压 replay 的差异。

本轮建议封板进入下水测试，不再继续为台架目测结果调整 planner 数学。
