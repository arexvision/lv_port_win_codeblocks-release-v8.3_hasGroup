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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define POD_TAG_BASE  1000U
#define POD1_TAG      ((uintptr_t)(POD_TAG_BASE + COMP_POD_0806))
#define POD2_TAG      ((uintptr_t)(2U * POD_TAG_BASE + COMP_POD_0806))

static bool comp_container_refresh_visible(lv_obj_t *container)
{
    return screen_obj_refresh_visible(container);
}

typedef void (*comp_refresh_container_cb_t)(lv_obj_t *container, void *ctx);

static void comp_for_each_refresh_container(comp_refresh_container_cb_t cb, void *ctx)
{
    uint8_t max_count;

    if (cb == NULL)
    {
        return;
    }

    /* 数据源始终由 g_sensor_data/DataBus 维护，和 UI 对象是否存在无关。
     * 这里仅裁剪“把数据写到哪些 LVGL 对象”：
     * - 左侧固定区始终是当前屏幕的一部分；
     * - 右侧自定义卡片只刷新当前可见页；
     * - 删除左侧或右侧任一小组件，只会让对应对象不再被遍历，不会影响另一侧同类数据。 */
    if (g_left_anchor_obj != NULL &&
        lv_obj_is_valid(g_left_anchor_obj) &&
        comp_container_refresh_visible(g_left_anchor_obj))
    {
        cb(g_left_anchor_obj, ctx);
    }

    max_count = (g_card_custom_obj_count < MAX_CUSTOM_CARDS)
                ? g_card_custom_obj_count
                : MAX_CUSTOM_CARDS;

    for (uint8_t c = 0; c < max_count; c++)
    {
        lv_obj_t *container;

        if (!screen_custom_card_refresh_visible(c))
        {
            continue;
        }

        container = g_card_custom_objs[c];
        if (container == NULL || !lv_obj_is_valid(container))
        {
            continue;
        }

        cb(container, ctx);
    }
}

static void comp_label_set_text_if_changed(lv_obj_t *label, const char *text)
{
    const char *old_text;

    if (label == NULL || !lv_obj_is_valid(label) || !lv_obj_check_type(label, &lv_label_class) || text == NULL)
    {
        return;
    }

    old_text = lv_label_get_text(label);
    if (old_text != NULL && strcmp(old_text, text) == 0)
    {
        return;
    }

    lv_label_set_text(label, text);
}

static void comp_label_set_text_fmt_if_changed(lv_obj_t *label, const char *fmt, ...)
{
    char buf[32];
    va_list args;

    if (label == NULL || fmt == NULL)
    {
        return;
    }

    va_start(args, fmt);
    (void)vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    comp_label_set_text_if_changed(label, buf);
}

static void comp_sync_text_from_vm(comp_id_t w_id, uint8_t pod_index)
{
    /* 文本类组件统一先走 VM，再落到具体 label，避免各处重复格式化。 */
    ui_vm_value_text_t value_vm;

    ui_vm_value_text_update(&value_vm, w_id, pod_index);
    comp_set_text(w_id, value_vm.text);
}

typedef struct
{
    comp_id_t id;
    float value;
} comp_value_update_ctx_t;

static void comp_set_value_in_container(lv_obj_t *container, void *ctx)
{
    comp_value_update_ctx_t *update = (comp_value_update_ctx_t *)ctx;
    comp_id_t id;
    float value;
    int16_t child_cnt;

    if (container == NULL || update == NULL)
    {
        return;
    }

    id = update->id;
    value = update->value;
    child_cnt = lv_obj_get_child_cnt(container);

    for (int16_t i = 0; i < child_cnt; i++)
    {
        lv_obj_t *child = lv_obj_get_child(container, i);
        if (!child || !lv_obj_is_valid(child)) continue;

        uintptr_t child_tag = (uintptr_t)lv_obj_get_user_data(child);

        if (id == COMP_DEPTH_1612 && child_tag == (uintptr_t)id)
        {
            float display_value = bus_get_depth_display(value);
            /* 大深度组件把整数和小数拆成两个 label 分开更新。 */
            int di = (int)display_value;
            float decimal_part = fabsf(display_value - di);
            int dd = (int)(decimal_part * 10 + 0.5f);
            lv_obj_t *part0;
            lv_obj_t *part1;

            if (dd > 9) dd = 9;

            part0 = lv_obj_get_child(child, 0);
            part1 = lv_obj_get_child(child, 1);
            if (part0 && lv_obj_is_valid(part0) && lv_obj_check_type(part0, &lv_label_class))
            {
                comp_label_set_text_fmt_if_changed(part0, "%d", di);
            }
            if (part1 && lv_obj_is_valid(part1) && lv_obj_check_type(part1, &lv_label_class))
            {
                comp_label_set_text_fmt_if_changed(part1, ".%d", dd);
            }
            lv_obj_t *unit = lv_obj_get_child(child, 2);
            if (unit && lv_obj_is_valid(unit) && lv_obj_check_type(unit, &lv_label_class))
            {
                comp_label_set_text_if_changed(unit, bus_get_depth_unit_label());
            }
            continue;
        }

        if (child_tag == (uintptr_t)id)
        {
            /* 其余组件按 user_data 定位到对应子 label 并格式化输出。 */
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
                        if (id == COMP_DEPTH_1606)
                        {
                            snprintf(buf, sizeof(buf), "%.1f", (double)bus_get_depth_display(value));
                        }
                        else if (id == COMP_TEMP_0806)
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
                        comp_label_set_text_if_changed(sub, buf);
                    }
                    break;
                }
            }
        }
    }
}

