# 流程：修改现有显示文案、badge 或组件样式

典型需求：

- SETUP 顶层 badge 文案不对。
- INFO MENU 某个聚焦效果不对。
- 某个 card 的标题、单位、颜色、字号要改。
- 某个 widget 显示格式要改，比如温度、GF、PPO2。

这类需求优先判断是“显示问题”还是“业务数据问题”。不要一上来改 data 或 menu action。

## 判断入口

```text
顶层 INFO / DIVE MENU 列表显示
  -> src/ui/menus/menu_info.c
  -> src/ui/menus/menu_setup.c

子菜单抽屉行样式/焦点/动画
  -> src/ui/views/submenu_view.c

子菜单动态文案
  -> src/ui/views/submenu_model.c

右侧业务卡片显示
  -> src/ui/cards/card_*.c

左侧固定组件 / 5F 组件显示
  -> src/ui/comp/comp_view.c
  -> src/ui/comp/comp_update.c
  -> src/ui/comp/comp_style.c

右侧页面顺序和注册
  -> src/ui/screen/page_registry.c/h
```

## 例子 1：SETUP 顶层 badge 不对

比如 `BRIGHTNESS` badge 没跟选项同步。

优先看：

```text
menus/menu_setup.c
  顶层 badge 创建和刷新

views/submenu_model.c
  submenu_brightness_badge()

views/menu_actions.c
  handle_brightness() 选中后刷新 badge
```

不要在 `menu_setup.c` 再写一份亮度字符串表。badge 应该从 `submenu_model.c` 的选项表拿。

## 例子 2：子菜单聚焦不加粗

比如 INFO MENU 被聚焦时没有 DIVE MENU 那种加粗效果。

优先看：

```text
views/submenu_view.c
  子菜单抽屉每一行的焦点样式

menus/menu_info.c
menus/menu_setup.c
  顶层菜单页自己的焦点样式
```

判断它属于哪层：

- 右侧顶层菜单页：看 `menus/menu_*.c`。
- 进入抽屉后的二级/三级菜单：看 `submenu_view.c`。

## 例子 3：card 上某个值格式不对

比如 DECO card 的 `GF99` 想从 `85%` 改为 `GF99 85%`。

优先看：

```text
cards/card_deco.c
```

如果同一个值也在左侧 widget 出现，再看：

```text
comp/comp_update.c
comp/comp_view.c
```

不要为了改显示格式去改 `data.c`。`data.c` 应该保存原始值，不保存格式化字符串。

## 例子 4：INFO 子菜单详情文案不对

比如 `TISSUE & TOX` 里想多显示 `CEILING`。

优先看：

```text
views/submenu_model.c
  submenu_build_info_items()
```

如果新增字段来自算法/TCP，先按 `FLOW_ADD_DATA_FIELD_TO_UI.md` 接好 data bus，再回来改文案。

## 检查点

- 只是显示格式，不要改数据源。
- 只是焦点样式，不要改 menu action。
- 只是 badge 文案，优先找对应 option/badge 函数。
- 同一文案不要在多个文件复制一份表。
- 顶层菜单页和子菜单抽屉不是同一个文件。

## 验证

```powershell
git diff --check
gcc -fsyntax-only -DLV_CONF_INCLUDE_SIMPLE=1 -DWINVER=0x0601 -I. -Isrc -Ilvgl -Ilv_drivers src\ui\views\submenu_view.c src\ui\menus\menu_info.c src\ui\menus\menu_setup.c
```

手动验证：

- 普通态、聚焦态、编辑态都看一遍。
- 长文案是否被裁切或溢出。
- 布局翻转/重建后显示是否保持。
- 如果是 badge，选项变化后是否同步刷新。
