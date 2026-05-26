#include "ui_engine.h"
#include "../screen/card_registry.h"
#include "../screen/screen.h"
#include "ui_state.h"
#include "../fonts/fonts.h"
#include "data.h"
#include "../alarm/alarm.h"
#include "../screen/layout_view.h"
#include "../comp/comp_style.h"
#include "../comp/comp_update.h"
#include "../comp/comp_view.h"
#include "update_router.h"
#include <stdio.h>
#include <string.h>

/* 气体名称(供全局引用) */
const char *GAS_NAMES[GAS_COUNT] =
{
    "AIR",
    "NX 32",
    "O2 100%",
    "GAS 4",
    "GAS 5"
};

/* 气体 MOD 表 (单位: 米) */
const uint8_t GAS_MOD_M[GAS_COUNT] =
{
    56,  /* AIR */
    34,  /* NX 32 */
    6,   /* O2 100% */
    0,   /* unused */
    0    /* unused */
};


/* =========================================================
 * 全局单例定义
 * ========================================================= */
sys_config_t  g_sys_config;
sensor_data_t g_sensor_data;  //注意这个是全局变量，所有UI层都要用它。因为赋值是原子操作，可以放心大胆用（不需要加锁）

/*当你写下 g_sensor_data.depth = 15.5f; 时，编译器会在底层把它翻译成
找到 g_sensor_data 的基地址 (0x20000000)
加上 depth 的偏移量 (+0)
直接生成一条单步汇编指令（STR），15.5 的二进制数据，像狙击枪一样，精准地打0x20000000 开始的 4 个字节里
它根本不会碰 heading，也不会battery这就是一次纯粹的、针对单32 位地址的“单指令写入”。因此，它是绝对原子的
*/


/* 左侧 2x7 绝对网格配置数组
 *
 * 160x420 区域 = 2 列(80px) x 7 行(60px)
 * 5F 卡片共用 comp_id_t 枚举体系
 *
 * Grid Layout:
 *   Row 0: NDL      | (占用 2x1 = 160x60)
 *   Row 1: DEPTH    | (占用 2x2 = 160x120, sudu 速率图标)
 *   Row 2: (DEPTH 第二行)
 *   Row 3: POD1     | POD2    (各占1x1 = 80x60)
 *   Row 4: TIME     | (占用 2x1 = 160x60)
 *   Row 5: GAS      | (占用 2x1 = 160x60)
 *   Row 6: SYS      | (占用 2x1 = 160x60，SystemData 可配置)
 * ========================================================= */
/* 左侧网格配置已迁移到 g_sys_config.left_widgets[] */

/* 5F 自定义网格配置已迁移到 g_sys_config.custom_cards[0].widgets[] */


/* KV 持久化存储加载配置（weak 实现由具体平台覆盖） */
/* =========================================================
 * 默认配置
 *
 * 当前实现的布局: Left Grid + Right Cards
 *   左侧: 160x420 固定 2x80) x 7y60) 网格
 *   右侧: tileview 滑动卡片 (INFO / 5F / DECO / COMPASS / GAS / PLAN / SETUP)
 *   安全 580x420 left_anchor(160) + right_cards(420) 组成
 *
 * 字段分组:
 *   [A] 活跃字段 当前渲染代码实际读取
 *   [R] 预留字段 已定义但渲染代码未使用，为未Classic 上下布局预留
 * ========================================================= */
