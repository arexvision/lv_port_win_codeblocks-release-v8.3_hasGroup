#ifndef APP_UI_DEBUG_FLAGS_H
#define APP_UI_DEBUG_FLAGS_H

/* app_ui 测试/诊断/trace 编译期开关总表。
 *
 * 放置原则：
 * 1. 这里只放 app_ui 层测试、诊断、trace、profile 类开关；
 * 2. 禁止 app_ui 为了拿宏去 include src/app、service、driver 或 SDK 头文件；
 * 3. 临时性能诊断类宏必须集中在这里，不能散落在 screen.c、ui_state.c、update_router.c 等业务文件里；
 * 4. 这里的 0/1 是“编译期是否启用”，不是运行时状态。修改后需要重新编译对应目标；
 * 5. 正常开发和量产时，本文件内所有 *_ENABLED 测试宏都应保持 0；
 * 6. 真实运行优化/保护策略不放这里，应放在 ui_build_flags.h。
 *
 * 当前分类：
 * - *_TRACE_ENABLED：细粒度调试日志，通常只在定位具体 UI 逻辑时短时打开；
 * - UI_SCROLL_PROFILE_* / UI_CLICK_PROFILE_*：纯性能定位日志，默认关闭，问题定位完成后不能长期打开；
 *
 * 0/1 总规则：
 * - 0：编译期关闭，不输出对应测试/诊断日志；
 * - 1：编译期打开，输出对应测试/诊断日志；
 * - 量产：全部保持 0；
 * - 正常开发：默认保持 0；
 * - 专项定位：只打开当前问题需要的最小开关，定位完成恢复为 0。
 */
#define UI_CALLBACK_TRACE_ENABLED    0
#define UI_DATA_LAYOUT_TRACE_ENABLED 0
#define UI_LAYOUT_TRACE_ENABLED      0
#define UI_COMPASS_DIAG_TRACE_ENABLED 0
#define UI_COMPASS_DIAG_SYSTEM_TRACE_ENABLED 0

/* 页面切换/点击路径聚合诊断，属于临时测试代码。
 *
 * 谁使用：
 * - UI_SCROLL_PROFILE_*：src/app_ui/ui/screen/screen.c；
 * - UI_CLICK_PROFILE_*：src/app_ui/ui/core/ui_state.c。
 *
 * 打开后输出：
 * - [UI_SCROLL]：统计 tile 切换、layout、罗盘强刷、补 dirty、dots 更新的耗时；
 * - [UI_CLICK]：统计 click 前 pending 页面落地、click action、debounce 次数。
 *
 * 默认为什么关闭：
 * - 这些日志只服务于性能定位，不属于产品功能；
 * - 打开后会周期性输出串口日志，虽然是聚合日志，仍会占用串口带宽和少量 CPU；
 * - 问题定位完成后必须关闭，避免测试代码长期混入正式运行链路。
 *
 * 什么时候可以打开：
 * - 再次出现 LVGL handler 200ms+ 慢帧；
 * - 需要判断慢帧来自页面切换、点击、dirty 补刷还是组件刷新之前。
 *
 * 取值说明：
 * - UI_SCROLL_PROFILE_ENABLED=1：输出 [UI_SCROLL] 聚合诊断；
 * - UI_SCROLL_PROFILE_ENABLED=0：关闭 [UI_SCROLL]，量产必须为 0；
 * - UI_CLICK_PROFILE_ENABLED=1：输出 [UI_CLICK] 聚合诊断；
 * - UI_CLICK_PROFILE_ENABLED=0：关闭 [UI_CLICK]，量产必须为 0；
 * - 量产建议：两个都保持 0。只有专项性能定位时临时改为 1。
 */
#define UI_SCROLL_PROFILE_ENABLED 0
/* UI_SCROLL_PROFILE_INTERVAL_MS:
 * - UI_SCROLL_PROFILE_ENABLED=1 时生效；
 * - 控制 [UI_SCROLL] 汇总日志输出周期；
 * - 只影响日志频率，不影响 UI 功能；
 * - 专项定位时建议保持秒级，不要改得过小。
 */
#define UI_SCROLL_PROFILE_INTERVAL_MS 5000U
/* UI_SCROLL_SLOW_MS:
 * - UI_SCROLL_PROFILE_ENABLED=1 时生效；
 * - 超过该阈值的页面切换子阶段会计入 slow；
 * - 只影响统计口径，不影响 UI 功能。
 */
#define UI_SCROLL_SLOW_MS 80U
#define UI_CLICK_PROFILE_ENABLED 0
/* UI_CLICK_PROFILE_INTERVAL_MS:
 * - UI_CLICK_PROFILE_ENABLED=1 时生效；
 * - 控制 [UI_CLICK] 汇总日志输出周期；
 * - 只影响日志频率，不影响点击功能。
 */
#define UI_CLICK_PROFILE_INTERVAL_MS 5000U
/* UI_CLICK_SLOW_MS:
 * - UI_CLICK_PROFILE_ENABLED=1 时生效；
 * - 超过该阈值的 click 处理会计入 slow；
 * - 只影响统计口径，不影响点击功能。
 */
#define UI_CLICK_SLOW_MS 80U

#if UI_CALLBACK_TRACE_ENABLED
#define UI_CALLBACK_TRACE(...) rt_kprintf(__VA_ARGS__)
#else
#define UI_CALLBACK_TRACE(...) do { } while (0)
#endif

#if UI_DATA_LAYOUT_TRACE_ENABLED
#define UI_DATA_LAYOUT_TRACE(...) rt_kprintf(__VA_ARGS__)
#else
#define UI_DATA_LAYOUT_TRACE(...) do { } while (0)
#endif

#if UI_LAYOUT_TRACE_ENABLED
#define UI_LAYOUT_TRACE(...) rt_kprintf(__VA_ARGS__)
#else
#define UI_LAYOUT_TRACE(...) do { } while (0)
#endif

#endif /* APP_UI_DEBUG_FLAGS_H */
