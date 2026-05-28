# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Project Overview

潜水电脑 UI — a Windows PC simulator for an embedded dive computer display system built on LVGL v8.3. The simulator runs on Windows (640×480 via GDI) and is designed to port to RT-Thread RTOS on embedded hardware.

## Build

**Primary build tool:** CodeBlocks 20.03+ with MinGW GCC.

Open `LittlevGL.cbp` in CodeBlocks and build with F9. Two targets:
- **Debug:** `bin/Debug/LittlevGL.exe` — includes `-g`, no optimization
- **Release:** `bin/Release/LittlevGL.exe` — `-O2`

Both targets define `-DLV_CONF_INCLUDE_SIMPLE=1 -DWINVER=0x0601` and link `mingw32`.

There is no command-line build script. There are no automated tests integrated into the build.

Do not modify `LittlevGL.cbp` during routine code changes. Only update the CodeBlocks project file when the user explicitly asks for it.

## Coding Preferences

Do not introduce project-name prefixes in code identifiers or directory names. New functions, types, globals, enums, macros, and source folders must not use a project-name prefix; use the module/domain name directly instead, for example `screen_create()`, `BUS_SET_*`, `src/ui`, `src/hal_sim`, or `src/algo_sim`.

不要为已经由菜单索引、枚举表、固定数组或状态机保证范围的设置值额外添加 `clamp`/兜底映射。此类防御会隐藏真实错误，并让简单配置修改变复杂。只有外部输入、协议数据、文件持久化数据或其他不可信边界进入系统时，才做范围校验。

Menu business logic must be driven by stable IDs (`menu_id_t`, `menu_item_id_t`) and row types. Display strings are only for LVGL labels; do not branch on menu titles or row text with `strcmp` in selection/action paths.

Do not rewrite source files with PowerShell/default-system encoding. Many project files contain UTF-8 Chinese comments; reading them as the Windows ANSI code page and writing them back will corrupt text into mojibake. Prefer `apply_patch` for manual edits. If a scripted edit is truly necessary, read and write explicitly as UTF-8 without BOM, keep the change narrowly scoped, and verify nearby Chinese comments with `Get-Content -Encoding UTF8` plus `git diff` before continuing. Never use `Set-Content`/`Out-File`/`[IO.File]::WriteAllText` without an explicit UTF-8 encoding on files that may contain non-ASCII text.

PC simulator-only modules (`src/hal_sim`, `src/algo_sim`, `input_pc`, `buhlmann_debug`, TCP/debug-link code, Windows-only headers) must not be included unguarded from shared UI/core code. If shared code needs a PC-only hook, wrap both the include and the call site with `#ifdef PC_SIMULATOR`, and provide a non-PC fallback path or backend interface that still compiles for the embedded target. `PC_SIMULATOR` is a build macro, not something a shared public header should define.

DIVE PLAN is not PC-only. The UI should call the neutral `dive_plan_backend_calculate()` backend interface. PC simulator builds may implement that backend with `buhlmann_debug`; embedded builds should provide their real algorithm implementation instead of depending on `src/algo_sim`.

## Git / Commit

本仓库的 commit message 默认使用中文；只有用户明确要求时，才使用其他语言。
每完成一个完整、可验证的任务后，必须及时创建一次 commit；不要把已完成的改动长时间留在未提交状态。

## Architecture

Current module architecture documentation is in `UI_html_DOC/UI_MODULE_MAP.md`. Read it before making structural changes.

### Startup sequence

`WinMain()` in `main.c` →
1. `lv_init()` + `lv_win32_init()` — LVGL + Windows GDI driver (640×480)
2. `UI_main()` in `src/ui_main.c` — UI entry point:
   - `ui_init()` — loads default config, zeroes sensor data
   - `screen_create()` — builds the full LVGL widget tree
   - `input_init()` — registers keyboard/encoder callbacks
   - `lv_timer_create(ui_update_task, 50ms)` — dirty-mask UI consumer task
   - `sim_data_start()` — PC-only simulation data source
3. Main loop: `lv_task_handler()` every 10ms

### Module map (`src/ui/`)

| File | Role |
|------|------|
| `core/ui_engine.h/c` | Global state: `g_sys_config`, `g_sensor_data`; UI init and update task |
| `core/data.h/c` | BLE sync frame struct; `bus_set_*()` write API |
| `core/ui_state.h/c` | UI state machine (DASH, INFO, SETUP, …); input routing |
| `core/update_router.h/c` | Periodic UI heartbeat and dirty-mask refresh routing |
| `screen/screen.h/c` | LVGL screen tree, public screen facade, scroll, walls, edit flows |
| `screen/layout_view.h/c` | Safe-zone, fixed-anchor, menu, and 5F grid layout rendering |
| `screen/page_registry.h/c` | Page lookup, registry, display/storage position mapping |
| `comp/comp_*.h/c` | Reusable widget creation, update, and style application |
| `views/menu_defs.*`, `views/menu_runtime.*`, `views/menu_actions.*`, `views/submenu_view.*`, `views/modal_view.*` | Menu definition/runtime/action layers, submenu drawer, and overlay dialogs |
| `alarm/alarm*.h/c` | Alarm event engine and alarm visual layer |
| `cards/card_*.c` | Right-side business card implementations (compass, deco, gas, plan, blank, etc.); INFO/DIVE MENU do not live here |
| `menus/menu_*.c` | Right-side top-level menu pages (INFO MENU and DIVE MENU) |
| `hal_sim/input_pc.h` | Keyboard/encoder input simulation for PC |

### Data flow

```
Hardware/BLE → bus_set_*() → g_sensor_data (dirty_mask)
                                         ↓
                               ui_update_task()
                                         ↓
                         ui_update_router_dispatch()
                                         ↓
                   widgets / cards / alarms / layout rebuild
```

**Rule:** never write `g_sensor_data` or `g_sys_config` directly from outside `ui_engine.c`. Always use `bus_set_*()`.

Alarm state is not part of the data bus. `bus_set_*()` must only update data and dirty masks; it must not evaluate thresholds or trigger/clear warnings. Warning ownership belongs to the algorithm, sensor, platform, or debug layer, which should call `alarm_set_active()`, `alarm_raise_custom()`, `alarm_clear_custom()`, or `alarm_clear_all()` explicitly.

### Screen layout

- **Left anchor:** 2-column × 7-row fixed grid (160px wide) — always-visible dive metrics
- **Right pages:** LVGL tileview with dynamic page ordering via `card_order[]`
- **Safe zone:** reserved area for alerts/overlays
- **5F custom grid:** 5-column × 6-row widget grid for custom card type

### Card system

Each right-side page registers with `page_registry` providing `create_cb`, `update_cb`, and `on_enter_cb`. Pages are identified by enum ID (INFO, COMPASS, DECO, GAS, PLAN, CUSTOM_GRID, BLANK, SETUP). Dynamic ordering is controlled by `card_order[]` in `ui_engine` for BLE compatibility.

### LVGL configuration

- `lv_conf.h` — 32-bit color, custom malloc, Windows tick source
- `lv_drv_conf.h` — driver stubs (only `win32drv` is active)
- LVGL v8.3 and lv_drivers are git submodules under `lvgl/` and `lv_drivers/`
