#include "../core/ui_engine.h"
#include "comp_update.h"
#include "comp_view.h"

#include <math.h>
#include <stdio.h>

void comp_set_value(comp_id_t id, float value)
{
    uint8_t max_count = (g_card_custom_obj_count < MAX_CUSTOM_CARDS)
                        ? g_card_custom_obj_count
                        : MAX_CUSTOM_CARDS;

    for (uint8_t c = 0; c <= max_count; c++)
    {
        lv_obj_t *container = (c < max_count) ? g_card_custom_objs[c] : g_left_anchor_obj;
        if (!container) continue;

        int16_t child_cnt = lv_obj_get_child_cnt(container);
        for (int16_t i = 0; i < child_cnt; i++)
        {
            lv_obj_t *child = lv_obj_get_child(container, i);
            if (!child) continue;

            uintptr_t child_tag = (uintptr_t)lv_obj_get_user_data(child);

            if (id == COMP_DEPTH_1612 && child_tag == (uintptr_t)id)
            {
                int di = (int)value;
                float decimal_part = fabsf(value - di);
                int dd = (int)(decimal_part * 10 + 0.5f);
                if (dd > 9) dd = 9;

                lv_obj_t *part0 = lv_obj_get_child(child, 0);
                lv_obj_t *part1 = lv_obj_get_child(child, 1);
                if (part0 && lv_obj_check_type(part0, &lv_label_class))
                {
                    lv_label_set_text_fmt(part0, "%d", di);
                }
                if (part1 && lv_obj_check_type(part1, &lv_label_class))
                {
                    lv_label_set_text_fmt(part1, ".%d", dd);
                }
                continue;
            }

            if (child_tag == (uintptr_t)COMP_POD_0806)
            {
                int16_t sub_cnt = lv_obj_get_child_cnt(child);
                for (int16_t j = 0; j < sub_cnt; j++)
                {
                    lv_obj_t *sub = lv_obj_get_child(child, j);
                    if (!sub) continue;
                    if ((uintptr_t)lv_obj_get_user_data(sub) == (uintptr_t)COMP_POD_0806)
                    {
                        if (lv_obj_check_type(sub, &lv_label_class))
                        {
                            char buf[32];
                            snprintf(buf, sizeof(buf), "%.0f", (double)value);
                            lv_label_set_text(sub, buf);
                        }
                        break;
                    }
                }
                continue;
            }

            if (child_tag == (uintptr_t)id)
            {
                int16_t sub_cnt = lv_obj_get_child_cnt(child);
                for (int16_t j = 0; j < sub_cnt; j++)
                {
                    lv_obj_t *sub = lv_obj_get_child(child, j);
                    if (!sub) continue;
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
    if (!text) return;

    uint8_t max_count = (g_card_custom_obj_count < MAX_CUSTOM_CARDS)
                        ? g_card_custom_obj_count
                        : MAX_CUSTOM_CARDS;

    for (uint8_t c = 0; c <= max_count; c++)
    {
        lv_obj_t *container = (c < max_count) ? g_card_custom_objs[c] : g_left_anchor_obj;
        if (!container) continue;

        int16_t child_cnt = lv_obj_get_child_cnt(container);
        for (int16_t i = 0; i < child_cnt; i++)
        {
            lv_obj_t *child = lv_obj_get_child(container, i);
            if (!child) continue;

            if ((comp_id_t)(uintptr_t)lv_obj_get_user_data(child) == id)
            {
                int16_t sub_cnt = lv_obj_get_child_cnt(child);
                for (int16_t j = 0; j < sub_cnt; j++)
                {
                    lv_obj_t *sub = lv_obj_get_child(child, j);
                    if (!sub) continue;
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

void comp_sync_data(comp_id_t w_id)
{
    char buf[32];

    switch (w_id)
    {
    /* =========================================================
     * 1. 核心驻留& 复杂状态机 (这些由专属函数处理，这里做兜
     * ========================================================= */
    case COMP_NDL_STOP_1606:
    case COMP_COMPASS_1612:
    case COMP_TISSUE_GF_4012:
    case COMP_TISSUE_RAW_4012:
        /* 这些是包含动多元素的复杂状态机，已ui_update_task 有专属刷新逻辑 */
        break;

    case COMP_SYS_1606:
        comp_refresh_sys(DIRTY_BATT | DIRTY_TEMP);
        break;

    /* =========================================================
     * 2. 深度组件
     * ========================================================= */
    case COMP_DEPTH_1612:
    case COMP_DEPTH_1606:
        comp_set_value(w_id, g_sensor_data.depth);
        break;

    /* =========================================================
     * 3. 潜水时间（MM:SS 格式化）
     * ========================================================= */
    case COMP_DIVE_TIME_1606:
        snprintf(buf, sizeof(buf), "%02d:%02d",
                 g_sensor_data.dive_time_s / 60,
                 g_sensor_data.dive_time_s % 60);
        comp_set_text(w_id, buf);
        break;

    /* =========================================================
     * 4. 气体组件
     * ========================================================= */
    case COMP_GAS_1606:
        comp_set_text(w_id, g_sensor_data.gas_name);
        break;

    /* =========================================================
     * 5. 基础组件 (Basic)
     * ========================================================= */
    case COMP_TEMP_0806:
        comp_set_value(w_id, g_sensor_data.temperature_c);
        break;

    case COMP_TIME_1606:
        snprintf(buf, sizeof(buf), "%02d:%02d",
                 g_sensor_data.sys_time_h,
                 g_sensor_data.sys_time_m);
        comp_set_text(w_id, buf);
        break;

    case COMP_TTS_0806:
        comp_set_value(w_id, (float)g_sensor_data.tts);
        break;

    case COMP_ASCENT_0806:
    case COMP_ASCENT_0812:
        comp_set_value(w_id, g_sensor_data.ascent_rate);
        break;

    case COMP_BATTERY_0806:
        comp_set_value(w_id, g_sensor_data.battery_pct);
        break;

    case COMP_STOP_DEPTH_0806:
        comp_set_value(w_id, g_sensor_data.stop_depth_m);
        break;

    case COMP_STOP_TIME_1606:
        snprintf(buf, sizeof(buf), "%02d:%02d",
                 g_sensor_data.stop_time_left_s / 60,
                 g_sensor_data.stop_time_left_s % 60);
        comp_set_text(w_id, buf);
        break;

    case COMP_PPO2_0806:
        /* 根据激活气体索引选择对应PPO2 */
        comp_set_value(w_id, g_sensor_data.ppo2[g_sensor_data.gas_active_idx]);
        break;

    /* =========================================================
     * 6. 技术潜(Tech Dive)
     * ========================================================= */
    case COMP_SURF_GF_0806:
        comp_set_value(w_id, g_sensor_data.surf_gf);
        break;

    case COMP_GF99_0806:
        comp_set_value(w_id, g_sensor_data.gf99);
        break;

    case COMP_GF_0806:
        snprintf(buf, sizeof(buf), "%d/%d",
                 g_sensor_data.gf_low,
                 g_sensor_data.gf_high);
        comp_set_text(w_id, buf);
        break;

    case COMP_CNS_0806:
        comp_set_value(w_id, (float)g_sensor_data.cns_pct);
        break;

    case COMP_OTU_0806:
        comp_set_value(w_id, (float)g_sensor_data.otu);
        break;

    case COMP_MOD_0806:
        comp_set_value(w_id, g_sensor_data.mod_m);
        break;

    case COMP_CEILING_0806:
        comp_set_value(w_id, g_sensor_data.ceiling_m);
        break;

    case COMP_GAS_MIX_1606:
        snprintf(buf, sizeof(buf), "%d/%d",
                 g_sensor_data.gas_o2_pct,
                 g_sensor_data.gas_he_pct);
        comp_set_text(w_id, buf);
        break;

    case COMP_GAS_DENS_0806:
        comp_set_value(w_id, g_sensor_data.gas_density);
        break;

    case COMP_FIO2_0806:
        comp_set_value(w_id, g_sensor_data.fio2_pct);
        break;

    /* =========================================================
     * 7. 传感& 拓展 (Sensors)
     * ========================================================= */
    case COMP_HEADING_0806:
        comp_set_value(w_id, (float)g_sensor_data.heading);
        break;

    case COMP_POD_0806:
        /* POD 由状态机使用 user_data 靶向刷新，此处做兜底 */
        comp_set_value(COMP_POD_0806, g_sensor_data.pod1_bar);
        break;

    case COMP_DEPTH_MAX_0806:
        comp_set_value(w_id, g_sensor_data.max_depth);
        break;

    case COMP_DEPTH_AVG_0806:
        comp_set_value(w_id, g_sensor_data.avg_depth);
        break;

    case COMP_TEMP_MIN_0806:
        comp_set_value(w_id, g_sensor_data.min_temp);
        break;

    case COMP_TEMP_AVG_0806:
        comp_set_value(w_id, g_sensor_data.avg_temp);
        break;

    /* =========================================================
     * 8. ղλδ֪ ID
     * ========================================================= */
    case COMP_EMPTY:
    default:
        break;
    }
}
