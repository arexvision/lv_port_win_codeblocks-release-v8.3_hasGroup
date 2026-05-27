# 流程：给 GF / CONSERVATISM 增加一个档位

典型需求：在 `CONSERVATISM` 菜单里新增一个 `GF 50/60` 档位，选中后写入 `gf_low=50`、`gf_high=60`，SETUP badge 同步显示。

## 当前链路

```text
menu_defs.h
  定义 MENU_ITEM_CONSERVATISM_*

submenu_model.c
  s_conservatism_options[] 提供菜单文案和 badge

menu_runtime.c
  build_setup_conservatism() 绑定每一行的 item ID

menu_actions.c
  handle_conservatism() 根据 item ID 算 index，拿 option，调用 ui_on_conservatism_set()

callbacks.c
  ui_on_conservatism_set() 把档位映射成 GF low/high，再走 bus_set_gf_setting()

data.c
  bus_set_gf_setting() 写 g_sensor_data.gf_low/high 和 g_sys_config.conservatism
```

## 修改步骤

### 1. 增加 conservatism 枚举

文件：`src/ui/core/ui_engine.h`

在 `conservatism_level_t` 里加一个枚举，放在 `CONSERVATISM_COUNT` 前面。

示例：

```c
typedef enum
{
    CONSERVATISM_LOW = 0,
    CONSERVATISM_MED,
    CONSERVATISM_HIGH,
    CONSERVATISM_CUSTOM,
    CONSERVATISM_GF_50_60,
    CONSERVATISM_COUNT
} conservatism_level_t;
```

### 2. 增加菜单行 ID

文件：`src/ui/views/menu_defs.h`

在 `MENU_ITEM_CONSERVATISM_*` 一组里增加稳定行 ID。

示例：

```c
MENU_ITEM_CONSERVATISM_LOW,
MENU_ITEM_CONSERVATISM_MED,
MENU_ITEM_CONSERVATISM_HIGH,
MENU_ITEM_CONSERVATISM_CUSTOM,
MENU_ITEM_CONSERVATISM_GF_50_60,
```

### 3. 增加显示文案和 badge

文件：`src/ui/views/submenu_model.c`

在 `s_conservatism_options[]` 里增加一项。

示例：

```c
{ CONSERVATISM_GF_50_60, "GF 50/60", "50/60" },
```

这里是唯一的菜单文案来源。不要在 `submenu_view.c` 或 `menu_actions.c` 里重复写文案。

### 4. 把新行 ID 加入 runtime

文件：`src/ui/views/menu_runtime.c`

在 `build_setup_conservatism()` 的 `ids[]` 里增加新 ID。

示例：

```c
static const menu_item_id_t ids[] =
{
    MENU_ITEM_CONSERVATISM_LOW,
    MENU_ITEM_CONSERVATISM_MED,
    MENU_ITEM_CONSERVATISM_HIGH,
    MENU_ITEM_CONSERVATISM_CUSTOM,
    MENU_ITEM_CONSERVATISM_GF_50_60,
};
```

注意：`submenu_model.c` 的文案表数量和这里的 `ids[]` 数量必须一致。

### 5. 增加 GF 实际映射

文件：`src/ui/core/callbacks.c`

在 `ui_on_conservatism_set()` 里的 `gf_table` 增加 `{ 50, 60 }`。

示例：

```c
static const uint8_t gf_table[][2] =
{
    { 40, 95 },
    { 40, 85 },
    { 30, 70 },
    { 50, 70 },
    { 50, 60 },
};
```

### 6. 让反向识别也能认出 50/60

文件：`src/ui/core/data.c`

如果 TCP / 算法通过 `bus_set_gf_setting(50, 60)` 写入，希望 SETUP badge 也显示新档位，就要更新 `conservatism_from_gf()`。

示例：

```c
if (gf_low == 50U && gf_high == 60U) return CONSERVATISM_GF_50_60;
```

## 检查点

- `CONSERVATISM_COUNT` 自动变大，`s_conservatism_dyn[CONSERVATISM_COUNT + 1]` 不需要手改。
- `ids[]` 数量必须等于 `s_conservatism_options[]` 数量。
- `gf_table` 顺序必须和 `conservatism_level_t` 枚举顺序一致。
- 不要用 `strcmp("GF 50/60")` 判断选择。

## 验证

```powershell
rg -n "CONSERVATISM_GF_50_60|GF 50/60|50, 60" src\ui
gcc -fsyntax-only -DLV_CONF_INCLUDE_SIMPLE=1 -DWINVER=0x0601 -I. -Isrc -Ilvgl -Ilv_drivers src\ui\views\menu_runtime.c src\ui\views\menu_actions.c src\ui\views\submenu_model.c src\ui\core\callbacks.c src\ui\core\data.c
git diff --check
```

手动验证：

- 进入 `DIVE MENU -> CONSERVATISM` 能看到 `GF 50/60`。
- 选中后 SETUP badge 显示 `50/60`。
- 组织/GF 显示刷新为 `50/60`。
- TCP 或算法写入 `50/60` 后 badge 能反向显示对应档位。
