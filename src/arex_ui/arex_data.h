#ifndef AREX_DATA_H
#define AREX_DATA_H

/* =========================================================
 * arex_ui_engine.h — 核心数据总线
 * 所有实时数据、配置结构体、气体表常量均已迁移至此。
 * ========================================================= */
#include "arex_ui_engine.h"

/* 历史轨迹推流接口（由 sim_tick_cb 每秒调用） */
void arex_dive_log_append(float current_time_s, float current_depth_m);

#endif /* AREX_DATA_H */
