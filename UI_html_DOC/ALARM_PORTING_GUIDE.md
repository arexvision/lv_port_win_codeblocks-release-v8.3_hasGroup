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

## 5. Alarm confirmation model

Current alarms separate condition ownership from visual acknowledgement.

- The owner calls `alarm_set_active(id, true/false)` to set or clear the real condition.
- `back 2` calls `alarm_confirm_current()`.
- Confirmation hides the banner, but it does not mean the condition is gone. CRITICAL target flashing is not cancelled by confirmation.
- When the owner clears the condition with `alarm_set_active(id, false)`, ack state is reset for the next trigger.

Level behavior:

- CRITICAL: banner and target flash. `back 2` hides the banner only; the target keeps flashing until condition clear.
- WARNING: if the target widget is visible, only the target breathes; if the target is not visible, a warning banner is shown. `back 2` hides the banner/stops breathing and leaves a steady highlight until condition clear.
- INFO: normal notifications auto-hide after 5 seconds.

`INFO_GAS_SWITCH` is special:

- It does not auto-hide after 5 seconds.
- It appears when a recommended gas is available.
- It records the depth at which the prompt appears.
- If the diver ascends more than `ALARM_GAS_SWITCH_PROMPT_EXIT_DELTA_M` shallower than that prompt depth without confirming, the prompt hides for this recommendation.
- `back 2` confirms it and automatically submits `request_gas_switch(bus_get_recommended_gas_idx())`.
- ENTER does not confirm alarms and does not switch gas.

## 6. Input mapping

The generic click-to-clear path is retired. `alarm_mark_clear_requested()` remains as a compatibility hook and returns false.

Current input ownership:

- normal ENTER: regular UI confirm/click only.
- normal BACK: regular UI back only.
- long BACK / TCP `back 2`: `alarm_confirm_current()`.

That distinction is important for real-device porting:

- `CONFIRM` means: "I saw it" or, for GAS_SWITCH, "switch to the recommended gas".
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
4. the condition owner and visual confirmation behavior should be consistent every time

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
5. define whether each new alarm is condition, 5s auto-timeout notification, or a `back 2` confirm action
6. wire `LOG RATE` to a real config field and a real logging task

## 12. Files to read

- `src/ui/alarm/alarm.h`
- `src/ui/alarm/alarm.c`
- `src/ui/alarm/alarm_view.c`
- `src/ui/core/ui_state.c`
- `src/ui/core/data.c`
- `src/ui/core/callbacks.c`
