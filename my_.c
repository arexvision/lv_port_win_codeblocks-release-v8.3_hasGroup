#include <stdlib.h>
#include <unistd.h>

#include "lvgl/lvgl.h"
#include "lvgl/demos/widgets/lv_demo_widgets.h"
#include "lv_drivers/win32drv/win32drv.h"
#include "lv_100ask_teach_demos.h"
#include "lv_100ask_teach_demos_conf.h"

#include "src/lv_100ask_demo_course_3_1_1/lv_100ask_demo_course_3_1_1.h"


#include <windows.h>

static lv_color_filter_dsc_t filter_dsc;

void a()
{

    lv_group_t *group_main = lv_group_create();
    lv_group_set_default(group_main);
    lv_indev_set_group(lv_win32_keypad_device_object,group_main);
    lv_indev_set_group(lv_win32_encoder_device_object,group_main);


    lv_obj_t *button = lv_btn_create(lv_scr_act());
    lv_obj_t *button2 = lv_btn_create(lv_scr_act());
    lv_obj_align_to(button2,button,LV_ALIGN_BOTTOM_MID,0,20);
    lv_obj_align(button,LV_ALIGN_CENTER,0,0);
    lv_obj_add_state(button, LV_STATE_DISABLED);
    // 设置颜色滤镜效果
    lv_obj_set_style_color_filter_dsc(button, &filter_dsc, LV_STATE_DISABLED);
    lv_obj_set_size(button, 150, 100);

    lv_obj_t * label = lv_label_create(button);
    lv_label_set_text(label, "123");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *label2 = lv_label_create(button2);
    lv_label_set_text(label2, "qwe");
    lv_obj_align(label2, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

}
