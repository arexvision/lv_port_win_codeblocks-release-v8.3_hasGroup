# AREX UI 架构现状拓扑报告

> 生成日期：2026-04-29
> 用途：BLE 二进制协议设计与 Data Bus 极简解耦架构

---

## 第一部分：核心模块字典 (Enums)

### 1.1 右侧卡片 ID 枚举

**文件：** `src/arex_ui/arex_card_registry.h`

```c
typedef enum {
    CARD_ID_INFO         = 0,
    CARD_ID_COMPASS      = 1,
    CARD_ID_DECO         = 2,
    CARD_ID_GAS          = 3,
    CARD_ID_PLAN         = 4,
    CARD_ID_CUSTOM_GRID  = 5,   /* 5F 自定义网格卡片 */
    CARD_ID_SETUP        = 6,
    CARD_ID_COUNT               /* 总数 = 7 */
} arex_card_id_t;
```

### 1.2 卡片位置枚举（tileview 显示顺序）

**文件：** `src/arex_ui/arex_card_registry.h`

```c
typedef enum {
    CARD_POS_INFO  = 0,
    CARD_POS_1     = 1,   /* 可重排区间 */
    CARD_POS_2     = 2,
    CARD_POS_3     = 3,
    CARD_POS_4     = 4,
    CARD_POS_5     = 5,
    CARD_POS_SETUP = 6,   /* 固定 tile 6 */
    CARD_POS_COUNT
} arex_card_pos_t;

#define AREX_CARD_COUNT      CARD_ID_COUNT   /* = 7 */
#define AREX_DASH_CARD_COUNT (AREX_CARD_COUNT - 2)  /* = 5 (排除 INFO/SETUP) */
```

### 1.3 卡片引擎类型枚举

**文件：** `src/arex_ui/arex_card_registry.h`

```c
typedef enum {
    CARD_ENGINE_MENU   = 0,   /* arex_render_dynamic_menu()     */
    CARD_ENGINE_GRID   = 1,   /* arex_render_5f_custom_grid()   */
    CARD_ENGINE_CHART  = 2,   /* reserved                       */
    CARD_ENGINE_CUSTOM = 3,   /* create_cb() full control       */
} arex_card_engine_t;
```

### 1.4 组件 ID 枚举（左侧锚点 + 5F 网格共用）

**文件：** `src/arex_ui/arex_ui_engine.h`

```c
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
    AREX_WIDGET_COUNT             /* 总数 = 14 */
} arex_widget_id_t;
```

### 1.5 其他辅助枚举

**文件：** `src/arex_ui/arex_ui_engine.h`

```c
/* 字体 ID（APP 下发数字 ID，引擎映射） */
typedef enum {
    AREX_FONT_ID_SMALL  = 0,  /* 14px */
    AREX_FONT_ID_TITLE  = 1,  /* 20px */
    AREX_FONT_ID_MEDIUM = 2,  /* 28px */
    AREX_FONT_ID_HUGE   = 3,  /* 48px */
} arex_font_id_t;

/* 主题模式 */
typedef enum {
    AREX_THEME_TECH    = 0,   /* Left Grid + Right Cards（当前使用） */
    AREX_THEME_CLASSIC = 1    /* 上下流式布局（预留，渲染代码未实现） */
} arex_theme_t;

/* 告警级别 */
typedef enum {
    AREX_ALARM_NONE  = 0,
    AREX_ALARM_INFO  = 1,
    AREX_ALARM_WARN  = 2,
    AREX_ALARM_CRIT  = 3,
} arex_alarm_level_t;

/* 分割线样式 */
typedef enum {
    AREX_SEP_NONE   = 0,
    AREX_SEP_SOLID  = 1,
    AREX_SEP_DASHED = 2,
    AREX_SEP_DOTTED = 3,
} arex_sep_style_t;
```

---

## 第二部分：全局配置结构体 (Global Config Struct)

### 2.1 核心配置结构体 — `arex_sys_config_t`

**文件：** `src/arex_ui/arex_ui_engine.h`
**特性：** `#pragma pack(push, 1)` — 1 字节对齐，可直接用于 BLE 二进制传输

