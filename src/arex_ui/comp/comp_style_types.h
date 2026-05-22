#ifndef COMP_STYLE_TYPES_H
#define COMP_STYLE_TYPES_H

#include "lvgl/lvgl.h"
#include <stdint.h>

/*
 * Widget layout/style type definitions.
 * This header is included from ui_engine.h after the core AREX enums
 * such as comp_id_t and arex_font_id_t are declared.
 */

/* =========================================================
 * 2c. 左侧 2x7 绝对网格组件配置 (APP 同步核心)
 *
 * 架构铁律：APP 只下发 [widget_id, x, y] 三字段（3字节）。
 * MCU 根据 widget_id 从样式表自动查表获取 span_w/span_h。
 * ========================================================= */
#define AREX_LEFT_COLS   2
#define AREX_LEFT_ROWS   7
#define AREX_LEFT_CELL_W 80
#define AREX_LEFT_CELL_H 60
#define AREX_LEFT_GRID_W (AREX_LEFT_COLS * AREX_LEFT_CELL_W)  /* 160px */
#define AREX_LEFT_GRID_H (AREX_LEFT_ROWS * AREX_LEFT_CELL_H)  /* 420px */

/* APP 下发数据结构（只含位置，无样式）已迁移到 arex_grid_widget_t */

/* 组件布局类型 */
typedef enum
{
    AREX_LAYOUT_CENTER      = 0,  /* 通用居中：标题上 + 数值中 + 单位下 */
    AREX_LAYOUT_DIAGONAL   = 1,  /* 对角线：标题左上 + 数值右下 */
    AREX_LAYOUT_TOP_BOTTOM = 2,  /* 上下：标题顶部 + 数值底部 */
    AREX_LAYOUT_LEFT_RIGHT = 3,  /* 左右：数值左侧 + 单位/标题右侧 */
    AREX_LAYOUT_NDL_BAR    = 4,  /* NDL专属：进度条 + 数值 + 标题 */
    AREX_LAYOUT_DEPTH_SPLIT = 5, /* DEPTH专属：整数 + 小数 + 单位 + 箭头 */
} arex_layout_type_t;

/* =========================================================
 * 第二步：极简 BLE 通信布局结构体 (APP → MCU)
 *
 * 架构铁律：APP 只下发"意图和网格坐标"，MCU 包揽所有"内部像素级排版"。
 * BLE 协议中每个网格组件仅占 3 字节，彻底压榨协议体积。
 * ========================================================= */
#define MAX_WIDGETS 30

#pragma pack(push, 1)
typedef struct
{
    comp_id_t widget_id;  /* 组件类型 ID（必须与枚举严格对齐） */
    uint8_t x;                    /* 列索引 0~4 */
    uint8_t y;                    /* 行索引 0~5 */
} comp_pos_t;
#pragma pack(pop)

/* =========================================================
 * 第三步：MCU 本地样式字典结构体 (Union 内存优化版)
 *
 * 架构铁律：APP 省略掉的 w 和 h 由 MCU 本地样式表提供。
 * 各种奇形怪状模块的专属 offset 用 Union 强制共享内存，防止结构体膨胀。
 * ========================================================= */

/* DEPTH 专属样式参数
 * 用于 DEPTH_1612/1606 等深度组件，实现整数+小数+单位+箭头图标分离排版 */
typedef struct
{
    int8_t  int_offset_x;    /* 整数部分 X 偏移 */
    int8_t  int_offset_y;    /* 整数部分 Y 偏移 */
    uint8_t int_align;       /* 整数部分对齐方式 */
    int8_t  dec_offset_x;    /* 小数部分 X 偏移（相对整数） */
    int8_t  dec_offset_y;    /* 小数部分 Y 偏移（相对整数） */
    int8_t  unit_offset_x;   /* 单位 X 偏移（相对小数） */
    int8_t  unit_offset_y;   /* 单位 Y 偏移（相对小数） */
    int8_t  icon_offset_x;  /* 箭头图标 X 偏移 */
    int8_t  icon_offset_y;  /* 箭头图标 Y 偏移 */
    uint8_t icon_align;      /* 箭头图标对齐方式 */
} arex_style_depth_t;

