/*
 * 文件: src/app_ui/ui/comp/comp_update.c
 * 作用: 该文件属于公共组件模块，负责复用样式、通用控件、局部刷新逻辑或组件级显示封装。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时应重点检查布局尺寸、刷新频率与复用接口是否一致，避免局部样式或坐标调整影响同类页面的对齐和更新节奏。
 */

#include "../core/data.h"
#include "../core/vm/ui_vm_dashboard_types.h"
#include "../core/vm/ui_vm_dashboard.h"
#include "../screen/screen.h"
#include "comp_update.h"
#include "comp_view.h"

#include <math.h>
#include <stdio.h>

#define POD_TAG_BASE  1000U
#define POD1_TAG      ((uintptr_t)(POD_TAG_BASE + COMP_POD_0806))
#define POD2_TAG      ((uintptr_t)(2U * POD_TAG_BASE + COMP_POD_0806))

static bool comp_container_refresh_visible(lv_obj_t *container)
{
    return screen_obj_refresh_visible(container);
}

static void comp_sync_text_from_vm(comp_id_t w_id, uint8_t pod_index)
{
    /* 文本类组件统一先走 VM，再落到具体 label，避免各处重复格式化。 */
    ui_vm_value_text_t value_vm;

    ui_vm_value_text_update(&value_vm, w_id, pod_index);
    comp_set_text(w_id, value_vm.text);
}

void comp_set_value(comp_id_t id, float value)
{
    /* 数值型刷新会在左锚点和所有自定义卡片容器里遍历匹配组件。 */
    /* 这套实现依赖 comp_view 创建组件时在对象/子对象的 user_data 里写入 comp_id_t。
     * 因此刷新时不需要保存一堆全局 label 指针，而是靠“烙印”反向找到目标控件。 */
    uint8_t max_count = (g_card_custom_obj_count < MAX_CUSTOM_CARDS)
                        ? g_card_custom_obj_count
                        : MAX_CUSTOM_CARDS;

    for (uint8_t c = 0; c <= max_count; c++)
    {
        lv_obj_t *container = (c < max_count) ? g_card_custom_objs[c] : g_left_anchor_obj;
        if (!container || !lv_obj_is_valid(container)) continue;
        if (!comp_container_refresh_visible(container)) continue;

        int16_t child_cnt = lv_obj_get_child_cnt(container);
        for (int16_t i = 0; i < child_cnt; i++)
        {
            lv_obj_t *child = lv_obj_get_child(container, i);
            if (!child || !lv_obj_is_valid(child)) continue;

            uintptr_t child_tag = (uintptr_t)lv_obj_get_user_data(child);

            if (id == COMP_DEPTH_1612 && child_tag == (uintptr_t)id)
            {
                /* 大深度组件把整数和小数拆成两个 label 分开更新。 */
                /* 这样做不是为了炫技，而是为了做“大整数 + 小数点后一位”的异形排版，
                 * 单 label 很难同时兼顾字号、对齐和视觉重心。 */
                int di = (int)value;
                float decimal_part = fabsf(value - di);
                int dd = (int)(decimal_part * 10 + 0.5f);
                if (dd > 9) dd = 9;

                lv_obj_t *part0 = lv_obj_get_child(child, 0);
                lv_obj_t *part1 = lv_obj_get_child(child, 1);
                if (part0 && lv_obj_is_valid(part0) && lv_obj_check_type(part0, &lv_label_class))
                {
                    lv_label_set_text_fmt(part0, "%d", di);
                }
                if (part1 && lv_obj_is_valid(part1) && lv_obj_check_type(part1, &lv_label_class))
                {
                    lv_label_set_text_fmt(part1, ".%d", dd);
                }
                continue;
            }

            if (child_tag == (uintptr_t)id)
            {
                /* 其余组件按 user_data 定位到对应子 label 并格式化输出。 */
                /* 这里统一处理不同组件的小数位规则，避免格式化逻辑散落在各个页面里。 */
                int16_t sub_cnt = lv_obj_get_child_cnt(child);
                for (int16_t j = 0; j < sub_cnt; j++)
                {
                    lv_obj_t *sub = lv_obj_get_child(child, j);
                    if (!sub || !lv_obj_is_valid(sub)) continue;
                    if ((uintptr_t)lv_obj_get_user_data(sub) == (uintptr_t)id)
                    {
                        if (lv_obj_check_type(sub, &lv_label_class))
                        {
                            char buf[32];
                            if (id == COMP_TEMP_0806 || id == COMP_DEPTH_1606)
                            {
                                snprintf(buf, sizeof(buf), "%.1f", (double)value);
                            }
                            else if (id == COMP_PPO2_0806)
                            {
                                snprintf(buf, sizeof(buf), "%.2f", (double)value);
                            }
                            else if (id == COMP_BATTERY_0806)
                            {
                                snprintf(buf, sizeof(buf), "%.0f%%", (double)value);
                            }
                            else if (id == COMP_TTS_0806 || id == COMP_NDL_STOP_1606)
                            {
                                snprintf(buf, sizeof(buf), "%d", (int)value);
                            }
                            else
                            {
                                snprintf(buf, sizeof(buf), "%.0f", (double)value);
                            }
                            lv_label_set_text(sub, buf);
                        }
                        break;
                    }
                }
            }
        }
    }
}

