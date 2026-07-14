# DIVE PLAN TRACK 真机专项测试规范

## 1. 文档目的

本文档用于验证真机设备上的 `DIVE PLAN TRACK` 图表在长时间潜水、200 点历史轨迹、减压预测、LVGL 小 draw buffer 分块刷新等场景下的：

- 显示正确性
- 绘制性能
- 几何缓存有效性
- dirty/invalidation 刷新频率
- CPU 占用与交互响应
- LVGL heap、RT-Thread heap 和线程栈稳定性
- 长时间运行可靠性

重点验证以下提交带来的优化：

- `91eda4e 优化PLAN轨迹绘制性能`
- `0defc3a 缓存PLAN轨迹绘制几何`

当前关键上限：

| 项目 | 当前值 | 定义位置 |
|---|---:|---|
| 历史轨迹最大点数 | 200 | `src/ui/core/ui_types.h` 的 `MAX_DIVE_LOG` |
| 减压停留最大数量 | 10 | `src/ui/core/ui_types.h` 的 `MAX_DECO_STOPS` |
| 默认轨迹采样周期 | 2 秒 | `src/ui/core/ui_settings.h` 的 `UI_LOG_RATE_DEFAULT_S` |
| UI dirty 消费周期 | 100 ms | `src/config/build/ui_build_flags.h` 的 `APP_UI_UPDATE_TIMER_DELAY_MS` |

## 2. 测试结论必须回答的问题

每轮真机测试结束后，必须能够明确回答：

1. `card_plan_update()` 实际每秒调用多少次？
2. 每次 invalidation 会触发多少次 `plan_chart_draw_cb()`？
3. draw callback 次数增加是正常分块刷新，还是上游重复 invalidation？
4. 同一轮分块刷新中，历史轨迹几何是否只重建一次？
5. 原始 200 点经过映射、去重和共线合并后剩余多少像素点？
6. 每个 chunk 实际提交了多少条历史线段和预测虚线？
7. 单次完整 PLAN 刷新的总耗时、P95、P99 和最大值是多少？
8. CPU 100% 是 PLAN 绘图导致，还是其它任务、算法或日志打印导致？
9. 长时间运行后 LVGL heap、RT-Thread heap、PSRAM 和线程栈是否稳定？
10. 页面滚动、布局变化和 VM 更新后，缓存是否正确失效并重建？

## 3. 测试原则

### 3.1 真机数据优先

最终结论必须来自目标 MCU、目标 LCD 驱动、目标 draw buffer 大小和正式编译优化等级。PC 模拟器只能用于功能预检查，不能代替真机性能结论。

### 3.2 不在绘图回调内直接打印

禁止在 `plan_chart_draw_cb()`、`plan_chart_profile_cache_draw()` 或虚线循环内逐条调用 `rt_kprintf()`。

原因：

- UART 输出会显著放大绘图耗时
- 打印锁可能阻塞 LVGL 线程
- 每个 chunk 都打印会制造新的 CPU 100% 问题
- 日志本身会改变调度时序，使测量失真

正确做法是只累加整数计数器和 cycle 值，每 1 秒或 2 秒在回调外统一打印一次聚合数据。

### 3.3 同时记录基线

每个 PLAN 测试场景都必须记录同一潜水状态下的非 PLAN 页面基线，例如 `BLANK` 或低负载页面。不能只看 PLAN 页自身 CPU 数字。

推荐比较：

```text
PLAN_CPU_DELTA = PLAN 页面 CPU - 基线页面 CPU
```

### 3.4 区分 update、invalidate 和 draw callback

真机小 draw buffer 下，一次 `lv_obj_invalidate()` 可能产生多个 draw callback。这三个计数必须分别记录：

| 计数 | 含义 |
|---|---|
| `update_count` | `card_plan_update()` 被调用次数 |
| `invalidate_count` | 实际调用 `lv_obj_invalidate(s_chart_obj)` 次数 |
| `draw_cb_count` | `plan_chart_draw_cb()` 被 LVGL 调用次数 |

不能把 `draw_cb_count` 高直接判定为 invalidation 风暴。

## 4. 测试环境记录表

每份测试报告必须先记录以下信息：

