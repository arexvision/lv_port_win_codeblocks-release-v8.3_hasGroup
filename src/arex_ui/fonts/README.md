# fonts

`fonts` 放项目自带的 LVGL 字体资源。业务模块不应直接散落声明字体对象，统一从这里取。

## 文件职责

| 文件 | 作用 |
|---|---|
| `arex_fonts.h` | 字体资源声明和项目字体宏入口。 |
| `FONT_GUIDE.md` | 字体接入、生成和 CodeBlocks 工程配置说明。 |
| `lv_font_consola_*.c` | Consola 字体资源。 |
| `lv_font_courier_*.c` | Courier 字体资源。 |
| `lv_font_ordinar_*.c` | Ordinar 字体资源。 |

## 接入规则

- 字体 `.c` 是独立编译单元，必须加入 `LittlevGL.cbp`。
- 不要在业务文件中 `#include` 字体 `.c` 文件。
- 新字体接入后，先确认 `arex_fonts.h` 和 `core/arex_ui_engine.c` 的字体映射是否需要同步。
