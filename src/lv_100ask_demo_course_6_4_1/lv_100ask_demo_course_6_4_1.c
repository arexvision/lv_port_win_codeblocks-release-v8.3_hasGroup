/**
 ******************************************************************************
 * @file    lv_100ask_demo_course_6_4_1.c
 * @author  百问科技
 * @version V1.0
 * @date    2023-08-24
 * @brief	6_4_1 课堂代码
 ******************************************************************************
 * Change Logs:
 * Date           Author          Notes
 * 2023-08-24     zhouyuebiao     First version
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

#if LV_USE_100ASK_DEMO_COURSE_6_4_1 && LV_USE_FREETYPE

#include "lv_100ask_demo_course_6_4_1.h"


/*********************
 *      DEFINES
 *********************/
//#define FREETYPE_FONT_FILE ("D:/Lato-Regular.ttf")
#define FREETYPE_FONT_FILE ("D:/SourceHanSansCN-Bold-2.otf")

/**********************
 *      TYPEDEFS
 **********************/


/**********************
 *  STATIC VARIABLES
 **********************/


/**********************
 *  STATIC PROTOTYPES
 **********************/


/**********************
 *   GLOBAL FUNCTIONS
 **********************/
// 需要先在 lv_conf.h 中打开  LV_USE_FREETYPE

void lv_100ask_demo_course_6_4_1(void)
{
    /*Create a font*/
    static lv_ft_info_t info;
    /*FreeType uses C standard file system, so no driver letter is required.*/
    info.name = FREETYPE_FONT_FILE;
    info.weight = 24;
    info.style = FT_FONT_STYLE_NORMAL;
    info.mem = NULL;
    if(!lv_ft_font_init(&info)) {
        LV_LOG_ERROR("create failed.");
    }

    /*Create style with the new font*/
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_font(&style, info.font);

    /*Create a label with the new style*/
    lv_obj_t * label = lv_label_create(lv_scr_act());
    lv_obj_add_style(label, &style, 0);
    lv_label_set_text(label, "Hello LVGL\nI'm a font created with FreeType\n\n"\
                             "你好，LVGL\n我是用FreeType创建的字体。\n\n"\
                             "你好，百问网（https://www.100ask.net）！");
    lv_obj_center(label);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/


#endif /* LV_USE_100ASK_DEMO_COURSE_6_4_1 */
