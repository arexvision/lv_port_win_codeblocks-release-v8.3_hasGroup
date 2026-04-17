#ifndef AREX_UI_ENGINE_H
#define AREX_UI_ENGINE_H

#include "lvgl/lvgl.h"
#include "arex_card_registry.h"   /* 引入 AREX_CARD_COUNT / arex_card_id_t */
#include <stdint.h>
#include <stdbool.h>

/* =========================================================
 * 1. 系统核心宏定义
 * ========================================================= */
#define AREX_BASE_U         10      /* 物理基准单位 1U = 10px */
#define AREX_PHYSICAL_W     640     /* 硬件屏幕极限宽 */
#define AREX_PHYSICAL_H     480     /* 硬件屏幕极限高 */
#define AREX_LEFT_ANCHOR_W  160     /* Tech 模式左锚区固定宽度 */
#define AREX_MAX_WIDGETS    30      /* 5x6 网格最大组件数 */
/* AREX_CARD_COUNT 由 arex_card_registry.h 提供，此处不重复定义 */
#define AREX_MIN_RIGHT_H    200     /* 右侧容器零高度防坍塌保底高度 */

/* =========================================================
 * 2. 枚举字典
 * ========================================================= */
typedef enum {
    AREX_THEME_TECH    = 0,   /* 左右宽屏布局 */
    AREX_THEME_CLASSIC = 1,   /* 上下流式布局（预留） */
} arex_theme_t;

typedef enum {
    AREX_ORDER_NORMAL  = 0,   /* 标准：左锚在左 / 上锚在上 */
    AREX_ORDER_REVERSE = 1,   /* 翻转：左锚在右 / 上锚在下 */
} arex_order_t;

typedef enum {
    AREX_DOTS_RIGHT  = 0,
    AREX_DOTS_LEFT   = 1,
    AREX_DOTS_BOTTOM = 2,
    AREX_DOTS_NONE   = 3,
} arex_dots_pos_t;

typedef enum {
    AREX_COMPASS_CLASSIC = 0,  /* 战术卷尺 */
    AREX_COMPASS_AERO    = 1,  /* 战斗机平显 HUD */
    AREX_COMPASS_SUB     = 2,  /* 潜艇声呐 */
} arex_compass_style_t;

typedef enum {
    AREX_SEP_NONE   = 0,
    AREX_SEP_SOLID  = 1,
    AREX_SEP_DASHED = 2,
    AREX_SEP_DOTTED = 3,
} arex_sep_style_t;

typedef enum {
    AREX_ALIGN_LEFT   = 0,
    AREX_ALIGN_CENTER = 1,
    AREX_ALIGN_RIGHT  = 2,
} arex_align_t;

/* =========================================================
 * 3. NVDS 核心配置结构体（持久化到 Flash）
 *    #pragma pack(push,1) 确保字节对齐，便于序列化存储
 * ========================================================= */