/* NDL 专属样式参数
 * 用于 NDL_STOP_1606 等停留组件，实现进度条+数值分离排版 */
typedef struct
{
    int8_t  bar_offset_x;   /* 进度条 X 偏移 */
    int8_t  bar_offset_y;   /* 进度条 Y 偏移 */
    uint8_t bar_align;      /* 进度条对齐方式 */
    int8_t  bar_w;          /* 进度条宽度 */
    int8_t  bar_h;          /* 进度条高度 */
    uint8_t bar_fill_dir;   /* 填充方向：0=从下往上, 1=从上往下 */
} arex_style_ndl_t;

/* NDL_STOP 多形态专属样式参数
 * 用于 NDL_STOP_1606，支持三种状态：NDL常态/Safety停留/Deco停留 */
typedef struct
{
    /* 垂直进度条（NDL常态显示） */
    int8_t  vert_offset_x;  /* 垂直条 X 偏移 */
    int8_t  vert_offset_y;  /* 垂直条 Y 偏移 */
    uint8_t vert_align;      /* 垂直条对齐方式 */
    int8_t  vert_w;          /* 垂直条宽度 */
    int8_t  vert_h;          /* 垂直条高度 */
    /* 横向进度条（停留态显示） */
    int8_t  horiz_offset_x;  /* 横向条 X 偏移 */
    int8_t  horiz_offset_y;  /* 横向条 Y 偏移 */
    int8_t  horiz_w;         /* 横向条宽度 */
    int8_t  horiz_h;         /* 横向条高度 */

    /* =======================================
     * 常态 (Normal) 排版参数
     * ======================================= */
    int8_t  norm_main_x;
    int8_t norm_main_y;
    uint8_t norm_main_align; /* NDL 巨大数字 */
    int8_t  norm_sub_x;
    int8_t norm_sub_y;
    uint8_t norm_sub_align;  /* 底部 NDL 文本 */

    /* =======================================
     * 停留态 (Deco/Safety) 排版参数
     * ======================================= */
    int8_t  deco_title_x;
    int8_t deco_title_y;
    uint8_t deco_title_align; /* 顶部 SAFETY/DECO */
    int8_t  deco_main_x;
    int8_t deco_main_y;
    uint8_t deco_main_align;  /* 停留倒计时 MM:SS */
    int8_t  deco_sub_x;
    int8_t deco_sub_y;
    uint8_t deco_sub_align;   /* Safety 悬浮的 NDL 文本 */
} arex_style_ndl_stop_t;

/* TISSUE 组织图专属样式参数
 * 用于 TISSUE_GF_4012/TISSUE_RAW_4012，实现16柱组织图排版 */
typedef struct
{
    int8_t  chart_offset_x; /* 柱状图 X 偏移 */
    int8_t  chart_offset_y; /* 柱状图 Y 偏移 */
    uint8_t chart_align;     /* 柱状图对齐方式 */
    int8_t  bar_count;      /* 柱状图数量（固定16） */
    int8_t  bar_spacing;    /* 柱子间距 */
} arex_style_tissue_t;

/* COMPASS 罗盘专属样式参数
 * 用于 COMPASS_1612，实现卷尺+数值分离排版 */
typedef struct
{
    int8_t  tape_offset_x;  /* 卷尺 X 偏移 */
    int8_t  tape_offset_y;  /* 卷尺 Y 偏移 */
    uint8_t tape_align;      /* 卷尺对齐方式 */
    int8_t  val_offset_x;   /* 航向数值 X 偏移 */
    int8_t  val_offset_y;   /* 航向数值 Y 偏移 */
    uint8_t val_align;      /* 航向数值对齐方式 */
} arex_style_compass_t;

/* 通用基础样式（无特殊参数的组件使用）
 * 用于 TEMP/TIME/TTS/BATT/POD 等1x1或2x1通用组件 */
typedef struct
{
    int8_t  value_offset_x; /* 数值 X 偏移 */
    int8_t  value_offset_y; /* 数值 Y 偏移 */
    uint8_t value_align;     /* 数值对齐方式 */
} arex_style_basic_t;

