# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Project Overview

AREX Pro 潜水电脑 UI — a Windows PC simulator for an embedded dive computer display system built on LVGL v8.3. The simulator runs on Windows (640×480 via GDI) and is designed to port to RT-Thread RTOS on embedded hardware.

## Build

**Primary build tool:** CodeBlocks 20.03+ with MinGW GCC.

Open `LittlevGL.cbp` in CodeBlocks and build with F9. Two targets:
- **Debug:** `bin/Debug/LittlevGL.exe` — includes `-g`, no optimization
- **Release:** `bin/Release/LittlevGL.exe` — `-O2`

Both targets define `-DLV_CONF_INCLUDE_SIMPLE=1 -DWINVER=0x0601` and link `mingw32`.

There is no command-line build script. There are no automated tests integrated into the build.

Do not modify `LittlevGL.cbp` during routine code changes. Only update the CodeBlocks project file when the user explicitly asks for it.

## Git / Commit

本仓库的 commit message 默认使用中文；只有用户明确要求时，才使用其他语言。
每完成一个完整、可验证的任务后，必须及时创建一次 commit；不要把已完成的改动长时间留在未提交状态。

## Architecture

Full architecture documentation is in `UI_html_DOC/AREX_ARCH.md` (authoritative, 207KB). Read it before making structural changes.

### Startup sequence

`WinMain()` in `main.c` →
1. `lv_init()` + `lv_win32_init()` — LVGL + Windows GDI driver (640×480)
2. `UI_main()` in `src/UI_main.c` — AREX UI entry point:
   - `arex_ui_init()` — loads default config, zeroes sensor data
   - `arex_screen_create()` — builds the full LVGL widget tree
   - `arex_input_init()` — registers keyboard/encoder callbacks
   - `lv_timer_create(arex_ui_update_task, 50ms)` — dirty-mask UI consumer task
   - `arex_sim_data_start()` — PC-only simulation data source
3. Main loop: `lv_task_handler()` every 10ms

### Module map (`src/arex_ui/`)

| File | Role |
|------|------|
| `core/arex_ui_engine.h/c` | Global state: `g_sys_config`, `g_sensor_data`; UI init and update task |
| `core/arex_data.h/c` | BLE sync frame struct; `arex_bus_set_*()` write API |
| `core/arex_ui_state.h/c` | UI state machine (DASH, INFO, SETUP, …); input routing |
| `core/arex_ui_update_router.h/c` | Periodic UI heartbeat and dirty-mask refresh routing |
| `screen/arex_screen.h/c` | LVGL screen tree, public screen facade, scroll, walls, edit flows |
| `screen/arex_layout_view.h/c` | Safe-zone, fixed-anchor, menu, and 5F grid layout rendering |
| `screen/arex_card_registry.h/c` | Card lookup, registry, display/storage position mapping |
| `widgets/arex_widget_*.h/c` | Reusable widget creation, update, and style application |
| `views/arex_modal_view.h/c`, `views/arex_submenu_*.h/c` | Overlay dialogs and submenu drawer/model |
| `alarm/arex_alarm*.h/c` | Alarm event engine and alarm visual layer |
| `cards/card_*.c` | 7 card implementations (compass, deco, gas, plan, info, setup, blank) |
| `arex_hal_sim/arex_input_pc.h` | Keyboard/encoder input simulation for PC |

### Data flow

```
Hardware/BLE → arex_bus_set_*() → g_sensor_data (dirty_mask)
                                         ↓
                               arex_ui_update_task()
                                         ↓
                         arex_ui_update_router_dispatch()
                                         ↓
                   widgets / cards / alarms / layout rebuild
```

**Rule:** never write `g_sensor_data` or `g_sys_config` directly from outside `arex_ui_engine.c`. Always use `arex_bus_set_*()`.

### Screen layout

- **Left anchor:** 2-column × 7-row fixed grid (160px wide) — always-visible dive metrics
- **Right cards:** LVGL tileview with dynamic card ordering via `card_order[]`
- **Safe zone:** reserved area for alerts/overlays
- **5F custom grid:** 5-column × 6-row widget grid for custom card type

### Card system

Each card registers with `arex_card_registry` providing `create_cb`, `update_cb`, and `on_enter_cb`. Cards are identified by enum ID (INFO, COMPASS, DECO, GAS, PLAN, CUSTOM_GRID, BLANK, SETUP). Dynamic ordering is controlled by `card_order[]` in `arex_ui_engine`.

### LVGL configuration

- `lv_conf.h` — 32-bit color, custom malloc, Windows tick source
- `lv_drv_conf.h` — driver stubs (only `win32drv` is active)
- LVGL v8.3 and lv_drivers are git submodules under `lvgl/` and `lv_drivers/`
