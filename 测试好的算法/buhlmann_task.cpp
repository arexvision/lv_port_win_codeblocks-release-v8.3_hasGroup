#include "buhlmann_task.h"
#include "task_config.h"
#include "../config/global_vars.h"
#include "../modules/Buhlmann.h"
#include "rtthread.h"
#include <stdio.h>
#include <math.h>
#include "../ui/arex_ui/arex_data.h"
#include "../ui/arex_ui/arex_ui_state.h"
#include "../modules/dive_log_encoder.h"   // 潜水日志编码器
#include "../modules/dive_log_storage.h"
#define DBG_TAG "buhlmann"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

extern "C" {
#include "../ble/simple_kv.h"              // KVDB 接口
}

// ========== 共享数据实例 ==========
rt_mutex_t buhlmannDataMutex = RT_NULL;
DiveInfo sharedDiveInfo;

// ========== 外部引用 ==========
extern Buhlmann buhlmann;
extern bool is_diving_for_buhlmann;  // 是否在潜水状态
extern bool need_reset_start_time;   // 需要重置 StartTime 的标志
extern volatile uint32_t g_dive_start_tick_ms;
extern rt_mutex_t ui_snapshot_mutex;  // UI 快照互斥锁
extern UiDataSnapshot g_ui_snapshot;  // UI 数据快照

static const float kDiveStartConfirmDepthM = 1.2f;
static const float kDiveEndConfirmDepthM = 0.8f;
static const uint32_t kDiveStartConfirmMs = 2000;
static const uint32_t kDiveEndConfirmMs = 30000;
static const float kDepthSanityMinM = -1.0f;
static const float kDepthSanityMaxM = 100.0f;
static const uint32_t kDepthSnapshotFreshnessMs = 1000;

static uint16_t clamp_u16_non_negative(int value)
{
    if (value <= 0) {
        return 0;
    }
    if (value >= 65535) {
        return 65535;
    }
    return (uint16_t)value;
}

static bool is_depth_value_sane(float depth_m)
{
    return !isnan(depth_m) &&
           !isinf(depth_m) &&
           depth_m >= kDepthSanityMinM &&
           depth_m <= kDepthSanityMaxM;
}

static bool read_depth_snapshot(float *depth_out,
                                float *temperature_out,
                                float *pressure_out,
                                sensor_status_t *health_out,
                                uint32_t *timestamp_age_out)
{
    if (depth_out == NULL) {
        return false;
    }

    bool snapshot_valid = false;

    if (rt_mutex_take(ui_snapshot_mutex, rt_tick_from_millisecond(50)) == RT_EOK) {
        uint32_t now = rt_tick_get_millisecond();
        uint32_t timestamp = g_ui_snapshot.depth_sensor.timestamp;
        uint32_t age_ms = now - timestamp;
        sensor_status_t health = g_ui_snapshot.depth_sensor.health;
        float depth_m = g_ui_snapshot.depth_sensor.depth;

        if (temperature_out != NULL) {
            *temperature_out = g_ui_snapshot.depth_sensor.temperature;
        }
        if (pressure_out != NULL) {
            *pressure_out = g_ui_snapshot.depth_sensor.pressure;
        }
        if (health_out != NULL) {
            *health_out = health;
        }
        if (timestamp_age_out != NULL) {
            *timestamp_age_out = age_ms;
        }

        *depth_out = depth_m;

        extern bool serial_debug_mode_active;
        if (serial_debug_mode_active) {
            snapshot_valid = is_depth_value_sane(depth_m);
        } else if (timestamp > 0 &&
                   age_ms < kDepthSnapshotFreshnessMs &&
                   health == SENSOR_STATUS_HEALTHY &&
                   is_depth_value_sane(depth_m)) {
            snapshot_valid = true;
        }

        rt_mutex_release(ui_snapshot_mutex);
    }

    return snapshot_valid;
}

// C 函数声明（用于 AREX UI 潜水轨迹记录）

