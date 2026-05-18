#include "arex_ui_engine.h"
#include "arex_card_registry.h"
#include "arex_screen.h"
#include "arex_ui_state.h"
#include "fonts/arex_fonts.h"
#include "arex_data.h"
#include "../../ble/ble_sensor_debug_config.h"
#include "../../ble/ble_sensor_debug_data.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

extern void rt_kprintf(const char *fmt, ...);

/* ============================================================
 * 閫熺巼鎸囩ず鍣ㄥ浘鐗囪祫婧愶紙6绾у姩鎬佺澶达級
 * ============================================================ */
LV_IMG_DECLARE(sudo_up_level0);
LV_IMG_DECLARE(sudo_up_level1);
LV_IMG_DECLARE(sudo_up_level2);
LV_IMG_DECLARE(sudo_down_level0);
LV_IMG_DECLARE(sudo_down_level1);
LV_IMG_DECLARE(sudo_down_level2);

/* ============================================================
 * 閫熺巼鍥炬爣鎸囬拡闃靛垪锛堟敮鎸佸涓?DEPTH 妯″潡鍚屾椂瀛樺湪锛?
 * 鏈€澶氭敮鎸佸睆骞曚笂鍑虹幇 MAX_ASCENT_ICONS 涓繁搴︽ā鍧?
 * (宸︿晶閿氱偣 1 涓?+ 5F 鑷畾涔夌綉鏍煎涓?
 * ============================================================ */

/* ============================================================
 * NDL_STOP 澶氬舰鎬佺粍浠跺彞鏌勶紙160x60 鏋侀檺绌洪棿鍐呯殑"鍙樺舰閲戝垰"锛?
 * 鏀寔灞忓箷涓婂涓?NDL 妯″潡锛堝乏渚ч敋鐐?1 涓?+ 5F 澶氫釜锛?
 * 涓夌鐘舵€? NDL甯告€?/ Safety鍋滅暀 / Deco鍋滅暀
 * ============================================================ */
lv_obj_t *s_img_ascent_rate[MAX_ASCENT_ICONS];
uint8_t  s_ascent_icon_count = 0;
ndl_handle_t s_ndl_handles[MAX_NDL_ICONS];
uint8_t      s_ndl_handle_count = 0;

/* 鍛婅鏄剧ず璁℃椂鍣細鎺у埗鍛婅鏈€灏戞樉绀?5 绉?*/
static uint32_t s_alarm_start_tick = 0;
#define ALARM_MIN_DISPLAY_MS  5000   /* 鍛婅鏈€灏戞樉绀?5 绉?*/

/* 鍛婅娲昏穬鏍囪锛氳Е鍙戝憡璀﹀悗鎸佺画闂儊锛岀洿鍒伴€熷害闄嶅埌瀹夊叏鑼冨洿 */
static bool s_alarm_active = false;
/* 鐢ㄦ埛宸茬‘璁ゆ竻闄わ紝绛夊緟婊¤冻鏈€鐭樉绀烘椂闂村悗鑷姩娑堝け */
static bool s_alarm_clear_armed = false;
/* 淇濆瓨娓呴櫎鍓嶇殑鐩爣 ID锛岀敤浜庢仮澶嶆椂绮剧‘瀹氫綅 */
static arex_widget_id_t s_last_alarm_target = WIDGET_EMPTY;

/* ============================================================
 * 缃楃洏鍗＄墖闈欐€佸彞鏌勶紙鐢?card_compass.c 鎸佹湁锛?
 * 鐢ㄤ簬 arex_ui_update_task 涓殑闆跺唴瀛樺紩鎿庡埛鏂?
 * ============================================================ */
extern lv_obj_t *s_compass_tape_obj;
extern lv_obj_t *s_heading_val_lbl;
extern lv_obj_t *s_heading_hint_lbl;

/* 鍑忓帇璺熻釜鑺傛祦鏃堕棿鎴筹紙鐢?arex_ui_update_task 浣跨敤锛?*/
static uint32_t _deco_last_refresh_ms = 0;

/* 姘斾綋鍚嶇О琛?(渚涘叏灞€寮曠敤) */
const char *AREX_GAS_NAMES[AREX_GAS_COUNT] =
{
    "AIR",
    "NX 32",
    "TX 18/45",
    "O2 100%"
};

/* 姘斾綋 MOD 琛?(鍗曚綅: 绫? */
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
 * 鍏ㄥ眬鍗曚緥瀹氫箟
 * ========================================================= */
arex_sys_config_t  g_sys_config;
arex_sensor_data_t g_sensor_data;  //娉ㄦ剰杩欎釜鏄叏灞€鍙橀噺锛屾墍鏈塙I灞傞兘瑕佺敤瀹冦€傚洜涓鸿祴鍊兼槸鍘熷瓙鎿嶄綔锛屽彲浠ユ斁蹇冨ぇ鑳嗙敤锛堜笉闇€瑕佸姞閿侊級

/*褰撲綘鍐欎笅 g_sensor_data.depth = 15.5f; 鏃讹紝缂栬瘧鍣ㄤ細鍦ㄥ簳灞傛妸瀹冪炕璇戞垚锛?
鎵惧埌 g_sensor_data 鐨勫熀鍦板潃 (0x20000000)銆?
鍔犱笂 depth 鐨勫亸绉婚噺 (+0)銆?
鐩存帴鐢熸垚涓€鏉″崟姝ユ眹缂栨寚浠わ紙濡?STR锛夛紝鎶?15.5 鐨勪簩杩涘埗鏁版嵁锛屽儚鐙欏嚮鏋竴鏍凤紝绮惧噯鍦版墦鍏?0x20000000 寮€濮嬬殑 4 涓瓧鑺傞噷锛?
瀹冩牴鏈笉浼氱 heading锛屼篃涓嶄細纰?battery銆?杩欏氨鏄竴娆＄函绮圭殑銆侀拡瀵瑰崟涓?32 浣嶅湴鍧€鐨勨€滃崟鎸囦护鍐欏叆鈥濄€傚洜姝わ紝瀹冩槸缁濆鍘熷瓙鐨勩€?
*/

static const arex_widget_style_t g_widget_styles[] =
{
    /* =========================================================
     * 绗洓姝ワ細MCU 鏈湴鍙 CSS 鏍峰紡娉ㄥ唽琛?
     *
     * 鏋舵瀯閾佸緥锛歎I 宸ョ▼甯堣皟鏁村唴閮ㄥ儚绱犱綅绉诲彧闇€鍦ㄨ繖閲屾敼鏁板瓧锛岀紪璇戝嵆鐢熸晥銆?
     * 瀹屽叏涓嶉渶瑕佹敼 APP锛屼篃涓嶉渶瑕佹敼 BLE 鍗忚銆?
     * ========================================================= */
    /* ========== 鏍稿績椹荤暀缁勪欢 ========== */
    {
        .widget_id = WIDGET_DEPTH_1612,
        .span_w = 2, .span_h = 2,
        .elements = ELEM_VALUE | ELEM_UNIT | ELEM_BAR,  /* 2x2 澶ч€氭爮锛氭棤 title锛岄潬 spec.depth 鍋?int/dec/unit 鍒嗙 */
        .font_id = AREX_FONT_ID_HUGE,           //鏁存暟瀛椾綋
        .title_font_id = AREX_FONT_ID_MEDIUM,    //灏忔暟瀛椾綋
        .unit = "m",
        .title = "DEPTH",
        .title_offset_x = 8, .title_offset_y = 4, .title_align = LV_TEXT_ALIGN_LEFT,//瀹為檯鏃犵敤
        .spec.depth = {
            // 鏍稿績淇锛氬皢澶ф暣鏁扮殑閿氱偣璁句负 RIGHT_MID (闈犲彸瀵归綈)锛?
            // offset_x = -45锛屾剰鍛崇潃鍙宠竟缂樼剨姝诲湪璺濈鍙宠竟鐣?45px 鐨勪綅缃?(缁欏彸杈圭殑绠ご鐣欏嚭绌洪棿)
            .int_offset_x = -80, .int_offset_y = 10, .int_align = LV_ALIGN_RIGHT_MID,
            // 灏忔暟鐐规寕鍦ㄦ暣鏁扮殑澶栭儴鍙充笅瑙掞紝Y杞村井寰線涓婃彁涓€鐐?(-6) 璁╁熀绾垮榻?
            .dec_offset_x = 2,  .dec_offset_y = -30,
            // 鍗曚綅鎸傚湪灏忔暟鐨勬涓嬫柟
            .unit_offset_x = 0, .unit_offset_y = 2,
            .icon_offset_x = -10, .icon_offset_y = 0, .icon_align = LV_ALIGN_RIGHT_MID
        }
    },
    {
        .widget_id = WIDGET_DEPTH_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "DEPTH",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_TEXT_ALIGN_CENTER,
        .spec.depth = {
            .int_offset_x = 0, .int_offset_y = 4, .int_align = LV_ALIGN_BOTTOM_MID,
            .dec_offset_x = 2,  .dec_offset_y = 3,
            .unit_offset_x = 0, .unit_offset_y = 1,
            .icon_offset_x = -6, .icon_offset_y = 0, .icon_align = LV_ALIGN_RIGHT_MID
        }
    },
    {
        .widget_id = WIDGET_NDL_STOP_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT | ELEM_BAR,
        .font_id = AREX_FONT_ID_NDL,  /* 48px NDL鍑忓帇鏃堕棿 */
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "min",
        .title = "NDL",
        .title_offset_x = 0, .title_offset_y = 0, .title_align = LV_TEXT_ALIGN_LEFT,
        .spec.ndl_stop = {
            /* 鍩虹 Bar 璁剧疆 */
            .vert_offset_x = 10, .vert_offset_y = 0, .vert_align = LV_ALIGN_LEFT_MID,
            .vert_w = 14, .vert_h = 40,
            .horiz_offset_x = 0, .horiz_offset_y = -4, .horiz_w = 140, .horiz_h = 6,
            /* 甯告€?(Normal) 鎺掔増锛氬乏瀵归綈锛岃 NDL 鍦ㄥ乏涓婅 */
            .norm_main_x = 0, .norm_main_y = 0,  .norm_main_align = LV_ALIGN_LEFT_MID,
            .norm_sub_x  = 0, .norm_sub_y  = -5, .norm_sub_align  = LV_ALIGN_BOTTOM_LEFT,
            /* 鍋滅暀鎬?(Stop) 鎺掔増 */
            .deco_title_x = 0,  .deco_title_y = 4,   .deco_title_align = LV_ALIGN_TOP_LEFT,
            .deco_main_x  = 0, .deco_main_y  = -6,  .deco_main_align  = LV_ALIGN_LEFT_MID,
            .deco_sub_x   = 0,  .deco_sub_y   = -14,.deco_sub_align   = LV_ALIGN_BOTTOM_LEFT
        }
    },
    {
        .widget_id = WIDGET_DIVE_TIME_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_BAR,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "DIVE",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_GAS_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "GAS",
        .title_offset_x = 10, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -10, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_SYS_1606,
        .span_w = 2, .span_h = 1,
        .elements = 0,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "SYS",
        .title_offset_x = 0, .title_offset_y = 0, .title_align = LV_TEXT_ALIGN_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = 0, .value_align = LV_ALIGN_CENTER }
    },
    /* ========== 鍩虹缁勪欢 ========== */
    {
        .widget_id = WIDGET_TEMP_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "",
        .title = "TEMP",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_TIME_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "TIME",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_TTS_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "",
        .title = "TTS",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_ASCENT_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m/m",
        .title = "RATE",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_ASCENT_0812,
        .span_w = 1, .span_h = 2,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT | ELEM_BAR,  /* 1x2 甯?sudu 閫熺巼鍥炬爣 */
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m/m",
        .title = "RATE",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_COMPASS_1612,
        .span_w = 2, .span_h = 2,
        .elements = ELEM_VALUE | ELEM_BAR,  /* 缃楃洏锛氭棤 title锛岄潬 spec.compass 鍋?tape/val 鍒嗙 */
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "HDG",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_TEXT_ALIGN_CENTER,
        .spec.compass = {
            .tape_offset_x = 0, .tape_offset_y = 20, .tape_align = LV_ALIGN_TOP_MID,
            .val_offset_x = 0, .val_offset_y = -4, .val_align = LV_ALIGN_BOTTOM_MID
        }
    },
    {
        .widget_id = WIDGET_BATTERY_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "",   /* 鍗曚綅宸插湪鍒锋柊浠ｇ爜涓‖缂栫爜 */
        .title = "BATT",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_STOP_DEPTH_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "STOP",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_STOP_TIME_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "min",
        .title = "STIME",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_PPO2_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "PPO2",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    /* ========== 鎶€鏈綔姘寸粍浠?========== */
    {
        .widget_id = WIDGET_SURF_GF_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "SURF.GF",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_GF99_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "%",
        .title = "GF99",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_CNS_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "%",
        .title = "CNS",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_OTU_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "OTU",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_GF_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "GF",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_MOD_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "MOD",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_CEILING_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "CEIL",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_GAS_MIX_1606,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "O2/He",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_TISSUE_GF_4012,
        .span_w = 4, .span_h = 2,
        .elements = ELEM_TITLE | ELEM_BAR,  /* 4x2 澶у浘锛歵itle(Med) + tissue 鏌辩姸鍥撅紝chart 鐢?spec.tissue 椹卞姩 */
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_MEDIUM,
        .unit = NULL,
        .title = "TISSUE(GF)",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.tissue = {
            .chart_offset_x = 0, .chart_offset_y = 20, .chart_align = LV_ALIGN_BOTTOM_RIGHT,
            .bar_count = 16, .bar_spacing = 2
        }
    },
    {
        .widget_id = WIDGET_TISSUE_RAW_4012,
        .span_w = 4, .span_h = 2,
        .elements = ELEM_TITLE | ELEM_BAR,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_MEDIUM,
        .unit = NULL,
        .title = "TISSUE(RAW)",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.tissue = {
            .chart_offset_x = 0, .chart_offset_y = 20, .chart_align = LV_ALIGN_BOTTOM_RIGHT,
            .bar_count = 16, .bar_spacing = 2
        }
    },
    {
        .widget_id = WIDGET_GAS_DENS_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "g/L",
        .title = "DENS",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_FIO2_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "%",
        .title = "FIO2",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    /* ========== 浼犳劅鍣ㄧ粍浠?========== */
    {
        .widget_id = WIDGET_HEADING_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "HDG",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_POD_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,  /* ELEM_EXTRA 鈫?POD1/POD2 涓撳睘 ID 鏍囩 */
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "",
        .title = "POD",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = -2, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_DEPTH_MAX_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "MAX D",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_DEPTH_AVG_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "m",
        .title = "AVG D",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_TEMP_MIN_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "C",
        .title = "MIN T",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_TEMP_AVG_0806,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "C",
        .title = "AVG T",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_LEFT,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_RIGHT }
    },
    {
        .widget_id = WIDGET_EMPTY,
        .span_w = 2, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "TIME",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_EMPTY,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = "PPO2 MAX",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_EMPTY,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "min",
        .title = "NDL MIN",
        .title_offset_x = 0, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_EMPTY,
        .span_w = 1, .span_h = 1,
        .elements = ELEM_TITLE | ELEM_VALUE | ELEM_UNIT,
        .font_id = AREX_FONT_ID_MEDIUM,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = "l/m",
        .title = "SAC MAX",
        .title_offset_x = 4, .title_offset_y = 4, .title_align = LV_ALIGN_TOP_MID,
        .spec.basic = { .value_offset_x = 4, .value_offset_y = -4, .value_align = LV_ALIGN_BOTTOM_MID }
    },
    {
        .widget_id = WIDGET_EMPTY,
        .span_w = 1, .span_h = 1,
        .elements = 0,
        .font_id = AREX_FONT_ID_SMALL,
        .title_font_id = AREX_FONT_ID_SMALL,
        .unit = NULL,
        .title = NULL,
        .title_offset_x = 0, .title_offset_y = 0, .title_align = LV_TEXT_ALIGN_CENTER,
        .spec.basic = { .value_offset_x = 0, .value_offset_y = 0, .value_align = LV_ALIGN_CENTER }
    },
};