void sys_config_defaults(sys_config_t *cfg)
{
    memset(cfg, 0, sizeof(sys_config_t));

    /* ========== [A] 安全========== */
    cfg->safe_zone_w  = 580;
    cfg->safe_zone_h  = 420;
    cfg->offset_x     = 0;            /* x=0 表示水平居中（左右各留白 3U*/
    cfg->offset_y     = -10;          /* y=-10 向上偏移（上面留2U，下面留4U*/

    /* ========== [A] 鏋舵瀯 ========== */
    cfg->layout_order  = ORDER_REVERSE;  /* 0=标准(左锚右卡)=翻转(右锚左卡) */
    cfg->dots_position = DOTS_LEFT;    /* tileview 指示点位*/
    cfg->compass_style = COMPASS_CLASSIC;
    cfg->mask_enabled  = false;

    /* ========== [R] 主题模式预留（当前固定为 Left Grid + Right Cards==========
     * 可选扩展为 Classic 上下流式布局，届时渲染代码需读取以下字段
     *   - theme_mode        THEME_CLASSIC
     *   - h_depth / h_ndl / h_pod / h_batt / h_gas / h_time   上下分区高度
     *   - sep_style / sep_thick                                分割线样
     *   - split_outward / flash_speed                          动画参数
     *   - title_h_u / h_menu_item / gap_menu                  菜单排版
     *   - h_tissues_chart                                     组织图高
     */
    cfg->theme_mode    = THEME_TECH;    /* 当前固定 TECH（Left Grid + Right Cards*/
    cfg->sep_style     = SEP_DASHED;    /* [R] 分割线样式（待用*/
    cfg->sep_thick     = 2;                  /* [R] 线条粗细 px（待用） */
    cfg->split_outward = true;               /* [R] 双拼模块展开方向（待用） */
    cfg->flash_speed   = 1;                  /* [R] 动画闪烁速度（待用） */

    /* ========== [A] 分割线透明========== */
    cfg->sep_alpha  = 51;   /* 20% of 255 SystemData 顶部分割线透明*/

    /* ========== [R] Classic 上下布局 10U 高度分配 (当前未使 ==========
     * 1U = 10px，总计 10U = 100px（预留将来改为上下分区流式布局
     * DEPTH 大通栏 NDL/TTS 双拼 POD 双拼 BATT 双拼 GAS DIVE TIME
     */
    cfg->h_depth         = 8;   /* DEPTH 大通栏: 8U=80px */
    cfg->h_ndl           = 6;   /* NDL/TTS 鍙屾嫾: 6U=60px */
    cfg->h_pod           = 6;   /* POD 1/2 鍙屾嫾: 6U=60px */
    cfg->h_batt          = 5;   /* BATT/W.TIME 鍙屾嫾: 5U=50px */
    cfg->h_gas           = 6;   /* GAS 中通栏: 6U=60px */
    cfg->h_time          = 5;   /* DIVE TIME 搴曢儴: 5U=50px */
    cfg->title_h_u       = 2;   /* [R] 标题高度（待用） */
    cfg->h_menu_item     = 5;   /* [R] 菜单项高度（待用*/
    cfg->gap_menu        = 1;   /* [R] 菜单项间距（待用*/
    cfg->h_tissues_chart = 9;   /* [R] 组织柱图高度（待用） */

    /* ========== [A] 面板间距 ========== */
    cfg->gap_u       = 0;   /* 左侧锚点与右侧面板间 0U=0px（由 sep_thick 负责分割线粗细） */
    cfg->panel_gap_u = 1;   /* tileview 容器间距: 1U=10px */

    /* ========== [A] 5F 自定义网格 (5列 x 6行) ==========
     *
     *  5列布局示意（10格，6行）
     *  col:  0  1  2  3  4
     *  row0: [DEPTH 2x2 大块     ] [TEMP  ] [HEADING 2x1]
     *  row2: [空槽      ]          [BATT   ] [PPO2 1x1]
     *  row3: [NDL 2x1           ] [TTS 2x1 ] [CNS  1x1 ]
     *  row4: [POD1              ] [POD2    ] [绌烘Ы   ]
     *  row5: [绌烘Ы               ] [绌烘Ы    ] [绌烘Ы   ]
     *
     *  简洁位置配置：widget_id + x/y 三字段，span_w/h MCU 样式表自动推
     */
    /* 兼容新架 使用 custom_cards[0] 存储单张卡片的配*/
    cfg->custom_card_count = 1;
    cfg->custom_cards[0].widget_count = 12;
    cfg->custom_cards[0].widgets[0]  = (grid_widget_t)
    {
        COMP_DEPTH_1612,      0, 0
    };
    cfg->custom_cards[0].widgets[1]  = (grid_widget_t)
    {
        COMP_DEPTH_1612,      2, 0
    };
    cfg->custom_cards[0].widgets[2]  = (grid_widget_t)
    {
        COMP_HEADING_0806,   4, 0
    };
    cfg->custom_cards[0].widgets[3]  = (grid_widget_t)
    {
        COMP_EMPTY,           0, 2
    };  /* SAC 已移*/
    cfg->custom_cards[0].widgets[4]  = (grid_widget_t)
    {
        COMP_BATTERY_0806,   2, 2
    };
    cfg->custom_cards[0].widgets[5]  = (grid_widget_t)
    {
        COMP_PPO2_0806,       4, 2
    };
    cfg->custom_cards[0].widgets[6]  = (grid_widget_t)
    {
        COMP_NDL_STOP_1606,  0, 3
    };
    cfg->custom_cards[0].widgets[7]  = (grid_widget_t)
    {
        COMP_TTS_0806,       2, 3
    };
    cfg->custom_cards[0].widgets[8]  = (grid_widget_t)
    {
        COMP_CNS_0806,       4, 3
    };
    cfg->custom_cards[0].widgets[9]  = (grid_widget_t)
    {
        COMP_DEPTH_1612,       0, 4
    };
    cfg->custom_cards[0].widgets[10] = (grid_widget_t)
    {
        COMP_DEPTH_1612,       2, 4
    };

    /* ========== [A] 左侧 2x7 固定网格 (160x420) ==========
     * 160x420 区域 = 280px) x 760px)，由 render_left_anchor_grid() 渲染
     *
     *  Grid Layout:
     *    Row 0: NDL      | (2x1 160x60)
     *    Row 1-2: DEPTH  | (2x2 160x120，带 sudu 速率图标)
     *    Row 3: POD1     | POD2    (1x1 80x60)
     *    Row 4: TIME     | (2x1 160x60)
     *    Row 5: GAS      | (2x1 160x60)
     *    Row 6: SYS      | (2x1 160x60，SystemData 可配
     */
    /* 简洁位置配置：widget_id + x/y，span_w/h MCU 样式表自动推*/
    cfg->left_widgets[0] = (grid_widget_t)
    {
        COMP_NDL_STOP_1606,   0, 0
    };
    cfg->left_widgets[1] = (grid_widget_t)
    {
        COMP_DEPTH_1612,      0, 1
    };
    cfg->left_widgets[2] = (grid_widget_t)
    {
        COMP_DIVE_TIME_1606,  0, 3
    };  /* 潜水时间 */
    cfg->left_widgets[3] = (grid_widget_t)
    {
        COMP_GAS_1606,        0, 4
    };
    /* Ĭϲȹر POD1/POD2 ʾλӰ߶ */
    cfg->left_widgets[4] = (grid_widget_t)
    {
        COMP_EMPTY,           0, 5
    };
    cfg->left_widgets[5] = (grid_widget_t)
    {
        COMP_EMPTY,           1, 5
    };
    cfg->left_widgets[6] = (grid_widget_t)
    {
        COMP_SYS_1606,        0, 6
    };

    /* 动态计算实widget 数量（以最后一个非widget 为准*/
    cfg->left_widget_count = 0;
    for (int i = 0; i < LEFT_MAX_WIDGETS; i++)
    {
        if (cfg->left_widgets[i].widget_id != 0)
        {
            cfg->left_widget_count = i + 1;
        }
    }

    /* ========== [A] 右侧卡片顺序 (tileview 滑动顺序) ==========
     * card_order[pos] = card_id
     * INFO(0) 固定，SETUP(13) 固定，中12 张可APP 重排
     * 必须初始化所14 个位置！
     * CARD_ID_UNUSED(0xFF)=δռòλʾdot, CARD_ID_BLANK=հ׿ҲЧƬӦʾdot
     */
    memset(cfg->card_order, CARD_ID_UNUSED, sizeof(cfg->card_order));
    cfg->card_order[CARD_POS_INFO]   = CARD_ID_INFO;//菜单，不算卡
    cfg->card_order[CARD_POS_1]      = CARD_ID_COMPASS;
    cfg->card_order[CARD_POS_2]      = CARD_ID_DECO;
    cfg->card_order[CARD_POS_3]      = CARD_ID_PLAN;
    cfg->card_order[CARD_POS_4]      = CARD_ID_GAS;
    cfg->card_order[CARD_POS_5]      = CARD_ID_CUSTOM_GRID;
    /* CARD_POS_7 ~ CARD_POS_12 淇濇寔 CARD_ID_BLANK */
    cfg->card_order[CARD_POS_SETUP]  = CARD_ID_SETUP;//菜单，不算卡

    /* ========== [A] 卡片槽位映射 ==========
     * custom_card_slot[pos] = custom_card_index (0~11)
     * pos Ӧ card_order еĶ̬λ
     * ĬϣһCUSTOM_GRID Ƭӳcustom_cards[0]
     */
    memset(cfg->custom_card_slot, 0xFF, sizeof(cfg->custom_card_slot));
    cfg->custom_card_slot[CARD_POS_5] = 0;  /* CUSTOM_GRID 映射custom_cards[0] */

    /* ========== [A] 用户设置默认========== */
    cfg->mod_ppo2       = 1.4f;
    cfg->conservatism   = CONSERVATISM_MED;
    cfg->salinity_mode   = 0;    /* FRESH */
    cfg->last_deco_stop_m = 3;
    cfg->brightness     = BRIGHTNESS_ECO;
}