// 潜水日志系统相关
static uint16_t g_next_dive_id = 0;        // 下一个潜水编号
static uint32_t g_dive_start_time = 0;     // 潜水开始时间（秒）
static bool g_dive_log_enabled = true;     // 日志记录开关

extern int debug_time_acceleration;

/**
 * @brief 初始化Buhlmann任务相关资源
 */
void buhlmann_task_init(void)
{
    // 创建互斥锁
    buhlmannDataMutex = rt_mutex_create("buhlmann", RT_IPC_FLAG_FIFO);

    /* 统一默认气体配置，确保 UI 显示、算法计算、Gas Switch 使用的是同一组槽位。 */
    buhlmann.setGas(0, 0.21f, 0.0f, true, 1.4f);
    buhlmann.setGas(1, 0.32f, 0.0f, false, 1.4f);
    buhlmann.setGas(2, 0.18f, 0.45f, false, 1.4f);
    buhlmann.setGas(3, 1.00f, 0.0f, false, 1.6f);
    buhlmann.setActiveGas(0);
    buhlmann.setOxygenRateInGas(0.21f);
    buhlmann.setNitrogenRateInGas(0.79f);
    buhlmann.setFinalStopDepth(DECO_DEFAULT_FINAL_STOP_METERS);

    // 初始化共享数据
    memset(&sharedDiveInfo, 0, sizeof(DiveInfo));
    sharedDiveInfo.ascendRate = ASCEND_OK;
    sharedDiveInfo.minutesToDeco = 99;
    sharedDiveInfo.ppo2 = 0.21f;
}

/**
 * @brief 安全读取共享数据
 */
bool buhlmann_get_dive_info(DiveInfo *outData, rt_tick_t timeout)
{
    if (outData == NULL || buhlmannDataMutex == RT_NULL) {
        return false;
    }

    if (rt_mutex_take(buhlmannDataMutex, timeout) == RT_EOK) {
        memcpy(outData, &sharedDiveInfo, sizeof(DiveInfo));
        rt_mutex_release(buhlmannDataMutex);
        return true;
    }

    return false;
}

/**
 * @brief 重置Buhlmann算法状态
 */
void buhlmann_reset(void)
{
    buhlmann.stopDive(rt_tick_get());
    DiveResult* initialResult = buhlmann.initializeCompartments();
    buhlmann.startDive(initialResult, rt_tick_get());
    buhlmann.resetCNS();
    buhlmann.resetOTU();

    if (rt_mutex_take(buhlmannDataMutex, rt_tick_from_millisecond(100)) == RT_EOK)
    {
        memset(&sharedDiveInfo, 0, sizeof(DiveInfo));
        sharedDiveInfo.ascendRate = ASCEND_OK;
        sharedDiveInfo.minutesToDeco = 99;
        sharedDiveInfo.ppo2 = 0.21f;
        rt_mutex_release(buhlmannDataMutex);
    }

    // rt_kprintf("[Buhlmann] Algorithm reset complete\n");
}

// 算法任务的周期状态：
// - lastWakeTick 用来实现类似 xTaskDelayUntil 的固定周期调度
// - algorithmRemainderMs 保存倍速后不足 1 秒的余数，避免时间被截断丢失
// - diveLogSampleRemainderMs 保存潜水日志采样余数，保证日志仍按整秒落点记录
struct BuhlmannTaskTiming
{
    rt_tick_t lastWakeTick;
    uint32_t algorithmRemainderMs;
    uint32_t diveLogSampleRemainderMs;
};

// 自动入水/出水检测状态：
// 深度超过阈值后不会立刻切换潜水状态，而是先累计确认时间，防止传感器抖动误触发。
struct BuhlmannDiveDetectState
{
    uint32_t startCandidateTickMs;
    uint32_t startConfirmElapsedMs;
    uint32_t endConfirmElapsedMs;
};