void comp_set_value(comp_id_t id, float value)
{
    comp_value_update_ctx_t ctx = { id, value };

    if (comp_value_handle_set_value(id, value))
    {
        return;
    }

    comp_for_each_refresh_container(comp_set_value_in_container, &ctx);
}

typedef struct
{
    comp_id_t id;
    const char *text;
} comp_text_update_ctx_t;

static void comp_set_text_in_container(lv_obj_t *container, void *ctx)
{
    comp_text_update_ctx_t *update = (comp_text_update_ctx_t *)ctx;
    comp_id_t id;
    const char *text;
    int16_t child_cnt;

    if (container == NULL || update == NULL || update->text == NULL)
    {
        return;
    }

    id = update->id;
    text = update->text;
    child_cnt = lv_obj_get_child_cnt(container);

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
                        comp_label_set_text_if_changed(sub, text);
                    }
                    break;
                }
            }
        }
    }
}

void comp_set_text(comp_id_t id, const char *text)
{
    comp_text_update_ctx_t ctx = { id, text };

    if (!text) return;

    if (comp_value_handle_set_text(id, text))
    {
        return;
    }

    comp_for_each_refresh_container(comp_set_text_in_container, &ctx);
}

static void comp_sync_pod_values_in_container(lv_obj_t *container, void *ctx)
{
    int16_t child_cnt;

    (void)ctx;

    if (container == NULL)
    {
        return;
    }

    child_cnt = lv_obj_get_child_cnt(container);
    for (int16_t i = 0; i < child_cnt; i++)
    {
        lv_obj_t *child = lv_obj_get_child(container, i);
        uintptr_t child_tag;
        ui_vm_value_text_t value_vm;
        uint8_t pod_index;
        int16_t sub_cnt;

        if (!child || !lv_obj_is_valid(child)) continue;

        child_tag = (uintptr_t)lv_obj_get_user_data(child);
        if (child_tag != POD1_TAG && child_tag != POD2_TAG)
        {
            continue;
        }

        pod_index = (child_tag == POD2_TAG) ? 2U : 1U;
        ui_vm_value_text_update(&value_vm, COMP_POD_0806, pod_index);

        sub_cnt = lv_obj_get_child_cnt(child);
        for (int16_t j = 0; j < sub_cnt; j++)
        {
            lv_obj_t *sub = lv_obj_get_child(child, j);
            if (!sub || !lv_obj_is_valid(sub)) continue;

            if ((uintptr_t)lv_obj_get_user_data(sub) == (uintptr_t)COMP_POD_0806 &&
                lv_obj_check_type(sub, &lv_label_class))
            {
                comp_label_set_text_if_changed(sub, value_vm.text);
                break;
            }
        }
    }
}

static void comp_sync_pod_values(void)
{
    if (comp_value_handle_sync_pod())
    {
        return;
    }

    comp_for_each_refresh_container(comp_sync_pod_values_in_container, NULL);
}

void comp_sync_data(comp_id_t w_id)
{
    /* 这个分发器把组件 ID 映射到对应的刷新策略。 */
    /* 它是组件层的最后一道适配层：
     * 上游 screen 只知道“某个 widget 要刷新”，
     * 具体是直接写数值、走 VM 文本、还是调用专用刷新函数，在这里统一决策。 */
    comp_refresh_depth_unit_labels();
    switch (w_id)
    {
    /* =========================================================
     * 1. 核心驻留& 复杂状态机 (这些由专属函数处理，这里做兜
     * ========================================================= */
    case COMP_NDL_STOP_1606:
    case COMP_TISSUE_GF_4012:
    case COMP_TISSUE_RAW_4012:
        /*
         * 这些复杂组件在页面运行时会走专属刷新链路。
         * 这里补一次完整同步，确保布局重建/初次进入时不会残留占位态，
         * 也避免外部只触发全量刷新时看起来像“数据源未绑定”。
         */
        if (w_id == COMP_NDL_STOP_1606)
        {
            comp_refresh_ndl_stop(DIRTY_DIVE_PROFILE | DIRTY_DECO_STATUS);
        }
        else
        {
            ui_vm_deco_t deco_vm;

            ui_vm_deco_update(&deco_vm, NULL, NULL);
            comp_refresh_tissue_widgets(&deco_vm, DIRTY_TISSUE_TOX);
        }
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
        comp_set_value(w_id, bus_get_depth());
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

    case COMP_TTS_AT_5MIN_0806:
    case COMP_TTS_DELTA_5MIN_0806:
    case COMP_NDL_UP_3M_0806:
    case COMP_NDL_DOWN_3M_0806:
    case COMP_NDL_DELTA_3M_0806:
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

    case COMP_GTR_0806:
    case COMP_RMV_0806:
    case COMP_SAC_0806:
        comp_sync_text_from_vm(w_id, 0U);
        break;

    /* =========================================================
     * 7. 传感& 拓展 (Sensors)
     * ========================================================= */
    case COMP_HEADING_0806:
    case COMP_COMPASS_1612:
        comp_sync_text_from_vm(w_id, 0U);
        if (w_id == COMP_COMPASS_1612)
        {
            comp_refresh_compass_widgets();
        }
        break;

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
