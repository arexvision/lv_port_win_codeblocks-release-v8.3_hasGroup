/**
 ******************************************************************************
 * @file    lv_100ask_demo_course_3_13_1.c
 * @author  百问科技
 * @version V1.0
 * @date    2022-06-16
 * @brief	3_13_1 课的课堂代码
 ******************************************************************************
 * Change Logs:
 * Date           Author          Notes
 * 2022-07-14     zhouyuebiao     First version
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

#if LV_USE_100ASK_DEMO_COURSE_3_13_1

#include <string.h>
#include "lv_100ask_demo_course_3_13_1.h"


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
static void event_handler(lv_event_t * e);
static void ta_event_handler(lv_event_t * e);



/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void lv_100ask_demo_course_3_13_1(void)
{
	lv_obj_t * ta = lv_textarea_create(lv_scr_act());

	/* 设置文本框可容纳的文本的长度 */
	// 这个要在添加或者设置文本之前设置，否则第一次添加或者设置文本不会有长度限制
	// 为 0 时无限制
	//lv_textarea_set_max_length(ta, 8);
	//lv_textarea_set_max_length(ta, 0); // 无限制
	
	/* 添加文本 */
	// 1. 单个字符添加
#if 0
	lv_textarea_add_char(ta, 'b');
	lv_textarea_add_char(ta, 'w');
	lv_textarea_add_char(ta, 'w');
	lv_textarea_add_char(ta, '1');
	lv_textarea_add_char(ta, '2');
	lv_textarea_add_char(ta, '3');
#endif

	// 2. 字符串添加，可以通过 \n 换行
	lv_textarea_add_text(ta, "100ask.net \nlvgl.100ask.net");

	/* 设置文本 */
	// 可以通过 \n 换行
	// 这会清空之前的所有文本再覆盖新的文本
	//lv_textarea_set_text(ta, "1234567890 100ask.net\nlvgl.100ask.net");

	/* 设置光标的位置 */
	// 1. 将光标移动到最开始的位置(第1个字符之前)
	//lv_textarea_set_cursor_pos(ta, 0);
	// 2. 将光标移动到其他任意位置，这里移动到(第10个字符之前)
	//lv_textarea_set_cursor_pos(ta, 10);
	// 3. 将光标移动到末尾的位置(最后一个字符之后)
	//lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
	// 4. 在行列之间，上、下、左、右移动
	//lv_textarea_cursor_up(ta);	// 上
	//lv_textarea_cursor_down(ta);	// 下
	//lv_textarea_cursor_left(ta);	// 左
	//lv_textarea_cursor_right(ta);	// 右
	// 5. 让光标跟随触摸跳转位置(在文本框范围内)，默认为 true
	//lv_textarea_set_cursor_click_pos(ta, true);

	/* 设置光标的闪烁时间 */
	//lv_obj_set_style_anim_time(ta, 1000, LV_STATE_FOCUSED | LV_PART_CURSOR);

	/* 通过代码手动聚焦到 textarea   */
	// 我们可以通过点击 textarea 来让其聚焦，只有 textarea 在
	// 聚焦状态下，光标才会展示出来，如果我们想一开打页面或者执行的时候就让
	// textarea 进入聚焦状态，那么就可以用这个方法
	//lv_obj_add_state(ta, LV_STATE_FOCUSED);
	
	/* 删除字符 */
	// 1. 删除光标后面的一个字符
	//lv_textarea_del_char(ta);
	// 2. 删除光标后面的一个字符
	//lv_textarea_del_char_forward(ta);

	/* 设置文本占位符 */
	// 设置了之后，如果 textarea 的文本内容为空时，文本占位符就会展示出来，
	// 文本占位符的作用是 提供信息帮助用户输入内容
	//lv_textarea_set_placeholder_text(ta, "Placeholder text");

	/* 单行模式 */
	// 这个模式下高度自动设置为仅显示一行，忽略换行符，并禁用自动换行
	//lv_textarea_set_one_line(ta, true);

	/* 密码模式 */
	// 这个模式下，我们输入的任何内容都会在 LV_TEXTAREA_DEF_PWD_SHOW_TIME 时间(ms)之后变为 '•'
	// 这里要注意，我们的的字体中要存在 '•' 字符，如果没有 '•' ，将会使用 '*' 。
	// 也就是说，这个模式下，我们的字体要有 '•' 或 '*' 字符。
	//lv_textarea_set_password_mode(ta, true);

	/* 字符白名单 */
	// 设置可接受字符列表(白名单)，输入白名单之外的字符将被忽略。
	lv_textarea_set_accepted_chars(ta, "0123456789.+-");

	/* 可选择文本 */
	// 这和我们使用鼠标在电脑上选择文字非常相似，但是要注意，
	// textarea 本身并没有 LV_PART_SELECTED ，说 textarea 有这部分是因为其集成了 label，
	// 所以我们不能直接设置 textarea 的 LV_PART_SELECTED 的样式，而是要设置 textarea 中的 label 的 LV_PART_SELECTED 的样式
	// 为什么要设置 LV_PART_SELECTED 的样式，因为选中的文本默认我们看不到现象，
	// 最好修改选中的文本的 text_color 和 bg_color 这样选中文本时才会时我们想要的效果
