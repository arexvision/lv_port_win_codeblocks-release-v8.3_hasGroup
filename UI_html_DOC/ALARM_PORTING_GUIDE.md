# Alarm Porting Guide

## 1. Current architecture

The current alarm system has been split into two layers:

1. `data bus`
   `bus_set_*()` only writes sensor/config data and dirty masks.
   It must not evaluate thresholds or trigger/clear alarms.

2. `alarm module`
   The real warning state lives in `src/ui/alarm/alarm.c`.
   Algorithm, platform, debug, or real-device task code should call the alarm API explicitly.

This means future real-device code should follow this rule:

```c
bus_set_depth(depth_m);
bus_set_ppo2(slot, ppo2);
alarm_set_active(ALARM_ID_WARN_PO2_ELEVATED, ppo2 > 1.4f);
```

Do not do this anymore:

```c
bus_set_depth(depth_m);   /* expecting bus to auto-create alarms */
```

## 2. Alarm levels

There are 3 levels in the UI:

1. `ALARM_INFO`
   Informational prompt.
   Banner shows normal text without `WARNING:` / `CRITICAL:` prefix.

2. `ALARM_WARN`
   Warning prompt.
   Banner shows `WARNING: ...`.
   Related widgets can be highlighted.

3. `ALARM_CRIT`
   Critical prompt.
   Banner shows `CRITICAL: ...`.
   Related widgets can be highlighted more aggressively.

Level priority is fixed:

```text
CRIT > WARN > INFO
```

If multiple alarms of the same level are active, the banner rotates between them every 3 seconds.

## 3. Alarm IDs

Fixed alarm IDs are defined in:

- `src/ui/alarm/alarm.h`

Current built-in IDs include:

- `ALARM_ID_CRIT_ASCENT_RATE`
- `ALARM_ID_CRIT_PO2_MAX`
- `ALARM_ID_CRIT_CEIL_BROKEN`
- `ALARM_ID_CRIT_ALGO_LOCK`
- `ALARM_ID_CRIT_TANK_EMPTY`
- `ALARM_ID_CRIT_BATTERY_DEAD`
- `ALARM_ID_WARN_PO2_ELEVATED`
- `ALARM_ID_WARN_NDL_LOW`
- `ALARM_ID_WARN_CNS_HIGH`
- `ALARM_ID_WARN_OTU_HIGH`
- `ALARM_ID_WARN_SAFETY_BROKEN`
- `ALARM_ID_WARN_TANK_TURN`
- `ALARM_ID_WARN_SIDEMOUNT_DIFF`
- `ALARM_ID_WARN_DEPTH_LIMIT`
- `ALARM_ID_WARN_TIME_LIMIT`
- `ALARM_ID_WARN_BATTERY_LOW`
- `ALARM_ID_WARN_POD_LOST`
- `ALARM_ID_INFO_SAFETY_STOP`
- `ALARM_ID_INFO_GAS_SWITCH`
- `ALARM_ID_INFO_STOP_DONE`
- `ALARM_ID_INFO_COMPASS_CALI`

Use fixed IDs whenever the warning meaning is stable and known in advance.

## 4. Alarm APIs

Public APIs are in:

- `src/ui/alarm/alarm.h`

### 4.1 Fixed alarms

Use:

```c
alarm_set_active(ALARM_ID_WARN_NDL_LOW, true);
alarm_set_active(ALARM_ID_WARN_NDL_LOW, false);
```

Meaning:

- `true`: condition became active
- `false`: condition cleared

This is the preferred API for real-device porting.

### 4.2 Custom alarms

Use:

```c
alarm_raise_custom(ALARM_WARN, "SENSOR CRC ERROR", COMP_EMPTY);
alarm_clear_custom();
```

Use custom alarms only for temporary or debug-only messages that do not deserve a stable `alarm_id_t`.

Current limitation:

- There is only one custom alarm slot.
- Raising a new custom alarm replaces the previous custom alarm.

### 4.3 Clear all

Use:

```c
alarm_clear_all();
```

This force-clears every fixed and custom alarm.

Recommended usage:

- mode switch reset
- simulator reset
- leaving a fake/debug session

Not recommended for ordinary runtime condition handling.

## 5. Why some alarms can be acknowledged and some cannot

Each fixed alarm has a clear policy in `src/ui/alarm/alarm.c`.

There are 3 policies:

1. `ALARM_CLEAR_CONDITION_ONLY`
   User click does not hide it.
   It stays visible until the owner clears the condition with `alarm_set_active(id, false)`.

2. `ALARM_CLEAR_ACK_HIDE`
   User click can hide it.
   But internally it still counts as active until the owner clears the condition first.
   If the condition later clears and then happens again, it can show again.

3. `ALARM_CLEAR_AUTO_TIMEOUT`
   Auto-hides after the configured timeout.
   Used for one-shot prompt style notifications.