#pragma pack(push, 1)
typedef struct {
    /* --- 1. 光学安全区 (Safe Zone) --- */
    uint16_t safe_zone_w;    /* 默认 580 */
    uint16_t safe_zone_h;    /* 默认 400 */
    int16_t  offset_x;       /* 瞳距(IPD)校准偏移，正值右移 */
    int16_t  offset_y;       /* 浮力盲区校准偏移，正值下移 */

    /* --- 2. 全局架构 (Architecture) --- */
    uint8_t  theme_mode;     /* arex_theme_t */
    uint8_t  layout_order;   /* arex_order_t */
    uint8_t  dots_position;  /* arex_dots_pos_t */
    uint8_t  compass_style;  /* arex_compass_style_t */
    uint8_t  flash_speed_ds; /* 闪烁半周期，单位 0.1s，如 3 = 0.3s */

    /* --- 3. 字体与对齐 (Typo & Align) --- */
    uint8_t  font_sz_huge;   /* 大字号，如 48 */
    uint8_t  font_sz_med;    /* 中字号，如 28 */
    uint8_t  font_sz_small;  /* 小字号，如 14 */
    uint8_t  align_title;    /* arex_align_t：标题对齐 */
    uint8_t  align_huge;     /* arex_align_t：大字数值对齐 */
    uint8_t  align_med;      /* arex_align_t：中字数值对齐 */
    uint8_t  split_outward;  /* 双拼向外展开：0=跟随全局, 1=强制外展 */

    /* --- 4. 分割线 (Separator) --- */
    uint8_t  sep_style;      /* arex_sep_style_t */
    uint8_t  sep_thick;      /* 线条粗细 px，默认 1 */
    uint8_t  sep_alpha;      /* 透明度 0~255，默认 51(≈20%) */

    /* --- 5. 10U 网格高度（单位：U，1U=10px） --- */
    uint8_t  h_depth;        /* DEPTH 大通栏，默认 8U */
    uint8_t  h_ndl;          /* NDL/TTS 双拼，默认 6U */
    uint8_t  h_pod;          /* POD1/POD2 双拼，默认 6U */
    uint8_t  h_batt;         /* BATT/W.TIME 双拼，默认 4U */
    uint8_t  h_gas;          /* GAS 中通栏，默认 6U */
    uint8_t  h_time;         /* DIVE TIME 底部，默认 4U */
    uint8_t  gap_u;          /* 组件间距，默认 1U */
    uint8_t  title_h_u;      /* 标题区高度，默认 2U */

    /* --- 6. 卡片排序（存卡片 ID，长度由 AREX_CARD_COUNT 决定） --- */
    uint8_t  card_order[CARD_ID_COUNT]; /* 默认 {0,1,2,3,4,5} */

    /* --- 7. 5x6 自定义网格（5F Custom Widgets） --- */
    uint8_t  widget_count;
    uint8_t  widget_ids[AREX_MAX_WIDGETS];
    uint8_t  widget_col[AREX_MAX_WIDGETS];   /* 起始列 0~4 */
    uint8_t  widget_row[AREX_MAX_WIDGETS];   /* 起始行 0~5 */
    uint8_t  widget_w[AREX_MAX_WIDGETS];     /* 列跨度 1 或 2 */
    uint8_t  widget_h[AREX_MAX_WIDGETS];     /* 行跨度 1 或 2 */

} arex_sys_config_t;
#pragma pack(pop)

/* =========================================================
 * 4. 实时传感器数据总线（RAM Only，仅由底层定时器写入）
 *    UI 定时器只读不写，永远不触发重新排版。
 * ========================================================= */
typedef struct {
    float    depth_m;         /* 当前深度，单位 m */
    float    max_depth_m;     /* 最大深度 */
    uint16_t heading_deg;     /* 航向 0~359 */
    uint16_t dive_time_sec;   /* 潜水时长，秒 */
    int16_t  ndl_min;         /* 免减压时间，负数表示在减压 */
    uint16_t tts_min;         /* Time To Surface */
    uint16_t next_stop_m;     /* 下一停留站深度 */
    uint8_t  next_stop_min;   /* 下一停留站时长 */
    uint16_t pod1_bar;        /* 气瓶1压力 */
    uint16_t pod2_bar;        /* 气瓶2压力 */
    float    ppo2[3];         /* [当前, 上限, 下限] */
    uint8_t  battery_pct;     /* 电量百分比 */
    uint8_t  tissue_pct[16];  /* 16节组织饱和度 0~110 */
    uint8_t  gf99;            /* GF99 */
    uint8_t  surf_gf;         /* 水面 GF，>100 触发报警 */
    uint8_t  cns_pct;         /* CNS% */
    uint16_t otu;             /* OTU */
} arex_sensor_data_t;

/* =========================================================
 * 5. 布局推算缓存（arex_ui_apply_config 执行后填充，只读）
 *    卡片/标签的 update_cb 可直接读取，无需重复计算
 * ========================================================= */