```c
#pragma pack(push, 1)
typedef struct {
    /* === 安全区 === */
    uint16_t safe_zone_w;    /* 默认 580 */
    uint16_t safe_zone_h;    /* 默认 420 */
    int16_t  offset_x;      /* 瞳距校准 (IPD) */
    int16_t  offset_y;      /* 浮力盲区校准 */

    /* === 全局架构与行为 === */
    uint8_t  theme_mode;      /* arex_theme_t: 0=Left Grid+Right Cards(当前), 1=Classic上下(预留) */
    uint8_t  layout_order;    /* arex_order_t */
    uint8_t  dots_position;   /* arex_dots_pos_t */
    uint8_t  compass_style;    /* arex_compass_style_t */
    uint8_t  flash_speed;     /* 动画闪烁速度 (0=慢, 1=中, 2=快) */
    bool     mask_enabled;     /* 面镜盲区掩膜开关 */

    /* === 样式与对齐 === */
    bool     split_outward;    /* 双拼模块向外展开 */

    /* === 分割线系统 === */
    uint8_t  sep_style;       /* arex_sep_style_t */
    uint8_t  sep_thick;       /* 线条粗细 px */
    uint8_t  sep_alpha;       /* 透明度 0~255 */

    /* === 10U 网格高度分配 (1U = 10px) === */
    uint8_t  h_depth;         /* DEPTH 大通栏 (默认 8U) */
    uint8_t  h_ndl;           /* NDL/TTS 双拼 (默认 6U) */
    uint8_t  h_pod;           /* POD 1/2 双拼 (默认 6U) */
    uint8_t  h_batt;          /* BATT/W.TIME 双拼 (默认 5U) */
    uint8_t  h_gas;           /* GAS 中通栏 (默认 6U) */
    uint8_t  h_time;          /* DIVE TIME 底部 (默认 5U) */
    uint8_t  gap_u;           /* 模块间距 (默认 1U) */
    uint8_t  panel_gap_u;     /* 左侧锚点与右侧面板间距 (默认 1U) */
    uint8_t  title_h_u;      /* 标题高度 (默认 2U) */
    uint8_t  h_menu_item;    /* 菜单项高度 (默认 5U=50px) */
    uint8_t  gap_menu;        /* 菜单项间距 (默认 1U=10px) */
    uint8_t  h_tissues_chart; /* 组织柱图高度 (默认 9U=90px) */

    /* === 5F 自定义网格 (5列x6行，最多30个组件) === */
    uint8_t  widget_count;                    /* 当前装填数量 (最多30) */
    uint8_t  widget_ids[30];                  /* 组件类型: arex_widget_id_t */
    uint8_t  widget_r[30];                    /* 起始行 0~5 */
    uint8_t  widget_c[30];                    /* 起始列 0~4 */
    uint8_t  widget_w[30];                    /* 列跨度 1~2 */
    uint8_t  widget_h[30];                    /* 行跨度 1~2 */

    /* === 卡片滑动顺序 === */
    uint8_t  card_order[7];                   /* card_order[pos] = card_id */

    /* === 用户设置 === */
    float    mod_ppo2;           /* 默认 1.4f */
    uint8_t  conservatism;       /* 默认 1 (MED) */
    uint8_t  brightness;         /* 默认 2 (HIGH) */

} arex_sys_config_t;
#pragma pack(pop)
```

### 2.2 左侧 2x6 网格配置结构体（独立于 `arex_sys_config_t`）

**文件：** `src/arex_ui/arex_ui_engine.h`

> **重要：** 左侧锚点的网格配置数据存在**独立的全局数组** `g_left_widgets[]` 中，不在 `arex_sys_config_t` 内部。

```c
#define AREX_LEFT_COLS   2
#define AREX_LEFT_ROWS   6
#define AREX_LEFT_CELL_W 80   /* 80px per cell */
#define AREX_LEFT_CELL_H 60   /* 60px per cell */
#define AREX_LEFT_GRID_W 160  /* 2 * 80 */
#define AREX_LEFT_GRID_H 360  /* 6 * 60 */
#define AREX_LEFT_MAX_WIDGETS 12  /* 最多 12 个组件 */

typedef struct {
    arex_widget_id_t widget_id;  /* 组件类型 ID (0~13) */
    uint8_t x;                  /* 列索引 0~1 */
    uint8_t y;                  /* 行索引 0~5 */
    uint8_t w;                  /* 跨越列数 1~2 */
    uint8_t h;                  /* 跨越行数 1~2 */
    uint8_t font_id;            /* 字号: arex_font_id_t (0~3) */
} arex_custom_widget_cfg_t;

/* 独立于 arex_sys_config_t 的全局数组 */
extern arex_custom_widget_cfg_t g_left_widgets[AREX_LEFT_MAX_WIDGETS];
extern uint8_t g_left_widget_count;
```