#define STYLE_COUNT (int)(sizeof(g_widget_styles) / sizeof(g_widget_styles[0]))

/* 鏌ヨ〃鍑芥暟 */
const arex_widget_style_t* arex_get_widget_style(arex_widget_id_t id)
{
    for (int i = 0; i < STYLE_COUNT; i++)
    {
        if (g_widget_styles[i].widget_id == id)
            return &g_widget_styles[i];
    }
    return NULL;
}

/* =========================================================
 * POD 鍗曟ā鍏疯疆杞垎閰嶇姸鎬佹満
 *
 * 鏋舵瀯锛歐IDGET_POD_0806 (33) 鏄叏灞€鍞竴鐪熷疄瀛樺湪鐨勬皵鐡舵ā鍏枫€?
 * APP 涓嬪彂鍚屼竴涓?POD_0806 鍙互鍑虹幇澶氭锛堝宸︿晶閿氱偣鐨?POD1+POD2锛屾垨 5F 涓殑澶氫釜锛夈€?
 * MCU 閫氳繃娓叉煋璁℃暟鍣?s_pod_render_count 鑷姩鍒嗛厤韬唤銆?
 *
 * 娓叉煋鏃舵嫤鎴?WIDGET_POD_0806锛屾牴鎹鏁板櫒鍒ゆ柇锛?
 *   - 绗?娆￠亣鍒?(count=1, 濂囨暟) 鈫?鍒嗛厤涓?POD1
 *   - 绗?娆￠亣鍒?(count=2, 鍋舵暟) 鈫?鍒嗛厤涓?POD2
 *
 * user_data 鐑欏嵃浣跨敤楂樹綅鎺╃爜鍖哄垎锛?
 *   - POD1: 1000 + WIDGET_POD_0806 = 1033
 *   - POD2: 2000 + WIDGET_POD_0806 = 2033
 * ========================================================= */
static uint8_t s_pod_render_count = 0;  /* POD 娓叉煋璁℃暟鍣?*/

#define POD_TAG_BASE  1000  /* POD 鏍囩鍩哄噯鍋忕Щ */
#define POD1_TAG      (POD_TAG_BASE + WIDGET_POD_0806)  /* 1033 */
#define POD2_TAG      (2 * POD_TAG_BASE + WIDGET_POD_0806)  /* 2033 */

/* =========================================================
 * SYS 妯″潡鍏ㄥ眬闈欐€佹寚閽堬紙O(1) 鐩存帴璁块棶锛岄浂閬嶅巻锛?
 * ========================================================= */
static lv_obj_t *s_sys_batt_lbl = NULL;      /* 鐢甸噺鐧惧垎姣?*/
static lv_obj_t *s_sys_temp_lbl = NULL;      /* 娓╁害 */
static lv_obj_t *s_sys_strobe_img = NULL;    /* 鐣欒浆鐏浘鏍?*/
static lv_obj_t *s_sys_flash_img = NULL;     /* 鎵嬬數绛掑浘鏍?*/
static lv_obj_t *s_sys_cyl_lbl = NULL;      /* 姘旂摱鏁伴噺鏂囨湰 "x0" */

/* =========================================================
 * 鑾峰彇 POD 鏍囩锛堟牴鎹綋鍓嶆覆鏌撹鏁板櫒杩斿洖鍊硷級
 * 杩斿洖 POD1_TAG 鎴?POD2_TAG锛岀敤浜庣儥鍗板埌 user_data
 *
 * 娉ㄦ剰锛歴_pod_render_count 宸插湪 render_widget_by_id 涓厛閫掑銆?
 * 鎵€浠?count=1 鏃朵负绗?涓狿OD锛宑ount=2 鏃朵负绗?涓狿OD銆?
 * ========================================================= */
static uintptr_t arex_get_pod_tag(void)
{
    /* 绗?娆¤皟鐢?count=1锛屽鏁? 鈫?POD1_TAG
     * 绗?娆¤皟鐢?count=2锛屽伓鏁? 鈫?POD2_TAG */
    return (s_pod_render_count % 2 == 1) ? POD1_TAG : POD2_TAG;
}

/* =========================================================
 * 鑾峰彇 POD 缂栧彿锛堣繑鍥?1 鎴?2锛?
 * ========================================================= */
static uint8_t arex_get_pod_index(void)
{
    /* 绗?娆¤皟鐢?count=1锛屽鏁? 鈫?POD1
     * 绗?娆¤皟鐢?count=2锛屽伓鏁? 鈫?POD2 */
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
 * 娓叉煋璁℃暟鍣ㄥ綊闆讹紙姣忔缃戞牸閲嶅缓/閲嶇粯鍓嶅繀椤昏皟鐢級
 * 鐢?arex_screen_rebuild_layout() 鎴?left_anchor_create() 璋冪敤
 * ========================================================= */
void arex_reset_widget_render_state(void)
{
    s_pod_render_count = 0;

    /* 褰掗浂搴曢儴 SystemData 闈欐€佸彞鏌勶紝闃叉 lv_timer 璁块棶姝诲唴瀛?*/
    s_sys_batt_lbl     = NULL;
    s_sys_temp_lbl     = NULL;
    s_sys_strobe_img   = NULL;
    s_sys_flash_img    = NULL;
    s_sys_cyl_lbl      = NULL;
}

/* 宸︿晶 2x7 缁濆缃戞牸閰嶇疆鏁扮粍
 *
 * 160x420 鍖哄煙 = 2鍒?80px) x 7琛?60px)
 * 涓?5F 鍗＄墖鍏辩敤 arex_widget_id_t 鏋氫妇浣撶郴銆?
 *
 * Grid Layout:
 *   Row 0: NDL      | (鍗犵敤 2x1 = 160x60)
 *   Row 1: DEPTH    | (鍗犵敤 2x2 = 160x120, 甯?sudu 閫熺巼鍥炬爣)
 *   Row 2: (DEPTH 绗簩琛?
 *   Row 3: POD1     | POD2    (鍚勫崰鐢?1x1 = 80x60)
 *   Row 4: TIME     | (鍗犵敤 2x1 = 160x60)
 *   Row 5: GAS      | (鍗犵敤 2x1 = 160x60)
 *   Row 6: SYS      | (鍗犵敤 2x1 = 160x60锛孲ystemData 鍙厤缃?
 * ========================================================= */
/* 宸︿晶缃戞牸閰嶇疆宸茶縼绉诲埌 g_sys_config.left_widgets[] */

/* 5F 鑷畾涔夌綉鏍奸厤缃凡杩佺Щ鍒?g_sys_config.custom_cards[0].widgets[] */


/* 浠?KV 鎸佷箙鍖栧瓨鍌ㄥ姞杞介厤缃紙weak 瀹炵幇鐢卞叿浣撳钩鍙拌鐩栵級 */
/* =========================================================
 * 榛樿閰嶇疆鍊?
 *
 * 褰撳墠瀹炵幇鐨勫竷灞€: Left Grid + Right Cards
 *   宸︿晶: 160x420 鍥哄畾 2鍒?x80) x 7琛?y60) 缃戞牸
 *   鍙充晶: tileview 婊戝姩鍗＄墖 (INFO / 5F / DECO / COMPASS / GAS / PLAN / SETUP)
 *   瀹夊叏鍖? 580x420 鐢?left_anchor(160) + right_cards(420) 缁勬垚
 *
 * 瀛楁鍒嗙粍:
 *   [A] 娲昏穬瀛楁 鈥?褰撳墠娓叉煋浠ｇ爜瀹為檯璇诲彇
 *   [R] 棰勭暀瀛楁 鈥?宸插畾涔変絾娓叉煋浠ｇ爜鏈娇鐢紝涓烘湭鏉?Classic 涓婁笅甯冨眬棰勭暀
 * ========================================================= */
void arex_sys_config_defaults(arex_sys_config_t *cfg)
{
    memset(cfg, 0, sizeof(arex_sys_config_t));

    /* ========== [A] 瀹夊叏鍖?========== */
    cfg->safe_zone_w  = 580;
    cfg->safe_zone_h  = 420;
    cfg->offset_x     = 0;            /* x=0 琛ㄧず姘村钩灞呬腑锛堝乏鍙冲悇鐣欑櫧 3U锛?*/
    cfg->offset_y     = -10;          /* y=-10 鍚戜笂鍋忕Щ锛堜笂闈㈢暀鐧?2U锛屼笅闈㈢暀鐧?4U锛?*/

    /* ========== [A] 鏋舵瀯 ========== */
    cfg->layout_order  = AREX_ORDER_NORMAL;  /* 0=鏍囧噯(宸﹂敋鍙冲崱)锛?=缈昏浆(鍙抽敋宸﹀崱) */
    cfg->dots_position = AREX_DOTS_LEFT;    /* tileview 鎸囩ず鐐逛綅缃?*/
    cfg->compass_style = AREX_COMPASS_CLASSIC;
    cfg->mask_enabled  = false;

    /* ========== [R] 涓婚妯″紡棰勭暀锛堝綋鍓嶅浐瀹氫负 Left Grid + Right Cards锛?==========
     * 鍙€夋墿灞曚负 Classic 涓婁笅娴佸紡甯冨眬锛屽眾鏃舵覆鏌撲唬鐮侀渶璇诲彇浠ヤ笅瀛楁锛?
     *   - theme_mode        鈫?AREX_THEME_CLASSIC
     *   - h_depth / h_ndl / h_pod / h_batt / h_gas / h_time   鈫?涓婁笅鍒嗗尯楂樺害
     *   - sep_style / sep_thick                                鈫?鍒嗗壊绾挎牱寮?
     *   - split_outward / flash_speed                          鈫?鍔ㄧ敾鍙傛暟
     *   - title_h_u / h_menu_item / gap_menu                  鈫?鑿滃崟鎺掔増
     *   - h_tissues_chart                                     鈫?缁勭粐鍥鹃珮搴?
     */
    cfg->theme_mode    = AREX_THEME_TECH;    /* 褰撳墠鍥哄畾 TECH锛圠eft Grid + Right Cards锛?*/
    cfg->sep_style     = AREX_SEP_DASHED;    /* [R] 鍒嗗壊绾挎牱寮忥紙寰呯敤锛?*/
    cfg->sep_thick     = 2;                  /* [R] 绾挎潯绮楃粏 px锛堝緟鐢級 */
    cfg->split_outward = true;               /* [R] 鍙屾嫾妯″潡灞曞紑鏂瑰悜锛堝緟鐢級 */
    cfg->flash_speed   = 1;                  /* [R] 鍔ㄧ敾闂儊閫熷害锛堝緟鐢級 */

    /* ========== [A] 鍒嗗壊绾块€忔槑搴?========== */
    cfg->sep_alpha  = 51;   /* 20% of 255 鈥?SystemData 椤堕儴鍒嗗壊绾块€忔槑搴?*/

    /* ========== [R] Classic 涓婁笅甯冨眬 10U 楂樺害鍒嗛厤 (褰撳墠鏈娇鐢? ==========
     * 1U = 10px锛屾€昏 10U = 100px锛堥鐣欏皢鏉ユ敼涓轰笂涓嬪垎鍖烘祦寮忓竷灞€锛?
     * DEPTH 澶ч€氭爮 鈫?NDL/TTS 鍙屾嫾 鈫?POD 鍙屾嫾 鈫?BATT 鍙屾嫾 鈫?GAS 鈫?DIVE TIME
     */
    cfg->h_depth         = 8;   /* DEPTH 澶ч€氭爮: 8U=80px */
    cfg->h_ndl           = 6;   /* NDL/TTS 鍙屾嫾: 6U=60px */
    cfg->h_pod           = 6;   /* POD 1/2 鍙屾嫾: 6U=60px */
    cfg->h_batt          = 5;   /* BATT/W.TIME 鍙屾嫾: 5U=50px */
    cfg->h_gas           = 6;   /* GAS 涓€氭爮: 6U=60px */
    cfg->h_time          = 5;   /* DIVE TIME 搴曢儴: 5U=50px */
    cfg->title_h_u       = 2;   /* [R] 鏍囬楂樺害锛堝緟鐢級 */
    cfg->h_menu_item     = 5;   /* [R] 鑿滃崟椤归珮搴︼紙寰呯敤锛?*/
    cfg->gap_menu        = 1;   /* [R] 鑿滃崟椤归棿璺濓紙寰呯敤锛?*/
    cfg->h_tissues_chart = 9;   /* [R] 缁勭粐鏌卞浘楂樺害锛堝緟鐢級 */

    /* ========== [A] 闈㈡澘闂磋窛 ========== */
    cfg->gap_u       = 0;   /* 宸︿晶閿氱偣涓庡彸渚ч潰鏉块棿璺? 0U=0px锛堢敱 sep_thick 璐熻矗鍒嗗壊绾跨矖缁嗭級 */
    cfg->panel_gap_u = 1;   /* tileview 瀹瑰櫒闂磋窛: 1U=10px */

    /* ========== [A] 5F 鑷畾涔夌綉鏍?(5鍒?x 6琛? ==========
     *
     *  5鍒楀竷灞€绀烘剰锛?鍒?10鏍硷紝6琛岋級锛?
     *  col:  0  1  2  3  4
     *  row0: [DEPTH 2x2 澶у潡    ] [TEMP  ] [HEADING 2 x1 ]
     *  row2: [绌烘Ы    ]          [BATT   ] [PPO2 1x1 ]
     *  row3: [NDL 2x1           ] [TTS 2x1 ] [CNS  1x1 ]
     *  row4: [POD1              ] [POD2    ] [绌烘Ы   ]
     *  row5: [绌烘Ы               ] [绌烘Ы    ] [绌烘Ы   ]
     *
     *  绠€娲佷綅缃厤缃細widget_id + x/y 涓夊瓧娈碉紝span_w/h 鐢?MCU 鏍峰紡琛ㄨ嚜鍔ㄦ帹瀵?
     */
    /* 鍏煎鏂版灦鏋? 浣跨敤 custom_cards[0] 瀛樺偍鍗曞紶鍗＄墖鐨勯厤缃?*/
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
    };  /* SAC 宸茬Щ闄?*/
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

    /* ========== [A] 宸︿晶 2x7 鍥哄畾缃戞牸 (160x420) ==========
     * 160x420 鍖哄煙 = 2鍒?80px) x 7琛?60px)锛岀敱 arex_render_left_anchor_grid() 娓叉煋
     *
     *  Grid Layout:
     *    Row 0: NDL      | (2x1 鈫?160x60)
     *    Row 1-2: DEPTH  | (2x2 鈫?160x120锛屽甫 sudu 閫熺巼鍥炬爣)
     *    Row 3: POD1     | POD2    (鍚?1x1 鈫?80x60)
     *    Row 4: TIME     | (2x1 鈫?160x60)
     *    Row 5: GAS      | (2x1 鈫?160x60)
     *    Row 6: SYS      | (2x1 鈫?160x60锛孲ystemData 鍙厤缃?
     */
    /* 绠€娲佷綅缃厤缃細widget_id + x/y锛宻pan_w/h 鐢?MCU 鏍峰紡琛ㄨ嚜鍔ㄦ帹瀵?*/
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
    };  /* 娼滄按鏃堕棿 */
    cfg->left_widgets[3] = (arex_grid_widget_t)
    {
        WIDGET_GAS_1606,        0, 4
    };
    /* 默认布局先关闭 POD1/POD2 显示，保留槽位避免影响整体版面高度 */
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

    /* 鍔ㄦ€佽绠楀疄闄?widget 鏁伴噺锛堜互鏈€鍚庝竴涓潪闆?widget 涓哄噯锛?*/
    cfg->left_widget_count = 0;
    for (int i = 0; i < AREX_LEFT_MAX_WIDGETS; i++)
    {
        if (cfg->left_widgets[i].widget_id != 0)
        {
            cfg->left_widget_count = i + 1;
        }
    }

    /* ========== [A] 鍙充晶鍗＄墖椤哄簭 (tileview 婊戝姩椤哄簭) ==========
     * card_order[pos] = card_id
     * INFO(0) 鍥哄畾锛孲ETUP(13) 鍥哄畾锛屼腑闂?12 寮犲彲鐢?APP 閲嶆帓
     * 蹇呴』鍒濆鍖栨墍鏈?14 涓綅缃紒
     * CARD_ID_UNUSED(0xFF)=鏈崰鐢ㄦЫ浣嶄笉鏄剧ずdot, CARD_ID_BLANK=绌虹櫧鍗′篃鏄湁鏁堝崱鐗囧簲鏄剧ずdot
     */
    memset(cfg->card_order, CARD_ID_UNUSED, sizeof(cfg->card_order));
    cfg->card_order[CARD_POS_INFO]   = CARD_ID_INFO;//鑿滃崟锛屼笉绠楀崱鐗?
    cfg->card_order[CARD_POS_1]      = CARD_ID_COMPASS;
    cfg->card_order[CARD_POS_2]      = CARD_ID_DECO;
    cfg->card_order[CARD_POS_3]      = CARD_ID_PLAN;
    cfg->card_order[CARD_POS_4]      = CARD_ID_GAS;
    cfg->card_order[CARD_POS_5]      = CARD_ID_CUSTOM_GRID;
    cfg->card_order[CARD_POS_6]      = CARD_ID_BLANK;      /* 绌虹櫧鍗＄墖 */
    /* CARD_POS_7 ~ CARD_POS_12 淇濇寔 CARD_ID_BLANK */
    cfg->card_order[CARD_POS_SETUP]  = CARD_ID_SETUP;//鑿滃崟锛屼笉绠楀崱鐗?

    /* ========== [A] 鍗＄墖妲戒綅鏄犲皠 ==========
     * custom_card_slot[pos] = custom_card_index (0~11)
     * pos 瀵瑰簲 card_order 涓殑鍔ㄦ€佹Ы浣嶇疆
     * 榛樿锛氱涓€涓?CUSTOM_GRID 鍗＄墖鏄犲皠鍒?custom_cards[0]
     */
    memset(cfg->custom_card_slot, 0xFF, sizeof(cfg->custom_card_slot));
    cfg->custom_card_slot[CARD_POS_5] = 0;  /* CUSTOM_GRID 鏄犲皠鍒?custom_cards[0] */

    /* ========== [A] 鐢ㄦ埛璁剧疆榛樿鍊?========== */
    cfg->mod_ppo2       = 1.4f;
    cfg->conservatism   = 1;    /* MED */
    cfg->brightness     = 1;    /* ECO */
}