/* =========================================================
 * LVGL 样式辅助
 * ========================================================= */

/* ALIGN_* 转换LVGL 对齐方式 */
lv_text_align_t align_to_lv(uint8_t align)
{
    if (align == ALIGN_LEFT)   return LV_TEXT_ALIGN_LEFT;
    if (align == ALIGN_CENTER) return LV_TEXT_ALIGN_CENTER;
    return LV_TEXT_ALIGN_RIGHT;
}

/* 将对齐转换为 LVGL ALIGN 常量 */
lv_align_t align_to_lv_align(uint8_t align)
{
    if (align == ALIGN_LEFT)   return LV_ALIGN_LEFT_MID;
    if (align == ALIGN_CENTER) return LV_ALIGN_CENTER;
    return LV_ALIGN_RIGHT_MID;
}

/* =========================================================
 * 字体映射(Font Mapper)
 *
 * 全系统唯一允许将字ID 转换为真lvgl 字体指针的地方
 * 所有配置结构体中保存的 title_font / val_font 均应font_id_t 值
 *
 * ID 映射表：
 *   FONT_ID_SMALL  (0) 20px  标签/单位/Badge
 *   FONT_ID_TITLE  (1) 20px  菜单卡片标题
 *   FONT_ID_MEDIUM (2) 32px  数据
 *   FONT_ID_LARGE  (3) 64px  深度大数
 *   FONT_ID_HUGE   (4) 64px  大字
 *   FONT_ID_NDL    (5) 48px  NDL减压时间
 * ========================================================= */
