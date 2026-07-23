# TCP 调试基础使用说明

本文档用于 PC 模拟器录屏、算法调试和基础 UI 操作。TCP 调试端口为 `127.0.0.1:7623`，每条命令单独发送一行。

## 连接方式

启动 `LittlevGL.exe` 后，用 TCP 工具连接：

```text
127.0.0.1:7623
```

连接成功后可先输入：

```text
help
state
```

`help` 查看所有指令，`state` 查看当前 UI 状态、深度、速度、指南针、电量、温度等调试值。

## 基本操作

| 指令 | 作用 | 示例 |
|---|---|---|
| `click` | 模拟确认键 / encoder click | `click` |
| `rotate [steps]` | 模拟旋钮旋转，正数向下/向右，负数反向 | `rotate 1`、`rotate -1` |
| `back` | 模拟返回键 | `back` |
| `back 2` | 模拟长按返回 2 秒，常用于告警确认/退出 | `back 2` |
| `pause on` | 暂停 PC 模拟数据推进，UI 和 TCP 仍可操作 | `pause on` |
| `pause off` | 恢复模拟数据推进 | `pause off` |
| `pause toggle` | 暂停/恢复切换 | `pause toggle` |
| `manual on` | 进入手动/TCP 驱动模式 | `manual on` |
| `auto on` | 退出手动模式，恢复自动模拟 | `auto on` |

说明：TCP client 连接后，模拟器会进入调试数据驱动环境，原自动深度脚本不会继续乱跑。

## 深度与速度

| 指令 | 作用 | 示例 |
|---|---|---|
| `<number>` | 直接写入深度，单位 m | `13` |
| `depth <m>` | 直接写入深度，单位 m | `depth 13.0` |
| `goto <m>` | 自动移动到目标深度；默认下潜 18m/min，上升 10m/min | `goto 30` |
| `goto <m> <m/min>` | 按指定速度移动到目标深度 | `goto 13 6` |
| `goto stop` | 停止自动移动 | `goto stop` |
| `rate <m/min>` | 手动设置上升/下降速度显示 | `rate -9` |
| `sample <time_s> <depth_m>` | 添加指定时间点的潜水轨迹采样 | `sample 120 18.5` |
| `time <seconds>` | 直接设置 dive time | `time 1684` |
| `speed <1..120>` | 设置算法调试倍速 | `speed 1`、`speed 40` |

`speed` 只加速深度、潜水时间和算法 tick，不加速指南针独立自动旋转。  
例如 `speed 40` 表示 1 秒内推进约 40 秒的算法状态，适合压力测试，不适合录制正常 UI 视频。

常用场景：

```text
speed 1
goto 13
```

```text
speed 40
goto 45
```

第二组会快速压算法和 DECO 刷新，CPU 压力会明显更大。

## 随机深度扰动

| 指令 | 作用 | 示例 |
|---|---|---|
| `glitch on` | 开启默认随机深度扰动 | `glitch on` |
| `glitch <min> <max>` | 在指定深度范围内随机跳动 | `glitch 8.2 8.5` |
| `glitch <min> <max> <spike> <speed>` | 指定范围、尖峰幅度和倍速 | `glitch 0 45 30 20` |
| `glitch off` | 停止随机扰动 | `glitch off` |

录屏时如果想稳定一点，建议用 `goto` 或 `video` 脚本，不建议用 `glitch`。

## 温度、电量与气压

| 指令 | 作用 | 示例 |
|---|---|---|
| `batt <pct>` | 固定电量百分比 | `batt 83` |
| `batt auto` | 恢复默认电量模拟 | `batt auto` |
| `temp <c>` | 固定水温/环境温度，单位摄氏度 | `temp 19.3` |
| `temp auto` | 恢复默认水温模拟 | `temp auto` |
| `bat_temp <c>` | 固定电池温度 | `bat_temp 22.0` |
| `bat_temp auto` | 恢复默认电池温度模拟 | `bat_temp auto` |
| `prj_temp <c>` | 固定光机/投影温度 | `prj_temp 24.0` |
| `prj_temp auto` | 恢复默认光机温度模拟 | `prj_temp auto` |
| `pod <0|1> <bar>` | 设置气瓶压力 | `pod 0 200` |

这些 override 在当前 TCP 连接内会持续生效；发 `auto/default` 或断开 TCP 后恢复默认模拟值。

## 指南针

| 指令 | 作用 | 示例 |
|---|---|---|
| `heading <deg>` | 设置指南针角度 | `heading 271` |
| `heading <deg> <pitch> <roll>` | 同时设置 heading、pitch、roll | `heading 271 2 -1` |
| `heading_speed <dps>` | 设置自动旋转速度，单位 deg/s | `heading_speed 10` |
| `heading_speed 0` | 停止自动旋转 | `heading_speed 0` |
| `compass on` | 设置指南针可用 | `compass on` |
| `compass off` | 设置指南针不可用 | `compass off` |
| `compass <deg>` | 设置指南针可用并写入角度 | `compass 280` |
| `attitude <pitch> <roll> [heading]` | 设置姿态 | `attitude 3 -2 275` |

录屏时如果要稳定控制角度，建议先：

```text
heading_speed 0
heading 271
```

## 录屏脚本

| 指令 | 作用 |
|---|---|
| `video 1` | 15 秒脚本，深度 8.3m -> 8.5m -> 8.2m，NDL 固定 70 |
| `video 2` | 15 秒脚本，深度固定 27m，dive time 从 28:04 开始递增，指南针 271° 缓慢偏到 260°，DECO/NDL 由算法计算 |
| `video stop` | 停止录屏脚本 |
| `demo 1` / `demo 2` | `video` 的别名 |

推荐录屏前先设置：

```text
speed 1
pause off
heading_speed 0
```

然后再发：

```text
video 1
```

或：

```text
video 2
```

## DECO 与算法显示

| 指令 | 作用 | 示例 |
|---|---|---|
| `ndl <min>` | 强制设置 NDL 分钟数 | `ndl 70` |
| `tts <min>` | 强制设置 TTS 分钟数 | `tts 12` |
| `stop none ...` | 设置无停留状态 | `stop none 70 0 0 0 zone0` |
| `stop safety <ndl> <depth> <total_s> <left_s> <zone0|1>` | 设置安全停留 | `stop safety 30 5 180 120 zone1` |
| `stop deco <ndl> <depth> <total_s> <left_s> <zone0|1>` | 设置减压停留 | `stop deco 0 6 300 180 zone1` |
| `gf <low> <high>` | 设置 GF low/high | `gf 30 70` |
| `gf99 <pct>` | 设置 GF99 | `gf99 42` |
| `surf_gf <pct>` | 设置 SurfGF | `surf_gf 85` |

注意：如果目标是看真实算法输出，不要手动发 `ndl/tts/stop/gf99` 这类强制显示命令，直接用 `depth/goto/video 2` 让算法自己算。

## 常用组合

### 固定 27m 正常算法录屏

```text
speed 1
heading_speed 0
batt 83
temp 19.3
video 2
```

### 快速进入 45m 压力测试

```text
speed 40
goto 45
state
```

### 暂停画面后微调数据

```text
pause on
batt 83
temp 19.3
heading 271
pause off
```

## 使用建议

- 录屏用 `speed 1`，压力测试才用 `speed 20/40`。
- 想看算法真实结果时，只控制深度和时间，不要再强制写 `ndl/tts/stop`。
- 想看 UI 极端状态时，才用 `ndl/tts/stop/glitch` 这类强制命令。
- 修改多个数据后可以发 `state` 确认当前值。
