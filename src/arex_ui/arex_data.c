#include "arex_data.h"
#include <math.h>
#include <string.h>

/* card_plan.c 中的减压站序列全局数组（由减压引擎写入，UI 消费） */
extern arex_deco_stop_t g_deco_stops[MAX_DECO_STOPS];
extern uint16_t         g_deco_stop_count;

/* =========================================================
 * Data Bus Setter 实现 — 硬件/模拟层专用
 * 铁律：仅更新数值 + 打脏标记，绝不碰 LVGL！
 * ========================================================= */

void arex_bus_set_depth(float depth_m)
{
    /* 防抖：只有变化超过 0.05m 才触发 UI 刷新，极大节省 CPU */
    if (fabsf(g_sensor_data.depth - depth_m) > 0.05f) {
        g_sensor_data.depth = depth_m;
        g_sensor_data.dirty_mask |= DIRTY_DEPTH | DIRTY_DECO;  //深度变化的时候也会触发跟踪
    }
}

void arex_bus_set_ndl(int16_t ndl_min)
{
    if (g_sensor_data.ndl != ndl_min) {
        g_sensor_data.ndl = ndl_min;
        g_sensor_data.dirty_mask |= DIRTY_NDL;
    }
}

void arex_bus_set_tts(uint16_t tts_min)
{
    if (g_sensor_data.tts != tts_min) {
        g_sensor_data.tts = tts_min;
        g_sensor_data.dirty_mask |= DIRTY_TTS;
    }
}

void arex_bus_set_pod(uint8_t pod_idx, float bar)
{
    if (pod_idx == 0 && g_sensor_data.pod1_bar != bar) {
        g_sensor_data.pod1_bar = bar;
        g_sensor_data.dirty_mask |= DIRTY_POD;
    } else if (pod_idx == 1 && g_sensor_data.pod2_bar != bar) {
        g_sensor_data.pod2_bar = bar;
        g_sensor_data.dirty_mask |= DIRTY_POD;
    }
}

void arex_bus_set_battery(float pct)
{
    if (fabsf(g_sensor_data.battery_pct - pct) > 0.1f) {
        g_sensor_data.battery_pct = pct;
        g_sensor_data.dirty_mask |= DIRTY_BATT;
    }
}

void arex_bus_set_heading(uint16_t heading_deg)
{
    if (g_sensor_data.heading != heading_deg) {
        g_sensor_data.heading = heading_deg;
        g_sensor_data.dirty_mask |= DIRTY_HEADING;
    }
}

void arex_bus_set_dive_time(uint32_t dive_s)
{
    if (g_sensor_data.dive_time_s != dive_s) {
        g_sensor_data.dive_time_s = dive_s;
        g_sensor_data.dirty_mask |= DIRTY_TIME;
    }
}

void arex_bus_set_surface_time(uint32_t surface_s)
{
    if (g_sensor_data.surface_time_s != surface_s) {
        g_sensor_data.surface_time_s = surface_s;
        g_sensor_data.dirty_mask |= DIRTY_TIME;
    }
}

void arex_bus_set_ppo2(uint8_t sensor_idx, float ppo2_val)
{
    if (sensor_idx < 3 && g_sensor_data.ppo2[sensor_idx] != ppo2_val) {
        g_sensor_data.ppo2[sensor_idx] = ppo2_val;
        g_sensor_data.dirty_mask |= DIRTY_PPO2;
    }
}

void arex_bus_set_gas(uint8_t gas_idx, const char *gas_name)
{
    if (g_sensor_data.gas_active_idx != gas_idx) {
        g_sensor_data.gas_active_idx = gas_idx;
    }
    if (gas_name != NULL && strncmp(g_sensor_data.gas_name, gas_name, 15) != 0) {
        strncpy(g_sensor_data.gas_name, gas_name, 15);
        g_sensor_data.gas_name[15] = '\0';
    }
    g_sensor_data.dirty_mask |= DIRTY_GAS;
}

