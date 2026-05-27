# UI 架构优化逻辑说明

这份文档记录本轮架构收敛的思路：为什么要改、改到哪里为止、以后修改设置项或页面时应该走哪条路径。

## 优化目标

本轮优化不是重写整个 UI，而是把最容易反复出问题的区域收稳：

- 菜单业务不能再靠显示文字判断。
- 简单设置项要有稳定入口，避免改一个选项要同时改多个文件。
- 数据源归属要清楚，避免 core 反向依赖 card。
- `cards/`、`menus/`、`views/` 的职责要分开，文件名和目录名不再误导。
- 不保留无用兼容函数；旧路径不用了就删除。

最终目标是：以后改亮度、GF、盐度、最后减压停留这类设置时，先找定义和动作，不再在 view 里猜字符串。

## 当前主结构

当前 UI 主链路保持不动：

```text
core/data.c
  -> 写入传感器/配置数据，打 dirty mask
  -> core/update_router.c 根据 dirty mask 分发刷新
  -> screen/page_registry.c 管右侧页面注册和顺序
  -> cards/menus/views 负责具体页面和交互
```

这条大分层是合理的，所以没有继续重构 `screen/card/update_router` 主架构。

## 菜单优化逻辑

旧结构的问题是：菜单点击后经常通过标题或行文字判断业务。

旧模式类似：

```c
if (strcmp(cur_title, "BRIGHTNESS") == 0) { ... }
if (strcmp(text, "ECO") == 0) { ... }
```

这种结构会导致几个问题：

- 改显示文案会影响业务逻辑。
- 删除一个选项时，多个文件都要同步改。
- 同一个概念可能出现多份表，比如亮度标签、遮罩透明度、badge 文案各写一份。
- 不容易看出某一行到底是什么业务项。

现在改成 ID 驱动：

```text
menu_defs.c
  定义菜单 ID 和行 ID

menu_runtime.c
  根据当前 menu_id_t 生成 menu_row_t[]

submenu_view.c
  只负责画 LVGL 行和焦点样式

menu_actions.c
  根据 menu_item_id_t 执行业务动作

submenu_model.c
  只提供动态文案、复杂编辑状态、DIVE PLAN 数据和设置值应用
```

现在点击路径是：

```text
用户选择一行
  -> submenu_view 拿当前 row
  -> menu_actions_handle_select(row)
  -> menu_actions 按 row.id 分发
```

显示文字只用于 label，不参与业务判断。

## 为什么还保留 submenu_model

`submenu_model.c` 没有被完全删除，是因为它现在还保存这些状态和文案：

- INFO 详情页动态文案。
- GAS SWITCH 动态气体列表。
- DIVE PLAN 输入、结果、分页和渲染数据。
- NITROX / 3 GAS / OC Tech 这类复杂编辑状态。
- 一些设置值应用函数。

但旧的字符串解析后端已经删除：

- `submenu_setting_from_selection()`
- `submenu_direct_setting_from_selection()`
- `submenu_edit_spec_from_selection()`
- `submenu_nested_items_for()`
- `submenu_child_items_for()`

也就是说，`submenu_model` 现在是“模型辅助”，不是“菜单业务分发后端”。

## 减压站数据优化逻辑

旧结构里 `g_deco_stops` 和 `g_deco_stop_count` 定义在 `card_plan.c`，但是 `core/data.c` 会通过 `extern` 写入它们。

这属于层级倒挂：

```text
core/data.c 依赖 card_plan.c 的全局变量
```

问题是 data bus 是数据入口，card 应该只是消费和绘制，不应该反过来拥有核心数据。

现在调整为：

```text
core/data.c
  持有 g_deco_stops / g_deco_stop_count
  bus_set_deco_plan() 统一写入

cards/card_plan.c
  只读取减压站序列并绘制轨迹
```

这样 TCP、算法、模拟器、UI 都可以通过同一个 data bus 写入减压站，不会让某个 card 成为数据源。

## 目录命名收敛

右侧 tileview 里有两类东西：

- 业务卡片：COMPASS、DECO、GAS、PLAN、CUSTOM GRID、BLANK。
- 顶层菜单页：INFO MENU、DIVE MENU。

因此现在目录职责是：

```text
cards/
  只放业务卡片

menus/
  放 INFO MENU 和 DIVE MENU 顶层入口

views/
  放子菜单抽屉、弹窗、菜单定义/运行时/动作

screen/page_registry.c
  统一注册右侧 tileview 页面
```

`PAGE_ID_*` 是 UI 内部新概念；`CARD_ID_*` 只作为旧协议兼容别名保留，不代表源码还把菜单当 card。

## 以后怎么改一个设置项

以新增或修改一个简单枚举设置为例，优先按这个顺序看：

1. `views/menu_defs.h/c`
   定义稳定的 `menu_id_t` / `menu_item_id_t`。

2. `views/menu_runtime.c`
   根据当前 `menu_id_t` 生成 rows，把每一行绑定到稳定 `menu_item_id_t`。

3. `views/menu_actions.c`
   根据 `menu_item_id_t` 写入设置、弹确认框或打开子菜单。

4. `views/submenu_model.c`
   只在需要动态文案、编辑值、复杂状态时补模型数据。

5. `core/data.c`
   如果设置会影响系统配置或算法输入，优先通过 `bus_set_*()` 收口。

不要把业务判断写进 `submenu_view.c`，也不要用 label/title 做 `strcmp` 分发。

## 当前还值得继续优化的点

这些是后续低风险收口，不是大架构重构：

1. 清理调试输出。
   `screen.c` 和 `data.c` 里还有 `[BUS]`、`[DOTS]`、`[REBUILD_LAYOUT]` 等临时日志，建议统一成 debug 宏或删除。

2. 设置写入继续走 data bus。
   `mod_ppo2`、`conservatism` 还有少量直接写 `g_sys_config` 的路径，建议补 `bus_set_mod_ppo2()` 这类明确入口。

3. 屏幕重建后的 dirty 重排。
   `screen_rebuild_layout()` 里还有直接写 dirty mask 的地方，可以改成 `bus_requeue_dirty()`。

4. `update_router.c` 刷新合并。
   现在多个 dirty 分支可能重复刷新 widget。后续可以先累计刷新标记，最后统一刷新一次。

5. `submenu_model.c` 继续瘦身。
   如果 DIVE PLAN 继续变复杂，可以拆成独立 plan model；但不需要再改菜单主架构。

## 判断一段代码是否走错方向

看到下面几种写法，就要停下来确认：

```c
strcmp(title, "...")
strcmp(row_text, "...")
g_sensor_data.xxx = ...
g_sys_config.xxx = ...
```

一般规则：

- 菜单业务看 `menu_item_id_t`。
- 页面状态看 `menu_id_t`。
- 显示文字只给 LVGL label。
- 数据写入优先走 `bus_set_*()`。
- 可信的菜单 index / enum 不额外加 clamp。
- 外部输入、协议、文件、TCP、BLE 这类不可信边界才做范围校验。

## 一句话总结

这轮优化的核心不是“拆更多文件”，而是把判断依据从“屏幕上显示了什么字”改成“这一行稳定代表什么业务 ID”，再把数据所有权从具体 card 收回到 core data bus。这样以后改设置项时，改的是定义和动作，不是到处追字符串。