### 2.3 实时数据总线 — `arex_sensor_data_t`

**文件：** `src/arex_ui/arex_ui_engine.h`

```c
typedef struct {
    /* 左侧锚点数据 */
    float   depth;              /* 当前深度 m */
    int16_t ndl;               /* 免减压时间 min */
    uint16_t tts;              /* 回到水面时间 min */
    float   pod1_bar;          /* 气瓶1压力 bar */
    float   pod2_bar;          /* 气瓶2压力 bar */
    float   ppo2[3];           /* 三段 PO2 */
    float   battery_pct;        /* 电池百分比 */

    /* 气体信息 */
    uint8_t  gas_active_idx;
    char     gas_name[16];

    /* 罗盘数据 */
    uint16_t heading;           /* 当前航向 0~359 */
    bool     heading_locked;
    uint16_t heading_target;

    /* 潜水时间 */
    uint32_t dive_time_s;
    uint32_t surface_time_s;    /* WTM 水面休息时间 */

    /* 减压/组织数据 */
    uint8_t  tissue_pct[16];    /* 16 隔室饱和度 */
    uint8_t  cns_pct;          /* CNS 氧中毒百分比 */
    uint16_t otu;              /* OTU 氧中毒剂量 */
    int16_t  next_stop_m;      /* 下一站深度 m */
    uint8_t  next_stop_min;    /* 下一站停留时间 min */
    bool     deco_violation;   /* 减压违规标志 */

    /* System Data */
    float    temperature_c;     /* 设备/水温温度 */
    bool     strobe_on;        /* 频闪灯开关 */
    bool     flashlight_on;     /* 手电筒开关 */
    uint8_t  cylinder_count;   /* 气瓶连接数量 */

    /* Data Bus 脏标记位域 */
    uint32_t dirty_mask;       /* 16 个 DIRTY_* 位掩码 */

} arex_sensor_data_t;
```

### 2.4 脏标记位枚举

```c
typedef enum {
    DIRTY_NONE     = 0,
    DIRTY_DEPTH    = (1U << 0),   /* 深度数据 */
    DIRTY_NDL      = (1U << 1),   /* 免减压时间 */
    DIRTY_TTS      = (1U << 2),   /* 回到水面时间 */
    DIRTY_POD      = (1U << 3),   /* 气瓶压力 */
    DIRTY_BATT     = (1U << 4),   /* 电池电量 */
    DIRTY_HEADING  = (1U << 5),   /* 罗盘航向 */
    DIRTY_TIME     = (1U << 6),   /* 潜水时间 / W.TIME */
    DIRTY_PPO2     = (1U << 7),   /* PO2 值 */
    DIRTY_GAS      = (1U << 8),   /* 气体切换 */
    DIRTY_ALARM    = (1U << 9),   /* 告警状态 */
    DIRTY_DECO     = (1U << 10),  /* 减压站序列 */
    DIRTY_TEMP     = (1U << 11),  /* 温度数据 */
    DIRTY_DEVICES  = (1U << 12),  /* 外设状态 */
    DIRTY_TISSUES  = (1U << 13),  /* 16 组织舱饱和度数组 */
    DIRTY_CNS      = (1U << 14),  /* CNS 百分比 */
    DIRTY_OTU      = (1U << 15),  /* OTU 值 */
} arex_dirty_bit_t;
```

---

## 第三部分：假数据初始化入口 (Mock Data)

### 3.1 全局单例定义

**文件：** `src/arex_ui/arex_ui_engine.c`

```c
arex_sys_config_t  g_sys_config;      /* 布局配置全局实例 */
arex_sensor_data_t g_sensor_data;     /* 实时数据全局实例（可原子读写） */

/* 左侧 2x6 网格配置数组（独立于 g_sys_config） */
arex_custom_widget_cfg_t g_left_widgets[AREX_LEFT_MAX_WIDGETS] = {0};
uint8_t g_left_widget_count = 0;
```

