# cards

`cards` 放右侧 tileview 中的业务卡片。

主要职责：

- `card_info.c`: INFO 主菜单。
- `card_setup.c`: SETUP 主菜单和 badge。
- `card_compass.c`: 指南针与航向显示。
- `card_deco.c`: 组织舱、减压、GF、CNS、OTU 显示。
- `card_gas.c`: 气体列表、PPO2、MOD 和气体切换显示。
- `card_plan.c`: 潜水轨迹、计划曲线和减压站图表。
- `card_blank.c`: 空白占位卡。

新增或调整右侧页面时，通常需要同时检查 `screen/arex_card_registry.c`。
