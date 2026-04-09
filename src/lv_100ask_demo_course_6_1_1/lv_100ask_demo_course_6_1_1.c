/**
 ******************************************************************************
 * @file    lv_100ask_demo_course_6_1_1.c
 * @author  百问科技
 * @version V1.0
 * @date    2022-08-02
 * @brief	6_1_1 课堂代码
 ******************************************************************************
 * Change Logs:
 * Date           Author          Notes
 * 2022-08-02     zhouyuebiao     First version
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

#if LV_USE_100ASK_DEMO_COURSE_6_1_1

#include "lv_100ask_demo_course_6_1_1.h"


/*********************
 *      DEFINES
 *********************/
// 要打开的文件
#define FILE_NAME	"D:/100ask/tmp/lv_fs_test.txt"
// 要读取的目录
#define DIR_PATH	"D:/100ask/tmp/"

/**********************
 *      TYPEDEFS
 **********************/


/**********************
 *  STATIC VARIABLES
 **********************/


/**********************
 *  STATIC PROTOTYPES
 **********************/
/* 通过LVGL文件系统接口统一不同的文件系统并读取文件 */
static void lv_fs_read_dir(char * fn);

/* 通过LVGL文件系统接口统一不同的文件系统并读取目录内容 */
static void lv_fs_read_file(char * path);


/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_100ask_demo_course_6_1_1(void)
{
#if 1
	// 读取文件	
	lv_fs_read_file(FILE_NAME);
#else
	// 读取目录内容
	lv_fs_read_dir(DIR_PATH);
#endif	

}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* 通过LVGL文件系统接口统一不同的文件系统并读取文件 */
static void lv_fs_read_file(char * fn)
{
	lv_fs_file_t f;
	lv_fs_res_t res;

	// 打开文件有两个模式： LV_FS_MODE_RD(只读) 和 LV_FS_MODE_WR(写)
	res = lv_fs_open(&f, fn, LV_FS_MODE_RD);
	// 如果一切正常会返回 LV_FS_RES_OK ，其他错误代码请看 lv_fs.h 中的 lv_fs_res_t 定义
	if(res != LV_FS_RES_OK) {
		LV_LOG_USER("Open error! Error code: %d", res);
		return;
	}

	/* 每次实际读取到的数据大小(byte) */
	uint32_t read_num;
	/* 数据缓冲区 */
	uint8_t buf[8];

	/* 读取整个文件并打印内容 */
	while (1) {
		res = lv_fs_read(&f, buf, 8, &read_num);
		if(res != LV_FS_RES_OK) {
			LV_LOG_USER("Read error! Error code: %d", res);
			break;
		}

		/* 将读取到数据打印出来 */
		printf("%s", buf);

		if (read_num != 8)	break;
	}

	lv_fs_close(&f);

}

/* 通过LVGL文件系统接口统一不同的文件系统并读取目录内容 */
static void lv_fs_read_dir(char * path)
{
	lv_fs_dir_t dir;
	lv_fs_res_t res;
	res = lv_fs_dir_open(&dir, path);
	if(res != LV_FS_RES_OK){
		LV_LOG_USER("Open DIR error! Error code: %d", res);
		return;
	}

	char fn[256];	// 缓冲区
	while(1) {
		res = lv_fs_dir_read(&dir, fn);
		if(res != LV_FS_RES_OK) {
			LV_LOG_USER("Read DIR error! Error code: %d", res);
			break;
		}

		/* 如果没有更多文件可以读取时 fn 就为空 */
		if(strlen(fn) == 0) {
			LV_LOG_USER("Fn is empty, if not more files to read.");
			break;
		}

		printf("%s\n", fn);
	}

	lv_fs_dir_close(&dir);

}

#endif /* LV_USE_100ASK_DEMO_COURSE_6_1_1 */