const lv_font_t *get_font(uint8_t font_id)
{
    switch (font_id)
    {
    case FONT_ID_SMALL:
        return FONT_SMALL;   /* 20px */
    case FONT_ID_TITLE:
        return FONT_TITLE;   /* 20px */
    case FONT_ID_MEDIUM:
        return FONT_MEDIUM;  /* 32px */
    case FONT_ID_LARGE:
        return FONT_LARGE;   /* 64px */
    case FONT_ID_HUGE:
        return FONT_HUGE;    /* 64px */
    case FONT_ID_NDL:
        return FONT_NDL;     /* 48px */
    default:
        return FONT_SMALL;   /* 兜底：永不为 NULL */
    }
}

/* =========================================================
 * JSON 配置解析 (用于 App 蓝牙同步 / SETUP 导入)
 * ========================================================= */
/*
 * 当接收到 JSON 配置时，按以下流程处理：
 *
 * 1. 解析 JSON 到临时结构体
 * 2. 调用 memcpy(&g_sys_config, &tmp, sizeof(...)) 覆盖
 * 3. 调用 ui_apply_config() 重排 UI
 *
 * JSON 字段示例 (HTML configIds 对应):
 * {
 *   "theme_mode": "tech",
 *   "safe_zone_w": 580,
 *   "h_depth": 8,
 *   "h_ndl": 6,
 *   "widget_ids": [0, 1, 2, 3, 4, 5],
 *   "widget_w":  [2, 2, 1, 2, 2, 1],
 *   "widget_h":  [2, 1, 1, 2, 1, 1],
 *   ...
 * }
 */