### 3.2 配置默认值填充 — `arex_sys_config_defaults()`

**文件：** `src/arex_ui/arex_ui_engine.c`
**调用链：** `arex_ui_init()` → `arex_sys_config_defaults(&g_sys_config)`

```c
void arex_sys_config_defaults(arex_sys_config_t *cfg)
{
    memset(cfg, 0, sizeof(arex_sys_config_t));

    /* 安全区 */
    cfg->safe_zone_w  = 580;
    cfg->safe_zone_h  = 420;
    cfg->offset_x     = 0;
    cfg->offset_y     = -10;

    /* 架构 */
    cfg->theme_mode    = AREX_THEME_TECH;    /* 当前固定 Left Grid + Right Cards */
    cfg->layout_order  = AREX_ORDER_NORMAL;
    cfg->dots_position = AREX_DOTS_RIGHT;
    cfg->compass_style = AREX_COMPASS_CLASSIC;
    cfg->flash_speed   = 1;
    cfg->mask_enabled  = false;
    cfg->split_outward = true;

    /* 分割线 */
    cfg->sep_style  = AREX_SEP_DASHED;
    cfg->sep_thick  = 2;
    cfg->sep_alpha  = 51;

    /* 10U 高度分配 */
    cfg->h_depth   = 8;
    cfg->h_ndl     = 6;
    cfg->h_pod     = 6;
    cfg->h_batt    = 5;
    cfg->h_gas     = 6;
    cfg->h_time    = 5;
    cfg->gap_u     = 0;
    cfg->panel_gap_u   = 1;
    cfg->title_h_u     = 2;
    cfg->h_menu_item   = 5;
    cfg->gap_menu      = 1;
    cfg->h_tissues_chart = 9;

    /* === 5F 网格配置（5列x6行，最多30个组件） === */
    cfg->widget_count = 12;
    /*  id                r  c  w  h */
    cfg->widget_ids[0]  = AREX_WIDGET_DEPTH;    cfg->widget_r[0] = 0; cfg->widget_c[0] = 0; cfg->widget_w[0] = 2; cfg->widget_h[0] = 2;
    cfg->widget_ids[1]  = AREX_WIDGET_TEMP;     cfg->widget_r[1] = 0; cfg->widget_c[1] = 2; cfg->widget_w[1] = 1; cfg->widget_h[1] = 1;
    cfg->widget_ids[2]  = AREX_WIDGET_HEADING;  cfg->widget_r[2] = 0; cfg->widget_c[2] = 3; cfg->widget_w[2] = 2; cfg->widget_h[2] = 1;
    cfg->widget_ids[3]  = AREX_WIDGET_SAC_RATE; cfg->widget_r[3] = 2; cfg->widget_c[3] = 0; cfg->widget_w[3] = 2; cfg->widget_h[3] = 1;
    cfg->widget_ids[4]  = AREX_WIDGET_BATTERY;  cfg->widget_r[4] = 2; cfg->widget_c[4] = 2; cfg->widget_w[4] = 2; cfg->widget_h[4] = 1;
    cfg->widget_ids[5]  = AREX_WIDGET_PPO2;     cfg->widget_r[5] = 2; cfg->widget_c[5] = 4; cfg->widget_w[5] = 1; cfg->widget_h[5] = 1;
    cfg->widget_ids[6]  = AREX_WIDGET_NDL;      cfg->widget_r[6] = 3; cfg->widget_c[6] = 0; cfg->widget_w[6] = 2; cfg->widget_h[6] = 1;
    cfg->widget_ids[7]  = AREX_WIDGET_TTS;      cfg->widget_r[7] = 3; cfg->widget_c[7] = 2; cfg->widget_w[7] = 2; cfg->widget_h[7] = 1;
    cfg->widget_ids[8]  = AREX_WIDGET_CNS;      cfg->widget_r[8] = 3; cfg->widget_c[8] = 4; cfg->widget_w[8] = 1; cfg->widget_h[8] = 1;
    cfg->widget_ids[9]  = AREX_WIDGET_POD1;     cfg->widget_r[9] = 4; cfg->widget_c[9] = 0; cfg->widget_w[9] = 2; cfg->widget_h[9] = 1;
    cfg->widget_ids[10] = AREX_WIDGET_POD2;     cfg->widget_r[10] = 4; cfg->widget_c[10] = 2; cfg->widget_w[10] = 2; cfg->widget_h[10] = 1;
    cfg->widget_ids[11] = AREX_WIDGET_WTIME;     cfg->widget_r[11] = 4; cfg->widget_c[11] = 4; cfg->widget_w[11] = 1; cfg->widget_h[11] = 1;
    cfg->widget_ids[11] = AREX_WIDGET_WTIME;    cfg->widget_r[11] = 5; cfg->widget_c[11] = 0; cfg->widget_w[11] = 1; cfg->widget_h[11] = 1;

    /* === 左侧 2x6 网格配置（独立数组 g_left_widgets[]） === */
    g_left_widget_count = 6;
    g_left_widgets[0] = (arex_custom_widget_cfg_t){ AREX_WIDGET_NDL,    0, 0, 2, 1, AREX_FONT_ID_MEDIUM };
    g_left_widgets[1] = (arex_custom_widget_cfg_t){ AREX_WIDGET_DEPTH,  0, 1, 2, 2, AREX_FONT_ID_HUGE   };
    g_left_widgets[2] = (arex_custom_widget_cfg_t){ AREX_WIDGET_WTIME,  0, 3, 2, 1, AREX_FONT_ID_MEDIUM };
    g_left_widgets[3] = (arex_custom_widget_cfg_t){ AREX_WIDGET_GAS,    0, 4, 2, 1, AREX_FONT_ID_MEDIUM };
    g_left_widgets[4] = (arex_custom_widget_cfg_t){ AREX_WIDGET_POD1,   0, 5, 1, 1, AREX_FONT_ID_SMALL  };
    g_left_widgets[5] = (arex_custom_widget_cfg_t){ AREX_WIDGET_POD2,   1, 5, 1, 1, AREX_FONT_ID_SMALL  };

    /* === 卡片滑动顺序 === */
    cfg->card_order[CARD_POS_INFO]  = CARD_ID_INFO;         /* 固定 tile 0 */
    cfg->card_order[CARD_POS_1]      = CARD_ID_CUSTOM_GRID;  /* 5F 自定义网格 */
    cfg->card_order[CARD_POS_2]      = CARD_ID_DECO;
    cfg->card_order[CARD_POS_3]      = CARD_ID_COMPASS;
    cfg->card_order[CARD_POS_4]      = CARD_ID_GAS;
    cfg->card_order[CARD_POS_5]      = CARD_ID_PLAN;
    cfg->card_order[CARD_POS_SETUP]  = CARD_ID_SETUP;        /* 固定 tile 6 */

    /* 用户设置 */
    cfg->mod_ppo2     = 1.4f;
    cfg->conservatism = 1;     /* MED */
    cfg->brightness   = 2;     /* HIGH */
}
```

