# picture

`picture` 放 LVGL 图片和图标资源。

主要职责：

- 上升/下降速率等级图标。
- 气瓶、手电筒、流转灯等图标资源。
- 供组件和卡片通过 LVGL image source 直接引用。

这里通常只放资源定义，不写业务逻辑。新增图片后需要确认对应 `.c` 文件已加入 CodeBlocks 工程。
