#include "arex_ui_engine.h"

#include <math.h>
#include <stdio.h>

void arex_widget_set_value(arex_widget_id_t id, float value)
{
    uint8_t max_count = (g_card_custom_obj_count < AREX_MAX_CUSTOM_CARDS)
                        ? g_card_custom_obj_count
                        : AREX_MAX_CUSTOM_CARDS;

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

            if (id == WIDGET_DEPTH_1612 && child_tag == (uintptr_t)id)
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

            if (child_tag == (uintptr_t)WIDGET_POD_0806)
            {
                int16_t sub_cnt = lv_obj_get_child_cnt(child);
                for (int16_t j = 0; j < sub_cnt; j++)
                {
                    lv_obj_t *sub = lv_obj_get_child(child, j);
                    if (!sub) continue;
                    if ((uintptr_t)lv_obj_get_user_data(sub) == (uintptr_t)WIDGET_POD_0806)
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
                            if (id == WIDGET_TEMP_0806 || id == WIDGET_DEPTH_1606)
                            {
                                snprintf(buf, sizeof(buf), "%.1f", (double)value);
                            }
                            else if (id == WIDGET_PPO2_0806)
                            {
                                snprintf(buf, sizeof(buf), "%.2f", (double)value);
                            }
                            else if (id == WIDGET_BATTERY_0806)
                            {
                                snprintf(buf, sizeof(buf), "%.0f%%", (double)value);
                            }
                            else if (id == WIDGET_TTS_0806 || id == WIDGET_NDL_STOP_1606)
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

void arex_widget_set_text(arex_widget_id_t id, const char *text)
{
    if (!text) return;

    uint8_t max_count = (g_card_custom_obj_count < AREX_MAX_CUSTOM_CARDS)
                        ? g_card_custom_obj_count
                        : AREX_MAX_CUSTOM_CARDS;

    for (uint8_t c = 0; c <= max_count; c++)
    {
        lv_obj_t *container = (c < max_count) ? g_card_custom_objs[c] : g_left_anchor_obj;
        if (!container) continue;

        int16_t child_cnt = lv_obj_get_child_cnt(container);
        for (int16_t i = 0; i < child_cnt; i++)
        {
            lv_obj_t *child = lv_obj_get_child(container, i);
            if (!child) continue;

            if ((arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(child) == id)
            {
                int16_t sub_cnt = lv_obj_get_child_cnt(child);
                for (int16_t j = 0; j < sub_cnt; j++)
                {
                    lv_obj_t *sub = lv_obj_get_child(child, j);
                    if (!sub) continue;
                    if ((arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(sub) == id)
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
