# AREX 算法与模拟器同步变更记录

## 目的

本文档专门记录每次 AREX 算法包升级后，PC 模拟器侧为了保持与真机计算口径一致而做的适配。

记录范围包括：

- 模拟器是否删除了本地公式，改为调用算法接口。
- 模拟器是否调整了算法输出字段的使用口径。
- UI 是否只做生命周期/展示门控，而不再复刻算法判断。
- 静态库、头文件、文档和工程链接路径是否同步。

原则：只要算法层已经提供计算接口，模拟器和主工程都必须调用算法接口，不在 UI 或模拟器里重复实现公式。

## 未发布 UI 口径调整

### DECO 主图切换到归一化组织图

改动：

- DECO 卡片主图从纵向 `absolute_gf_percent` 柱状图切换为横向归一化组织条。
- 主图读取 `tissue_bar_permille[16]` 和 `tissue_pi_permille`，不再用 `tissue_raw_pct[16]` 驱动。
- 图表固定 400 为环境压力线，900 为 M 值线，PI 使用动态虚线。

原因：

- 需要验证“静态参考线 + 动态组织条长度”的新组织图方案是否符合潜水员阅读习惯。
- 旧 absolute GF 图适合工程调试，但用户现在要验证前后端归一化协议。

旧口径和新口径差异：

- 旧口径：主图使用 `tissue_raw_pct[16]`，Y 轴为 `-100..120`，0% 为环境压力，100% 为 M 值。
- 新口径：主图使用 `tissue_bar_permille[16]`，X 轴为 `0..1000`，400 为环境压力，900 为 M 值。
- `tissue_raw_pct[16]` / `tissue_gf_pct[16]` 继续保留给旧小组件和信息概览。

验证方式：

- DECO 主图应显示 16 条横向组织条，而不是 16 根竖向柱。
- 环境压力线固定在图表宽度 40% 处，M 值线固定在 90% 处。
- PI 虚线随 `tissue_pi_permille` 改变位置。
- 组织条超过 900 的部分应进入红色闪烁。

### 归一化组织图测试载荷

改动：

- PC 适配层在同步 `absolute_gf_percent[16]` / `relative_gf_percent[16]` 的同时，额外写入一份归一化组织图测试载荷：`tissue_bar_permille[16]`、`tissue_pi_permille`、`tissue_ambient_pressure_bar`、`tissue_inspired_n2_bar`、`tissue_n2_bar[16]`、`tissue_m_value_bar[16]`。
- `tissue_bar_permille[16]` 使用 0~1000 坐标，400 表示环境压力线，900 表示 M 值线。
- `tissue_bar_permille[16]` 按总惰性气体压力 `PN2 + PHe` 计算；`tissue_n2_bar[16]` 只是氮气分压调试字段，Trimix 下不能单独拿它重算组织条长度。
- 当前算法 API 未直接返回每仓 M 值，PC 适配层临时使用 `absolute_gf_percent[i] = ((P_tissue - P_amb) / (M_amb - P_amb)) * 100` 反推 `M_amb`。

原因：

- 新组织图方案需要先验证“静态环境压力线 + 静态 M 值线 + 动态组织条长度”的 UI 表达，但算法团队暂时无法立即扩展正式 API。
- 归一化载荷不覆盖现有 `tissue_raw_pct[16]` / `tissue_gf_pct[16]`；DECO 主图已改为消费归一化载荷，旧小组件仍使用原百分比字段。

旧口径和新口径差异：

- 旧口径：UI 只能消费 `tissue_raw_pct[16]` / `tissue_gf_pct[16]` 这类百分比字段。
- 新增测试口径：UI 可以读取独立的 0~1000 归一化字段做横向条图试验；M 值字段是 PC 适配层临时反推结果，不等同于算法正式 API。

验证方式：

