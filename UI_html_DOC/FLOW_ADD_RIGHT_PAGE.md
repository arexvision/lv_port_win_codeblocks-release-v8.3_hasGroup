# 流程：增加一个右侧 tileview 页面

典型需求：新增一个右侧页面，比如：

- 新业务卡片 `SENSOR`
- 新图表页 `PROFILE`
- 新工具页 `LOG`

注意：INFO MENU / DIVE MENU 是顶层菜单入口，放在 `menus/`；普通业务页面放在 `cards/`。

## 当前链路

```text
screen/page_registry.h
  定义 PAGE_ID_*

screen/page_registry.c
  注册 create/update/on_enter 回调

cards/card_xxx.c
  实现页面 UI

core/ui_engine.c
  默认 card_order 页面顺序

core/data.c
  BLE/TCP 下发 card_order 时映射页面

LittlevGL.cbp
  如果新增 .c/.h，需要登记到 CodeBlocks 工程
```

## 例子：新增 SENSOR 页面

### 1. 增加 page ID

文件：`src/ui/screen/page_registry.h`

示例：

```c
PAGE_ID_SENSOR,
PAGE_ID_COUNT
```

重要：如果 BLE 旧协议依赖 page ID 数值，不要随便插入到中间。优先追加到末尾，并确认协议是否允许。

### 2. 新增页面实现文件

文件建议：

```text
src/ui/cards/card_sensor.c
src/ui/cards/card_sensor.h
```

最小接口：

```c
lv_obj_t *card_sensor_create(lv_obj_t *parent);
void card_sensor_update(void);
void card_sensor_on_enter(void);
```

如果不需要 `on_enter`，可以给 `NULL`。

### 3. 注册 page

文件：`src/ui/screen/page_registry.c`

增加 extern：

```c
lv_obj_t *card_sensor_create(lv_obj_t *parent);
void card_sensor_update(void);
```

在 `g_pages[]` 增加：

```c
[PAGE_ID_SENSOR] = {
    .id          = PAGE_ID_SENSOR,
    .storage_pos = PAGE_POS_DYNAMIC,
    .create_cb   = card_sensor_create,
    .update_cb   = card_sensor_update,
    .on_enter_cb = NULL,
    .root        = NULL,
},
```

实际字段按当前 `page_t` 定义填写。

### 4. 放入默认页面顺序

文件：`src/ui/core/ui_engine.c`

在 `sys_config_defaults()` 的 `card_order` 默认顺序里加入：

```c
cfg->card_order[PAGE_POS_6] = PAGE_ID_SENSOR;
```

如果不想默认显示，就不加，让 APP 下发时再放进顺序。

### 5. 处理 BLE/TCP 下发顺序

文件：`src/ui/core/data.c`

`bus_set_ui_layout()` 会读取 payload 的 `card_order[]`。如果新 page ID 会从外部下发，要确认：

- payload 允许这个 ID。
- `PAGE_ID_UNUSED` 和 `PAGE_ID_BLANK` 语义不变。
- 不要破坏旧 `card_order[8]` 兼容规则。

### 6. 更新 CodeBlocks 工程

如果新增 `.c/.h` 文件，必须更新 `LittlevGL.cbp`。

只在明确新增源文件时改工程文件；普通逻辑修改不要碰它。

## 检查点

- `PAGE_ID_*` 数值是否会影响 BLE 旧协议。
- 新页面是否应该属于 `cards/`，不是 `menus/`。
- 页面 create/update/on_enter 是否注册完整。
- 默认顺序是否真的需要新增页面。
- 新 `.c` 是否已登记 `LittlevGL.cbp`。

## 验证

```powershell
rg -n "PAGE_ID_SENSOR|card_sensor" src LittlevGL.cbp
gcc -fsyntax-only -DLV_CONF_INCLUDE_SIMPLE=1 -DWINVER=0x0601 -I. -Isrc -Ilvgl -Ilv_drivers src\ui\screen\page_registry.c src\ui\cards\card_sensor.c
git diff --check
```

手动验证：

- 程序启动后页面顺序正确。
- dot 数量正确。
- 滑动能进入新页面。
- 数据更新能刷新新页面。
- 布局重建后页面还在。