### 3.3 UI 初始化入口 — `arex_ui_init()`

**文件：** `src/arex_ui/arex_ui_engine.c`
**调用链：** `UI_main()` → `arex_ui_init()`

```c
void arex_ui_init(void)
{
    /* 1. 加载布局默认值 */
    arex_sys_config_defaults(&g_sys_config);

    /* 2. 清零实时数据 */
    memset(&g_sensor_data, 0, sizeof(g_sensor_data));

    /* 3. 填充模拟传感器数据（PC 仿真阶段） */
    g_sensor_data.depth           = 0.0f;
    g_sensor_data.ndl             = 5;
    g_sensor_data.tts             = 24;
    g_sensor_data.pod1_bar        = 0.0f;   /* 0 = "--" (未连接) */
    g_sensor_data.pod2_bar        = 0.0f;   /* 0 = "--" (未连接) */
    g_sensor_data.battery_pct     = 85.0f;
    g_sensor_data.heading          = 265;
    g_sensor_data.dive_time_s     = 0;     /* 启动后开始计时 */
    g_sensor_data.surface_time_s  = 0;
    g_sensor_data.gas_active_idx  = 2;
    strcpy(g_sensor_data.gas_name, "AIR");
    g_sensor_data.ppo2[0]         = 1.2f;
    g_sensor_data.ppo2[1]         = 1.2f;
    g_sensor_data.ppo2[2]         = 1.3f;
    g_sensor_data.cns_pct         = 15;
    g_sensor_data.otu             = 22;
    g_sensor_data.next_stop_m     = 21;
    g_sensor_data.next_stop_min   = 3;
    g_sensor_data.temperature_c   = 25.0f;
    g_sensor_data.strobe_on       = true;
    g_sensor_data.flashlight_on   = true;
    g_sensor_data.cylinder_count  = 1;
}
```