| 项目 | 记录值 |
|---|---|
| MCU 型号 |  |
| CPU 主频 |  |
| FPU/硬浮点配置 |  |
| 编译器及版本 |  |
| Debug/Release |  |
| 优化等级 |  |
| LCD 分辨率 |  |
| 色深 |  |
| draw buffer 像素数 |  |
| draw buffer 行数 |  |
| 单/双 buffer |  |
| direct/full refresh 模式 |  |
| LCD 接口及带宽 |  |
| LVGL 刷新周期 |  |
| RT-Thread tick 频率 |  |
| UART 波特率 |  |
| 日志输出方式，同步/异步/DMA |  |
| `cpu_usage_profiler.h` 是否存在 |  |
| 固件 commit |  |
| 测试日期 |  |

如果真机构建中没有 `cpu_usage_profiler.h`，当前 `ui_monkey` 会使用返回 `0.0f` 的 fallback，所有 CPU 日志均无效。开始测试前必须确认设备端链接的是实际 CPU usage profiler。

### 4.1 实际 chunk 数

必须从 LCD flush 驱动或 `draw_ctx->clip_area` 统计出真实 chunk 数，不要按 draw buffer 比例猜测。

推荐记录：

```text
chunk_count_per_invalidate = 本次 invalidation 对应的 draw callback 次数
```

如果一轮刷新有 8 个 chunk，正常情况应接近：

```text
invalidate_count = 1
draw_cb_count = 8
cache_build_count = 1
cache_hit_count = 7
```

## 5. 建议增加的测试开关

建议在 `src/ui_test/ui_test_flags.h` 增加测试专用开关：

```c
#define UI_PLAN_PERF_TEST_ENABLED 0
#define UI_PLAN_PERF_LOG_PERIOD_MS 1000U
```

要求：

- 正式发布版本必须为 `0`
- 所有 PLAN 性能计数、测试命令和日志均放在该宏内
- 不要在共享公共头文件中定义 `PC_SIMULATOR`
- 测试数据必须通过 `bus_set_*()`、`dive_log_append()`、`dive_log_reset()` 写入
- 不允许测试代码直接修改 `g_sensor_data` 或 `g_sys_config`

## 6. 建议的真机测试命令

当前仓库已有 `ui_monkey` MSH 命令，但没有用于真机精确构造 PLAN 轨迹的 MSH 命令。建议后续在 `src/ui_test` 增加以下测试专用命令，命令名称可使用 `plan_test`：

```text
plan_test reset
plan_test load flat
plan_test load triangle
plan_test load stair
plan_test load zigzag
plan_test load compressed
plan_test load max_deco
plan_test axis <seconds>
plan_test invalidate <hz> <seconds>
plan_test perf start
plan_test perf stop
plan_test perf status
```

这些命令是本文档建议的测试接口，当前代码中尚未实现。

### 6.1 命令行为要求

| 命令 | 行为 |
|---|---|
| `reset` | 调用 `dive_log_reset()`，清空减压站，恢复正常刷新频率 |
| `load flat` | 写入 200 个恒深点 |
| `load triangle` | 写入下潜、恒深、上升完整曲线 |
| `load stair` | 写入阶梯式深度变化 |
| `load zigzag` | 写入 200 个最坏非共线点 |
| `load compressed` | 写入长时间轴上大量映射到相同像素的点 |
| `load max_deco` | 写入 10 个减压停留站 |
| `axis` | 构造指定潜水时间，用于测试刻度边界 |
| `invalidate` | 测试专用重复置脏，测量系统极限，不用于正常验收 |
| `perf start/stop/status` | 控制 PLAN 聚合统计 |

所有 MSH 参数属于外部输入，必须做范围检查。

## 7. PLAN 性能计数器设计

现有代码已经提供以下可复用入口：

| 现有入口 | 用途 |
|---|---|
| `app_ui_perf_note_dirty_mask()` | 记录 `DIRTY_PLAN` 出现频率和组合 mask |
| `app_ui_perf_note_router_cost()` | 记录 router 总耗时和 `plan_ms` VM 耗时 |
| `app_ui_perf_get_context()` | 记录 UI state、dash page、page ID 和 storage position |
| `ui_monkey` memory monitor | 记录 LVGL heap、RT heap 和线程栈水位 |

真机测试工程可通过强实现覆盖 weak perf hook，不需要让算法层或数据层直接依赖 PLAN 页面对象。

