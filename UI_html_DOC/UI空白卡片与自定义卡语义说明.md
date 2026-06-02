# UI 空白卡片与自定义卡语义说明

## 背景

旧版本没有独立空白卡片，APP/BLE 如果想让某一页看起来为空，通常会下发一个 `CUSTOM` 自定义卡片，并让它的 `widgets` 为空。这样硬件端会把“空自定义卡”当作空白页处理，连标题也不显示。

新版本 `diy_layout.proto` 已新增独立空白卡片：

- `DynamicCardType.CUSTOM = 5`
- `DynamicCardType.BLANK = 6`

因此现在需要把“空白卡片”和“空自定义卡”拆成两个不同语义。

## 当前语义

### BLANK 卡片

`BLANK` 是真正的空白卡片。

- 页面可滑到。
- 页面不显示标题。
- 页面不显示任何模块。
- 页面只保留黑色背景。
- 不占用 `custom_cards[]` 自定义卡片槽位。

硬件端映射链路：

- `DIY_DYNAMIC_CARD_BLANK`
- `UI_LAYOUT_RUNTIME_CARD_ID_BLANK`
- `PAGE_ID_BLANK`
- `card_blank_create()`

### CUSTOM 自定义卡片

`CUSTOM` 是自定义卡片，即使没有任何模块，也仍然是自定义卡片。

- 页面可滑到。
- `card_name` 有值时显示 `card_name`。
- `card_name` 为空时显示默认标题 `CUSTOM WIDGETS`。
- `widgets` 可以为空。
- `widgets` 为空时只显示标题，不显示模块。
- 占用 `custom_cards[]` 自定义卡片槽位。

硬件端映射链路：

- `DIY_DYNAMIC_CARD_CUSTOM`
- `UI_LAYOUT_RUNTIME_CARD_ID_CUSTOM_GRID`
- `PAGE_ID_CUSTOM_GRID`
- `render_5f_custom_grid()`

## APP/BLE 下发建议

如果希望某页完全空白，下发：

```text
card_type = BLANK
widgets = []
card_name = ""
```

如果希望某页是一个自定义卡，但暂时还没有模块，下发：

```text
card_type = CUSTOM
card_name = "自定义标题"
widgets = []
```

这时硬件端会显示标题，不会把它当成空白页。

## 模拟器验证

PC 模拟器 TCP debug 已支持：

```text
layout empty
```

该命令会生成一个测试布局：

- 一个空 `CUSTOM` 自定义卡片，用于验证“空自定义卡显示标题”。
- 一个 `BLANK` 空白卡片，用于验证“真正空白页什么都不显示”。

也可以通过模拟器自动布局轮换观察该场景。

## 注意事项

- `BLANK` 不再被 page registry 折叠跳过，它是有效可见页面。
- dot 高亮计算会把 `BLANK` 计入动态页数量，避免滑到空白页时圆点错位。
- 旧 `ble_ui_sync_payload_t` 模拟器入口也已兼容：只要 `card_order` 中存在 `PAGE_ID_CUSTOM_GRID`，即使 `custom_5f_count = 0`，也会创建自定义卡并显示标题。
