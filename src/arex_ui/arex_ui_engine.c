#include "arex_ui_engine.h"
#include "arex_card_registry.h"
#include "arex_screen.h"
#include "fonts/arex_fonts.h"
#include "arex_data.h"
#include <stdio.h>
#include <string.h>

/* 减压跟踪节流时间戳（由 arex_ui_update_task 使用） */
static uint32_t _deco_last_refresh_ms = 0;

/* 气体名称表 (供全局引用) */
const char *AREX_GAS_NAMES[AREX_GAS_COUNT] = {
    "AIR",
    "NX 32",
    "TX 18/45",
    "O2 100%"
};

/* 气体 MOD 表 (单位: 米) */
const uint8_t AREX_GAS_MOD_M[AREX_GAS_COUNT] = {
    56,  /* AIR */
    34,  /* NX 32 */
    68,  /* TX 18/45 */
    6    /* O2 100% */
};

/* =========================================================
 * 全局单例定义
 * ========================================================= */
arex_sys_config_t  g_sys_config;
arex_sensor_data_t g_sensor_data;  //注意这个是全局变量，所有UI层都要用它。因为赋值是原子操作，可以放心大胆用（不需要加锁）

/*当你写下 g_sensor_data.depth = 15.5f; 时，编译器会在底层把它翻译成：
找到 g_sensor_data 的基地址 (0x20000000)。
加上 depth 的偏移量 (+0)。
直接生成一条单步汇编指令（如 STR），把 15.5 的二进制数据，像狙击枪一样，精准地打入 0x20000000 开始的 4 个字节里！
它根本不会碰 heading，也不会碰 battery。 这就是一次纯粹的、针对单个 32 位地址的“单指令写入”。因此，它是绝对原子的。
*/


/* 左侧 2x6 绝对网格配置数组
 *
 * 160x360 区域（不含底部 60px SystemData）= 2列(80px) x 6行(60px)
 * 与 5F 卡片共用 arex_widget_id_t 枚举体系。
 *
 * Grid Layout:
 *   Row 0: NDL      | (占用 2x1 = 160x60)
 *   Row 1: DEPTH    | (占用 2x2 = 160x120, 带 sudu 速率图标)
 *   Row 2: (DEPTH 第二行)
 *   Row 3: POD1     | POD2    (各占用 1x1 = 80x60)
 *   Row 4: TIME     | (占用 2x1 = 160x60)
 *   Row 5: GAS      | (占用 2x1 = 160x60，正好塞满第 6 行)
 * ========================================================= */
arex_custom_widget_cfg_t g_left_widgets[AREX_LEFT_MAX_WIDGETS] = {0};
uint8_t g_left_widget_count = 0;


/* =========================================================
 * 从 KV 持久化存储加载配置
 *
 * 模拟实现（PC 端 / 调试用）：
 *   - 直接返回 false，强制走 arex_sys_config_defaults() 默认值
 * 真机实现（替换为本函数体）：
 *   - 从 Flash/NVDS 读取 arex_sys_config_t 二进制块
 *   - 做 CRC 校验，数据损坏则返回 false
 *   - 成功返回 true，失败返回 false
 * ========================================================= */
static bool arex_config_load(arex_sys_config_t *cfg)
{
    /* TODO(真机): 替换为实际的 KV 读取实现
     *
     * 示例（伪代码）：
     *   uint8_t buf[sizeof(arex_sys_config_t)];
     *   if (nvds_get(NVDS_TAG_UI_CONFIG, sizeof(buf), buf)) {
     *       if (crc16_check(buf, sizeof(buf) - 2)) {
     *           memcpy(cfg, buf, sizeof(arex_sys_config_t));
     *           return true;
     *       }
     *   }
     *   return false;
     */
    return false;
}


/* =========================================================
 * 默认配置值
 *
 * 当前实现的布局: Left Grid + Right Cards
 *   左侧: 160x360 固定 2列(x80) x 6行(y60) 网格
 *   右侧: tileview 滑动卡片 (INFO / 5F / DECO / COMPASS / GAS / PLAN / SETUP)
 *   安全区: 580x420 由 left_anchor(160) + right_cards(420) 组成
 *
 * 字段分组:
 *   [A] 活跃字段 — 当前渲染代码实际读取
 *   [R] 预留字段 — 已定义但渲染代码未使用，为未来 Classic 上下布局预留
 * ========================================================= */