建议新增测试结构体：

```c
typedef struct
{
    uint32_t window_start_ms;
    uint32_t update_count;
    uint32_t invalidate_count;
    uint32_t draw_cb_count;
    uint32_t cache_build_count;
    uint32_t cache_hit_count;
    uint32_t area_change_rebuild_count;
    uint32_t vm_change_rebuild_count;
    uint32_t raw_point_sum;
    uint32_t cached_point_sum;
    uint32_t profile_segment_test_count;
    uint32_t profile_segment_draw_count;
    uint32_t dash_route_count;
    uint32_t dash_segment_generated_count;
    uint32_t dash_segment_draw_count;
    uint64_t cache_build_cycles_sum;
    uint32_t cache_build_cycles_max;
    uint64_t draw_cycles_sum;
    uint32_t draw_cycles_max;
    uint32_t draw_time_hist[7];
} plan_perf_stats_t;
```

如果编译器或平台不适合使用 `uint64_t`，可以使用 32 位窗口累计，并保证每秒清零一次。

### 7.1 计数器埋点位置

| 埋点位置 | 需要记录的数据 |
|---|---|
| `card_plan_update()` | `update_count`、VM 变化导致的 cache invalidation |
| 调用 `lv_obj_invalidate()` 前 | `invalidate_count` |
| `plan_chart_draw_cb()` 入口/出口 | `draw_cb_count`、单 chunk draw cycles |
| `plan_chart_profile_cache_rebuild()` | `cache_build_count`、raw/cached 点数、build cycles |
| cache 有效且 area 未变 | `cache_hit_count` |
| area 不一致触发重建 | `area_change_rebuild_count` |
| `plan_chart_profile_cache_draw()` | 被检查和实际提交的历史线段数 |
| `draw_diagonal_dashed_line()` | 路径数、生成子段数、实际提交子段数 |

### 7.2 缓存命中定义

一次 draw callback 进入后：

```text
缓存有效并且 area 坐标一致：cache hit
缓存无效或 area 坐标变化：cache build
```

稳定停留在 PLAN 页面时，如果 draw buffer 分 8 块，一次 invalidation 的预期是 1 次 build 和 7 次 hit。

页面滚动动画期间 area 会变化，允许每个动画帧重建一次，但同一帧的多个 chunk 仍应共享本帧缓存。

## 8. 高精度计时要求

### 8.1 推荐使用 CPU cycle counter

毫秒级 `lv_tick_get()` 无法准确区分 0.2 ms、0.8 ms 和 1.5 ms。真机性能测试优先使用：

- Cortex-M DWT `CYCCNT`
- 芯片高精度硬件定时器
- BSP 已有的微秒计时接口

绘图回调内只读取 cycle counter 并做整数加减，不进行除法和浮点格式化。

### 8.2 计时范围

需要分别测量：

| 指标 | 起止位置 |
|---|---|
| `cache_build_cycles` | `plan_chart_profile_cache_rebuild()` 入口到出口 |
| `draw_chunk_cycles` | `plan_chart_draw_cb()` 入口到出口 |
| `profile_draw_cycles` | 缓存历史线段绘制前后 |
| `dash_draw_cycles` | 减压预测虚线处理前后 |
| `lcd_flush_cycles` | LCD flush 提交到完成回调，需在显示驱动层记录 |
| `plan_vm_cycles` | 可复用现有 `app_ui_perf_note_router_cost()` 的 `plan_ms` |

### 8.3 draw callback 耗时直方图

不要保存每次回调的全部样本。建议使用固定桶：

| 桶 | 微秒范围 |
|---|---:|
| H0 | `< 1000` |
| H1 | `1000 - 1999` |
| H2 | `2000 - 3999` |
| H3 | `4000 - 7999` |
| H4 | `8000 - 15999` |
| H5 | `16000 - 31999` |
| H6 | `>= 32000` |

测试结束后根据直方图和日志计算 P50、P95、P99。

### 8.4 区分 UI 绘制和 LCD flush

PLAN 刷新总延迟至少包含：

```text
VM 生成 + 几何缓存重建 + LVGL 软件绘制 + LCD flush/DMA 等待
```

必须分别记录软件绘制时间和 LCD flush 时间。如果 `draw_us` 很低但完整刷新仍然很慢，问题可能在 SPI/QSPI、DMA、cache clean 或 flush 等待，不应继续修改 PLAN 几何代码。