// 本轮算法使用的深度快照：
// 串口调试模式和真实传感器最终都会汇总成这个结构，主循环后面只认这一份数据。
struct BuhlmannDepthFrame
{
    float press;
    float depth;
    float temperature;
    bool pressValid;
    bool depthValid;
    uint32_t timestampAge;
    sensor_status_t health;
};

// 等待下一个算法周期，并返回本轮要推进的“算法秒数”。
// 串口 speed 倍速会在这里放大 elapsed_algorithm_ms，所以后面的算法和日志都能同步加速。
static unsigned int wait_next_algorithm_tick(BuhlmannTaskTiming &timing, uint32_t &elapsed_algorithm_ms)
{
    const rt_tick_t period_tick = rt_tick_from_millisecond(BUHLMANN_TASK_DELAY_MS);
    rt_tick_t now_tick = rt_tick_get();
    rt_int32_t elapsed_tick = (rt_int32_t)(now_tick - timing.lastWakeTick);
    rt_int32_t delay_tick = (rt_int32_t)period_tick - elapsed_tick;

    if (delay_tick > 0)
    {
        rt_thread_delay((rt_tick_t)delay_tick);
        now_tick = rt_tick_get();
        elapsed_tick = (rt_int32_t)(now_tick - timing.lastWakeTick);
    }

    timing.lastWakeTick = now_tick;
    elapsed_algorithm_ms = (uint32_t)((rt_tick_t)elapsed_tick * 1000U / RT_TICK_PER_SECOND);
    if (elapsed_algorithm_ms == 0U)
    {
        elapsed_algorithm_ms = BUHLMANN_TASK_DELAY_MS;
    }

    uint32_t time_scale = 1;
    if (debug_time_acceleration > 1)
    {
        time_scale = (uint32_t)debug_time_acceleration;
    }
    elapsed_algorithm_ms *= time_scale;

    timing.algorithmRemainderMs += elapsed_algorithm_ms;
    unsigned int delta_time_seconds = timing.algorithmRemainderMs / 1000U;
    timing.algorithmRemainderMs %= 1000U;
    if (delta_time_seconds == 0U)
    {
        delta_time_seconds = 1U;
    }

    return delta_time_seconds;
}

// 从 UI 快照读取当前深度、压力、温度和传感器健康状态。
// 如果深度无效，就回退到海平面默认压力，保证算法不会读到随机压力。
static BuhlmannDepthFrame read_current_depth_frame(void)
{
    BuhlmannDepthFrame frame;
    frame.press = 1013.25f;
    frame.depth = 0.0f;
    frame.temperature = 0.0f;
    frame.pressValid = false;
    frame.depthValid = false;
    frame.timestampAge = 0;
    frame.health = SENSOR_STATUS_DEAD;

    frame.depthValid = read_depth_snapshot(&frame.depth,
                                            &frame.temperature,
                                            &frame.press,
                                            &frame.health,
                                            &frame.timestampAge);
    if (frame.depthValid)
    {
        frame.pressValid = true;
    }

    return frame;
}

// 潜水刚开始时创建日志编码器，并写入本次潜水的固定头信息。
// 真正的采样点由 update_dive_log_samples() 负责追加。
static void start_dive_log_if_needed(void)
{
    if (is_diving_for_buhlmann && g_current_dive_encoder == nullptr && g_dive_log_enabled)
    {
        g_dive_start_time = rt_tick_get() / RT_TICK_PER_SECOND;
        g_current_dive_encoder = new DiveLogEncoder(g_next_dive_id, g_dive_start_time, 1);

        g_current_dive_encoder->setSurfacePressure(1013);
        g_current_dive_encoder->setWaterSalinity(0);
        g_current_dive_encoder->setGradientFactors(30, 85);
        g_current_dive_encoder->setDiveMode(MODE_OC_REC);
        g_current_dive_encoder->addEvent(EVENT_DIVE_START);
    }
}