- 调用 `bus_get_tissue_normalized_valid()` 应在算法同步后返回 true。
- `bus_get_tissue_bar_permille(i)` 应位于 0~1000；组织压力等于环境压力附近时应接近 400，达到 M 值附近时应接近 900。
- 后续算法层提供正式 `P_amb/P_I/PN2_i/M_i` 或归一化 payload 接口后，应删除 PC 适配层反推逻辑，改为直接消费算法输出。

### 16 组织仓全物理态图表（历史口径）

本节保留此前 absolute GF 主图方案的历史记录，不代表当前 DECO 主图实现。

改动：

- DECO 卡片 16 组织仓柱状图从 `relative_gf_percent[16]` 改为使用 `absolute_gf_percent[16]`。
- PC 适配层不再把 `absolute_gf_percent` 负值截成 0，data bus 的 `tissue_raw_pct[16]` 改为有符号百分比，并额外同步 `current_target_gf` 的百分比值给 UI。
- 主图表 Y 轴按 `-100..120` 绘制，0% 基准线、动态 GF 虚线和 100% M 值线三条横线同时显示。
- 柱子以 0% 横线为原点：负值向下画，正值向上画；超过 GF 线的段为黄色，超过 100% 的段为红色闪烁，并在 120% 绘制封顶。

原因：

- `relative_gf_percent` 已经叠加当前 GF 保守度，适合表达“是否突破当前 target GF”，但不能同时表达欠饱和负值、0% 环境水压线、动态 GF 线和 100% 绝对 M 值线。
- `absolute_gf_percent` 是绝对生理百分比，可以在同一个坐标系里同时容纳欠饱和、当前 GF 限制和 M 值极限。

旧口径和新口径差异：

- 旧口径：DECO 主图使用 `tissue_gf_pct`，图表只画 `0..120` 的单向向上柱，负值在进入 `uint8_t` 时变成 0，100 表示当前 target GF。
- 当时口径：DECO 主图使用有符号 `tissue_raw_pct`，图表画 `-100..120` 的上下双向柱，0 表示 `P_tissue = P_amb`，动态 GF 线来自 `current_target_gf`，100 表示绝对 M 值。
- `relative_gf_percent` / `tissue_gf_pct` 保留给旧小组件和信息概览兼容，不再驱动 DECO 主图。

验证方式：

- 构造或接入算法返回负数 `absolute_gf_percent` 时，DECO 主图柱子必须从 0% 横线向下生长，不能消失为 0。
- 构造 `absolute_gf_percent` 位于 `0..current_target_gf`、`current_target_gf..100`、`100..120` 三个区间时，柱段颜色应分别为绿色、黄色、红色闪烁。
- 修改 GF 设置或算法返回的 `current_target_gf` 后，动态 GF 虚线位置应随 VM 刷新移动。

## 0.0.18 -> 0.0.19

### 算法包变化

本次从 AREX Deco Core `0.0.18` 升级到 `0.0.19`。

算法层新增/明确了以下字段和行为：

- `ArexDecoSchedule` 新增 `ceiling_violated` 字段。
- `ceiling_violated == 1` 表示 plan 时当前深度浅于 GF-high ceiling。
- 即使 `stops == 0` 或 `tts_seconds == 0`，只要 `ceiling_violated == 1`，产品层也必须提示 ceiling violation 风险。
- API patch 版本升至 `0.0.19`。

### 模拟器口径调整

#### Ceiling Violation

旧口径：

- PC 模拟器主要通过 `depth < ceiling` 的本地策略触发 `CEILING BROKEN`。
- 当算法返回空计划或 `tts=0` 时，产品层没有显式消费 schedule 中的 ceiling violation 语义。

新口径：

- `sync_core_data()` 读取 `schedule->ceiling_violated`。
- `ceiling_violated != 0` 时触发现有 `ALARM_ID_CRIT_CEIL_BROKEN`。
- 调试日志增加 `cv=<0|1>`，方便确认算法返回的 ceiling violation 标志。

影响：

- Missed-deco、已浅于 ceiling 或已到水面但仍有 ceiling 的场景，不再只依赖 `stops/tts` 判断安全。
- 真机侧也必须消费 `ArexDecoSchedule.ceiling_violated`，不能只用 `stops == 0` 或 `tts == 0` 判断无减压风险。