建议显示驱动提供以下聚合字段：

```text
flush_count
flush_pixels
flush_cycles_sum
flush_cycles_max
flush_timeout_count
```

### 8.5 cycle counter 注意事项

- 32 位 DWT counter 会回绕，但单次 callback 时间远小于回绕周期，可使用无符号减法计算差值。
- 只在测试初始化时启用 DWT，不要在每次 draw callback 中反复开启和清零全局 counter。
- 如果其它模块也使用 DWT，禁止独占式重置 `DWT->CYCCNT`，应读取开始值和结束值做差。
- cycle 转微秒应在聚合打印阶段完成，不要放在线段循环中。

## 9. 聚合日志格式

### 9.1 每秒状态日志

推荐格式：

```text
[PLAN_PERF] t=3600 page=4 upd=1 inv=1 draw=8 build=1 hit=7 area_rebuild=0 vm_rebuild=1 raw=200 cached=6 prof_test=42 prof_draw=9 dash_route=7 dash_gen=68 dash_draw=12 build_us=420 draw_us_sum=8750 draw_us_max=1680 flush=8 flush_us=4100 flush_max_us=620 cpu_x10=284 lv_free=25120 rt_free=182304 stack_free=4864
```

字段定义：

| 字段 | 含义 |
|---|---|
| `t` | 当前潜水时间，秒 |
| `page` | 当前页面 ID，PLAN 应为 `PAGE_ID_PLAN` |
| `upd` | 本窗口 `card_plan_update()` 次数 |
| `inv` | 本窗口 PLAN 对象 invalidation 次数 |
| `draw` | 本窗口 draw callback 次数 |
| `build` | 几何缓存重建次数 |
| `hit` | 几何缓存命中次数 |
| `area_rebuild` | 因对象坐标变化触发的重建次数 |
| `vm_rebuild` | 因 VM 更新触发的重建次数 |
| `raw` | 最近一次 VM 原始历史点数 |
| `cached` | 最近一次缓存像素点数 |
| `prof_test` | 检查是否与 clip 相交的历史线段数 |
| `prof_draw` | 实际提交给 `lv_draw_line()` 的历史线段数 |
| `dash_route` | 预测路线原始线段数量 |
| `dash_gen` | 手工虚线生成的子段数 |
| `dash_draw` | 实际提交的虚线子段数 |
| `build_us` | 本窗口缓存重建总耗时 |
| `draw_us_sum` | 本窗口全部 draw callback 总耗时 |
| `draw_us_max` | 单个 chunk 最大耗时 |
| `flush` | LCD flush 次数 |
| `flush_us` | LCD flush 累计耗时 |
| `flush_max_us` | 单次 LCD flush 最大耗时 |
| `cpu_x10` | CPU 百分比乘 10，避免浮点打印 |
| `lv_free` | LVGL heap 空闲字节 |
| `rt_free` | RT-Thread heap 空闲字节 |
| `stack_free` | LVGL/UI 线程栈最低剩余字节 |

### 9.2 刷新轮次日志

为了准确计算一次 invalidation 对应的 chunk 数，建议在测试版本增加刷新序号：

```text
[PLAN_FRAME] seq=182 inv_seq=47 chunks=8 build=1 hit=7 raw=200 cached=6 draw_us=8750 max_chunk_us=1680
```

如果无法从 LCD flush 驱动得到 frame 结束事件，可以先使用 `invalidate_seq` 加时间窗口近似分组，并在报告中注明。

### 9.3 异常日志

只在达到阈值时打印：

```text
[PLAN_WARN] reason=update_rate upd=8 window_ms=1000
[PLAN_WARN] reason=cache_rebuild_per_chunk draw=8 build=8
[PLAN_WARN] reason=draw_budget draw_us=34120 budget_us=30000
[PLAN_WARN] reason=cpu_sustained cpu_x10=946 streak=3
[PLAN_WARN] reason=stack_low stack_free=1536
```

### 9.4 测试结束汇总

