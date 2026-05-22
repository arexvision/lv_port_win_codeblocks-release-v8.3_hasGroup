# fonts

`fonts` 放 LVGL 字体资源和字体入口。

主要职责：

- `arex_fonts.h` 统一声明项目使用的字体对象和字体宏。
- `lv_font_consola_*.c`、`lv_font_courier_*.c`、`lv_font_ordinar_*.c` 是独立编译的字体资源。
- `FONT_GUIDE.md` 记录字体生成和接入规则。

字体 `.c` 文件必须作为独立编译单元加入工程，不要用 `#include` 方式包含字体 `.c` 文件。
