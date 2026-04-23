#ifndef AREX_UI_ENGINE_H
#define AREX_UI_ENGINE_H

#include "lvgl/lvgl.h"
#include <stdint.h>
#include <stdbool.h>

/* =========================================================
 * 1. 系统核心宏定义
 * ========================================================= */
#define AREX_BASE_U             10   /* 物理基准单位 1U = 10px */
#define AREX_MIN_CLASSIC_TOP_H  200  /* Classic 模式下最小上区高度 px */
#define AREX_MASK_EDGE_GUARD    80   /* 面镜盲区掩膜底部警戒阈值 px */
#define AREX_PHYSICAL_W    640  /* 硬件屏幕极限宽 */
#define AREX_PHYSICAL_H    480  /* 硬件屏幕极限高 */
#define AREX_LEFT_ANCHOR_W  160  /* 左侧锚点固定宽度 */

/* 5列x6行网格，最多装30个组件 */
#define AREX_MAX_WIDGETS    30
#define AREX_WIDGET_COLS    5
#define AREX_WIDGET_ROWS    6

/* 10U 左侧锚点组件数量 */
#define ANCHOR_COMP_COUNT   9
#define ANCHOR_LEFT_MODULE_COUNT 9   /* DEPTH NDL TTS POD1 POD2 BATT WTM GAS TIME */

/* =========================================================
 * 2. 枚举字典 (配置项映射)
 * ========================================================= */
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
 * 2b. 左侧锚点模块枚举 (模块类型标识)
 *
 * APP 通过修改 g_sys_config.left_order[] 中的枚举值，
 * 即可自由调整左侧模块的显示顺序和组合。
 * ========================================================= */
typedef enum {
    AREX_MODULE_NONE    = 0,   /* 占位/空白 */
    AREX_MODULE_DEPTH   = 1,   /* DEPTH 大通栏 */
    AREX_MODULE_NDL     = 2,   /* NDL 双拼左块 */
    AREX_MODULE_TTS     = 3,   /* TTS 双拼右块 */
    AREX_MODULE_POD1    = 4,   /* POD 1 双拼左块 */
    AREX_MODULE_POD2    = 5,   /* POD 2 双拼右块 */
    AREX_MODULE_BATT    = 6,   /* BATT 双拼左块 */
    AREX_MODULE_WTM     = 7,   /* W.TIME 双拼右块 */
    AREX_MODULE_GAS     = 8,   /* GAS 中通栏 */
    AREX_MODULE_TIME    = 9,   /* DIVE TIME 底部通栏 */
    AREX_MODULE_CUSTOM  = 10,  /* 自定义预留 */
} arex_left_module_t;

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

    /* --- 样式与字体 --- */
    uint8_t  font_sz_huge;   /* 大字号 (默认 58px) */
    uint8_t  font_sz_med;    /* 中字号 (默认 28px) */
    uint8_t  font_sz_small;  /* 小字号 (默认 14px) */
    uint8_t  align_title;    /* arex_align_t */
    uint8_t  align_huge;     /* arex_align_t */
    uint8_t  align_med;      /* arex_align_t */
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
    uint8_t  widget_count;    /* 当前装填的组件数量 (最多30) */
    uint8_t  widget_ids[AREX_MAX_WIDGETS];
    uint8_t  widget_w[AREX_MAX_WIDGETS];
    uint8_t  widget_h[AREX_MAX_WIDGETS];

    /* --- 左侧锚点模块顺序 (APP 同步就绪) --- */
    /* left_order[i] = arex_left_module_t，控制模块渲染顺序
     * 例：{AREX_MODULE_DEPTH, AREX_MODULE_NDL, AREX_MODULE_TTS, ...}
     * 默认 9 个模块依次排列。双拼块（NDL/TTS）必须成对出现。 */
    uint8_t  left_order[ANCHOR_LEFT_MODULE_COUNT];

    /* --- 左侧锚点模块属性表 (每模块独立样式配置) --- */
    /* 每个模块可独立设置：分割类型、标题字体、数值字体、对齐方式 */
    uint8_t  left_mod_split[ANCHOR_LEFT_MODULE_COUNT];    /* 0=单栏 1=双拼左 2=双拼右 */
    uint8_t  left_mod_title_font[ANCHOR_LEFT_MODULE_COUNT];/* 0=SMALL 1=MEDIUM 2=TITLE */
    uint8_t  left_mod_val_font[ANCHOR_LEFT_MODULE_COUNT];  /* 0=SMALL 1=MEDIUM 2=TITLE 3=HUGE */
    uint8_t  left_mod_title_align[ANCHOR_LEFT_MODULE_COUNT];/* arex_align_t */
    uint8_t  left_mod_val_align[ANCHOR_LEFT_MODULE_COUNT];  /* arex_align_t */

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
 * ========================================================= */
typedef struct {
    arex_left_module_t module;  /* 模块类型枚举 */
    int16_t  y;                /* Y 坐标 px */
    uint16_t h;                /* 总高度 px */
    uint16_t title_h;          /* 标题区高度 px */
    uint16_t val_h;            /* 数值区高度 px */
    uint16_t w;                /* 宽度 px */
    uint8_t  split;            /* 0=单栏 1=双拼左 2=双拼右 */
    uint8_t  title_font;       /* 0=SMALL 1=MEDIUM 2=TITLE */
    uint8_t  val_font;         /* 0=SMALL 1=MEDIUM 2=TITLE 3=HUGE */
    uint8_t  title_align;      /* arex_align_t */
    uint8_t  val_align;        /* arex_align_t */
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

/* 左侧锚点组件 Y 坐标推算 */
void arex_calc_anchor_layout(arex_anchor_comp_t comps[ANCHOR_COMP_COUNT],
                             uint16_t *out_total_h);

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

/* =========================================================
 * 9. 布局矩形计算 (供 rebuild 调用)
 * ========================================================= */
void arex_calc_layout_rect(int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h,
                           int16_t anchor_offset_x, int16_t anchor_offset_y);

#endif /* AREX_UI_ENGINE_H */