// 根据深度和持续时间自动切换 is_diving_for_buhlmann。
// 入水阈值和出水阈值分开，并带确认时间，避免水面附近来回跳。
static void update_auto_dive_state(const BuhlmannDepthFrame &frame,
                                   uint32_t elapsed_algorithm_ms,
                                   BuhlmannDiveDetectState &state)
{
    if (!is_diving_for_buhlmann)
    {
        state.endConfirmElapsedMs = 0;

        if (frame.depthValid && frame.depth > kDiveStartConfirmDepthM)
        {
            if (state.startCandidateTickMs == 0)
            {
                state.startCandidateTickMs = rt_tick_get_millisecond();
                state.startConfirmElapsedMs = 0;
            }
            else
            {
                state.startConfirmElapsedMs += elapsed_algorithm_ms;
            }

            if (state.startConfirmElapsedMs >= kDiveStartConfirmMs)
            {
                g_dive_start_tick_ms = state.startCandidateTickMs;
                is_diving_for_buhlmann = true;
                need_reset_start_time = true;
                state.startCandidateTickMs = 0;
                state.startConfirmElapsedMs = 0;
                arex_dive_log_reset();
            }
        }
        else
        {
            state.startCandidateTickMs = 0;
            state.startConfirmElapsedMs = 0;
        }
    }
    else
    {
        state.startCandidateTickMs = 0;
        state.startConfirmElapsedMs = 0;

        if (frame.depthValid && frame.depth < kDiveEndConfirmDepthM)
        {
            state.endConfirmElapsedMs += elapsed_algorithm_ms;
            if (state.endConfirmElapsedMs >= kDiveEndConfirmMs)
            {
                is_diving_for_buhlmann = false;
                state.endConfirmElapsedMs = 0;
            }
        }
        else if (frame.depthValid && frame.depth >= kDiveEndConfirmDepthM)
        {
            state.endConfirmElapsedMs = 0;
        }
    }
}

// 当系统没有单独的 StartTime 任务时，由算法任务维护潜水计时。
// 这样最小模式只启动算法任务时，TTS/串口打印里的潜水时间也会继续走。
static void update_start_time_if_needed(unsigned int delta_time_seconds)
{
    if (startTimeTaskHandle != RT_NULL)
    {
        return;
    }

    if (need_reset_start_time)
    {
        StartTime = 0;
        arex_bus_set_dive_time(0);
        need_reset_start_time = false;
    }

    if (is_diving_for_buhlmann)
    {
        StartTime += delta_time_seconds;
        arex_bus_set_dive_time(StartTime);
    }
}

// 推进 Buhlmann 核心算法一步，并补齐 UI/串口要用的派生数据。
// 这里保持“算法相关”的内容集中：progressDive、PPO2、CNS/OTU、GF、16 舱分压。
static DiveInfo run_algorithm_step(float current_press, unsigned int delta_time_seconds)
{
    DiveInfo diveInfo = buhlmann.progressDive(current_press, delta_time_seconds);

    diveInfo.ppo2 = buhlmann.calculateOxygenPartialPressure(current_press);
    if (is_diving_for_buhlmann)
    {
        float timeInMinutes = (float)delta_time_seconds / 60.0f;
        buhlmann.updateCNS(diveInfo.ppo2, timeInMinutes);
        buhlmann.updateOTU(diveInfo.ppo2, timeInMinutes);
    }

    diveInfo.cns = buhlmann.getCumulativeCNS();
    diveInfo.otu = buhlmann.getCumulativeOTU();
    diveInfo.gf99 = buhlmann.calculateGF99();
    diveInfo.surfGF = buhlmann.calculateSurfaceGF();
    buhlmann.getCurrentCompartmentPressures(diveInfo.compartmentPressures);

    return diveInfo;
}

// 把最新算法结果写入共享 DiveInfo，供其他任务用 buhlmann_get_dive_info() 读取。
static void publish_shared_dive_info(const DiveInfo &diveInfo)
{
    if (rt_mutex_take(buhlmannDataMutex, rt_tick_from_millisecond(50)) == RT_EOK)
    {
        sharedDiveInfo = diveInfo;
        rt_mutex_release(buhlmannDataMutex);
    }
}

