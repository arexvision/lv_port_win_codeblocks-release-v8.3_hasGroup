/*
 * 文件: src/app_ui/ui/core/ui_engine.c
 * 作用: 该文件属于 UI 核心模块，负责状态机、数据桥接、事件分发、更新调度或 UI 运行时公共定义。
 * 说明: 本文件位于 app_ui 目录下，主要服务于潜水电脑前端界面的构建、刷新与交互流程；阅读时建议结合同目录下的 .h/.c 配对文件、上层状态机入口以及页面注册关系一起理解。
 * 维护: 维护时需要同时关注 UI 状态机、LVGL 对象生命周期以及跨模块回调关系，避免只改显示层而忽略状态同步、对象释放或重建后的引用有效性。
 */

#include "ui_engine.h"
#include "../screen/page_registry.h"
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
#include "../alarm/alarm_view.h"
#include <stdio.h>
#include "../views/submenu_dive_plan_state.h"
#include "../../ui_test/ui_test.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern void ui_test_poll_runtime_control(void) __attribute__((weak));

__attribute__((weak)) void app_ui_perf_note_dirty_mask(uint32_t mask)
{
    (void)mask;
}

__attribute__((weak)) void app_ui_perf_note_router_cost(uint32_t total_ms,
                                                        uint32_t deco_ms,
                                                        uint32_t ndl_ms,
                                                        uint32_t plan_ms,
                                                        uint32_t widget_ms,
                                                        uint32_t info_ms,
                                                        uint32_t mask)
{
    (void)total_ms;
    (void)deco_ms;
    (void)ndl_ms;
    (void)plan_ms;
    (void)widget_ms;
    (void)info_ms;
    (void)mask;
}

__attribute__((weak)) void app_ui_perf_get_context(uint8_t *state,
                                                   uint8_t *dash_page,
                                                   uint8_t *page_id,
                                                   uint8_t *storage_pos)
{
    if (state)
    {
        *state = (uint8_t)ui_state_get_state();
    }
    if (dash_page)
    {
        *dash_page = ui_state_get_dash_page();
    }
    if (page_id)
    {
        *page_id = page_id_at(ui_state_get_dash_page());
    }
    if (storage_pos)
    {
        *storage_pos = page_storage_pos(ui_state_get_dash_page());
    }
}

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


/* 固定栏实际坐标配置数组
 *
 * 160x420 区域 = 2 列(80px) x 7 行(60px)
 * 自定义卡片共用 comp_id_t 枚举体系
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
/* 固定栏网格配置已迁移到 g_sys_config.left_widgets[]，字段名保留旧协议兼容 */

/* 自定义网格配置已迁移到 g_sys_config.custom_cards[0].widgets[] */


/* KV 持久化存储加载配置（weak 实现由具体平台覆盖） */
/* =========================================================
 * 默认配置
 *
 * 当前实现的布局: Left Grid + Right Cards
 *   固定栏: side=2x7，top/bottom=7x2
 *   内容区: tileview 滑动卡片 (INFO / CUSTOM / DECO / COMPASS / GAS / PLAN / SETUP)
 *   安全 580x420 left_anchor(160) + right_cards(420) 组成
 *
 * 字段分组:
 *   [A] 活跃字段 当前渲染代码实际读取
 *   [R] 预留字段 已定义但渲染代码未使用，为未Classic 上下布局预留
 * ========================================================= */
