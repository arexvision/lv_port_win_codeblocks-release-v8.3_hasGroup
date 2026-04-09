/**
 ******************************************************************************
 * @file    lv_100ask_demo_course_x_1_1.c
 * @author  百问科技
 * @version V1.0
 * @date    2023-08-29
 * @brief	x_1_1 课堂代码
 ******************************************************************************
 * Change Logs:
 * Date           Author          Notes
 * 2023-08-29     zhouyuebiao     First version
 ******************************************************************************
 * @attention
 *
 * Copyright (C) 2008-2023 深圳百问网科技有限公司<https://www.100ask.net>
 * All rights reserved
 *
 ******************************************************************************
 */


/*********************
 *      INCLUDES
 *********************/
#include "../../lv_100ask_teach_demos.h"

#if LV_USE_100ASK_DEMO_COURSE_X_1_1 && LV_USE_FREETYPE

#include "lv_100ask_demo_course_x_1_1.h"


/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
#define  FONTAWESOME_SYMBOL_USB     "\xef\x8a\x87"

/**********************
 *  STATIC VARIABLES
 **********************/


/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/
LV_FONT_DECLARE(my_fontawesome_30);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/* ！！！运行前的操作步骤！！！ */
/*  需要先在 lv_conf.h 中打开  LV_USE_FREETYPE
 *  然后观看视频，或者阅读相应的PPT进行配置之后方可编译通过！
 *
 *  参考解决办法：https://forums.100ask.net/t/topic/4321
 *  遇到问题请在这里留言：https://forums.100ask.net/t/topic/4321
 */

void lv_100ask_demo_course_x_1_1(void)
{
    lv_obj_t * label = lv_label_create(lv_scr_act());

    // 使用新添加的图标（symbol）
    lv_obj_set_style_text_font(label, &my_fontawesome_30, 0);
    lv_label_set_text(label, FONTAWESOME_SYMBOL_USB);

    // 使用内置的图标（symbol）
    //lv_obj_set_style_text_font(label, &lv_font_montserrat_8, 0);
    //lv_label_set_text(label, LV_SYMBOL_OK LV_SYMBOL_WIFI LV_SYMBOL_PLAY"\n\nhttp://lvgl.100ask.net");

    lv_obj_center(label);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/


#endif /* LV_USE_100ASK_DEMO_COURSE_X_1_1 */