/* =========================================================
 * 第三步：8-bit UI 元素开关掩码 (Element Bitmask)
 *
 * 每 bit 对应一个 UI 零件的有无，字典中按 PRD 约束"按需勾选"。
 * 渲染工厂完全依据此掩码决定流水线装配哪些零件，彻底消灭 if-else 膨胀。
 * ========================================================= */
/* ELEM_TITLE : 标题行（如 "DEPTH"、"TEMP"） */
#define ELEM_TITLE (1 << 0)
/* ELEM_VALUE  : 主数值（整数/小数/字符串） */
#define ELEM_VALUE (1 << 1)
/* ELEM_UNIT   : 单位字符串（"m"、"min"、"C" 等，NULL 单位自动跳过） */
#define ELEM_UNIT  (1 << 2)
/* ELEM_BAR    : 特殊进度条/图标（DEPTH 速率箭头、NDL 停留柱、SYS 电池条等） */
#define ELEM_BAR   (1 << 3)
/* ELEM_EXTRA  : 附加异构元素（POD1/POD2 专属 ID 标签等） */
#define ELEM_EXTRA (1 << 4)

/* ELEM_SYS_BAR: 底部 SYS 区域标记（COMP_SYS_1606 组件化渲染） */
#define ELEM_SYS_BAR (1 << 5)

/* MCU 本地样式字典（Union 共享内存，大小永远等于最大成员） */
#define AREX_MAX_STYLE_SPEC_SIZE 32
typedef struct
{
    comp_id_t widget_id;   /* 绑定的组件 ID */
    uint8_t span_w;              /* 跨越列数 */
    uint8_t span_h;              /* 跨越行数 */

    /* 元素开关掩码，决定流水线要装配哪些零件 */
    uint8_t elements;            /* ELEM_TITLE | ELEM_VALUE | ELEM_UNIT | ELEM_BAR | ELEM_EXTRA */

    arex_font_id_t font_id;      /* 主数值字号 */
    arex_font_id_t title_font_id; /* 标题字号 */
    const char *unit;            /* 单位字符串（如 "m"、"min"、NULL） */
    int8_t  title_offset_x;
    int8_t  title_offset_y;
    uint8_t title_align;

    /* 专属样式 Union（强制共享内存，防止膨胀） */
    union
    {
        arex_style_depth_t        depth;                            /* DEPTH专属排版参数 */
        arex_style_ndl_t          ndl;                              /* NDL 专属参数（进度条/停留态） */
        arex_style_ndl_stop_t     ndl_stop;                         /* NDL_STOP专属排版参数 */
        arex_style_tissue_t       tissue;                           /* TISSUE专属排版参数 */
        arex_style_compass_t      compass;                          /* COMPASS专属排版参数 */
        arex_style_basic_t        basic;                            /* 通用组件排版参数 */
        uint8_t                   dummy[AREX_MAX_STYLE_SPEC_SIZE];  /* 强制对齐 */
    } spec;

    /* 组件显示名称 */
    const char *title;
} comp_style_t;

/* 辅助查表函数声明 */

/* =========================================================
 * POD 渲染状态机（POD1/POD2 共用同一枚举值，靠计数器区分）
 *
 * 策略：静态渲染计数器，同一渲染批次内按顺序分配 POD1→POD2。
 * 每次网格重建/重绘前必须调用 arex_reset_widget_render_state() 归零。
 * ========================================================= */
#define ALIGN_TL LV_ALIGN_TOP_LEFT
#define ALIGN_TM LV_ALIGN_TOP_MID
#define ALIGN_TR LV_ALIGN_TOP_RIGHT
#define ALIGN_LM LV_ALIGN_LEFT_MID
#define ALIGN_CT LV_ALIGN_CENTER
#define ALIGN_RM LV_ALIGN_RIGHT_MID
#define ALIGN_BL LV_ALIGN_BOTTOM_LEFT
#define ALIGN_BM LV_ALIGN_BOTTOM_MID
#define ALIGN_BR LV_ALIGN_BOTTOM_RIGHT


#endif /* COMP_STYLE_TYPES_H */
