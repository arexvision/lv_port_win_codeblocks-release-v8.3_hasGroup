/**
 ******************************************************************************
 * @file    lv_100ask_demo_course_3_14_2.c
 * @author  百问科技
 * @version V1.0
 * @date    2022-08-01
 * @brief	3_14_2 课的课堂代码
 ******************************************************************************
 * Change Logs:
 * Date           Author          Notes
 * 2022-08-01     zhouyuebiao     First version
 ******************************************************************************
 * @attention
 *
 * Copyright (C) 2008-2022 深圳百问网科技有限公司<https://www.100ask.net>
 * All rights reserved
 *
 ******************************************************************************
 */


/*********************
 *      INCLUDES
 *********************/
#include "../../lv_100ask_teach_demos.h"

#if LV_USE_100ASK_DEMO_COURSE_3_14_2

#include <string.h>
#include "lv_100ask_demo_course_3_14_2.h"


/*********************
 *      DEFINES
 *********************/
/* 创建的按钮个数 */
#define ITEMS_SUM 	(100)

/**********************
 *      TYPEDEFS
 **********************/


/**********************
 *  STATIC VARIABLES
 **********************/


/**********************
 *  STATIC PROTOTYPES
 **********************/
/* 释放屏幕上的所有组件(obj)，对比内存碎片的状况 */
static void free_obj_event_handler(lv_event_t * e);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_100ask_demo_course_3_14_2(void)
{
#if 1
	/* 创建普通的按钮(btn) */
	lv_obj_t * btn;
	lv_obj_t * label;

	lv_obj_set_flex_flow(lv_scr_act(), LV_FLEX_FLOW_ROW_WRAP);
	for(uint32_t i = 0; i < ITEMS_SUM; i++) {
		btn = lv_btn_create(lv_scr_act());
		label = lv_label_create(btn);
		lv_obj_center(label);

		/* 点击btnm上的任意一个按钮就会执行删除操作，将屏幕上的所有组件删除，然后查看内存碎片状况 */
		lv_obj_add_event_cb(btn, free_obj_event_handler, LV_EVENT_CLICKED, NULL);
	}

#else
	/* 创建按钮矩阵(btnm) */
	/* 实际创建出来的btnm会比指定的ITEMS_SUM少 ITEMS_SUM/x 个，x是btnm的换行行数。
	   这对我们的试验影响可以忽略不计
	*/
	static char * btnm_map[ITEMS_SUM];
	char text[] = "text";
	size_t text_size = strlen(text) + 1;

	for(uint32_t i = 0; i < ITEMS_SUM; i++) {
		btnm_map[i] = lv_mem_alloc(text_size);

		if((i != 0) && ((i+1) % 10) == 0)
			strcpy(btnm_map[i], "\n");
		else
			strcpy(btnm_map[i], text);

        printf("%d\n", i);
	}

	/* 最后一项必须是 NULL 或者 ""
	   否则就算不报错，btnm的显示会异常。
	*/
	btnm_map[ITEMS_SUM] = NULL;

	lv_obj_t * btnm = lv_btnmatrix_create(lv_scr_act());
	lv_obj_set_size(btnm, 1024, 600);
	lv_btnmatrix_set_map(btnm, btnm_map);

	/* 点击btnm上的任意一个按钮就会执行删除操作，将屏幕上的所有组件删除，然后查看内存碎片状况 */
	lv_obj_add_event_cb(btnm, free_obj_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

#endif

}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* 释放屏幕上的所有组件(obj)，对比内存碎片的状况 */
static void free_obj_event_handler(lv_event_t * e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * obj = lv_event_get_target(e);

	/* 删除(释放)在屏幕上创建的所有组件(obj) */
	lv_obj_clean(lv_scr_act());
}

#endif /* LV_USE_100ASK_DEMO_COURSE_3_14_2 */
