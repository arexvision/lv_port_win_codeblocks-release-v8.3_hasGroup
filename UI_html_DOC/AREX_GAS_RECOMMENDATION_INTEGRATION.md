# AREX 气体推荐集成记录

## 背景

AREX Deco Core `0.0.18` 已提供 `arex_deco_recommend_gas()`，文档定义它是 runtime 切气提示的单一输出接口。

主工程不再自行判断“是否需要提示更好气体”，只消费算法返回的 `ArexDecoGasRecommendation`：

- `available == 1`：弹出 `BETTER GAS AVAILABLE`。
- `recommended_gas_index`：作为确认后要切换的目标气体。
- `is_emergency_no_safe_gas == 1`：算法表示当前没有安全可用气体；当前 UI 尚未映射成独立告警，后续需要产品侧确认告警文案和等级。

## 本次删除的上层策略

`src/algo_sim/deco_core.cpp` 删除了以下本地过滤：

- 必须满足 `schedule->stop_count > 0` 才提示切气。
- 必须满足 `ceiling_depth_m > DECO_CEILING_ACTIVE_M` 才提示切气。
- 使用 UI 侧 `gas->max_depth_m` 和当前深度再次判断是否可切气。

这些判断属于算法策略，已交由 `arex_deco_recommend_gas()` 统一决定。

`src/ui/core/ui_state.c` 删除了确认切气时的本地 MOD/深度复核：

- 不再用 `bus_get_gas_slot_mod_m()` 和 `bus_get_depth()` 判断是否允许确认。
- 当 INFO 弹窗显示且存在有效推荐索引时，确认键直接下发 `request_gas_switch()`。

## 仍保留的边界检查

主工程仍保留最小边界防护：

- 推荐索引必须在当前 `gas_plan.gas_count` 范围内。
- 推荐索引不能等于当前 active gas。
- 无有效推荐时清除 `ALARM_ID_INFO_GAS_SWITCH`。

这些检查不是切气策略，只用于防止异常 ABI 数据或状态错位导致数组越界。

## 调用链

实时 tick 后：

```text
deco_core_tick()
  -> arex_deco_step()
  -> arex_deco_plan(..., NULL)
  -> arex_deco_recommend_gas()
  -> sync_gas_recommendation()
  -> bus_set_recommended_gas_idx()
  -> alarm_set_active(ALARM_ID_INFO_GAS_SWITCH)
```

用户确认后：

```text
ui_try_confirm_gas_switch_info()
  -> request_gas_switch(recommended_gas_index)
  -> deco_core_tick()
  -> handle_pending_gas_switch()
  -> arex_deco_step(duration_seconds = 0, gas_index = target)
```
