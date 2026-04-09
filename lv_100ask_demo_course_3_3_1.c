/**
 ******************************************************************************
 * @file    lv_100ask_demo_course_3_3_1.c
 * @author  百问科技
 * @version V1.0
 * @date    2022-01-20
 * @brief	3_3_1 课的课堂代码
 ******************************************************************************
 * Change Logs:
 * Date           Author          Notes
 * 2022-01-20     zhouyuebiao     First version
 ******************************************************************************
 * @attention
 *
 * Copyright (C) 2008-2021 深圳百问网科技有限公司<https://www.100ask.net/>
 * All rights reserved
 *
 ******************************************************************************
 */


/*********************
 *      INCLUDES
 *********************/
#include "src/lv_100ask_demo_course_3_3_1/lv_100ask_demo_course_3_3_1.h"



// 用Windows PC模拟器键盘或鼠标需要包含此头文件
#include "lv_drivers/win32drv/win32drv.h"


/*********************
 *      DEFINES
 *********************/


/**********************
 *  STATIC VARIABLES
 **********************/

void lv_100ask_demo_course_3_3_1(void)
{
    // 创建一个组，稍后将需要使用键盘或编码器或按钮控制的部件(对象)添加进去，并且将输入设备和组关联
    // 如果将这个组设置为默认组，那么对于那些在创建时会添加到默认组的部件(对象)就可以省略 lv_group_add_obj()
    lv_group_t * g = lv_group_create();

    // 将上面创建的组设置为默认组
    // 如果稍后创建的部件(对象)，使用默认组那必须要在其创建之前设置好默认组，否则不生效
    lv_group_set_default(g);

    // 将输入设备和组关联(使用前先打开上面注释掉的头文件)
    lv_indev_set_group(lv_win32_keypad_device_object, g);     // 键盘
    lv_indev_set_group(lv_win32_encoder_device_object, g);      // 鼠标上的滚轮(编码器)


    /* 创建一个btn部件(对象) */
    lv_obj_t * btn1 = lv_btn_create(lv_scr_act());       // 创建一个btn部件(对象),他的父对象是活动屏幕对象
    lv_obj_set_size(btn1, 100, 50);
    lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -100);

    lv_obj_t * btn2 = lv_btn_create(lv_scr_act());       // 创建一个btn部件(对象),他的父对象是活动屏幕对象
    lv_obj_set_size(btn2, 100, 50);
    lv_obj_align_to(btn2, btn1, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    lv_obj_t * btn3 = lv_btn_create(lv_scr_act());       // 创建一个btn部件(对象),他的父对象是活动屏幕对象
    lv_obj_set_size(btn3, 100, 50);
    lv_obj_align_to(btn3, btn2, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    lv_obj_t * btn4 = lv_btn_create(lv_scr_act());       // 创建一个btn部件(对象),他的父对象是活动屏幕对象
    lv_obj_set_size(btn4, 100, 50);
    lv_obj_align_to(btn4, btn3, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    lv_obj_t * slider = lv_slider_create(lv_scr_act());
    lv_obj_align_to(slider, btn4, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    // 将部件(对象)添加到组，如果设置了默认组，这里可以省略
    //lv_group_add_obj(g, btn1);
    //lv_group_add_obj(g, btn2);
    //lv_group_add_obj(g, btn3);
    //lv_group_add_obj(g, btn4);
    //lv_group_add_obj(g, slider);
}



static lv_group_t *group_main;    // 主界面组
static lv_group_t *group_init;    // 初始界面组
lv_obj_t * label_surgf;
// 主界面按钮全局声明（方便聚焦函数访问）
static lv_obj_t *btn1, *btn2, *btn_plus;
static lv_obj_t *flex_container;  // Flex容器全局（后续可加动画）

// 函数声明
static void btn1_event_cb(lv_event_t * e);
static void enter_btn_event_cb(lv_event_t * e);
static void create_main_ui(void);  // 创建主屏幕（独立屏幕）
static void create_init_ui(void);  // 创建初始屏幕（独立屏幕）
// 聚焦函数：主动设置组的初始焦点+启用循环切换
static void group_set_focus(lv_group_t * group, lv_obj_t * init_obj) {
    if(group == NULL || init_obj == NULL) return;
    lv_group_set_wrap(group, true);  // 启用焦点循环
    lv_group_focus_obj(init_obj);    // 主动设置初始焦点
    lv_group_set_default(group);     // 设为默认组
}

// 主屏幕创建函数（独立屏幕，用 lv_scr_load 加载）
static void create_main_ui(void) {
    // 创建主屏幕对象（独立屏幕，非子容器）
    lv_obj_t * main_scr = lv_obj_create(NULL);  // 父对象为NULL，创建独立屏幕
    lv_obj_set_size(main_scr, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(main_scr, lv_color_hex(0x222222), 0);

    /* 创建btn1（全局变量） */
    btn1 = lv_btn_create(main_scr);
    lv_obj_align(btn1, LV_ALIGN_CENTER, -100, 0);
    lv_obj_t * label_btn1 = lv_label_create(btn1);
    lv_label_set_text(label_btn1, "btn1");
    // 焦点样式（聚焦时背景变色）
    lv_obj_set_style_bg_color(btn1, lv_color_hex(0x444444), LV_STATE_FOCUSED);

    /* 创建btn2（全局变量） */
    btn2 = lv_btn_create(main_scr);
    lv_obj_align(btn2, LV_ALIGN_CENTER, -200, 0);
    lv_obj_t * label_btn2 = lv_label_create(btn2);
    lv_label_set_text(label_btn2, "btn2");
    lv_obj_add_event_cb(btn2, btn1_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(btn2, lv_color_hex(0x444444), LV_STATE_FOCUSED);

    /* 创建右侧弹性布局容器 */
    flex_container = lv_obj_create(main_scr);
    lv_obj_set_style_bg_color(flex_container, lv_color_hex(0x000000), 0);
    lv_obj_set_size(flex_container, 150, LV_PCT(100));
    lv_obj_align(flex_container, LV_ALIGN_RIGHT_MID, 0, 0);

    // 配置弹性布局
    lv_obj_set_flex_flow(flex_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(flex_container,
                          LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER,
                          0);

    /* 添加文本元素 */
    label_surgf = lv_label_create(flex_container);
    lv_label_set_text(label_surgf, "SurGF");
    lv_obj_set_style_text_color(label_surgf, lv_color_hex(0x00FF00), 0);

    lv_obj_t * label_percent = lv_label_create(flex_container);
    lv_label_set_text(label_percent, "44%");
    lv_obj_set_style_text_color(label_percent, lv_color_hex(0x00FF00), 0);

    /* 创建btn_plus（全局变量） */
    btn_plus = lv_btn_create(flex_container);
    lv_obj_set_size(btn_plus, 60, 60);
    lv_obj_set_style_bg_color(btn_plus, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(btn_plus, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_width(btn_plus, 2, 0);
    lv_obj_set_style_radius(btn_plus, 10, 0);
    lv_obj_set_style_bg_color(btn_plus, lv_color_hex(0x444444), LV_STATE_FOCUSED);

    lv_obj_t * label_plus = lv_label_create(btn_plus);
    lv_label_set_text(label_plus, "+");
    lv_obj_set_style_text_color(label_plus, lv_color_hex(0x00FF00), 0);

    lv_obj_t * label_gas = lv_label_create(flex_container);
    lv_label_set_text(label_gas, "Gas");
    lv_obj_set_style_text_color(label_gas, lv_color_hex(0x00FF00), 0);

    lv_obj_t * label_air = lv_label_create(flex_container);
    lv_label_set_text(label_air, "AIR");
    lv_obj_set_style_text_color(label_air, lv_color_hex(0x00FF00), 0);

    /* 将按钮加入主界面组 */
    lv_group_add_obj(group_main, btn1);
    lv_group_add_obj(group_main, btn2);
    lv_group_add_obj(group_main, btn_plus);

    // 绑定输入设备到主组（关键：确保编码器控制主屏幕按钮）
    lv_indev_set_group(lv_win32_keypad_device_object, group_main);
    lv_indev_set_group(lv_win32_encoder_device_object, group_main);
    group_set_focus(group_main, btn2);  // 主屏幕初始焦点在btn2

    // 加载主屏幕（切换到主屏幕）
    lv_scr_load(main_scr);
}

// 初始屏幕创建函数（独立屏幕，程序启动先加载）
static void create_init_ui(void) {
    // 创建初始屏幕对象（独立屏幕）
    lv_obj_t * init_scr = lv_obj_create(NULL);
    lv_obj_set_size(init_scr, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(init_scr, lv_color_hex(0x111111), 0);

    /* 创建初始屏幕按钮 */
    lv_obj_t * enter_btn = lv_btn_create(init_scr);
    lv_obj_t * enter_btn2 = lv_btn_create(init_scr);
    lv_obj_set_size(enter_btn, 120, 60);
    lv_obj_align(enter_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(enter_btn2, LV_ALIGN_RIGHT_MID, -20, 0);

    // 按钮样式+聚焦样式
    lv_obj_set_style_bg_color(enter_btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_color(enter_btn, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_border_width(enter_btn, 2, 0);
    lv_obj_set_style_bg_color(enter_btn, lv_color_hex(0x555555), LV_STATE_FOCUSED);

    lv_obj_set_style_bg_color(enter_btn2, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_color(enter_btn2, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_border_width(enter_btn2, 2, 0);
    lv_obj_set_style_bg_color(enter_btn2, lv_color_hex(0x555555), LV_STATE_FOCUSED);

    /* 按钮文本 */
    lv_obj_t * enter_label = lv_label_create(enter_btn);
    lv_label_set_text(enter_label, "enter");
    lv_obj_set_style_text_color(enter_label, lv_color_hex(0x00FFFF), 0);

    lv_obj_t * enter_label2 = lv_label_create(enter_btn2);
    lv_label_set_text(enter_label2, "InitBtn2");
    lv_obj_set_style_text_color(enter_label2, lv_color_hex(0x00FFFF), 0);

    /* 初始组配置+聚焦激活 */
    lv_group_add_obj(group_init, enter_btn);
    lv_group_add_obj(group_init, enter_btn2);
    // 绑定输入设备到初始组
    lv_indev_set_group(lv_win32_keypad_device_object, group_init);
    lv_indev_set_group(lv_win32_encoder_device_object, group_init);
    group_set_focus(group_init, enter_btn);  // 初始焦点在enter按钮

    /* 绑定回调：点击enter加载主屏幕 */
    lv_obj_add_event_cb(enter_btn, enter_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // 加载初始屏幕（程序启动默认显示）
    lv_scr_load(init_scr);
}

// 初始屏幕enter按钮回调：加载主屏幕
static void enter_btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        // 直接创建并加载主屏幕（lv_scr_load 自动切换）
        create_main_ui();
        
        
    }
}

// btn2回调函数：切换label_surgf显示/隐藏
static void btn1_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        if(lv_obj_has_flag(label_surgf, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_clear_flag(label_surgf, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(label_surgf, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// 入口函数：初始化组并加载初始屏幕
void btn_2group_test(void) {
    /* 初始化两个组（必须先初始化，再创建界面） */
    group_main = lv_group_create();
    group_init = lv_group_create();

    /* 创建并加载初始屏幕（程序启动先显示这个） */
    create_init_ui();
}