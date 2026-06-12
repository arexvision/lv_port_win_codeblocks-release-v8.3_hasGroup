# UI 修改流程索引

这组文档按“我要改什么”来分，每个流程只讲一个典型场景。以后先从这里选入口，不要直接全项目搜索乱改。

## 典型流程

| 需求 | 看哪个文档 |
|---|---|
| 给 CONSERVATISM / GF 增加一个档位，比如 50/60 | `FLOW_ADD_GF_PRESET.md` |
| 给 BRIGHTNESS 这种简单列表增加/删除一个选项 | `FLOW_CHANGE_SIMPLE_ENUM_SETTING.md` |
| 给 DIVE SETUP 增加一个数值编辑项，比如 MOD PO2 / 深度报警 | `FLOW_ADD_NUMERIC_EDIT_SETTING.md` |
| 增加一个新的子菜单页，比如 DISPLAY 下面再加一层 | `FLOW_ADD_SUBMENU_PAGE.md` |
| 增加一个右侧 tileview 页面，比如新业务卡片 | `FLOW_ADD_RIGHT_PAGE.md` |
| 算法/TCP 新增一个数据字段并显示到 UI | `FLOW_ADD_DATA_FIELD_TO_UI.md` |
| 真机侧触发/解除警告，或新增固定警告 ID | `ALARM_SYSTEM_PORTING_GUIDE.md` |
| 算法口径、计算参数、真机回调移植规则 | `规则书.md` |

## 总规则

- 菜单业务判断走 `menu_id_t` / `menu_item_id_t`。
- 显示文字只给 LVGL label，不参与业务分发。
- 数据写入优先走 `bus_set_*()`。
- 可信菜单 index / enum 不额外加 clamp。
- TCP、BLE、文件、协议输入属于不可信边界，需要校验。
- 不新增项目名前缀。
- 涉及算法计算、派生显示或真机业务回调时，必须同步更新 `规则书.md` 和相关专题文档。

## 快速判断改哪里

```text
改菜单入口/行 ID
  -> views/menu_defs.h/c

改菜单当前页生成哪些 rows
  -> views/menu_runtime.c

改点了某一行做什么
  -> views/menu_actions.c

改动态文案/复杂编辑状态/DIVE PLAN
  -> views/submenu_model.c

改数据写入和 dirty mask
  -> core/data.c/h

改右侧页面注册和顺序
  -> screen/page_registry.h/c
```
