# hal_sim

`hal_sim` 是 Windows PC 模拟器专用适配层。它让 UI 在没有真实传感器和按键硬件时也能运行。

## 文件职责

| 文件 | 作用 |
|---|---|
| `input_pc.h/c` | 把 PC 键盘和模拟 encoder 事件转成 `ui_handle_rotate/click/back()`。 |
| `sim_data.h/c` | 创建模拟 tick；可通过 `TCP_ALGO_DEBUG` 在原版自动模拟和 TCP 算法调试之间切换。 |
| `debug_link_pc.h` | TCP 调试链路，供外部调试脚本控制模拟器数据和读取状态。 |

## 与 UI 的边界

- 输入层调用 UI 状态机，不直接操作具体卡片对象。
- 模拟数据层应尽量通过 `bus_set_*()` 写数据，模拟真实硬件/算法任务接入方式。
- `TCP_ALGO_DEBUG=1` 时，模拟 tick 在 TCP client 连接前不推进数据；连接成功后触发一次调试重置，再用 TCP 深度驱动 Arex 减压算法。
- TCP 指令支持 `back` 模拟返回键，`speed <1..120>` 设置算法调试倍速；该倍速只加速深度、潜水时间和算法 tick，不加速指南针自动转动。
- TCP 指令支持 `pause on|off|toggle` 临时冻结 PC 模拟推进（深度、潜水时间、goto/glitch、自动指南针）；LVGL 和 TCP 仍继续运行，方便继续发指令恢复。
- TCP 指令支持 `batt <pct|auto>`、`temp <c|auto>`、`bat_temp <c|auto>`、`prj_temp <c|auto>` 覆盖电量、水温、电池温度和光机温度；覆盖值在当前 TCP 连接内每秒持续写入，发 `auto` 或断开连接后恢复默认模拟值。别名支持 `battery`/`battery_pct`、`temperature`、`battery_temp`、`projector_temp`。
- TCP 指令支持 `video <1|2|3|4|stop>` / `demo <1|2|3|4|stop>` 启动 15 秒录屏脚本；当前 `video 1` 模拟 8.3m→8.5m→8.2m 和 NDL 70，`video 2` 固定 27m、潜水时间 28:04 起每秒递增、指南针 271° 缓慢偏到 260°，`3/4` 预留给后续曲线。
- TCP 指令支持 `orphan <submenu|edit|modal|confirm|menu|turn_off> [tile_pos]` 制造 DASH 孤儿态；随后发一次 `click` 或 `rotate` 记录候选，等待 1 秒以上后再发一次 `click` 或 `rotate`，可验证 UI 状态机延迟自恢复。
- TCP 指令支持 `heading_speed <0..3600>` 设置指南针自动旋转速度，单位为度/秒；指南针用独立 10ms 定时器推进，`heading_speed 10` 会在 1 秒内连续显示约 10 次 1° 变化，`heading_speed 0` 暂停自动旋转。
- 指南针 TCP 调试同步写入 UI bus 的 heading、指南针原始磁场和姿态数据：`heading <deg> [pitch roll]` / `compass on|off|<deg> [pitch roll]` / `mag <x> <y> <z>` / `attitude <pitch> <roll> [heading]`；`mlx <x> <y> <z>` 和 `tmag <x> <y> <z>` 只更新对应诊断字段，不直接改变 heading。
- 指南针校准 UI 可用 `compass_cal <state> [progress] [hint] [coverage_mask] [bins]` 调试，`state` 支持 `idle|running|saving|verifying|ready|save_error|error`，`coverage_mask` 支持十进制或 `0x` 十六进制。
- TCP 指令支持 `goto <depth_m> [m/min]` 自动移动到目标深度；未给速度时下潜 18m/min、上升 10m/min，给速度时上下行都按指定 m/min，`goto stop` 可取消，倍速模式按模拟时间同步加速。
- `TCP_ALGO_DEBUG=0` 时，不启动 TCP 算法调试，恢复原版自动深度脚本和假数据模拟。
- TCP 算法调试期间 1Hz 时间推进和 PC 侧简单速率采样仍继续运行。
- 真机移植时，这里的文件通常由硬件输入、传感器任务、算法任务或 BLE 任务替换。