void arex_sys_config_defaults(arex_sys_config_t *cfg)
{
    memset(cfg, 0, sizeof(arex_sys_config_t));

    /* ========== [A] 安全区 ========== */
    cfg->safe_zone_w  = 580;
    cfg->safe_zone_h  = 420;
    cfg->offset_x     = 0;            /* x=0 表示水平居中（左右各留白 3U） */
    cfg->offset_y     = -10;          /* y=-10 向上偏移（上面留白 2U，下面留白 4U） */

    /* ========== [A] 架构 ========== */
    cfg->layout_order  = AREX_ORDER_REVERSE;  /* 0=标准(左锚右卡)，1=翻转(右锚左卡) */
    cfg->dots_position = AREX_DOTS_RIGHT;    /* tileview 指示点位置 */
    cfg->compass_style = AREX_COMPASS_CLASSIC;
    cfg->mask_enabled  = false;

    /* ========== [R] 主题模式预留（当前固定为 Left Grid + Right Cards） ==========
     * 可选扩展为 Classic 上下流式布局，届时渲染代码需读取以下字段：
     *   - theme_mode        → AREX_THEME_CLASSIC
     *   - h_depth / h_ndl / h_pod / h_batt / h_gas / h_time   → 上下分区高度
     *   - sep_style / sep_thick                                → 分割线样式
     *   - split_outward / flash_speed                          → 动画参数
     *   - title_h_u / h_menu_item / gap_menu                  → 菜单排版
     *   - h_tissues_chart                                     → 组织图高度
     */
    cfg->theme_mode    = AREX_THEME_TECH;    /* 当前固定 TECH（Left Grid + Right Cards） */
    cfg->sep_style     = AREX_SEP_DASHED;    /* [R] 分割线样式（待用） */
    cfg->sep_thick     = 2;                  /* [R] 线条粗细 px（待用） */
    cfg->split_outward = true;               /* [R] 双拼模块展开方向（待用） */
    cfg->flash_speed   = 1;                  /* [R] 动画闪烁速度（待用） */

    /* ========== [A] 分割线透明度 ========== */
    cfg->sep_alpha  = 51;   /* 20% of 255 — SystemData 顶部分割线透明度 */

    /* ========== [R] Classic 上下布局 10U 高度分配 (当前未使用) ==========
     * 1U = 10px，总计 10U = 100px（预留将来改为上下分区流式布局）
     * DEPTH 大通栏 → NDL/TTS 双拼 → POD 双拼 → BATT 双拼 → GAS → DIVE TIME
     */
    cfg->h_depth         = 8;   /* DEPTH 大通栏: 8U=80px */
    cfg->h_ndl           = 6;   /* NDL/TTS 双拼: 6U=60px */
    cfg->h_pod           = 6;   /* POD 1/2 双拼: 6U=60px */
    cfg->h_batt          = 5;   /* BATT/W.TIME 双拼: 5U=50px */
    cfg->h_gas           = 6;   /* GAS 中通栏: 6U=60px */
    cfg->h_time          = 5;   /* DIVE TIME 底部: 5U=50px */
    cfg->title_h_u       = 2;   /* [R] 标题高度（待用） */
    cfg->h_menu_item     = 5;   /* [R] 菜单项高度（待用） */
    cfg->gap_menu        = 1;   /* [R] 菜单项间距（待用） */
    cfg->h_tissues_chart = 9;   /* [R] 组织柱图高度（待用） */

    /* ========== [A] 面板间距 ========== */
    cfg->gap_u       = 0;   /* 左侧锚点与右侧面板间距: 0U=0px（由 sep_thick 负责分割线粗细） */
    cfg->panel_gap_u = 1;   /* tileview 容器间距: 1U=10px */

    /* ========== [A] 5F 自定义网格 (5列 x 6行) ==========
     *
     *  5列布局示意（5列=10格，6行）：
     *  col:  0  1  2  3  4
     *  row0: [DEPTH 2x2 大块    ] [TEMP  ] [HEADING 2x1 ]
     *  row2: [SAC 2x1           ] [BATT 2x1] [PPO2 1x1 ]
     *  row3: [NDL 2x1           ] [TTS 2x1 ] [CNS  1x1 ]
     *  row4: [POD1 2x1          ] [POD2 2x1] [WTIME 1x1]
     *  row5: [(POD1续)         ] [(POD2续)] [(WTIME续)]
     *
     *  widget_id → arex_widget_id_t:
     *    0=EMPTY 1=DEPTH 2=TEMP 3=HEADING 4=SAC_RATE 5=BATTERY
     *    6=NDL 7=TTS 8=PPO2 9=CNS 10=POD1 11=POD2 12=WTIME 13=GAS
     */
    cfg->widget_count = 12;
    /*  id              r  c  w  h */
    cfg->widget_ids[0] = AREX_WIDGET_DEPTH;     cfg->widget_r[0] = 0; cfg->widget_c[0] = 0; cfg->widget_w[0] = 2; cfg->widget_h[0] = 2;
    cfg->widget_ids[1] = AREX_WIDGET_TEMP;      cfg->widget_r[1] = 0; cfg->widget_c[1] = 2; cfg->widget_w[1] = 1; cfg->widget_h[1] = 1;
    cfg->widget_ids[2] = AREX_WIDGET_HEADING;   cfg->widget_r[2] = 0; cfg->widget_c[2] = 3; cfg->widget_w[2] = 2; cfg->widget_h[2] = 1;
    cfg->widget_ids[3] = AREX_WIDGET_SAC_RATE;  cfg->widget_r[3] = 2; cfg->widget_c[3] = 0; cfg->widget_w[3] = 2; cfg->widget_h[3] = 1;
    cfg->widget_ids[4] = AREX_WIDGET_BATTERY;   cfg->widget_r[4] = 2; cfg->widget_c[4] = 2; cfg->widget_w[4] = 2; cfg->widget_h[4] = 1;
    cfg->widget_ids[5] = AREX_WIDGET_PPO2;       cfg->widget_r[5] = 2; cfg->widget_c[5] = 4; cfg->widget_w[5] = 1; cfg->widget_h[5] = 1;
    cfg->widget_ids[6] = AREX_WIDGET_NDL;       cfg->widget_r[6] = 3; cfg->widget_c[6] = 0; cfg->widget_w[6] = 2; cfg->widget_h[6] = 1;
    cfg->widget_ids[7] = AREX_WIDGET_TTS;        cfg->widget_r[7] = 3; cfg->widget_c[7] = 2; cfg->widget_w[7] = 2; cfg->widget_h[7] = 1;
    cfg->widget_ids[8] = AREX_WIDGET_CNS;        cfg->widget_r[8] = 3; cfg->widget_c[8] = 4; cfg->widget_w[8] = 1; cfg->widget_h[8] = 1;
    cfg->widget_ids[9] = AREX_WIDGET_POD1;      cfg->widget_r[9] = 4; cfg->widget_c[9] = 0; cfg->widget_w[9] = 2; cfg->widget_h[9] = 1;
    cfg->widget_ids[10] = AREX_WIDGET_POD2;      cfg->widget_r[10] = 4; cfg->widget_c[10] = 2; cfg->widget_w[10] = 2; cfg->widget_h[10] = 1;
    cfg->widget_ids[11] = AREX_WIDGET_WTIME;     cfg->widget_r[11] = 4; cfg->widget_c[11] = 4; cfg->widget_w[11] = 1; cfg->widget_h[11] = 1;
    cfg->widget_ids[12] = AREX_WIDGET_WTIME;    cfg->widget_r[12] = 5; cfg->widget_c[12] = 0; cfg->widget_w[12] = 1; cfg->widget_h[12] = 1;

    /* ========== [A] 左侧 2x6 固定网格 (160x360) ==========
     * 160x360 区域 = 2列(80px) x 6行(60px)，由 arex_render_left_anchor_grid() 渲染
     *
     *  Grid Layout:
     *    Row 0: NDL      | (2x1 → 160x60)
     *    Row 1-2: DEPTH  | (2x2 → 160x120，带 sudu 速率图标)
     *    Row 3: TIME     | (2x1 → 160x60)
     *    Row 4: GAS      | (2x1 → 160x60)
     *    Row 5: POD1     | POD2    (各 1x1 → 80x60，塞满第 6 行)
     */
    g_left_widget_count = 6;
    g_left_widgets[0] = (arex_custom_widget_cfg_t){ AREX_WIDGET_NDL,    0, 0, 2, 1, AREX_FONT_ID_MEDIUM };
    g_left_widgets[1] = (arex_custom_widget_cfg_t){ AREX_WIDGET_DEPTH,  0, 1, 2, 2, AREX_FONT_ID_HUGE   };
    g_left_widgets[2] = (arex_custom_widget_cfg_t){ AREX_WIDGET_WTIME,  0, 3, 2, 1, AREX_FONT_ID_MEDIUM };
    g_left_widgets[3] = (arex_custom_widget_cfg_t){ AREX_WIDGET_GAS,    0, 4, 2, 1, AREX_FONT_ID_MEDIUM };
    g_left_widgets[4] = (arex_custom_widget_cfg_t){ AREX_WIDGET_POD1,   0, 5, 1, 1, AREX_FONT_ID_SMALL  };
    g_left_widgets[5] = (arex_custom_widget_cfg_t){ AREX_WIDGET_POD2,   1, 5, 1, 1, AREX_FONT_ID_SMALL  };

    /* ========== [A] 右侧卡片顺序 (tileview 滑动顺序) ==========
     * card_order[pos] = card_id
     * INFO(0) / SETUP(6) 固定，中间 5 张可由 APP 重排
     */
    cfg->card_order[CARD_POS_INFO]  = CARD_ID_INFO;
    cfg->card_order[CARD_POS_1]     = CARD_ID_CUSTOM_GRID;  /* 5F 自定义网格 — 默认最前 */
    cfg->card_order[CARD_POS_2]     = CARD_ID_DECO;
    cfg->card_order[CARD_POS_3]     = CARD_ID_COMPASS;
    cfg->card_order[CARD_POS_4]     = CARD_ID_GAS;
    cfg->card_order[CARD_POS_5]     = CARD_ID_PLAN;
    cfg->card_order[CARD_POS_SETUP] = CARD_ID_SETUP;

    /* ========== [A] 用户设置默认值 ========== */
    cfg->mod_ppo2       = 1.4f;
    cfg->conservatism   = 1;    /* MED */
    cfg->brightness     = 2;    /* HIGH */
}

/* =========================================================
 * 安全区边界检测
 * ========================================================= */
bool arex_safe_zone_in_danger(void)
{
    int16_t max_offset_x = (int16_t)((AREX_PHYSICAL_W - g_sys_config.safe_zone_w) / 2);
    int16_t max_offset_y = (int16_t)((AREX_PHYSICAL_H - g_sys_config.safe_zone_h) / 2);

    if (g_sys_config.offset_x < -max_offset_x || g_sys_config.offset_x > max_offset_x)
        return true;
    if (g_sys_config.offset_y < -max_offset_y || g_sys_config.offset_y > max_offset_y)
        return true;

    /* 面镜盲区掩膜检测 */
    if (g_sys_config.mask_enabled) {
        int16_t bottom_edge = (int16_t)(AREX_PHYSICAL_H / 2 + g_sys_config.safe_zone_h / 2 + g_sys_config.offset_y);
        if (bottom_edge > AREX_PHYSICAL_H - AREX_MASK_EDGE_GUARD)
            return true;
    }

    return false;
}

/* =========================================================
 * 辅助：计算 Safe Zone 内部可用区域
 * ========================================================= */
void arex_calc_layout_rect(int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h,
                           int16_t anchor_offset_x, int16_t anchor_offset_y)
{
    /* 以屏幕中心为原点，应用安全区偏移 */
    int16_t center_x = (int16_t)(AREX_PHYSICAL_W / 2) + anchor_offset_x;
    int16_t center_y = (int16_t)(AREX_PHYSICAL_H / 2) + anchor_offset_y;

    *out_x = center_x - (int16_t)(g_sys_config.safe_zone_w / 2);
    *out_y = center_y - (int16_t)(g_sys_config.safe_zone_h / 2);
    *out_w = g_sys_config.safe_zone_w;
    *out_h = g_sys_config.safe_zone_h;
}

/* =========================================================
 * Tech 模式绝对坐标推算
 *
 * 左锚点: (0, 0), 宽=160px, 高=全 safe_zone_h
 * 右卡片: (160+gap, 0), 宽=safe_zone_w-160-gap, 高=全 safe_zone_h
 * 翻转时: 交换左右 X
 * ========================================================= */
