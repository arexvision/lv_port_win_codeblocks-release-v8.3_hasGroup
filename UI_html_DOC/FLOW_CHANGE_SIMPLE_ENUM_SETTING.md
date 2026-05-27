# 流程：修改简单枚举设置

典型需求：增加/删除一个亮度档位，或给某个固定列表设置改文案、改默认值。

适用对象：

- `BRIGHTNESS`
- `CONSERVATISM`
- 后续可收敛的 `SALINITY`
- 后续可收敛的 `LAST DECO STOP`
- 后续可收敛的 `LOG RATE`

## 判断它是不是简单枚举

满足下面条件就属于简单枚举：

- 用户只是在列表里选一项。
- 每一项都有固定 ID。
- 选中后写一个固定配置值。
- 不需要输入数字。
- 不需要确认弹窗或复杂页面。

## 以 BRIGHTNESS 为例

当前亮度链路：

```text
ui_engine.h
  BRIGHTNESS_ECO..SUN

submenu_model.c
  s_brightness_options[] 统一提供 value / menu label / badge / visible opa

menu_defs.h
  MENU_ITEM_BRIGHTNESS_*

menu_runtime.c
  build_setup_brightness() 绑定 row ID

menu_actions.c
  handle_brightness() 写 bus_set_brightness() 和 set_brightness()

screen.c
  apply_software_brightness() 根据亮度值更新遮罩
```

## 增加一个亮度档位

### 1. 增加枚举

文件：`src/ui/core/ui_engine.h`

示例：

```c
typedef enum
{
    BRIGHTNESS_ECO = 0,
    BRIGHTNESS_MED,
    BRIGHTNESS_HIGH,
    BRIGHTNESS_MAX,
    BRIGHTNESS_SUN,
    BRIGHTNESS_OUTDOOR,
    BRIGHTNESS_COUNT
} brightness_level_t;
```

### 2. 增加选项表

文件：`src/ui/views/submenu_model.c`

示例：

```c
{ BRIGHTNESS_OUTDOOR, "OUTDOOR", "OUTDOOR", 248 },
```

字段含义：

```text
value       写入 g_sys_config.brightness 的值
menu_label  菜单列表显示
badge_label SETUP 顶层 badge 显示
visible_opa 软件亮度遮罩使用
```

### 3. 增加行 ID

文件：`src/ui/views/menu_defs.h`

示例：

```c
MENU_ITEM_BRIGHTNESS_OUTDOOR,
```

### 4. 绑定 runtime 行 ID

文件：`src/ui/views/menu_runtime.c`

在 `build_setup_brightness()` 的 `ids[]` 增加：

```c
MENU_ITEM_BRIGHTNESS_OUTDOOR,
```

### 5. 更新 action 范围

文件：`src/ui/views/menu_actions.c`

如果新增 ID 放在 `MENU_ITEM_BRIGHTNESS_ECO` 到 `MENU_ITEM_BRIGHTNESS_SUN` 之外，需要把范围终点改成新最后一项。

示例：

```c
if (id < MENU_ITEM_BRIGHTNESS_ECO || id > MENU_ITEM_BRIGHTNESS_OUTDOOR)
{
    return false;
}
```

## 删除一个亮度档位

删除顺序反过来：

1. 从 `menu_runtime.c` 的 `ids[]` 删除行 ID。
2. 从 `submenu_model.c` 的 `s_brightness_options[]` 删除选项。
3. 从 `menu_defs.h` 删除对应 `MENU_ITEM_BRIGHTNESS_*`。
4. 从 `ui_engine.h` 删除对应 `BRIGHTNESS_*`。
5. 检查默认值是否还有效。

注意：如果明确不兼容旧值，就不要保留旧枚举，不要写兜底映射。

## 检查点

- `BRIGHTNESS_COUNT` 和 `s_brightness_options[]` 数量一致。
- `menu_runtime.c` 的 `ids[]` 数量和显示选项数量一致。
- 不要在 `screen.c` 里再维护一份亮度 label。
- 不要在 `card/menu/view` 里靠 `"ECO"` / `"SUN"` 做判断。

## 验证

```powershell
rg -n "BRIGHTNESS_|s_brightness_options|MENU_ITEM_BRIGHTNESS" src\ui
rg -n "strcmp\(.*ECO|strcmp\(.*SUN" src\ui
gcc -fsyntax-only -DLV_CONF_INCLUDE_SIMPLE=1 -DWINVER=0x0601 -I. -Isrc -Ilvgl -Ilv_drivers src\ui\views\menu_runtime.c src\ui\views\menu_actions.c src\ui\views\submenu_model.c src\ui\screen\screen.c
git diff --check
```

手动验证：

- `DIVE MENU -> BRIGHTNESS` 列表显示正确。
- 每一项选中后 badge 同步。
- 每一项选中后软件遮罩变化符合预期。
- 布局重建/翻转布局后亮度保持。