// 处理 UI 发来的气体切换命令。
// 切换前检查 MOD 和 ICD 风险，通过后才真正 setActiveGas。
static void handle_pending_gas_switch(float current_depth)
{
    uint8_t target_gas_idx = 0;
    if (!arex_has_pending_gas_switch(&target_gas_idx))
    {
        return;
    }

    if (target_gas_idx < MAX_GASES)
    {
        Gas target_gas = buhlmann.getGas(target_gas_idx);
        bool mod_ok = target_gas.enabled && current_depth <= target_gas.modDepth;
        bool icd_risk = buhlmann.checkICDRisk(target_gas_idx, current_depth);

        if (mod_ok && !icd_risk)
        {
            buhlmann.setActiveGas(target_gas_idx);
            if (g_current_dive_encoder != nullptr)
            {
                g_current_dive_encoder->addEvent(EVENT_GAS_SWITCH, target_gas_idx);
            }
        }
    }

    arex_clear_gas_switch_cmd();
}

// 同步 AREX UI 的核心算法数据：组织饱和度、CNS/OTU、GF、MOD、ceiling、气体密度等。
// 注意：停留站和气体列表分别在 sync_arex_stop_data()/sync_arex_gas_data() 里处理。
static void sync_arex_core_data(const DiveInfo &diveInfo, const BuhlmannDepthFrame &frame)
{
    uint8_t tissue_load[16];
    const float surface_pressure_bar = buhlmann.getSurfacePressure() / 1000.0f;

    for (int i = 0; i < 16; i++)
    {
        /* Per-compartment SurfGF: reaches GF High when this tissue drives NDL to zero. */
        float tissue_pressure_bar = buhlmann.getCompartmentTotalInertLoad(i) / 1000.0f;
        float m_value_bar = buhlmann.getCompartmentCombinedA(i) +
                            surface_pressure_bar / buhlmann.getCompartmentCombinedB(i);
        float denominator = m_value_bar - surface_pressure_bar;
        float load_percent = 0.0f;

        if (denominator > 0.0001f)
        {
            load_percent = ((tissue_pressure_bar - surface_pressure_bar) / denominator) * 100.0f;
        }

        if (load_percent > 200.0f) load_percent = 200.0f;
        if (load_percent < 0.0f) load_percent = 0.0f;
        tissue_load[i] = (uint8_t)(load_percent + 0.5f);
    }

    arex_bus_set_tissues((const uint8_t *)tissue_load);
    arex_bus_set_cns((uint8_t)diveInfo.cns);
    arex_bus_set_otu((uint16_t)diveInfo.otu);
    arex_bus_set_gf99(diveInfo.gf99);
    arex_bus_set_surf_gf(diveInfo.surfGF);

    uint8_t gf_low_pct = (uint8_t)(buhlmann.getGFLow() * 100.0f);
    uint8_t gf_high_pct = (uint8_t)(buhlmann.getGFHigh() * 100.0f);
    arex_bus_set_gf_setting(gf_low_pct, gf_high_pct);

    handle_pending_gas_switch(frame.depth);

    float mod_m = buhlmann.calculateMOD(g_sys_config.mod_ppo2);
    arex_bus_set_mod(mod_m);

    float ceiling_m = buhlmann.calculateDepthFromPressure(buhlmann.getLastCeilingPressure());
    arex_bus_set_ceiling(ceiling_m);

    Gas active_gas = buhlmann.getGas(buhlmann.getActiveGas());
    uint8_t o2_pct = (uint8_t)(active_gas.oxygenFraction * 100.0f);
    uint8_t he_pct = (uint8_t)(active_gas.heliumFraction * 100.0f);
    arex_bus_set_gas_mix(o2_pct, he_pct);

    float current_pressure = buhlmann.calculateHydrostaticPressureFromDepth(frame.depth);
    float surface_pressure = buhlmann.getSurfacePressure();
    float n2_fraction = 1.0f - active_gas.oxygenFraction - active_gas.heliumFraction;
    float gas_density = (active_gas.oxygenFraction * 1.429f +
                         n2_fraction * 1.251f +
                         active_gas.heliumFraction * 0.179f) *
                        (current_pressure / surface_pressure);
    arex_bus_set_gas_density(gas_density);

    float fio2_pct = (active_gas.oxygenFraction * current_pressure / surface_pressure) * 100.0f;
    arex_bus_set_fio2(fio2_pct);

    uint16_t tts_val = (uint16_t)(diveInfo.ttsSeconds / 60);
    if (tts_val > 9999) tts_val = 9999;
    arex_bus_set_tts(tts_val);
}

