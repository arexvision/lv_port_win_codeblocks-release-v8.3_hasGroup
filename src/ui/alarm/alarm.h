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
#define ALARM_VISIBLE_TARGET_MAX 64
#define ALARM_GAS_SWITCH_PROMPT_EXIT_DELTA_M 1.0f
#define ALARM_TARGET_MATCH_DEPTH_1612  0U  /* 深度告警是否同时命中 DEPTH_1612；未确认前关闭 */

/* 告警 ID 按 CRIT/WARN/INFO 分组，顺序会影响展示优先级和轮播行为。 */
typedef enum
{
    ALARM_ID_CRIT_ASCENT_RATE = 0,
    ALARM_ID_CRIT_PO2_MAX,
    ALARM_ID_CRIT_PO2_MIN,
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

typedef enum
{
    ALARM_TARGET_EFFECT_NONE = 0,
    ALARM_TARGET_EFFECT_CRIT_FLASH,
    ALARM_TARGET_EFFECT_WARN_BREATHE,
} alarm_target_effect_t;

typedef struct
{
    /* 这是闹钟模块对外暴露的当前展示快照，view 层只读使用。 */
    bool visible;
    alarm_level_t level;
    const char *text;
    comp_id_t banner_target;
    uint32_t revision;
} alarm_display_t;

typedef struct
{
    comp_id_t target;
    alarm_level_t level;
    alarm_target_effect_t effect;
} alarm_target_effect_entry_t;

void alarm_init(void);
/* active=true 表示条件触发，false 表示条件解除。 */
bool alarm_set_active(alarm_id_t id, bool active);
/* 自定义告警用于非固定告警表里的临时提示。 */
bool alarm_raise_custom(alarm_level_t level,
                             const char *text,
                             comp_id_t target);
bool alarm_clear_custom(void);
void alarm_clear_all(void);
/* 同步服务层确认状态；确认只影响视觉提示，不代表条件解除。 */
bool alarm_set_acknowledged(alarm_id_t id, bool acknowledged);
bool alarm_current_requires_ack(void);
/* 确认当前展示告警；INFO_GAS_SWITCH 会在这里提交自动切气请求。 */
bool alarm_confirm_current(void);
/* 兼容旧接口，内部等价于 alarm_confirm_current()。 */
bool alarm_ack_current(void);
bool alarm_display_is(alarm_id_t id);
/* 周期推进告警展示和轮播逻辑。 */
void alarm_tick(uint32_t now_ms);

const alarm_display_t *alarm_get_display(void);
void alarm_set_visible_targets(const comp_id_t *targets, uint8_t count);
uint8_t alarm_get_target_effects(alarm_target_effect_entry_t *entries, uint8_t max_entries);

#ifdef __cplusplus
}
#endif

#endif /* ALARM_H */
