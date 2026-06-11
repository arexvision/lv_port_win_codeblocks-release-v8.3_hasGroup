# AREX 最优气体推荐调用说明

## 当前调用方式

实时算法 tick 中，主工程先推进当前潜水状态：

```c
arex_deco_step(&s_state, &input, &next_state, &s_metrics);
s_state = next_state;
```

随后用更新后的完整 `ArexDecoDiveState` 查询推荐气体：

```c
ArexDecoGasRecommendation gas_rec;
memset(&gas_rec, 0, sizeof(gas_rec));

ArexDecoStatus gas_status = arex_deco_recommend_gas(&s_state, &gas_rec);
```

当前 UI 只消费这些字段：

- `gas_rec.available == 1`：认为有推荐切气。
- `gas_rec.recommended_gas_index`：推荐目标气体。
- `gas_rec.active_gas_index`：算法认为的当前气体。
- `gas_rec.is_emergency_no_safe_gas == 1`：没有安全可用气体，后续应映射成独立告警。

主工程只保留索引范围检查，避免数组越界；不再自行判断 MOD、是否减压、哪个气体更优。

## 确认切气

用户在 UI 上确认后，主工程用 0 秒 step 切换 active gas：

```c
switch_input.start_depth_m = s_state.current_depth_m;
switch_input.end_depth_m = s_state.current_depth_m;
switch_input.duration_seconds = 0U;
switch_input.gas_index = gas_rec.recommended_gas_index;

arex_deco_step(&s_state, &switch_input, &switched_state, &s_metrics);
s_state = switched_state;
```

## UI 展示门控

主工程只在已经进入潜水生命周期后展示 `BETTER GAS AVAILABLE`。PC 模拟器目前用 `dive_time_s > 0` 判断。

原因：上电 0 m、多气体配置时，算法可能返回“当前深度更高氧气体更优”，但产品 UI 不希望在未入水时弹切气提示。

## 建议

建议算法接口文档明确 `arex_deco_recommend_gas()` 的语义：

- 它是“当前状态下最佳可用气体推荐”，还是“水下应提示潜水员切气”。
- 如果只负责最佳气体选择，UI 保留潜水生命周期门控是合理的。
- 如果算法希望直接决定是否弹窗，建议接口增加产品状态输入，至少包含是否已入水/是否处于潜水中。