```text
[PLAN_SUMMARY] case=P03 elapsed_s=600 inv=602 draw=4816 build=602 hit=4214 raw_max=200 cached_min=2 cached_max=200 draw_p50_us=980 draw_p95_us=1720 draw_p99_us=2440 draw_max_us=4100 refresh_p95_us=12600 refresh_max_us=21800 cpu_avg_x10=286 cpu_max_x10=617 lv_drop=0 rt_drop=0 stack_min=4608 result=PASS
```

### 9.5 日志传输对测试的影响

UART 使用 8N1 时，每字节通常需要约 10 bit。300 字节日志在 115200 baud 下理论发送时间约为：

```text
300 * 10 / 115200 = 26 ms
```

如果 `rt_kprintf()` 同步等待 UART，单条日志就可能超过一帧预算。

推荐顺序：

1. 最优：计数器快照写入 ring buffer，由低优先级日志线程异步输出。
2. 次优：UART DMA 异步发送，日志线程只排队。
3. 最低要求：聚合周期改为 2 秒，压缩字段，并把打印耗时单独记录。

PLAN 绘制计时必须在日志打印之前完成。不能把 `rt_kprintf()` 时间计入 `draw_us`。

## 10. 标准测试数据集

### 10.1 C01 空轨迹

```text
raw_count = 0
current_depth = 0
deco_stop_count = 0
```

预期：不绘制历史线，页面不崩溃，无越界访问。

### 10.2 C02 单点轨迹

```text
(0 s, 0 m)
```

预期：缓存点数为 1，不提交历史线段，NOW 连接逻辑正常。

### 10.3 C03 200 点恒深轨迹

```text
time_s = i * 2
depth_m = 20
i = 0..199
```

预期：

```text
raw = 200
cached = 2
历史几何线段 = 1
```

这是验证共线合并和几何缓存的首要用例。

### 10.4 C04 下潜、恒深、上升

建议：

```text
0 - 60 s: 0 m 线性下潜到 30 m
60 - 300 s: 恒深 30 m
300 - 480 s: 30 m 线性上升到 0 m
采样周期: 2 s
```

预期缓存点数应显著小于原始点数，并保留三个主要阶段。

### 10.5 C05 阶梯轨迹

```text
0 m -> 40 m -> 30 m -> 20 m -> 10 m -> 0 m
每个平台保持 2 分钟
```

用于检查水平线、垂直/斜线转折和标签遮挡。

### 10.6 C06 200 点最坏锯齿轨迹

```text
time_s = i * 2
depth_m = (i % 2 == 0) ? 5 : 55
i = 0..199
```

预期：

```text
raw = 200
cached 接近 200
```

这是历史轨迹最坏绘制复杂度，不代表真实潜水曲线，但必须用于性能上限测试。

### 10.7 C07 长时间像素压缩轨迹

构造 8 到 12 小时时间轴，并让大量最近采样点映射到相同 X 像素。

预期：重复像素点被合并，缓存点数不应机械保持 200。

### 10.8 C08 最大减压站

构造 10 个停留站，例如：

```text
30 m / 1 min
27 m / 1 min
24 m / 1 min
21 m / 2 min
18 m / 2 min
15 m / 3 min
12 m / 3 min
9 m / 4 min
6 m / 5 min
3 m / 6 min
```

检查预测虚线生成数量、标签位置、时间轴扩展和 CPU 峰值。

### 10.9 C09 安全停留

构造 `STOP_SAFETY`、5 m、3 分钟，分别测试：

- 尚未进入停留区
- 已进入停留区
- 倒计时接近 0
- 停留完成

### 10.10 C10 时间轴边界

至少测试以下秒数附近：

```text
59, 60, 61
299, 300, 301
599, 600, 601
1199, 1200, 1201
3599, 3600, 3601
10799, 10800, 10801
14399, 14400, 14401
28799, 28800, 28801
```

检查秒、分钟、小时模式切换时刻度数量、单位和缓存重建。

## 11. 功能与视觉测试矩阵