### 3.4 定时器驱动模拟数据写入

**文件：** `src/UI_main.c`
**定时器：** `sim_tick_cb` — 1Hz lv_timer

```c
/* 模拟深度缓慢上升（每秒 0.1m） */
g_sensor_data.depth += 0.1f;
if (g_sensor_data.depth > 50.0f)
    g_sensor_data.depth = 0.0f;

/* 模拟航向漂移 */
g_sensor_data.heading = (g_sensor_data.heading + 1) % 360;

/* 模拟潜水时间递增 */
g_sensor_data.dive_time_s++;
g_sensor_data.surface_time_s++;

/* 通过 Data Bus Setter 写入（打脏标记） */
arex_bus_set_depth(g_sensor_data.depth);
arex_bus_set_heading(g_sensor_data.heading);
arex_bus_set_dive_time(g_sensor_data.dive_time_s);
```

---

## 附录：结构体内存布局速查

### A.1 `arex_sys_config_t` 二进制布局

| 字段 | 类型 | 字节数 | 偏移 | 说明 |
|------|------|--------|------|------|
| `safe_zone_w/h` | uint16_t | 2+2 | 0/2 | 安全区宽高 |
| `offset_x/y` | int16_t | 2+2 | 4/6 | IPD/浮力校准 |
| `theme_mode` | uint8_t | 1 | 8 | 主题 |
| `layout_order` | uint8_t | 1 | 9 | 翻转 |
| `dots_position` | uint8_t | 1 | 10 | 滚动点位置 |
| `compass_style` | uint8_t | 1 | 11 | 罗盘样式 |
| `flash_speed` | uint8_t | 1 | 12 | 闪烁速度 |
| `mask_enabled` | bool | 1 | 13 | 掩膜开关 |
| `split_outward` | bool | 1 | 14 | 双拼方向 |
| `sep_style/thick/alpha` | uint8_t | 3 | 15/16/17 | 分割线 |
| `h_depth/ndl/pod/batt/gas/time` | uint8_t | 6 | 18~23 | 高度分配 |
| `gap_u/panel_gap_u/title_h_u` | uint8_t | 3 | 24/25/26 | 间距/标题 |
| `h_menu_item/gap_menu/h_tissues_chart` | uint8_t | 3 | 27/28/29 | 菜单 |
| `widget_count` | uint8_t | 1 | 30 | 组件数量 |
| `widget_ids[30]` | uint8_t | 30 | 31~60 | 5F 组件类型 |
| `widget_r[30]` | uint8_t | 30 | 61~90 | 5F 起始行 |
| `widget_c[30]` | uint8_t | 30 | 91~120 | 5F 起始列 |
| `widget_w[30]` | uint8_t | 30 | 121~150 | 5F 列跨度 |
| `widget_h[30]` | uint8_t | 30 | 151~180 | 5F 行跨度 |
| `card_order[7]` | uint8_t | 7 | 181~187 | 卡片顺序 |
| `mod_ppo2` | float | 4 | 188~191 | 用户设置 |
| `conservatism` | uint8_t | 1 | 192 | 保守度 |
| `brightness` | uint8_t | 1 | 193 | 亮度 |
| **总计** | | **194 字节** | | 1 字节对齐 |

### A.2 `g_left_widgets[]` 二进制布局

每个 `arex_custom_widget_cfg_t` = 6 字节：

| 字段 | 类型 | 字节数 | 偏移 |
|------|------|--------|------|
| `widget_id` | uint8_t | 1 | 0 |
| `x` | uint8_t | 1 | 1 |
| `y` | uint8_t | 1 | 2 |
| `w` | uint8_t | 1 | 3 |
| `h` | uint8_t | 1 | 4 |
| `font_id` | uint8_t | 1 | 5 |

