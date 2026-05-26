# menus

`menus` 放右侧 tileview 中的顶层菜单页：INFO MENU 和 DIVE MENU。
它们看起来占用右侧页面，但职责是菜单入口，不是业务卡片。

## 文件职责

| 文件 | 作用 |
|---|---|
| `menu_info.c` | INFO 顶层菜单页，注册 info list，打开 INFO 子菜单入口。 |
| `menu_setup.c` | DIVE MENU 顶层菜单页，显示设置入口和动态 badge。 |

## 规则

- 顶层菜单页放这里，不放 `cards/`。
- 普通业务卡片继续放 `cards/`。
- 具体子菜单定义、运行时和动作分发在 `../views/menu_defs.c`、`../views/menu_runtime.c`、`../views/menu_actions.c`。
- `screen/page_registry.c` 仍统一登记右侧 tileview 页面；这里的 `CARD_ID_INFO` 和 `CARD_ID_SETUP` 只是兼容别名，不表示源码还属于 card 模块。
