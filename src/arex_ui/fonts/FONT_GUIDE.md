# 字体生成指南

本文档供 AI 阅读，用于指导如何为项目生成和集成 LVGL 字体。

## 1. 字体生成工具

使用 `lv_font_conv` 将 TTF/OTF 转换为 LVGL 字体格式。

### 安装
```bash
npm i lv_font_conv -g
```

## 2. 生成字体命令

```bash
lv_font_conv --font "字体路径.ttf" --size 字号 --bpp 4 --format lvgl -r 0x20-0x7E -r 0x00B0 --no-compress -o 输出.c
```

### 参数说明

| 参数 | 必须 | 说明 |
|------|------|------|
| `--font` | 是 | TTF/OTF 字体文件路径 |
| `--size` | 是 | 字号（像素） |
| `--bpp` | 是 | 每像素位数，推荐 4 |
| `--format` | 是 | 固定写 `lvgl` |
| `-o` | 是 | 输出文件名 |
| `--no-compress` | 建议 | 禁用 RLE 压缩，避免兼容问题 |
| `-r` | 按需 | 字符范围，如 `-r 0x20-0x7E` 表示空格到 `~` |

### 常用字符范围

```bash
-r 0x20-0x7E    # 可打印 ASCII（空格、英文、数字、符号）
-r 0x00B0       # 度符号 °（潜水用）
```

## 3. 项目中的文件位置

字体文件放在：
```
src/arex_ui/fonts/
```

## 4. 字体命名规范

格式：`lv_font_家族_字号.c`

示例：
- `lv_font_consola_14.c`
- `lv_font_consola_20.c`
- `lv_font_consola_58.c`

## 5. 集成步骤（重要！）

### 5.1 生成字体文件

```bash
cd src/arex_ui/fonts
lv_font_conv --font "E:/字体/xxx.ttf" --size 24 --bpp 4 --format lvgl -r 0x20-0x7E -r 0x00B0 --no-compress -o lv_font_xxx_24.c
```

### 5.2 添加到项目（.cbp）

**必须将字体文件作为独立编译单元添加到 `.cbp`，禁止使用 `#include`！**

在 `LittlevGL.cbp` 的 `<Units>` 中添加：

```xml
<Unit filename="src/arex_ui/fonts/lv_font_xxx_24.c">
    <Option compilerVar="CC" />
</Unit>
```

**如果只修改 `arex_fonts.h` 加 `#include`，会导致所有字体文件编译到同一个目标文件，造成变量名冲突错误！**

### 5.3 声明和宏定义

在 `src/arex_ui/fonts/arex_fonts.h` 中：

```c
// 1. 添加 extern 声明（放在对应字体族下）
extern const lv_font_t lv_font_xxx_24;

// 2. 添加宏定义
#define AREX_FONT_24 (&lv_font_xxx_24)
```

## 6. 字体族切换

编辑 `arex_fonts.h` 顶部的宏：

```c
#define AREX_USE_FONT_CONSOLA   // 当前：Consolas
// #define AREX_USE_FONT_COURIER    // 可选：Courier New Bold
```

## 7. 常见问题

### 错误：redefinition of 'glyph_bitmap'

**原因**：多个字体文件作为独立编译单元时，内部变量名相同。

**解决**：每个字体文件是独立的 `lv_font_conv` 产物，变量名已唯一。确保 `.cbp` 中正确添加，**不要** `#include` 字体文件。

### 错误：fatal error: xxx.c: No such file or directory

**原因**：include 路径缺少 `src` 目录。

**解决**：在 `.cbp` 的 `<Compiler>` 中添加：
```xml
<Add directory="src" />
```

### 警告：'AREX_FONT_XXX' redefined

**原因**：`arex_screen.h` 和 `arex_fonts.h` 都定义了相同的宏。

**解决**：`arex_screen.h` 应该 `#include "fonts/arex_fonts.h"`，然后删除其中重复的宏定义。

## 8. 禁止事项

1. **禁止**用 `#include` 方式包含字体 `.c` 文件
2. **禁止**将多个字体文件内容合并成一个文件
3. **禁止**修改 LVGL 内部字体生成器的变量命名逻辑

## 9. 当前字体状态

| 字体族 | 尺寸 | 文件 | 状态 |
|--------|------|------|------|
| Consolas | 14/20/24/28/48/58px | lv_font_consola_*.c | 启用 |
| Courier | 14/20/28/48/58px | lv_font_courier_*.c | 已禁用 |
| Linotype Ordinar | 14/20/28/48/58px | lv_font_ordinar_*.c | 可用 |

切换字体族：修改 `arex_fonts.h` 的 `#define AREX_USE_FONT_*` 宏。
