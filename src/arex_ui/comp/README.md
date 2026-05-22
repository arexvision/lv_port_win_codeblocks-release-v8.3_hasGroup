# comp

`comp` 是 component 层，负责左侧固定区和 5F 自定义网格里可复用的数据组件。它和 `screen/` 的区别是：`screen/` 决定组件放在哪，`comp/` 决定组件本身怎么创建和怎么刷新。

## 文件职责

| 文件 | 作用 |
|---|---|
| `arex_comp_view.h/c` | 组件工厂，按 `comp_id_t` 创建 DEPTH、NDL、GAS、POD、TEMP、SYS 等 LVGL 对象，并保存运行态句柄。 |
| `arex_comp_update.h/c` | 组件数据刷新，提供 `comp_set_value()`、`comp_set_text()`、`comp_sync_data()`。 |
| `arex_comp_style.h/c` | 组件样式查询和样式应用入口。 |
| `arex_comp_style_types.h` | 组件尺寸、字体、元素显示策略和样式配置类型。 |

## 典型职责拆分

```text
screen/arex_layout_view.c
  -> 计算组件 x/y/w/h
  -> comp/arex_comp_view.c 创建组件
  -> comp/arex_comp_update.c 刷新值
  -> comp/arex_comp_style.c 应用样式
```

## 修改入口

- 改组件内部 label、单位、bar、icon 结构：看 `arex_comp_view.c`。
- 改组件显示的数据格式或取值路由：看 `arex_comp_update.c`。
- 改组件尺寸元数据、字体、边框、颜色：看 `arex_comp_style*`。
- 新增 `comp_id_t` 后，通常需要同时检查 `core/arex_ui_engine.h` 和这里的创建/刷新逻辑。
