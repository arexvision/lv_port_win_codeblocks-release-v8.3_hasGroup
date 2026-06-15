# UI Information Architecture

本页是 `UI_INFORMATION_ARCHITECTURE_WBS.puml` 的说明文件。WBS 图用于表达“信息架构树”，视觉形态接近功能树/产品结构图，而不是甘特图。

## 文件

- `UI_INFORMATION_ARCHITECTURE_WBS.puml`：PlantUML WBS 源文件，包含当前 UI 的页面、菜单、设置、组件、运行数据和告警树。
- `UI_MODULE_MAP.md`：模块职责和调用边界说明。

## 如何渲染

可以用任意 PlantUML 工具渲染：

```bash
plantuml UI_html_DOC/UI_INFORMATION_ARCHITECTURE_WBS.puml
```

如果使用 VS Code，可安装 PlantUML 插件后直接预览 `.puml` 文件。

## 维护规则

这张树应该跟代码中的稳定 ID 保持一致：

- 页面 ID 来自 `src/ui/screen/page_registry_types.h`
- 菜单 ID 和菜单项 ID 来自 `src/ui/views/menu_defs.h`
- 默认布局和默认配置来自 `src/ui/core/ui_engine.c`
- 组件 ID 来自 `src/ui/core/ui_defs.h`
- 运行数据字段来自 `src/ui/core/ui_types.h`
- 告警 ID 和靶向组件来自 `src/ui/alarm/alarm.h`、`src/ui/alarm/alarm.c`

菜单业务逻辑仍然以 `menu_id_t` / `menu_item_id_t` 为准；WBS 里的显示文案只用于阅读和产品沟通。

## 更新建议

新增页面、菜单或设置项时，先更新代码里的稳定 ID，再同步更新 WBS 对应节点。新增自定义卡片或默认布局时，同时更新 `默认自定义卡片` 和 `页面结构 TileView` 两段。