/* =========================================================
 * 初始化入(UI_main 调用)
 *
 * 启动流程
 *   1. KV 持久化读取配成功则直接使
 *   2. KV 无数读取失败 填入默认
 *   3. 传感器数据清零（sim_tick_cb / 外部 API 实时写入
 * ========================================================= */
void ui_init(void)
{
    /* 1. 加载持久化配置，失败则用默认值保*/
    if (!config_load(&g_sys_config))
    {
        sys_config_defaults(&g_sys_config);
    }

    /* 2. 传感器数据清*/
    data_init();
}

/* =========================================================
 * 应用配置变更 (配置界面修改后调用此函数)
 * 1. 检safe zone 边界
 * 2. 重建 left anchor 排版
 * 3. 重建 right card 布局
 * ========================================================= */
void ui_apply_config(void)
{
    /* 安全区边界校*/
    if (safe_zone_in_danger())
    {
        /* TODO: 触发危险警告 UI */
    }

    /* TODO: 触发 screen_rebuild_layout() 重建排版
     * 这将在重screen.c 时实
     */
}

/* =========================================================
 * 卡片顺序查询 (统一入口 替代旧的 card_order 全局访问)
 * ========================================================= */
uint8_t g_sys_card_order(uint8_t pos)
{
    if (pos >= CARD_COUNT) return 0;
    return g_sys_config.card_order[pos];
}

/* =========================================================
 * 5F 自定义网格组件外部容器（screen.c 注入
 * ========================================================= */
lv_obj_t *g_left_anchor_obj = NULL;
/* 多张自定义卡片容器数*/
lv_obj_t *g_card_custom_objs[MAX_CUSTOM_CARDS];
uint8_t   g_card_custom_obj_count;


/* =========================================================
 * 全局组件数据路由分发器
 *
 * 架构设计：此函数作为数据路由总机，根据 widget_id 自动从 g_sensor_data
 * 取值并调用 comp_set_value() 或 comp_set_text() 刷新界面。
 *
 * 使用场景
 *   - screen_refresh_all_widgets() 遍历全量 widget 调用此函数
 *   - update 任务在收到 DIRTY_ALL 时调用全量刷新
 *   - 任何需要单独刷新某个组件数据的场景
 *
 * 注意：复杂状态机组件（NDL_STOP/SYS/COMPASS/TISSUE）已在 update_task
 *       有专属刷新逻辑，此处仅做兜底处理
 * ========================================================= */

/* =========================================================
 * 🚨 靶向告警触发引擎（新版本：仅设置状态，50ms 定时器执行闪烁）
 * ========================================================= */
void trigger_alarm(alarm_level_t level,
                        const char *eng_text,
                        comp_id_t target_id)
{
    (void)alarm_raise_custom(level, eng_text, target_id);
    g_ui.alarm_pending_click = (level >= ALARM_WARN);
}

/* =========================================================
 * 🚨 清除所有告警样式（50ms 定时器会自动把样式复原）
 * 新逻辑：速度降到安全范围后自动清除，但最少显示 5 秒
 * ========================================================= */
void clear_all_alarm_styles(void)
{
    alarm_clear_all();
    g_ui.alarm_pending_click = false;
}

bool alarm_mark_clear_requested(void)
{
    return alarm_ack_current();
}



/* =========================================================
 * 定时数据更新 (lv_timer 1Hz/2Hz 调用)
 * 仅更lv_label 文字，绝不触发排版重
 * ========================================================= */
void ui_update_data(void)
{
    /* 由调用方screen.c 中实现具体的 lv_label_set_text 调用
     * 此函数作为空钩子存在，供未来扩展
     */
}

/* =========================================================
 * 11. Data Bus UI 消费任务 全系统唯一允许执行 lv_label_set_text 的地
 *
 * 架构铁律
 *   - 硬件工程师：只能调用 bus_set_*() 系列函数（仅写数打脏标记
 *   - 两者通过 g_sensor_data.dirty_mask 完全解
 *
 * lv_timer 驱动，建50ms 周期0 FPS 足够覆盖所有传感器变化
 * ========================================================= */
void ui_update_task(lv_timer_t *timer)
{
    (void)timer;

    ui_update_router_tick();

    uint32_t mask = bus_take_dirty();
    if (mask == DIRTY_NONE)
    {
        return;
    }

    ui_update_router_dispatch(mask);
}
