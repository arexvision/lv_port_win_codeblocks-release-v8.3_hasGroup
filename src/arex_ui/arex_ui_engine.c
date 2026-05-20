#include "arex_ui_engine.h"
#include "arex_card_registry.h"
#include "arex_screen.h"
#include "arex_ui_state.h"
#include "fonts/arex_fonts.h"
#include "arex_data.h"
#include "arex_alarm.h"
#include "arex_alarm_view.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

extern void rt_kprintf(const char *fmt, ...);

/* ============================================================
 * 速率指示器图片资源（6级动态箭头）
 * ============================================================ */
LV_IMG_DECLARE(sudo_up_level0);
LV_IMG_DECLARE(sudo_up_level1);
LV_IMG_DECLARE(sudo_up_level2);
LV_IMG_DECLARE(sudo_down_level0);
LV_IMG_DECLARE(sudo_down_level1);
LV_IMG_DECLARE(sudo_down_level2);

/* ============================================================
 * 速率图标指针阵列（支持多DEPTH 模块同时存在
 * 最多支持屏幕上出现 MAX_ASCENT_ICONS 个深度模
 * (左侧锚点 1 + 5F 自定义网格多
 * ============================================================ */

/* ============================================================
 * NDL_STOP 多形态组件句柄（160x60 极限空间内的"变形金刚"
 * 支持屏幕上多NDL 模块（左侧锚1 + 5F 多个
 * 三种状 NDL常/ Safety停留 / Deco停留
 * ============================================================ */
lv_obj_t *s_img_ascent_rate[MAX_ASCENT_ICONS];
uint8_t  s_ascent_icon_count = 0;
ndl_handle_t s_ndl_handles[MAX_NDL_ICONS];
uint8_t      s_ndl_handle_count = 0;

/* ============================================================
 * 罗盘卡片静态句柄（card_compass.c 持有
 * 用于 arex_ui_update_task 中的零内存引擎刷
 * ============================================================ */
extern lv_obj_t *s_compass_tape_obj;
extern lv_obj_t *s_heading_val_lbl;
extern lv_obj_t *s_heading_hint_lbl;

/* 减压跟踪节流时间戳（arex_ui_update_task 使用*/
static uint32_t _deco_last_refresh_ms = 0;

/* 气体名称(供全局引用) */
const char *AREX_GAS_NAMES[AREX_GAS_COUNT] =
{
    "AIR",
    "NX 32",
    "TX 18/45",
    "O2 100%"
};

/* 气体 MOD 表 (单位: 米) */
const uint8_t AREX_GAS_MOD_M[AREX_GAS_COUNT] =
{
    56,  /* AIR */
    34,  /* NX 32 */
    68,  /* TX 18/45 */
    6    /* O2 100% */
};

static uint8_t arex_ui_clamp_battery_pct(float pct)
{
    if (pct <= 0.0f)
    {
        return 0U;
    }
    if (pct >= 100.0f)
    {
        return 100U;
    }
    return (uint8_t)pct;
}

/* =========================================================
 * 全局单例定义
 * ========================================================= */
arex_sys_config_t  g_sys_config;
arex_sensor_data_t g_sensor_data;  //注意这个是全局变量，所有UI层都要用它。因为赋值是原子操作，可以放心大胆用（不需要加锁）

/*当你写下 g_sensor_data.depth = 15.5f; 时，编译器会在底层把它翻译成
找到 g_sensor_data 的基地址 (0x20000000)
加上 depth 的偏移量 (+0)
直接生成一条单步汇编指令（STR），15.5 的二进制数据，像狙击枪一样，精准地打0x20000000 开始的 4 个字节里
它根本不会碰 heading，也不会battery这就是一次纯粹的、针对单32 位地址的“单指令写入”。因此，它是绝对原子的
*/

/* =========================================================
 * POD 单模具轮转分配状态机
 *
 * 架构：WIDGET_POD_0806 (33) 是全局唯一真实存在的气瓶模具
 * APP 下发同一POD_0806 可以出现多次（如左侧锚点POD1+POD2，或 5F 中的多个）
 * MCU 通过渲染计数s_pod_render_count 自动分配身份
 *
 * 渲染时拦WIDGET_POD_0806，根据计数器判断
 *   - 次遇(count=1, 奇数) 分配POD1
 *   - 次遇(count=2, 偶数) 分配POD2
 *
 * user_data 烙印使用高位掩码区分
 *   - POD1: 1000 + WIDGET_POD_0806 = 1033
 *   - POD2: 2000 + WIDGET_POD_0806 = 2033
 * ========================================================= */
static uint8_t s_pod_render_count = 0;  /* POD 渲染计数*/

#define POD_TAG_BASE  1000  /* POD 标签基准偏移 */
#define POD1_TAG      (POD_TAG_BASE + WIDGET_POD_0806)  /* 1033 */
#define POD2_TAG      (2 * POD_TAG_BASE + WIDGET_POD_0806)  /* 2033 */

/* =========================================================
 * SYS 模块全局静态指针（O(1) 直接访问，零遍历
 * ========================================================= */
static lv_obj_t *s_sys_batt_lbl = NULL;      /* 电量百分*/
static lv_obj_t *s_sys_temp_lbl = NULL;      /* 温度 */
static lv_obj_t *s_sys_strobe_img = NULL;    /* 留转灯图*/
static lv_obj_t *s_sys_flash_img = NULL;     /* 手电筒图*/
static lv_obj_t *s_sys_cyl_lbl = NULL;      /* 气瓶数量文本 "x0" */

/* =========================================================
 * 获取 POD 标签（根据当前渲染计数器返回值）
 * 返回 POD1_TAG POD2_TAG，用于烙印到 user_data
 *
 * 注意：s_pod_render_count 已在 render_widget_by_id 中先递增
 * 所count=1 时为个POD，count=2 时为个POD
 * ========================================================= */
static uintptr_t arex_get_pod_tag(void)
{
    /* 次调count=1，奇 POD1_TAG
     * 次调count=2，偶 POD2_TAG */
    return (s_pod_render_count % 2 == 1) ? POD1_TAG : POD2_TAG;
}

/* =========================================================
 * 获取 POD 编号（返1 2
 * ========================================================= */
static uint8_t arex_get_pod_index(void)
{
    /* 次调count=1，奇 POD1
     * 次调count=2，偶 POD2 */
    return (s_pod_render_count % 2 == 1) ? 1 : 2;
}

static void arex_add_left_anchor_sep_line(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w)
{
    lv_obj_t *line;

    if (!parent) return;

    line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, w, 1);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_style_bg_color(line, AREX_GREEN, 0);
    lv_obj_set_style_bg_opa(line, 140, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
}

static arex_grid_widget_t *arex_left_find_widget_at_cell(uint8_t col, uint8_t row)
{
    for (uint8_t i = 0; i < g_sys_config.left_widget_count && i < AREX_LEFT_MAX_WIDGETS; i++)
    {
        arex_grid_widget_t *cfg = &g_sys_config.left_widgets[i];
        if (cfg->widget_id == WIDGET_EMPTY)
        {
            continue;
        }

        const arex_widget_style_t *style = arex_get_widget_style(cfg->widget_id);
        uint8_t span_w = (style != NULL) ? style->span_w : 1;
        uint8_t span_h = (style != NULL) ? style->span_h : 1;
        if (col >= cfg->x && col < (uint8_t)(cfg->x + span_w) &&
                row >= cfg->y && row < (uint8_t)(cfg->y + span_h))
        {
            return cfg;
        }
    }

    return NULL;
}

/* =========================================================
 * 渲染计数器归零（每次网格重建/重绘前必须调用）
 * arex_screen_rebuild_layout() left_anchor_create() 调用
 * ========================================================= */
void arex_reset_widget_render_state(void)
{
    s_pod_render_count = 0;

    /* 归零底部 SystemData 静态句柄，防止 lv_timer 访问死内*/
    s_sys_batt_lbl     = NULL;
    s_sys_temp_lbl     = NULL;
    s_sys_strobe_img   = NULL;
    s_sys_flash_img    = NULL;
    s_sys_cyl_lbl      = NULL;
}

/* 左侧 2x7 绝对网格配置数组
 *
 * 160x420 区域 = 2 列(80px) x 7 行(60px)
 * 5F 卡片共用 arex_widget_id_t 枚举体系
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
void arex_sys_config_defaults(arex_sys_config_t *cfg)
{
    memset(cfg, 0, sizeof(arex_sys_config_t));

    /* ========== [A] 安全========== */
    cfg->safe_zone_w  = 580;
    cfg->safe_zone_h  = 420;
    cfg->offset_x     = 0;            /* x=0 表示水平居中（左右各留白 3U*/
    cfg->offset_y     = -10;          /* y=-10 向上偏移（上面留2U，下面留4U*/

    /* ========== [A] 鏋舵瀯 ========== */
    cfg->layout_order  = AREX_ORDER_NORMAL;  /* 0=标准(左锚右卡)=翻转(右锚左卡) */
    cfg->dots_position = AREX_DOTS_LEFT;    /* tileview 指示点位*/
    cfg->compass_style = AREX_COMPASS_CLASSIC;
    cfg->mask_enabled  = false;

    /* ========== [R] 主题模式预留（当前固定为 Left Grid + Right Cards==========
     * 可选扩展为 Classic 上下流式布局，届时渲染代码需读取以下字段
     *   - theme_mode        AREX_THEME_CLASSIC
     *   - h_depth / h_ndl / h_pod / h_batt / h_gas / h_time   上下分区高度
     *   - sep_style / sep_thick                                分割线样
     *   - split_outward / flash_speed                          动画参数
     *   - title_h_u / h_menu_item / gap_menu                  菜单排版
     *   - h_tissues_chart                                     组织图高
     */
    cfg->theme_mode    = AREX_THEME_TECH;    /* 当前固定 TECH（Left Grid + Right Cards*/
    cfg->sep_style     = AREX_SEP_DASHED;    /* [R] 分割线样式（待用*/
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
    cfg->custom_cards[0].widgets[0]  = (arex_grid_widget_t)
    {
        WIDGET_DEPTH_1612,      0, 0
    };
    cfg->custom_cards[0].widgets[1]  = (arex_grid_widget_t)
    {
        WIDGET_TEMP_0806,      2, 0
    };
    cfg->custom_cards[0].widgets[2]  = (arex_grid_widget_t)
    {
        WIDGET_HEADING_0806,   3, 0
    };
    cfg->custom_cards[0].widgets[3]  = (arex_grid_widget_t)
    {
        WIDGET_EMPTY,           0, 2
    };  /* SAC 已移*/
    cfg->custom_cards[0].widgets[4]  = (arex_grid_widget_t)
    {
        WIDGET_BATTERY_0806,   2, 2
    };
    cfg->custom_cards[0].widgets[5]  = (arex_grid_widget_t)
    {
        WIDGET_PPO2_0806,       4, 2
    };
    cfg->custom_cards[0].widgets[6]  = (arex_grid_widget_t)
    {
        WIDGET_NDL_STOP_1606,  0, 3
    };
    cfg->custom_cards[0].widgets[7]  = (arex_grid_widget_t)
    {
        WIDGET_TTS_0806,       2, 3
    };
    cfg->custom_cards[0].widgets[8]  = (arex_grid_widget_t)
    {
        WIDGET_CNS_0806,       4, 3
    };
    cfg->custom_cards[0].widgets[9]  = (arex_grid_widget_t)
    {
        WIDGET_POD_0806,       0, 4
    };
    cfg->custom_cards[0].widgets[10] = (arex_grid_widget_t)
    {
        WIDGET_POD_0806,       2, 4
    };
    cfg->custom_cards[0].widgets[11] = (arex_grid_widget_t)
    {
        WIDGET_EMPTY,          4, 4
    };  /* 淇濈暀绌烘Ы */

    /* ========== [A] 左侧 2x7 固定网格 (160x420) ==========
     * 160x420 区域 = 280px) x 760px)，由 arex_render_left_anchor_grid() 渲染
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
    cfg->left_widgets[0] = (arex_grid_widget_t)
    {
        WIDGET_NDL_STOP_1606,   0, 0
    };
    cfg->left_widgets[1] = (arex_grid_widget_t)
    {
        WIDGET_DEPTH_1612,      0, 1
    };
    cfg->left_widgets[2] = (arex_grid_widget_t)
    {
        WIDGET_DIVE_TIME_1606,  0, 3
    };  /* 潜水时间 */
    cfg->left_widgets[3] = (arex_grid_widget_t)
    {
        WIDGET_GAS_1606,        0, 4
    };
    /* Ĭϲȹر POD1/POD2 ʾλӰ߶ */
    cfg->left_widgets[4] = (arex_grid_widget_t)
    {
        WIDGET_EMPTY,           0, 5
    };
    cfg->left_widgets[5] = (arex_grid_widget_t)
    {
        WIDGET_EMPTY,           1, 5
    };
    cfg->left_widgets[6] = (arex_grid_widget_t)
    {
        WIDGET_SYS_1606,        0, 6
    };

    /* 动态计算实widget 数量（以最后一个非widget 为准*/
    cfg->left_widget_count = 0;
    for (int i = 0; i < AREX_LEFT_MAX_WIDGETS; i++)
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
    cfg->card_order[CARD_POS_6]      = CARD_ID_BLANK;      /* 空白卡片 */
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
    cfg->conservatism   = 1;    /* MED */
    cfg->brightness     = 1;    /* ECO */
}

