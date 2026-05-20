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

## Architecture

Full architecture documentation is in `UI_html_DOC/AREX_ARCH.md` (authoritative, 207KB). Read it before making structural changes.

### Startup sequence

`WinMain()` in `main.c` →
1. `lv_init()` + `lv_win32_init()` — LVGL + Windows GDI driver (640×480)
2. `UI_main()` in `src/UI_main.c` — AREX UI entry point:
   - `arex_ui_init()` — loads default config, zeroes sensor data
   - `arex_screen_create()` — builds the full LVGL widget tree
   - `arex_input_init()` — registers keyboard/encoder callbacks
   - `lv_timer_create(sim_tick_cb, 1000ms)` — 1 Hz simulation tick
3. Main loop: `lv_task_handler()` every 10ms

### Module map (`src/arex_ui/`)

| File | Role |
|------|------|
| `arex_ui_engine.h/c` | Global state: `g_sys_config`, `g_sensor_data`; `arex_bus_set_*()` write API |
| `arex_ui_state.h/c` | 9-state machine (DASH, INFO, SETUP, …); input routing |
| `arex_screen.h/c` | LVGL widget tree creation, left-panel refresh, scroll, modals |
| `arex_data.h/c` | BLE sync frame struct; sensor write API |
| `arex_card_registry.h/c` | Card lookup, registry, `update_cb()` dispatch |
| `cards/card_*.c` | 7 card implementations (compass, deco, gas, plan, info, setup, blank) |
| `arex_hal_sim/arex_input_pc.h` | Keyboard/encoder input simulation for PC |

### Data flow

```
Hardware/BLE → arex_bus_set_*() → g_sensor_data (dirty_mask)
                                         ↓
                               arex_ui_refresh_all()
                                         ↓
                             card_*_update() callbacks → LVGL widgets
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
