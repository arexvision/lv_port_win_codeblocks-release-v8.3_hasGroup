# 流程：增加一个数值编辑设置

典型需求：新增一个可以用旋钮调数字的设置，比如：

- `MOD PO2: 1.4`
- `DEPTH ALARM: 40m`
- `TIME ALARM: 60min`
- `DATE & CLOCK` 的年月日时分

## 当前链路

```text
menu_defs.h
  增加 MENU_ITEM_*

menu_runtime.c
  把新 item ID 放进对应菜单的 ids[]

submenu_model.c
  生成菜单行显示文案

menu_actions.c
  edit_spec_for_id() 返回编辑规格

screen.c
  编辑控件根据 edit_spec 显示和旋转

submenu_model.c
  submenu_apply_edit_value() 更新模型状态

callbacks.c / data.c
  真正业务写入
```

## 例子：增加一个 PPO2 Warning 设置

假设要在 `DIVE SETUP` 里增加 `PPO2 WARN: 1.3`，范围 `1.0~1.6`，步进 `0.1`。

### 1. 增加 setting kind

文件：`src/ui/views/submenu_model.h`

在 `submenu_setting_kind_t` 里增加：

```c
SUBMENU_SETTING_PPO2_WARN,
```

### 2. 增加菜单行 ID

文件：`src/ui/views/menu_defs.h`

在 DIVE SETUP 相关 ID 旁边增加：

```c
MENU_ITEM_DIVE_PPO2_WARN,
```

### 3. 增加模型状态和动态文案

文件：`src/ui/views/submenu_model.c`

新增状态：

```c
static float s_ppo2_warn = 1.3f;
static char s_ppo2_warn_str[24];
```

在 `build_nested_dive_setup()` 里生成文案：

```c
snprintf(s_ppo2_warn_str, sizeof(s_ppo2_warn_str), "PPO2 WARN: %.1f", (double)s_ppo2_warn);
```

然后把它插入 `s_nested_dive_setup[]`。

注意：插入一行后，`s_nested_dive_setup` 数组大小和 `out_count` 上限也要同步。

### 4. 在 runtime 绑定行 ID

文件：`src/ui/views/menu_runtime.c`

在 `MENU_DIVE_SETUP` 的 `ids[]` 增加：

```c
MENU_ITEM_DIVE_PPO2_WARN,
```

位置必须和 `build_nested_dive_setup()` 生成的文案位置一致。

### 5. 返回编辑规格

文件：`src/ui/views/menu_actions.c`

在 `edit_spec_for_id()` 增加：

```c
case MENU_ITEM_DIVE_PPO2_WARN:
    edit_spec_prepare(out_spec,
                      SUBMENU_SETTING_PPO2_WARN,
                      0U,
                      submenu_ppo2_warn(),
                      1.0f,
                      1.6f,
                      0.1f,
                      1U,
                      "PPO2 WARN:");
    return true;
```

如果需要读取当前值，就在 `submenu_model.h/c` 加一个 getter：

```c
float submenu_ppo2_warn(void);
```

### 6. 应用编辑值

文件：`src/ui/views/submenu_model.c`

在 `submenu_apply_edit_value()` 增加：

```c
case SUBMENU_SETTING_PPO2_WARN:
    s_ppo2_warn = value;
    break;
```

如果它要影响算法或系统配置，不要只停在 model，要继续调用明确的业务回调或 `bus_set_*()`。

### 7. 真正业务写入

优先做法：

- 在 `core/data.h/c` 增加 `bus_set_ppo2_warn(float value)`。
- `menu_actions.c` 或 `screen.c` 编辑完成时触发业务回调。

如果只是 UI 本地显示，可以先存在 `submenu_model.c`。

## 检查点

- 文案数组顺序和 `ids[]` 顺序必须一致。
- `edit_spec_for_id()` 必须按 `menu_item_id_t` 判断，不看 label。
- 数值范围来自 edit spec，不在菜单 index 上加无意义 clamp。
- 外部输入写入同一设置时，必须走 data/bus 边界校验。

## 验证

```powershell
rg -n "PPO2_WARN|MENU_ITEM_DIVE_PPO2_WARN|SUBMENU_SETTING_PPO2_WARN" src\ui
gcc -fsyntax-only -DLV_CONF_INCLUDE_SIMPLE=1 -DWINVER=0x0601 -I. -Isrc -Ilvgl -Ilv_drivers src\ui\views\menu_runtime.c src\ui\views\menu_actions.c src\ui\views\submenu_model.c src\ui\screen\screen.c
git diff --check
```

手动验证：

- 进入对应菜单能看到新行。
- 按确认进入编辑态。
- 旋钮调整范围和步进正确。
- 退出后菜单文案刷新。
- 如果有业务效果，对应 card/widget 同步刷新。