| ID | 场景 | 检查项 |
|---|---|---|
| F01 | 空数据进入 PLAN | 无崩溃、无随机线段 |
| F02 | 1 点、2 点、200 点 | 起点、终点和 NOW 正确 |
| F03 | 恒深 200 点 | 一条连续水平线，无断点 |
| F04 | 锯齿 200 点 | 所有转折保留，无越界 |
| F05 | 最大减压站 | 10 个站点和标签完整 |
| F06 | metric/imperial 切换 | 深度标签单位和值正确 |
| F07 | 页面滑入滑出 | 缓存随 area 变化重建，无旧坐标残影 |
| F08 | 横竖布局切换 | 缓存失效，尺寸和轴线重算 |
| F09 | 告警覆盖 PLAN | 告警层、图表和裁剪无异常 |
| F10 | 睡眠恢复 | 首帧轨迹正确，无旧缓存 |
| F11 | 长时间轴 | 刻度不重叠，单位正确 |
| F12 | chunk 边界 | 线条跨 chunk 连续，无接缝和缺口 |

视觉检查必须使用真机拍照或视频，至少保留：

- 恒深 200 点
- 锯齿 200 点
- 最大减压站
- 小时轴
- 页面滑动中间帧
- 告警覆盖状态

## 12. 性能测试矩阵

| ID | 场景 | 时长 | 重点指标 |
|---|---|---:|---|
| P01 | 点数阶梯 0/1/10/50/100/199/200 | 每档 2 分钟 | draw 时间随点数变化 |
| P02 | 恒深 200 点 | 10 分钟 | cached=2、CPU 和 cache hit |
| P03 | 锯齿 200 点 | 10 分钟 | 最坏历史线段开销 |
| P04 | 最大 10 个减压站 | 10 分钟 | 虚线生成和标签开销 |
| P05 | 12 小时时间轴 | 10 分钟 | 映射压缩、小时刻度 |
| P06 | 页面进入/退出 1000 次 | 完成即止 | area 重建和对象生命周期 |
| P07 | layout 重建 200 次 | 完成即止 | 缓存失效、内存稳定 |
| P08 | invalidation 1/2/5/10/20 Hz | 每档 60 秒 | 系统极限和拐点 |
| P09 | 告警动画叠加 | 10 分钟 | 多层绘制峰值 |
| P10 | 正常潜水数据 | 1 小时 | 实际业务负载 |
| P11 | LCD flush 单独统计 | 10 分钟 | draw 与传输耗时占比 |
| P12 | 日志关闭/开启 A-B 对比 | 每档 5 分钟 | 判断日志测量扰动 |

P08 是压力极限测试，不作为正常业务刷新频率。

## 13. 长时间稳定性测试

### 13.1 最低要求

| 测试 | 时长/次数 |
|---|---:|
| PLAN 页面持续显示 | 12 小时 |
| 扩展耐久测试 | 24 小时 |
| 页面切换 | 1000 次 |
| 布局重建 | 200 次 |
| metric/imperial 切换 | 200 次 |
| 安全停留/减压状态切换 | 500 次 |

### 13.2 配合 ui_monkey

项目当前已经启用 `UI_LVGL_MONKEY_TEST_ENABLED`，可通过 MSH 执行：

```text
ui_monkey start
ui_monkey speed 50
ui_monkey status
ui_monkey stop
```

PLAN 专项建议先运行确定性用例，再运行 `ui_monkey start`。只有需要同时注入业务数据和告警时才使用：

```text
ui_monkey start full
```

现有 monkey 判定可直接复用：

| 项目 | 当前阈值 |
|---|---:|
| LVGL heap 允许下降 | 16384 bytes |
| RT-Thread heap 允许下降 | 16384 bytes |
| 最低线程栈剩余 | 2048 bytes |
| timer stall 容忍 | 500 ms |
| 单次动作耗时告警 | 80 ms |

## 14. 推荐验收指标

以下阈值是当前 33 FPS、约 30 ms frame period 的建议值。若真机产品刷新周期不同，应按比例换算，同时在报告中保留原始微秒值。

### 14.1 正确性硬指标

- 无 HardFault、assert、watchdog reset
- 无数组越界、缓存越界和对象失效访问
- 轨迹跨 chunk 连续，无缺线、重影和残影
- 页面移动或布局变化后不使用旧绝对坐标
- 恒深 200 点必须得到 `cached=2`
- 锯齿 200 点必须保留全部有效转折，不得为了性能静默截断

### 14.2 缓存指标

稳定页面单次 invalidation：

```text
cache_build_count <= 1
cache_hit_count = draw_cb_count - cache_build_count
```

如果稳定页面出现：

```text
cache_build_count == draw_cb_count
```

判定为缓存失效策略错误，必须修复。

### 14.3 刷新频率指标

