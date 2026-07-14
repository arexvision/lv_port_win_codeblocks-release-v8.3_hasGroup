#ifndef APP_UI_HAL_SIM_SIM_POLICY_H_
#define APP_UI_HAL_SIM_SIM_POLICY_H_

/*
 * PC 模拟器专用策略参数。
 * 这些值只服务 hal_sim，不代表真机判定；真机策略见 app/modules/decompression/arex_deco_runtime_policy.h。
 */
#define SIM_LAYOUT_PHASE_COUNT       4U       /* 模拟器自动布局切换的阶段数量。 */
#define SIM_LAYOUT_SWITCH_TICKS      5U       /* 模拟器自动布局切换间隔，单位 tick。 */
#define SIM_DIVE_ENTRY_CONFIRM_S     3U       /* PC 模拟器入水确认秒数，和真机 AREX runtime 可独立调试。 */
#define SIM_SURFACE_DEPTH_M          0.2f     /* PC 模拟器出水确认深度，单位 m，不影响真机出水判定。 */
#define SIM_TEMP_C                   99.9f    /* PC 模拟器固定温度显示值，单位摄氏度。 */
#define AREX_RUNTIME_DEFAULT_SURFACE_MBAR 1010.0f /* AREX 运行时默认水面大气压，单位 mbar。 */
#define SIM_SURFACE_PRESSURE_MBAR    AREX_RUNTIME_DEFAULT_SURFACE_MBAR /* PC 模拟器水面大气压，单位 mbar，用于深度换算压力。 */
#define SIM_WATER_METERS_PER_BAR     10.0f    /* PC 模拟器水深/压力换算系数，单位 m/bar。 */
#define SIM_HEADING_TIMER_MS         10U      /* PC 模拟器指南针刷新周期，单位 ms。 */

#endif /* APP_UI_HAL_SIM_SIM_POLICY_H_ */