// UI 的 NDL 显示范围固定为 0~99 分钟，算法结果在这里做显示层裁剪。
static int16_t clamp_ndl_for_display(int minutes_to_deco)
{
    int16_t ndl_val = (int16_t)minutes_to_deco;
    if (ndl_val < 0) ndl_val = 0;
    if (ndl_val > 99) ndl_val = 99;
    return ndl_val;
}

// 同步停留状态：Safety/Deco 都由 Buhlmann 算法层计算，任务层只做 UI 映射。
static void sync_arex_stop_data(const DiveInfo &diveInfo)
{
    static bool deco_bar_active = false;
    static float deco_bar_depth_m = 0.0f;
    static uint16_t deco_bar_total_s = 0;

    int16_t ndl_val = clamp_ndl_for_display(diveInfo.minutesToDeco);
    arex_stop_type_t stop_type = AREX_STOP_NONE;
    if (diveInfo.stopType == BUHLMANN_STOP_SAFETY) {
        stop_type = AREX_STOP_SAFETY;
    } else if (diveInfo.stopType == BUHLMANN_STOP_DECO) {
        stop_type = AREX_STOP_DECO;
    }

    int16_t ndl_display_min = (stop_type == AREX_STOP_DECO) ? 0 : ndl_val;
    uint16_t stop_time_total_s = clamp_u16_non_negative(diveInfo.stopTimeTotalSeconds);
    uint16_t stop_time_left_s = clamp_u16_non_negative(diveInfo.stopTimeRemainingSeconds);

    if (stop_type == AREX_STOP_DECO) {
        if (!diveInfo.inStopZone) {
            deco_bar_active = false;
            deco_bar_total_s = stop_time_left_s;
            deco_bar_depth_m = diveInfo.stopDepthMeters;
            stop_time_total_s = stop_time_left_s;
        } else if (!deco_bar_active || fabsf(deco_bar_depth_m - diveInfo.stopDepthMeters) > 0.1f) {
            deco_bar_active = true;
            deco_bar_depth_m = diveInfo.stopDepthMeters;
            deco_bar_total_s = (stop_time_left_s > 0) ? stop_time_left_s : 1;
            stop_time_total_s = deco_bar_total_s;
        } else {
            if (stop_time_left_s > deco_bar_total_s) {
                deco_bar_total_s = stop_time_left_s;
            }
            stop_time_total_s = deco_bar_total_s;
        }
    } else {
        deco_bar_active = false;
        deco_bar_total_s = 0;
        deco_bar_depth_m = 0.0f;
    }

    arex_bus_update_deco(ndl_display_min,
                         stop_type,
                         diveInfo.stopDepthMeters,
                         stop_time_total_s,
                         stop_time_left_s,
                         diveInfo.inStopZone);

    arex_deco_stop_t deco_stops_ui[MAX_DECO_STOPS];
    uint8_t deco_count_ui = 0;
    if (diveInfo.decoSequence.stopCount > 0 && diveInfo.decoSequence.currentStopIdx >= 0)
    {
        for (int i = 0; i < diveInfo.decoSequence.stopCount && deco_count_ui < MAX_DECO_STOPS; i++)
        {
            deco_stops_ui[deco_count_ui].depth_m = diveInfo.decoSequence.stops[i].depth;
            deco_stops_ui[deco_count_ui].stay_min = (float)diveInfo.decoSequence.stops[i].remainingTime / 60.0f;
            deco_count_ui++;
        }
    }
    arex_bus_set_deco_plan(deco_stops_ui, deco_count_ui);
}