/* =========================================================
 * 瀹夊叏鍖鸿竟鐣屾娴?
 * ========================================================= */
bool arex_safe_zone_in_danger(void)
{
    int16_t max_offset_x = (int16_t)((AREX_PHYSICAL_W - g_sys_config.safe_zone_w) / 2);
    int16_t max_offset_y = (int16_t)((AREX_PHYSICAL_H - g_sys_config.safe_zone_h) / 2);

    if (g_sys_config.offset_x < -max_offset_x || g_sys_config.offset_x > max_offset_x)
        return true;
    if (g_sys_config.offset_y < -max_offset_y || g_sys_config.offset_y > max_offset_y)
        return true;

    /* 闈㈤暅鐩插尯鎺╄啘妫€娴?*/
    if (g_sys_config.mask_enabled)
    {
        int16_t bottom_edge = (int16_t)(AREX_PHYSICAL_H / 2 + g_sys_config.safe_zone_h / 2 + g_sys_config.offset_y);
        if (bottom_edge > AREX_PHYSICAL_H - AREX_MASK_EDGE_GUARD)
            return true;
    }

    return false;
}

/* =========================================================
 * 杈呭姪锛氳绠?Safe Zone 鍐呴儴鍙敤鍖哄煙
 * ========================================================= */
void arex_calc_layout_rect(int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h,
                           int16_t anchor_offset_x, int16_t anchor_offset_y)
{
    /* 浠ュ睆骞曚腑蹇冧负鍘熺偣锛屽簲鐢ㄥ畨鍏ㄥ尯鍋忕Щ */
    int16_t center_x = (int16_t)(AREX_PHYSICAL_W / 2) + anchor_offset_x;
    int16_t center_y = (int16_t)(AREX_PHYSICAL_H / 2) + anchor_offset_y;

    *out_x = center_x - (int16_t)(g_sys_config.safe_zone_w / 2);
    *out_y = center_y - (int16_t)(g_sys_config.safe_zone_h / 2);
    *out_w = g_sys_config.safe_zone_w;
    *out_h = g_sys_config.safe_zone_h;
}

/* =========================================================
 * Tech 妯″紡缁濆鍧愭爣鎺ㄧ畻
 *
 * 宸﹂敋鐐? (0, 0), 瀹?160px, 楂?鍏?safe_zone_h
 * 鍙冲崱鐗? (160+gap, 0), 瀹?safe_zone_w-160-gap, 楂?鍏?safe_zone_h
 * 缈昏浆鏃? 浜ゆ崲宸﹀彸 X
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
 * Classic 妯″紡缁濆鍧愭爣鎺ㄧ畻
 *
 * 涓婂尯: (0, 0), 瀹?safe_zone_w, 楂?鎸?10U 绱姞璁＄畻
 * 涓嬪尯: (0, top_h+gap), 瀹?safe_zone_w, 楂?safe_zone_h-top_h-gap
 * 缈昏浆鏃? 浜ゆ崲涓婁笅 Y
 * ========================================================= */
void arex_calc_classic_layout(int16_t *out_top_x, int16_t *out_top_y,
                              uint16_t *out_top_w, uint16_t *out_top_h,
                              int16_t *out_bot_x, int16_t *out_bot_y,
                              uint16_t *out_bot_w, uint16_t *out_bot_h)
{
    uint16_t gap = g_sys_config.gap_u * AREX_BASE_U;

    /* 璁＄畻涓婂尯鎬婚珮搴?= 鍚勬ā鍧楅珮搴︾疮鍔?*/
    uint16_t top_h = 0;
    top_h += g_sys_config.h_depth * AREX_BASE_U;               /* DEPTH */
    top_h += g_sys_config.h_ndl * AREX_BASE_U + gap;            /* NDL/TTS 鍙屾嫾 */
    top_h += g_sys_config.h_pod * AREX_BASE_U + gap;            /* POD 鍙屾嫾 */
    top_h += g_sys_config.h_batt * AREX_BASE_U + gap;           /* BATT 鍙屾嫾 */
    top_h += g_sys_config.h_gas * AREX_BASE_U + gap;            /* GAS */
    top_h += g_sys_config.h_time * AREX_BASE_U;                 /* DIVE TIME */

    /* 闆堕珮搴︿繚鎶わ細鏈€灏?AREX_MIN_CLASSIC_TOP_H px */
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
 * 5x6 缃戞牸甯冨眬鎺ㄧ畻
 *
 * 璁＄畻姣忎釜 widget 鍗曞厓鏍肩殑缁濆浣嶇疆銆?
 * 鏀跺埌 row(0~5), col(0~4), w_span(1~2), h_span(1~2)
 * 鐩存帴绠楀嚭 X = col * unit_w, Y = row * unit_h
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

    /* 杈圭晫淇锛岄槻姝㈣秺鐣?*/
    if (*out_x + *out_w > parent_w) *out_w = parent_w - *out_x;
    if (*out_y + *out_h > parent_h) *out_h = parent_h - *out_y;
}

/* =========================================================
 * 16 鏌辩粍缁囧浘 X 鍧愭爣鎺ㄧ畻
 *
 * 搴曢儴瀵归綈锛?6 绛夊垎鏌辩姸鍥俱€?
 * 姣忔牴鏌卞 = total_w / 16锛孹 = i * col_w
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
    (void)bar_max_h; /* 鏌遍珮鐢辫皟鐢ㄦ柟鎸夌櫨鍒嗘瘮绠楀嚭锛屾澶勫彧杩斿洖 X 鍜?W */
}

/* =========================================================
 * LVGL 鏍峰紡杈呭姪
 * ========================================================= */

/* 灏?AREX_ALIGN_* 杞崲涓?LVGL 瀵归綈鏂瑰紡 */
lv_text_align_t arex_align_to_lv(uint8_t align)
{
    if (align == AREX_ALIGN_LEFT)   return LV_TEXT_ALIGN_LEFT;
    if (align == AREX_ALIGN_CENTER) return LV_TEXT_ALIGN_CENTER;
    return LV_TEXT_ALIGN_RIGHT;
}

/* 灏嗗榻愯浆鎹负 LVGL ALIGN 甯搁噺 */
lv_align_t arex_align_to_lv_align(uint8_t align)
{
    if (align == AREX_ALIGN_LEFT)   return LV_ALIGN_LEFT_MID;
    if (align == AREX_ALIGN_CENTER) return LV_ALIGN_CENTER;
    return LV_ALIGN_RIGHT_MID;
}

/* =========================================================
 * 瀛椾綋鏄犲皠鍣?(Font Mapper)
 *
 * 鍏ㄧ郴缁熷敮涓€鍏佽灏嗗瓧浣?ID 杞崲涓虹湡瀹?lvgl 瀛椾綋鎸囬拡鐨勫湴鏂广€?
 * 鎵€鏈夐厤缃粨鏋勪綋涓繚瀛樼殑 title_font / val_font 鍧囧簲涓?arex_font_id_t 鍊笺€?
 *
 * ID 鏄犲皠琛細
 *   AREX_FONT_ID_SMALL  (0) 鈫?20px  鏍囩/鍗曚綅/Badge
 *   AREX_FONT_ID_TITLE  (1) 鈫?20px  鑿滃崟椤?鍗＄墖鏍囬
 *   AREX_FONT_ID_MEDIUM (2) 鈫?32px  鏁版嵁鍊?
 *   AREX_FONT_ID_LARGE  (3) 鈫?64px  娣卞害澶ф暟瀛?
 *   AREX_FONT_ID_HUGE   (4) 鈫?64px  澶у瓧浣?
 *   AREX_FONT_ID_NDL    (5) 鈫?48px  NDL鍑忓帇鏃堕棿
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
        return AREX_FONT_SMALL;   /* 鍏滃簳锛氭案涓嶄负 NULL */
    }
}

/* =========================================================
 * JSON 閰嶇疆瑙ｆ瀽 (鐢ㄤ簬 App 钃濈墮鍚屾 / SETUP 瀵煎叆)
 * ========================================================= */
/*
 * 褰撴帴鏀跺埌 JSON 閰嶇疆鏃讹紝鎸変互涓嬫祦绋嬪鐞嗭細
 *
 * 1. 瑙ｆ瀽 JSON 鍒颁复鏃剁粨鏋勪綋
 * 2. 璋冪敤 memcpy(&g_sys_config, &tmp, sizeof(...)) 瑕嗙洊
 * 3. 璋冪敤 arex_ui_apply_config() 閲嶆帓 UI
 *
 * JSON 瀛楁绀轰緥 (涓?HTML configIds 瀵瑰簲):
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
 * 鍒濆鍖栧叆鍙?(鐢?UI_main 璋冪敤)
 *
 * 鍚姩娴佺▼锛?
 *   1. 浠?KV 鎸佷箙鍖栬鍙栭厤缃?鈫?鎴愬姛鍒欑洿鎺ヤ娇鐢?
 *   2. KV 鏃犳暟鎹?璇诲彇澶辫触 鈫?濉叆榛樿鍊?
 *   3. 浼犳劅鍣ㄦ暟鎹竻闆讹紙鐢?sim_tick_cb / 澶栭儴 API 瀹炴椂鍐欏叆锛?
 * ========================================================= */
void arex_ui_init(void)
{
    /* 1. 鍔犺浇鎸佷箙鍖栭厤缃紝澶辫触鍒欑敤榛樿鍊间繚搴?*/
    if (!arex_config_load(&g_sys_config))
    {
        arex_sys_config_defaults(&g_sys_config);
    }

    /* 2. 浼犳劅鍣ㄦ暟鎹竻闆?*/
    arex_data_init();
}

/* =========================================================
 * 搴旂敤閰嶇疆鍙樻洿 (閰嶇疆鐣岄潰淇敼鍚庤皟鐢ㄦ鍑芥暟)
 * 1. 妫€娴?safe zone 杈圭晫
 * 2. 閲嶅缓 left anchor 鎺掔増
 * 3. 閲嶅缓 right card 甯冨眬
 * ========================================================= */
void arex_ui_apply_config(void)
{
    /* 瀹夊叏鍖鸿竟鐣屾牎楠?*/
    if (arex_safe_zone_in_danger())
    {
        /* TODO: 瑙﹀彂鍗遍櫓璀﹀憡 UI */
    }

    /* TODO: 瑙﹀彂 arex_screen_rebuild_layout() 閲嶅缓鎺掔増
     * 杩欏皢鍦ㄩ噸鏋?arex_screen.c 鏃跺疄鐜?
     */
}