/* =========================================================
 * 安全区边界检
 * ========================================================= */
bool arex_safe_zone_in_danger(void)
{
    int16_t max_offset_x = (int16_t)((AREX_PHYSICAL_W - g_sys_config.safe_zone_w) / 2);
    int16_t max_offset_y = (int16_t)((AREX_PHYSICAL_H - g_sys_config.safe_zone_h) / 2);

    if (g_sys_config.offset_x < -max_offset_x || g_sys_config.offset_x > max_offset_x)
        return true;
    if (g_sys_config.offset_y < -max_offset_y || g_sys_config.offset_y > max_offset_y)
        return true;

    /* 面镜盲区掩膜检*/
    if (g_sys_config.mask_enabled)
    {
        int16_t bottom_edge = (int16_t)(AREX_PHYSICAL_H / 2 + g_sys_config.safe_zone_h / 2 + g_sys_config.offset_y);
        if (bottom_edge > AREX_PHYSICAL_H - AREX_MASK_EDGE_GUARD)
            return true;
    }

    return false;
}

/* =========================================================
 * 辅助：计Safe Zone 内部可用区域
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
 * 左锚 (0, 0), 160px, safe_zone_h
 * 右卡 (160+gap, 0), safe_zone_w-160-gap, safe_zone_h
 * 翻转 交换左右 X
 * ========================================================= */
void arex_calc_tech_layout(int16_t *out_lx, int16_t *out_ly,
                           uint16_t *out_lw, uint16_t *out_lh,
                           int16_t *out_rx, int16_t *out_ry,
                           uint16_t *out_rw, uint16_t *out_rh)
{
    uint16_t gap = g_sys_config.gap_u * AREX_BASE_U;

    if (g_sys_config.layout_order == AREX_ORDER_NORMAL)
    {
        *out_lx = 0;
        *out_rx = (int16_t)(AREX_LEFT_ANCHOR_W + gap);
    }
    else
    {
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
 * 上区: (0, 0), safe_zone_w, 10U 累加计算
 * 下区: (0, top_h+gap), safe_zone_w, safe_zone_h-top_h-gap
 * 翻转 交换上下 Y
 * ========================================================= */
void arex_calc_classic_layout(int16_t *out_top_x, int16_t *out_top_y,
                              uint16_t *out_top_w, uint16_t *out_top_h,
                              int16_t *out_bot_x, int16_t *out_bot_y,
                              uint16_t *out_bot_w, uint16_t *out_bot_h)
{
    uint16_t gap = g_sys_config.gap_u * AREX_BASE_U;

    /* 计算上区总高= 各模块高度累*/
    uint16_t top_h = 0;
    top_h += g_sys_config.h_depth * AREX_BASE_U;               /* DEPTH */
    top_h += g_sys_config.h_ndl * AREX_BASE_U + gap;            /* NDL/TTS 鍙屾嫾 */
    top_h += g_sys_config.h_pod * AREX_BASE_U + gap;            /* POD 鍙屾嫾 */
    top_h += g_sys_config.h_batt * AREX_BASE_U + gap;           /* BATT 鍙屾嫾 */
    top_h += g_sys_config.h_gas * AREX_BASE_U + gap;            /* GAS */
    top_h += g_sys_config.h_time * AREX_BASE_U;                 /* DIVE TIME */

    /* 零高度保护：最AREX_MIN_CLASSIC_TOP_H px */
    if (top_h < AREX_MIN_CLASSIC_TOP_H) top_h = AREX_MIN_CLASSIC_TOP_H;

    uint16_t bottom_h = (g_sys_config.safe_zone_h > top_h + gap)
                        ? (g_sys_config.safe_zone_h - top_h - gap)
                        : AREX_MIN_CLASSIC_TOP_H;

    if (g_sys_config.layout_order == AREX_ORDER_NORMAL)
    {
        *out_top_x = 0;
        *out_bot_x = 0;
        *out_top_y = 0;
        *out_bot_y = (int16_t)(top_h + gap);
    }
    else
    {
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
 * 计算每个 widget 单元格的绝对位置
 * 鏀跺埌 row(0~5), col(0~4), w_span(1~2), h_span(1~2)
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

    /* 边界修正，防止越*/
    if (*out_x + *out_w > parent_w) *out_w = parent_w - *out_x;
    if (*out_y + *out_h > parent_h) *out_h = parent_h - *out_y;
}

/* =========================================================
 * 16 柱组织图 X 坐标推算
 *
 * 底部对齐6 等分柱状图
 * 每根柱宽 = total_w / 16，X = i * col_w
 * ========================================================= */
void arex_calc_tissue_bars(uint16_t total_w, uint16_t bar_max_h,
                           int16_t out_x[16], uint16_t out_w[16])
{
    uint16_t col_w = total_w / 16;
    for (uint8_t i = 0; i < 16; i++)
    {
        out_x[i] = (int16_t)(i * col_w);
        out_w[i] = col_w;
    }
    (void)bar_max_h; /* 柱高由调用方按百分比算出，此处只返回 X W */
}

/* =========================================================
 * LVGL 样式辅助
 * ========================================================= */

/* AREX_ALIGN_* 转换LVGL 对齐方式 */
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
 * 字体映射(Font Mapper)
 *
 * 全系统唯一允许将字ID 转换为真lvgl 字体指针的地方
 * 所有配置结构体中保存的 title_font / val_font 均应arex_font_id_t 值
 *
 * ID 映射表：
 *   AREX_FONT_ID_SMALL  (0) 20px  标签/单位/Badge
 *   AREX_FONT_ID_TITLE  (1) 20px  菜单卡片标题
 *   AREX_FONT_ID_MEDIUM (2) 32px  数据
 *   AREX_FONT_ID_LARGE  (3) 64px  深度大数
 *   AREX_FONT_ID_HUGE   (4) 64px  大字
 *   AREX_FONT_ID_NDL    (5) 48px  NDL减压时间
 * ========================================================= */
const lv_font_t *arex_get_font(uint8_t font_id)
{
    switch (font_id)
    {
    case AREX_FONT_ID_SMALL:
        return AREX_FONT_SMALL;   /* 20px */
    case AREX_FONT_ID_TITLE:
        return AREX_FONT_TITLE;   /* 20px */
    case AREX_FONT_ID_MEDIUM:
        return AREX_FONT_MEDIUM;  /* 32px */
    case AREX_FONT_ID_LARGE:
        return AREX_FONT_LARGE;   /* 64px */
    case AREX_FONT_ID_HUGE:
        return AREX_FONT_HUGE;    /* 64px */
    case AREX_FONT_ID_NDL:
        return AREX_FONT_NDL;     /* 48px */
    default:
        return AREX_FONT_SMALL;   /* 兜底：永不为 NULL */
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
void arex_ui_init(void)
{
    /* 1. 加载持久化配置，失败则用默认值保*/
    if (!arex_config_load(&g_sys_config))
    {
        arex_sys_config_defaults(&g_sys_config);
    }

    /* 2. 传感器数据清*/
    arex_data_init();
}

/* =========================================================
 * 应用配置变更 (配置界面修改后调用此函数)
 * 1. 检safe zone 边界
 * 2. 重建 left anchor 排版
 * 3. 重建 right card 布局
 * ========================================================= */
void arex_ui_apply_config(void)
{
    /* 安全区边界校*/
    if (arex_safe_zone_in_danger())
    {
        /* TODO: 触发危险警告 UI */
    }

    /* TODO: 触发 arex_screen_rebuild_layout() 重建排版
     * 这将在重arex_screen.c 时实
     */
}

/* =========================================================
 * 卡片顺序查询 (统一入口 替代旧的 g_arex_card_order)
 * ========================================================= */
uint8_t g_sys_card_order(uint8_t pos)
{
    if (pos >= AREX_CARD_COUNT) return 0;
    return g_sys_config.card_order[pos];
}

/* =========================================================
 * 通用动态菜单工
 * 所有尺寸从 g_sys_config 推算，不含硬编码像素值
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
    int item_w = right_canvas_w - 15;  /* 右侧 15px 呼吸*/

    int current_y = start_y;
    for (uint8_t i = 0; i < item_count; i++)
    {
        const arex_menu_item_cfg_t *item_cfg = &items[i];
        /* height_u 默认 0 h_menu_item (单位 U) */
        int item_h = (int)(item_cfg->height_u > 0 ? item_cfg->height_u : g_sys_config.h_menu_item)
                     * AREX_BASE_U;
        /* gap_y gap_menu (单位 U) 推算 */
        int gap_y = (int)g_sys_config.gap_menu * AREX_BASE_U;

        lv_obj_t *item = lv_obj_create(parent_card);
        lv_obj_remove_style_all(item);
        lv_obj_set_pos(item, 0, current_y);
        lv_obj_set_size(item, item_w, item_h);
        lv_obj_set_style_bg_color(item, AREX_BLACK, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, AREX_DARK, 0);
        lv_obj_set_style_border_width(item, AREX_CARD_DEBUG_BORDERS ? item_cfg->border_width : 0, LV_PART_MAIN);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, LV_PART_MAIN);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        /* 标题 label */
        if (item_cfg->title_text)
        {
            lv_obj_t *title_lbl = lv_label_create(item);
            lv_label_set_text(title_lbl, item_cfg->title_text);
            lv_obj_set_style_text_font(title_lbl, arex_get_font(item_cfg->title_font_id), 0);
            lv_obj_set_style_text_color(title_lbl, AREX_GREEN, 0);
            lv_obj_set_size(title_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 12, 0);
            lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
        }

        /* 鍙充晶寰界珷 label */
        if (item_cfg->value_badge)
        {
            lv_obj_t *badge_lbl = lv_label_create(item);
            lv_label_set_text(badge_lbl, item_cfg->value_badge);
            lv_obj_set_style_text_font(badge_lbl, arex_get_font(item_cfg->value_font_id), 0);
            lv_obj_set_style_text_color(badge_lbl, AREX_LIGHT, 0);
            lv_obj_set_size(badge_lbl, 80, 28);
            lv_obj_align(badge_lbl, LV_ALIGN_RIGHT_MID, -12, 0);
            lv_obj_set_style_text_align(badge_lbl, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_long_mode(badge_lbl, LV_LABEL_LONG_DOT);
        }

        if (out_item_handles)
        {
            out_item_handles[i] = item;
        }

        current_y += item_h + gap_y;
    }
}

/* =========================================================
 * 通用卡片标题渲染
 * 标题文字(Y=8)与分割线(Y=48)为视觉组合，绝对焊死在卡片顶部
 * AREX_CARD_TITLE_H 仅作为下方内容区（菜单/图表）的起始 Y 坐标偏移。
 *
 * parent_card: 父容器（tile 对象）
 * title_text:  标题文字
 *
 * 标题布局（焊死，绝对不跟AREX_CARD_TITLE_H）：
 *   文字:   Y=8,  高度 40px，AREX_LIGHT
 *   分割线: Y=48, h=2px, AREX_DARK
 * ========================================================= */
void arex_render_card_title(lv_obj_t *parent_card, const char *title_text)
{
    uint16_t right_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                       - ((int)g_sys_config.gap_u * AREX_BASE_U);

    /* 1. 标题文字：扒光默认样+ 强制小字次级颜色 */
    lv_obj_t *lbl = lv_label_create(parent_card);
    lv_obj_remove_style_all(lbl);
    lv_obj_set_pos(lbl, 16, 8);
    lv_obj_set_size(lbl, right_w - 32, 40);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl, title_text);
    lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);

    /* 2. 分割线：绝对固定在文字下方（焊死 Y=48*/
    lv_obj_t *line = lv_obj_create(parent_card);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, right_w - 32, 2);
    lv_obj_set_pos(line, 16, 48);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(line, AREX_DARK, 0);
}

/* =========================================================
 * 5F 自定义网格组件外部容器（arex_screen.c 注入
 * ========================================================= */
lv_obj_t *g_left_anchor_obj = NULL;
/* 多张自定义卡片容器数*/
lv_obj_t *g_card_custom_objs[AREX_MAX_CUSTOM_CARDS];
uint8_t   g_card_custom_obj_count;

/* =========================================================
 * 5F 㣨ѧӳ䣬 lv_grid
 *
 * 核心公式
 *   cell_w = parent_w / 5
 *   cell_h = parent_h / 6
 *   abs_x  = col * cell_w + gap
 *   abs_y  = row * cell_h + gap
 *   abs_w  = span_w * cell_w - gap*2
 *   abs_h  = span_h * cell_h - gap*2
 * ========================================================= */
#define WIDGET_GAP  0   /* 网格缝隙 px */

/* =========================================================
 * 5F 网格坐标推算（锁5 + 标题避让，动cell_h 自适应
 *
 * parent_w/parent_h: 父容器总尺寸（用于动态推算）
 * row/col: 网格行列索引(0~5 / 0~4)
 * span_w/span_h: 跨越的列行数
 * out_*: 输出绝对坐标
 *
 * 排版矩阵严格锁定 5 列：
 *   cell_w = parent_w / 5
 *   cell_h = (parent_h - AREX_CARD_TITLE_H) / 6
 * Y 坐标增加 AREX_CARD_TITLE_H=60px 偏移，确保第一行落在标题区下方
 * 宽高4px (2px 缝隙 x2) 制造四2px 物理留白
 * 如果标题高度改为其他值，cell_h 会自动重新计算，内容区完美自适应
 * ========================================================= */
void arex_calc_widget_grid(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t span_w, uint8_t span_h,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h)
{
    /* 锁定 5 列基准，动态计cell_h */
    uint16_t cell_w = parent_w / 5;
    uint16_t cell_h = (parent_h > AREX_CARD_TITLE_H)
                      ? ((parent_h - AREX_CARD_TITLE_H) / AREX_WIDGET_ROWS)
                      : 60;  /* 淇濆簳 fallback */

    /* X: 列偏+ 缝隙(2px) */
    *out_x = (int16_t)(col * cell_w + WIDGET_GAP);
    /* Y: 标题区下+ 行偏+ 缝隙(2px) */
    *out_y = (int16_t)(AREX_CARD_TITLE_H + row * cell_h + WIDGET_GAP);
    /* 宽高: 跨距×基准 - 4px 缝隙(四周2px) */
    *out_w = (uint16_t)(span_w * cell_w - WIDGET_GAP * 2);
    *out_h = (uint16_t)(span_h * cell_h - WIDGET_GAP * 2);

    /* 边界修正（以容器总尺寸为边界*/
    if (*out_x + *out_w > (int16_t)parent_w)
        *out_w = (uint16_t)((int16_t)parent_w - *out_x);
    if (*out_y + *out_h > (int16_t)parent_h)
        *out_h = (uint16_t)((int16_t)parent_h - *out_y);
}

/* =========================================================
 * NDL 底部横向 10 宫格进度条绘制回(0 RAM)
 * 数学推演：容器宽abs_w - 16，两边各8px 边距
 * 10个块 + 9px间隙 = 137px（完美填满）
 * ========================================================= */
static void ndl_horiz_bar_draw_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    lv_draw_ctx_t * draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t * area = &obj->coords;

    int total_w = lv_area_get_width(area);
    int gap = 3;
    int block_w = (total_w - 9 * gap) / 10;
    if (block_w < 1) block_w = 1;

    /* 计算总体百分比：
     * - 常态：NDL/99 显示9 视为满格
     * - 安全停留：未进站前仍NDL；进站后按停留剩余时间缩
     * - 减压停留：未进站前保持满格；进站后按当前减压站剩余时间缩*/
    float pct = 0.0f;
    if (g_sensor_data.stop_type == AREX_STOP_NONE)
    {
        pct = (float)g_sensor_data.ndl / 99.0f;
    }
    else if (g_sensor_data.stop_type == AREX_STOP_SAFETY)
    {
        if (!g_sensor_data.in_stop_zone)
        {
            pct = (float)g_sensor_data.ndl / 99.0f;
        }
        else if (g_sensor_data.stop_time_total_s > 0)
        {
            pct = (float)g_sensor_data.stop_time_left_s / g_sensor_data.stop_time_total_s;
        }
        else
        {
            pct = 1.0f;
        }
    }
    else if (g_sensor_data.stop_type == AREX_STOP_DECO)
    {
        if (!g_sensor_data.in_stop_zone)
        {
            pct = 1.0f;
        }
        else if (g_sensor_data.stop_time_total_s > 0)
        {
            pct = (float)g_sensor_data.stop_time_left_s / g_sensor_data.stop_time_total_s;
        }
    }
    if (pct > 1.0f) pct = 1.0f;
    if (pct < 0.0f) pct = 0.0f;

    int active_blocks = (int)(pct * 10.0f);
    float remainder = (pct * 10.0f) - active_blocks;

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.radius = 0; /* 纯直*/

    for (int i = 0; i < 10; i++)
    {
        int x1 = area->x1 + i * (block_w + gap);
        int x2 = x1 + block_w - 1;
        lv_area_t block_area = {x1, area->y1, x2, area->y2};

        if (i < active_blocks)
        {
            /* 全亮格子 */
            rect_dsc.bg_color = AREX_GREEN;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_draw_rect(draw_ctx, &rect_dsc, &block_area);
        }
        else if (i == active_blocks && remainder > 0.05f)
        {
            /* 半亮格子 (先画暗底，再盖亮 */
            rect_dsc.bg_color = AREX_DARK;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_draw_rect(draw_ctx, &rect_dsc, &block_area);

            int partial_w = (int)(block_w * remainder);
            if (partial_w > 0)
            {
                lv_area_t partial_area = {x1, area->y1, x1 + partial_w - 1, area->y2};
                rect_dsc.bg_color = AREX_GREEN;
                lv_draw_rect(draw_ctx, &rect_dsc, &partial_area);
            }
        }
        else
        {
            /* 未激活的暗格 */
            rect_dsc.bg_color = AREX_DARK;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_draw_rect(draw_ctx, &rect_dsc, &block_area);
        }
    }
}