void arex_bus_set_deco(int16_t stop_m, uint8_t stop_min)
{
    if (g_sensor_data.next_stop_m != stop_m || g_sensor_data.next_stop_min != stop_min) {
        g_sensor_data.next_stop_m = stop_m;
        g_sensor_data.next_stop_min = stop_min;
        g_sensor_data.dirty_mask |= DIRTY_DECO;
    }
}

void arex_bus_set_cns(uint8_t cns_pct)
{
    if (g_sensor_data.cns_pct != cns_pct) {
        g_sensor_data.cns_pct = cns_pct;
        g_sensor_data.dirty_mask |= DIRTY_CNS;
    }
}

void arex_bus_set_otu(uint16_t otu_val)
{
    if (g_sensor_data.otu != otu_val) {
        g_sensor_data.otu = otu_val;
        g_sensor_data.dirty_mask |= DIRTY_OTU;
    }
}

/* =========================================================
 * 临界区保护的数组写入 — 防止多线程数据撕裂
 *
 * 铁律：> 32bit 的数据块拷贝必须包在关中断临界区里。
 *   - PC 仿真器: rt_hw_interrupt_disable/enable 替换为空操作
 *   - 真机 RT-Thread: 触发底层 cpsr 关中断，耗时 < 0.1us
 * ========================================================= */

/* 16 组织舱饱和度数组写入（16 字节，必须包临界区） */
void arex_bus_set_tissues(const uint8_t tissue_pct[16])
{
    rt_base_t level = rt_hw_interrupt_disable();
    memcpy(g_sensor_data.tissue_pct, tissue_pct, 16);
    g_sensor_data.dirty_mask |= DIRTY_TISSUES;
    rt_hw_interrupt_enable(level);
}

/* 完整减压站序列写入（可变长度，必须包临界区） */
void arex_bus_set_deco_plan(const arex_deco_stop_t *stops, uint8_t count)
{
    if (count > MAX_DECO_STOPS) {
        count = MAX_DECO_STOPS;
    }
    rt_base_t level = rt_hw_interrupt_disable();
    g_deco_stop_count = count;
    if (count > 0 && stops != NULL) {
        memcpy(g_deco_stops, stops, count * sizeof(arex_deco_stop_t));
    }
    g_sensor_data.dirty_mask |= DIRTY_DECO;
    rt_hw_interrupt_enable(level);
}

void arex_bus_clear_all_dirty(void)
{
    g_sensor_data.dirty_mask = DIRTY_NONE;
}

void arex_bus_set_temperature(float temp_c)
{
    if (fabsf(g_sensor_data.temperature_c - temp_c) > 0.1f) {
        g_sensor_data.temperature_c = temp_c;
        g_sensor_data.dirty_mask |= DIRTY_TEMP;
    }
}

void arex_bus_set_ui_layout(const arex_ble_ui_sync_payload_t *payload)
{
    if (payload == NULL || payload->version != AREX_BLE_CFG_VERSION) {
        return;
    }

    /* 临界区保护，防止 UI 任务在中途读到撕裂的数据 */
#ifdef PC_SIMULATOR
    volatile rt_base_t level = 0;
#else
    rt_base_t level = rt_hw_interrupt_disable();
#endif

    /* 2. 拷贝右侧卡片滑动顺序 */
    memcpy(g_sys_config.card_order, payload->card_order, sizeof(g_sys_config.card_order));

    /* 3. 映射左侧 2x6 锚点配置 */
    g_left_widget_count = (payload->left_count > AREX_LEFT_MAX_WIDGETS)
                          ? AREX_LEFT_MAX_WIDGETS
                          : payload->left_count;
    for (int i = 0; i < g_left_widget_count; i++) {
        g_left_widgets[i].widget_id = (arex_widget_id_t)payload->left_widgets[i].id;
        g_left_widgets[i].x         = payload->left_widgets[i].x;
        g_left_widgets[i].y         = payload->left_widgets[i].y;
        g_left_widgets[i].w         = payload->left_widgets[i].w;
        g_left_widgets[i].h         = payload->left_widgets[i].h;
        g_left_widgets[i].font_id   = (arex_font_id_t)payload->left_widgets[i].font_id;
    }

    /* 4. 映射 5F 自定义网格配置 */
    g_sys_config.widget_count = (payload->custom_5f_count > 30)
                                 ? 30
                                 : payload->custom_5f_count;
    for (int i = 0; i < g_sys_config.widget_count; i++) {
        g_sys_config.widget_ids[i] = (arex_widget_id_t)payload->custom_5f_widgets[i].id;
        g_sys_config.widget_r[i]  = payload->custom_5f_widgets[i].r;
        g_sys_config.widget_c[i]  = payload->custom_5f_widgets[i].c;
        g_sys_config.widget_w[i]  = payload->custom_5f_widgets[i].w;
        g_sys_config.widget_h[i]  = payload->custom_5f_widgets[i].h;
    }

    /* 5. 打上终极脏标记，通知 UI 推倒重建 */
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;

#ifndef PC_SIMULATOR
    rt_hw_interrupt_enable(level);
#else
    (void)level;
#endif
}

