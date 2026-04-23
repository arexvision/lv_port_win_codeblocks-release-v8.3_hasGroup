#ifndef AREX_UI_ENGINE_H
#define AREX_UI_ENGINE_H

#include "lvgl/lvgl.h"
#include <stdint.h>
#include <stdbool.h>
#include "arex_card_registry.h"

/* =========================================================
 * 系统核心宏定义
 * ========================================================= */
#define AREX_BASE_U             10   /* 物理基准单位 1U = 10px */
#define AREX_MIN_CLASSIC_TOP_H  200  /* Classic 模式下最小上区高度 px */
#define AREX_MASK_EDGE_GUARD    80   /* 面镜盲区掩膜底部警戒阈值 px */
#define AREX_PHYSICAL_W    640  /* 硬件屏幕极限宽 */
#define AREX_PHYSICAL_H    480  /* 硬件屏幕极限高 */
#define AREX_LEFT_ANCHOR_W  160  /* 左侧锚点固定宽度 */

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
#define ANCHOR_COMP_COUNT     16  /* 最大组件句柄数（兼容旧 API，UI 层用） */

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
 * 2e. 左侧锚点模块枚举
 * ========================================================= */
typedef enum {
    AREX_MODULE_EMPTY  = 0,   /* 空槽位：不渲染任何模块 */
    AREX_MODULE_DEPTH  = 1,   /* DEPTH 大数字（独立一行，全宽） */
    AREX_MODULE_NDL    = 2,   /* NDL 免减压时间 */
    AREX_MODULE_TTS    = 3,   /* TTS 回到水面时间 */
    AREX_MODULE_POD1  = 4,   /* POD1 气瓶1压力 */
    AREX_MODULE_POD2  = 5,   /* POD2 气瓶2压力 */
    AREX_MODULE_BATT  = 6,   /* BATT 电池 */
    AREX_MODULE_WTM   = 7,   /* W.TIME 潜水总时间 */
    AREX_MODULE_GAS   = 8,   /* GAS 当前气体 */
    AREX_MODULE_TIME  = 9,   /* TIME 独立计时 */
} arex_left_module_t;

/* =========================================================
 * 2c. 左侧行配置结构体（APP 同步核心）
 *
 * 每行描述一行模块布局。APP 通过修改 left_layout[] 即可
 * 自由组合任意两个模块为双拼，或让某一模块独占全宽。
 *
 * 渲染引擎只遍历 left_layout[]，不做任何"如果是 NDL 就配 TTS" 的硬编码判断。
 * ========================================================= */
#define AREX_ROW_MAX_SLOTS  2   /* 每行最多 2 个模块槽（左+右） */

typedef struct {
    /* 该行包含的模块（AREX_MODULE_*），AREX_MODULE_EMPTY 表示空槽 */
    uint8_t left_module;   /* 左侧模块枚举 */
    uint8_t right_module;  /* 右侧模块枚举（可为 EMPTY 使左侧独占全宽） */

    /* 尺寸：全部以 U 为单位，由渲染引擎乘以 AREX_BASE_U */
    uint8_t h_u;           /* 该行总高度（不含 gap） */
    uint8_t title_h_u;     /* 标题区高度（默认为全局 title_h_u） */

    /* 样式 (字号字段已改为 arex_font_id_t) */
    uint8_t title_font;    /* 标题字号: arex_font_id_t */
    uint8_t val_font;      /* 数值字号: arex_font_id_t */
    uint8_t val_align;    /* 数值对齐: 0=LEFT 1=CENTER 2=RIGHT */
    uint8_t sep_style;     /* 分割线样式: 0=NONE 1=SOLID 2=DASHED 3=DOTTED */
    uint8_t sep_thick;     /* 分割线粗细 px（覆盖全局 sep_thick，0=用全局） */
} arex_left_row_cfg_t;

/* =========================================================
 * 3. NVDS 核心配置结构体 (字节对齐，用于持久化)
 * ========================================================= */
#pragma pack(push, 1)
typedef struct {
    /* --- 光学与安全区 (Safe Zone) --- */
    uint16_t safe_zone_w;    /* 默认 580 */
    uint16_t safe_zone_h;    /* 默认 400 */
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

    /* --- 左侧锚点行配置 (APP 同步就绪 — 自由双拼) --- */
    /* APP 只需修改 left_layout[] 中任意行的 left/right_module 即可自由组合 */
    arex_left_row_cfg_t left_layout[AREX_MAX_LEFT_ROWS];

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

    /* 减压/组织数据 */
    uint8_t  tissue_pct[16];
    uint8_t  cns_pct;
    uint16_t otu;
    int16_t  next_stop_m;
    uint8_t  next_stop_min;

    /* 减压违规标志：仅当真实减压引擎判断进入减压区时由业务逻辑置 true */
    bool     deco_violation;

    /* 潜水曲线日志 */
    uint16_t deco_stops[8];
    uint8_t  deco_stop_count;

} arex_sensor_data_t;

/* =========================================================
 * 5. 左侧锚点组件布局信息结构 (供 arex_screen.c 使用)
 *
 * 由 arex_calc_anchor_layout() 在运行时填充，
 * 所有尺寸基于 AREX_BASE_U 推算，不含硬编码像素值。
 *
 * 该结构由 arex_calc_anchor_layout() 按 left_layout[] 遍历填充，
 * 单栏块有 1 个入口，双拼块有 2 个入口（split=1 和 split=2）。
 * ========================================================= */
typedef struct {
    arex_left_module_t module;   /* 模块类型枚举 */
    int16_t  y;                 /* Y 坐标 px */
    uint16_t h;                 /* 总高度 px */
    uint16_t title_h;            /* 标题区高度 px */
    uint16_t val_h;             /* 数值区高度 px */
    uint16_t w;                 /* 宽度 px */
    uint8_t  split;             /* 0=单栏 1=双拼左 2=双拼右 */
    uint8_t  title_font;        /* arex_font_id_t */
    uint8_t  val_font;          /* arex_font_id_t */
    uint8_t  title_align;       /* 0=LEFT 1=CENTER 2=RIGHT */
    uint8_t  val_align;        /* 0=LEFT 1=CENTER 2=RIGHT */
    uint8_t  sep_style;         /* arex_sep_style_t: 0=NONE 1=SOLID 2=DASHED 3=DOTTED */
    uint8_t  sep_thick;         /* 分割线粗细 px（0=用全局 g_sys_config.sep_thick） */
} arex_anchor_comp_t;

/* =========================================================
 * 6. 全局单例
 * ========================================================= */
extern arex_sys_config_t  g_sys_config;
extern arex_sensor_data_t g_sensor_data;

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

/* 左侧锚点组件布局推算（自由双拼版）
 * 遍历 left_layout[]，填充 comps[]（单栏1入口，双拼2入口）
 * out_count 返回实际填充的入口数量 */
void arex_calc_anchor_layout(arex_anchor_comp_t comps[ANCHOR_COMP_COUNT],
                             uint16_t *out_total_h, uint8_t *out_count);

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

/* 菜单列表包装体 — 作为 arex_card_desc_t.config_data 传入注册表 */
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

/* 外部告警状态容器（由 arex_screen.c 在创建锚点和卡片时注入） */
extern lv_obj_t *g_left_anchor_obj;
extern lv_obj_t *g_card_custom_obj;

#endif /* AREX_UI_ENGINE_H */
