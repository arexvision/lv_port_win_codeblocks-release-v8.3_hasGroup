# cards

`cards` 放右侧 tileview 里的整页业务卡片。每张卡片通常暴露 `create` 和 `update` 函数，再由 `screen/arex_card_registry.c` 注册。

## 文件职责

| 文件 | 作用 |
|---|---|
| `card_info.c` | INFO 主菜单页，注册 info list，向子菜单系统提供入口。 |
| `card_setup.c` | SETUP 主菜单页，显示设置入口和动态 badge。 |
| `card_compass.c/h` | 指南针横带、航向值和罗盘相关刷新入口。 |
| `card_deco.c` | 组织舱柱状图、GF、CNS、OTU、减压相关显示。 |
| `card_gas.c` | 气体列表、当前气体、PPO2、MOD 和气体切换可用状态。 |
| `card_plan.c` | 潜水轨迹图、减压站预测图，并实现潜水轨迹点追加接口。 |
| `card_blank.c` | 空白占位页，保留 tile 序列空间。 |

## 修改入口

- 改右侧某一整页业务 UI：直接从对应 `card_*.c` 开始。
- 新增卡片：新增卡片实现后，还要更新 `screen/arex_card_registry.h/c` 和默认 `card_order`。
- 改左侧固定组件或 5F 组件：去 `../comp/`，不要把公共组件逻辑复制进 card 文件。