/* =========================================================
 * 鍗＄墖椤哄簭鏌ヨ (缁熶竴鍏ュ彛 鈥?鏇夸唬鏃х殑 g_arex_card_order)
 * ========================================================= */
uint8_t g_sys_card_order(uint8_t pos)
{
    if (pos >= AREX_CARD_COUNT) return 0;
    return g_sys_config.card_order[pos];
}

/* =========================================================
 * 閫氱敤鍔ㄦ€佽彍鍗曞伐鍘?
 * 鎵€鏈夊昂瀵镐粠 g_sys_config 鎺ㄧ畻锛屼笉鍚‖缂栫爜鍍忕礌鍊笺€?
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
    int item_w = right_canvas_w - 15;  /* 鍙充晶 15px 鍛煎惛璺?*/

    int current_y = start_y;
    for (uint8_t i = 0; i < item_count; i++)
    {
        const arex_menu_item_cfg_t *item_cfg = &items[i];
        /* height_u 榛樿 0 鈫?鏌?h_menu_item (鍗曚綅 U) */
        int item_h = (int)(item_cfg->height_u > 0 ? item_cfg->height_u : g_sys_config.h_menu_item)
                     * AREX_BASE_U;
        /* gap_y 浠?gap_menu (鍗曚綅 U) 鎺ㄧ畻 */
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

        /* 鏍囬 label */
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
 * 閫氱敤鍗＄墖鏍囬娓叉煋鍣?
 * 鏍囬鏂囧瓧(Y=8)涓庡垎鍓茬嚎(Y=48)涓鸿瑙夌粍鍚堬紝缁濆鐒婃鍦ㄥ崱鐗囬《閮ㄣ€?
 * AREX_CARD_TITLE_H 浠呬綔涓轰笅鏂?鍐呭鍖?鑿滃崟/鍥捐〃)鐨勮捣濮?Y 鍧愭爣鍋忕Щ"銆?
 *
 * parent_card: 鐖跺鍣紙tile 瀵硅薄锛?
 * title_text:  鏍囬鏂囧瓧
 *
 * 鏍囬甯冨眬锛堢剨姝伙紝缁濆涓嶈窡闅?AREX_CARD_TITLE_H锛夛細
 *   鏂囧瓧:   Y=8,  楂樺害 40px锛孉REX_LIGHT 鑹?
 *   鍒嗗壊绾? Y=48, h=2px, AREX_DARK 鑹?
 * ========================================================= */
void arex_render_card_title(lv_obj_t *parent_card, const char *title_text)
{
    uint16_t right_w = g_sys_config.safe_zone_w - AREX_LEFT_ANCHOR_W
                       - ((int)g_sys_config.gap_u * AREX_BASE_U);

    /* 1. 鏍囬鏂囧瓧锛氭墥鍏夐粯璁ゆ牱寮?+ 寮哄埗灏忓瓧鍙?娆＄骇棰滆壊 */
    lv_obj_t *lbl = lv_label_create(parent_card);
    lv_obj_remove_style_all(lbl);
    lv_obj_set_pos(lbl, 16, 8);
    lv_obj_set_size(lbl, right_w - 32, 40);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_label_set_text(lbl, title_text);
    lv_obj_set_style_text_font(lbl, arex_get_font(AREX_FONT_ID_TITLE), 0);
    lv_obj_set_style_text_color(lbl, AREX_LIGHT, 0);

    /* 2. 鍒嗗壊绾匡細缁濆鍥哄畾鍦ㄦ枃瀛椾笅鏂癸紙鐒婃 Y=48锛?*/
    lv_obj_t *line = lv_obj_create(parent_card);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, right_w - 32, 2);
    lv_obj_set_pos(line, 16, 48);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(line, AREX_DARK, 0);
}

/* =========================================================
 * 5F 鑷畾涔夌綉鏍肩粍浠跺閮ㄥ鍣紙鐢?arex_screen.c 娉ㄥ叆锛?
 * ========================================================= */
lv_obj_t *g_left_anchor_obj = NULL;
/* 澶氬紶鑷畾涔夊崱鐗囧鍣ㄦ暟缁?*/
lv_obj_t *g_card_custom_objs[AREX_MAX_CUSTOM_CARDS];
uint8_t   g_card_custom_obj_count;

/* ============================================================
 * 馃毃 鍏ㄥ煙鍛婅鐘舵€侊紙50ms 瀹氭椂鍣ㄤ細鎵弿杩欎袱涓鍣級
 * ============================================================ */
static arex_widget_id_t g_current_alarm_target = WIDGET_EMPTY;
static uint8_t          g_current_alarm_level = 0;
static lv_obj_t        *s_alarm_banner = NULL;
static lv_obj_t        *s_alarm_banner_lbl = NULL;

/* =========================================================
 * 5F 缃戞牸鍧愭爣鎺ㄧ畻锛堢函鏁板缁濆鏄犲皠锛屾棤 lv_grid锛?
 *
 * 鏍稿績鍏紡锛?
 *   cell_w = parent_w / 5
 *   cell_h = parent_h / 6
 *   abs_x  = col * cell_w + gap
 *   abs_y  = row * cell_h + gap
 *   abs_w  = span_w * cell_w - gap*2
 *   abs_h  = span_h * cell_h - gap*2
 * ========================================================= */
#define WIDGET_GAP  0   /* 缃戞牸缂濋殭 px */

/* =========================================================
 * 5F 缃戞牸鍧愭爣鎺ㄧ畻锛堥攣瀹?5 鍒?+ 鏍囬閬胯锛屽姩鎬?cell_h 鑷€傚簲锛?
 *
 * parent_w/parent_h: 鐖跺鍣ㄦ€诲昂瀵革紙鐢ㄤ簬鍔ㄦ€佹帹绠楋級
 * row/col: 缃戞牸琛屽垪绱㈠紩(0~5 / 0~4)
 * span_w/span_h: 璺ㄨ秺鐨勫垪鏁?琛屾暟
 * out_*: 杈撳嚭缁濆鍧愭爣
 *
 * 鎺掔増鐭╅樀涓ユ牸閿佸畾 5 鍒楋細
 *   cell_w = parent_w / 5
 *   cell_h = (parent_h - AREX_CARD_TITLE_H) / 6
 * Y 鍧愭爣澧炲姞 AREX_CARD_TITLE_H=60px 鍋忕Щ锛岀‘淇濈涓€琛岃惤鍦ㄦ爣棰樺尯涓嬫柟銆?
 * 瀹介珮鍑?4px (2px 缂濋殭 x2) 鍒堕€犲洓鍛?2px 鐗╃悊鐣欑櫧銆?
 * 濡傛灉鏍囬楂樺害鏀逛负鍏朵粬鍊硷紝cell_h 浼氳嚜鍔ㄩ噸鏂拌绠楋紝鍐呭鍖哄畬缇庤嚜閫傚簲銆?
 * ========================================================= */
void arex_calc_widget_grid(uint16_t parent_w, uint16_t parent_h,
                           uint8_t row, uint8_t col,
                           uint8_t span_w, uint8_t span_h,
                           int16_t *out_x, int16_t *out_y,
                           uint16_t *out_w, uint16_t *out_h)
{
    /* 閿佸畾 5 鍒楀熀鍑嗭紝鍔ㄦ€佽绠?cell_h */
    uint16_t cell_w = parent_w / 5;
    uint16_t cell_h = (parent_h > AREX_CARD_TITLE_H)
                      ? ((parent_h - AREX_CARD_TITLE_H) / AREX_WIDGET_ROWS)
                      : 60;  /* 淇濆簳 fallback */

    /* X: 鍒楀亸绉?+ 缂濋殭(2px) */
    *out_x = (int16_t)(col * cell_w + WIDGET_GAP);
    /* Y: 鏍囬鍖轰笅鏂?+ 琛屽亸绉?+ 缂濋殭(2px) */
    *out_y = (int16_t)(AREX_CARD_TITLE_H + row * cell_h + WIDGET_GAP);
    /* 瀹介珮: 璺ㄨ窛脳鍩哄噯 - 4px 缂濋殭(鍥涘懆鍚?2px) */
    *out_w = (uint16_t)(span_w * cell_w - WIDGET_GAP * 2);
    *out_h = (uint16_t)(span_h * cell_h - WIDGET_GAP * 2);

    /* 杈圭晫淇锛堜互瀹瑰櫒鎬诲昂瀵镐负杈圭晫锛?*/
    if (*out_x + *out_w > (int16_t)parent_w)
        *out_w = (uint16_t)((int16_t)parent_w - *out_x);
    if (*out_y + *out_h > (int16_t)parent_h)
        *out_h = (uint16_t)((int16_t)parent_h - *out_y);
}

/* 瀛楀彿鑷€傚簲寮曟搸锛堝凡鍐呰仈鍒?render_widget_by_id锛屼繚鐣欏嚱鏁颁綋渚涙湭鏉ユ墿灞曪級 */

/* =========================================================
 * 鑾峰彇 widget 鏄剧ず鍚嶇О锛堜粠 g_widget_styles[] 璇诲彇 title 瀛楁锛?
 * ========================================================= */
const char *arex_get_widget_name(arex_widget_id_t id)
{
    const arex_widget_style_t *style = arex_get_widget_style(id);
    if (!style) return "???";
    return style->title ? style->title : "";
}

/* =========================================================
 * NDL 搴曢儴妯悜 10 瀹牸杩涘害鏉＄粯鍒跺洖璋?(0 RAM)
 * 鏁板鎺ㄦ紨锛氬鍣ㄥ搴?abs_w - 16锛屼袱杈瑰悇鐣?8px 杈硅窛
 * 10涓潡 + 9涓?px闂撮殭 = 137px锛堝畬缇庡～婊★級
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

    /* 璁＄畻鎬讳綋鐧惧垎姣旓細
     * - 甯告€侊細鎸?NDL/99 鏄剧ず锛?9 瑙嗕负婊℃牸
     * - 瀹夊叏鍋滅暀锛氭湭杩涚珯鍓嶄粛鎸?NDL锛涜繘绔欏悗鎸夊仠鐣欏墿浣欐椂闂寸缉鐭?
     * - 鍑忓帇鍋滅暀锛氭湭杩涚珯鍓嶄繚鎸佹弧鏍硷紱杩涚珯鍚庢寜褰撳墠鍑忓帇绔欏墿浣欐椂闂寸缉鐭?*/
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
    rect_dsc.radius = 0; /* 绾洿瑙?*/

    for (int i = 0; i < 10; i++)
    {
        int x1 = area->x1 + i * (block_w + gap);
        int x2 = x1 + block_w - 1;
        lv_area_t block_area = {x1, area->y1, x2, area->y2};

        if (i < active_blocks)
        {
            /* 鍏ㄤ寒鏍煎瓙 */
            rect_dsc.bg_color = AREX_GREEN;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_draw_rect(draw_ctx, &rect_dsc, &block_area);
        }
        else if (i == active_blocks && remainder > 0.05f)
        {
            /* 鍗婁寒鏍煎瓙 (鍏堢敾鏆楀簳锛屽啀鐩栦寒缁? */
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
            /* 鏈縺娲荤殑鏆楁牸 */
            rect_dsc.bg_color = AREX_DARK;
            rect_dsc.bg_opa = LV_OPA_COVER;
            lv_draw_rect(draw_ctx, &rect_dsc, &block_area);
        }
    }
}

/* =========================================================
 * 鍒涘缓鍗曚釜鑷畾涔夌粍浠讹紙缁勪欢宸ュ巶 鈥?宸︿晶缃戞牸 + 5F 鍏辩敤锛?
 *
 * 鍏抽敭锛氭瘡涓粍浠剁殑 lv_obj_set_user_data() 瀛樺偍浜嗘爣绛剧儥鍗般€?
 * 瀵逛簬 POD锛屼娇鐢ㄩ珮浣嶆帺鐮佸尯鍒嗭紙1033=POD1, 2033=POD2锛夈€?
 * 鍛婅寮曟搸闈犺繖涓儥鍗板疄鐜?宸︿晶閿氱偣 + 5F 缁勪欢鍚屾椂闂儊"銆?
 *
 * 鏋舵瀯閾佸緥锛?
 *   - 浣嶇疆鍙傛暟 (abs_x/y/w/h, span_w/h) 鐢辫皟鐢ㄦ柟浼犲叆
 *   - 鏍峰紡鍙傛暟 (font, offsets) 鐢?arex_get_widget_style(w_id) 鑷姩鏌ヨ〃
 *   - cfg_font_id != 255 鏃跺己鍒惰鐩栬嚜鍔ㄥ瓧鍙?
 *   - 閫熺巼鍥炬爣鐢卞伐鍘傝嚜涓绘煡瀛楀吀鍐冲畾锛堟牴鎹?elements & ELEM_BAR锛?
 *   - 涓撳睘缁勪欢锛圖EPTH/NDL锛夎蛋鏃╂湡杩斿洖锛屽唴閮ㄤ粛璇?style 鍙傛暟
 *   - 閫氱敤缁勪欢鎸?elements 鎺╃爜瑁呴厤娴佹按绾匡細TITLE 鈫?VALUE 鈫?UNIT 鈫?BAR
 *
 * POD 鍗曟ā鍏疯疆杞垎閰嶏細
 *   - 鍑芥暟鍏ュ彛妫€娴?w_id == WIDGET_POD_0806
 *   - 璋冪敤 arex_get_pod_tag() 鑾峰緱楂樹綅鎺╃爜鏍囩 (1033/2033)
 *   - 璋冪敤 arex_get_pod_index() 鑾峰緱 POD 缂栧彿 (1/2)
 *   - 灏嗘爣绛剧儥鍗板埌瀹瑰櫒 user_data
 * ========================================================= */