void comp_set_text(comp_id_t id, const char *text)
{
    /* 纯文本刷新路径，适合 TIME/GAS/POD 等已经格式化完成的内容。 */
    /* 与 comp_set_value() 的区别是：
     * - comp_set_value() 负责“拿到数值后格式化”
     * - comp_set_text() 负责“上游已经格式化好，直接写 label”
     * 两者分开后，VM 可以更灵活地决定格式策略。 */
    if (!text) return;

    uint8_t max_count = (g_card_custom_obj_count < MAX_CUSTOM_CARDS)
                        ? g_card_custom_obj_count
                        : MAX_CUSTOM_CARDS;

    for (uint8_t c = 0; c <= max_count; c++)
    {
        lv_obj_t *container = (c < max_count) ? g_card_custom_objs[c] : g_left_anchor_obj;
        if (!container || !lv_obj_is_valid(container)) continue;
        if (!comp_container_refresh_visible(container)) continue;

        int16_t child_cnt = lv_obj_get_child_cnt(container);
        for (int16_t i = 0; i < child_cnt; i++)
        {
            lv_obj_t *child = lv_obj_get_child(container, i);
            if (!child || !lv_obj_is_valid(child)) continue;

            if ((comp_id_t)(uintptr_t)lv_obj_get_user_data(child) == id)
            {
                int16_t sub_cnt = lv_obj_get_child_cnt(child);
                for (int16_t j = 0; j < sub_cnt; j++)
                {
                    lv_obj_t *sub = lv_obj_get_child(child, j);
                    if (!sub || !lv_obj_is_valid(sub)) continue;
                    if ((comp_id_t)(uintptr_t)lv_obj_get_user_data(sub) == id)
                    {
                        if (lv_obj_check_type(sub, &lv_label_class))
                        {
                            lv_label_set_text(sub, text);
                        }
                        break;
                    }
                }
            }
        }
    }
}