void sys_config_defaults(sys_config_t *cfg)
{
    /* 先整体清零，再写入默认值，避免旧配置残留影响布局和菜单行为。 */
    memset(cfg, 0, sizeof(sys_config_t));

    /* ========== [A] 安全========== */
    /* 安全区尺寸决定了后续所有布局计算的基准尺寸。 */
    cfg->safe_zone_w  = 580;
    cfg->safe_zone_h  = 420;
    cfg->offset_x     = 0;            /* side 布局水平居中：左右各留白 3U */
    cfg->offset_y     = -10;          /* y=-10 向上偏移：上留白 2U，下留白 4U */

    /* ========== [A] 鏋舵瀯 ========== */
    cfg->layout_order  = ORDER_REVERSE;  /* side 默认：固定栏在右，内容区在左 */
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
    cfg->gap_u       = 0;   /* 组件块内部间距预留 */
    cfg->panel_gap_u = 2;   /* 固定栏与内容区内部 GAP: 2U=20px */

    /* ========== [A] 自定义网格：默认展示全部组件模块 ========== */
    static const char *module_titles[] =
    {
        "CORE LARGE", "DECO LIMITS", "DIVE STATS", "SENSORS", "TISSUES"
    };
    static const uint8_t module_counts[] = { 14U, 15U, 12U, 15U, 2U };
    static const grid_widget_t module_cards[][MAX_5F_WIDGETS] =
    {
        {
            { COMP_DEPTH_1612, 0, 0 }, { COMP_COMPASS_1612, 2, 0 }, { COMP_ASCENT_0812, 4, 0 },
            { COMP_NDL_STOP_1606, 0, 2 }, { COMP_DIVE_TIME_1606, 2, 2 }, { COMP_TEMP_0806, 4, 2 },
            { COMP_DEPTH_1606, 0, 3 }, { COMP_GAS_1606, 2, 3 }, { COMP_BATTERY_0806, 4, 3 },
            { COMP_TIME_1606, 0, 4 }, { COMP_SYS_1606, 2, 4 }, { COMP_ASCENT_0806, 4, 4 },
            { COMP_POD_0806, 0, 5 }, { COMP_HEADING_0806, 1, 5 },
        },
        {
            { COMP_STOP_TIME_1606, 0, 0 }, { COMP_GAS_MIX_1606, 2, 0 }, { COMP_STOP_DEPTH_0806, 4, 0 },
            { COMP_TTS_0806, 0, 1 }, { COMP_PPO2_0806, 1, 1 }, { COMP_SURF_GF_0806, 2, 1 },
            { COMP_GF99_0806, 3, 1 }, { COMP_CNS_0806, 4, 1 }, { COMP_OTU_0806, 0, 2 },
            { COMP_GF_0806, 1, 2 }, { COMP_MOD_0806, 2, 2 }, { COMP_CEILING_0806, 3, 2 },
            { COMP_GAS_DENS_0806, 4, 2 }, { COMP_FIO2_0806, 0, 3 }, { COMP_NOFLY_0806, 1, 3 },
        },
        {
            { COMP_DEPTH_MAX_0806, 0, 0 }, { COMP_DEPTH_AVG_0806, 1, 0 }, { COMP_TEMP_MIN_0806, 2, 0 },
            { COMP_TEMP_AVG_0806, 3, 0 }, { COMP_TTS_AT_5MIN_0806, 4, 0 }, { COMP_TTS_DELTA_5MIN_0806, 0, 1 },
            { COMP_NDL_UP_3M_0806, 1, 1 }, { COMP_NDL_DOWN_3M_0806, 2, 1 }, { COMP_NDL_DELTA_3M_0806, 3, 1 },
            { COMP_GTR_0806, 4, 1 }, { COMP_RMV_0806, 0, 2 }, { COMP_SAC_0806, 1, 2 },
        },
        {
            { COMP_ACCEL_2406, 0, 0 }, { COMP_BATT_V_0806, 3, 0 }, { COMP_PRESSURE_0806, 4, 0 },
            { COMP_GYRO_2406, 0, 1 }, { COMP_CPU_0806, 3, 1 }, { COMP_FPS_0806, 4, 1 },
            { COMP_MAG_2406, 0, 2 }, { COMP_BLE_RSSI_0806, 3, 2 }, { COMP_CHARGE_0806, 4, 2 },
            { COMP_MLX_2406, 0, 3 }, { COMP_BATT_TEMP_0806, 3, 3 }, { COMP_PRJ_TEMP_0806, 4, 3 },
            { COMP_TMAG_2406, 0, 4 }, { COMP_SENSOR_STAT_1606, 3, 4 }, { COMP_ATTITUDE_2406, 0, 5 },
        },
        {
            { COMP_TISSUE_GF_4012, 0, 0 }, { COMP_TISSUE_RAW_4012, 0, 2 },
        },
    };

    (void)memset(cfg->custom_cards, 0, sizeof(cfg->custom_cards));
    cfg->custom_card_count = (uint8_t)(sizeof(module_counts) / sizeof(module_counts[0]));
    for (uint8_t card_idx = 0U; card_idx < cfg->custom_card_count; card_idx++)
    {
        cfg->custom_cards[card_idx].widget_count = module_counts[card_idx];
        (void)snprintf(cfg->custom_cards[card_idx].title, sizeof(cfg->custom_cards[card_idx].title), "%s", module_titles[card_idx]);
        for (uint8_t i = 0U; i < module_counts[card_idx]; i++) cfg->custom_cards[card_idx].widgets[i] = module_cards[card_idx][i];
    }
    /* ========== [A] 默认 side 固定栏 2x7 网格 (160x420) ==========
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
    /* 简洁位置配置：widget_id + 当前方向实际 x/y，span_w/h MCU 样式表自动推 */
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
     * card_order[pos] = page_id
     * INFO(0) 固定，SETUP(13) 固定，中12 张可APP 重排
     * 必须初始化所14 个位置！
     * PAGE_ID_UNUSED(0xFF)=不显示且不占 dot，PAGE_ID_BLANK=空白有效卡片且显示 dot
     */
    memset(cfg->card_order, PAGE_ID_UNUSED, sizeof(cfg->card_order));
    cfg->card_order[PAGE_POS_INFO]   = PAGE_ID_INFO;//菜单，不算卡
    cfg->card_order[PAGE_POS_1]      = PAGE_ID_BLANK;
    cfg->card_order[PAGE_POS_2]      = PAGE_ID_COMPASS;
    cfg->card_order[PAGE_POS_3]      = PAGE_ID_DECO;
    cfg->card_order[PAGE_POS_4]      = PAGE_ID_PLAN;
    cfg->card_order[PAGE_POS_5]      = PAGE_ID_GAS;
    for (uint8_t pos = PAGE_POS_6; pos < PAGE_POS_6 + cfg->custom_card_count; pos++) cfg->card_order[pos] = PAGE_ID_CUSTOM_GRID;
    cfg->card_order[PAGE_POS_SETUP]  = PAGE_ID_SETUP;//菜单，不算卡

    /* ========== [A] 卡片槽位映射 ==========
     * custom_card_slot[pos] = custom_card_index (0~11)
     * pos Ӧ card_order еĶ̬λ
     * ĬϣһCUSTOM_GRID Ƭӳcustom_cards[0]
     */
    memset(cfg->custom_card_slot, 0xFF, sizeof(cfg->custom_card_slot));
    for (uint8_t pos = PAGE_POS_6; pos < PAGE_POS_6 + cfg->custom_card_count; pos++) cfg->custom_card_slot[pos] = (uint8_t)(pos - PAGE_POS_6);

    /* ========== [A] 用户设置默认========== */
    cfg->mod_ppo2       = 1.4f;
    cfg->conservatism   = CONSERVATISM_MED;
    cfg->salinity_mode   = 0;    /* FRESH */
    cfg->last_deco_stop_m = 3;
    cfg->brightness     = BRIGHTNESS_MED;
    cfg->log_rate_s     = UI_LOG_RATE_DEFAULT_S;
    cfg->time_24h_enabled = 1U;
    cfg->units_mode = UI_UNITS_DEFAULT;
    cfg->date_format = 1U;
    cfg->temperature_unit = UI_TEMP_UNIT_DEFAULT;
    cfg->safety_stop_mode = UI_SAFETY_STOP_DEFAULT;
    cfg->surface_confirm_min = UI_SURFACE_CONFIRM_DEFAULT_MIN;
    cfg->altitude_level = 0;
    cfg->depth_alarm_m = 40;
    cfg->time_alarm_min = 60;
    cfg->ndl_alarm_min = 5;
    cfg->dive_start_depth_m = UI_DIVE_START_DEPTH_DEFAULT_M;
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
 *   1. 布局/参数恢复由 bootstrap + service 层先完成
 *   2. UI core 只兜底默认值，不再直接参与配置持久化
 *   3. 传感器数据清零（sim_tick_cb / 外部 API 实时写入）
 * ========================================================= */
void ui_init(void)
{
    /* 1. UI core 不再自行读取持久化配置。
     * 持久化恢复已在 system bootstrap 阶段完成，这里只做“是否已有有效配置”的兜底判断。 */
    if (g_sys_config.safe_zone_w == 0U || g_sys_config.safe_zone_h == 0U)
    {
        sys_config_defaults(&g_sys_config);
    }

    /* 2. 传感器数据和告警状态初始化 */
    data_init();
    alarm_init();
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
 * 右侧页面顺序查询 (统一入口，替代直接访问 card_order)
 * ========================================================= */
uint8_t g_sys_page_order(uint8_t pos)
{
    if (pos >= PAGE_COUNT) return 0;
    return g_sys_config.card_order[pos];
}

/* =========================================================
 * 自定义网格组件外部容器（screen.c 注入
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
 *   - update router 在收到 DIRTY_WIDGET_REFRESH_MASK 时按订阅 dirty 刷新当前布局组件
 *   - 任何需要单独刷新某个组件数据的场景
 *
 * 注意：复杂状态机组件（NDL_STOP/SYS/COMPASS/TISSUE）已在 update_task
 *       有专属刷新逻辑，此处仅做兜底处理
 * ========================================================= */

bool alarm_mark_clear_requested(void)
{
    /* 告警确认已经从普通 key 输入中解耦，普通 click/back 不再吞输入。
     * PC 调试的 BACK 长按由 debug_link_pc 直接调用 alarm_confirm_current()。 */
    return false;
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

void ui_update_flush_pending_once(void)
{
    /*
     * 输入事件已经在 display task 的 LVGL 持锁窗口内消费。这里不推进告警闪烁、
     * 异步菜单和其它周期状态机，只把“切页/点击刚产生的 pending navigation 与
     * dirty mask”立即落到当前可见对象，避免再等 100ms UI timer 才补齐页面首帧。
     */
    ui_state_poll_deferred_navigation();
    screen_poll_deferred_page_dirty();

    dirty_mask_t mask = bus_take_dirty();
    if (mask == DIRTY_NONE)
    {
        return;
    }

    app_ui_perf_note_dirty_mask((uint32_t)mask);
    ui_update_router_dispatch(mask);
}

/* =========================================================
 * 11. Data Bus UI 消费任务 全系统唯一允许执行 lv_label_set_text 的地
 *
 * 架构铁律
 *   - 硬件工程师：只能调用 bus_set_*() 系列函数（仅写数打脏标记
 *   - 两者通过 g_sensor_data.dirty_mask 完全解
 *
 * lv_timer 驱动，周期跟随 UI_DATA_FETCH_TASK_DELAY_MS。
 * 这个任务只消费脏标记和推进轻量状态，不应靠高频空转追传感器。
 * ========================================================= */
void ui_update_task(lv_timer_t *timer)
{
    (void)timer;

    if (ui_test_poll_runtime_control)
    {
        ui_test_poll_runtime_control();
    }

    {
        alarm_view_context_t ctx;
        ctx.safe_zone = get_safe_zone();
        ctx.left_anchor = g_left_anchor_obj;
        ctx.custom_cards = g_card_custom_objs;
        ctx.custom_card_count = g_card_custom_obj_count;
        ctx.max_custom_cards = MAX_CUSTOM_CARDS;
        ctx.layout_order = ui_layout_order_get();
        ctx.vertical_split = ui_layout_is_vertical_split();
        ctx.safe_zone_w = ui_safe_zone_w_get();
        ctx.left_anchor_w = ui_anchor_w_get();
        ctx.panel_gap_px = ui_panel_gap_px_get();
        ctx.content_w = ui_content_w_get();
        ctx.content_h = ui_content_h_get();
        if (ui_layout_is_vertical_split())
        {
            ctx.content_x = (ui_layout_order_get() == ORDER_NORMAL)
                            ? (uint16_t)(ui_anchor_w_get() + ui_panel_gap_px_get())
                            : 0U;
            ctx.content_y = 0U;
        }
        else
        {
            ctx.content_x = 0U;
            ctx.content_y = (ui_layout_order_get() == ORDER_NORMAL)
                            ? (uint16_t)(ui_anchor_h_get() + ui_panel_gap_px_get())
                            : 0U;
        }
        ctx.alarm_pending_click = ui_state_get_alarm_pending_click();
        alarm_view_tick(&ctx);
    }

    {
        static compass_cal_ui_state_t s_last_compass_cal_state = COMPASS_CAL_IDLE;
        compass_cal_ui_state_t cal_state = get_compass_calibration_ui_state();
        if (cal_state != s_last_compass_cal_state)
        {
            s_last_compass_cal_state = cal_state;
            screen_refresh_setup_menu();
        }
    }

    {
        static bool s_last_flash_state = false;
        bool current_flash_state = (lv_tick_get() / 500U) % 2U == 0U;
        if (current_flash_state != s_last_flash_state)
        {
            s_last_flash_state = current_flash_state;
            if (fabsf(bus_get_ascent_rate()) > RATE_STILL_THRESHOLD)
            {
                bus_requeue_dirty(DIRTY_DIVE_PROFILE);
            }
        }
    }

    if (submenu_dive_plan_poll_async())
    {
        screen_refresh_info_submenu_if_open();
    }

    ui_update_flush_pending_once();
}