lv_obj_t *render_widget_by_id(lv_obj_t *parent,
                              arex_widget_id_t w_id,
                              int16_t abs_x, int16_t abs_y,
                              uint16_t abs_w, uint16_t abs_h,
                              uint8_t span_w, uint8_t span_h,
                              arex_font_id_t cfg_font_id)
{
    /* ===== POD 鍗曟ā鍏锋嫤鎴細鎻愬墠娑堣€楄鏁板櫒 ===== */
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

    /* 瀛楀彿閫夋嫨閫昏緫锛?
     *   cfg_font_id != 255 鈫?寮哄埗瑕嗙洊锛堣繍琛屾椂鎸囧畾锛?
     *   DEPTH 绯诲垪 鈫?鑷姩閫傞厤灏哄锛圚UGE/MEDIUM/SMALL锛?
     *   鍏朵粬缁勪欢 鈫?鐩存帴浣跨敤瀛楀吀 font_id */
    arex_font_id_t val_font_id;
    if (cfg_font_id != (arex_font_id_t)255)
    {
        val_font_id = cfg_font_id;  /* 寮哄埗瑕嗙洊锛堣繍琛屾椂鎸囧畾锛?*/
    }
    else if (w_id == WIDGET_DEPTH_1612 || w_id == WIDGET_DEPTH_1606)
    {
        /* DEPTH 缁勪欢锛氳嚜鍔ㄩ€傞厤灏哄 */
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
        /* 鍏朵粬缁勪欢锛氱洿鎺ヤ娇鐢ㄥ瓧鍏?font_id */
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

    /* 灏佹潃鎵€鏈夋粴鍔ㄦ潯 */
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);

    /* ===== 闈跺悜鍛婅鐑欏嵃 =====
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

    /* ===== DEPTH 2x2 涓撳睘娓叉煋锛堟暣鏁?灏忔暟+鍗曚綅鍒嗙锛?===== */
    bool is_2x2 = (span_w >= 2 && span_h >= 2);
    if (w_id == WIDGET_DEPTH_1612 && is_2x2)
    {
        /* 鏍峰紡鍙傛暟鏉ヨ嚜 arex_widget_style_t */
        const arex_style_depth_t *s = &style->spec.depth;

        /* ==========================================
         * 1. 瓒呭ぇ鍙锋暣鏁?-> 瀹藉害蹇呴』绱у瘑鍖呰９锛?
         * ========================================== */
        lv_obj_t *int_lbl = lv_label_create(obj);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT) lv_label_set_text(int_lbl, "--");
        else lv_label_set_text_fmt(int_lbl, "%d", (int)g_sensor_data.depth);
        // 瀛椾綋浠庡瓧鍏歌鍙栵紙font_id = HUGE 58px锛?
        lv_obj_set_style_text_font(int_lbl, arex_get_font(style->font_id), 0);
        lv_obj_set_style_text_color(int_lbl, AREX_GREEN, 0);

        // 缁濇潃鎶€锛氬繀椤昏涓?CONTENT锛佽繖鏍锋棤璁哄彉鎴?"6" 杩樻槸 "45"锛?
        // Label 鐨勫彸杈圭紭閮戒細姝绘鍖呬綇涓綅鏁帮紝缁濅笉鐣欎竴涓濈紳闅欙紒
        lv_obj_set_size(int_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

        // 璇诲彇瀛楀吀涓殑 RIGHT_MID 鍜?-45锛屾妸鍙宠竟缂樼剨姝诲湪杩欏牭澧欎笂锛?
        lv_obj_align(int_lbl, (lv_align_t)s->int_align, s->int_offset_x, s->int_offset_y);

        /* ==========================================
         * 2. 涓彿灏忔暟 -> 绱ц创鏁存暟鐨勫彸杈圭晫锛?
         * ========================================== */
        lv_obj_t *dec_lbl = lv_label_create(obj);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT) lv_label_set_text(dec_lbl, ".-");
        else
        {
            /* 鎻愬彇灏忔暟閮ㄥ垎锛氬彧淇濈暀涓€浣嶅皬鏁帮紝鑼冨洿 0-9 */
            float decimal_part = fabsf(g_sensor_data.depth - (int)g_sensor_data.depth);
            int dd = (int)(decimal_part * 10 + 0.5f);
            if (dd > 9) dd = 9;  /* 闃叉娴偣绮惧害闂瀵艰嚧澶氫綅鏁?*/
            lv_label_set_text_fmt(dec_lbl, ".%d", dd);
        }
        // 瀛椾綋浠庡瓧鍏歌鍙栵紙title_font_id = MEDIUM 28px锛屽皬鏁版瘮鏁存暟灏忥級
        lv_obj_set_style_text_font(dec_lbl, arex_get_font(style->title_font_id), 0);
        lv_obj_set_style_text_color(dec_lbl, AREX_GREEN, 0);
        lv_obj_set_size(dec_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

        // 鍥犱负鏁存暟鐨勫彸杈圭紭(涓綅鏁?琚剨姝讳簡锛屽皬鏁版寕鍦ㄥ畠鍙宠竟锛岃嚜鐒跺氨姘歌繙璐寸揣涓綅鏁帮紒
        lv_obj_align_to(dec_lbl, int_lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, s->dec_offset_x, s->dec_offset_y);

        /* ==========================================
         * 3. 灏忓彿鍗曚綅 (m) -> 绱ц创灏忔暟姝ｄ笅鏂?
         * ========================================== */
        if (style->elements & ELEM_UNIT)
        {
            lv_obj_t *unit_lbl = lv_label_create(obj);
            lv_label_set_text(unit_lbl, style->unit ? style->unit : "");
            // 鍗曚綅鍥哄畾鐢ㄥ皬鍙峰瓧浣?
            lv_obj_set_style_text_font(unit_lbl, arex_get_font(AREX_FONT_ID_SMALL), 0);
            lv_obj_set_style_text_color(unit_lbl, AREX_LIGHT, 0);
            lv_obj_set_size(unit_lbl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_align_to(unit_lbl, dec_lbl, LV_ALIGN_OUT_BOTTOM_MID, s->unit_offset_x, s->unit_offset_y);
        }

        /* 閫熺巼鍥炬爣锛氬伐鍘傝嚜涓绘煡瀛楀吀鍒ゆ柇鏄惁闇€瑕佺粯鍒?*/
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
        /* NDL 鍙樺舰閲戝垰锛氫粠 style->spec.ndl_stop 璇诲彇鎵€鏈変綅缃弬鏁?*/
        if (s_ndl_handle_count >= MAX_NDL_ICONS) return obj;
        ndl_handle_t *h = &s_ndl_handles[s_ndl_handle_count++];
        h->comp = obj;

        const arex_style_ndl_stop_t *s = &style->spec.ndl_stop;

        /* 鍒涘缓 10 瀹牸鐨勫簳灞傞€忔槑鐢绘澘 */
        h->horiz_bg = lv_obj_create(obj);
        lv_obj_remove_style_all(h->horiz_bg);
        /* 馃毃 瀹藉害濉弧鍑忓幓涓よ竟鐣欑櫧锛歛bs_w - 16锛屼袱杈瑰悇鐣?8px */
        lv_obj_set_size(h->horiz_bg, abs_w - 16, 10);
        /* 璐寸揣搴曢儴锛岀暐寰笂娴?4px */
        lv_obj_align(h->horiz_bg, LV_ALIGN_BOTTOM_MID, 0, -4);
        lv_obj_add_event_cb(h->horiz_bg, ndl_horiz_bar_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
        lv_obj_add_flag(h->horiz_bg, LV_OBJ_FLAG_HIDDEN);

        /* 椤堕儴鏍囬锛堥粯璁ら殣钘忥紝鍋滅暀鎬佹椂鏄剧ず锛?*/
        h->title_top = lv_label_create(obj);
        lv_obj_set_style_text_font(h->title_top, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(h->title_top, AREX_GREEN, 0);
        lv_label_set_text(h->title_top, "");
        lv_obj_add_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);

        /* 涓绘暟瀛?(濡?22, 3:00) - 浣跨敤48px瀛椾綋 */
        h->main_val = lv_label_create(obj);
        lv_obj_set_style_text_color(h->main_val, AREX_GREEN, 0);
        lv_obj_set_style_text_font(h->main_val, arex_get_font(AREX_FONT_ID_NDL), 0);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(h->main_val, "--");
        else
            lv_label_set_text_fmt(h->main_val, "%d", g_sensor_data.ndl);

        /* 搴曢儴鏍囬 (NDL 45) */
        h->sub_bot = lv_label_create(obj);
        lv_obj_set_style_text_font(h->sub_bot, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(h->sub_bot, AREX_GREEN, 0);
        lv_obj_add_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN);
        return obj;
    }
    else if (w_id == WIDGET_SYS_1606)
    {
        /* ===== SYS 妯″潡锛氱數姹?+ 娓╁害妯悜鎺掑垪 ===== */

        /* 宸︿晶锛氱數閲?Label */
        s_sys_batt_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(s_sys_batt_lbl, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
        lv_obj_set_style_text_color(s_sys_batt_lbl, AREX_GREEN, 0);
        lv_obj_align(s_sys_batt_lbl, LV_ALIGN_LEFT_MID, 4, 0);
        if (AREX_SHOW_PLACEHOLDER_ON_INIT)
            lv_label_set_text(s_sys_batt_lbl, "--%");
        else
            lv_label_set_text_fmt(s_sys_batt_lbl, "%u%%", arex_ui_clamp_battery_pct(g_sensor_data.battery_pct));

        /* 鍙充晶锛氭俯搴?Label */
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

    /* ===== 閫氱敤娴佹按绾匡細鎸?elements 鎺╃爜鎸夐渶瑁呴厤闆朵欢 =====
     * POD1/POD2/WTIME 鍙婃墍鏈?1x1/2x1 閫氱敤缁勪欢璧版璺緞
     * ELEM_TITLE 鈫?ELEM_VALUE 鈫?ELEM_UNIT 鈫?ELEM_BAR
     *
     * 鏍峰紡鍙傛暟鍏ㄩ儴鏉ヨ嚜 arex_get_widget_style(w_id) 鏌ヨ〃缁撴灉
     * 浠?title 鏂囨湰鍜屾暟鍊兼暟鎹簮渚濊禆 w_id 鍋?switch 鍒嗗彂 */

    /* --- 闆朵欢 1锛氭爣棰?--- */
    if ((style->elements & ELEM_TITLE) && style->title)
    {
        lv_obj_t *title_lbl = lv_label_create(obj);
        /* POD 鍗曟ā鍏凤細鏍规嵁 pod_index 鍔ㄦ€佸喅瀹氭爣棰樻枃瀛?*/
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

    /* --- 闆朵欢 2锛氫富鏁板€?--- */
    lv_obj_t *val_lbl = NULL;
    if (style->elements & ELEM_VALUE)
    {
        val_lbl = lv_label_create(obj);
        lv_obj_set_style_text_font(val_lbl, arex_get_font(val_font_id), 0);
        lv_obj_set_style_text_color(val_lbl, AREX_GREEN, 0);

        if (AREX_SHOW_PLACEHOLDER_ON_INIT)
        {
            /* 閫氱敤鍗犱綅绗?*/
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
            /* ===== POD 鍗曟ā鍏凤細鏁版嵁婧愭牴鎹?pod_index 鍔ㄦ€佸垎閰?===== */
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
            /* 馃毃 浠ヤ笅宸插簾寮冿紝Protobuf 宸茬Щ闄ゅ搴?ID
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
        /* 鎵€鏈変娇鐢?ELEM_VALUE 鐨?widget 閮戒娇鐢?spec.basic.value_align */
        lv_obj_align(val_lbl, (lv_align_t)style->spec.basic.value_align,
                     style->spec.basic.value_offset_x, style->spec.basic.value_offset_y);
        lv_obj_set_user_data(val_lbl, (void *)(uintptr_t)w_id);
    }

    /* --- 闆朵欢 3锛氬崟浣?--- */
    if ((style->elements & ELEM_UNIT) && style->unit)
    {
        lv_obj_t *unit_lbl = lv_label_create(obj);
        lv_label_set_text(unit_lbl, style->unit);
        lv_obj_set_style_text_font(unit_lbl, arex_get_font(AREX_FONT_ID_SMALL), 0);
        lv_obj_set_style_text_color(unit_lbl, AREX_LIGHT, 0);
        /* 鍗曚綅浣嶄簬鏁板€煎彸渚э紙瀵逛簬 2x1 绛夌獎缁勪欢锛?*/
        if ((style->elements & ELEM_VALUE) && (val_lbl != NULL))
        {
            /* 鎸傚湪鏁板€?label 鍙充晶 */
            lv_obj_align_to(unit_lbl, val_lbl, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
        }
        else
        {
            lv_obj_align(unit_lbl, (lv_align_t)style->title_align,
                         style->title_offset_x, style->title_offset_y);
        }
    }

    /* --- 闆朵欢 4锛氱壒娈?BAR --- */
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
            /* ASCENT_0812 (1x2)锛氱粯鍒朵笂鍗囬€熺巼鏂瑰悜绠ご鍥炬爣锛堝伐鍘傝嚜涓绘煡瀛楀吀鍐冲畾锛?*/
            lv_obj_t *sudu_img = lv_img_create(obj);
            lv_img_set_src(sudu_img, &sudo_up_level0);
            lv_obj_align(sudu_img, LV_ALIGN_CENTER, 0, 0);
            if (s_ascent_icon_count < MAX_ASCENT_ICONS)
                s_img_ascent_rate[s_ascent_icon_count++] = sudu_img;
        }
        else if (w_id == WIDGET_COMPASS_1612)
        {
            /* COMPASS_1612 (2x2)锛氬嵎灏?tape 鍦ㄦ棭鏈熷垎鏀噷锛孍LEM_BAR 鏍囪鐢?spec.compass 椹卞姩 */
        }
        else if (w_id == WIDGET_TISSUE_GF_4012 || w_id == WIDGET_TISSUE_RAW_4012)
        {
            /* TISSUE (4x2)锛?6 鏌辩粍缁囧浘锛孍LEM_BAR 鏍囪鐢?spec.tissue 椹卞姩 */
        }
        else if (w_id == WIDGET_SYS_1606)
        {
            /* SYS 鐢垫睜鏉?+ 澶栬鍥炬爣锛堢郴缁熺姸鎬佹爮锛?*/
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
 * 5F 缃戞牸鎬荤嚎娓叉煋鍣?
 *
 * 1. 浠?g_sys_config.custom_cards[] 璇诲彇缁勪欢閰嶇疆
 * 2. 閫愪竴鏋氶亶鍘嗭紝鐢ㄧ函鏁板琛屆楀垪鏄犲皠绠楀嚭缁濆鍧愭爣
 * 3. 璋冪敤缁勪欢宸ュ巶娓叉煋锛屾敞鍏?user_data 鐑欏嵃
 * 4. 娉ㄥ唽澶栭儴瀹瑰櫒鍒板憡璀﹀紩鎿?
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

    /* tile 鍒氬垱寤烘椂锛宑ontent 灏哄鏈夋鐜囪繕娌＄ǔ瀹氾紱杩欓噷鐩存帴浣跨敤瀵硅薄瀹介珮锛?
     * 骞跺湪寮傚父鏃跺洖閫€鍒?Safe Zone 鎺ㄥ鍊硷紝閬垮厤鑷畾涔夌粍浠惰绠楁垚 0 灏哄銆?*/
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

        /* 浠庢牱寮忚〃鏌?span_w/span_h */
        const arex_widget_style_t *style = arex_get_widget_style(w_id);
        uint8_t span_w = (style != NULL) ? style->span_w : 1;
        uint8_t span_h = (style != NULL) ? style->span_h : 1;

        /* 绾暟瀛︾粷瀵瑰潗鏍囨槧灏勶紙鍚?AREX_CARD_TITLE_H 鏍囬閬胯鍋忕Щ锛?*/
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
 * arex_5f_grid_rebuild 鈥?閲嶅缓 5F 鑷畾涔夌綉鏍?
 *
 * 鐢?arex_screen_rebuild_layout() 璋冪敤锛屽綋 BLE 涓嬪彂鏂扮殑 5F 甯冨眬鏃惰Е鍙戙€?
 * 鐩存帴鎿嶄綔 g_card_custom_objs[] 瀹瑰櫒鏁扮粍锛屾竻闄ゅ苟閲嶅缓鎵€鏈夌綉鏍肩粍浠躲€?
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
 * 鎸?widget_id 璁剧疆鏁板€硷紙鐢卞閮?update 寰幆璋冪敤锛?
 *
 * 鏋舵瀯锛?
 *   - 閬嶅巻 g_card_custom_obj 鍜?g_left_anchor_obj 涓や釜瀹瑰櫒
 *   - 鐢?user_data 鐑欏嵃鍖归厤 target_id
 *   - POD 浣跨敤楂樹綅鎺╃爜鏍囩 (1033=POD1, 2033=POD2)
 *
 * 绠楁硶锛?
 *   - DEPTH: 鐢?child[0]/child[1] 涓嬫爣璁块棶
 *   - POD: 閬嶅巻鏌ユ壘鏍囩 1033/2033
 *   - 鍏朵粬: 閬嶅巻瀛愯妭鐐规煡鎵?user_data 鍖归厤
 *
 * 缁濅笉瑙﹀彂浠讳綍閲嶇粯鎴栨帓鐗堥噸鏋勶紒鍙洿鏂?lv_label 鏂囧瓧銆?
 * ========================================================= */
void arex_widget_set_value(arex_widget_id_t id, float value)
{
    /* 淇 Bug #3锛氭坊鍔犳暟缁勮秺鐣屼繚鎶?*/
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
                /* 鍙繚鐣欎竴浣嶅皬鏁帮紝鑼冨洿 0-9 */
                float decimal_part = fabsf(value - di);
                int dd = (int)(decimal_part * 10 + 0.5f);
                if (dd > 9) dd = 9;  /* 闃叉娴偣绮惧害闂瀵艰嚧澶氫綅鏁?*/
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

            /* ===== POD 鍗曟ā鍏凤細鏁版嵁婧愭牴鎹?pod_index 鍔ㄦ€佸垎閰?=====
             * 娉ㄦ剰锛氱敱浜庡叧闂簡 ELEM_EXTRA锛孭OD 涓嶅啀鏈夌嫭绔嬬殑 ID 鏍囩瀛愬厓绱犮€?
             * 鏁板€?label 閫氳繃閫氱敤璺緞鍒涘缓锛屽叾 user_data = WIDGET_POD_0806銆?
             * 鍥犳鍙互绠€鍖栭€昏緫锛氱洿鎺ラ€氳繃 child_tag == WIDGET_POD_0806 鍖归厤鍗冲彲銆?
             * POD1/POD2 鐨勫尯鍒嗙敱娓叉煋鏃剁殑 pod_index 鍐冲畾锛屾洿鏂版椂鏃犻渶鍖哄垎銆?*/
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

            /* ===== 閫氱敤 widget锛氱敤 user_data == id 鍖归厤 ===== */
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
 * 鎸?widget_id 璁剧疆瀛楃涓诧紙鐢ㄤ簬 GAS 绛夐潪鏁板€肩粍浠讹級
 * ========================================================= */
void arex_widget_set_text(arex_widget_id_t id, const char *text)
{
    if (!text) return;

    /* 閬嶅巻鎵€鏈?5F 鍗＄墖瀹瑰櫒 + 宸︿晶閿氱偣 */
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
 * 鍏ㄥ眬缁勪欢鏁版嵁璺敱鍒嗗彂鍣?
 *
 * 鏋舵瀯璁捐锛氭鍑芥暟浣滀负鏁版嵁璺敱鎬绘満锛屾牴鎹?widget_id 鑷姩浠?g_sensor_data
 * 鍙栧€煎苟璋冪敤 arex_widget_set_value() 鎴?arex_widget_set_text() 鍒锋柊鐣岄潰銆?
 *
 * 浣跨敤鍦烘櫙锛?
 *   - arex_screen_refresh_all_widgets() 閬嶅巻鍏ㄩ噺 widget 璋冪敤姝ゅ嚱鏁?
 *   - update 浠诲姟鍦ㄦ敹鍒?DIRTY_ALL 鏃惰皟鐢ㄥ叏閲忓埛鏂?
 *   - 浠讳綍闇€瑕佸崟鐙埛鏂版煇涓粍浠舵暟鎹殑鍦烘櫙
 *
 * 娉ㄦ剰锛氬鏉傜姸鎬佹満缁勪欢锛圢DL_STOP/SYS/COMPASS/TISSUE锛夊凡鍦?update_task 涓?
 *       鏈変笓灞炲埛鏂伴€昏緫锛屾澶勪粎鍋氬厹搴曞鐞嗐€?
 * ========================================================= */
void arex_widget_sync_data(arex_widget_id_t w_id)
{
    char buf[32];

    switch (w_id)
    {
    /* =========================================================
     * 1. 鏍稿績椹荤暀鍖?& 澶嶆潅鐘舵€佹満 (杩欎簺鐢变笓灞炲嚱鏁板鐞嗭紝杩欓噷鍋氬厹搴?
     * ========================================================= */
    case WIDGET_NDL_STOP_1606:
    case WIDGET_COMPASS_1612:
    case WIDGET_TISSUE_GF_4012:
    case WIDGET_TISSUE_RAW_4012:
        /* 杩欎簺鏄寘鍚姩鐢?澶氬厓绱犵殑澶嶆潅鐘舵€佹満锛屽凡鍦?arex_ui_update_task 鏈変笓灞炲埛鏂伴€昏緫 */
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
     * 2. 娣卞害缁勪欢
     * ========================================================= */
    case WIDGET_DEPTH_1612:
    case WIDGET_DEPTH_1606:
        arex_widget_set_value(w_id, g_sensor_data.depth);
        break;

    /* =========================================================
     * 3. 娼滄按鏃堕棿锛圡M:SS 鏍煎紡鍖栵級
     * ========================================================= */
    case WIDGET_DIVE_TIME_1606:
        snprintf(buf, sizeof(buf), "%02d:%02d",
                 g_sensor_data.dive_time_s / 60,
                 g_sensor_data.dive_time_s % 60);
        arex_widget_set_text(w_id, buf);
        break;

    /* =========================================================
     * 4. 姘斾綋缁勪欢
     * ========================================================= */
    case WIDGET_GAS_1606:
        arex_widget_set_text(w_id, g_sensor_data.gas_name);
        break;

    /* =========================================================
     * 5. 鍩虹缁勪欢 (Basic)
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
        /* 鏍规嵁婵€娲绘皵浣撶储寮曢€夋嫨瀵瑰簲鐨?PPO2 鍊?*/
        arex_widget_set_value(w_id, g_sensor_data.ppo2[g_sensor_data.gas_active_idx]);
        break;

    /* =========================================================
     * 6. 鎶€鏈綔姘?(Tech Dive)
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
     * 7. 浼犳劅鍣?& 鎷撳睍 (Sensors)
     * ========================================================= */
    case WIDGET_HEADING_0806:
        arex_widget_set_value(w_id, (float)g_sensor_data.heading);
        break;

    case WIDGET_POD_0806:
        /* POD 鐢辩姸鎬佹満浣跨敤 user_data 闈跺悜鍒锋柊锛屾澶勫仛鍏滃簳 */
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
     * 8. 绌烘Ы浣嶅拰鏈煡 ID
     * ========================================================= */
    case WIDGET_EMPTY:
    default:
        break;
    }
}

/* =========================================================
 * 馃毃 闈跺悜鍛婅瑙﹀彂寮曟搸锛堟柊鐗堟湰锛氫粎璁剧疆鐘舵€侊紝鐢?50ms 瀹氭椂鍣ㄦ墽琛岄棯鐑侊級
 * ========================================================= */
void arex_trigger_alarm(arex_alarm_level_t level,
                        const char *eng_text,
                        arex_widget_id_t target_id)
{
    /* 濡傛灉宸叉湁娲昏穬鍛婅涓旀湭杈惧埌鏈€灏忔樉绀烘椂闂达紝閲嶇疆璁℃椂鍣紙鑰屼笉鏄拷鐣ワ級 */
    if (s_alarm_active && g_current_alarm_level != AREX_ALARM_NONE)
    {
        uint32_t elapsed = lv_tick_elaps(s_alarm_start_tick);
        if (elapsed < ALARM_MIN_DISPLAY_MS)
        {
            /* 浠嶅湪鏈€鐭樉绀烘湡鍐咃紝閲嶆柊璁℃椂 5 绉?*/
            s_alarm_start_tick = lv_tick_get();
            s_alarm_clear_armed = false;
            return;
        }
    }

    arex_show_alarm_banner(level, eng_text);  /* 1. 寮瑰嚭妯箙 */
    g_current_alarm_target = target_id;         /* 2. 閿佸畾闈跺績 */
    g_current_alarm_level = level;              /* 3. 閿佸畾绾у埆 */
    g_ui.alarm_pending_click = true;            /* 4. 瑕佹眰鐢ㄦ埛鍏堟搷浣滄墠鑳芥竻闄?*/
    s_alarm_start_tick = lv_tick_get();         /* 5. 璁板綍寮€濮嬫椂闂?*/
    s_alarm_active = true;                      /* 6. 鏍囪鍛婅娲昏穬 */
    s_alarm_clear_armed = false;                /* 7. 鏂板憡璀﹂渶瑕侀噸鏂扮‘璁?*/
}

/* =========================================================
 * 馃毃 娓呴櫎鎵€鏈夊憡璀︽牱寮忥紙50ms 瀹氭椂鍣ㄤ細鑷姩鎶婃牱寮忓鍘燂級
 * 鏂伴€昏緫锛氶€熷害闄嶅埌瀹夊叏鑼冨洿鍚庤嚜鍔ㄦ竻闄わ紝浣嗘渶灏戞樉绀?5 绉?
 * ========================================================= */
void arex_clear_all_alarm_styles(void)
{
    /* 妫€鏌ユ槸鍚︽弧瓒虫渶灏忔樉绀烘椂闂?*/
    uint32_t elapsed = lv_tick_elaps(s_alarm_start_tick);
    if (elapsed < ALARM_MIN_DISPLAY_MS)
    {
        return;  /* 鏈揪鍒版渶鐭樉绀烘湡锛屼笉娓呴櫎 */
    }

    /* 淇濆瓨娓呴櫎鍓嶇殑鐩爣 ID锛堢敤浜庢仮澶嶆椂绮剧‘瀹氫綅锛岄伩鍏嶈浼ゅ垎鍓茬嚎绛夐潪鍛婅鍏冪礌锛?*/
    s_last_alarm_target = g_current_alarm_target;

    if (s_alarm_banner)
    {
        lv_obj_add_flag(s_alarm_banner, LV_OBJ_FLAG_HIDDEN);  /* 钘忚捣妯箙 */
    }

    g_current_alarm_target = WIDGET_EMPTY;  /* 娓呴櫎闈跺績 */
    g_current_alarm_level = 0;
    s_alarm_active = false;                /* 鏍囪鍛婅宸叉竻闄?*/
    s_alarm_clear_armed = false;
}

bool arex_alarm_mark_clear_requested(void)
{
    s_alarm_clear_armed = true;
    return true;
}

/* =========================================================
 * 馃毃 鍛婅妯箙鏄剧ず锛圸-Order 鎻愭潈鍒?Safe Zone 鏈€椤跺眰锛?
 * ========================================================= */
void arex_show_alarm_banner(arex_alarm_level_t level, const char *eng_text)
{
    lv_obj_t *safe_zone = arex_get_safe_zone();
    if (!safe_zone) return;

    /* 鍔ㄦ€佹帹绠楀崱鐗囧尯瀹藉害锛屽苟鏍规嵁甯冨眬鏂瑰悜鍐冲畾妯箙鎵€鍦ㄤ晶 */
    int panel_gap = (int)(g_sys_config.gap_u * AREX_BASE_U);
    int card_canvas_w = (int)g_sys_config.safe_zone_w - (int)AREX_LEFT_ANCHOR_W - panel_gap;

    if (!s_alarm_banner)
    {
        s_alarm_banner = lv_obj_create(safe_zone);
        lv_obj_remove_style_all(s_alarm_banner);

        lv_obj_set_size(s_alarm_banner, card_canvas_w, 60);

        s_alarm_banner_lbl = lv_label_create(s_alarm_banner);
        lv_obj_set_style_text_font(s_alarm_banner_lbl, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
        lv_obj_align(s_alarm_banner_lbl, LV_ALIGN_LEFT_MID, 20, 0);
    }

    lv_obj_set_size(s_alarm_banner, card_canvas_w, 60);
    if (g_sys_config.layout_order == AREX_ORDER_NORMAL)
    {
        lv_obj_align(s_alarm_banner, LV_ALIGN_TOP_RIGHT, 0, 0);
    }
    else
    {
        lv_obj_align(s_alarm_banner, LV_ALIGN_TOP_LEFT, 0, 0);
    }

    /* 馃毃 缁堟瀬鏉€鎷涳細鎶婃í骞呭己琛屾媺鍒版墍鏈夊崱鐗囦箣涓婏紒缁濆闃查伄鎸★紒 */
    lv_obj_move_foreground(s_alarm_banner);
    lv_obj_clear_flag(s_alarm_banner, LV_OBJ_FLAG_HIDDEN);

    /* 娴呭簳娣卞瓧閰嶈壊 */
    lv_color_t bg_color = (level >= 3) ? AREX_LIGHT : AREX_LIGHT;  /* 閮芥槸娴呰壊鑳屾櫙 */
    lv_obj_set_style_bg_color(s_alarm_banner, bg_color, 0);
    lv_obj_set_style_bg_opa(s_alarm_banner, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_alarm_banner_lbl, AREX_BLACK, 0);

#if AREX_ALARM_SHOW_PREFIX
    /* 鎷兼帴鍓嶇紑 */
    const char *prefix = "INFO";
    if (level >= 3) prefix = "CRITICAL";
    else if (level >= 2) prefix = "WARNING";

    char full_text[128];
    snprintf(full_text, sizeof(full_text), "%s: %s", prefix, eng_text ? eng_text : "");
    lv_label_set_text(s_alarm_banner_lbl, full_text);
#else
    /* 鐩存帴鏄剧ず浼犲叆鐨勬枃瀛楋紝涓嶅甫鍓嶇紑 */
    lv_label_set_text(s_alarm_banner_lbl, eng_text ? eng_text : "");
#endif
}

/* =========================================================
 * 瀹氭椂鏁版嵁鏇存柊 (鐢?lv_timer 浠?1Hz/2Hz 璋冪敤)
 * 浠呮洿鏂?lv_label 鏂囧瓧锛岀粷涓嶈Е鍙戞帓鐗堥噸鏋?
 * ========================================================= */
void arex_ui_update_data(void)
{
    /* 鐢辫皟鐢ㄦ柟鍦?arex_screen.c 涓疄鐜板叿浣撶殑 lv_label_set_text 璋冪敤
     * 姝ゅ嚱鏁颁綔涓虹┖閽╁瓙瀛樺湪锛屼緵鏈潵鎵╁睍
     */
}

/* =========================================================
 * 11. Data Bus UI 娑堣垂浠诲姟 鈥?鍏ㄧ郴缁熷敮涓€鍏佽鎵ц lv_label_set_text 鐨勫湴鏂?
 *
 * 鏋舵瀯閾佸緥锛?
 *   - 纭欢宸ョ▼甯堬細鍙兘璋冪敤 arex_bus_set_*() 绯诲垪鍑芥暟锛堜粎鍐欐暟鎹?鎵撹剰鏍囪锛?
 *   - UI 宸ョ▼甯? 锛氬彧鑳戒慨鏀?arex_ui_update_task() 娑堣垂鑰?
 *   - 涓よ€呴€氳繃 g_sensor_data.dirty_mask 瀹屽叏瑙ｈ€?
 *
 * 鐢?lv_timer 椹卞姩锛屽缓璁?50ms 鍛ㄦ湡锛?0 FPS 瓒冲瑕嗙洊鎵€鏈変紶鎰熷櫒鍙樺寲锛?
 * ========================================================= */
void arex_ui_update_task(lv_timer_t *timer)
{
    (void)timer;

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
     * 馃毃 鍏ㄥ煙鍛婅闂儊寮曟搸 (Heartbeat Flasher)
     * ============================================================ */
    {
        static bool s_last_alarm_flash = false;

        if (g_current_alarm_target != WIDGET_EMPTY)
        {
            /* Level3 棰戠巼 500ms(2Hz)锛孡evel2 棰戠巼 1000ms(1Hz) */
            int interval = (g_current_alarm_level >= 3) ? 250 : 500;
            bool is_flash_on = (lv_tick_get() / interval) % 2 == 0;

            /* 鍙湁鐩镐綅鍒囨崲鏃舵墠鎿嶄綔 UI锛屾瀬澶у湴鑺傜渷 CPU */
            if (is_flash_on != s_last_alarm_flash)
            {
                s_last_alarm_flash = is_flash_on;

                lv_color_t bg_color = (g_current_alarm_level >= 3) ? AREX_LIGHT : AREX_LIGHT;
                lv_color_t txt_color = AREX_BLACK;

                /* 鐏浉浣嶆椂锛岄€€鍥炴櫘閫氱殑榛戝簳缁垮瓧 */
                if (!is_flash_on)
                {
                    bg_color = AREX_BLACK;
                    txt_color = AREX_GREEN;
                }

                /* 馃毃 鏍稿績淇锛氳椤堕儴鐨勬í骞呰窡鐫€涓€璧峰弽鑹查棯鐑侊紒 */
                if (s_alarm_banner && s_alarm_banner_lbl)
                {
                    lv_obj_set_style_bg_color(s_alarm_banner, bg_color, 0);
                    lv_obj_set_style_text_color(s_alarm_banner_lbl, txt_color, 0);
                }

                /* 馃毃 鍏ㄥ煙鎼滄崟锛氬悓鏃舵壂鎻忓乏渚ч敋鐐瑰拰鎵€鏈?5F 鍗＄墖锛?*/
                uint8_t max_count = (g_card_custom_obj_count < AREX_MAX_CUSTOM_CARDS)
                                    ? g_card_custom_obj_count : AREX_MAX_CUSTOM_CARDS;

                for (int c = 0; c <= max_count; c++)
                {
                    lv_obj_t *container = (c < max_count) ? g_card_custom_objs[c] : g_left_anchor_obj;
                    if (!container) continue;

                    for (int i = 0; i < lv_obj_get_child_cnt(container); i++)
                    {
                        lv_obj_t *child = lv_obj_get_child(container, i);
                        if ((uintptr_t)lv_obj_get_user_data(child) == g_current_alarm_target)
                        {

                            /* 鍛戒腑闈跺績锛佸疄鏂芥煋鑹叉墦鍑伙紒 */
                            lv_obj_set_style_bg_color(child, bg_color, 0);
                            lv_obj_set_style_bg_opa(child, is_flash_on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);

                            /* 璁╅噷闈㈢殑鏂囧瓧涓€璧峰弽鑹?*/
                            for (int j = 0; j < lv_obj_get_child_cnt(child); j++)
                            {
                                lv_obj_t *lbl = lv_obj_get_child(child, j);
                                if (lv_obj_check_type(lbl, &lv_label_class))
                                {
                                    lv_obj_set_style_text_color(lbl, txt_color, 0);
                                }
                            }
                        }
                    }
                }
            }
        }
        else if (s_last_alarm_flash)
        {
            /* 濡傛灉鎶ヨ琚竻闄や簡锛屼絾鍒氭墠杩樺湪浜潃锛岄渶瑕佺簿纭鍘熺洰鏍囨帶浠?*/
            s_last_alarm_flash = false;

            /* 妯箙涔熶竴璧峰鍘?*/
            if (s_alarm_banner && s_alarm_banner_lbl)
            {
                lv_obj_set_style_bg_color(s_alarm_banner, AREX_BLACK, 0);
                lv_obj_set_style_text_color(s_alarm_banner_lbl, AREX_GREEN, 0);
            }

            /* 鍙仮澶?s_last_alarm_target 鎸囧畾鐨勭洰鏍囨帶浠讹紙閬垮厤璇激鍒嗗壊绾跨瓑闈炲憡璀﹀厓绱狅級 */
            if (s_last_alarm_target != WIDGET_EMPTY)
            {
                /* 鎵弿鎵€鏈夊鍣紝绮剧‘鎭㈠鐩爣鎺т欢鐨勬牱寮?*/
                lv_obj_t *targets[3];
                uint8_t target_count = 0;
                if (g_left_anchor_obj) targets[target_count++] = g_left_anchor_obj;
                for (int c = 0; c < g_card_custom_obj_count && c < AREX_MAX_CUSTOM_CARDS; c++)
                {
                    if (g_card_custom_objs[c]) targets[target_count++] = g_card_custom_objs[c];
                }

                for (int tc = 0; tc < target_count; tc++)
                {
                    lv_obj_t *container = targets[tc];
                    for (int i = 0; i < lv_obj_get_child_cnt(container); i++)
                    {
                        lv_obj_t *child = lv_obj_get_child(container, i);
                        /* 绮剧‘鍖归厤鐩爣 ID */
                        if ((uintptr_t)lv_obj_get_user_data(child) == s_last_alarm_target)
                        {
                            /* 鎭㈠鐩爣鎺т欢锛氳儗鏅€忔槑 + 鏂囧瓧缁胯壊 */
                            lv_obj_set_style_bg_color(child, AREX_BLACK, 0);
                            lv_obj_set_style_bg_opa(child, LV_OPA_TRANSP, 0);
                            /* 璁╅噷闈㈢殑鏂囧瓧涔熸仮澶嶇豢鑹?*/
                            for (int j = 0; j < lv_obj_get_child_cnt(child); j++)
                            {
                                lv_obj_t *lbl = lv_obj_get_child(child, j);
                                if (lv_obj_check_type(lbl, &lv_label_class))
                                {
                                    lv_obj_set_style_text_color(lbl, AREX_GREEN, 0);
                                }
                            }
                            break;  /* 鎵惧埌鐩爣灏遍€€鍑猴紝鏃犻渶缁х画閬嶅巻 */
                        }
                    }
                }
            }
            s_last_alarm_target = WIDGET_EMPTY;  /* 娓呴櫎澶囦唤 */
        }
    }

    /* ============================================================
     * 馃毃 鏍稿績淇锛氱嫭绔嬩簬鏁版嵁鐨?鏃堕棿蹇冭烦寮曟搸"蹇呴』鏀惧湪鏈€鍓嶉潰锛?
     *
     * 鍗充娇娌℃湁浠讳綍鑴忔爣璁帮紝鍙澶勪簬杩愬姩鐘舵€?|rate|>=3.0 m/min)锛?
     * 鎴栬€呮湁娲昏穬鍛婅锛屾垜浠氨寮鸿娉ㄥ叆 DIRTY_DEPTH 鑴忔爣璁帮紝
     * 鍞ら啋 UI 寮曟搸鍘荤敾闂儊鍔ㄧ敾锛?
     * ============================================================ */
    {
        static bool last_flash_state = false;
        bool current_flash_state = (lv_tick_get() / 500) % 2 == 0;

        if (current_flash_state != last_flash_state)
        {
            last_flash_state = current_flash_state;

            float rate = g_sensor_data.ascent_rate;
            /* 鏈夋椿璺冨憡璀︽垨閫熷害瓒呰繃闈欐闃堝€兼椂锛屼繚鎸佸績璺冲埛鏂?*/
            if (s_alarm_active || fabsf(rate) >= AREX_RATE_STILL_THRESHOLD)
            {
                g_sensor_data.dirty_mask |= DIRTY_DEPTH;
            }
        }
    }

    uint32_t mask = arex_bus_take_dirty();
    if (mask == DIRTY_NONE) return;

    /* 鏈€楂樹紭鍏堢骇锛歎I 甯冨眬閲嶅缓锛圔LE 閰嶇疆鍚屾瑙﹀彂锛夈€?
     * 閲嶅缓鑰楁椂杈冮暱锛岄攣浣?LVGL invalidation 闃叉闂儊锛屾湰甯х洿鎺ラ€€鍑恒€?*/
    if (mask & DIRTY_UI_LAYOUT)
    {
        lv_disp_t *disp = lv_disp_get_default();
        if (disp) lv_disp_enable_invalidation(disp, false);
        arex_screen_rebuild_full();
        if (disp) lv_disp_enable_invalidation(disp, true);
        arex_bus_requeue_dirty(mask & ~DIRTY_UI_LAYOUT);
        return;
    }

    /* 娣卞害 + NDL + TTS + 缁勭粐鑸?鈥斺€?鍏ㄥ睆鍒锋柊锛堝寘鍚乏渚ч敋鐐?+ 5F 缃戞牸锛? 2F Deco 鍗＄墖鍒锋柊 */
    if (mask & (DIRTY_DEPTH | DIRTY_NDL | DIRTY_TTS | DIRTY_TISSUES))
    {
        arex_screen_refresh_all_widgets();  // 淇锛氭敼涓哄叏灞忓埛鏂帮紝纭繚 5F 缃戞牸涓殑 widget 涔熻兘鏇存柊
        card_deco_update();

        /* ============================================================
         * ============================================================
         * 閫熺巼鍥炬爣闂儊寮曟搸锛堢函姝?500ms 蹇冭烦椹卞姩锛屾棤瑙嗘繁搴︽姈鍔級
         * ============================================================ */
        if (s_ascent_icon_count > 0)
        {
            static int8_t s_last_direction = 0;  /* 0=闈欐, 1=涓婂崌, -1=涓嬮檷 */

            float rate = g_sensor_data.ascent_rate;
            bool is_moving = (fabsf(rate) >= AREX_RATE_STILL_THRESHOLD);

            /* 鑾峰彇 500ms 蹇冭烦鐩镐綅 */
            bool current_flash_state = (lv_tick_get() / 500) % 2 == 0;

            /* 鍒ゆ柇褰撳墠瀹為檯杩愬姩鏂瑰悜 */
            int8_t current_direction = 0;
            if (rate > 0.0f)
            {
                current_direction = 1;
            }
            else if (rate < 0.0f)
            {
                current_direction = -1;
            }

            /* 閫熷害闄嶅埌瀹夊叏鑼冨洿鍚庤嚜鍔ㄦ竻闄ゅ憡璀?*/
            if (!is_moving && s_alarm_active)
            {
                arex_clear_all_alarm_styles();
            }

            if (s_alarm_active && s_alarm_clear_armed &&
                    lv_tick_elaps(s_alarm_start_tick) >= ALARM_MIN_DISPLAY_MS)
            {
                arex_clear_all_alarm_styles();
            }

            const void *target_img_src = &sudo_up_level0;

            if (!is_moving)
            {
                /* 闈欐鐘舵€侊細涓嶉棯鐑侊紝淇濇寔鏈€鍚庝竴涓柟鍚戠殑 level0 */
                target_img_src = (s_last_direction > 0) ? &sudo_up_level0 :
                                 (s_last_direction < 0) ? &sudo_down_level0 : &sudo_up_level0;
            }
            else
            {
                /* 杩愬姩鐘舵€侊細鏍规嵁 500ms 鑺傛媿杩涜鍛煎惛闂儊 */
                if (current_direction > 0)
                {
                    /* 涓婂崌鏂瑰悜 */
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
                    /* 涓嬮檷鏂瑰悜 */
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

            /* 鏇存柊鏈€鍚庣殑鏂瑰悜鐘舵€?*/
            if (current_direction != 0)
            {
                s_last_direction = current_direction;
            }

            /* 鍚屾鍒锋柊鎵€鏈夊浘鏍?*/
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
     * NDL_STOP 澶氬舰鎬佺姸鎬佹満锛歂DL甯告€?/ Safety鍋滅暀 / Deco鍋滅暀
     * 鏍规嵁 g_sensor_data.stop_type 鐬棿鍒囨崲鎵€鏈夊瓙缁勪欢鐨勬樉闅愩€佷綅缃拰瀛楀彿
     * 閬嶅巻鏁扮粍锛屽悓姝ュ埛鏂版墍鏈?NDL 瀹炰緥锛堝乏渚ч敋鐐?+ 5F 澶氫釜锛?
     * ============================================================ */
    if (s_ndl_handle_count > 0 && (mask & (DIRTY_NDL_STOP | DIRTY_DEPTH | DIRTY_NDL)))
    {
        /* 瀹炴椂鏌ヨ〃鑾峰彇鏍峰紡瀛楀吀 */
        const arex_widget_style_t *style = arex_get_widget_style(WIDGET_NDL_STOP_1606);
        if (!style) return;

        const arex_style_ndl_stop_t *s = &style->spec.ndl_stop;

        for (int i = 0; i < s_ndl_handle_count; i++)
        {
            ndl_handle_t *h = &s_ndl_handles[i];

            /* 鏃犺浣曠鐘舵€侊紝鍗佸鏍兼案杩滃父椹绘樉绀?*/
            lv_obj_clear_flag(h->horiz_bg, LV_OBJ_FLAG_HIDDEN);
            /* 鍙鏁版嵁鍙樹簡锛屽氨瑙﹀彂鍗佸鏍奸噸鏂版墽琛岀粯鍒惰绠?*/
            lv_obj_invalidate(h->horiz_bg);

            /* ========== 鐘舵€?1: 甯告€?NDL 妯″紡 ========== */
            if (g_sensor_data.stop_type == AREX_STOP_NONE)
            {
                lv_obj_add_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);  /* 椤堕儴闅愯棌 */
                lv_obj_clear_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN);

                /* NDL 瀛楁牱灞呭乏 */
                lv_label_set_text(h->sub_bot, "NDL");
                lv_obj_align(h->sub_bot, LV_ALIGN_LEFT_MID, 8, -6);

                /* NDL涓撶敤瀛椾綋 48px 鏁板瓧灞呭彸 */
                lv_obj_set_style_text_font(h->main_val, arex_get_font(AREX_FONT_ID_NDL), 0);
                lv_label_set_text_fmt(h->main_val, "%d", g_sensor_data.ndl);
                lv_obj_align(h->main_val, LV_ALIGN_CENTER, 0, -8);
            }
            /* ========== 鐘舵€?2: 瀹夊叏鍋滅暀妯″紡 ========== */
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
                lv_obj_align(h->sub_bot, LV_ALIGN_BOTTOM_LEFT, 8, -16); /* 鎮仠鍦?10 瀹牸涓婃柟 */

                /* 澶у瓧浣?64px 鏁板瓧灞呭彸 */
                int m = g_sensor_data.stop_time_left_s / 60;
                int s = g_sensor_data.stop_time_left_s % 60;
                lv_obj_set_style_text_font(h->main_val, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
                lv_label_set_text_fmt(h->main_val, "%d:%02d", m, s);
                lv_obj_align(h->main_val, LV_ALIGN_RIGHT_MID, -4, -6);
            }
            /* ========== 鐘舵€?3: 鍑忓帇鍋滅暀妯″紡 ========== */
            else if (g_sensor_data.stop_type == AREX_STOP_DECO)
            {
                lv_obj_clear_flag(h->title_top, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(h->sub_bot, LV_OBJ_FLAG_HIDDEN); /* DECO 涓嬮殣钘?NDL 鍓爣棰?*/

                lv_label_set_text_fmt(h->title_top, "DECO %dm", (int)g_sensor_data.stop_depth_m);
                lv_obj_align(h->title_top, LV_ALIGN_TOP_LEFT, 8, 2);

                /* 澶у瓧浣?64px 鏁板瓧灞呭彸 */
                int m = g_sensor_data.stop_time_left_s / 60;
                int s = g_sensor_data.stop_time_left_s % 60;
                lv_obj_set_style_text_font(h->main_val, arex_get_font(AREX_FONT_ID_MEDIUM), 0);
                lv_label_set_text_fmt(h->main_val, "%d:%02d", m, s);
                lv_obj_align(h->main_val, LV_ALIGN_RIGHT_MID, -4, -6);
            }
        }
    }

    /* 姘旂摱鍘嬪姏 鈥斺€?鍏ㄥ睆鍒锋柊锛岀‘淇?5F 缃戞牸涓殑 POD 鍚屾鏇存柊 */
    if (mask & DIRTY_POD)
    {
        arex_screen_refresh_all_widgets();
    }

    /* 鐢垫睜鍒锋柊 鈥斺€?鏁版嵁椹卞姩缃戞牸鑷姩鏇存柊 */
    if (mask & DIRTY_BATT)
    {
        arex_widget_set_value(WIDGET_BATTERY_0806, g_sensor_data.battery_pct);
    }

    /* 缃楃洏鑸悜 鈥?闆跺唴瀛樻暟瀛﹀紩鎿庯紝瑙﹀彂 invalidate + 鏇存柊鏍囩 */
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
        /* 鏇存柊鍗峰昂涓嬫柟鐨勫法鍨嬫枃瀛?*/
        if (s_heading_val_lbl)
        {
            lv_label_set_text_fmt(s_heading_val_lbl, "%03d", g_sensor_data.heading);
        }
        /* 瑙﹀彂鍗峰昂鐢绘澘鐨勫簳灞傛暟瀛﹂噸缁橈紙鏋佸叾杞婚噺锛?*/
        if (s_compass_tape_obj)
        {
            lv_obj_invalidate(s_compass_tape_obj);
        }
        /* 濡傛灉鏈夐攣瀹氾紝鏇存柊鎻愮ず鏂囨湰 */
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

    /* 娼滄按鏃堕棿 + W.TIME 鈥斺€?鍏ㄥ睆鍒锋柊锛岀‘淇?5F 缃戞牸涓殑鏃堕棿缁勪欢鍚屾鏇存柊 */
    if (mask & DIRTY_DIVE_TIME)
    {
        arex_screen_refresh_all_widgets();
    }

    /* PO2 鍊?鈥斺€?鍏ㄥ睆鍒锋柊锛岀‘淇?5F 缃戞牸涓殑 PPO2 缁勪欢鍚屾鏇存柊 */
    if (mask & DIRTY_PPO2)
    {
        arex_screen_refresh_all_widgets();
    }

    /* 姘斾綋鍒囨崲 */
    if (mask & DIRTY_GAS)
    {
        arex_screen_refresh_gas_menu();
        arex_screen_refresh_all_widgets();
    }

    /* 娼滄按杞ㄨ抗+鍑忓帇绔欏浘琛ㄥ埛鏂?
     * 杞ㄨ抗杩藉姞鐢辫皟鐢ㄦ柟鍦?sim_tick_cb 涓洿鎺ヨ皟鐢?arex_dive_log_append锛?
     * 姝ゅ浠呰礋璐ｅ埛鏂板浘琛紙鍙?AREX_DECO_REFRESH_MS 鑺傛祦淇濇姢锛?*/
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

    /* CNS 姘т腑姣?鈥斺€?2F Deco 鍗＄墖 + 5F 缃戞牸 */
    if (mask & DIRTY_CNS)
    {
        card_deco_update();
        arex_screen_refresh_all_widgets();
    }

    /* OTU 姘т腑姣?鈥斺€?2F Deco 鍗＄墖 + 5F 缃戞牸 */
    if (mask & DIRTY_OTU)
    {
        card_deco_update();
        arex_screen_refresh_all_widgets();
    }

    /* 娓╁害鍒锋柊 鈥斺€?鏁版嵁椹卞姩缃戞牸鑷姩鏇存柊 */
    if (mask & DIRTY_TEMP)
    {
        arex_widget_set_value(WIDGET_TEMP_0806, g_sensor_data.temperature_c);
    }

    /* 娣卞害/娓╁害缁熻鍒锋柊 鈥斺€?鏈€澶?骞冲潎/鏈€浣庨殢涓绘暟鎹悓姝ユ洿鏂?*/
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

    /* 鎶€鏈綔姘村弬鏁板埛鏂?鈥斺€?鍏ㄥ睆鍒锋柊锛堝寘鍚乏渚ч敋鐐?+ 5F 缃戞牸锛?*/
    if (mask & (DIRTY_GF_SETTING | DIRTY_MOD | DIRTY_CEILING | DIRTY_GAS_MIX | DIRTY_GAS_DENS | DIRTY_FIO2))
    {
        arex_screen_refresh_all_widgets();
    }

    /* ============================================================
     * O(1) SYS_1606 鍏ㄦā鍧楁瀬閫熺偣瀵圭偣鍒锋柊
     * 鐩存帴鎿嶄綔闈欐€佹寚閽堬紝缁濅笉閬嶅巻 UI 鏍戯紒
     * ============================================================ */
    if (mask & (DIRTY_BATT | DIRTY_TEMP))
    {
        /* 1. 鐢甸噺鐧惧垎姣?*/
        if (mask & DIRTY_BATT)
        {
            if (s_sys_batt_lbl)
            {
                lv_label_set_text_fmt(s_sys_batt_lbl, "%u%%", arex_ui_clamp_battery_pct(g_sensor_data.battery_pct));
            }
        }
        /* 2. 娓╁害 */
        if (mask & DIRTY_TEMP)
        {
            if (s_sys_temp_lbl)
            {
                /* 鏁存暟鎷兼帴娉曠粫杩?%f 闄愬埗锛屽畬缇庢樉绀?26.5 C */
                int t_int = (int)g_sensor_data.temperature_c;
                int t_dec = (int)(fabsf(g_sensor_data.temperature_c - t_int) * 10);
                lv_label_set_text_fmt(s_sys_temp_lbl, "%d.%d C", t_int, t_dec);
            }
        }
        /* 璁惧鐘舵€佸浘鏍囧埛鏂颁唬鐮佸凡绉婚櫎锛堝浘鏍囧凡鍒犻櫎锛?*/
    }

    /* ============================================================
     * 馃毃 鍛婅寰呭鐞嗘爣蹇楃粺涓€澶勭悊
     * 鐢?arex_bus_set_depth 绛夊嚱鏁拌缃爣蹇椾綅锛屽湪姝ょ粺涓€瑙﹀彂
     * ============================================================ */
    if (g_alarm_pending)
    {
        g_alarm_pending = false;
        arex_trigger_alarm(g_pending_alarm_level,
                           g_pending_alarm_text,
                           g_pending_alarm_target);
    }

}

/* =========================================================
 * 12. 宸︿晶 2x6 缁濆缃戞牸娓叉煋寮曟搸
 *
 * 涓ユ牸灏?160x360 鍖哄煙鍒掑垎涓?2鍒?80px) x 6琛?60px) 鐨勭粷瀵圭綉鏍肩煩闃碉紝
 * 褰诲簳搴熷純 current_y 绱姞鎺掔増锛屾敼鐢?x*y*w*h 绾暟瀛﹀潗鏍囨帹婕斻€?
 * SystemData 搴曢儴 60px 鐢?WIDGET_SYS_1606 缁勪欢鍖栨覆鏌撱€?
 * ========================================================= */

/* 宸︿晶缃戞牸鎬荤嚎娓叉煋鍣細閬嶅巻 g_sys_config.left_widgets[] 鏁扮粍锛?
 * 鐢ㄧ函鏁板 cell_w * cell_h 鎺ㄧ畻缁濆鍧愭爣骞舵覆鏌撴墍鏈夌粍浠躲€?
 * left_anchor 浼犲叆鐢ㄤ簬鍛婅寮曟搸璺ㄥ尯鎼滅储鐑欏嵃瀵硅薄銆?*/
void arex_render_left_anchor_grid(lv_obj_t *left_anchor)
{
    if (!left_anchor) return;

    /* 娉ㄥ叆澶栭儴瀹瑰櫒锛堜緵鍛婅寮曟搸璺ㄥ尯鎼滅储鐑欏嵃瀵硅薄锛?*/
    g_left_anchor_obj = left_anchor;

    /* 娉ㄦ剰锛氫笉鍗曠嫭娓呯┖ s_img_ascent_rate[] / s_ndl_handles[]锛?
     * 瀹冧滑宸茬粡鍦?arex_screen_rebuild_layout() 鍏ュ彛缁熶竴娓呯┖浜嗐€?
     * 杩欓噷鍙渶瑕佽拷鍔犲乏渚ч敋鐐圭殑 widget 鎸囬拡鍗冲彲锛堣拷鍔犳ā寮忥級銆?*/

    /* 鍩哄噯缃戞牸鍗曞厓锛?鍒?x 6琛岋紝姣忔牸 80x60 */
    const uint16_t cell_w = AREX_LEFT_CELL_W;   /* 80px */
    const uint16_t cell_h = AREX_LEFT_CELL_H;   /* 60px */

    /* 閬嶅巻骞舵覆鏌撳熀浜庣綉鏍肩殑缁勪欢 */
    for (uint8_t i = 0; i < g_sys_config.left_widget_count && i < AREX_LEFT_MAX_WIDGETS; i++)
    {
        arex_grid_widget_t *cfg = &g_sys_config.left_widgets[i];
        if (cfg->widget_id == WIDGET_EMPTY) continue;

        /* 浠庢牱寮忚〃鏌ヨ〃鑾峰彇璺ㄥ害淇℃伅 */
        const arex_widget_style_t *style = arex_get_widget_style(cfg->widget_id);
        uint8_t span_w = (style != NULL) ? style->span_w : 1;
        uint8_t span_h = (style != NULL) ? style->span_h : 1;

        /* 缁濆鐗╃悊鍧愭爣鎺ㄦ紨锛歝ol * cell_w, row * cell_h */
        int16_t  abs_x = (int16_t)(cfg->x * cell_w);
        int16_t  abs_y = (int16_t)(cfg->y * cell_h);
        uint16_t abs_w = span_w * cell_w;
        uint16_t abs_h = span_h * cell_h;

        /* 璋冪敤搴曞眰宸ュ巶锛氶€熺巼鍥炬爣鐢卞伐鍘傝嚜涓绘煡瀛楀吀鍐冲畾 */
        render_widget_by_id(left_anchor, cfg->widget_id,
                            abs_x, abs_y, abs_w, abs_h,
                            span_w, span_h, (arex_font_id_t)255);
    }

    /* 宸︿晶妯嚎鎸夌粍浠惰竟鐣岀粯鍒讹細鍙湪鐪熷疄鐨勪笂涓嬩袱涓粍浠朵箣闂寸敾绾?*/
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
 * 绗簲姝ワ細鏂扮畝鍖栧伐鍘傚嚱鏁帮紙APP涓嬪彂浣嶇疆 + MCU鏈湴鏌ユ牱寮忚〃锛?
 *
 * 鏋舵瀯閾佸緥锛欰PP 鍙笅鍙?[widget_id, x, y]锛孧CU 鏍规嵁 widget_id
 * 鑷姩浠庢牱寮忔敞鍐岃〃鑾峰彇 w/h/offset锛屾覆鏌撴椂缁勫悎涓よ€呫€?
 * ========================================================= */
lv_obj_t* arex_render_widget(lv_obj_t *parent,
                             const arex_widget_pos_t *pos,
                             uint16_t cell_w, uint16_t cell_h,
                             uint16_t title_h)
{
    if (!parent || !pos) return NULL;
    if (pos->widget_id == WIDGET_EMPTY) return NULL;

    /* 1. 鏌ユ湰鍦版牱寮忚〃 */
    const arex_widget_style_t *style = arex_get_widget_style(pos->widget_id);
    if (!style)
    {
        /* 瀹归敊锛氭湭鐭D锛屽皾璇曠敤閫氱敤鏂瑰紡娓叉煋 */
        lv_obj_t *comp = lv_obj_create(parent);
        lv_obj_remove_style_all(comp);
        int16_t ax = (int16_t)(pos->x * cell_w);
        int16_t ay = (int16_t)(pos->y * cell_h) + title_h;
        lv_obj_set_pos(comp, ax, ay);
        lv_obj_set_size(comp, cell_w, cell_h);
        return comp;
    }

    /* 2. 鎺ㄧ畻缁濆鐗╃悊鍧愭爣 */
    int16_t  abs_x = (int16_t)(pos->x * cell_w);
    int16_t  abs_y = (int16_t)(pos->y * cell_h) + title_h;
    uint16_t abs_w = (uint16_t)(style->span_w * cell_w);
    uint16_t abs_h = (uint16_t)(style->span_h * cell_h);

    /* 3. 鐩存帴璋冪敤搴曞眰宸ュ巶锛堥€熺巼鍥炬爣鐢卞伐鍘傝嚜涓绘煡瀛楀吀鍐冲畾锛?*/
    return render_widget_by_id(parent, pos->widget_id,
                               abs_x, abs_y, abs_w, abs_h,
                               style->span_w, style->span_h,
                               (arex_font_id_t)255);
}