Examples in current code:

- `ALARM_ID_CRIT_CEIL_BROKEN`
  `CONDITION_ONLY`
  Reason: user should not be able to dismiss a still-dangerous condition.

- `ALARM_ID_WARN_PO2_ELEVATED`
  `ACK_HIDE`
  Reason: user may confirm it once, but the underlying condition still exists until the owner clears it.

- `ALARM_ID_INFO_STOP_DONE`
  `AUTO_TIMEOUT`
  Reason: this is a short informational event.

## 6. What the click-to-clear logic currently does

The click-side logic is in:

- `src/ui/core/ui_state.c`
- `src/ui/alarm/alarm.c`

Current behavior:

1. UI detects `alarm_pending_click`
2. next user click calls `alarm_mark_clear_requested()`
3. that calls `alarm_ack_current()`
4. only alarms with `ALARM_CLEAR_ACK_HIDE` will actually hide

So the user click is not a universal clear.

It is only an `ACK` action.

That distinction is important for real-device porting:

- `ACK` means: "I saw it"
- `CLEAR` means: "condition is gone"

Real-device code should still call `alarm_set_active(id, false)` when the condition truly disappears.

## 7. Recommended real-device integration pattern

Recommended ownership:

1. Sensor/driver layer
   Writes raw values through `bus_set_*()`

2. Algorithm / safety task / device state task
   Evaluates conditions
   Calls `alarm_set_active()`

3. UI layer
   Only renders current alarm state

Typical frame:

```c
void device_runtime_tick(void)
{
    bus_set_depth(depth_m);
    bus_set_dive_time(dive_s);
    bus_set_ppo2(active_slot, ppo2);

    alarm_set_active(ALARM_ID_WARN_DEPTH_LIMIT, depth_m >= depth_limit_m);
    alarm_set_active(ALARM_ID_WARN_NDL_LOW, ndl_min >= 0 && ndl_min < ndl_warn_min);
    alarm_set_active(ALARM_ID_CRIT_PO2_MAX, ppo2 > 1.6f);
}
```

Recommended rule:

- fixed condition -> fixed `alarm_id_t`
- temporary text event -> `alarm_raise_custom()`

## 8. When to create a new fixed alarm ID

Create a new `alarm_id_t` when all of these are true:

1. the condition is stable and expected in production
2. the text is stable
3. the target widget is known
4. the clear policy should be consistent every time

If the text is temporary, debug-only, or highly variable, use custom alarm instead.

## 9. Current LOG RATE status

Current `LOG RATE` menu item is wired to the UI config field and simulator trajectory sampling.

Confirmed current chain:

1. menu changes `s_log_rate_s` in `src/ui/views/submenu_model.c`
2. action calls `ui_on_log_rate_set(seconds)` in `src/ui/views/menu_actions.c`
3. default weak callback writes `g_sys_config.log_rate_s` through `bus_set_log_rate(seconds)`
4. simulator depth ticks call `dive_log_append_sampled()`

```c
dive_log_append_sampled(time_s, depth_m);
```

Current scope:

- simulator auto trajectory cadence: wired
- TCP direct depth trajectory cadence: wired
- TCP explicit `sample <time_s> <depth_m>` command: intentionally forces a sample
- algorithm tick cadence: not controlled by LOG RATE
- real hardware logging task: should call `dive_log_append_sampled()` or use `bus_get_log_rate()`

So current status is:

```text
UI value changes: yes
simulator trajectory sampling effect: yes
real hardware logging task ownership: platform integration
```

## 10. How LOG RATE should be wired on hardware

Recommended ownership for `LOG RATE`:

1. keep `g_sys_config.log_rate_s` as the UI-owned config field
2. menu callback writes that config field through `bus_set_log_rate(seconds)`
3. real logging task uses `bus_get_log_rate()` to decide record cadence
4. UI only displays the value

Recommended logging task behavior:

```c
if (now_s - last_log_s >= bus_get_log_rate())
{
    append_log_sample();
    last_log_s = now_s;
}
```

## 11. Practical porting checklist

For real-device porting, the recommended order is:

1. keep `bus_set_*()` as pure data input
2. move all warning condition decisions into algorithm/device tasks
3. use `alarm_set_active()` for stable conditions
4. use `alarm_raise_custom()` only for temporary/debug text
5. define whether each new alarm is `CONDITION_ONLY`, `ACK_HIDE`, or `AUTO_TIMEOUT`
6. wire `LOG RATE` to a real config field and a real logging task

## 12. Files to read

- `src/ui/alarm/alarm.h`
- `src/ui/alarm/alarm.c`
- `src/ui/alarm/alarm_view.c`
- `src/ui/core/ui_state.c`
- `src/ui/core/data.c`
- `src/ui/core/callbacks.c`
