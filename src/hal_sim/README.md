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
- `TCP_ALGO_DEBUG=1` 时，模拟 tick 在 TCP client 连接前不推进数据；连接成功后触发一次调试重置，再用 TCP 深度驱动 Buhlmann 调试算法。
- TCP 指令支持 `back` 模拟返回键，`speed <1..120>` 设置算法调试倍速。
- `TCP_ALGO_DEBUG=0` 时，不启动 TCP 算法调试，恢复原版自动深度脚本和假数据模拟。
- TCP 算法调试期间 1Hz 时间推进和 PC 侧简单速率采样仍继续运行。
- 真机移植时，这里的文件通常由硬件输入、传感器任务、算法任务或 BLE 任务替换。