void arex_calc_tech_layout(int16_t *out_lx, int16_t *out_ly,
                           uint16_t *out_lw, uint16_t *out_lh,
                           int16_t *out_rx, int16_t *out_ry,
                           uint16_t *out_rw, uint16_t *out_rh)
{
    uint16_t gap = g_sys_config.gap_u * AREX_BASE_U;

    if (g_sys_config.layout_order == AREX_ORDER_NORMAL) {
        *out_lx = 0;
        *out_rx = (int16_t)(AREX_LEFT_ANCHOR_W + gap);
    } else {
        *out_lx = (int16_t)(g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - gap);
        *out_rx = 0;
    }

    *out_ly = 0;
    *out_ry = 0;

    *out_lw = AREX_LEFT_ANCHOR_W;
    *out_lh = g_sys_config.safe_zone_h;

    *out_rw = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W - gap;
    *out_rh = g_sys_config.safe_zone_h;
}

/* =========================================================
 * Classic 模式绝对坐标推算
 *
 * 上区: (0, 0), 宽=safe_zone_w, 高=按 10U 累加计算
 * 下区: (0, top_h+gap), 宽=safe_zone_w, 高=safe_zone_h-top_h-gap
 * 翻转时: 交换上下 Y
 * ========================================================= */
void arex_calc_classic_layout(int16_t *out_top_x, int16_t *out_top_y,
                              uint16_t *out_top_w, uint16_t *out_top_h,
                              int16_t *out_bot_x, int16_t *out_bot_y,
                              uint16_t *out_bot_w, uint16_t *out_bot_h)
{
    uint16_t gap = g_sys_config.gap_u * AREX_BASE_U;

    /* 计算上区总高度 = 各模块高度累加 */
    uint16_t top_h = 0;
    top_h += g_sys_config.h_depth * AREX_BASE_U;               /* DEPTH */
    top_h += g_sys_config.h_ndl * AREX_BASE_U + gap;            /* NDL/TTS 双拼 */
    top_h += g_sys_config.h_pod * AREX_BASE_U + gap;            /* POD 双拼 */
    top_h += g_sys_config.h_batt * AREX_BASE_U + gap;           /* BATT 双拼 */
    top_h += g_sys_config.h_gas * AREX_BASE_U + gap;            /* GAS */
    top_h += g_sys_config.h_time * AREX_BASE_U;                 /* DIVE TIME */

    /* 零高度保护：最小 AREX_MIN_CLASSIC_TOP_H px */
    if (top_h < AREX_MIN_CLASSIC_TOP_H) top_h = AREX_MIN_CLASSIC_TOP_H;

    uint16_t bottom_h = (g_sys_config.safe_zone_h > top_h + gap)
                         ? (g_sys_config.safe_zone_h - top_h - gap)
                         : AREX_MIN_CLASSIC_TOP_H;

    if (g_sys_config.layout_order == AREX_ORDER_NORMAL) {
        *out_top_x = 0;
        *out_bot_x = 0;
        *out_top_y = 0;
        *out_bot_y = (int16_t)(top_h + gap);
    } else {
        *out_top_x = 0;
        *out_bot_x = 0;
        *out_top_y = (int16_t)(bottom_h + gap);
        *out_bot_y = 0;
    }

    *out_top_w = g_sys_config.safe_zone_w;
    *out_top_h = top_h;
    *out_bot_w = g_sys_config.safe_zone_w;
    *out_bot_h = bottom_h;
}


/* =========================================================
 * 5x6 网格布局推算
 *
 * 计算每个 widget 单元格的绝对位置。
 * 收到 row(0~5), col(0~4), w_span(1~2), h_span(1~2)
 * 直接算出 X = col * unit_w, Y = row * unit_h
 * ========================================================= */
void arex_calc_widget_cell(uint16_t parent_w, uint16_t parent_h,
                          uint8_t row, uint8_t col,
                          uint8_t w_span, uint8_t h_span,
                          int16_t *out_x, int16_t *out_y,
                          uint16_t *out_w, uint16_t *out_h)
{
    uint16_t unit_w = parent_w / AREX_WIDGET_COLS;  /* e.g. 92px if parent=460 */
    uint16_t unit_h = parent_h / AREX_WIDGET_ROWS;   /* e.g. 80px if parent=480 */

    *out_x = (int16_t)(col * unit_w);
    *out_y = (int16_t)(row * unit_h);
    *out_w = w_span * unit_w;
    *out_h = h_span * unit_h;

    /* 边界修正，防止越界 */
    if (*out_x + *out_w > parent_w) *out_w = parent_w - *out_x;
    if (*out_y + *out_h > parent_h) *out_h = parent_h - *out_y;
}

/* =========================================================
 * 16 柱组织图 X 坐标推算
 *
 * 底部对齐，16 等分柱状图。
 * 每根柱宽 = total_w / 16，X = i * col_w
 * ========================================================= */
void arex_calc_tissue_bars(uint16_t total_w, uint16_t bar_max_h,
                           int16_t out_x[16], uint16_t out_w[16])
{
    uint16_t col_w = total_w / 16;
    for (uint8_t i = 0; i < 16; i++) {
        out_x[i] = (int16_t)(i * col_w);
        out_w[i] = col_w;
    }
    (void)bar_max_h; /* 柱高由调用方按百分比算出，此处只返回 X 和 W */
}

/* =========================================================
 * LVGL 样式辅助
 * ========================================================= */

/* 将 AREX_ALIGN_* 转换为 LVGL 对齐方式 */
lv_text_align_t arex_align_to_lv(uint8_t align)
{
    if (align == AREX_ALIGN_LEFT)   return LV_TEXT_ALIGN_LEFT;
    if (align == AREX_ALIGN_CENTER) return LV_TEXT_ALIGN_CENTER;
    return LV_TEXT_ALIGN_RIGHT;
}

/* 将对齐转换为 LVGL ALIGN 常量 */
lv_align_t arex_align_to_lv_align(uint8_t align)
{
    if (align == AREX_ALIGN_LEFT)   return LV_ALIGN_LEFT_MID;
    if (align == AREX_ALIGN_CENTER) return LV_ALIGN_CENTER;
    return LV_ALIGN_RIGHT_MID;
}

/* =========================================================
 * 字体映射器 (Font Mapper)
 *
 * 全系统唯一允许将字体 ID 转换为真实 lvgl 字体指针的地方。
 * 所有配置结构体中保存的 title_font / val_font 均应为 arex_font_id_t 值。
 *
 * ID 映射表：
 *   AREX_FONT_ID_SMALL  (0) → lv_font_courier_14  14px  标签/单位/Badge
 *   AREX_FONT_ID_TITLE  (1) → lv_font_courier_20  20px  菜单项/卡片标题
 *   AREX_FONT_ID_MEDIUM (2) → lv_font_courier_28  28px  数据值
 *   AREX_FONT_ID_HUGE   (3) → lv_font_courier_48  48px  深度大数字
 * ========================================================= */