#### Runtime 停站显示

旧口径：

- 0.0.18 同步后，runtime 当前站和轨迹图曾使用 `hold_seconds`，避免把预测切气惩罚显示成停留倒计时。
- 该口径会导致纯切气预测站显示为 `DECO xxm 0:00`，也可能在确认切气后出现倒计时跳变。

新口径：

- Runtime 主倒计时和轨迹图改为使用 `duration_seconds`。
- 当某站满足 `hold_seconds == 0 && switch_penalty_seconds > 0` 时，视为纯切气预测站，不作为 runtime 当前减压站显示，也不进入实时轨迹停站列表。
- 切气提示仍通过 `arex_deco_recommend_gas()` 和产品层门控展示。

影响：

- 不再出现纯切气预测站渲染成 `DECO xxm 0:00`。
- 对于包含切气 penalty 的真实停站，runtime 主倒计时与 TTS/planner 保持同一 planning 口径。

#### Safety Stop 显示归属

旧口径：

- PC 适配层除了消费算法 `schedule.stops[]`，还保留了一段本地 fallback：
  `safety_stop_enabled && max_depth > 10m && current_depth >= safety zone && ndl <= 0`。
- 这会让右上角 SAFE 显示混合“算法计划”和“适配层自判定”，不利于判断算法侧安全停留状态机是否完整。

新口径：

- 删除 `safety_stop_fallback_active()` 本地补偿。
- 右上角 `STOP_SAFETY` 只来自 `arex_deco_plan()` 返回的第一个有效 runtime stop，且当前没有强制 ceiling。
- 安全停留触发深度、停留深度、开关和时长继续由算法配置/常量负责：
  `AREX_DECO_SAFETY_STOP_TRIGGER_DEPTH_M`、`AREX_DECO_SAFETY_STOP_DEPTH_M`、`safety_stop_enabled`、`safety_stop_seconds`。

影响：

- 模拟器可以直接验证算法侧是否稳定返回安全停留，不再被 UI/适配层 fallback 掩盖。
- 如果 fast `goto/speed` 场景下 SAFE 短暂退回 NDL，说明该帧算法 schedule 未返回有效 safety stop；后续应和算法侧确认是否需要 core 内部 latch/state，而不是在 UI 侧复刻。

#### Plan 失败时的 UI 同步

旧口径：

- `arex_deco_plan()` 失败时，适配层把 `NULL schedule` 传入 `sync_core_data()`。
- 这会把 TTS、右上角 SAFE/DECO 当前站、实时轨迹停站列表清空，造成 fast `goto/speed` 场景下短暂退回普通 NDL 样式。

新口径：

- `arex_deco_step()` 成功但 `arex_deco_plan()` 返回非 OK 时，认为组织状态已经推进，但本轮 schedule 不可用。
- 适配层只同步组织仓、CNS/OTU、GF、ceiling、nofly、gas 等非 schedule 数据。
- TTS、当前停站、实时轨迹停站列表和 `ceiling_violated` 告警保持上一帧有效 plan 的输出，不用空计划覆盖。

影响：

- `plan=INVALID_STATE` 不再被误显示为“无停站/无安全停留”。
- 日志中的 `[AREX_CALL]` 会继续打印 step/plan 状态、NDL、ceiling、stops、TTS，用于和算法侧定位 plan 失败原因。

#### 16 组织仓柱状图

旧口径：

- DECO 卡片里的 16 组织仓柱状图使用 `tissue_raw_pct` 绘制。
- 参考线按用户 GF High 百分比绘制，并显示 `M-VALUE xx%` 文案。
- 柱高按 `0..100` 映射，超过 100 的部分被直接顶满。

新口径：