#if 1
	// 想选择文本，要先确保设置了：让光标跟随触摸跳转位置
	// 也就是：lv_textarea_set_cursor_click_pos(ta, true)
	lv_textarea_set_text_selection(ta, true);
	lv_obj_set_style_text_color(lv_textarea_get_label(ta), lv_color_hex(0xc43e1c), LV_PART_SELECTED); 
	lv_obj_set_style_bg_color(lv_textarea_get_label(ta), lv_color_hex(0x43a72d), LV_PART_SELECTED); 
#endif

	/* 为 textarea 分配回调函数 */
	// 1. 完成替换即将插入的问题为其他指定的文本
	// 2. 当文本框的内容被改变时，打印提示
	lv_obj_add_event_cb(ta, ta_event_handler, LV_EVENT_ALL, NULL);

	/* 创建两个按钮，模拟键盘按键输入和删除 */
	// 第1个按钮： 输入字符 'a'
	lv_obj_t * btn;
	lv_obj_t * label;
	btn = lv_btn_create(lv_scr_act());
	lv_obj_center(btn);
	label = lv_label_create(btn);
	lv_label_set_text(label, "a");
	// 如果开启了上面的字符白名单，这可以使用下面这个文本测试看看效果
	// a、b、c 字符不在白名单中，所以无法通过，不能插入到文本框中
	//lv_label_set_text(label, "1a2b3c+");
	lv_obj_center(label);

	lv_obj_add_event_cb(btn, event_handler, LV_EVENT_ALL, ta);

	// 第2个按钮：删除一个字符
	btn = lv_btn_create(lv_scr_act());
	lv_obj_align(btn, LV_ALIGN_CENTER, 100, 0);
	label = lv_label_create(btn);
	lv_label_set_text(label, LV_SYMBOL_BACKSPACE);
	lv_obj_center(label);
	
	lv_obj_add_event_cb(btn, event_handler, LV_EVENT_ALL, ta);


}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void event_handler(lv_event_t * e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * btn = lv_event_get_target(e);
	lv_obj_t * ta = lv_event_get_user_data(e);
	lv_obj_t * label = lv_obj_get_child(btn, 0);

	// 当按钮被点击时，判断当前的按钮，并根据不同的按钮执行不同的操作
	if(code == LV_EVENT_CLICKED) {
		char * label_text = lv_label_get_text(label);
		if(strcmp(label_text, LV_SYMBOL_BACKSPACE) == 0)
			lv_textarea_del_char(ta);  // 删除一个字符
		else
			lv_textarea_add_text(ta, label_text);	// 添加字符或字符串
	}
}

static void ta_event_handler(lv_event_t * e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * ta = lv_event_get_target(e);

	if(code == LV_EVENT_INSERT) {
		// 如果我们通过点击按钮输入文本到文本框前，会触发这个事件，
		// 处理完所有接收 LV_EVENT_INSERT 回调函数之后才会将文本真正添加到文本框。
		// 所以我们这里可以做检查或者替换，比如帮助用户纠正输入。
		//lv_textarea_set_insert_replace(ta, "bbb");
	}
	else if(code == LV_EVENT_VALUE_CHANGED) {
		// 当文本框的内容被改变时发送该事件
		LV_LOG_USER("TA: LV_EVENT_VALUE_CHANGED!");
	}
}

#endif /* LV_USE_100ASK_DEMO_COURSE_3_13_1 */
