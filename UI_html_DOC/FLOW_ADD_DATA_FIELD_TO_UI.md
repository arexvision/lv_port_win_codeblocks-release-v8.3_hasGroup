# 流程：算法/TCP 新增数据字段并显示到 UI

典型需求：

- TCP 新增一个字段。
- 算法输出一个新值。
- UI 某个 card 或 widget 要显示这个值。
- 比如新增 `ceiling_m`、`gas_density`、`tissue_pct[16]`、`gf99`。

## 当前链路

```text
TCP / 算法 / 模拟器
  -> bus_set_*()
  -> g_sensor_data / g_sys_config
  -> dirty_mask
  -> update_router.c
  -> card / comp / submenu 刷新
```

核心规则：外部输入不要直接写 `g_sensor_data`，要走 `bus_set_*()`。

## 例子：新增一个算法输出 `foo_value`

### 1. 在 sensor_data_t 增加字段

文件：`src/ui/core/ui_engine.h`

示例：

```c
float foo_value;
```

放在语义接近的位置，比如算法相关值就放在 GF / deco 旁边。

### 2. 增加 dirty bit

文件：`src/ui/core/ui_engine.h`

在 dirty mask enum 里增加：

```c
DIRTY_FOO = (1U << xx),
```

注意不要和已有 bit 冲突。如果 bit 快用完，要先整体整理 dirty mask。

### 3. 增加 bus setter

文件：

```text
src/ui/core/data.h
src/ui/core/data.c
```

声明：

```c
void bus_set_foo_value(float value);
```

实现：

```c
void bus_set_foo_value(float value)
{
    if (g_sensor_data.foo_value != value)
    {
        g_sensor_data.foo_value = value;
        g_sensor_data.dirty_mask |= DIRTY_FOO;
    }
}
```

如果这个值来自 TCP/BLE/文件，要在 setter 或解析层做范围校验。

### 4. 让 update_router 分发刷新

文件：`src/ui/core/update_router.c`

如果它影响 widget：

```c
if (mask & DIRTY_FOO)
{
    screen_refresh_all_widgets();
}
```

如果它影响某个 card：

```c
if (mask & DIRTY_FOO)
{
    card_deco_update();
}
```

如果它影响 INFO 子菜单，也要把 `DIRTY_FOO` 加到 `screen_refresh_info_submenu_if_open()` 那个 mask 组合里。

### 5. 显示到 widget 或 card

如果是固定组件：

文件：`src/ui/comp/comp_update.c` 或 `src/ui/comp/comp_view.c`

增加对应 comp ID 的显示逻辑。

如果是右侧 card：

文件：`src/ui/cards/card_xxx.c`

在 update 函数中读取 `g_sensor_data.foo_value`。

如果是 INFO 子菜单：

文件：`src/ui/views/submenu_model.c`

在 `submenu_build_info_items()` 对应页面里生成文案。

### 6. 接入 TCP / 算法

调用统一 setter：

```c
bus_set_foo_value(foo);
```

不要在 TCP 或算法模拟代码里直接写：

```c
g_sensor_data.foo_value = foo;
```

## 数组字段例子：16 组织舱

16 组织舱是数组，不能逐字节裸写。

当前做法：

```c
void bus_set_tissues(const uint8_t tissue_pct[16]);
```

写入时：

```c
bus_set_tissues(tissue_load);
```

UI 显示读取：

- `card_deco.c` 画组织柱。
- `submenu_model.c` 的 TISSUE & TOX 页面取最大组织负荷。

## 检查点

- 新字段是否初始化。
- 是否有对应 dirty bit。
- 外部输入是否走 `bus_set_*()`。
- update_router 是否刷新对应 UI。
- INFO 子菜单打开时是否能实时刷新。
- 数组/结构体写入是否需要临界区保护。

## 验证

```powershell
rg -n "foo_value|DIRTY_FOO|bus_set_foo_value" src\ui
gcc -fsyntax-only -DLV_CONF_INCLUDE_SIMPLE=1 -DWINVER=0x0601 -I. -Isrc -Ilvgl -Ilv_drivers src\ui\core\data.c src\ui\core\update_router.c
git diff --check
```

手动验证：

- TCP/算法写入后 UI 能刷新。
- 值不变时不会无限刷新。
- 布局重建后仍能显示当前值。
- INFO 子菜单打开时能看到最新值。