- 16 组织仓柱状图改用算法 `relative_gf_percent[16]` 对应的数据，即 data bus 中的 `tissue_gf_pct[16]`。
- UI 绘制前只做显示限幅：小于 0 的算法值在适配层进入 `uint8_t` 时为 0，绘制上限为 120。
- 100 表示当前 target GF danger line；柱子超过 100 时触发危险闪烁，最多绘制到 120。
- 参考线固定在 100/120 高度，不再显示 `GF HIGH` 或 `M-VALUE` 文案。

影响：

- DECO 卡片的 16 组织仓与算法侧“结合当前 GF 保守度后的相对百分比”一致。
- 自定义卡片 `TISSUE(GF)` 同步使用 0..120 绘制量程；`TISSUE(RAW)` 仍保持 raw 口径和 0..100 绘制量程。

### 工程和库同步

- 更新 AREX 头文件、`mingw64` 静态库、`sf32` 静态库和算法文档到 `0.0.19`。
- 恢复 `src/algo_core/lib/libarex_deco_core.a` 兼容拷贝，避免 CodeBlocks 仍引用旧路径时报链接错误。

### 展示开关

- 新增 `DECO_HIDE_SWITCH_ONLY_STOPS` 宏，默认 `1`。
- 该宏只影响 UI 展示层是否跳过纯切气预测站。
- 开启时，满足 `hold_seconds == 0 && switch_penalty_seconds > 0` 的 stop 不进入右上角 runtime 当前站、实时轨迹停站列表和 DIVE PLAN 表格停站行。
- 关闭时，纯切气预测站会按 `duration_seconds` 作为普通 schedule stop 显示，便于和算法日志逐项对齐。
- 无论宏开关如何，算法原始 `schedule`、`tts_seconds`、氧暴露预测、气量估算和推荐气体链路都不被改写。

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
- 启动默认 AIR 气体槽和 TCP 调试默认种子不再硬编码 `56m` 或 `33m`，改为通过 `bus_calculate_gas_mod(21, 0, 1.4)` 获取算法口径 MOD；只有算法接口不可用时才 fallback 到 `56m`。
- `SALINITY` 变更后会重新应用当前 dive mode 的 gas profile，触发每个气体槽按当前盐度和各自最大 PO2 重新计算 MOD，避免 GAS 卡片继续显示旧水型下缓存的 `gas_slot_mod_m`。
- 真机非 PC 路径的 `bus_calculate_gas_mod()` 改为调用 `ui_calculate_gas_mod()` 弱回调，便于真机业务层覆盖并接入真实算法库；PC 路径仍直接调用 `deco_core_calculate_gas_mod()`。
- 删除 UI/模拟器侧 MOD 公式复刻。

影响：

- 模拟器、真机和算法文档中的 MOD 口径统一。
- 海水/淡水、水面压力、气体最大 PPO2 等影响因素由算法层统一处理。
- 上电默认 AIR / PO2 1.4 的气体卡片 MOD 与进入 AIR GAS 后确认得到的 MOD 保持一致；默认 `FRESH` 约为 `58.2m`，`SALT` 约为 `56.5m`。
- DIVE SETUP 中切换 `FRESH / SALT / EN13319` 后，当前已配置气体的 MOD 显示会立即跟随新水型刷新，不需要重新进入气体配置页确认。
- 真机移植时必须实现 `ui_calculate_gas_mod()` 或提供等价强实现，否则默认弱实现返回 `0.0f`，UI 会落入 fallback，不能代表真实算法口径。

#### 气体最大 PPO2

旧口径：

- 模拟器曾通过气体槽 MOD 反推 `max_ppo2_bar`。
- 引入每槽 PO2 后，GAS 卡片 MOD 已按每槽 PO2 计算，但算法 gas plan 仍使用全局 `MOD PPO2` 作为所有气体的 `max_ppo2_bar`。

新口径：

- 不再从 MOD 反推 PPO2。
- 每个气体槽新增独立 `gas_slot_max_ppo2` 数据，算法 `fill_gas_plan_from_ui()` 优先使用该槽自己的最大 PO2；只有旧 TCP/旧数据没有 per-slot PO2 时，才 fallback 到全局 `MOD PPO2`。
- GAS 卡片右侧 `PO2` 显示同样改为每槽最大 PO2，不再显示实时 PPO2，也不再统一显示全局 `MOD PPO2`。

