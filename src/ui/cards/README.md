# cards

`cards` 放右侧 tileview 里的独立业务卡片。这里的文件通常直接负责一整页业务 UI 的创建和刷新，再由 `screen/card_registry.c` 登记到右侧页面序列。

INFO MENU 和 DIVE MENU 虽然也显示在右侧 tileview 里，但职责是菜单入口，不属于业务卡片；它们已经放到 `../menus/`。

## 文件职责

| 文件 | 作用 |
|---|---|
| `card_compass.c/h` | 指南针横带、航向值和罗盘相关刷新入口。 |
| `card_deco.c` | 组织舱状态图、GF、CNS、OTU、减压相关显示。 |
| `card_gas.c` | 气体列表、当前气体、PPO2、MOD 和气体切换可用状态。 |
| `card_plan.c` | 潜水轨迹图、减压站预测图，并实现潜水轨迹点追加接口。 |
| `card_blank.c` | 空白占位页，保留 tile 序列空间。 |

## 修改入口

- 改右侧某个独立业务 UI：从对应 `card_*.c` 开始。
- 改 INFO MENU / DIVE MENU 顶层入口：去 `../menus/menu_info.c` 或 `../menus/menu_setup.c`。
- 新增业务卡片：新增卡片实现后，还要更新 `screen/card_registry.h/c` 和默认 `card_order`。
- 改左侧固定组件或 5F 组件：去 `../comp/`，不要把公共组件逻辑复制进 card 文件。
