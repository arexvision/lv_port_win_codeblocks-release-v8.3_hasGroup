# algo_sim

`algo_sim` 是 PC 调试用算法接入层。

## 文件职责

| 文件 | 作用 |
|---|---|
| `deco_core.h/cpp` | PC 调试适配层，链接 Arex Deco Core 静态库并把算法输出同步到 UI data bus。 |
| `rtthread.h` / `rtdevice.h` | PC 编译兼容占位，提供算法核心需要的最小 RT-Thread API。 |

## 边界

- 这里不直接操作 LVGL 对象。
- 对 UI 的输出统一走 `bus_set_*()` / `bus_update_deco()`。
- 真机移植时，应由真实算法任务替换 `deco_core_tick()` 的 PC tick 驱动。