const lv_font_t *arex_get_font(uint8_t font_id)
{
    switch (font_id) {
        case AREX_FONT_ID_SMALL:  return AREX_FONT_SMALL;   /* 14px */
        case AREX_FONT_ID_TITLE:  return AREX_FONT_TITLE;   /* 20px */
        case AREX_FONT_ID_MEDIUM: return AREX_FONT_MEDIUM;  /* 28px */
        case AREX_FONT_ID_HUGE:   return AREX_FONT_HUGE;   /* 48px */
        default:                   return AREX_FONT_SMALL;   /* 兜底：永不为 NULL */
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
 * 3. 调用 arex_ui_apply_config() 重排 UI
 *
 * JSON 字段示例 (与 HTML configIds 对应):
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
 * 初始化入口 (由 UI_main 调用)
 *
 * 启动流程：
 *   1. 从 KV 持久化读取配置 → 成功则直接使用
 *   2. KV 无数据/读取失败 → 填入默认值
 *   3. 传感器数据清零（由 sim_tick_cb / 外部 API 实时写入）
 * ========================================================= */
void arex_ui_init(void)
{
    /* 1. 加载持久化配置，失败则用默认值保底 */
    if (!arex_config_load(&g_sys_config)) {
        arex_sys_config_defaults(&g_sys_config);
    }

    strcpy(g_sensor_data.gas_name, "AIR");
    /* 2. 传感器数据清零 */
    memset(&g_sensor_data, 0, sizeof(g_sensor_data));
}

/* =========================================================
 * 应用配置变更 (配置界面修改后调用此函数)
 * 1. 检测 safe zone 边界
 * 2. 重建 left anchor 排版
 * 3. 重建 right card 布局
 * ========================================================= */
void arex_ui_apply_config(void)
{
    /* 安全区边界校验 */
    if (arex_safe_zone_in_danger()) {
        /* TODO: 触发危险警告 UI */
    }

    /* TODO: 触发 arex_screen_rebuild_layout() 重建排版
     * 这将在重构 arex_screen.c 时实现
     */
}

/* =========================================================
 * 卡片顺序查询 (统一入口 — 替代旧的 g_arex_card_order)
 * ========================================================= */
uint8_t g_sys_card_order(uint8_t pos)
{
    if (pos >= AREX_CARD_COUNT) return 0;
    return g_sys_config.card_order[pos];
}

/* =========================================================
 * 通用动态菜单工厂
 * 所有尺寸从 g_sys_config 推算，不含硬编码像素值。
 * ========================================================= */
void arex_render_dynamic_menu(lv_obj_t *parent_card,
                              const arex_menu_item_cfg_t *items,
                              uint8_t item_count,
                              int start_y,
                              lv_obj_t **out_item_handles)
{
    if (!parent_card || !items || item_count == 0) return;

    int right_canvas_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                       - ((int)g_sys_config.gap_u * AREX_BASE_U);
    int item_w = right_canvas_w - 15;  /* 右侧 15px 呼吸距 */

    int current_y = start_y;
    for (uint8_t i = 0; i < item_count; i++) {
        const arex_menu_item_cfg_t *item_cfg = &items[i];
        /* height_u 默认 0 → 查 h_menu_item (单位 U) */
        int item_h = (int)(item_cfg->height_u > 0 ? item_cfg->height_u : g_sys_config.h_menu_item)
                   * AREX_BASE_U;
        /* gap_y 从 gap_menu (单位 U) 推算 */
        int gap_y = (int)g_sys_config.gap_menu * AREX_BASE_U;

        lv_obj_t *item = lv_obj_create(parent_card);
        lv_obj_remove_style_all(item);
        lv_obj_set_pos(item, 0, current_y);
        lv_obj_set_size(item, item_w, item_h);
        lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, AREX_DARK, 0);
        lv_obj_set_style_border_width(item, item_cfg->border_width, LV_PART_MAIN);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        /* 标题 label */
        if (item_cfg->title_text) {
            lv_obj_t *title_lbl = lv_label_create(item);
            lv_label_set_text(title_lbl, item_cfg->title_text);
            lv_obj_set_style_text_font(title_lbl, arex_get_font(item_cfg->title_font_id), 0);
            lv_obj_set_style_text_color(title_lbl, AREX_GREEN, 0);
            lv_obj_set_size(title_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 12, 0);
            lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
        }

        /* 右侧徽章 label */
        if (item_cfg->value_badge) {
            lv_obj_t *badge_lbl = lv_label_create(item);
            lv_label_set_text(badge_lbl, item_cfg->value_badge);
            lv_obj_set_style_text_font(badge_lbl, arex_get_font(item_cfg->value_font_id), 0);
            lv_obj_set_style_text_color(badge_lbl, AREX_LIGHT, 0);
            lv_obj_set_size(badge_lbl, 80, 28);
            lv_obj_align(badge_lbl, LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_set_style_text_align(badge_lbl, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_long_mode(badge_lbl, LV_LABEL_LONG_DOT);
        }

        if (out_item_handles) {
            out_item_handles[i] = item;
        }

        current_y += item_h + gap_y;
    }
}

/* =========================================================
 * 通用卡片标题渲染器
 * 标题文字(Y=8)与分割线(Y=48)为视觉组合，绝对焊死在卡片顶部。
 * AREX_CARD_TITLE_H 仅作为下方"内容区(菜单/图表)的起始 Y 坐标偏移"。
 *
 * parent_card: 父容器（tile 对象）
 * title_text:  标题文字
 *
 * 标题布局（焊死，绝对不跟随 AREX_CARD_TITLE_H）：
 *   文字:   Y=8,  高度 40px，AREX_LIGHT 色
 *   分割线: Y=48, h=2px, AREX_DARK 色
 * ========================================================= */
void arex_render_card_title(lv_obj_t *parent_card, const char *title_text)
{
    uint16_t right_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                     - ((int)g_sys_config.gap_u * AREX_BASE_U);

    /* 1. 标题文字：扒光默认样式 + 强制小字号/次级颜色 */
    lv_obj_t *lbl = lv_label_create(parent_card);
    lv_obj_remove_style_all(lbl);
    lv_obj_set_pos(lbl, 16, 8);
    lv_obj_set_size(lbl, right_w - 32, 40);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl, title_text);
    lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);

    /* 2. 分割线：绝对固定在文字下方（焊死 Y=48） */
    lv_obj_t *line = lv_obj_create(parent_card);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, right_w - 32, 2);
    lv_obj_set_pos(line, 16, 48);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(line, AREX_DARK, 0);
}

/* =========================================================
 * 5F 自定义网格组件外部容器（由 arex_screen.c 注入）
 * ========================================================= */
lv_obj_t *g_left_anchor_obj = NULL;
lv_obj_t *g_card_custom_obj = NULL;

/* 告警状态 */
static arex_alarm_level_t s_alarm_level = AREX_ALARM_NONE;
static char s_alarm_text[64] = {0};
static lv_timer_t *s_alarm_blink_timer = NULL;
static bool s_alarm_blink_on = false;
static lv_obj_t *s_alarm_banner = NULL;

/* 告警横幅静态样式（避免每次创建重复初始化） */
static lv_style_t s_banner_style_warn;
static lv_style_t s_banner_style_crit;

/* =========================================================
 * 5F 网格坐标推算（纯数学绝对映射，无 lv_grid）
 *
 * 核心公式：
 *   cell_w = parent_w / 5
 *   cell_h = parent_h / 6
 *   abs_x  = col * cell_w + gap
 *   abs_y  = row * cell_h + gap
 *   abs_w  = span_w * cell_w - gap*2
 *   abs_h  = span_h * cell_h - gap*2
 * ========================================================= */
#define WIDGET_GAP  0   /* 网格缝隙 px */

/* =========================================================
 * 5F 网格坐标推算（锁定 5 列 + 标题避让，动态 cell_h 自适应）
 *
 * parent_w/parent_h: 父容器总尺寸（用于动态推算）
 * row/col: 网格行列索引(0~5 / 0~4)
 * span_w/span_h: 跨越的列数/行数
 * out_*: 输出绝对坐标
 *
 * 排版矩阵严格锁定 5 列：
 *   cell_w = parent_w / 5
 *   cell_h = (parent_h - AREX_CARD_TITLE_H) / 6
 * Y 坐标增加 AREX_CARD_TITLE_H=60px 偏移，确保第一行落在标题区下方。
 * 宽高减 4px (2px 缝隙 x2) 制造四周 2px 物理留白。
 * 如果标题高度改为其他值，cell_h 会自动重新计算，内容区完美自适应。
 * ========================================================= */
void arex_calc_widget_grid(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t span_w, uint8_t span_h,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h)
{
    /* 锁定 5 列基准，动态计算 cell_h */
    uint16_t cell_w = parent_w / 5;
    uint16_t cell_h = (parent_h > AREX_CARD_TITLE_H)
                      ? ((parent_h - AREX_CARD_TITLE_H) / AREX_WIDGET_ROWS)
                      : 60;  /* 保底 fallback */

    /* X: 列偏移 + 缝隙(2px) */
    *out_x = (int16_t)(col * cell_w + WIDGET_GAP);
    /* Y: 标题区下方 + 行偏移 + 缝隙(2px) */
    *out_y = (int16_t)(AREX_CARD_TITLE_H + row * cell_h + WIDGET_GAP);
    /* 宽高: 跨距×基准 - 4px 缝隙(四周各 2px) */
    *out_w = (uint16_t)(span_w * cell_w - WIDGET_GAP * 2);
    *out_h = (uint16_t)(span_h * cell_h - WIDGET_GAP * 2);

    /* 边界修正（以容器总尺寸为边界） */
    if (*out_x + *out_w > (int16_t)parent_w)
        *out_w = (uint16_t)((int16_t)parent_w - *out_x);
    if (*out_y + *out_h > (int16_t)parent_h)
        *out_h = (uint16_t)((int16_t)parent_h - *out_y);
}