默认 2 秒采样、无减压变化时：

```text
update_rate 目标约 0.5 次/秒
```

减压计划每秒变化时：

```text
update_rate 可接近 1 到 2 次/秒
```

稳定业务状态下持续 10 秒超过 5 次/秒，判定为异常高频刷新，需要追查 dirty 来源。

### 14.4 耗时指标

以 `frame_period_us` 表示完整 LVGL 帧预算：

| 指标 | 目标 | 硬上限 |
|---|---:|---:|
| 缓存重建 P95 | `<= 0.10 * frame_period` | `<= 0.20 * frame_period` |
| 单次完整 PLAN 刷新 P95 | `<= 0.60 * frame_period` | `<= 1.00 * frame_period` |
| 单次完整 PLAN 刷新最大值 | `<= 1.00 * frame_period` | `<= 1.50 * frame_period` |
| 页面输入响应 P95 | `<= 100 ms` | `<= 200 ms` |

以 30 ms frame period 为例：

```text
cache_build_p95 <= 3 ms，硬上限 6 ms
plan_refresh_p95 <= 18 ms，硬上限 30 ms
plan_refresh_max 目标 <= 30 ms，硬上限 45 ms
```

### 14.5 CPU 指标

建议使用相同潜水状态下的非 PLAN 页面作为基线：

```text
PLAN 页面平均 CPU 增量目标 <= 15 个百分点
PLAN 页面平均 CPU 增量硬上限 <= 25 个百分点
```

硬失败条件：

- CPU >= 90% 连续 3 个 1 秒采样窗口
- CPU 100% 持续 2 秒及以上
- 出现 watchdog、输入失去响应或 UI timer stall

### 14.6 LCD flush 指标

- `flush_timeout_count` 必须为 0
- 同一测试场景下 flush 像素数不应随潜水时间无界增长
- 单次完整刷新总耗时应同时报告 draw 占比和 flush 占比
- 如果 flush 占完整刷新耗时超过 60%，优先优化显示驱动、DMA 和传输带宽
- 如果软件 draw 占比超过 60%，再继续分析历史线段、虚线、文字和 mask

### 14.7 功耗与温升，可选但推荐

长时间显示 PLAN 页可能改变 CPU 活跃率和 LCD 刷新频率。量产前建议记录：

- 基线页面平均电流
- PLAN 页面平均电流
- 12 小时测试前后设备温度
- 电池电量下降速度

功耗阈值按整机产品预算验收，不在本文档中给统一绝对值。

### 14.8 内存与栈指标

- LVGL heap 相对稳定基线不得持续下降超过 16384 bytes
- RT-Thread heap 相对稳定基线不得持续下降超过 16384 bytes
- 连续 3 个监控样本下降才判定失败，避免瞬时缓存误报
- LVGL/UI 线程栈最低剩余必须 >= 2048 bytes
- `s_profile_cache` 为静态 PSRAM/BSS 数据，运行期间占用必须恒定
- 12 小时测试结束后 free heap 应回到稳定区间，不得呈单调下降趋势

## 15. 标准执行流程

### 15.1 准备阶段

1. 记录第 4 节全部硬件和固件信息。
2. 使用 Release 或与量产一致的优化等级编译。
3. 启用 `UI_PLAN_PERF_TEST_ENABLED`。
4. 确认日志周期为 1 秒，禁止逐线打印。
5. 烧录后静置 5 分钟，等待系统和 heap 基线稳定。
6. 记录非 PLAN 页面 5 分钟 CPU、heap 和 stack 基线。

### 15.2 功能测试

按 C01 到 C10、F01 到 F12 顺序执行。每个用例保存：

- 测试命令
- 起止时间
- 关键日志
- 真机照片或视频
- PASS/FAIL
- 异常说明

### 15.3 性能测试

1. 执行 `plan_test perf start`。
2. 加载指定测试轨迹。
3. 进入 PLAN 页面并保持测试要求的时长。
4. 不操作设备时记录稳定态。
5. 执行页面切换、告警或 layout 变化时记录动态态。
6. 执行 `plan_test perf stop` 输出汇总。
7. 将 `[PLAN_PERF]`、`[PLAN_FRAME]` 和 `[PLAN_SUMMARY]` 保存为原始日志。

### 15.4 长稳测试