// 把气体配置格式化成 UI 使用的短名称，比如 AIR、NX 32、TX 18/45。
static void format_gas_name(const Gas &gas, char *name_buf, size_t name_buf_size)
{
    uint8_t o2_pct = (uint8_t)(gas.oxygenFraction * 100.0f + 0.5f);
    uint8_t he_pct = (uint8_t)(gas.heliumFraction * 100.0f + 0.5f);

    if (he_pct > 0)
    {
        snprintf(name_buf, name_buf_size, "TX %d/%d", o2_pct, he_pct);
    }
    else if (o2_pct == 21)
    {
        snprintf(name_buf, name_buf_size, "AIR");
    }
    else if (o2_pct == 100)
    {
        snprintf(name_buf, name_buf_size, "O2 100%%");
    }
    else
    {
        snprintf(name_buf, name_buf_size, "NX %d", o2_pct);
    }
}

// 同步当前气体、各气体 PPO2、以及气体槽位列表。
// 这部分和核心算法解耦，方便以后恢复多气体调试时单独看。
static void sync_arex_gas_data(float current_press)
{
    int active_gas_idx = buhlmann.getActiveGas();
    Gas active_gas = buhlmann.getGas(active_gas_idx);

    if (active_gas.enabled)
    {
        char gas_name_buf[16];
        format_gas_name(active_gas, gas_name_buf, sizeof(gas_name_buf));
        arex_bus_set_gas((uint8_t)active_gas_idx, gas_name_buf);
    }

    for (int i = 0; i < MAX_GASES; i++)
    {
        Gas gas = buhlmann.getGas(i);
        if (gas.enabled)
        {
            float ppo2 = gas.oxygenFraction * (current_press / 1013.25f);
            arex_bus_set_ppo2((uint8_t)i, ppo2);
        }
    }

    for (int i = 0; i < MAX_GASES; i++)
    {
        Gas gas = buhlmann.getGas(i);
        char gas_name_buf[16] = {0};
        uint8_t o2_pct = (uint8_t)(gas.oxygenFraction * 100.0f + 0.5f);
        uint8_t he_pct = (uint8_t)(gas.heliumFraction * 100.0f + 0.5f);

        format_gas_name(gas, gas_name_buf, sizeof(gas_name_buf));
        arex_bus_set_gas_slot((uint8_t)i, gas_name_buf, o2_pct, he_pct, gas.modDepth);
    }
}

// 把当前深度追加到 AREX 轨迹缓存，用于 UI 的 PLAN/TRACK 路径显示。
static void append_arex_dive_track_if_needed(const BuhlmannDepthFrame &frame)
{
    if (is_diving_for_buhlmann && frame.depthValid)
    {
        uint32_t dive_time_sec = g_sensor_data.dive_time_s;
        arex_dive_log_append((float)dive_time_sec, frame.depth);
    }
}

// 按真实经过的算法时间每 1 秒写一次潜水日志采样点。
// speed 倍速时 elapsed_algorithm_ms 会变大，所以 while 循环可能一次补多个采样点。
static void update_dive_log_samples(const DiveInfo &diveInfo,
                                    const BuhlmannDepthFrame &frame,
                                    uint32_t elapsed_algorithm_ms,
                                    uint32_t &sample_remainder_ms)
{
    if (g_current_dive_encoder == nullptr)
    {
        sample_remainder_ms = 0;
        return;
    }

    float depth_m = frame.depthValid ? frame.depth : 0.0f;
    float temp_c = frame.depthValid ? frame.temperature : 0.0f;
    float pressure_bar = 200.0f;

    sample_remainder_ms += elapsed_algorithm_ms;
    while (sample_remainder_ms >= 1000U)
    {
        g_current_dive_encoder->addSample(depth_m, temp_c, pressure_bar);
        sample_remainder_ms -= 1000U;
    }

    g_current_dive_encoder->setCNS(0, (uint8_t)diveInfo.cns);
    g_current_dive_encoder->setOTU((uint16_t)diveInfo.otu);
}

