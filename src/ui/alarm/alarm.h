/*
 * 文件: src/app_ui/ui/alarm/alarm.h
 * 作用: 该文件属于闹钟界面模块，负责闹钟数据、视图构建、交互刷新或与上层 UI 状态之间的衔接。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时建议先理解该文件在 app_ui 流程中的位置，再修改注释所描述的职责边界，避免把数据准备、状态切换和视图渲染职责混杂到一起。
 */

#ifndef ALARM_H
#define ALARM_H

#include "../core/ui_engine.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALARM_TARGET_MAX  12

/* 告警 ID 按 CRIT/WARN/INFO 分组，顺序会影响展示优先级和轮播行为。 */
typedef enum
{
    ALARM_ID_CRIT_ASCENT_RATE = 0,
    ALARM_ID_CRIT_PO2_MAX,
    ALARM_ID_CRIT_CEIL_BROKEN,
    ALARM_ID_CRIT_ALGO_LOCK,
    ALARM_ID_CRIT_TANK_EMPTY,
    ALARM_ID_CRIT_BATTERY_DEAD,

    ALARM_ID_WARN_PO2_ELEVATED,
    ALARM_ID_WARN_NDL_LOW,
    ALARM_ID_WARN_CNS_HIGH,
    ALARM_ID_WARN_OTU_HIGH,
    ALARM_ID_WARN_SAFETY_BROKEN,
    ALARM_ID_WARN_TANK_TURN,
    ALARM_ID_WARN_SIDEMOUNT_DIFF,
    ALARM_ID_WARN_DEPTH_LIMIT,
    ALARM_ID_WARN_TIME_LIMIT,
    ALARM_ID_WARN_BATTERY_LOW,
    ALARM_ID_WARN_POD_LOST,

    ALARM_ID_INFO_SAFETY_STOP,
    ALARM_ID_INFO_GAS_SWITCH,
    ALARM_ID_INFO_STOP_DONE,
    ALARM_ID_INFO_COMPASS_CALI,

    ALARM_ID_COUNT
} alarm_id_t;

typedef struct
{
    /* 这是闹钟模块对外暴露的当前展示快照，view 层只读使用。 */
    bool visible;
    alarm_level_t level;
    const char *text;
    comp_id_t banner_target;
    uint32_t revision;
} alarm_display_t;

void alarm_init(void);
/* active=true 表示条件触发，false 表示条件解除。 */
bool alarm_set_active(alarm_id_t id, bool active);
/* 自定义告警用于非固定告警表里的临时提示。 */
bool alarm_raise_custom(alarm_level_t level,
                             const char *text,
                             comp_id_t target);
void alarm_clear_all(void);
/* ACK 当前展示告警，用于支持用户手动确认隐藏。 */
bool alarm_ack_current(void);
/* 周期推进告警展示和轮播逻辑。 */
void alarm_tick(uint32_t now_ms);

const alarm_display_t *alarm_get_display(void);
/* 收集当前某一等级下需要高亮的组件 ID 列表。 */
uint8_t alarm_get_active_targets(alarm_level_t level,
                                      comp_id_t *targets,
                                      uint8_t max_targets);

#ifdef __cplusplus
}
#endif

#endif /* ALARM_H */