void arex_bus_set_device_status(bool strobe_on, bool flashlight_on, uint8_t cylinder_count)
{
    if (g_sensor_data.strobe_on != strobe_on ||
        g_sensor_data.flashlight_on != flashlight_on ||
        g_sensor_data.cylinder_count != cylinder_count) {
        g_sensor_data.strobe_on = strobe_on;
        g_sensor_data.flashlight_on = flashlight_on;
        g_sensor_data.cylinder_count = cylinder_count;
        g_sensor_data.dirty_mask |= DIRTY_DEVICES;
    }
}

void arex_bus_toggle_layout_order(void)
{
    g_sys_config.layout_order = (g_sys_config.layout_order == AREX_ORDER_NORMAL)
                                ? AREX_ORDER_REVERSE
                                : AREX_ORDER_NORMAL;
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

void arex_bus_toggle_theme(void)
{
    g_sys_config.theme_mode = (g_sys_config.theme_mode == AREX_THEME_TECH)
                              ? AREX_THEME_CLASSIC
                              : AREX_THEME_TECH;
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

void arex_bus_toggle_dots_position(void)
{
    static const uint8_t seq[] = { AREX_DOTS_RIGHT, AREX_DOTS_LEFT, AREX_DOTS_BOTTOM, AREX_DOTS_NONE };
    static uint8_t idx = 0;
    idx = (idx + 1) % (sizeof(seq) / sizeof(seq[0]));
    g_sys_config.dots_position = seq[idx];
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

void arex_bus_toggle_compass_style(void)
{
    static const uint8_t seq[] = { AREX_COMPASS_CLASSIC, AREX_COMPASS_AERO, AREX_COMPASS_SUB };
    static uint8_t idx = 0;
    idx = (idx + 1) % (sizeof(seq) / sizeof(seq[0]));
    g_sys_config.compass_style = seq[idx];
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

void arex_bus_toggle_sep_style(void)
{
    static const uint8_t seq[] = { AREX_SEP_NONE, AREX_SEP_SOLID, AREX_SEP_DASHED, AREX_SEP_DOTTED };
    static uint8_t idx = 0;
    idx = (idx + 1) % (sizeof(seq) / sizeof(seq[0]));
    g_sys_config.sep_style = seq[idx];
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

void arex_bus_toggle_flash_speed(void)
{
    g_sys_config.flash_speed = (g_sys_config.flash_speed + 1) % 3;
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

void arex_bus_toggle_mask(void)
{
    g_sys_config.mask_enabled = !g_sys_config.mask_enabled;
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

void arex_bus_toggle_split_outward(void)
{
    g_sys_config.split_outward = !g_sys_config.split_outward;
    g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT;
}

/* =========================================================
 * 配置持久化接口
 * 由具体平台（PC 模拟器 / 真机）提供 weak 实现覆盖
 * ========================================================= */
bool arex_config_load(arex_sys_config_t *cfg)
{
    (void)cfg;
    return false;
}

bool arex_config_save(const arex_sys_config_t *cfg)
{
    (void)cfg;
    return false;
}