typedef struct {
    /* 安全区容器几何 */
    lv_obj_t *safe_zone;      /* ui_safe_zone 父容器句柄 */

    /* 左锚区（相对 safe_zone 的绝对坐标） */
    lv_obj_t *left_anchor;
    int16_t   la_x, la_y;
    int16_t   la_w, la_h;

    /* 右卡片区（相对 safe_zone 的绝对坐标） */
    lv_obj_t *right_canvas;
    int16_t   rc_x, rc_y;
    int16_t   rc_w, rc_h;

    /* 10U 块绝对坐标缓存（Y 坐标，相对 left_anchor） */
    int16_t   block_y[6];     /* [0]=DEPTH [1]=NDL [2]=POD [3]=BATT [4]=GAS [5]=TIME */
    int16_t   block_h[6];     /* 每块像素高度 */

    /* 右侧卡片电梯：每张卡片的 Y 坐标（相对 right_canvas） */
    int16_t   card_y[AREX_CARD_COUNT];
    int16_t   card_h;         /* 单卡高度 = rc_h */

    /* 5F 网格单元尺寸 */
    int16_t   grid_unit_w;    /* = rc_w / 5 */
    int16_t   grid_unit_h;    /* = rc_h / 6 */

} arex_layout_cache_t;

/* =========================================================
 * 6. 全局单例暴露
 * ========================================================= */
extern arex_sys_config_t   g_sys_config;
extern arex_sensor_data_t  g_sensor;
extern arex_layout_cache_t g_layout;

/* =========================================================
 * 7. 内联坐标推算工具宏
 *    所有布局代码只能使用这些宏，严禁在 UI 代码里硬编码 px 值
 * ========================================================= */
/* U 单位转 px */
#define U2PX(u)             ((u) * AREX_BASE_U)

/* 对齐枚举 → LVGL 文本对齐 */
#define AREX_ALIGN_TO_LV(a) ((a) == AREX_ALIGN_LEFT   ? LV_TEXT_ALIGN_LEFT  : \
                             ((a) == AREX_ALIGN_CENTER ? LV_TEXT_ALIGN_CENTER: \
                                                         LV_TEXT_ALIGN_RIGHT))

/* 对齐枚举 → lv_obj_align 常量（用于图标/子件定位） */
#define AREX_ALIGN_TO_OBJ(a) ((a) == AREX_ALIGN_LEFT   ? LV_ALIGN_LEFT_MID   : \
                              ((a) == AREX_ALIGN_CENTER ? LV_ALIGN_CENTER      : \
                                                          LV_ALIGN_RIGHT_MID))

/* 安全的 alpha → LV_OPA 转换（0~255） */
#define AREX_ALPHA_TO_OPA(a) ((lv_opa_t)(a))

/* =========================================================
 * 8. API 接口
 * ========================================================= */

/* 初始化：加载默认配置、创建 safe_zone、首次调用 apply */
void arex_ui_engine_init(void);

/* 配置变更后调用：重新推算所有绝对坐标并刷新布局
   只移动/缩放 lv_obj，不销毁重建 */
void arex_ui_apply_config(void);

/* 传感器数据已更新后调用：仅刷新 lv_label 文本，绝不排版 */
void arex_ui_update_data(void);

/* 默认配置填充（首次上电或恢复出厂） */
void arex_sys_config_default(arex_sys_config_t *cfg);

/* 工具：根据当前配置获取左锚区总高度（Classic 模式需要此值） */
int16_t arex_layout_calc_anchor_h(const arex_sys_config_t *cfg);

/* 工具：获取卡片区实际高度（含零高度防坍塌保护） */
int16_t arex_layout_calc_right_h(const arex_sys_config_t *cfg, int16_t anchor_h);

#endif /* AREX_UI_ENGINE_H */
