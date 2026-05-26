# picture

`picture` 放 LVGL 图片资源，主要供组件和卡片引用。

## 当前资源

| 文件族 | 用途 |
|---|---|
| `sudo_up_level*.c` | 上升速率等级图标。 |
| `sudo_down_level*.c` | 下降速率等级图标。 |
| `qiping.c` | 气瓶图标。 |
| `Shoudiantong.c` | 手电筒图标。 |
| `liuzhuandeng.c` | 流转灯图标。 |

## 接入规则

- 这里放图像资源定义，不放业务判断和交互逻辑。
- 新资源需要确认声明、引用方和 `LittlevGL.cbp` 中的编译单元都已同步。
- 图标显示异常时，同时检查资源透明度/色深和 `lv_conf.h` 的相关配置。