/* 字号自适应引擎（已内联到 render_widget_by_id，保留函数体供未来扩展）

/* =========================================================
 * 组件元数据字典（按 arex_widget_id_t 索引）
 * 渲染引擎只查此表，不做任何"如果是 DEPTH" 的硬编码判断。
 * ========================================================= */
typedef struct {
    const char *title;     /* 显示标题（英文） */
    const char *unit;     /* 单位字符串 */
    arex_font_id_t title_font;
    arex_font_id_t val_font;
    arex_align_t   val_align;
} widget_meta_t;

static const widget_meta_t s_widget_meta[AREX_WIDGET_COUNT] = {
    /* EMPTY */     { NULL,          NULL,   AREX_FONT_ID_SMALL,  AREX_FONT_ID_SMALL,  AREX_ALIGN_CENTER },
    /* DEPTH  */    { "DEPTH",      "m",    AREX_FONT_ID_SMALL,  AREX_FONT_ID_HUGE,   AREX_ALIGN_CENTER },
    /* TEMP   */    { "TEMP",       "C",    AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* HEADING */   { "HEADING",    "",     AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* SAC    */    { "SAC",        "l/m",  AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* BATT   */    { "BATTERY",    "%",    AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* NDL    */    { "NDL",        "min",  AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* TTS    */    { "TTS",        "min",  AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* PPO2   */    { "PPO2",      "",     AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* CNS    */    { "CNS",        "%",    AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* POD1   */    { "POD1",       "bar",  AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* POD2   */    { "POD2",       "bar",  AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* WTIME  */    { "TIME",       "",     AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
    /* GAS    */    { "GAS",        "",     AREX_FONT_ID_SMALL,  AREX_FONT_ID_MEDIUM, AREX_ALIGN_CENTER },
};

/* =========================================================
 * 获取 widget 显示名称
 * ========================================================= */
const char *arex_get_widget_name(arex_widget_id_t id)
{
    if (id >= AREX_WIDGET_COUNT) return "???";
    return s_widget_meta[id].title ? s_widget_meta[id].title : "";
}

/* =========================================================
 * 创建单个自定义组件（组件工厂 — 左侧网格 + 5F 共用）
 *
 * 关键：每个组件的 lv_obj_set_user_data() 存储了 arex_widget_id_t，
 * 告警引擎靠这个烙印实现"左侧锚点 + 5F 组件同时闪烁"。
 *
 * 字号由 span 自动决定，大块→Huge，中块→Medium，小块→Small。
 * is_depth_icon == true 时，在 DEPTH 模块内挂载 sudu 速率图标。
 * ========================================================= */
lv_obj_t *render_widget_by_id(lv_obj_t *parent,
                               arex_widget_id_t w_id,
                               int16_t abs_x, int16_t abs_y,
                               uint16_t abs_w, uint16_t abs_h,
                               uint8_t span_w, uint8_t span_h,
                               bool is_depth_icon)
{
    if (w_id >= AREX_WIDGET_COUNT) return NULL;

    const widget_meta_t *meta = &s_widget_meta[w_id];

    /* 字号自适应引擎：
     *   2x2 大块 → AREX_FONT_ID_HUGE (48px)
     *   2x1 长条 → AREX_FONT_ID_MEDIUM (28px)
     *   1x1 小块 → AREX_FONT_ID_SMALL (14px) */
    arex_font_id_t val_font_id;
    if (span_w >= 2 && span_h >= 2) {
        val_font_id = AREX_FONT_ID_HUGE;
    } else if (span_w >= 2) {
        val_font_id = AREX_FONT_ID_MEDIUM;
    } else {
        val_font_id = AREX_FONT_ID_SMALL;
    }

    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, abs_x, abs_y);
    lv_obj_set_size(obj, abs_w, abs_h);
    lv_obj_set_style_bg_color(obj, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, AREX_DARK, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 2, 0);

    /* ========== 第一步：封杀所有滚动条 ========== */
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);

    /* ========== 靶向告警烙印 ========== */
    lv_obj_set_user_data(obj, (void *)(uintptr_t)w_id);

    /* ========== 第二步：DEPTH 专属渲染（整数+小数+单位分离） ========== */
    if (w_id == AREX_WIDGET_DEPTH) {
        /* child[0] 整数，Huge 字体，靠左 */
        lv_obj_t *int_lbl = lv_label_create(obj);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT) {
            lv_label_set_text(int_lbl, "--");
        } else {
            lv_label_set_text_fmt(int_lbl, "%d", (int)g_sensor_data.depth);
        }
        lv_obj_set_style_text_font(int_lbl, arex_get_font(AREX_FONT_ID_HUGE), 0);
        lv_obj_set_style_text_color(int_lbl, AREX_GREEN, 0);
        lv_obj_align(int_lbl, LV_ALIGN_LEFT_MID, 8, 0);

        /* child[1] 小数，Medium 字体，贴整数右上角 */
        lv_obj_t *dec_lbl = lv_label_create(obj);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT) {
            lv_label_set_text(dec_lbl, ".-");
        } else {
            lv_label_set_text_fmt(dec_lbl, ".%d", (int)((g_sensor_data.depth - (int)g_sensor_data.depth) * 10 + 0.5f));
        }
        lv_obj_set_style_text_font(dec_lbl, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
        lv_obj_set_style_text_color(dec_lbl, AREX_GREEN, 0);
        lv_obj_align_to(dec_lbl, int_lbl, LV_ALIGN_OUT_RIGHT_TOP, 2, 5);

        /* child[2] 单位m，Small 字体，贴小数正下方 */
        lv_obj_t *unit_lbl = lv_label_create(obj);
        lv_label_set_text(unit_lbl, "m");
        lv_obj_set_style_text_font(unit_lbl, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(unit_lbl, AREX_LIGHT, 0);
        lv_obj_align_to(unit_lbl, dec_lbl, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

        /* child[3] 速率箭头，贴右边缘 */
        LV_IMG_DECLARE(sudu);
        lv_obj_t *sudu_img = lv_img_create(obj);
        lv_img_set_src(sudu_img, &sudu);
        lv_obj_align(sudu_img, LV_ALIGN_RIGHT_MID, -5, 0);

        /* 容器自身设烙印，供 arex_widget_set_value 遍历匹配 */
        lv_obj_set_user_data(obj, (void *)(uintptr_t)w_id);
        return obj;
    }

    /* ========== 第三步：NDL 专属渲染（电池型 Bar + 数值 + 标签） ========== */
    if (w_id == AREX_WIDGET_NDL) {
        lv_obj_t *bar_bg = lv_obj_create(obj);
        lv_obj_remove_style_all(bar_bg);
        lv_obj_set_size(bar_bg, 14, 40);
        lv_obj_align(bar_bg, LV_ALIGN_LEFT_MID, 10, 0);
        lv_obj_set_style_border_width(bar_bg, 2, 0);
        lv_obj_set_style_border_color(bar_bg, AREX_GREEN, 0);
        lv_obj_set_style_radius(bar_bg, 4, 0);

        lv_obj_t *bar_fill = lv_obj_create(bar_bg);
        lv_obj_remove_style_all(bar_fill);
        lv_obj_set_size(bar_fill, LV_PCT(100), LV_PCT(60));
        lv_obj_align(bar_fill, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(bar_fill, AREX_GREEN, 0);
        lv_obj_set_style_bg_opa(bar_fill, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(bar_fill, 2, 0);

        lv_obj_t *val_lbl = lv_label_create(obj);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT) {
            lv_label_set_text(val_lbl, "--");
        } else {
            lv_label_set_text_fmt(val_lbl, "%d", g_sensor_data.ndl);
        }
        lv_obj_set_style_text_font(val_lbl, arex_get_font(AREX_FONT_ID_HUGE), 0);
        lv_obj_set_style_text_color(val_lbl, AREX_GREEN, 0);
        lv_obj_align_to(val_lbl, bar_bg, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
        lv_obj_set_user_data(val_lbl, (void *)(uintptr_t)AREX_WIDGET_NDL);

        lv_obj_t *title_lbl = lv_label_create(obj);
        lv_label_set_text(title_lbl, "NDL");
        lv_obj_set_style_text_font(title_lbl, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
        lv_obj_set_style_text_color(title_lbl, AREX_GREEN, 0);
        lv_obj_align_to(title_lbl, val_lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, 5, -8);

        /* 容器自身设烙印，供 arex_widget_set_value 遍历匹配 */
        lv_obj_set_user_data(obj, (void *)(uintptr_t)w_id);
        return obj;
    }

    /* ========== 通用渲染（标题 + 数值 + 单位）========== */

    /* 标题 label */
    if (meta->title) {
        lv_obj_t *title_lbl = lv_label_create(obj);
        lv_label_set_text(title_lbl, meta->title);
        lv_obj_set_style_text_font(title_lbl, arex_get_font(meta->title_font), 0);
        lv_obj_set_style_text_color(title_lbl, AREX_GREEN, 0);
        lv_obj_set_size(title_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 2);
        lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
    }

    /* 数值 label（存储句柄供 update 循环更新文字）*/
    lv_obj_t *val_lbl = lv_label_create(obj);
    if (AREX_SHOW_PLACEHOLDER_ON_INIT) {
        lv_label_set_text(val_lbl, "--");
    } else {
        /* 取对应字段的初始值，与 arex_widget_set_value() 格式保持一致 */
        char buf[32];        if (w_id == AREX_WIDGET_TTS || w_id == AREX_WIDGET_NDL)
            snprintf(buf, sizeof(buf), "%d", g_sensor_data.ndl);
        else if (w_id == AREX_WIDGET_HEADING)
            snprintf(buf, sizeof(buf), "%d", g_sensor_data.heading);
        else if (w_id == AREX_WIDGET_CNS)
            snprintf(buf, sizeof(buf), "%d", g_sensor_data.cns_pct);
        else if (w_id == AREX_WIDGET_PPO2)
            snprintf(buf, sizeof(buf), "%.2f", (double)g_sensor_data.ppo2[g_sensor_data.gas_active_idx]);
        else if (w_id == AREX_WIDGET_POD1)
            snprintf(buf, sizeof(buf), "%.0f", (double)g_sensor_data.pod1_bar);
        else if (w_id == AREX_WIDGET_POD2)
            snprintf(buf, sizeof(buf), "%.0f", (double)g_sensor_data.pod2_bar);
        else if (w_id == AREX_WIDGET_BATTERY)
            snprintf(buf, sizeof(buf), "%.0f%%", (double)g_sensor_data.battery_pct);
        else if (w_id == AREX_WIDGET_WTIME)
            snprintf(buf, sizeof(buf), "%02d:%02d", g_sensor_data.dive_time_s / 60, g_sensor_data.dive_time_s % 60);
        else if (w_id == AREX_WIDGET_TEMP)
            snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.temperature_c);
        else if (w_id == AREX_WIDGET_GAS)
            snprintf(buf, sizeof(buf), "%s", g_sensor_data.gas_name);
        else
            snprintf(buf, sizeof(buf), "%.0f", (double)g_sensor_data.depth);
        lv_label_set_text(val_lbl, buf);
    }
    lv_obj_set_style_text_font(val_lbl, arex_get_font(val_font_id), 0);
    lv_obj_set_style_text_color(val_lbl, AREX_GREEN, 0);
    lv_obj_set_size(val_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(val_lbl, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(val_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_user_data(val_lbl, (void *)(uintptr_t)w_id);

    /* 单位 label */
    if (meta->unit && meta->unit[0]) {
        lv_obj_t *unit_lbl = lv_label_create(obj);
        lv_label_set_text(unit_lbl, meta->unit);
        lv_obj_set_style_text_font(unit_lbl, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(unit_lbl, AREX_LIGHT, 0);
        lv_obj_set_size(unit_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(unit_lbl, LV_ALIGN_BOTTOM_MID, 0, -2);
        lv_label_set_long_mode(unit_lbl, LV_LABEL_LONG_DOT);
    }

    (void)meta;
    (void)is_depth_icon;
    return obj;
}

/* =========================================================
 * 全局 widget 句柄表（按 arex_widget_id_t 索引，供 update 循环查找）
 * 注意：一个 widget_id 可能有多个物理实例（左侧锚点1个 + 5F N个），
 * 所以这是链表表头，实际使用时遍历子节点查找。
 * ========================================================= */
#define MAX_WIDGET_HANDLES 16
static lv_obj_t *s_widget_handles[AREX_WIDGET_COUNT]; /* 仅记录 5F 区域的句柄 */
static uint8_t   s_widget_handle_count = 0;

/* 按 widget_id 在容器中查找第一个匹配的子节点 */
static lv_obj_t *find_widget_in_container(lv_obj_t *container, arex_widget_id_t w_id)
{
    if (!container) return NULL;
    int16_t child_cnt = lv_obj_get_child_cnt(container);
    for (int16_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(container, i);
        if (child && (arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(child) == w_id) {
            return child;
        }
    }
    return NULL;
}

/* =========================================================
 * 5F 网格总线渲染器
 *
 * 1. 从 g_sys_config.widget_* 读取所有组件配置
 * 2. 逐一枚遍历，用纯数学行×列映射算出绝对坐标
 * 3. 调用组件工厂渲染，注入 user_data 烙印
 * 4. 注册外部容器到告警引擎
 * ========================================================= */
void arex_render_5f_custom_grid(lv_obj_t *card_custom, lv_obj_t *left_anchor)
{
    /* 注入外部容器 */
    g_card_custom_obj = card_custom;
    g_left_anchor_obj = left_anchor;

    if (!card_custom) return;

    /* 获取容器总尺寸（标题区偏移由 arex_calc_widget_grid 内部处理） */
    uint16_t parent_w = lv_obj_get_content_width(card_custom);
    uint16_t parent_h = lv_obj_get_content_height(card_custom);

    /* 清除旧 widget 句柄表 */
    memset(s_widget_handles, 0, sizeof(s_widget_handles));
    s_widget_handle_count = 0;

    /* 清除容器中所有旧组件（rebuild 时） */
    lv_obj_clean(card_custom);

    /* ---- 创建卡片标题（使用通用引擎函数） ---- */
    arex_render_card_title(card_custom, "5F: CUSTOM WIDGETS");

    /* 遍历所有组件 */
    uint8_t count = g_sys_config.widget_count;
    if (count > AREX_MAX_WIDGETS) count = AREX_MAX_WIDGETS;

    for (uint8_t i = 0; i < count; i++) {
        arex_widget_id_t w_id   = (arex_widget_id_t)g_sys_config.widget_ids[i];
        uint8_t r       = g_sys_config.widget_r[i];
        uint8_t c       = g_sys_config.widget_c[i];
        uint8_t span_w  = g_sys_config.widget_w[i];
        uint8_t span_h  = g_sys_config.widget_h[i];

        if (w_id == AREX_WIDGET_EMPTY) continue;
        if (r >= AREX_WIDGET_ROWS || c >= AREX_WIDGET_COLS) continue;
        if (span_w == 0) span_w = 1;
        if (span_h == 0) span_h = 1;

        /* 纯数学绝对坐标映射（含 AREX_CARD_TITLE_H=40px 标题避让偏移） */
        int16_t abs_x, abs_y;
        uint16_t abs_w, abs_h;
        arex_calc_widget_grid(parent_w, parent_h,
                              r, c, span_w, span_h,
                              &abs_x, &abs_y, &abs_w, &abs_h);

        /* 调用组件工厂 */
        lv_obj_t *w = render_widget_by_id(card_custom, w_id,
                                          abs_x, abs_y, abs_w, abs_h,
                                          span_w, span_h, false);

        /* 记录句柄（用于 update 循环） */
        if (w && s_widget_handle_count < MAX_WIDGET_HANDLES) {
            s_widget_handles[s_widget_handle_count++] = w;
        }
    }
}

/* =========================================================
 * 按 widget_id 设置数值（由外部 update 循环调用）
 *
 * 算法：在 g_card_custom_obj 和 g_left_anchor_obj 两个容器中
 * 遍历所有子节点，用 user_data 烙印匹配 target_id，
 * 找到后定位其中的数值 label 并更新文字。
 *
 * 绝不触发任何重绘或排版重构！只更新 lv_label 文字。
 * ========================================================= */
void arex_widget_set_value(arex_widget_id_t id, float value)
{
    /* 遍历两个容器（5F 卡片 + 左侧锚点） */
    lv_obj_t *containers[2] = { g_card_custom_obj, g_left_anchor_obj };

    for (uint8_t c = 0; c < 2; c++) {
        lv_obj_t *container = containers[c];
        if (!container) continue;

        int16_t child_cnt = lv_obj_get_child_cnt(container);
        for (int16_t i = 0; i < child_cnt; i++) {
            lv_obj_t *child = lv_obj_get_child(container, i);
            if (!child) continue;

            /* user_data 烙印匹配：找到 widget 容器 */
            if ((arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(child) == id) {
                /* DEPTH 专属：整数/小数用 child[0]/child[1] 下标访问，不走通用子 label 路由 */
                if (id == AREX_WIDGET_DEPTH) {
                    int di = (int)value;
                    int dd = (int)((value - di) * 10 + 0.5f);
                    lv_obj_t *part0 = lv_obj_get_child(child, 0);
                    lv_obj_t *part1 = lv_obj_get_child(child, 1);
                    if (part0 && lv_obj_check_type(part0, &lv_label_class)) {
                        lv_label_set_text_fmt(part0, "%d", di);
                    }
                    if (part1 && lv_obj_check_type(part1, &lv_label_class)) {
                        lv_label_set_text_fmt(part1, ".%d", dd);
                    }
                    break;
                }

                /* 通用 widget：在子节点中找 user_data == id 的数值 label */
                int16_t sub_cnt = lv_obj_get_child_cnt(child);
                for (int16_t j = 0; j < sub_cnt; j++) {
                    lv_obj_t *sub = lv_obj_get_child(child, j);
                    if (!sub) continue;
                    if ((arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(sub) == id) {
                        if (lv_obj_check_type(sub, &lv_label_class)) {
                            char buf[32];
                            if (id == AREX_WIDGET_TEMP) {
                                snprintf(buf, sizeof(buf), "%.1f", (double)value);
                                lv_label_set_text(sub, buf);
                            } else if (id == AREX_WIDGET_PPO2) {
                                snprintf(buf, sizeof(buf), "%.2f", (double)value);
                            } else if (id == AREX_WIDGET_POD1 || id == AREX_WIDGET_POD2) {
                                snprintf(buf, sizeof(buf), "%.0f", (double)value);
                            } else if (id == AREX_WIDGET_WTIME) {
                                snprintf(buf, sizeof(buf), "%02d:%02d",
                                         ((uint32_t)value) / 60,
                                         ((uint32_t)value) % 60);
                            } else if (id == AREX_WIDGET_BATTERY) {
                                snprintf(buf, sizeof(buf), "%.0f%%", (double)value);
                            } else if (id == AREX_WIDGET_TTS || id == AREX_WIDGET_NDL) {
                                snprintf(buf, sizeof(buf), "%d", (int)value);
                            } else {
                                snprintf(buf, sizeof(buf), "%.0f", (double)value);
                            }
                            lv_label_set_text(sub, buf);
                        }
                        break;
                    }
                }
            }
        }
    }
}

/* =========================================================
 * 按 widget_id 设置字符串（用于 GAS 等非数值组件）
 * ========================================================= */
void arex_widget_set_text(arex_widget_id_t id, const char *text)
{
    if (!text) return;

    /* 遍历两个容器（5F 卡片 + 左侧锚点） */
    lv_obj_t *containers[2] = { g_card_custom_obj, g_left_anchor_obj };

    for (uint8_t c = 0; c < 2; c++) {
        lv_obj_t *container = containers[c];
        if (!container) continue;

        int16_t child_cnt = lv_obj_get_child_cnt(container);
        for (int16_t i = 0; i < child_cnt; i++) {
            lv_obj_t *child = lv_obj_get_child(container, i);
            if (!child) continue;

            if ((arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(child) == id) {
                int16_t sub_cnt = lv_obj_get_child_cnt(child);
                for (int16_t j = 0; j < sub_cnt; j++) {
                    lv_obj_t *sub = lv_obj_get_child(child, j);
                    if (!sub) continue;
                    if ((arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(sub) == id) {
                        if (lv_obj_check_type(sub, &lv_label_class)) {
                            lv_label_set_text(sub, text);
                        }
                        break;
                    }
                }
            }
        }
    }
}

/* =========================================================
 * 告警闪烁定时器回调
 * ========================================================= */
static void alarm_blink_cb(lv_timer_t *timer)
{
    (void)timer;
    s_alarm_blink_on = !s_alarm_blink_on;

    /* 遍历所有注册的 widget 句柄应用/移除闪烁样式 */
    for (uint8_t i = 0; i < s_widget_handle_count; i++) {
        lv_obj_t *w = s_widget_handles[i];
        if (!w) continue;

        if (s_alarm_blink_on) {
            lv_obj_set_style_bg_color(w, AREX_LIGHT, 0);
            lv_obj_set_style_text_color(w, AREX_BLACK, 0);
        } else {
            lv_obj_set_style_bg_color(w, AREX_BLACK, 0);
            lv_obj_set_style_text_color(w, AREX_GREEN, 0);
        }
    }
}

/* =========================================================
 * 靶向告警触发引擎
 *
 * 1. 弹出顶部的纯英文告警横幅（永不显示图案）
 * 2. 若 target_id != EMPTY，遍历所有子节点，
 *    找到打了烙印的组件并加入闪烁队列
 * 3. 启动/停止 blink 定时器
 *
 * 告警消失时调用 arex_clear_all_alarm_styles() 清除。
 * ========================================================= */
void arex_trigger_alarm(arex_alarm_level_t level,
                        const char *eng_text,
                        arex_widget_id_t target_id)
{
    /* 停止旧的 blink 定时器 */
    if (s_alarm_blink_timer) {
        lv_timer_pause(s_alarm_blink_timer);
        s_alarm_blink_on = false;
    }

    /* 清除旧告警样式 */
    arex_clear_all_alarm_styles();

    /* 弹出告警横幅 */
    arex_show_alarm_banner(level, eng_text);

    if (target_id == AREX_WIDGET_EMPTY) {
        /* 仅横幅告警，不做靶向 */
        return;
    }

    /* 初始化告警样式到所有匹配的 widget（注册到闪烁队列） */
    for (uint8_t i = 0; i < s_widget_handle_count; i++) {
        lv_obj_t *w = s_widget_handles[i];
        if (!w) continue;

        if ((arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(w) == target_id) {
            /* 加入闪烁队列（已在 s_widget_handles 中） */
            /* 立即应用一次 */
            lv_obj_set_style_bg_color(w, AREX_LIGHT, 0);
        }
    }

    /* 启动/重设 blink 定时器 */
    if (level == AREX_ALARM_CRIT) {
        /* CRITICAL: 2Hz 快速闪烁 */
        if (!s_alarm_blink_timer) {
            s_alarm_blink_timer = lv_timer_create(alarm_blink_cb, 500, NULL);
        } else {
            lv_timer_reset(s_alarm_blink_timer);
            lv_timer_set_period(s_alarm_blink_timer, 500);
        }
        lv_timer_resume(s_alarm_blink_timer);
    } else if (level == AREX_ALARM_WARN) {
        /* WARN: 1Hz 慢闪烁 */
        if (!s_alarm_blink_timer) {
            s_alarm_blink_timer = lv_timer_create(alarm_blink_cb, 1000, NULL);
        } else {
            lv_timer_reset(s_alarm_blink_timer);
            lv_timer_set_period(s_alarm_blink_timer, 1000);
        }
        lv_timer_resume(s_alarm_blink_timer);
    }
    /* INFO 级别不闪烁，仅横幅 */

    s_alarm_level = level;
}

/* =========================================================
 * 清除所有组件的告警样式，恢复正常显示
 * ========================================================= */
void arex_clear_all_alarm_styles(void)
{
    /* 停止 blink 定时器 */
    if (s_alarm_blink_timer) {
        lv_timer_pause(s_alarm_blink_timer);
    }

    /* 恢复所有 widget 句柄的正常样式 */
    for (uint8_t i = 0; i < s_widget_handle_count; i++) {
        lv_obj_t *w = s_widget_handles[i];
        if (!w) continue;
        lv_obj_set_style_bg_color(w, AREX_BLACK, 0);
        lv_obj_set_style_text_color(w, AREX_GREEN, 0);
    }

    /* 隐藏横幅 */
    arex_hide_alarm_banner();

    s_alarm_level = AREX_ALARM_NONE;
}

/* =========================================================
 * 告警横幅显示/隐藏
 * 默认横幅显示在屏幕顶部（独立图层），永不显示图案，纯英文文字。
 * ========================================================= */
void arex_show_alarm_banner(arex_alarm_level_t level, const char *eng_text)
{
    arex_hide_alarm_banner();

    if (!eng_text || !eng_text[0]) return;

    /* 创建横幅（顶层 screen，避免被其他对象遮挡） */
    lv_obj_t *screen = lv_scr_act();
    s_alarm_banner = lv_obj_create(screen);
    lv_obj_set_size(s_alarm_banner, AREX_PHYSICAL_W, 36);
    lv_obj_set_pos(s_alarm_banner, 0, 0);
    lv_obj_set_style_border_width(s_alarm_banner, 0, 0);
    lv_obj_set_style_radius(s_alarm_banner, 0, 0);
    lv_obj_set_style_pad_all(s_alarm_banner, 0, 0);

    if (level == AREX_ALARM_CRIT) {
        lv_obj_set_style_bg_color(s_alarm_banner, AREX_DARK, 0);
    } else if (level == AREX_ALARM_WARN) {
        lv_obj_set_style_bg_color(s_alarm_banner, AREX_DARK, 0);
    } else {
        lv_obj_set_style_bg_color(s_alarm_banner, AREX_BLACK, 0);
    }
    lv_obj_set_style_bg_opa(s_alarm_banner, LV_OPA_COVER, 0);

    /* 告警文字 label */
    lv_obj_t *lbl = lv_label_create(s_alarm_banner);
    lv_label_set_text(lbl, eng_text);
    lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);
    lv_obj_set_size(lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
}

void arex_hide_alarm_banner(void)
{
    if (s_alarm_banner) {
        lv_obj_del(s_alarm_banner);
        s_alarm_banner = NULL;
    }
}

/* =========================================================
 * 定时数据更新 (由 lv_timer 以 1Hz/2Hz 调用)
 * 仅更新 lv_label 文字，绝不触发排版重构
 * ========================================================= */
void arex_ui_update_data(void)
{
    /* 由调用方在 arex_screen.c 中实现具体的 lv_label_set_text 调用
     * 此函数作为空钩子存在，供未来扩展
     */
}

/* =========================================================
 * 11. Data Bus UI 消费任务 — 全系统唯一允许执行 lv_label_set_text 的地方
 *
 * 架构铁律：
 *   - 硬件工程师：只能调用 arex_bus_set_*() 系列函数（仅写数据+打脏标记）
 *   - UI 工程师  ：只能修改 arex_ui_update_task() 消费者
 *   - 两者通过 g_sensor_data.dirty_mask 完全解耦
 *
 * 由 lv_timer 驱动，建议 50ms 周期（20 FPS 足够覆盖所有传感器变化）
 * ========================================================= */
void arex_ui_update_task(lv_timer_t *timer)
{
    (void)timer;

    uint32_t mask = g_sensor_data.dirty_mask;
    if (mask == DIRTY_NONE) return;

    /* 最高优先级：UI 布局重建（BLE 配置同步触发）。
     * 重建耗时较长，锁住 LVGL invalidation 防止闪烁，本帧直接退出。 */
    if (mask & DIRTY_UI_LAYOUT) {
        lv_disp_t *disp = lv_disp_get_default();
        if (disp) lv_disp_enable_invalidation(disp, false);
        arex_screen_rebuild_layout();
        if (disp) lv_disp_enable_invalidation(disp, true);
        g_sensor_data.dirty_mask &= ~DIRTY_UI_LAYOUT;
        return;
    }

    /* 深度 + NDL + TTS + 组织舱 —— 左侧面板全量刷新 + 2F Deco 卡片刷新 */
    if (mask & (DIRTY_DEPTH | DIRTY_NDL | DIRTY_TTS | DIRTY_TISSUES)) {
        arex_screen_refresh_left_panel();
        card_deco_update();
    }

    /* 气瓶压力 —— 左侧面板 POD 刷新 */
    if (mask & DIRTY_POD) {
        arex_screen_refresh_left_panel();
    }

    /* 电池 —— 左侧面板刷新 + SystemData 专属物理防区 */
    if (mask & DIRTY_BATT) {
        arex_screen_refresh_left_panel();
        arex_screen_refresh_system_data();
    }

    /* 罗盘航向 */
    if (mask & DIRTY_HEADING) {
        arex_screen_refresh_compass_target();
    }

    /* 潜水时间 + W.TIME —— 左侧面板 + 4F 曲线图 */
    if (mask & DIRTY_TIME) {
        arex_screen_refresh_left_panel();
    }

    /* PO2 值 —— 左侧面板 */
    if (mask & DIRTY_PPO2) {
        arex_screen_refresh_left_panel();
    }

    /* 气体切换 */
    if (mask & DIRTY_GAS) {
        arex_screen_refresh_gas_menu();
        arex_screen_refresh_left_panel();
    }

    /* 4F 曲线图 + 减压站序列刷新（轨迹追加 + 减压站重绘，节流保护） */
    if (mask & DIRTY_DECO) {
        uint32_t now = lv_tick_get();
#if AREX_DECO_REFRESH_MS > 0 //相当于刷新的间隔（这个决定了刷新的点，但是和真正的采样频率有区别（一般要对应上））
        if (now - _deco_last_refresh_ms >= AREX_DECO_REFRESH_MS) {
            _deco_last_refresh_ms = now;
            card_plan_update();
        }
#else
        (void)_deco_last_refresh_ms;
        card_plan_update();
#endif
    }

    /* CNS 氧中毒 —— 2F Deco 卡片 */
    if (mask & DIRTY_CNS) {
        card_deco_update();
    }

    /* OTU 氧中毒 —— 2F Deco 卡片 */
    if (mask & DIRTY_OTU) {
        card_deco_update();
    }

    /* 温度刷新 — SystemData 专属物理防区 */
    if (mask & DIRTY_TEMP) {
        arex_screen_refresh_system_data();
    }

    /* 外设状态刷新（灯、气瓶数量） — SystemData 专属物理防区 */
    if (mask & DIRTY_DEVICES) {
        arex_screen_refresh_system_data();
    }

    /* 洗净所有脏标记 */
    arex_bus_clear_all_dirty();
}

/* =========================================================
 * 12. 左侧 2x6 绝对网格渲染引擎
 *
 * 严格将 160x360 区域划分为 2列(80px) x 6行(60px) 的绝对网格矩阵，
 * 彻底废弃 current_y 累加排版，改用 x*y*w*h 纯数学坐标推演。
 * SystemData 底部 60px 由 arex_render_system_data() 独立渲染。
 * ========================================================= */

/* 左侧网格总线渲染器：遍历 g_left_widgets[] 数组，
 * 用纯数学 cell_w * cell_h 推算绝对坐标并渲染所有组件。
 * left_anchor 传入用于告警引擎跨区搜索烙印对象。 */
void arex_render_left_anchor_grid(lv_obj_t *left_anchor)
{
    if (!left_anchor) return;

    /* 注入外部容器（供告警引擎跨区搜索烙印对象） */
    g_left_anchor_obj = left_anchor;

    /* 基准网格单元：2列 x 6行，每格 80x60 */
    const uint16_t cell_w = AREX_LEFT_CELL_W;   /* 80px */
    const uint16_t cell_h = AREX_LEFT_CELL_H;   /* 60px */

    /* 遍历并渲染基于网格的组件 */
    for (uint8_t i = 0; i < g_left_widget_count && i < AREX_LEFT_MAX_WIDGETS; i++) {
        arex_custom_widget_cfg_t *cfg = &g_left_widgets[i];
        if (cfg->widget_id == AREX_WIDGET_EMPTY) continue;

        /* 绝对物理坐标推演：col * cell_w, row * cell_h */
        int16_t  abs_x = (int16_t)(cfg->x * cell_w);
        int16_t  abs_y = (int16_t)(cfg->y * cell_h);
        uint16_t abs_w = cfg->w * cell_w;
        uint16_t abs_h = cfg->h * cell_h;

        /* DEPTH 模块挂载 sudu 速率图标 */
        bool is_depth = (cfg->widget_id == AREX_WIDGET_DEPTH);

        /* 调用底层工厂，左侧视觉紧凑，不扣除间隙 */
        render_widget_by_id(left_anchor, cfg->widget_id,
                            abs_x, abs_y, abs_w, abs_h,
                            cfg->w, cfg->h, is_depth);
    }
}
