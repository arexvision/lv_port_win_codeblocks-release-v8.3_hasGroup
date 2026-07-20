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
- TCP 指令支持 `page <info|compass|deco|plan|gas|custom|blank|setup|menu>` 直接跳到指定右侧页面，便于查看新 UI 分支里的卡片。
- TCP 指令支持 `heading_speed <0..3600>` 设置指南针自动旋转速度，单位为度/秒；指南针用独立 10ms 定时器推进，`heading_speed 10` 会在 1 秒内连续显示约 10 次 1° 变化，`heading_speed 0` 暂停自动旋转。
- TCP 指令支持 `heading <deg> [on|off]`、`heading off`、`compass_lock <deg|current|off>` 和罗盘校准后端模拟。菜单里点 `RESET AUTO CAL` 后会进入 `LEARN`，再用 `compass_cal success [duration_ms] [hint]` 模拟 0~100 成功，用 `compass_cal fail [duration_ms] [hint]` 模拟 0~50 后显示 `FAIL`；固定帧调试可用 `compass_cal set <idle|run|saving|verify|ready|save_error|error> [progress] [bins] [hint]`。
- TCP 指令支持 `ota off` 或 `ota <wait|prepare|recv|verify|install|reboot|error> [progress] [detail] [reason]`。当前 CodeBlocks 工程若未链接 `ota_update_view.c`，指令会返回 `ERR ota view not linked in this build`。
- TCP 指令支持 `goto <depth_m> [m/min]` 自动移动到目标深度；未给速度时下潜 18m/min、上升 10m/min，给速度时上下行都按指定 m/min，`goto stop` 可取消，倍速模式按模拟时间同步加速。
- `TCP_ALGO_DEBUG=0` 时，不启动 TCP 算法调试，恢复原版自动深度脚本和假数据模拟。
- TCP 算法调试期间 1Hz 时间推进和 PC 侧简单速率采样仍继续运行。
- 真机移植时，这里的文件通常由硬件输入、传感器任务、算法任务或 BLE 任务替换。
