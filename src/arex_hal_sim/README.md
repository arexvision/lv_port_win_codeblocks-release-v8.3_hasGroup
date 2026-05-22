# arex_hal_sim

`arex_hal_sim` 是 Windows PC 模拟器专用适配层。它让 AREX UI 在没有真实传感器和按键硬件时也能运行。

## 文件职责

| 文件 | 作用 |
|---|---|
| `input_pc.h/c` | 把 PC 键盘和模拟 encoder 事件转成 `ui_handle_rotate/click/back()`。 |
| `sim_data.h/c` | 创建模拟 tick，生成深度脚本，并把深度/温度/tick 输入到 PC 调试算法层。 |
| `debug_link_pc.h` | TCP 调试链路，供外部调试脚本控制模拟器数据和读取状态。 |

## 与 UI 的边界

- 输入层调用 UI 状态机，不直接操作具体卡片对象。
- 模拟数据层应尽量通过 `arex_bus_set_*()` 写数据，模拟真实硬件/算法任务接入方式。
- 当前 PC 调试工程通过 `arex_algo_sim/buhlmann_debug_tick()` 接入 Buhlmann 调试算法，后续真机工程可以替换为主工程算法任务。
- TCP client 连接成功后会触发一次调试重置：清空传感器数据、轨迹和算法状态，然后停用自动深度脚本，改用 TCP 深度驱动 Buhlmann 调试算法。
- TCP 调试期间 1Hz 时间推进和 PC 侧简单速率采样仍继续运行。
- 真机移植时，这里的文件通常由硬件输入、传感器任务、算法任务或 BLE 任务替换。