/* =========================================================
 * 创建单个自定义组件（组件工厂 左侧网格 + 5F 共用
 *
 * 关键：每个组件的 lv_obj_set_user_data() 存储了标签烙印
 * 对于 POD，使用高位掩码区分（1033=POD1, 2033=POD2）
 * 告警引擎靠这个烙印实左侧锚点 + 5F 组件同时闪烁"
 *
 * 架构铁律
 *   - 位置参数 (abs_x/y/w/h, span_w/h) 由调用方传入
 *   - 样式参数 (font, offsets) arex_get_widget_style(w_id) 自动查表
 *   - cfg_font_id != 255 时强制覆盖自动字
 *   - 速率图标由工厂自主查字典决定（根elements & ELEM_BAR
 *   - 专属组件（DEPTH/NDL）走早期返回，内部仍style 参数
 *   - 通用组件elements 掩码装配流水线：TITLE VALUE UNIT BAR
 *
 * POD 单模具轮转分配：
 *   - 函数入口检w_id == WIDGET_POD_0806
 *   - 调用 arex_get_pod_tag() 获得高位掩码标签 (1033/2033)
 *   - 调用 arex_get_pod_index() 获得 POD 编号 (1/2)
 *   - 将标签烙印到容器 user_data
 * ========================================================= */