// 检测到出水后收尾潜水日志：写 END 事件、最终采样点、减压摘要并保存到存储。
static void finish_dive_log_if_needed(const DiveInfo &diveInfo, const BuhlmannDepthFrame &frame)
{
    if (is_diving_for_buhlmann || g_current_dive_encoder == nullptr)
    {
        return;
    }

    arex_dive_log_reset();
    g_current_dive_encoder->addEvent(EVENT_DIVE_END);

    float end_depth_m = frame.depthValid ? frame.depth : 0.0f;
    float end_temp_c = frame.depthValid ? frame.temperature : 0.0f;
    g_current_dive_encoder->addSample(end_depth_m, end_temp_c, 200.0f);

    uint32_t end_time = rt_tick_get() / RT_TICK_PER_SECOND;
    g_current_dive_encoder->finalizeDive(end_time);
    g_current_dive_encoder->setDecoInfo((uint8_t)diveInfo.gf99,
                                        (uint16_t)(diveInfo.ttsSeconds),
                                        (uint8_t)diveInfo.decoStopInMeters);

    uint8_t* data = nullptr;
    size_t size = 0;
    if (g_current_dive_encoder->serialize(&data, &size))
    {
        if (dive_log_storage_save(g_next_dive_id, data, size) != 0)
        {
            rt_kprintf("[DiveLog] save failed, dive_id=%d, size=%u\n", g_next_dive_id, (unsigned)size);
        }
        else
        {
            g_next_dive_id++;
        }

        delete[] data;
    }
    else
    {
        rt_kprintf("[DiveLog] serialize failed\n");
    }

    delete g_current_dive_encoder;
    g_current_dive_encoder = nullptr;
}

/**
 * @brief Buhlmann算法任务 - 固定1000ms执行
 */
void buhlmann_task(void *parameter)
{
    (void)parameter;

    rt_thread_mdelay(500);
    dive_log_storage_get_next_id(&g_next_dive_id);

    BuhlmannTaskTiming timing = {rt_tick_get(),0,0};
    BuhlmannDiveDetectState dive_state = {0,0,0};

    while (1)
    {
        // 1. 固定周期调度，并计算这轮算法要推进多少秒。
        uint32_t elapsed_algorithm_ms = 0;
        unsigned int deltaTimeSeconds = wait_next_algorithm_tick(timing, elapsed_algorithm_ms);

        // 2. 统一读取深度/压力来源。真实传感器和串口调试最终都会走这里。
        BuhlmannDepthFrame depth_frame = read_current_depth_frame();

        // 3. 维护潜水状态和潜水计时。算法推进前先确定“当前是否在潜水”。
        start_dive_log_if_needed();
        update_auto_dive_state(depth_frame, elapsed_algorithm_ms, dive_state);
        update_start_time_if_needed(deltaTimeSeconds);

        // 4. 推进 Buhlmann 核心算法，并发布给其他任务。
        DiveInfo diveInfo = run_algorithm_step(depth_frame.press, deltaTimeSeconds);
        publish_shared_dive_info(diveInfo);

        // 5. 把算法结果拆分同步到 UI 总线。
        sync_arex_core_data(diveInfo, depth_frame);
        sync_arex_stop_data(diveInfo);
        sync_arex_gas_data(depth_frame.press);

        // 6. 记录潜水轨迹和日志文件。
        append_arex_dive_track_if_needed(depth_frame);
        update_dive_log_samples(diveInfo,
                                depth_frame,
                                elapsed_algorithm_ms,
                                timing.diveLogSampleRemainderMs);
        finish_dive_log_if_needed(diveInfo, depth_frame);
    }
}