static void comp_sync_pod_values(void)
{
    /* POD 组件比较特殊：同一个 comp_id_t 会在左右两个瓶压格里复用。
     * 所以不能只靠 comp_id 判断，还要额外借助 POD1_TAG / POD2_TAG 区分实例。 */
    uint8_t max_count = (g_card_custom_obj_count < MAX_CUSTOM_CARDS)
                        ? g_card_custom_obj_count
                        : MAX_CUSTOM_CARDS;

    for (uint8_t c = 0; c <= max_count; c++)
    {
        lv_obj_t *container = (c < max_count) ? g_card_custom_objs[c] : g_left_anchor_obj;
        if (!container || !lv_obj_is_valid(container)) continue;
        if (!comp_container_refresh_visible(container)) continue;

        int16_t child_cnt = lv_obj_get_child_cnt(container);
        for (int16_t i = 0; i < child_cnt; i++)
        {
            lv_obj_t *child = lv_obj_get_child(container, i);
            if (!child || !lv_obj_is_valid(child)) continue;

            uintptr_t child_tag = (uintptr_t)lv_obj_get_user_data(child);
            if (child_tag != POD1_TAG && child_tag != POD2_TAG)
            {
                continue;
            }

            ui_vm_value_text_t value_vm;
            uint8_t pod_index = (child_tag == POD2_TAG) ? 2U : 1U;
            ui_vm_value_text_update(&value_vm, COMP_POD_0806, pod_index);

            int16_t sub_cnt = lv_obj_get_child_cnt(child);
            for (int16_t j = 0; j < sub_cnt; j++)
            {
                lv_obj_t *sub = lv_obj_get_child(child, j);
                if (!sub || !lv_obj_is_valid(sub)) continue;

                if ((uintptr_t)lv_obj_get_user_data(sub) == (uintptr_t)COMP_POD_0806 &&
                    lv_obj_check_type(sub, &lv_label_class))
                {
                    lv_label_set_text(sub, value_vm.text);
                    break;
                }
            }
        }
    }
}

void comp_sync_data(comp_id_t w_id)
{
    /* 这个分发器把组件 ID 映射到对应的刷新策略。 */
    /* 它是组件层的最后一道适配层：
     * 上游 screen 只知道“某个 widget 要刷新”，
     * 具体是直接写数值、走 VM 文本、还是调用专用刷新函数，在这里统一决策。 */
    switch (w_id)
    {
    /* =========================================================
     * 1. 核心驻留& 复杂状态机 (这些由专属函数处理，这里做兜
     * ========================================================= */
    case COMP_NDL_STOP_1606:
    case COMP_TISSUE_GF_4012:
    case COMP_TISSUE_RAW_4012:
        /* 这些是包含动多元素的复杂状态机，已ui_update_task 有专属刷新逻辑 */
        break;

    case COMP_SYS_1606:
        comp_refresh_sys(DIRTY_SYSTEM);
        break;

    /* =========================================================
     * 2. 深度组件
     * ========================================================= */
    case COMP_DEPTH_1612:
        comp_set_value(w_id, bus_get_depth());
        break;

    case COMP_DEPTH_1606:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    /* =========================================================
     * 3. 潜水时间（MM:SS 格式化）
     * ========================================================= */
    case COMP_DIVE_TIME_1606:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    /* =========================================================
     * 4. 气体组件
     * ========================================================= */
    case COMP_GAS_1606:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    /* =========================================================
     * 5. 基础组件 (Basic)
     * ========================================================= */
    case COMP_TEMP_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_TIME_1606:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_TTS_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_ASCENT_0806:
    case COMP_ASCENT_0812:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_BATTERY_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_STOP_DEPTH_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_STOP_TIME_1606:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_PPO2_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    /* =========================================================
     * 6. 技术潜(Tech Dive)
     * ========================================================= */
    case COMP_SURF_GF_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_GF99_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_GF_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_CNS_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_OTU_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_MOD_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_CEILING_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_GAS_MIX_1606:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_GAS_DENS_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_FIO2_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    /* =========================================================
     * 7. 传感& 拓展 (Sensors)
     * ========================================================= */
    case COMP_HEADING_0806:
    case COMP_COMPASS_1612:
    case COMP_GYRO_2406:
    case COMP_BATT_V_0806:
    case COMP_BATT_TEMP_0806:
    case COMP_PRJ_TEMP_0806:
    case COMP_CHARGE_0806:
    case COMP_PRESSURE_0806:
    case COMP_NOFLY_0806:
    case COMP_ACCEL_2406:
    case COMP_MAG_2406:
    case COMP_MLX_2406:
    case COMP_TMAG_2406:
    case COMP_ATTITUDE_2406:
    case COMP_BLE_RSSI_0806:
    case COMP_CPU_0806:
    case COMP_FPS_0806:
    case COMP_SENSOR_STAT_1606:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_POD_0806:
        comp_sync_pod_values();
        break;

    case COMP_DEPTH_MAX_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_DEPTH_AVG_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_TEMP_MIN_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_TEMP_AVG_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    case COMP_EMPTY:
    default:
        break;
    }
}
