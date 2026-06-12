# AREX 算法与模拟器同步变更记录

## 目的

本文档专门记录每次 AREX 算法包升级后，PC 模拟器侧为了保持与真机计算口径一致而做的适配。

记录范围包括：

- 模拟器是否删除了本地公式，改为调用算法接口。
- 模拟器是否调整了算法输出字段的使用口径。
- UI 是否只做生命周期/展示门控，而不再复刻算法判断。
- 静态库、头文件、文档和工程链接路径是否同步。

原则：只要算法层已经提供计算接口，模拟器和主工程都必须调用算法接口，不在 UI 或模拟器里重复实现公式。

## 0.0.17 -> 0.0.18

### 算法包变化

本次从 AREX Deco Core `0.0.17` 升级到 `0.0.18`。

算法层新增/明确了以下接口和字段：

- 新增 `arex_deco_calculate_gas_mod()`，用于按算法压力模型计算气体 MOD。
- 新增 `arex_deco_recommend_gas()`，用于 runtime 查询当前状态下推荐切换气体。
- `ArexDecoStop` 新增 `hold_seconds` 和 `switch_penalty_seconds`。
- `duration_seconds` 明确为 planning 总时长，包含物理停留和切气惩罚。
- `hold_seconds` 明确为当前站物理停留时间，不包含切气惩罚。

### 模拟器公式/口径调整

#### MOD 计算

旧口径：

- 模拟器本地按 PPO2、氧浓度、水面压力、水密度推导 MOD。
- 菜单生成气体槽时也有一套 UI 层 MOD 公式。

新口径：

- 模拟器新增 `deco_core_calculate_gas_mod()`，内部调用 `arex_deco_calculate_gas_mod()`。
- UI 菜单通过 `bus_calculate_gas_mod()` 请求算法层计算 MOD。
- 实时 active gas 的 MOD 显示通过 `core_mod_depth_for_gas()` 调用 `arex_deco_calculate_gas_mod()`。
- 删除 UI/模拟器侧 MOD 公式复刻。

影响：

- 模拟器、真机和算法文档中的 MOD 口径统一。
- 海水/淡水、水面压力、气体最大 PPO2 等影响因素由算法层统一处理。

#### 气体最大 PPO2

旧口径：

- 模拟器曾通过气体槽 MOD 反推 `max_ppo2_bar`。

新口径：

- 气体传入算法时，`max_ppo2_bar` 直接使用 UI 配置的 `MOD PPO2`。
- 不再从 MOD 反推 PPO2。

影响：

- 气体配置的最大 PPO2 与产品设置一致。
- 避免 UI/模拟器公式与算法公式互相反推导致误差。

#### 停站时间显示

旧口径：

- 实时停站倒计时和轨迹图停站标签直接使用 `ArexDecoStop.duration_seconds`。
- 多气体计划中，`duration_seconds` 可能包含切气惩罚，导致 UI 把切气预测延迟显示成当前站必须停留时间。

新口径：

- 实时停站倒计时使用 `hold_seconds`。
- 轨迹图/实时停站列表使用 `hold_seconds`。
- Dive Plan、TTS、气量估算继续使用 `duration_seconds` / `tts_seconds`，因为它们表示完整 planning 预测。
- 调试日志同时打印 `dur/hold/sw`，方便和算法工程师对齐：
  - `dur = duration_seconds`
  - `hold = hold_seconds`
  - `sw = switch_penalty_seconds`

影响：

- 实时 UI 不再把 gas switch penalty 当成当前站倒计时。
- 静态计划仍保留切气惩罚，用于 TTS 和气量估算。

#### 推荐切换气体

旧口径：

- 模拟器在算法推荐之外又加了本地过滤：
  - 必须有停站。
  - 必须已经进入强制减压。
  - 当前深度必须小于 UI 气体槽 MOD。
- UI 确认切气时也再次按当前深度和 MOD 判断是否允许切气。

新口径：

- 每次 `arex_deco_step()` 后调用 `arex_deco_recommend_gas()`。
- 模拟器只消费算法返回：
  - `available`
  - `recommended_gas_index`
  - `active_gas_index`
  - `is_emergency_no_safe_gas`
- 删除本地“是否减压/是否有停站/是否超过 MOD”的切气策略判断。
- UI 确认切气后，使用 0 秒 `arex_deco_step()` 更新 active gas。

保留的非算法门控：

- UI 只在 `dive_time_s > 0` 时展示 `BETTER GAS AVAILABLE`。
- 这个门控只表达产品生命周期，避免上电 0 m、多气体配置时弹出切气提示。
- 它不判断哪个气体更好，不判断 MOD，不判断是否减压。

影响：

- “推荐哪个气体”完全由算法层决定。
- “是否已经进入潜水、是否允许展示弹窗”由产品生命周期决定。

### 工程和库同步

- 更新 AREX 头文件到 `0.0.18`。
- 更新 `mingw64` 与 `sf32` 静态库。
- 同步更新旧路径 `src/algo_core/lib/libarex_deco_core.a`，避免 CodeBlocks 仍引用旧路径时报链接错误。
- 更新 `MANIFEST.txt`、算法 API 文档和版本历史。
- 修复 CodeBlocks 工程中 `src/ui_test/ui_test.c` 未加入编译导致的 `ui_test_try_start` 链接错误。

### 相关文档

- `UI_html_DOC/AREX_GAS_RECOMMENDATION_INTEGRATION.md`
- `UI_html_DOC/AREX_GAS_RECOMMENDATION_CALL.md`
- `src/algo_core/docs/core_api_zh.md`
- `src/algo_core/docs/version_history_zh.md`

### 验证记录

已做的本地检查：

- `src/algo_sim/deco_core.cpp` 语法检查通过。
- `src/ui/views/submenu_model.c` 语法检查通过。
- `src/ui/core/data.c` 语法检查通过。
- `src/ui/core/ui_state.c` 语法检查通过。
- AREX `0.0.18` 静态库最小链接测试通过。

CodeBlocks Debug target 仍应作为最终确认方式。
