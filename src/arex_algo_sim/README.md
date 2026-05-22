# arex_algo_sim

`arex_algo_sim` 是 PC 调试用算法接入层。

## 文件职责

| 文件 | 作用 |
|---|---|
| `Buhlmann.h/cpp` | 从参考工程搬来的 Buhlmann/ZHL-16C-GF 算法核心，尽量保持原算法结构。 |
| `buhlmann_debug.h/cpp` | PC 调试适配层，把算法输出同步到 AREX UI data bus，并提供 TCP 调试重置入口。 |
| `rtthread.h` / `rtdevice.h` | PC 编译兼容占位，提供算法核心需要的最小 RT-Thread API。 |

## 边界

- 这里不直接操作 LVGL 对象。
- 对 UI 的输出统一走 `arex_bus_set_*()` / `arex_bus_update_deco()`。
- 真机移植时，应由真实算法任务替换 `buhlmann_debug_tick()` 的 PC tick 驱动。
