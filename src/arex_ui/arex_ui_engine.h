#ifndef AREX_UI_ENGINE_H
#define AREX_UI_ENGINE_H

#include "lvgl/lvgl.h"
#include <stdint.h>
#include <stdbool.h>
#include "arex_card_registry.h"

#ifdef __cplusplus
extern "C" {
#endif


/* =========================================================
 * 系统核心宏定义
 * ========================================================= */
/* 上电时所有数据区显示 "--" 占位符，等 sim_tick_cb 首次触发后才显示真实数据。
 * 设为 0 恢复原有行为（初始值直接显示）。 */
#define AREX_SHOW_PLACEHOLDER_ON_INIT  1/*只是用来模拟器使用的，实际项目也可以用来防抖，上电会显示--，传感器数据来了变为数据*/
#define AREX_BASE_U             10   /* 物理基准单位 1U = 10px */
#define AREX_MIN_CLASSIC_TOP_H  200  /* Classic 模式下最小上区高度 px */
#define AREX_MASK_EDGE_GUARD    80   /* 面镜盲区掩膜底部警戒阈值 px */
#define AREX_PHYSICAL_W    640  /* 硬件屏幕极限宽 */
#define AREX_PHYSICAL_H    480  /* 硬件屏幕极限高 */
#define AREX_LEFT_ANCHOR_W  160  /* 左侧锚点固定宽度 */
/* 右侧卡片全局标题区高度分配（统一控制标题文字+分割线占用高度） */
#define AREX_CARD_TITLE_H  60  /* 标题区高度：文字(Y=8) + 分隔线(Y=48) + 下方留白，视觉底边焊死 Y=48 */

/* 减压数据刷新节流宏：只允许每 N ms 刷新一次，避免高频深度变化时 UI 负载过高 */
#define AREX_DECO_REFRESH_MS   1000   /* 减压跟踪刷新间隔（ms），设为 0 则关闭节流（每次都刷新） */

/* =========================================================
 * 颜色宏 (统一集中管理)
 * ========================================================= */
#define AREX_GREEN   lv_color_make(0x00, 0xFF, 0x00)
#define AREX_LIGHT   lv_color_make(0x55, 0xFF, 0x55)
#define AREX_DARK    lv_color_make(0x00, 0x33, 0x00)
#define AREX_BLACK   lv_color_make(0x00, 0x00, 0x00)
#define AREX_BG      lv_color_make(0x05, 0x05, 0x05)

/* Debug 配置: 左侧锚点 title_zone / val_zone 调试边框: 0=关闭(默认), 1=开启 */
#define AREX_DEBUG_BORDER  0

/* 5列x6行网格，最多装30个组件 */
#define AREX_MAX_WIDGETS    30
#define AREX_WIDGET_COLS    5
#define AREX_WIDGET_ROWS    6

/* 10U 左侧锚点最大行数（布局数组长度） */
#define AREX_MAX_LEFT_ROWS    8

/* 左侧锚点模块间分割线宏 */
#define AREX_ANCHOR_SEP_THICK  3   /* 模块间分割线粗细 px */
#define AREX_ANCHOR_SEP_STYLE  AREX_SEP_SOLID  /* 分割线样式: SOLID/DASHED/DOTTED */

/* =========================================================
 * 1b. 气体表常量 (供全局引用)
 * ========================================================= */
#define AREX_GAS_COUNT  4

extern const char  *AREX_GAS_NAMES[AREX_GAS_COUNT];
extern const uint8_t AREX_GAS_MOD_M[AREX_GAS_COUNT];

/* 卡片数量常量（与 CARD_ID_* 枚举一致，引用 arex_card_registry.h 中的枚举尾项） */
#define AREX_CARD_COUNT  CARD_ID_COUNT

/* 卡片顺序配置读取接口（供 arex_card_registry.c / arex_ui_state.c 使用） */
extern uint8_t g_sys_card_order(uint8_t pos);

/* =========================================================
 * 2. 枚举字典 (配置项映射)
 * ========================================================= */

/* ---- 字体 ID 枚举 (APP 同步核心) ----
 *
 * 全系统唯一字体 ID 字典。APP 通过下发数字 ID 来切换字体，
 * 渲染引擎通过 arex_get_font(id) 映射为真实 lvgl 字体指针。
 * 禁止在配置结构体中保存 lv_font_t*，只允许保存此枚举值！
 */
typedef enum {
    AREX_FONT_ID_SMALL = 0,  /* 14px  标签/单位/Badge */
    AREX_FONT_ID_TITLE,      /* 20px  菜单项/卡片标题 */
    AREX_FONT_ID_MEDIUM,     /* 28px  数据值 */
    AREX_FONT_ID_HUGE,       /* 48px  深度大数字 */
} arex_font_id_t;

typedef enum {
    AREX_THEME_TECH = 0,   /* 左右宽屏布局 */
    AREX_THEME_CLASSIC      /* 上下流式布局 */
} arex_theme_t;

typedef enum {
    AREX_ORDER_NORMAL = 0,  /* 标准 (左/上) */
    AREX_ORDER_REVERSE      /* 翻转 (右/下) */
} arex_order_t;

typedef enum {
    AREX_DOTS_RIGHT = 0,
    AREX_DOTS_LEFT,
    AREX_DOTS_BOTTOM,
    AREX_DOTS_NONE
} arex_dots_pos_t;

typedef enum {
    AREX_COMPASS_CLASSIC = 0, /* 战术横带 (Tape) */
    AREX_COMPASS_AERO,        /* 战斗机平显 (HUD) */
    AREX_COMPASS_SUB          /* 潜艇声呐 (Sonar) */
} arex_compass_style_t;

typedef enum {
    AREX_ALIGN_LEFT   = 0,
    AREX_ALIGN_CENTER,
    AREX_ALIGN_RIGHT
} arex_align_t;

typedef enum {
    AREX_SEP_NONE    = 0,
    AREX_SEP_SOLID,
    AREX_SEP_DASHED,
    AREX_SEP_DOTTED
} arex_sep_style_t;

/* =========================================================
 * 2c. 5F 自定义网格组件 ID 枚举 (APP 同步核心)
 *
 * 全系统唯一组件类型字典。APP 下发 widget_id 即可指定组件类型，
 * 渲染引擎通过 arex_widget_id_t → display name + unit string + data source
 * 做数据绑定，绝不硬编码字号或排版。
 *
 * 注意：这些 ID 与左侧锚点 AREX_MODULE_* 共享同一个数据源！
 * 告警同步引擎靠这个共享 ID 实现"左侧锚点 + 5F 组件同时闪烁"。
 * ========================================================= */
typedef enum {
    AREX_WIDGET_EMPTY      = 0,   /* 空槽位 */
    AREX_WIDGET_DEPTH      = 1,   /* DEPTH 深度 — 数据源: g_sensor_data.depth */
    AREX_WIDGET_TEMP       = 2,   /* TEMP 水温 — 数据源: g_sensor_data.temp */
    AREX_WIDGET_HEADING    = 3,   /* HEADING 航向 — 数据源: g_sensor_data.heading */
    AREX_WIDGET_SAC_RATE   = 4,   /* SAC 呼吸速率 — 数据源: g_sensor_data.sac_rate */
    AREX_WIDGET_BATTERY    = 5,   /* BATTERY 电池 — 数据源: g_sensor_data.battery_pct */
    AREX_WIDGET_NDL        = 6,   /* NDL 免减压 — 数据源: g_sensor_data.ndl */
    AREX_WIDGET_TTS        = 7,   /* TTS 回到水面 — 数据源: g_sensor_data.tts */
    AREX_WIDGET_PPO2       = 8,   /* PPO2 — 数据源: g_sensor_data.ppo2[active_gas] */
    AREX_WIDGET_CNS        = 9,   /* CNS — 数据源: g_sensor_data.cns_pct */
    AREX_WIDGET_POD1       = 10,  /* POD1 气瓶1 — 数据源: g_sensor_data.pod1_bar */
    AREX_WIDGET_POD2       = 11,  /* POD2 气瓶2 — 数据源: g_sensor_data.pod2_bar */
    AREX_WIDGET_WTIME      = 12,  /* W.TIME 潜水总时 — 数据源: g_sensor_data.dive_time_s */
    AREX_WIDGET_GAS        = 13,  /* GAS 当前气体 — 数据源: g_sensor_data.gas_name */
    AREX_WIDGET_COUNT
} arex_widget_id_t;

/* =========================================================
 * 2d. 告警级别枚举
 * ========================================================= */
typedef enum {
    AREX_ALARM_NONE   = 0,
    AREX_ALARM_INFO   = 1,   /* INFO: 提醒 */
    AREX_ALARM_WARN   = 2,   /* WARN: 警告（1Hz 闪烁）*/
    AREX_ALARM_CRIT   = 3,   /* CRITICAL: 危险（2Hz 快速闪烁）*/
} arex_alarm_level_t;

/* =========================================================
 * 3. NVDS 核心配置结构体 (字节对齐，用于持久化)
 * ========================================================= */
#pragma pack(push, 1)
typedef struct {
    /* --- 光学与安全区 (Safe Zone) --- */
    uint16_t safe_zone_w;    /* 默认 580 */
    uint16_t safe_zone_h;    /* 默认 420 */
    int16_t  offset_x;      /* 瞳距校准 (IPD) */
    int16_t  offset_y;      /* 浮力盲区校准 */

    /* --- 全局架构与行为 --- */
    uint8_t  theme_mode;    /* arex_theme_t */
    uint8_t  layout_order;   /* arex_order_t */
    uint8_t  dots_position;   /* arex_dots_pos_t */
    uint8_t  compass_style;  /* arex_compass_style_t */
    uint8_t  flash_speed;    /* 动画闪烁速度 (0=慢, 1=中, 2=快) */
    bool     mask_enabled;   /* 面镜盲区掩膜开关 */

    /* --- 样式与对齐 --- */
    bool     split_outward;  /* 双拼模块向外展开 */

    /* --- 分割线系统 --- */
    uint8_t  sep_style;       /* arex_sep_style_t */
    uint8_t  sep_thick;       /* 线条粗细 px */
    uint8_t  sep_alpha;       /* 透明度 0~255 */

    /* --- 10U 网格高度分配 (1U = 10px) --- */
    uint8_t  h_depth;        /* DEPTH 大通栏 (默认 8U) */
    uint8_t  h_ndl;          /* NDL/TTS 双拼 (默认 6U) */
    uint8_t  h_pod;          /* POD 1/2 双拼 (默认 6U) */
    uint8_t  h_batt;         /* BATT/W.TIME 双拼 (默认 4U) */
    uint8_t  h_gas;          /* GAS 中通栏 (默认 6U) */
    uint8_t  h_time;         /* DIVE TIME 底部 (默认 4U) */
    uint8_t  gap_u;          /* 模块间距 (默认 1U) */
    uint8_t  panel_gap_u;     /* 左侧锚点与右侧面板间距 (默认 1U) */
    uint8_t  title_h_u;      /* 标题高度 (默认 2U) */
    uint8_t  h_menu_item;    /* 菜单项高度 (默认 4.8U→取整5U=48px) */
    uint8_t  gap_menu;       /* 菜单项间距 (默认 0.8U=8px) */
    uint8_t  h_tissues_chart;/* 组织柱图高度 (默认 9U=90px) */

    /* --- 5F 自定义网格 (5x6 密集排版) --- */
    /* APP 下发每个组件的: 类型ID, 起始行(0~5), 起始列(0~4), 列跨度(1~2), 行跨度(1~2) */
    uint8_t  widget_count;    /* 当前装填的组件数量 (最多30) */
    uint8_t  widget_ids[AREX_MAX_WIDGETS];  /* 组件类型: arex_widget_id_t */
    uint8_t  widget_r[AREX_MAX_WIDGETS];    /* 起始行 0~5 */
    uint8_t  widget_c[AREX_MAX_WIDGETS];    /* 起始列 0~4 */
    uint8_t  widget_w[AREX_MAX_WIDGETS];    /* 列跨度 1~2 */
    uint8_t  widget_h[AREX_MAX_WIDGETS];    /* 行跨度 1~2 */

    /* --- 卡片顺序 (APP 同步就绪) ---
     * card_order[i] = card_id（0=INFO 1=COMPASS 2=DECO 3=GAS 4=PLAN 5=SETUP）
     * 控制 tileview 中各卡片的显示顺序，默认 {0,1,2,3,4,5} */
    uint8_t card_order[AREX_CARD_COUNT];

    /* --- 用户设置 (运行时可修改) --- */
    float   mod_ppo2;           /* 默认 1.4f */
    uint8_t conservatism;       /* 默认 1 (MED) */
    uint8_t brightness;         /* 默认 2 (HIGH) */

} arex_sys_config_t;
#pragma pack(pop)

/* =========================================================
 * 4. 实时数据总线 (RAM Only - 高频刷新)
 * ========================================================= */
typedef struct {
    /* 左侧锚点数据 */
    float   depth;           /* 当前深度 m */
    int16_t ndl;            /* 免减压时间 min */
    uint16_t tts;           /* 回到水面时间 min */
    float   pod1_bar;       /* 气瓶1压力 bar */
    float   pod2_bar;       /* 气瓶2压力 bar */
    float   ppo2[3];        /* 三段 PO2 */
    float   battery_pct;      /* 电池百分比 */

    /* 气体信息 */
    uint8_t  gas_active_idx;
    char     gas_name[16];

    /* 罗盘数据 */
    uint16_t heading;        /* 当前航向 0~359 */
    bool     heading_locked;
    uint16_t heading_target;

    /* 潜水时间 */
    uint32_t dive_time_s;
    uint32_t surface_time_s;   /* WTM 水面休息时间 */

    /* 减压/组织数据 */
    uint8_t  tissue_pct[16];
    uint8_t  cns_pct;
    uint16_t otu;
    int16_t  next_stop_m;
    uint8_t  next_stop_min;

    /* 减压违规标志：仅当真实减压引擎判断进入减压区时由业务逻辑置 true */
    bool     deco_violation;

    /* System Data — 设备基础数据 */
    float    temperature_c;      /* 设备/水温 摄氏度 */
    bool     strobe_on;         /* 留转灯（频闪灯）开关状态 */
    bool     flashlight_on;      /* 手电筒开关状态 */
    uint8_t  cylinder_count;   /* 气瓶连接数量 (x0, x1...) */

    /* =========================================================
     * Data Bus 脏标记位域 (UI 消费任务专用)
     * ========================================================= */
    uint32_t dirty_mask;

} arex_sensor_data_t;

/* =========================================================
 * Data Bus 脏标记位掩码枚举
 * ========================================================= */
typedef enum {
    DIRTY_NONE      = 0,
    DIRTY_DEPTH     = (1U << 0),   /* 深度数据 */
    DIRTY_NDL       = (1U << 1),   /* 免减压时间 */
    DIRTY_TTS       = (1U << 2),   /* 回到水面时间 */
    DIRTY_POD       = (1U << 3),   /* 气瓶压力（pod1/pod2） */
    DIRTY_BATT      = (1U << 4),   /* 电池电量 */
    DIRTY_HEADING   = (1U << 5),   /* 罗盘航向 */
    DIRTY_TIME      = (1U << 6),   /* 潜水时间 / W.TIME */
    DIRTY_PPO2      = (1U << 7),   /* PO2 值 */
    DIRTY_GAS       = (1U << 8),   /* 气体切换 */
    DIRTY_ALARM     = (1U << 9),   /* 告警状态 */
    DIRTY_DECO      = (1U << 10),  /* 减压站序列 + 站点时间（临界区保护） */
    DIRTY_TEMP      = (1U << 11),  /* 温度数据 */
    DIRTY_DEVICES   = (1U << 12),  /* 外设状态（灯、气瓶数量） */
    DIRTY_TISSUES   = (1U << 13),  /* 16 组织舱饱和度数组（临界区保护） */
    DIRTY_CNS       = (1U << 14),  /* CNS 氧中毒百分比 */
    DIRTY_OTU       = (1U << 15),  /* OTU 氧中毒剂量单位 */
    DIRTY_UI_LAYOUT = (1U << 16),  /* UI 布局重建（BLE 配置同步触发） */
} arex_dirty_bit_t;

/* =========================================================
 * 5. 全局单例
 * ========================================================= */
extern arex_sys_config_t  g_sys_config;
extern arex_sensor_data_t g_sensor_data;

/* =========================================================
 * 6b. 潜水轨迹与减压停留（供 card_plan.c 和 UI_main.c 共享）
 * ========================================================= */
typedef struct { float time_s; float depth_m; } arex_dive_pt_t;
typedef struct { float depth_m; float stay_min; }  arex_deco_stop_t;

#define MAX_DIVE_LOG   100
#define MAX_DECO_STOPS 10

extern arex_dive_pt_t   g_dive_log[MAX_DIVE_LOG];
extern uint16_t         g_dive_log_count;
extern arex_deco_stop_t g_deco_stops[MAX_DECO_STOPS];
extern uint16_t         g_deco_stop_count;

/* =========================================================
 * 7. API 接口
 * ========================================================= */
void arex_ui_init(void);
void arex_ui_apply_config(void);
void arex_ui_update_data(void);

/* 配置默认值加载 */
void arex_sys_config_defaults(arex_sys_config_t *cfg);

/* 安全区边界检测 */
bool arex_safe_zone_in_danger(void);

/* =========================================================
 * 8. 绝对坐标推算引擎 (供 arex_screen.c 调用)
 * ========================================================= */

/* Tech 模式布局 */
void arex_calc_tech_layout(int16_t *out_lx, int16_t *out_ly,
                           uint16_t *out_lw, uint16_t *out_lh,
                           int16_t *out_rx, int16_t *out_ry,
                           uint16_t *out_rw, uint16_t *out_rh);

/* Classic 模式布局 */
void arex_calc_classic_layout(int16_t *out_top_x, int16_t *out_top_y,
                              uint16_t *out_top_w, uint16_t *out_top_h,
                              int16_t *out_bot_x, int16_t *out_bot_y,
                              uint16_t *out_bot_w, uint16_t *out_bot_h);

/* 5x6 网格坐标推算 */
void arex_calc_widget_cell(uint16_t parent_w, uint16_t parent_h,
                          uint8_t row, uint8_t col,
                          uint8_t w_span, uint8_t h_span,
                          int16_t *out_x, int16_t *out_y,
                          uint16_t *out_w, uint16_t *out_h);

/* 16 柱组织图 X 坐标推算 */
void arex_calc_tissue_bars(uint16_t total_w, uint16_t bar_max_h,
                           int16_t out_x[16], uint16_t out_w[16]);

/* LVGL 辅助 */
lv_text_align_t arex_align_to_lv(uint8_t align);
lv_align_t arex_align_to_lv_align(uint8_t align);

/* 通用卡片标题渲染器：标题区固定 AREX_CARD_TITLE_H(40px)，
 * 文字 Y=5，分割线 Y=AREX_CARD_TITLE_H-2。下方内容区以 AREX_CARD_TITLE_H 为 Y=0 起点。 */
void arex_render_card_title(lv_obj_t *parent_card, const char *title_text);

/* 字体映射器：唯一允许将字体 ID 转换为真实 lvgl 字体指针的地方 */
const lv_font_t *arex_get_font(uint8_t font_id);

/* =========================================================
 * 9. 布局矩形计算 (供 rebuild 调用)
 * ========================================================= */
void arex_calc_layout_rect(int16_t *out_x, int16_t *out_y,
                          uint16_t *out_w, uint16_t *out_h,
                          int16_t anchor_offset_x, int16_t anchor_offset_y);

/* =========================================================
 * 9b. 右侧卡片动态菜单配置 (APP 同步核心)
 *
 * 每个菜单选项的描述结构体。APP 下发 JSON 即可改变菜单外观，
 * 渲染引擎通过 arex_render_dynamic_menu() 统一遍历，不做硬编码判断。
 * ========================================================= */
typedef struct {
    const char *title_text;      /* 左侧主文本 (可为空) */
    const char *value_badge;     /* 右侧数值/状态徽章 (可为空) */
    uint8_t     title_font_id;   /* 标题字体 ID: arex_font_id_t */
    uint8_t     value_font_id;   /* 徽章字体 ID: arex_font_id_t */
    uint8_t     border_width;    /* 边框粗细 px，0=无边框 */
    uint8_t     height_u;        /* 该选项高度 (单位 U，默认 0=用 h_menu_item) */
} arex_menu_item_cfg_t;

/* 菜单列表包装体 — 作为 arex_card_t.config_data 传入注册表 */
typedef struct {
    const arex_menu_item_cfg_t *items;
    uint8_t                     count;
} arex_menu_list_cfg_t;

/* 通用动态菜单工厂声明 */
void arex_render_dynamic_menu(lv_obj_t *parent_card,
                              const arex_menu_item_cfg_t *items,
                              uint8_t item_count,
                              int start_y,
                              lv_obj_t **out_item_handles);

/* =========================================================
 * 10. 5F 自定义网格渲染引擎 (靶向告警同步核心)
 * ========================================================= */

/* 5F 网格总线渲染器：遍历 g_sys_config.widget_* 数组，
 * 用纯数学行×列→绝对坐标映射渲染所有组件。
 * left_anchor_obj 传入用于告警引擎跨区搜索烙印对象。 */
void arex_render_5f_custom_grid(lv_obj_t *card_custom,
                                 lv_obj_t *left_anchor_obj);

/* 按 widget_id 设置数值（由 update 循环调用，绝不触发重绘） */
void arex_widget_set_value(arex_widget_id_t id, float value);

/* 按 widget_id 设置字符串（用于 GAS 等非数值组件） */
void arex_widget_set_text(arex_widget_id_t id, const char *text);

/* 靶向告警触发：全屏搜索所有打了 user_data 烙印的组件并同步闪烁。
 * target_id = AREX_WIDGET_EMPTY 时仅弹出横幅，不做靶向同步。 */
void arex_trigger_alarm(arex_alarm_level_t level,
                        const char *eng_text,
                        arex_widget_id_t target_id);

/* 清除所有组件的告警样式（告警消失时调用） */
void arex_clear_all_alarm_styles(void);

/* 根据 widget_id 获取显示名称（供调试/横幅使用） */
const char *arex_get_widget_name(arex_widget_id_t id);

/* 告警横幅显示/隐藏（由 arex_trigger_alarm 调用） */
void arex_show_alarm_banner(arex_alarm_level_t level, const char *eng_text);
void arex_hide_alarm_banner(void);

/* =========================================================
 * 2c. 左侧 2x6 绝对网格配置结构体 (APP 同步核心)
 *
 * 左侧 160x360 区域（不含底部 60px SystemData）被划分为 2列(80px) x 6行(60px)。
 * 每行列/列/跨度完全复用 AREX_WIDGET_* 枚举（共享同一数据源）。
 * ========================================================= */
#define AREX_LEFT_COLS   2
#define AREX_LEFT_ROWS   6
#define AREX_LEFT_CELL_W 80
#define AREX_LEFT_CELL_H 60
#define AREX_LEFT_GRID_W (AREX_LEFT_COLS * AREX_LEFT_CELL_W)  /* 160px */
#define AREX_LEFT_GRID_H (AREX_LEFT_ROWS * AREX_LEFT_CELL_H)  /* 360px */

typedef struct {
    arex_widget_id_t widget_id;  /* 组件类型 ID */
    uint8_t x;                  /* 列索引 0~1 */
    uint8_t y;                  /* 行索引 0~5 */
    uint8_t w;                  /* 跨越列数 1~2 */
    uint8_t h;                  /* 跨越行数 1~2 */
    uint8_t font_id;            /* 字号: arex_font_id_t */
} arex_custom_widget_cfg_t;

/* 左侧网格组件数组声明（最多 12 个组件覆盖 2x6 网格） */
#define AREX_LEFT_MAX_WIDGETS 12
extern arex_custom_widget_cfg_t g_left_widgets[AREX_LEFT_MAX_WIDGETS];
extern uint8_t g_left_widget_count;

/* 外部告警状态容器（由 arex_screen.c 在创建锚点和卡片时注入） */
extern lv_obj_t *g_left_anchor_obj;
extern lv_obj_t *g_card_custom_obj;

/* 5F 网格坐标推算：支持 title_zone_h 避让偏移，确保网格落在标题区下方 */
void arex_calc_widget_grid(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t span_w, uint8_t span_h,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h);

/* =========================================================
 * 12. 左侧 2x6 绝对网格渲染引擎
 *
 * 严格将 160x360 区域划分为 2列(80px) x 6行(60px) 的绝对网格矩阵，
 * 彻底废弃 current_y 累加排版，改用 x*y*w*h 纯数学坐标推演。
 * 内部调用 render_widget_by_id 工厂函数，兼容 arex_widget_id_t 体系。
 * SystemData 底部 60px 由 arex_render_system_data() 独立渲染。
 * ========================================================= */

/* 左侧网格总线渲染器：遍历 g_left_widgets[] 数组，
 * 用纯数学 cell_w * cell_h 推算绝对坐标并渲染所有组件。
 * left_anchor 传入用于告警引擎跨区搜索烙印对象。 */
void arex_render_left_anchor_grid(lv_obj_t *left_anchor);

/* 通用组件工厂（左侧网格 + 5F 共用）：
 * 接收绝对物理坐标，生成标准化组件容器。
 * is_depth_icon == true 时，在 DEPTH 模块内挂载 sudu 速率图标。
 * 返回组件容器对象句柄。 */
lv_obj_t *render_widget_by_id(lv_obj_t *parent,
                               arex_widget_id_t w_id,
                               int16_t abs_x, int16_t abs_y,
                               uint16_t abs_w, uint16_t abs_h,
                               uint8_t span_w, uint8_t span_h,
                               bool is_depth_icon);

/* UI 消费任务 — 全系统唯一允许执行 lv_label_set_text 的地方（50ms 定时器驱动） */
void arex_ui_update_task(lv_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif /* AREX_UI_ENGINE_H */