当前默认 6 个组件 = **36 字节**（最大 72 字节 / 12 个）

### A.3 BLE 协议包建议分包

```
包 ID 0x01: arex_sys_config_t 全量 (194 bytes)
包 ID 0x02: g_left_widgets[] 全量 (36~72 bytes)
包 ID 0x03: 传感器数据快照 (g_sensor_data 子集，~50 bytes)
```

---

## 附录 B：BLE 布局同步 — Data Bus 极简解耦（v2026-04-29）

### B.1 设计目标与边界铁律

UI 文件夹作为**100% 纯净的渲染黑盒**，禁止包含任何 KV、Flash 读写或 CRC 校验逻辑。

```
┌─────────────────────┐     arex_bus_set_ui_layout()      ┌─────────────────────┐
│     BLE 任务侧      │ ─────────────────────────────────▶ │      UI 引擎侧       │
│  (真机 MCU / 模拟器) │                                    │   (arex_data.c)     │
└─────────┬───────────┘                                    └──────────┬──────────┘
          │                                                          │
          │ 1. CRC-16 校验                                           │ 4. 临界区 memcpy
          │ 2. Flash/KV 持久化                                        │ 5. DIRTY_UI_LAYOUT
          │ 3. 调用 arex_bus_set_ui_layout()                          │ 6. arex_ui_update_task()
          │                                                          │    重建布局
          ▼                                                          ▼
   Flash / KV 持久化                                    arex_screen_rebuild_layout()
```

| 模块 | 职责 | 禁止操作 |
|------|------|----------|
| BLE 任务 | 接收帧、CRC 校验、写入 Flash/KV、调用 Data Bus | 禁止调用 LVGL |
| Data Bus (`arex_data.c`) | 临界区 memcpy + 打脏标记 | 禁止调用 LVGL / Flash |
| UI 消费任务 (`arex_ui_update_task`) | 接收 `DIRTY_UI_LAYOUT` + 调用 `arex_screen_rebuild_layout()` | 禁止读 Flash |

### B.2 BLE 二进制帧结构体

**文件：** `src/arex_ui/arex_data.h`

```c
#pragma pack(push, 1)

/* 左侧 2x6 组件描述 (6 Bytes) */
typedef struct {
    uint8_t id;         /* arex_widget_id_t (0~13) */
    uint8_t x;          /* 列索引 0~1 */
    uint8_t y;          /* 行索引 0~5 */
    uint8_t w;          /* 列跨度 1~2 */
    uint8_t h;          /* 行跨度 1~2 */
    uint8_t font_id;    /* arex_font_id_t (0~3) */
} ble_sync_left_widget_t;

/* 5F 自定义组件描述 (5 Bytes) */
typedef struct {
    uint8_t id;         /* arex_widget_id_t (0~13) */
    uint8_t r;          /* 起始行 0~5 */
    uint8_t c;          /* 起始列 0~4 */
    uint8_t w;          /* 列跨度 1~2 */
    uint8_t h;          /* 行跨度 1~2 */
} ble_sync_5f_widget_t;

/* BLE UI 布局同步帧 — 总大小 184 字节，可单帧传输
 * 字节数: version(1) + card_order[7](7) + left_count(1)
 *       + left_widgets[12]×6B(72) + custom_5f_count(1)
 *       + custom_5f_widgets[20]×5B(100) + crc16(2)
 *       = 184 字节
 */
typedef struct {
    uint8_t  version;                     /* 协议版本 0x01 */
    uint8_t  card_order[7];               /* 卡片滑动顺序 */
    uint8_t  left_count;                  /* 左侧组件数量 */
    ble_sync_left_widget_t left_widgets[12];
    uint8_t  custom_5f_count;             /* 5F 组件数量 */
    ble_sync_5f_widget_t custom_5f_widgets[20];
    uint16_t crc16;                      /* CRC-16/XMODEM 校验和 */
} arex_ble_ui_sync_payload_t;
#pragma pack(pop)

#define AREX_BLE_CFG_VERSION  0x01
#define AREX_BLE_FRAME_SIZE   sizeof(arex_ble_ui_sync_payload_t)
```

### B.3 新增脏标记