lv_obj_t *render_widget_by_id(lv_obj_t *parent,
                              arex_widget_id_t w_id,
                              int16_t abs_x, int16_t abs_y,
                              uint16_t abs_w, uint16_t abs_h,
                              uint8_t span_w, uint8_t span_h,
                              arex_font_id_t cfg_font_id)
{
    /* ===== POD 单模具拦截：提前消耗计数器 ===== */
    bool is_pod_mold = (w_id == WIDGET_POD_0806);
    uint8_t pod_index = 0;        /* POD number 1 or 2 */
    uintptr_t pod_tag = 0;        /* POD tag 1033 or 2033 */
    if (is_pod_mold)
    {
        s_pod_render_count++;     /* Increment first, then get current value */
        pod_index = arex_get_pod_index();
        pod_tag = arex_get_pod_tag();
    }

    const arex_widget_style_t *style = arex_get_widget_style(w_id);
    if (!style) return NULL;

    /* 字号选择逻辑
     *   cfg_font_id != 255 强制覆盖（运行时指定
     *   DEPTH 系列 自动适配尺寸（HUGE/MEDIUM/SMALL
     *   其他组件 直接使用字典 font_id */
    arex_font_id_t val_font_id;
    if (cfg_font_id != (arex_font_id_t)255)
    {
        val_font_id = cfg_font_id;  /* 强制覆盖（运行时指定*/
    }
    else if (w_id == WIDGET_DEPTH_1612 || w_id == WIDGET_DEPTH_1606)
    {
        /* DEPTH 组件：自动适配尺寸 */
        if (span_w >= 2 && span_h >= 2)
        {
            val_font_id = AREX_FONT_ID_HUGE;
        }
        else if (span_w >= 2)
        {
            val_font_id = AREX_FONT_ID_MEDIUM;
        }
        else
        {
            val_font_id = AREX_FONT_ID_SMALL;
        }
    }
    else
    {
        /* 其他组件：直接使用字font_id */
        val_font_id = style->font_id;
    }

    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, abs_x, abs_y);
    lv_obj_set_size(obj, abs_w, abs_h);
    lv_obj_set_style_bg_color(obj, AREX_BLACK, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, AREX_DARK, 0);
    lv_obj_set_style_border_width(obj, AREX_DEBUG_BORDERS ? 1 : 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 2, 0);

    /* 封杀所有滚动条 */
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);

    /* ===== 靶向告警烙印 =====
     * POD uses high-bit mask tags (1033/2033), others use raw w_id */
    if (is_pod_mold)
    {
        lv_obj_set_user_data(obj, (void *)pod_tag);
    }
    else
    {
        lv_obj_set_user_data(obj, (void *)(uintptr_t)w_id);
    }

    if (w_id == WIDGET_EMPTY) return obj;

    /* ===== DEPTH 2x2 专属渲染（整小数+单位分离===== */
    bool is_2x2 = (span_w >= 2 && span_h >= 2);
    if (w_id == WIDGET_DEPTH_1612 && is_2x2)
    {
        /* 样式参数来自 arex_widget_style_t */
        const arex_style_depth_t *s = &style->spec.depth;

        /* ==========================================
         * 1. 超大号整-> 宽度必须紧密包裹
         * ========================================== */
        lv_obj_t *int_lbl = lv_label_create(obj);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT) lv_label_set_text(int_lbl, "--");
        else lv_label_set_text_fmt(int_lbl, "%d", (int)g_sensor_data.depth);
        // 字体从字典读取（font_id = HUGE 58px
        lv_obj_set_style_text_font(int_lbl, arex_get_font(style->font_id), 0);
        lv_obj_set_style_text_color(int_lbl, AREX_GREEN, 0);

        // 绝杀技：必须设CONTENT！这样无论变"6" 还是 "45"
        // Label 的右边缘都会死死包住个位数，绝不留一丝缝隙！
        lv_obj_set_size(int_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

        // 读取字典中的 RIGHT_MID -45，把右边缘焊死在这堵墙上
        lv_obj_align(int_lbl, (lv_align_t)s->int_align, s->int_offset_x, s->int_offset_y);

        /* ==========================================
         * 2. 中号小数 -> 紧贴整数的右边界
         * ========================================== */
        lv_obj_t *dec_lbl = lv_label_create(obj);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT) lv_label_set_text(dec_lbl, ".-");
        else
        {
            /* 提取小数部分：只保留一位小数，范围 0-9 */
            float decimal_part = fabsf(g_sensor_data.depth - (int)g_sensor_data.depth);
            int dd = (int)(decimal_part * 10 + 0.5f);
            if (dd > 9) dd = 9;  /* 防止浮点精度问题导致多位*/
            lv_label_set_text_fmt(dec_lbl, ".%d", dd);
        }
        // 字体从字典读取（title_font_id = MEDIUM 28px，小数比整数小）
        lv_obj_set_style_text_font(dec_lbl, arex_get_font(style->title_font_id), 0);
        lv_obj_set_style_text_color(dec_lbl, AREX_GREEN, 0);
        lv_obj_set_size(dec_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

        // 因为整数的右边缘(个位被焊死了，小数挂在它右边，自然就永远贴紧个位数！
        lv_obj_align_to(dec_lbl, int_lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, s->dec_offset_x, s->dec_offset_y);

        /* ==========================================
         * 3. 小号单位 (m) -> 紧贴小数正下
         * ========================================== */
        if (style->elements & ELEM_UNIT)
        {
            lv_obj_t *unit_lbl = lv_label_create(obj);
            lv_label_set_text(unit_lbl, style->unit ? style->unit : "");
            // 单位固定用小号字
            lv_obj_set_style_text_font(unit_lbl, arex_get_font(AREX_FONT_ID_SMALL), 0);
            lv_obj_set_style_text_color(unit_lbl, AREX_LIGHT, 0);
            lv_obj_set_size(unit_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align_to(unit_lbl, dec_lbl, LV_ALIGN_OUT_BOTTOM_MID, s->unit_offset_x, s->unit_offset_y);
        }

        /* 速率图标：工厂自主查字典判断是否需要绘*/
        bool needs_bar_icon = (style->elements & ELEM_BAR) != 0;
        if (needs_bar_icon)
        {
            lv_obj_t *sudu_img = lv_img_create(obj);
            lv_img_set_src(sudu_img, &sudo_up_level0);
            lv_obj_align(sudu_img, (lv_align_t)s->icon_align, s->icon_offset_x, s->icon_offset_y);
            if (s_ascent_icon_count < MAX_ASCENT_ICONS)
                s_img_ascent_rate[s_ascent_icon_count++] = sudu_img;
        }
        return obj;
    }
    else if (w_id == WIDGET_NDL_STOP_1606)
    {
        /* NDL 变形金刚：从 style->spec.ndl_stop 读取所有位置参*/
        if (s_ndl_handle_count >= MAX_NDL_ICONS) return obj;
        ndl_handle_t *h = &s_ndl_handles[s_ndl_handle_count++];
        h->comp = obj;

        const arex_style_ndl_stop_t *s = &style->spec.ndl_stop;

        /* 创建 10 宫格的底层透明画板 */
        h->horiz_bg = lv_obj_create(obj);
        lv_obj_remove_style_all(h->horiz_bg);
        /* 🚨 宽度填满减去两边留白：abs_w - 16，两边各8px */
        lv_obj_set_size(h->horiz_bg, abs_w - 16, 10);
        /* 贴紧底部，略微上4px */
        lv_obj_align(h->horiz_bg, LV_ALIGN_BOTTOM_MID, 0, -4);
        lv_obj_add_event_cb(h->horiz_bg, ndl_horiz_bar_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
        lv_obj_add_flag(h->horiz_bg, LV_OBJ_FLAG_HIDDEN);

        /* 顶部标题（默认隐藏，停留态时显示*/
        h->title_top = lv_label_create(obj);
        lv_obj_set_style_text_font(h->title_top, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(h->title_top, AREX_GREEN, 0);
        lv_label_set_text(h->title_top, "");
        lv_obj_add_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);

        /* 主数(22, 3:00) - 使用48px字体 */
        h->main_val = lv_label_create(obj);
        lv_obj_set_style_text_color(h->main_val, AREX_GREEN, 0);
        lv_obj_set_style_text_font(h->main_val, arex_get_font(AREX_FONT_ID_NDL), 0);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(h->main_val, "--");
        else
            lv_label_set_text_fmt(h->main_val, "%d", g_sensor_data.ndl);

        /* 底部标题 (NDL 45) */
        h->sub_bot = lv_label_create(obj);
        lv_obj_set_style_text_font(h->sub_bot, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(h->sub_bot, AREX_GREEN, 0);
        lv_obj_add_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN);
        return obj;
    }
    else if (w_id == WIDGET_SYS_1606)
    {
        /* ===== SYS 模块：电+ 温度横向排列 ===== */

        /* 左侧：电Label */
        s_sys_batt_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(s_sys_batt_lbl, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
        lv_obj_set_style_text_color(s_sys_batt_lbl, AREX_GREEN, 0);
        lv_obj_align(s_sys_batt_lbl, LV_ALIGN_LEFT_MID, 4, 0);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(s_sys_batt_lbl, "--%");
        else
            lv_label_set_text_fmt(s_sys_batt_lbl, "%u%%", arex_ui_clamp_battery_pct(g_sensor_data.battery_pct));

        /* 右侧：温Label */
        s_sys_temp_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(s_sys_temp_lbl, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
        lv_obj_set_style_text_color(s_sys_temp_lbl, AREX_GREEN, 0);
        lv_obj_align(s_sys_temp_lbl, LV_ALIGN_RIGHT_MID, -4, 0);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(s_sys_temp_lbl, "-- C");
        else
        {
            int t_int = (int)g_sensor_data.temperature_c;
            int t_dec = (int)(fabsf(g_sensor_data.temperature_c - t_int) * 10);
            lv_label_set_text_fmt(s_sys_temp_lbl, "%d.%d C", t_int, t_dec);
        }

        return obj;
    }

    /* ===== 通用流水线：elements 掩码按需装配零件 =====
     * POD1/POD2/WTIME 及所1x1/2x1 通用组件走此路径
     * ELEM_TITLE ELEM_VALUE ELEM_UNIT ELEM_BAR
     *
     * 样式参数全部来自 arex_get_widget_style(w_id) 查表结果
     * title 文本和数值数据源依赖 w_id switch 分发 */

    /* --- 零件 1：标--- */
    if ((style->elements & ELEM_TITLE) && style->title)
    {
        lv_obj_t *title_lbl = lv_label_create(obj);
        /* POD 单模具：根据 pod_index 动态决定标题文*/
        if (is_pod_mold)
        {
            lv_label_set_text_fmt(title_lbl, "POD %d", pod_index);
        }
        else
        {
            lv_label_set_text(title_lbl, style->title);
        }
        lv_obj_set_style_text_font(title_lbl, arex_get_font(style->title_font_id), 0);
        lv_obj_set_style_text_color(title_lbl, AREX_LIGHT, 0);
        lv_obj_set_size(title_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(title_lbl, (lv_align_t)style->title_align,
                     style->title_offset_x, style->title_offset_y);
        lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
    }

    /* --- 零件 2：主数--- */
    lv_obj_t *val_lbl = NULL;
    if (style->elements & ELEM_VALUE)
    {
        val_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(val_lbl, arex_get_font(val_font_id), 0);
        lv_obj_set_style_text_color(val_lbl, AREX_GREEN, 0);

        if (AREX_SHOW_PLACEHOLDER_ON_INIT)
        {
            /* 通用占位*/
            lv_label_set_text(val_lbl, "--");
        }
        else
        {
            char buf[48] = "--";
            switch (w_id)
            {
            case WIDGET_DEPTH_1612:
            case WIDGET_DEPTH_1606:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.depth);
                break;
            case WIDGET_NDL_STOP_1606:
                snprintf(buf, sizeof(buf), "%d", g_sensor_data.ndl_stop_value);
                break;
            case WIDGET_DIVE_TIME_1606:
                snprintf(buf, sizeof(buf), "%02d:%02d", g_sensor_data.dive_time_s/60, g_sensor_data.dive_time_s%60);
                break;
            case WIDGET_GAS_1606:
                snprintf(buf, sizeof(buf), "%s", g_sensor_data.gas_name);
                break;
            case WIDGET_SYS_1606:
                snprintf(buf, sizeof(buf), "%02d:%02d", g_sensor_data.sys_time_h, g_sensor_data.sys_time_m);
                break;
            case WIDGET_TEMP_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.temperature_c);
                break;
            case WIDGET_TIME_1606:
                snprintf(buf, sizeof(buf), "%02d:%02d", g_sensor_data.sys_time_h, g_sensor_data.sys_time_m);
                break;
            case WIDGET_TTS_0806:
                snprintf(buf, sizeof(buf), "%d", g_sensor_data.tts);
                break;
            case WIDGET_ASCENT_0806:
            case WIDGET_ASCENT_0812:
                snprintf(buf, sizeof(buf), "%+.1f", (double)g_sensor_data.ascent_rate);
                break;
            case WIDGET_COMPASS_1612:
                snprintf(buf, sizeof(buf), "%03d", g_sensor_data.heading);
                break;
            case WIDGET_BATTERY_0806:
                snprintf(buf, sizeof(buf), "%u", arex_ui_clamp_battery_pct(g_sensor_data.battery_pct));
                break;
            case WIDGET_STOP_DEPTH_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.stop_depth_m);
                break;
            case WIDGET_STOP_TIME_1606:
                snprintf(buf, sizeof(buf), "%d", g_sensor_data.stop_time_left_s);
                break;
            case WIDGET_PPO2_0806:
                snprintf(buf, sizeof(buf), "%.2f", (double)g_sensor_data.ppo2[g_sensor_data.gas_active_idx]);
                break;
            case WIDGET_SURF_GF_0806:
                snprintf(buf, sizeof(buf), "%.2f", (double)g_sensor_data.surf_gf);
                break;
            case WIDGET_GF99_0806:
                snprintf(buf, sizeof(buf), "%.0f", (double)g_sensor_data.gf99);
                break;
            case WIDGET_CNS_0806:
                snprintf(buf, sizeof(buf), "%d", g_sensor_data.cns_pct);
                break;
            case WIDGET_OTU_0806:
                snprintf(buf, sizeof(buf), "%d", g_sensor_data.otu);
                break;
            case WIDGET_GF_0806:
                snprintf(buf, sizeof(buf), "%d/%d", g_sensor_data.gf_low, g_sensor_data.gf_high);
                break;
            case WIDGET_MOD_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.mod_m);
                break;
            case WIDGET_CEILING_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.ceiling_m);
                break;
            case WIDGET_GAS_MIX_1606:
                snprintf(buf, sizeof(buf), "%d/%d", g_sensor_data.gas_o2_pct, g_sensor_data.gas_he_pct);
                break;
            case WIDGET_GAS_DENS_0806:
                snprintf(buf, sizeof(buf), "%.2f", (double)g_sensor_data.gas_density);
                break;
            case WIDGET_FIO2_0806:
                snprintf(buf, sizeof(buf), "%.0f%%", (double)g_sensor_data.fio2_pct);
                break;
            case WIDGET_HEADING_0806:
                snprintf(buf, sizeof(buf), "%03d", g_sensor_data.heading);
                break;
            /* ===== POD 单模具：数据源根pod_index 动态分===== */
            case WIDGET_POD_0806:
                if (is_pod_mold)
                {
                    if (pod_index == 1)
                    {
                        snprintf(buf, sizeof(buf), "%.0f", (double)g_sensor_data.pod1_bar);
                    }
                    else
                    {
                        snprintf(buf, sizeof(buf), "%.0f", (double)g_sensor_data.pod2_bar);
                    }
                }
                else
                {
                    snprintf(buf, sizeof(buf), "--");
                }
                break;
            case WIDGET_DEPTH_MAX_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.max_depth);
                break;
            case WIDGET_DEPTH_AVG_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.avg_depth);
                break;
            case WIDGET_TEMP_MIN_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.min_temp);
                break;
            case WIDGET_TEMP_AVG_0806:
                snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.avg_temp);
                break;
            /* 🚨 以下已废弃，Protobuf 已移除对ID
            case WIDGET_WTIME_0806: {
                uint32_t t = g_sensor_data.surface_time_s;
                snprintf(buf, sizeof(buf), "%02d:%02d", t / 60, t % 60);
            break;
            }
            case WIDGET_TEMP_MAX_0806: snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.max_temp); break;
            case WIDGET_SAC_RATE_0806:  snprintf(buf, sizeof(buf), "%.1f", (double)g_sensor_data.sac_rate); break;
            case WIDGET_PPO2_SAFE_0806: snprintf(buf, sizeof(buf), "%.2f", 1.4); break;
            case WIDGET_NDL_SAFE_0806:  snprintf(buf, sizeof(buf), "%d", 5); break;
            case WIDGET_SAC_SAFE_0806:  snprintf(buf, sizeof(buf), "%.1f", 25.0); break;
            */
            default:
                snprintf(buf, sizeof(buf), "--");
                break;
            }
            lv_label_set_text(val_lbl, buf);
        }
        /* 所有使ELEM_VALUE widget 都使spec.basic.value_align */
        lv_obj_align(val_lbl, (lv_align_t)style->spec.basic.value_align,
                     style->spec.basic.value_offset_x, style->spec.basic.value_offset_y);
        lv_obj_set_user_data(val_lbl, (void *)(uintptr_t)w_id);
    }

    /* --- 零件 3：单--- */
    if ((style->elements & ELEM_UNIT) && style->unit)
    {
        lv_obj_t *unit_lbl = lv_label_create(obj);
        lv_label_set_text(unit_lbl, style->unit);
        lv_obj_set_style_text_font(unit_lbl, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(unit_lbl, AREX_LIGHT, 0);
        /* 单位位于数值右侧（对于 2x1 等窄组件*/
        if ((style->elements & ELEM_VALUE) && (val_lbl != NULL))
        {
            /* 挂在数label 右侧 */
            lv_obj_align_to(unit_lbl, val_lbl, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
        }
        else
        {
            lv_obj_align(unit_lbl, (lv_align_t)style->title_align,
                         style->title_offset_x, style->title_offset_y);
        }
    }

    /* --- 零件 4：特BAR --- */
    if (style->elements & ELEM_BAR)
    {
        if (w_id == WIDGET_DEPTH_1612)
        {
            const arex_style_depth_t *s = &style->spec.depth;
            lv_obj_t *sudu_img = lv_img_create(obj);
            lv_img_set_src(sudu_img, &sudo_up_level0);
            lv_obj_align(sudu_img, (lv_align_t)s->icon_align, s->icon_offset_x, s->icon_offset_y);
            if (s_ascent_icon_count < MAX_ASCENT_ICONS)
                s_img_ascent_rate[s_ascent_icon_count++] = sudu_img;
        }
        else if (w_id == WIDGET_ASCENT_0812)
        {
            /* ASCENT_0812 (1x2)：绘制上升速率方向箭头图标（工厂自主查字典决定*/
            lv_obj_t *sudu_img = lv_img_create(obj);
            lv_img_set_src(sudu_img, &sudo_up_level0);
            lv_obj_align(sudu_img, LV_ALIGN_CENTER, 0, 0);
            if (s_ascent_icon_count < MAX_ASCENT_ICONS)
                s_img_ascent_rate[s_ascent_icon_count++] = sudu_img;
        }
        else if (w_id == WIDGET_COMPASS_1612)
        {
            /* COMPASS_1612 (2x2)：卷tape 在早期分支里，ELEM_BAR 标记spec.compass 驱动 */
        }
        else if (w_id == WIDGET_TISSUE_GF_4012 || w_id == WIDGET_TISSUE_RAW_4012)
        {
            /* TISSUE (4x2)6 柱组织图，ELEM_BAR 标记spec.tissue 驱动 */
        }
        else if (w_id == WIDGET_SYS_1606)
        {
            /* SYS 电池+ 外设图标（系统状态栏*/
            lv_obj_t *bat_bg = lv_obj_create(obj);
            lv_obj_remove_style_all(bat_bg);
            lv_obj_set_size(bat_bg, 60, 14);
            lv_obj_align(bat_bg, LV_ALIGN_BOTTOM_LEFT, 4, -4);
            lv_obj_set_style_border_width(bat_bg, 1, 0);
            lv_obj_set_style_border_color(bat_bg, AREX_GREEN, 0);
            lv_obj_set_style_radius(bat_bg, 2, 0);

            uint8_t pct = arex_ui_clamp_battery_pct(g_sensor_data.battery_pct);
            lv_obj_t *bat_fill = lv_obj_create(bat_bg);
            lv_obj_remove_style_all(bat_fill);
            lv_obj_set_size(bat_fill, LV_PCT(pct > 20 ? 100 : pct), LV_PCT(100));
            lv_obj_align(bat_fill, LV_ALIGN_LEFT_MID, 0, 0);
            lv_obj_set_style_bg_color(bat_fill, pct > 20 ? AREX_GREEN : AREX_LIGHT, 0);
            lv_obj_set_style_bg_opa(bat_fill, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(bat_fill, 1, 0);
            (void)bat_fill;
        }
    }

    return obj;
}

/* =========================================================
 * 5F 网格总线渲染
 *
 * 1. g_sys_config.custom_cards[] 读取组件配置
 * 2. һöôѧСӳ
 * 3. 调用组件工厂渲染，注user_data 烙印
 * 4. 注册外部容器到告警引
 * ========================================================= */
static void render_custom_card_widgets(lv_obj_t *card_custom, uint8_t custom_card_idx)
{
    if (!card_custom || custom_card_idx >= g_sys_config.custom_card_count ||
            custom_card_idx >= AREX_MAX_CUSTOM_CARDS)
    {
        return;
    }

    uint16_t parent_w = lv_obj_get_width(card_custom);
    uint16_t parent_h = lv_obj_get_height(card_custom);
    uint8_t count = g_sys_config.custom_cards[custom_card_idx].widget_count;
    uint16_t fallback_w;

    /* tile 刚创建时，content 尺寸有概率还没稳定；这里直接使用对象宽高
     * 并在异常时回退Safe Zone 推导值，避免自定义组件被算成 0 尺寸*/
    fallback_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                 - (g_sys_config.panel_gap_u * AREX_BASE_U);
    if (parent_w == 0 || parent_w > g_sys_config.safe_zone_w)
    {
        parent_w = fallback_w;
    }
    if (parent_h == 0 || parent_h > g_sys_config.safe_zone_h)
    {
        parent_h = g_sys_config.safe_zone_h;
    }

    if (count > AREX_5F_MAX_WIDGETS)
    {
        count = AREX_5F_MAX_WIDGETS;
    }

    lv_obj_clean(card_custom);
    arex_render_card_title(card_custom, "CUSTOM WIDGETS");

    for (uint8_t i = 0; i < count; i++)
    {
        arex_grid_widget_t *widget = &g_sys_config.custom_cards[custom_card_idx].widgets[i];
        arex_widget_id_t w_id = widget->widget_id;
        uint8_t c = widget->x;
        uint8_t r = widget->y;

        if (w_id == WIDGET_EMPTY) continue;
        if (r >= AREX_WIDGET_ROWS || c >= AREX_WIDGET_COLS) continue;

        /* 从样式表span_w/span_h */
        const arex_widget_style_t *style = arex_get_widget_style(w_id);
        uint8_t span_w = (style != NULL) ? style->span_w : 1;
        uint8_t span_h = (style != NULL) ? style->span_h : 1;

        /* ѧӳ䣨AREX_CARD_TITLE_H ƫ*/
        int16_t abs_x, abs_y;
        uint16_t abs_w, abs_h;
        arex_calc_widget_grid(parent_w, parent_h,
                              r, c, span_w, span_h,
                              &abs_x, &abs_y, &abs_w, &abs_h);

        render_widget_by_id(card_custom, w_id, abs_x, abs_y, abs_w, abs_h,
                            span_w, span_h, (arex_font_id_t)255);
    }
}

void arex_render_5f_custom_grid(lv_obj_t *card_custom, lv_obj_t *left_anchor, uint8_t custom_card_idx)
{
    g_left_anchor_obj = left_anchor;
    if (custom_card_idx < AREX_MAX_CUSTOM_CARDS)
    {
        g_card_custom_objs[custom_card_idx] = card_custom;
        if (g_card_custom_obj_count < (custom_card_idx + 1))
        {
            g_card_custom_obj_count = custom_card_idx + 1;
        }
    }

    render_custom_card_widgets(card_custom, custom_card_idx);
}

/* =========================================================
 * arex_5f_grid_rebuild 重建 5F 自定义网
 *
 * arex_screen_rebuild_layout() 调用，当 BLE 下发新的 5F 布局时触发
 * 直接操作 g_card_custom_objs[] 容器数组，清除并重建所有网格组件
 * ========================================================= */
void arex_5f_grid_rebuild_all(void)
{
    for (uint8_t i = 0; i < g_card_custom_obj_count && i < AREX_MAX_CUSTOM_CARDS; i++)
    {
        if (g_card_custom_objs[i] != NULL)
        {
            render_custom_card_widgets(g_card_custom_objs[i], i);
        }
    }
}

/* =========================================================
 * widget_id 设置数值（由外update 循环调用
 *
 * 架构
 *   - 遍历 g_card_custom_obj g_left_anchor_obj 两个容器
 *   - user_data 烙印匹配 target_id
 *   - POD 使用高位掩码标签 (1033=POD1, 2033=POD2)
 *
 * 算法
 *   - DEPTH: child[0]/child[1] 下标访问
 *   - POD: 遍历查找标签 1033/2033
 *   - 其他: 遍历子节点查user_data 匹配
 *
 * 绝不触发任何重绘或排版重构！只更lv_label 文字
 * ========================================================= */
void arex_widget_set_value(arex_widget_id_t id, float value)
{
    /* 修复 Bug #3：添加数组越界保*/
    uint8_t max_count = (g_card_custom_obj_count < AREX_MAX_CUSTOM_CARDS)
                        ? g_card_custom_obj_count
                        : AREX_MAX_CUSTOM_CARDS;

    for (uint8_t c = 0; c <= max_count; c++)
    {
        lv_obj_t *container = (c < max_count) ? g_card_custom_objs[c] : g_left_anchor_obj;
        if (!container) continue;

        int16_t child_cnt = lv_obj_get_child_cnt(container);
        for (int16_t i = 0; i < child_cnt; i++)
        {
            lv_obj_t *child = lv_obj_get_child(container, i);
            if (!child) continue;

            uintptr_t child_tag = (uintptr_t)lv_obj_get_user_data(child);

            /* DEPTH 专属：需要更新同一容器内的全部同类实例，不能命中第一个就提前退出 */
            if (id == WIDGET_DEPTH_1612 && child_tag == (uintptr_t)id)
            {
                int di = (int)value;
                /* 只保留一位小数，范围 0-9 */
                float decimal_part = fabsf(value - di);
                int dd = (int)(decimal_part * 10 + 0.5f);
                if (dd > 9) dd = 9;  /* 防止浮点精度问题导致多位*/
                lv_obj_t *part0 = lv_obj_get_child(child, 0);
                lv_obj_t *part1 = lv_obj_get_child(child, 1);
                if (part0 && lv_obj_check_type(part0, &lv_label_class))
                {
                    lv_label_set_text_fmt(part0, "%d", di);
                }
                if (part1 && lv_obj_check_type(part1, &lv_label_class))
                {
                    lv_label_set_text_fmt(part1, ".%d", dd);
                }
                continue;
            }

            /* ===== POD 单模具：数据源根pod_index 动态分=====
             * 注意：由于关闭了 ELEM_EXTRA，POD 不再有独立的 ID 标签子元素
             * 数label 通过通用路径创建，其 user_data = WIDGET_POD_0806
             * 因此可以简化逻辑：直接通过 child_tag == WIDGET_POD_0806 匹配即可
             * POD1/POD2 的区分由渲染时的 pod_index 决定，更新时无需区分*/
            if (child_tag == (uintptr_t)WIDGET_POD_0806)
            {
                int16_t sub_cnt = lv_obj_get_child_cnt(child);
                for (int16_t j = 0; j < sub_cnt; j++)
                {
                    lv_obj_t *sub = lv_obj_get_child(child, j);
                    if (!sub) continue;
                    if ((uintptr_t)lv_obj_get_user_data(sub) == (uintptr_t)WIDGET_POD_0806)
                    {
                        if (lv_obj_check_type(sub, &lv_label_class))
                        {
                            char buf[32];
                            snprintf(buf, sizeof(buf), "%.0f", (double)value);
                            lv_label_set_text(sub, buf);
                        }
                        break;
                    }
                }
                continue;
            }

            /* ===== 通用 widget：用 user_data == id 匹配 ===== */
            if (child_tag == (uintptr_t)id)
            {
                int16_t sub_cnt = lv_obj_get_child_cnt(child);
                for (int16_t j = 0; j < sub_cnt; j++)
                {
                    lv_obj_t *sub = lv_obj_get_child(child, j);
                    if (!sub) continue;
                    if ((uintptr_t)lv_obj_get_user_data(sub) == (uintptr_t)id)
                    {
                        if (lv_obj_check_type(sub, &lv_label_class))
                        {
                            char buf[32];
                            if (id == WIDGET_TEMP_0806 || id == WIDGET_DEPTH_1606)
                            {
                                snprintf(buf, sizeof(buf), "%.1f", (double)value);
                            }
                            else if (id == WIDGET_PPO2_0806)
                            {
                                snprintf(buf, sizeof(buf), "%.2f", (double)value);
                            }
                            else if (id == WIDGET_BATTERY_0806)
                            {
                                snprintf(buf, sizeof(buf), "%.0f%%", (double)value);
                            }
                            else if (id == WIDGET_TTS_0806 || id == WIDGET_NDL_STOP_1606)
                            {
                                snprintf(buf, sizeof(buf), "%d", (int)value);
                            }
                            else
                            {
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
 * widget_id 设置字符串（用于 GAS 等非数值组件）
 * ========================================================= */
void arex_widget_set_text(arex_widget_id_t id, const char *text)
{
    if (!text) return;

    /* 遍历所5F 卡片容器 + 左侧锚点 */
    uint8_t max_count = (g_card_custom_obj_count < AREX_MAX_CUSTOM_CARDS)
                        ? g_card_custom_obj_count : AREX_MAX_CUSTOM_CARDS;

    for (uint8_t c = 0; c <= max_count; c++)
    {
        lv_obj_t *container = (c < max_count) ? g_card_custom_objs[c] : g_left_anchor_obj;
        if (!container) continue;

        int16_t child_cnt = lv_obj_get_child_cnt(container);
        for (int16_t i = 0; i < child_cnt; i++)
        {
            lv_obj_t *child = lv_obj_get_child(container, i);
            if (!child) continue;

            if ((arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(child) == id)
            {
                int16_t sub_cnt = lv_obj_get_child_cnt(child);
                for (int16_t j = 0; j < sub_cnt; j++)
                {
                    lv_obj_t *sub = lv_obj_get_child(child, j);
                    if (!sub) continue;
                    if ((arex_widget_id_t)(uintptr_t)lv_obj_get_user_data(sub) == id)
                    {
                        if (lv_obj_check_type(sub, &lv_label_class))
                        {
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
 * 全局组件数据路由分发器
 *
 * 架构设计：此函数作为数据路由总机，根据 widget_id 自动从 g_sensor_data
 * 取值并调用 arex_widget_set_value() 或 arex_widget_set_text() 刷新界面。
 *
 * 使用场景
 *   - arex_screen_refresh_all_widgets() 遍历全量 widget 调用此函数
 *   - update 任务在收到 DIRTY_ALL 时调用全量刷新
 *   - 任何需要单独刷新某个组件数据的场景
 *
 * 注意：复杂状态机组件（NDL_STOP/SYS/COMPASS/TISSUE）已在 update_task
 *       有专属刷新逻辑，此处仅做兜底处理
 * ========================================================= */
void arex_widget_sync_data(arex_widget_id_t w_id)
{
    char buf[32];

    switch (w_id)
    {
    /* =========================================================
     * 1. 核心驻留& 复杂状态机 (这些由专属函数处理，这里做兜
     * ========================================================= */
    case WIDGET_NDL_STOP_1606:
    case WIDGET_COMPASS_1612:
    case WIDGET_TISSUE_GF_4012:
    case WIDGET_TISSUE_RAW_4012:
        /* 这些是包含动多元素的复杂状态机，已arex_ui_update_task 有专属刷新逻辑 */
        break;

    case WIDGET_SYS_1606:
        if (s_sys_batt_lbl)
        {
            lv_label_set_text_fmt(s_sys_batt_lbl, "%u%%", arex_ui_clamp_battery_pct(g_sensor_data.battery_pct));
        }
        if (s_sys_temp_lbl)
        {
            int t_int = (int)g_sensor_data.temperature_c;
            int t_dec = (int)(fabsf(g_sensor_data.temperature_c - t_int) * 10);
            lv_label_set_text_fmt(s_sys_temp_lbl, "%d.%d C", t_int, t_dec);
        }
        break;

    /* =========================================================
     * 2. 深度组件
     * ========================================================= */
    case WIDGET_DEPTH_1612:
    case WIDGET_DEPTH_1606:
        arex_widget_set_value(w_id, g_sensor_data.depth);
        break;

    /* =========================================================
     * 3. 潜水时间（MM:SS 格式化）
     * ========================================================= */
    case WIDGET_DIVE_TIME_1606:
        snprintf(buf, sizeof(buf), "%02d:%02d",
                 g_sensor_data.dive_time_s / 60,
                 g_sensor_data.dive_time_s % 60);
        arex_widget_set_text(w_id, buf);
        break;

    /* =========================================================
     * 4. 气体组件
     * ========================================================= */
    case WIDGET_GAS_1606:
        arex_widget_set_text(w_id, g_sensor_data.gas_name);
        break;

    /* =========================================================
     * 5. 基础组件 (Basic)
     * ========================================================= */
    case WIDGET_TEMP_0806:
        arex_widget_set_value(w_id, g_sensor_data.temperature_c);
        break;

    case WIDGET_TIME_1606:
        snprintf(buf, sizeof(buf), "%02d:%02d",
                 g_sensor_data.sys_time_h,
                 g_sensor_data.sys_time_m);
        arex_widget_set_text(w_id, buf);
        break;

    case WIDGET_TTS_0806:
        arex_widget_set_value(w_id, (float)g_sensor_data.tts);
        break;

    case WIDGET_ASCENT_0806:
    case WIDGET_ASCENT_0812:
        arex_widget_set_value(w_id, g_sensor_data.ascent_rate);
        break;

    case WIDGET_BATTERY_0806:
        arex_widget_set_value(w_id, g_sensor_data.battery_pct);
        break;

    case WIDGET_STOP_DEPTH_0806:
        arex_widget_set_value(w_id, g_sensor_data.stop_depth_m);
        break;

    case WIDGET_STOP_TIME_1606:
        snprintf(buf, sizeof(buf), "%02d:%02d",
                 g_sensor_data.stop_time_left_s / 60,
                 g_sensor_data.stop_time_left_s % 60);
        arex_widget_set_text(w_id, buf);
        break;

    case WIDGET_PPO2_0806:
        /* 根据激活气体索引选择对应PPO2 */
        arex_widget_set_value(w_id, g_sensor_data.ppo2[g_sensor_data.gas_active_idx]);
        break;

    /* =========================================================
     * 6. 技术潜(Tech Dive)
     * ========================================================= */
    case WIDGET_SURF_GF_0806:
        arex_widget_set_value(w_id, g_sensor_data.surf_gf);
        break;

    case WIDGET_GF99_0806:
        arex_widget_set_value(w_id, g_sensor_data.gf99);
        break;

    case WIDGET_GF_0806:
        snprintf(buf, sizeof(buf), "%d/%d",
                 g_sensor_data.gf_low,
                 g_sensor_data.gf_high);
        arex_widget_set_text(w_id, buf);
        break;

    case WIDGET_CNS_0806:
        arex_widget_set_value(w_id, (float)g_sensor_data.cns_pct);
        break;

    case WIDGET_OTU_0806:
        arex_widget_set_value(w_id, (float)g_sensor_data.otu);
        break;

    case WIDGET_MOD_0806:
        arex_widget_set_value(w_id, g_sensor_data.mod_m);
        break;

    case WIDGET_CEILING_0806:
        arex_widget_set_value(w_id, g_sensor_data.ceiling_m);
        break;

    case WIDGET_GAS_MIX_1606:
        snprintf(buf, sizeof(buf), "%d/%d",
                 g_sensor_data.gas_o2_pct,
                 g_sensor_data.gas_he_pct);
        arex_widget_set_text(w_id, buf);
        break;

    case WIDGET_GAS_DENS_0806:
        arex_widget_set_value(w_id, g_sensor_data.gas_density);
        break;

    case WIDGET_FIO2_0806:
        arex_widget_set_value(w_id, g_sensor_data.fio2_pct);
        break;

    /* =========================================================
     * 7. 传感& 拓展 (Sensors)
     * ========================================================= */
    case WIDGET_HEADING_0806:
        arex_widget_set_value(w_id, (float)g_sensor_data.heading);
        break;

    case WIDGET_POD_0806:
        /* POD 由状态机使用 user_data 靶向刷新，此处做兜底 */
        arex_widget_set_value(WIDGET_POD_0806, g_sensor_data.pod1_bar);
        break;

    case WIDGET_DEPTH_MAX_0806:
        arex_widget_set_value(w_id, g_sensor_data.max_depth);
        break;

    case WIDGET_DEPTH_AVG_0806:
        arex_widget_set_value(w_id, g_sensor_data.avg_depth);
        break;

    case WIDGET_TEMP_MIN_0806:
        arex_widget_set_value(w_id, g_sensor_data.min_temp);
        break;

    case WIDGET_TEMP_AVG_0806:
        arex_widget_set_value(w_id, g_sensor_data.avg_temp);
        break;

    /* =========================================================
     * 8. ղλδ֪ ID
     * ========================================================= */
    case WIDGET_EMPTY:
    default:
        break;
    }
}

/* =========================================================
 * 🚨 靶向告警触发引擎（新版本：仅设置状态，50ms 定时器执行闪烁）
 * ========================================================= */
void arex_trigger_alarm(arex_alarm_level_t level,
                        const char *eng_text,
                        arex_widget_id_t target_id)
{
    (void)arex_alarm_raise_custom(level, eng_text, target_id);
    g_ui.alarm_pending_click = (level >= AREX_ALARM_WARN);
}

/* =========================================================
 * 🚨 清除所有告警样式（50ms 定时器会自动把样式复原）
 * 新逻辑：速度降到安全范围后自动清除，但最少显示 5 秒
 * ========================================================= */
void arex_clear_all_alarm_styles(void)
{
    arex_alarm_clear_all();
    g_ui.alarm_pending_click = false;
}

bool arex_alarm_mark_clear_requested(void)
{
    return arex_alarm_ack_current();
}



static void arex_alarm_render_tick(void)
{
    arex_alarm_view_context_t ctx;
    ctx.safe_zone = arex_get_safe_zone();
    ctx.left_anchor = g_left_anchor_obj;
    ctx.custom_cards = g_card_custom_objs;
    ctx.custom_card_count = g_card_custom_obj_count;
    ctx.max_custom_cards = AREX_MAX_CUSTOM_CARDS;
    ctx.layout_order = g_sys_config.layout_order;
    ctx.safe_zone_w = g_sys_config.safe_zone_w;
    ctx.left_anchor_w = AREX_LEFT_ANCHOR_W;
    ctx.panel_gap_px = (uint16_t)(g_sys_config.gap_u * AREX_BASE_U);
    ctx.alarm_pending_click = &g_ui.alarm_pending_click;

    arex_alarm_view_tick(&ctx);
}

/* =========================================================
 * 定时数据更新 (lv_timer 1Hz/2Hz 调用)
 * 仅更lv_label 文字，绝不触发排版重
 * ========================================================= */
void arex_ui_update_data(void)
{
    /* 由调用方arex_screen.c 中实现具体的 lv_label_set_text 调用
     * 此函数作为空钩子存在，供未来扩展
     */
}

/* =========================================================
 * 11. Data Bus UI 消费任务 全系统唯一允许执行 lv_label_set_text 的地
 *
 * 架构铁律
 *   - 硬件工程师：只能调用 arex_bus_set_*() 系列函数（仅写数打脏标记
 *   - UI 工程 ：只能修arex_ui_update_task() 消费
 *   - 两者通过 g_sensor_data.dirty_mask 完全解
 *
 * lv_timer 驱动，建50ms 周期0 FPS 足够覆盖所有传感器变化
 * ========================================================= */
void arex_ui_update_task(lv_timer_t *timer)
{
    (void)timer;

    arex_alarm_render_tick();

    {
        static arex_compass_cal_ui_state_t s_last_compass_cal_state = AREX_COMPASS_CAL_IDLE;
        arex_compass_cal_ui_state_t cal_state = arex_get_compass_calibration_ui_state();
        if (cal_state != s_last_compass_cal_state)
        {
            s_last_compass_cal_state = cal_state;
            arex_screen_refresh_setup_menu();
        }
    }

    /* ============================================================
     * 🚨 核心修复：独立于数据时间心跳引擎"必须放在最前面
     *
     * 即使没有任何脏标记，只要处于运动状|rate|>=3.0 m/min)
     * 或者有活跃告警，我们就强行注入 DIRTY_DEPTH 脏标记，
     * 唤醒 UI 引擎去画闪烁动画
     * ============================================================ */
    {
        static bool last_flash_state = false;
        bool current_flash_state = (lv_tick_get() / 500) % 2 == 0;

        if (current_flash_state != last_flash_state)
        {
            last_flash_state = current_flash_state;

            float rate = g_sensor_data.ascent_rate;
            /* 速度超过静止阈值时，保持心跳刷新速率图标。 */
            if (fabsf(rate) >= AREX_RATE_STILL_THRESHOLD)
            {
                g_sensor_data.dirty_mask |= DIRTY_DEPTH;
            }
        }
    }

    uint32_t mask = arex_bus_take_dirty();
    if (mask == DIRTY_NONE) return;

    /* 最高优先级：UI 布局重建（BLE 配置同步触发）
     * 重建耗时较长，锁LVGL invalidation 防止闪烁，本帧直接退出*/
    if (mask & DIRTY_UI_LAYOUT)
    {
        lv_disp_t *disp = lv_disp_get_default();
        if (disp) lv_disp_enable_invalidation(disp, false);
        arex_screen_rebuild_full();
        if (disp) lv_disp_enable_invalidation(disp, true);
        arex_bus_requeue_dirty(mask & ~DIRTY_UI_LAYOUT);
        return;
    }

    /* 深度 + NDL + TTS + 组织—全屏刷新（包含左侧锚+ 5F 网格 2F Deco 卡片刷新 */
    if (mask & (DIRTY_DEPTH | DIRTY_NDL | DIRTY_TTS | DIRTY_TISSUES))
    {
        arex_screen_refresh_all_widgets();  // 修复：改为全屏刷新，确保 5F 网格中的 widget 也能更新
        card_deco_update();

        /* ============================================================
         * ============================================================
         * 速率图标闪烁引擎（纯500ms 心跳驱动，无视深度抖动）
         * ============================================================ */
        if (s_ascent_icon_count > 0)
        {
            static int8_t s_last_direction = 0;  /* 0=静止, 1=上升, -1=下降 */

            float rate = g_sensor_data.ascent_rate;
            bool is_moving = (fabsf(rate) >= AREX_RATE_STILL_THRESHOLD);

            /* 获取 500ms 心跳相位 */
            bool current_flash_state = (lv_tick_get() / 500) % 2 == 0;

            /* 判断当前实际运动方向 */
            int8_t current_direction = 0;
            if (rate > 0.0f)
            {
                current_direction = 1;
            }
            else if (rate < 0.0f)
            {
                current_direction = -1;
            }

            const void *target_img_src = &sudo_up_level0;

            if (!is_moving)
            {
                /* 静止状态：不闪烁，保持最后一个方向的 level0 */
                target_img_src = (s_last_direction > 0) ? &sudo_up_level0 :
                                 (s_last_direction < 0) ? &sudo_down_level0 : &sudo_up_level0;
            }
            else
            {
                /* 运动状态：根据 500ms 节拍进行呼吸闪烁 */
                if (current_direction > 0)
                {
                    /* 上升方向 */
                    if (rate >= AREX_RATE_LEVEL2_THRESHOLD)
                    {
                        target_img_src = current_flash_state ? &sudo_up_level2 : &sudo_up_level0;
                    }
                    else if (rate >= AREX_RATE_LEVEL1_THRESHOLD)
                    {
                        target_img_src = current_flash_state ? &sudo_up_level1 : &sudo_up_level0;
                    }
                    else
                    {
                        target_img_src = &sudo_up_level0;
                    }
                }
                else
                {
                    /* 下降方向 */
                    if (rate <= -AREX_RATE_LEVEL2_THRESHOLD)
                    {
                        target_img_src = current_flash_state ? &sudo_down_level2 : &sudo_down_level0;
                    }
                    else if (rate <= -AREX_RATE_LEVEL1_THRESHOLD)
                    {
                        target_img_src = current_flash_state ? &sudo_down_level1 : &sudo_down_level0;
                    }
                    else
                    {
                        target_img_src = &sudo_down_level0;
                    }
                }
            }

            /* 更新最后的方向状*/
            if (current_direction != 0)
            {
                s_last_direction = current_direction;
            }

            /* 同步刷新所有图*/
            for (int i = 0; i < s_ascent_icon_count; i++)
            {
                if (s_img_ascent_rate[i] != NULL)
                {
                    lv_img_set_src(s_img_ascent_rate[i], target_img_src);
                }
            }
        }
    }

    /* ============================================================
     * NDL_STOP 多形态状态机：NDL常/ Safety停留 / Deco停留
     * 根据 g_sensor_data.stop_type 瞬间切换所有子组件的显隐、位置和字号
     * 遍历数组，同步刷新所NDL 实例（左侧锚+ 5F 多个
     * ============================================================ */
    if (s_ndl_handle_count > 0 && (mask & (DIRTY_NDL_STOP | DIRTY_DEPTH | DIRTY_NDL)))
    {
        /* 实时查表获取样式字典 */
        const arex_widget_style_t *style = arex_get_widget_style(WIDGET_NDL_STOP_1606);
        if (!style) return;

        const arex_style_ndl_stop_t *s = &style->spec.ndl_stop;

        for (int i = 0; i < s_ndl_handle_count; i++)
        {
            ndl_handle_t *h = &s_ndl_handles[i];

            /* 无论何种状态，十宫格永远常驻显*/
            lv_obj_clear_flag(h->horiz_bg, LV_OBJ_FLAG_HIDDEN);
            /* 只要数据变了，就触发十宫格重新执行绘制计*/
            lv_obj_invalidate(h->horiz_bg);

            /* ========== 状1: 常NDL 模式 ========== */
            if (g_sensor_data.stop_type == AREX_STOP_NONE)
            {
                lv_obj_add_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);  /* 顶部隐藏 */
                lv_obj_clear_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN);

                /* NDL 字样居左 */
                lv_label_set_text(h->sub_bot, "NDL");
                lv_obj_align(h->sub_bot, LV_ALIGN_LEFT_MID, 8, -6);

                /* NDL专用字体 48px 数字居右 */
                lv_obj_set_style_text_font(h->main_val, arex_get_font(AREX_FONT_ID_NDL), 0);
                lv_label_set_text_fmt(h->main_val, "%d", g_sensor_data.ndl);
                lv_obj_align(h->main_val, LV_ALIGN_CENTER, 0, -8);
            }
            /* ========== 状2: 安全停留模式 ========== */
            else if (g_sensor_data.stop_type == AREX_STOP_SAFETY)
            {
                lv_obj_clear_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN);

                lv_label_set_text_fmt(h->title_top, "SAFE %dm", (int)g_sensor_data.stop_depth_m);
                lv_obj_align(h->title_top, LV_ALIGN_TOP_LEFT, 8, 2);

                if (g_sensor_data.in_stop_zone)
                {
                    lv_label_set_text(h->sub_bot, "IN STOP");
                }
                else
                {
                    lv_label_set_text_fmt(h->sub_bot, "NDL %d", g_sensor_data.ndl);
                }
                lv_obj_align(h->sub_bot, LV_ALIGN_BOTTOM_LEFT, 8, -16); /* 悬停10 宫格上方 */

                /* 大字64px 数字居右 */
                int m = g_sensor_data.stop_time_left_s / 60;
                int s = g_sensor_data.stop_time_left_s % 60;
                lv_obj_set_style_text_font(h->main_val, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
                lv_label_set_text_fmt(h->main_val, "%d:%02d", m, s);
                lv_obj_align(h->main_val, LV_ALIGN_RIGHT_MID, -4, -6);
            }
            /* ========== 状3: 减压停留模式 ========== */
            else if (g_sensor_data.stop_type == AREX_STOP_DECO)
            {
                lv_obj_clear_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN); /* DECO 下隐NDL 副标*/

                lv_label_set_text_fmt(h->title_top, "DECO %dm", (int)g_sensor_data.stop_depth_m);
                lv_obj_align(h->title_top, LV_ALIGN_TOP_LEFT, 8, 2);

                /* 大字64px 数字居右 */
                int m = g_sensor_data.stop_time_left_s / 60;
                int s = g_sensor_data.stop_time_left_s % 60;
                lv_obj_set_style_text_font(h->main_val, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
                lv_label_set_text_fmt(h->main_val, "%d:%02d", m, s);
                lv_obj_align(h->main_val, LV_ALIGN_RIGHT_MID, -4, -6);
            }
        }
    }

    /* 气瓶压力 —全屏刷新，确5F 网格中的 POD 同步更新 */
    if (mask & DIRTY_POD)
    {
        arex_screen_refresh_all_widgets();
    }

    /* 电池刷新 —数据驱动网格自动更新 */
    if (mask & DIRTY_BATT)
    {
        arex_widget_set_value(WIDGET_BATTERY_0806, g_sensor_data.battery_pct);
    }

    /* 罗盘航向 零内存数学引擎，触发 invalidate + 更新标签 */
    if (mask & DIRTY_HEADING)
    {
#if BLE_COMPASS_DIAG_LOG_ENABLED
        {
            static uint32_t s_last_compass_ui_log_tick = 0;
            static uint16_t s_last_compass_ui_heading = 0xFFFFU;
            uint32_t now_tick = lv_tick_get();
            bool heading_changed = (s_last_compass_ui_heading != g_sensor_data.heading);
            bool heartbeat_due =
            (s_last_compass_ui_log_tick == 0U) ||
            ((now_tick - s_last_compass_ui_log_tick) >= 2000U);

            if (heading_changed || heartbeat_due)
            {
                s_last_compass_ui_log_tick = now_tick;
                s_last_compass_ui_heading = g_sensor_data.heading;
                ble_sensor_debug_note_ui_dirty(g_sensor_data.heading);
#if BLE_COMPASS_DIAG_SYSTEM_LOG_ENABLED
                rt_kprintf("[COMPASS_UI] dirty heading=%u label=%d tape=%d card=%u dash=%u\r\n",
                           g_sensor_data.heading,
                           s_heading_val_lbl ? 1 : 0,
                           s_compass_tape_obj ? 1 : 0,
                           g_sys_config.card_order[g_ui.dash_card],
                           g_ui.dash_card);
#endif
            }
        }
#endif
        /* 更新卷尺下方的巨型文*/
        if (s_heading_val_lbl)
        {
            lv_label_set_text_fmt(s_heading_val_lbl, "%03d", g_sensor_data.heading);
        }
        /* 触发卷尺画板的底层数学重绘（极其轻量*/
        if (s_compass_tape_obj)
        {
            lv_obj_invalidate(s_compass_tape_obj);
        }
        /* 如果有锁定，更新提示文本 */
        if (s_heading_hint_lbl)
        {
            if (g_sensor_data.heading_locked)
            {
                lv_label_set_text_fmt(s_heading_hint_lbl, "[ TARGET LOCKED: %03d掳 ]", g_sensor_data.heading_target);
            }
            else
            {
                lv_label_set_text(s_heading_hint_lbl, "[ ENTER ] mark heading");
            }
        }
    }

    /* 潜水时间 + W.TIME —全屏刷新，确5F 网格中的时间组件同步更新 */
    if (mask & DIRTY_DIVE_TIME)
    {
        arex_screen_refresh_all_widgets();
    }

    /* PO2 —全屏刷新，确5F 网格中的 PPO2 组件同步更新 */
    if (mask & DIRTY_PPO2)
    {
        arex_screen_refresh_all_widgets();
    }

    /* 气体切换 */
    if (mask & DIRTY_GAS)
    {
        arex_screen_refresh_gas_menu();
        arex_screen_refresh_all_widgets();
    }

    /* 潜水轨迹+减压站图表刷
     * 轨迹追加由调用方sim_tick_cb 中直接调arex_dive_log_append
     * 此处仅负责刷新图表（AREX_DECO_REFRESH_MS 节流保护*/
    if (mask & DIRTY_TRAJECTORY)
    {
        uint32_t now = lv_tick_get();
#if AREX_DECO_REFRESH_MS > 0
        if (now - _deco_last_refresh_ms >= AREX_DECO_REFRESH_MS)
        {
            _deco_last_refresh_ms = now;
            card_plan_update();
        }
#else
        (void)_deco_last_refresh_ms;
        card_plan_update();
#endif
    }

    /* CNS 氧中—2F Deco 卡片 + 5F 网格 */
    if (mask & DIRTY_CNS)
    {
        card_deco_update();
        arex_screen_refresh_all_widgets();
    }

    /* OTU 氧中—2F Deco 卡片 + 5F 网格 */
    if (mask & DIRTY_OTU)
    {
        card_deco_update();
        arex_screen_refresh_all_widgets();
    }

    /* 温度刷新 —数据驱动网格自动更新 */
    if (mask & DIRTY_TEMP)
    {
        arex_widget_set_value(WIDGET_TEMP_0806, g_sensor_data.temperature_c);
    }

    /* 深度/温度统计刷新 —最平均/最低随主数据同步更*/
    if (mask & DIRTY_DEPTH)
    {
        arex_widget_set_value(WIDGET_DEPTH_MAX_0806, g_sensor_data.max_depth);
        arex_widget_set_value(WIDGET_DEPTH_AVG_0806, g_sensor_data.avg_depth);
    }
    if (mask & DIRTY_TEMP)
    {
        arex_widget_set_value(WIDGET_TEMP_MIN_0806, g_sensor_data.min_temp);
        arex_widget_set_value(WIDGET_TEMP_AVG_0806, g_sensor_data.avg_temp);
    }

    /* 技术潜水参数刷—全屏刷新（包含左侧锚+ 5F 网格*/
    if (mask & (DIRTY_GF_SETTING | DIRTY_MOD | DIRTY_CEILING | DIRTY_GAS_MIX | DIRTY_GAS_DENS | DIRTY_FIO2))
    {
        arex_screen_refresh_all_widgets();
    }

    /* ============================================================
     * O(1) SYS_1606 全模块极速点对点刷新
     * 直接操作静态指针，绝不遍历 UI 树！
     * ============================================================ */
    if (mask & (DIRTY_BATT | DIRTY_TEMP))
    {
        /* 1. 电量百分*/
        if (mask & DIRTY_BATT)
        {
            if (s_sys_batt_lbl)
            {
                lv_label_set_text_fmt(s_sys_batt_lbl, "%u%%", arex_ui_clamp_battery_pct(g_sensor_data.battery_pct));
            }
        }
        /* 2. 温度 */
        if (mask & DIRTY_TEMP)
        {
            if (s_sys_temp_lbl)
            {
                /* 整数拼接法绕%f 限制，完美显26.5 C */
                int t_int = (int)g_sensor_data.temperature_c;
                int t_dec = (int)(fabsf(g_sensor_data.temperature_c - t_int) * 10);
                lv_label_set_text_fmt(s_sys_temp_lbl, "%d.%d C", t_int, t_dec);
            }
        }
        /* 设备状态图标刷新代码已移除（图标已删除*/
    }

    /* ============================================================
     * Alarm event consumption. Data bus writers raise DIRTY_ALARM.
     * ============================================================ */
    if (mask & DIRTY_ALARM)
    {
        arex_alarm_render_tick();
    }

}

/* =========================================================
 * 12. 左侧 2x6 绝对网格渲染引擎
 *
 * 严格160x360 区域划分280px) x 660px) 的绝对网格矩阵，
 * 彻底废弃 current_y 累加排版，改x*y*w*h 纯数学坐标推演
 * SystemData 底部 60px WIDGET_SYS_1606 组件化渲染
 * ========================================================= */

/* 左侧网格总线渲染器：遍历 g_sys_config.left_widgets[] 数组
 * 用纯数学 cell_w * cell_h 推算绝对坐标并渲染所有组件
 * left_anchor 传入用于告警引擎跨区搜索烙印对象*/
void arex_render_left_anchor_grid(lv_obj_t *left_anchor)
{
    if (!left_anchor) return;

    /* 注入外部容器（供告警引擎跨区搜索烙印对象*/
    g_left_anchor_obj = left_anchor;

    /* 注意：不单独清空 s_img_ascent_rate[] / s_ndl_handles[]
     * 它们已经arex_screen_rebuild_layout() 入口统一清空了
     * 这里只需要追加左侧锚点的 widget 指针即可（追加模式）*/

    /* 基准网格单元x 6行，每格 80x60 */
    const uint16_t cell_w = AREX_LEFT_CELL_W;   /* 80px */
    const uint16_t cell_h = AREX_LEFT_CELL_H;   /* 60px */

    /* 遍历并渲染基于网格的组件 */
    for (uint8_t i = 0; i < g_sys_config.left_widget_count && i < AREX_LEFT_MAX_WIDGETS; i++)
    {
        arex_grid_widget_t *cfg = &g_sys_config.left_widgets[i];
        if (cfg->widget_id == WIDGET_EMPTY) continue;

        /* 从样式表查表获取跨度信息 */
        const arex_widget_style_t *style = arex_get_widget_style(cfg->widget_id);
        uint8_t span_w = (style != NULL) ? style->span_w : 1;
        uint8_t span_h = (style != NULL) ? style->span_h : 1;

        /* 绝对物理坐标推演：col * cell_w, row * cell_h */
        int16_t  abs_x = (int16_t)(cfg->x * cell_w);
        int16_t  abs_y = (int16_t)(cfg->y * cell_h);
        uint16_t abs_w = span_w * cell_w;
        uint16_t abs_h = span_h * cell_h;

        /* 调用底层工厂：速率图标由工厂自主查字典决定 */
        render_widget_by_id(left_anchor, cfg->widget_id,
                            abs_x, abs_y, abs_w, abs_h,
                            span_w, span_h, (arex_font_id_t)255);
    }

    /* 左侧横线按组件边界绘制：只在真实的上下两个组件之间画*/
    for (uint8_t row = 1; row < AREX_LEFT_ROWS; row++)
    {
        uint8_t seg_start = 0xFF;

        for (uint8_t col = 0; col < AREX_LEFT_COLS; col++)
        {
            arex_grid_widget_t *top_cfg = arex_left_find_widget_at_cell(col, (uint8_t)(row - 1));
            arex_grid_widget_t *bottom_cfg = arex_left_find_widget_at_cell(col, row);
            bool draw_seg = (top_cfg != NULL && bottom_cfg != NULL && top_cfg != bottom_cfg);

            if (draw_seg)
            {
                if (seg_start == 0xFF)
                {
                    seg_start = col;
                }
            }
            else if (seg_start != 0xFF)
            {
                arex_add_left_anchor_sep_line(left_anchor,
                                              (lv_coord_t)(seg_start * cell_w),
                                              (lv_coord_t)(row * cell_h),
                                              (lv_coord_t)((col - seg_start) * cell_w));
                seg_start = 0xFF;
            }
        }

        if (seg_start != 0xFF)
        {
            arex_add_left_anchor_sep_line(left_anchor,
                                          (lv_coord_t)(seg_start * cell_w),
                                          (lv_coord_t)(row * cell_h),
                                          (lv_coord_t)((AREX_LEFT_COLS - seg_start) * cell_w));
        }
    }
}

/* =========================================================
 * 第五步：新简化工厂函数（APP下发位置 + MCU本地查样式表
 *
 * 架构铁律：APP 只下[widget_id, x, y]，MCU 根据 widget_id
 * 自动从样式注册表获取 w/h/offset，渲染时组合两者
 * ========================================================= */
lv_obj_t* arex_render_widget(lv_obj_t *parent,
                             const arex_widget_pos_t *pos,
                             uint16_t cell_w, uint16_t cell_h,
                             uint16_t title_h)
{
    if (!parent || !pos) return NULL;
    if (pos->widget_id == WIDGET_EMPTY) return NULL;

    /* 1. 查本地样式表 */
    const arex_widget_style_t *style = arex_get_widget_style(pos->widget_id);
    if (!style)
    {
        /* 容错：未知ID，尝试用通用方式渲染 */
        lv_obj_t *comp = lv_obj_create(parent);
        lv_obj_remove_style_all(comp);
        int16_t ax = (int16_t)(pos->x * cell_w);
        int16_t ay = (int16_t)(pos->y * cell_h) + title_h;
        lv_obj_set_pos(comp, ax, ay);
        lv_obj_set_size(comp, cell_w, cell_h);
        return comp;
    }

    /* 2. 推算绝对物理坐标 */
    int16_t  abs_x = (int16_t)(pos->x * cell_w);
    int16_t  abs_y = (int16_t)(pos->y * cell_h) + title_h;
    uint16_t abs_w = (uint16_t)(style->span_w * cell_w);
    uint16_t abs_h = (uint16_t)(style->span_h * cell_h);

    /* 3. 直接调用底层工厂（速率图标由工厂自主查字典决定*/
    return render_widget_by_id(parent, pos->widget_id,
                               abs_x, abs_y, abs_w, abs_h,
                               style->span_w, style->span_h,
                               (arex_font_id_t)255);
}
