/**
 ******************************************************************************
 * @file    lv_100ask_demo_course_3_14_1.c
 * @author  百问科技
 * @version V1.0
 * @date    2022-07-18
 * @brief	3_14_1 课的课堂代码
 ******************************************************************************
 * Change Logs:
 * Date           Author          Notes
 * 2022-07-18     zhouyuebiao     First version
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

#if LV_USE_100ASK_DEMO_COURSE_3_14_1

#include <string.h>
#include "lv_100ask_demo_course_3_14_1.h"


/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/


/**********************
 *  STATIC VARIABLES
 **********************/


/**********************
 *  STATIC PROTOTYPES
 **********************/
static void btnm_event_handler(lv_event_t * e);


/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void lv_100ask_demo_course_3_14_1(void)
{

	/* 分配按钮以及按钮上的文字 */
	// 这里注意不要使用在函数退出时会被销毁的局部变量
	// 可以用 "\n" 进行换行，最后一个按钮之后的元素必须是 NULL 或 ""
	// 换行 "\n" 和 "NULL" 或 "" 不计入实际的按钮个数，不占据按钮展示空间
	static const char * map[] = {"btn11", "btn22", "btn11", "\n", "btn11", "btn55", NULL};
	// 同时设置按钮的颜色
	//static const char * map[] = {"#ff3a90 btn11#", "btn22", "#ff3a90 btn33#", "\n", "#ff3a90 btn44#", "btn55", NULL};

	/* 设置控制按钮的映射表 */
	// 不需要考虑上面的换行和最后的 NULL 或 ""
	// 索引 0 对应第一个按钮，以此类推。
	static lv_btnmatrix_ctrl_t ctrl_map[] = {LV_BTNMATRIX_CTRL_RECOLOR, LV_BTNMATRIX_CTRL_CHECKABLE, LV_BTNMATRIX_CTRL_CHECKED,\
											 1 | LV_BTNMATRIX_CTRL_RECOLOR | LV_BTNMATRIX_CTRL_CHECKABLE, LV_BTNMATRIX_CTRL_CHECKABLE};


	/* 创建一个按钮矩阵组件 */
	lv_obj_t * btnm = lv_btnmatrix_create(lv_scr_act());

	/* 设置按钮矩阵上的按钮 */
	// 如果不设置将使用默认的5个按钮
	//lv_btnmatrix_set_map(btnm, map);

	/* 单独设置某个按钮的宽度 */
	// 按钮的索引值从 0 开始
	// 如果一行有3个按钮，默认是每个按钮都是1也就是平均分配宽度
	// 如果2个按钮设置了 2 ，那么总比例变成 1+2+1=4 ，
	// 这时候按钮宽度分配占比： 1/4、 2/4、 1/4
	// 按钮的宽度比例的最大值不能大于 7 也就是只能分配 1-7
	//lv_btnmatrix_set_btn_width(btnm, 1, 2);
	//lv_btnmatrix_set_btn_width(btnm, 3, 2);

	/* 单独设置某个按钮的控制属性 */
	// 上面的宽度设置也属于控制属性
	// 按钮的索引值从 0 开始
	//lv_btnmatrix_set_btn_ctrl(btnm, 0, LV_BTNMATRIX_CTRL_HIDDEN);	// 隐藏按钮，这只是在屏幕上看不见了，还是占用了内存空间
	//lv_btnmatrix_set_btn_ctrl(btnm, 0, LV_BTNMATRIX_CTRL_NO_REPEAT);	// 不要重复按此按钮，这样就不能像长按删除按钮会一直执行删除字符的效果，智能按一次删除一个字符
	//lv_btnmatrix_set_btn_ctrl(btnm, 0, LV_BTNMATRIX_CTRL_DISABLED);	// 禁用这个按钮，可见，但是在屏幕上不能对其进行操作
	//lv_btnmatrix_set_btn_ctrl(btnm, 0, LV_BTNMATRIX_CTRL_CHECKABLE);	// 按钮可以切换(状态)
	//lv_btnmatrix_set_btn_ctrl(btnm, 0, LV_BTNMATRIX_CTRL_CHECKED);	// 按钮被点击时会被标记，这样我们就可以知道哪些按钮被点击过
	//lv_btnmatrix_set_btn_ctrl(btnm, 0, LV_BTNMATRIX_CTRL_CLICK_TRIG); // 按钮被点击(CLICK)的时候就发送 LV_EVENT_VALUE_CHANGE，不设置时按钮被按下(PRESS)时发送 LV_EVENT_VALUE_CHANGE，默认不设置
	//lv_btnmatrix_set_btn_ctrl(btnm, 0, LV_BTNMATRIX_CTRL_POPOVER);	// 按下此键时显示弹出框，在用户触点之上提示当前按下的按钮是哪一个
	//lv_btnmatrix_set_btn_ctrl(btnm, 0, LV_BTNMATRIX_CTRL_RECOLOR);	// 文本重新上色，这样可以给按钮设置颜色
	//lv_btnmatrix_set_btn_ctrl(btnm, 0, _LV_BTNMATRIX_CTRL_RESERVED);	// LVGL 备用按钮(功能待开发)

	// 同时添加多个属性，第一个属性值为 2 的意思是设置它的宽度占比为2
	//lv_btnmatrix_set_btn_ctrl(btnm, 0, 2 | LV_BTNMATRIX_CTRL_CHECKED | LV_BTNMATRIX_CTRL_NO_REPEAT | LV_BTNMATRIX_CTRL_RECOLOR);

	/* 一次设置所有按钮的控制属性 */
	//lv_btnmatrix_set_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_HIDDEN);		// 隐藏按钮，这只是在屏幕上看不见了，还是占用了内存空间
	//lv_btnmatrix_set_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_NO_REPEAT);		// 不要重复按此按钮，这样就不能像长按删除按钮会一直执行删除字符的效果，智能按一次删除一个字符
	//lv_btnmatrix_set_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_DISABLED);		// 禁用这个按钮，可见，但是在屏幕上不能对其进行操作
	//lv_btnmatrix_set_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_CHECKABLE);	// 按钮可以切换(状态)
	//lv_btnmatrix_set_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_CHECKED);		// 按钮被点击时会被标记，这样我们就可以知道哪些按钮被点击过
	//lv_btnmatrix_set_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_CLICK_TRIG);	// 按钮被点击(CLICK)的时候就发送 LV_EVENT_VALUE_CHANGE，不设置时按钮被按下(PRESS)时发送 LV_EVENT_VALUE_CHANGE，默认不设置
	//lv_btnmatrix_set_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_POPOVER);		// 按下此键时显示弹出框，在用户触点之上提示当前按下的按钮是哪一个
	//lv_btnmatrix_set_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_RECOLOR);		// 文本重新上色，这样可以给按钮设置颜色
	//lv_btnmatrix_set_btn_ctrl_all(btnm, _LV_BTNMATRIX_CTRL_RESERVED);		// LVGL 备用按钮(功能待开发)
	
	// 同时添加多个属性
	//lv_btnmatrix_set_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_CHECKED | LV_BTNMATRIX_CTRL_NO_REPEAT | LV_BTNMATRIX_CTRL_RECOLOR);

	/* 单独清除某个按钮的控制属性 */
	//lv_btnmatrix_clear_btn_ctrl(btnm, 0, LV_BTNMATRIX_CTRL_RECOLOR);
	// 清除宽度，之后会平均分配宽度
	//lv_btnmatrix_clear_btn_ctrl(btnm, 1, _LV_BTNMATRIX_WIDTH);
	//lv_btnmatrix_clear_btn_ctrl(btnm, 3, _LV_BTNMATRIX_WIDTH);
	

	/* 一次清除所有按钮的控制属性 */
	//lv_btnmatrix_clear_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_RECOLOR);
	// 清除宽度，之后会平均分配宽度
	//lv_btnmatrix_clear_btn_ctrl_all(btnm, _LV_BTNMATRIX_WIDTH);

	/* 设置控制按钮的映射表 */
	// 除了上面的单独设置某个按钮，以及一次设置所有的按钮
	// 这是一种更折中方便的方式
	//lv_btnmatrix_set_ctrl_map(btnm, ctrl_map);

	/* 一次检查功能 */
	// 这样一次只能检查一个按钮，这样我们就能知道最后点击的是哪个按钮
	//lv_btnmatrix_set_one_checked(btnm, true);


	/* Parts and Styles */
	// 设置 LV_PART_MAIN 的样式
	// 设置 行间距，这里设置行间距为 0 
	//lv_obj_set_style_pad_row(btnm, 0, LV_PART_MAIN);

	// 设置 LV_PART_MAIN 的样式
	// 设置 列间距，这里设置 列间距 为 0
	//lv_obj_set_style_pad_column(btnm, 0, LV_PART_MAIN);

	// 设置 LV_PART_ITEMS 的样式
	// 设置按钮被按下时按钮的颜色
	//lv_obj_set_style_bg_color(btnm, lv_color_hex(0x78c146) , LV_STATE_PRESSED | LV_PART_ITEMS);


	/* 分配事件处理回调函数 */
	// 当按钮被按下或长按时会触发 LV_EVENT_VALUE_CHANGED 事件，我们只处理 LV_EVENT_VALUE_CHANGED 事件类型即可
	// 如果前面设置了 LV_BTNMATRIX_CTRL_DISABLED ，那么点击按钮将不会触发 LV_EVENT_VALUE_CHANGED 事件
	// 如果前面设置了 LV_BTNMATRIX_CTRL_NO_REPEAT ， 那么长按按钮时不会一直触发 LV_EVENT_VALUE_CHANGED 事件
	//lv_obj_add_event_cb(btnm, btnm_event_handler, LV_EVENT_VALUE_CHANGED, NULL);

	// 当然我们也可以处理其他的事件类型比如被点击或者长按时
	// 如果前面设置了 LV_BTNMATRIX_CTRL_DISABLED ，只会触发 LV_EVENT_CLICKED 事件
	//lv_obj_add_event_cb(btnm, btnm_event_handler, LV_EVENT_ALL, NULL);
	
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void btnm_event_handler(lv_event_t * e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * obj = lv_event_get_target(e);
	if(code == LV_EVENT_VALUE_CHANGED) {
		// 获取当前被点击的按钮的ID
		uint32_t id = lv_btnmatrix_get_selected_btn(obj);
		// 根据按钮ID获取对应按钮上的文本
		const char * txt = lv_btnmatrix_get_btn_text(obj, id);

		LV_LOG_USER("%s was pressed\n", txt);
	}
	else if(code == LV_EVENT_CLICKED) {
		uint32_t id = lv_btnmatrix_get_selected_btn(obj);
		const char * txt = lv_btnmatrix_get_btn_text(obj, id);

		LV_LOG_USER("LV_EVENT_CLICKED: %s was pressed\n", txt);
	}
}


#endif /* LV_USE_100ASK_DEMO_COURSE_3_14_1 */