1. 先完成 1 小时测试。
2. 再完成 12 小时测试。
3. 最后根据项目阶段决定是否执行 24 小时扩展测试。
4. 每 2 秒保留 `ui_monkey` 状态日志，每 1 秒保留 PLAN 聚合日志。
5. 测试结束必须主动执行 `ui_monkey stop` 和 `plan_test perf stop` 获取最终汇总。

## 16. 日志分析方法

### 16.1 判断 invalidation 风暴

```text
upd/inv 很高，draw 按 chunk 比例同步升高
```

重点追查：

- `dive_log_append()` 的调用频率
- `bus_set_deco_plan()` 是否每次写入都被判定为变化
- `bus_set_next_stop()` 是否高频变化
- 是否重复 `bus_requeue_dirty(DIRTY_PLAN)`
- 页面进入逻辑是否反复请求当前页刷新

### 16.2 判断分块重复计算

```text
inv 正常，draw 高，build 与 draw 相等
```

说明每个 chunk 都重建缓存。检查：

- `s_profile_cache.valid` 是否被错误清除
- `area` 是否在同一刷新轮次中异常变化
- 是否把 `clip_area` 错误当成对象 area 存入缓存 key

### 16.3 判断历史轨迹仍是瓶颈

```text
raw=200，cached 很小，但 profile_draw 或 draw_us 仍高
```

检查：

- 线宽 3 的斜线软件 mask 开销
- 实际提交线段是否跨越多个 chunk
- 是否有其它 draw callback 同时运行
- 是否启用了额外抗锯齿、mask 或透明层

### 16.4 判断预测虚线是瓶颈

```text
cached 很小，dash_gen/dash_draw 很高，dash_draw_cycles 占比高
```

下一步可考虑缓存预测虚线子段，或者根据 clip 计算可见 dash 索引范围。

### 16.5 判断 CPU 高但 PLAN 不是主因

```text
PLAN draw_us 很低，但整机 CPU 仍接近 100%
```

检查：

- 减压算法计算周期
- LCD flush/DMA 等待
- UART 大量日志
- 告警动画
- 其它页面或隐藏对象错误刷新
- RT-Thread 高优先级任务饥饿

### 16.6 判断内存泄漏

只有满足以下特征才按泄漏调查：

- free heap 随时间单调下降
- 离开 PLAN 页面后不恢复
- 多次进入/退出页面后每轮下降量近似固定
- LVGL heap 或 RT heap 连续多个监控样本突破容忍值

静态 `s_profile_cache` 占用不会随运行时间增长，不属于泄漏。

## 17. 测试报告模板

```text
测试编号：
固件 commit：
硬件版本：
MCU/主频：
draw buffer：
测试场景：
测试时长：

原始点最大值：
缓存点最小/最大值：
平均 chunk 数：
update/invalidate/draw 次数：
cache build/hit 次数：
cache build P95/max：
完整 PLAN 刷新 P50/P95/P99/max：
CPU 基线/PLAN 平均/PLAN 最大：
LVGL heap baseline/min/drop：
RT heap baseline/min/drop：
stack baseline/min：
timer stall 次数：
视觉异常：
崩溃/assert/watchdog：

结果：PASS / FAIL
失败原因：
原始日志路径：
照片/视频路径：
测试人：
测试日期：
```

## 18. 发布前清理

性能验证结束后：

1. 将 `UI_PLAN_PERF_TEST_ENABLED` 设为 `0`。
2. 确认 draw callback 内没有保留 `rt_kprintf()`。
3. 测试命令只能保留在测试构建中。
4. 正式版本保留几何缓存和裁剪逻辑，不保留高频统计开销。
5. 再执行一次 Release 冒烟测试，确认关闭埋点后行为一致。

## 19. 最终验收建议

建议将以下项目设为合入/量产门槛：

- C03 恒深 200 点通过
- C06 锯齿 200 点通过
- C08 最大减压站通过
- P03、P04、P06、P07 通过
- 12 小时长稳通过
- CPU 不出现持续 90% 以上
- 无 timer stall、watchdog、heap 单调下降和 stack 低水位
- 缓存每轮分块刷新只重建一次
- 页面滚动和布局变化后无旧坐标残影

只有这些指标同时满足，才能证明 PLAN 图在真机上的功能、性能和稳定性均达到可交付状态。