```c
typedef enum {
    DIRTY_NONE      = 0,
    DIRTY_DEPTH     = (1U << 0),
    // ... DIRTY_CNS / DIRTY_OTU ...
    DIRTY_UI_LAYOUT = (1U << 16),  /* UI 布局重建（BLE 配置同步触发） */
} arex_dirty_bit_t;
```

### B.4 Data Bus API

| 函数 | 所在文件 | 触发方 | 作用 |
|------|----------|--------|------|
| `arex_bus_set_ui_layout()` | `arex_data.c` | BLE 任务 | 临界区 memcpy + 触发 `DIRTY_UI_LAYOUT` |

**真机 BLE 任务调用示例（伪代码）：**

```c
case 0x45: {  /* UI 布局同步帧 */
    arex_ble_ui_sync_payload_t *payload = (arex_ble_ui_sync_payload_t *)frame->payload;

    /* 1. BLE 层负责校验 CRC */
    uint16_t calc_crc = crc16(payload, sizeof(arex_ble_ui_sync_payload_t) - 2);
    if (calc_crc != payload->crc16) {
        ble_send_status_ack(env, frame->seq, frame->func, BLE_UI_ACK_CRC_ERROR);
        break;
    }

    /* 2. BLE 层负责将配置写入 Flash */
    my_hardware_kv_set("UI_CFG", payload, sizeof(arex_ble_ui_sync_payload_t));

    /* 3. 一切换写安全后，调用 UI Data Bus 接口 */
    arex_bus_set_ui_layout(payload);

    ble_send_status_ack(env, frame->seq, frame->func, BLE_UI_ACK_SUCCESS);
    break;
}
```

### B.5 帧处理流程

```
BLE 收到帧 (0x45 CMD)
  │
  ├─ CRC-16 校验（BLE 任务侧负责）
  ├─ Flash/KV 持久化（BLE 任务侧负责）
  │
  ▼
arex_bus_set_ui_layout(payload)      // arex_data.c
  │
  ├─ 版本号校验 (version == 0x01)
  ├─ 临界区保护 memcpy
  │     ├─ card_order[] → g_sys_config
  │     ├─ left_widgets[] → g_left_widgets[]
  │     └─ custom_5f_widgets[] → g_sys_config.widget_*
  └─ g_sensor_data.dirty_mask |= DIRTY_UI_LAYOUT
        │
        ▼
arex_ui_update_task() (50ms)
  │
  └─ mask & DIRTY_UI_LAYOUT
        │
        ├─ lv_disp_enable_invalidation(false)  // 锁屏防闪烁
        ├─ arex_screen_rebuild_layout()
        │     ├─ lv_obj_clean(s_left_anchor)
        │     ├─ arex_render_left_anchor_grid()
        │     └─ right_panel_create() / tileview 重建
        ├─ lv_disp_enable_invalidation(true)
        └─ return  // 重建耗时，本帧直接退出
```

### B.6 文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `arex_data.h` | 修改 | 新增 BLE 帧结构体 + `arex_bus_set_ui_layout()` 声明 |
| `arex_data.c` | 修改 | 新增 `arex_bus_set_ui_layout()` 实现（临界区 memcpy + 脏标记） |
| `arex_ui_engine.h` | 修改 | 新增 `DIRTY_UI_LAYOUT` 枚举值 |
| `arex_ui_engine.c` | 修改 | `arex_ui_update_task()` 第一优先拦截 `DIRTY_UI_LAYOUT` |
| `UI_main.c` | 修改 | 移除 `arex_config.h` include；移除 `arex_config_init()` 调用 |
| `UI_html_DOC/UI_TOPOLOGY_REPORT.md` | 修改 | 附录 B 重构为 Data Bus 极简解耦架构 |

---

## 变更日志

| 日期 | 作者 | 描述 |
|------|------|------|
| 2026-04-29 | ClaudeCode | 初版：提取 arex_ui_engine.h/c + arex_card_registry.h 全部核心枚举和结构体定义 |
| 2026-04-29 | ClaudeCode | 新增 Config Manager 中间件（arex_config.h/c）；新增 DIRTY_UI_LAYOUT 脏标记；新增附录 B 架构文档 |
| 2026-04-29 | ClaudeCode | 撤销 arex_config 设计；BLE 帧移入 arex_data.h；改为极简 Data Bus 架构（arex_bus_set_ui_layout）；附录 B 重写 |