影响：

- 气体配置的最大 PPO2 与产品设置一致。
- 避免 UI/模拟器公式与算法公式互相反推导致误差。
- `AIR / NITROX / 3 GAS / OC Tech` 中每个气体单独设置的 PO2 会同时影响 MOD 展示、算法 gas plan 的 `max_ppo2_bar` 和切气/计划口径。
- `DIVE SETUP / MOD PO2` 保留为旧路径和未配置槽的 fallback，不覆盖已配置气体槽自己的 PO2。

#### 算法配置变更后的即时刷新

旧口径：

- GF、盐度、last deco stop、safety stop、gas profile 等设置写入算法 config/gas plan 后，通常要等下一次 `deco_core_tick()` 才会重新 `plan/recommend/sync_core_data()`。
- 在 0m 或慢速调试时，部分 UI 可能短时间显示旧的 TTS、stop、gas recommendation、MOD 或组织仓/GF 指标。

新口径：

- `deco_core_set_gf()`、`deco_core_set_salinity_mode()`、`deco_core_set_final_stop_depth()`、`deco_core_set_safety_stop_mode()`、`deco_core_apply_gases_from_ui()` 在 apply config/gas plan 后，立即执行一次不推进时间的 `arex_deco_plan()` / `arex_deco_recommend_gas()` / `sync_core_data()`。
- 该刷新不调用 `arex_deco_step()`，不推进组织舱、不增加潜水时间，只把当前状态按新配置重新输出到 data bus。
- gas profile 提交使用 data bus 批量门控，所有 slot、slot count 和 active gas 都写完后才统一 apply 算法，避免中途用旧 active gas 或旧 per-slot PO2 生成一次临时 plan。

影响：

- 设置确认后，INFO、GAS、DECO、PLAN、组织仓/GF 等算法输出能立即使用新配置刷新。
- 避免配置项变更后等待下一帧算法 tick 才更新造成的短暂错觉。
- 多气体配置确认时只触发一次算法 apply/plan，避免逐个 slot 写入时重复规划造成 CPU 抖动。

#### 本轮参数检查结果

已确认并同步到算法/显示刷新链路：

- `GF / CONSERVATISM`：通过 `deco_core_set_gf()` 更新算法 config，并立即重新输出 plan/recommend/core data。
- `SALINITY`：通过 `deco_core_set_salinity_mode()` 更新水型；同时重新应用当前 gas profile，让每槽 MOD 按新水型刷新。
- `LAST DECO STOP`：通过 `deco_core_set_final_stop_depth()` 更新 `last_stop_m`，并立即重新规划。
- `SAFETY STOP`：通过 `deco_core_set_safety_stop_mode()` 更新 safety stop 秒数/开关，并立即重新规划。
- `GAS MODE / O2 / He / per-slot PO2`：气体槽携带 O2、He、MOD、每槽最大 PO2；算法 gas plan 与 GAS 卡片都使用同一份 per-slot PO2。
- `DIVE SETUP / MOD PO2`：保留为旧 TCP/旧数据 fallback，不覆盖已配置气体槽自己的 PO2。

已检查但暂不在主工程补公式：

- `ALTITUDE`：当前只保存 UI 配置，AREX 适配层尚无“altitude level -> surface_pressure_bar”的产品/算法接口。本仓库规则要求算法已有计算接口时必须直接调用算法接口，因此这里不在 UI 侧复刻气压换算公式。后续需要算法或平台层提供明确 surface pressure 输入/映射后再接入。

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
- PC 适配层新增 `arex_deco_get_api_version()` 一次性检查，运行时版本必须与 `AREX_DECO_API_VERSION_*` 头文件宏一致；不一致时拒绝初始化和 MOD 计算，避免头文件与静态库错配后继续输出错误结果。
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
