# 流程：增加一个子菜单页

典型需求：在某个菜单下面新增一层页面，比如：

- `DISPLAY -> COLOR THEME`
- `SYSTEMS SETUP -> SENSOR SETUP`
- `LIGHT CONTROL -> UV`

## 当前链路

```text
menu_defs.h
  增加 menu_id_t 和入口 item_id

menu_defs.c
  标题和父子菜单映射

menu_runtime.c
  当前 menu_id_t 生成 rows

submenu_model.c
  如需动态文案，提供 builder

menu_actions.c
  子菜单里的行被点击后执行动作

submenu_view.c
  只负责显示，不改业务逻辑
```

## 例子：DISPLAY 下增加 COLOR THEME 子菜单

### 1. 增加菜单 ID

文件：`src/ui/views/menu_defs.h`

在 `menu_id_t` 增加：

```c
MENU_COLOR_THEME,
```

### 2. 增加入口行 ID 和子菜单行 ID

文件：`src/ui/views/menu_defs.h`

示例：

```c
MENU_ITEM_DISPLAY_COLOR_THEME,
MENU_ITEM_COLOR_THEME_TECH,
MENU_ITEM_COLOR_THEME_CLASSIC,
```

### 3. 增加标题和父子映射

文件：`src/ui/views/menu_defs.c`

在 `menu_defs_title()` 增加：

```c
case MENU_COLOR_THEME: return "COLOR THEME";
```

在 `menu_defs_child_menu_for_item()` 增加：

```c
case MENU_ITEM_DISPLAY_COLOR_THEME: return MENU_COLOR_THEME;
```

### 4. 在父菜单显示入口

文件：`src/ui/views/submenu_model.c`

如果父菜单是动态文案，比如 DISPLAY，就在 `build_nested_display_sys()` 增加一行：

```c
s_nested_display_sys[5] = "COLOR THEME";
```

同时调整数组大小、NULL 结尾和 `count_items()` 上限。

文件：`src/ui/views/menu_runtime.c`

在 `MENU_DISPLAY` 的 `ids[]` 对应位置增加：

```c
MENU_ITEM_DISPLAY_COLOR_THEME,
```

文案位置和 ID 位置必须一致。

### 5. 为新子菜单生成 rows

如果是固定文案，可以在 `submenu_model.c` 加 builder：

```c
static const char *s_color_theme_items[] =
{
    "TECH",
    "CLASSIC",
    NULL
};

const char **submenu_build_color_theme_items(uint8_t *out_count)
{
    if (out_count)
    {
        *out_count = count_items(s_color_theme_items, 3);
    }
    return s_color_theme_items;
}
```

同时在 `submenu_model.h` 声明。

文件：`src/ui/views/menu_runtime.c`

新增 case：

```c
case MENU_COLOR_THEME:
{
    static const menu_item_id_t ids[] =
    {
        MENU_ITEM_COLOR_THEME_TECH,
        MENU_ITEM_COLOR_THEME_CLASSIC,
    };
    build_from_model(submenu_build_color_theme_items, ids, NULL);
    break;
}
```

### 6. 实现点击动作

文件：`src/ui/views/menu_actions.c`

在 `direct_setting_for_id()` 或独立 handler 中按 ID 处理：

```c
case MENU_ITEM_COLOR_THEME_TECH:
    setting_prepare(out_setting, SUBMENU_SETTING_THEME, 0U, 0U);
    return true;
case MENU_ITEM_COLOR_THEME_CLASSIC:
    setting_prepare(out_setting, SUBMENU_SETTING_THEME, 0U, 1U);
    return true;
```

如果这个设置已经有 `bus_set_*()`，最终要走 bus。

## 检查点

- 父菜单文案数组和父菜单 `ids[]` 顺序一致。
- 新子菜单文案数组和新子菜单 `ids[]` 顺序一致。
- 打开子菜单靠 `menu_defs_child_menu_for_item()`，不要在 view 里判断文字。
- 子菜单标题来自 `menu_defs_title()`，只用于显示。

## 验证

```powershell
rg -n "MENU_COLOR_THEME|DISPLAY_COLOR_THEME|COLOR THEME" src\ui
rg -n "strcmp\\(" src\ui\views src\ui\menus src\ui\screen src\ui\cards
gcc -fsyntax-only -DLV_CONF_INCLUDE_SIMPLE=1 -DWINVER=0x0601 -I. -Isrc -Ilvgl -Ilv_drivers src\ui\views\menu_defs.c src\ui\views\menu_runtime.c src\ui\views\menu_actions.c src\ui\views\submenu_model.c
git diff --check
```

手动验证：

- 父菜单能进入新子菜单。
- 返回能回到父菜单并恢复焦点。
- 子菜单选中态和 DIVE SETUP/INFO MENU 样式一致。
- 选择项能刷新父菜单文案或 badge。
