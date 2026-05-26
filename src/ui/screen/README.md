# screen

`screen` 负责屏幕级对象和布局安排。这里管“容器放在哪里、页面怎么挂上去、屏幕对外暴露什么 API”，不负责每个小组件内部怎么画。

## 文件职责

| 文件 | 作用 |
|---|---|
| `screen.h` | 屏幕门面 API，供状态机、卡片、告警和组件刷新调用。 |
| `screen.c` | 创建根屏、safe zone、左侧锚点、右侧 tileview、wall、dots、亮度遮罩，并承接弹窗/子菜单调用入口。 |
| `layout_view.h/c` | 计算屏幕布局，渲染左侧 2x7 锚点网格、5F 自定义网格、菜单行和卡片标题。 |
| `page_registry.h/c` | 定义 page ID、page position、页面注册表、`create_cb/update_cb` 和显示位置映射。 |

## 布局边界

```text
screen_create()
  -> 创建屏幕容器和 tileview
  -> page_registry 查页面定义
  -> layout_view 放置网格和卡片内容入口
```

## 修改入口

- 改 safe zone、左/右区域容器、tileview 滚动、wall-charge、dots：看 `screen.c`。
- 改 2x7 固定区或 5F 格子算法：看 `layout_view.c`。
- 新增右侧页面类型、调整 page order 映射：看 `page_registry.h/c`。
- 改某个 DEPTH 或 POD 组件内部 UI：不要在这里改，去 `../comp/`。
