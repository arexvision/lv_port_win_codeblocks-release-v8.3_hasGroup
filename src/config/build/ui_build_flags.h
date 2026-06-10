#ifndef APP_UI_BUILD_FLAGS_H
#define APP_UI_BUILD_FLAGS_H

/* app_ui 自己的构建期参数与量产运行策略。
 *
 * 放置原则：
 * 1. 这里只放 app_ui 层真实运行需要的构建期参数和默认策略；
 * 2. app_ui 不能直接包含上层 app/config/system/task_profile.h，否则 UI 组件层会反向依赖嵌入式任务配置；
 * 3. 测试日志、trace、profile 统计不放这里，应放在 ui_debug_flags.h 且默认关闭；
 * 4. 这里的优化/保护策略是正常开发和量产路径的一部分，不属于“测试宏”。
 */
#define APP_UI_UPDATE_TIMER_DELAY_MS 100U

/* UI dirty 发布节流，属于真实运行优化。
 *
 * 取值：
 * - 1：启用 UI dirty 合帧。正常开发和量产建议保持 1；
 * - 0：关闭 UI dirty 合帧。仅用于临时排查“显示刷新被节流导致数据滞后”。
 *
 * 作用：
 * - 数据 bus_set_* 仍然每次保存最新值；
 * - 这里只限制 UI 任务发布/取走 dirty 的频率；
 * - 被节流的 dirty 会重新并回 dirty_mask，后续到期再刷新，不会丢数据源。
 */
#define UI_DIRTY_THROTTLE_ENABLED 1
#define UI_DIRTY_SENSOR_MIN_INTERVAL_MS 500U
#define UI_DIRTY_COMPASS_MIN_INTERVAL_MS 100U
#define UI_DIRTY_SYSTEM_MIN_INTERVAL_MS 1000U
#define UI_DIRTY_GAS_MIN_INTERVAL_MS 200U

/* 页面切换后的可见页补刷新策略，属于真实运行优化。
 *
 * 取值：
 * - 1：切页后延后补 dirty。只适合专项验证“极限快速旋转时是否还存在中间页刷新压力”；
 * - 0：最终可见页立即补 dirty。正常开发和量产建议保持 0。
 *
 * 作用：
 * - 数据源始终由 bus_set_* 保存最新值，这个宏只控制“切到可见页后何时把最新值写入 LVGL 对象”；
 * - 快速旋转时，中间页刷新压力主要由 UI_DASH_ROTATE_COALESCE_ENABLED 解决；
 * - 最终落页再延后补 dirty 会造成“页面先到、组件值过一会才出现”的体感滞后。
 *
 * 嵌入式第一性原理：
 * - 不可见页不刷新，避免无效 CPU/LCD flush；
 * - 可见页必须尽快补齐当前数据，避免用户看到半成品界面。
 */
#define UI_PAGE_DIRTY_DEFER_ENABLED 0
#define UI_PAGE_DIRTY_DEFER_WINDOW_MS 120U

/* DASH 快速翻页目标合并，属于真实运行优化。
 *
 * 取值：
 * - 1：启用 DASH 普通页快速翻页合并。正常开发和量产建议保持 1；
 * - 0：每次旋转立即切页。仅用于临时排查“目标页/边界行为异常”。
 *
 * 作用：
 * - 连续旋转时只记录最终目标页；
 * - 停顿后只对最终目标页执行 screen_scroll_to_page()；
 * - 不合并菜单、编辑态、modal、边界蓄力进入 INFO/SETUP。
 */
#define UI_DASH_ROTATE_COALESCE_ENABLED 1
#define UI_DASH_ROTATE_COALESCE_WINDOW_MS 80U

/* 点击消费防抖，属于真实运行保护。
 *
 * 取值：
 * - 1：启用 UI 状态机入口 click 兜底防抖。正常开发和量产建议保持 1；
 * - 0：关闭 click 兜底防抖。仅用于临时排查“合法快速双击被过滤”。
 *
 * 作用：
 * - 物理输入层仍负责按键采样和基础状态判断；
 * - 这里过滤已经进入 UI 状态机的极短间隔重复 click；
 * - 过滤的是重复消费，不影响旋转输入，也不影响数据源同步。
 */
#define UI_CLICK_DEBOUNCE_ENABLED 1
#define UI_CLICK_DEBOUNCE_WINDOW_MS 120U

#endif /* APP_UI_BUILD_FLAGS_H */
