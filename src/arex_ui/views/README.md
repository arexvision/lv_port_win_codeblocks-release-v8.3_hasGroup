# views

`views` 放覆盖在主卡片之上的交互视图，当前主要是弹窗和子菜单抽屉。

## 文件职责

| 文件 | 作用 |
|---|---|
| `arex_modal_view.h/c` | 创建和控制 GAS、COMPASS、ACT、SETUP confirm 等弹窗，处理显示、隐藏和 pulse。 |
| `arex_submenu_view.h/c` | 子菜单抽屉的 LVGL 对象、滑入滑出、列表填充、选中态、确认和返回行为。 |
| `arex_submenu_model.h/c` | 子菜单数据模型，构建 INFO/SETUP 条目、嵌套菜单、设置值映射和内联编辑规格。 |

## 交互关系

```text
core/arex_ui_state.c
  -> screen 门面 API
  -> submenu view / modal view
  -> submenu model 提供菜单内容
```

## 修改入口

- 改弹窗布局和动画：看 `arex_modal_view.c`。
- 改子菜单抽屉外观、列表选中态、滑动行为：看 `arex_submenu_view.c`。
- 改菜单文案、菜单层级、某个设置项映射：看 `arex_submenu_model.c`。
- 改“按键进入哪个状态”：回到 `core/arex_ui_state.c`，不要只在 view 内补状态分支。
