# views

`views` 放覆盖在主卡片之上的交互视图，当前主要是弹窗和子菜单抽屉。

## 文件职责

| 文件 | 作用 |
|---|---|
| `modal_view.h/c` | 创建和控制 GAS、COMPASS、ACT、SETUP confirm 等弹窗，处理显示、隐藏和 pulse。 |
| `submenu_view.h/c` | 子菜单抽屉的 LVGL 对象、滑入/滑出、row 渲染、焦点样式、确认和返回行为；只消费 `menu_row_t`，不靠显示字符串做业务判断。 |
| `menu_defs.h/c` | 菜单定义层：稳定菜单 ID、行 ID、顶层 INFO/SETUP 定义表、标题、子菜单映射和 row type。 |
| `menu_runtime.h/c` | 菜单运行时层：当前菜单、父级栈、当前 rows、默认选中项、刷新与返回。 |
| `menu_actions.h/c` | 菜单动作层：根据 `menu_item_id_t` 执行设置、打开子菜单、弹窗、GAS/LIGHT/COMPASS/DIVE PLAN 入口。 |
| `submenu_model.h/c` | 菜单模型辅助：动态文案、复杂编辑流程、DIVE PLAN 渲染数据和设置值应用；不再按标题/行文本解析业务动作。 |

## 交互关系

```text
core/ui_state.c
  -> screen 门面 API
  -> submenu_view / modal_view
  -> menu_runtime 提供当前 rows
  -> menu_actions 按 row.id 执行业务动作
  -> submenu_model 提供动态文案和复杂编辑状态
```

## 修改入口

- 改弹窗布局和动画：看 `modal_view.c`。
- 改子菜单抽屉外观、列表选中态、滑动行为：看 `submenu_view.c`。
- 改菜单顶层文案、入口 ID、简单枚举设置项：优先看 `menu_defs.c`、`menu_runtime.c`、`menu_actions.c`。
- 改 DIVE PLAN、内联数值编辑或动态文案：看 `submenu_model.c`。
- 改“按键进入哪个状态”：回到 `core/ui_state.c`，不要只在 view 内补状态分支。

## 规则

- 菜单业务逻辑必须使用 `menu_id_t`、`menu_item_id_t` 和 row type。
- `menu_runtime_current_title()` / `menu_runtime_current_rows()` 只给渲染层使用。
- 禁止在选择/action 路径里用 `strcmp(title/text)` 判断业务分支。
