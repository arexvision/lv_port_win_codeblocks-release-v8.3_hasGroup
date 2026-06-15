# 潜水电脑 UI 信息架构

本文档是 UI 的可编辑 Markdown 信息架构版本，适合直接复制到飞书文档继续维护。它和 `UI_INFORMATION_ARCHITECTURE_WBS.puml` 表达同一套结构，但不依赖 PlantUML。

## 1. 总览

- 潜水电脑 UI
  - 启动与运行
  - 屏幕基础参数
  - 全局配置
  - DASH 首页
  - 右侧 TileView 页面
  - INFO MENU
  - SETUP MENU
  - 弹窗与编辑
  - 组件库
  - 运行数据
  - 告警系统
  - 刷新节奏

## 2. 启动与运行

- `WinMain`
  - `lv_init()`
  - `lv_win32_init()`
  - `UI_main()`
    - `ui_init()`
    - `screen_create()`
    - `input_init()`
    - `lv_timer_create(ui_update_task, 50ms)`
    - `sim_data_start()`，仅 PC 仿真
- 主循环
  - `lv_task_handler()`，10ms
  - 数据写入：`bus_set_*()`
  - 数据脏位：`dirty_mask`
  - UI 消费：`ui_update_router_dispatch(mask)`

## 3. 屏幕基础参数

| 分类 | 参数 | 当前值 | 说明 |
|---|---:|---:|---|
| 物理屏幕 | `PHYSICAL_W` | `640` | Windows 仿真窗口宽度 |
| 物理屏幕 | `PHYSICAL_H` | `480` | Windows 仿真窗口高度 |
| 基础单位 | `BASE_U` | `10px` | 1U = 10px |
| Safe Zone | `safe_zone_w` | `580` | 安全显示区宽 |
| Safe Zone | `safe_zone_h` | `420` | 安全显示区高 |
| Safe Zone | `offset_x` | `0` | 水平偏移 |
| Safe Zone | `offset_y` | `-10` | 垂直偏移 |
| Safe Zone | `MASK_EDGE_GUARD` | `80` | 越界保护距离 |
| 固定栏 | `LEFT_ANCHOR_W` | `160` | side 布局固定栏宽 |
| 固定栏 | `TOP_ANCHOR_H` | `120` | top/bottom 布局固定栏高 |
| 固定栏 | `FIXED_SIDE_COLS` | `2` | side 固定栏列数 |
| 固定栏 | `FIXED_SIDE_ROWS` | `7` | side 固定栏行数 |
| 自定义卡 | `CUSTOM_SIDE_COLS` | `5` | side 自定义卡列数 |
| 自定义卡 | `CUSTOM_SIDE_ROWS` | `6` | side 自定义卡行数 |
| 自定义卡 | `CUSTOM_TOP_COLS` | `7` | top/bottom 自定义卡列数 |
| 自定义卡 | `CUSTOM_TOP_ROWS` | `4` | top/bottom 自定义卡行数 |
| 内容区 | `CARD_TITLE_H` | `60` | 卡片标题高度 |
| 内容区 | `panel_gap_u` | `2` | 固定栏与内容区间距 |
| 内容区 | `gap_u` | `0` | 组件间距预留 |

## 4. 全局配置

### 4.1 布局字段

| 参数 | 默认值 | 说明 |
|---|---:|---|
| `theme_mode` | `THEME_TECH` | 当前主技术潜水布局 |
| `layout_order` | `ORDER_REVERSE` | side 默认固定栏在右，内容区在左 |
| `dots_position` | `DOTS_LEFT` | TileView 指示点位置 |
| `compass_style` | `COMPASS_CLASSIC` | 指南针样式 |
| `mask_enabled` | `false` | 软件遮罩 |
| `split_outward` | `true` | 预留动画方向 |
| `flash_speed` | `1` | 预留闪烁速度 |

### 4.2 分割线字段

| 参数 | 默认值 |
|---|---:|
| `sep_style` | `SEP_DASHED` |
| `sep_thick` | `2` |
| `sep_alpha` | `51` |
| `ANCHOR_SEP_THICK` | `3` |
| `ANCHOR_SEP_STYLE` | `SEP_SOLID` |

### 4.3 Classic 预留高度

| 参数 | 默认值 |
|---|---:|
| `h_depth` | `8U` |
| `h_ndl` | `6U` |
| `h_pod` | `6U` |
| `h_batt` | `5U` |
| `h_gas` | `6U` |
| `h_time` | `5U` |
| `title_h_u` | `2U` |
| `h_menu_item` | `5U` |
| `gap_menu` | `1U` |
| `h_tissues_chart` | `9U` |

### 4.4 用户设置默认值

| 设置 | 默认值 |
|---|---:|
| MOD PPO2 | `1.4` |
| Conservatism | `MED` |
| Salinity | `FRESH` |
| Last Deco Stop | `3m` |
| Brightness | `MED` |
| Log Rate | `UI_LOG_RATE_DEFAULT_S` |
| Safety Stop | `UI_SAFETY_STOP_DEFAULT` |
| Altitude | `0` |
| Depth Alarm | `40m` |
| Time Alarm | `60min` |
| NDL Alarm | `5min` |

## 5. 页面结构 TileView

### 5.1 页面 ID

- `PAGE_ID_INFO`
- `PAGE_ID_COMPASS`
- `PAGE_ID_DECO`
- `PAGE_ID_GAS`
- `PAGE_ID_PLAN`
- `PAGE_ID_CUSTOM_GRID`
- `PAGE_ID_BLANK`
- `PAGE_ID_SETUP`
- `PAGE_ID_UNUSED = 0xFF`

### 5.2 页面位置

| 位置 | 页面 | 说明 |
|---|---|---|
| `PAGE_POS_INFO` | `INFO` | 固定入口，不算普通卡 |
| `PAGE_POS_1` | `BLANK` | 默认空白卡 |
| `PAGE_POS_2` | `COMPASS` | 指南针 |
| `PAGE_POS_3` | `DECO` | 减压/组织/毒性 |
| `PAGE_POS_4` | `PLAN` | 轨迹与计划图 |
| `PAGE_POS_5` | `GAS` | 气体 |
| `PAGE_POS_6` | `CUSTOM_GRID` | `custom_cards[0]` |
| `PAGE_POS_7` | `CUSTOM_GRID` | `custom_cards[1]` |
| `PAGE_POS_8~12` | `UNUSED` | 不显示，不占 dot |
| `PAGE_POS_SETUP` | `SETUP` | 固定入口，不算普通卡 |

## 6. DASH 首页

### 6.1 固定栏默认布局

| 坐标 | 组件 |
|---|---|
| row0 col0 | `COMP_NDL_STOP_1606` |
| row1 col0 | `COMP_DEPTH_1612` |
| row3 col0 | `COMP_DIVE_TIME_1606` |
| row4 col0 | `COMP_GAS_1606` |
| row5 col0 | `COMP_EMPTY` |
| row5 col1 | `COMP_EMPTY` |
| row6 col0 | `COMP_SYS_1606` |

### 6.2 右侧页面

- `INFO MENU`
- `BLANK`
- `COMPASS`
- `DECO`
- `PLAN`
- `GAS`
- `CUSTOM GRID: ALARM TARGETS`
- `CUSTOM GRID: SENSOR PREVIEW`
- `SETUP MENU`

## 7. 默认自定义卡片

### 7.1 ALARM TARGETS

| 坐标 | 组件 |
|---|---|
| `0,0` | `COMP_DEPTH_1606` |
| `2,0` | `COMP_PPO2_0806` |
| `3,0` | `COMP_BATTERY_0806` |
| `4,0` | `COMP_POD_0806` |
| `0,1` | `COMP_NDL_STOP_1606` |
| `2,1` | `COMP_CNS_0806` |
| `3,1` | `COMP_OTU_0806` |
| `4,1` | `COMP_HEADING_0806` |
| `0,2` | `COMP_TIME_1606` |
| `2,2` | `COMP_DIVE_TIME_1606` |

### 7.2 SENSOR PREVIEW

| 坐标 | 组件 |
|---|---|
| `0,0` | `COMP_ACCEL_2406` |
| `3,0` | `COMP_BATT_V_0806` |
| `4,0` | `COMP_PRESSURE_0806` |
| `0,1` | `COMP_GYRO_2406` |
| `3,1` | `COMP_CPU_0806` |
| `4,1` | `COMP_FPS_0806` |
| `0,2` | `COMP_MAG_2406` |
| `3,2` | `COMP_BLE_RSSI_0806` |
| `4,2` | `COMP_CHARGE_0806` |
| `0,3` | `COMP_MLX_2406` |
| `3,3` | `COMP_BATT_TEMP_0806` |
| `4,3` | `COMP_PRJ_TEMP_0806` |
| `0,4` | `COMP_TMAG_2406` |
| `3,4` | `COMP_NOFLY_0806` |
| `0,5` | `COMP_ATTITUDE_2406` |
| `3,5` | `COMP_SENSOR_STAT_1606` |

## 8. INFO MENU

- `LAST DIVE`
  - `menu_id = MENU_INFO_LAST_DIVE`
  - `item_id = MENU_ITEM_INFO_LAST_DIVE`
  - 只读信息页
- `DIVE PLAN`
  - `menu_id = MENU_INFO_DIVE_PLAN`
  - `item_id = MENU_ITEM_INFO_DIVE_PLAN`
  - 特殊向导页
  - 操作：`Exit` / `Next` / `Plan` / `More`
- `TISSUE & TOX`
  - `menu_id = MENU_INFO_TISSUE_TOX`
  - `item_id = MENU_ITEM_INFO_TISSUE_TOX`
  - 只读信息页
- `GAS & CALC`
  - `menu_id = MENU_INFO_GAS_CALC`
  - `item_id = MENU_ITEM_INFO_GAS_CALC`
  - 只读信息页
- `SENSOR & DEVICE`
  - `menu_id = MENU_INFO_SENSOR_DEVICE`
  - `item_id = MENU_ITEM_INFO_SENSOR_DEVICE`
  - 只读信息页
- `DIVE LOG`
  - `menu_id = MENU_INFO_DIVE_LOG`
  - `item_id = MENU_ITEM_INFO_DIVE_LOG`
  - 字段：Log No / Start Time / Date
  - 操作：Edit / Delete / More / Exit

## 9. SETUP MENU

### 9.1 GAS SWITCH

- `menu_id = MENU_SETUP_GAS_SWITCH`
- 气体槽：`GAS 1~5`
- item ID：
  - `MENU_ITEM_GAS_SLOT_0`
  - `MENU_ITEM_GAS_SLOT_1`
  - `MENU_ITEM_GAS_SLOT_2`
  - `MENU_ITEM_GAS_SLOT_3`
  - `MENU_ITEM_GAS_SLOT_4`
- 选中后动作：`SHOW_GAS_MODAL`

### 9.2 CONSERVATISM

| 选项 | GF |
|---|---|
| LOW | `40/95` |
| MED | `40/85` |
| HIGH | `30/70` |
| CUSTOM | `50/70` |

### 9.3 BRIGHTNESS

| 选项 | 亮度值 |
|---|---:|
| LOW | `190` |
| MED | `212` |
| HIGH | `232` |
| MAX | `255` |

### 9.4 COMPASS CAL

- `menu_id = MENU_SETUP_COMPASS_CAL`
- `AUTO CAL: status`
- `RESET AUTO CAL`
- 状态：
  - `IDLE`
  - `RUNNING`
  - `READY`

### 9.5 LIGHT CONTROL

- `LIGHT ON/OFF`
- `LIGHT MODE`
- `RED COLOR`
  - `10%`
  - `30%`
  - `50%`
  - `70%`
  - `100%`
- `GREEN COLOR`
  - `10%`
  - `30%`
  - `50%`
  - `70%`
  - `100%`
- `BLUE COLOR`
  - `10%`
  - `30%`
  - `50%`
  - `70%`
  - `100%`
- `WHITE COLOR`
  - `10%`
  - `30%`
  - `50%`
  - `70%`
  - `100%`

### 9.6 SYSTEM SETUP

- `VERSION`
  - 显示：`SYSTEM_VERSION`
- `MODE SETUP`
  - `AIR`
    - `GAS CONFIG`
      - `PPO2`
      - `CONFIRM`
  - `NITROX`
    - `GAS CONFIG`
      - `O2 PERCENT`
      - `PPO2`
      - `CONFIRM`
  - `3 GAS`
    - `GAS 1 O2`
    - `GAS 2 O2`
    - `GAS 3 O2`
    - `CONFIRM`
    - 每个 GAS 可进入 `GAS CONFIG`
      - `O2 PERCENT`
      - `PPO2`
      - `SAVE GAS CONFIG`
  - `OC Tech`
    - `G1 TRIMIX`
    - `G2 TRIMIX`
    - `G3 TRIMIX`
    - `G4 TRIMIX`
    - `G5 TRIMIX`
    - `CONFIRM & ACTIVATE`
    - 每个 Gx 可进入 `GAS CONFIG`
      - `O2 PERCENT`
      - `HE PERCENT`
      - `PPO2`
      - `SAVE GAS CONFIG`
- `DIVE SETUP`
  - `SALINITY`
    - `FRESH`
    - `SALT`
    - `EN13319`
  - `MOD PO2`
    - min：`1.0`
    - max：`1.6`
    - step：`0.1`
  - `SAFETY STOP`
    - `UI_SAFETY_STOP_OFF`
    - `UI_SAFETY_STOP_3MIN`
    - `UI_SAFETY_STOP_4MIN`
    - `UI_SAFETY_STOP_5MIN`
  - `LAST DECO`
    - `3m`
    - `6m`
  - `ALTITUDE`
    - `AUTO`
    - `ALT1`
    - `ALT2`
    - `ALT3`
- `AI SETUP`
  - `T1 MAIN`
    - `UNPAIRED`
    - `PAIRING`
    - `PAIRED`
  - `T2 BUDDY`
    - `UNPAIRED`
    - `PAIRING`
    - `PAIRED`
  - `GTR MODE`
    - `OFF`
    - `ON`
- `ALERTS SETUP`
  - `DEPTH ALARM`
    - default：`40m`
    - min：`10m`
    - max：`150m`
    - step：`10m`
  - `TIME ALARM`
    - default：`60min`
    - min：`10min`
    - max：`300min`
    - step：`10min`
  - `LOW NDL ALARM`
    - default：`5min`
- `DISPLAY`
  - `UNITS`
    - `METRIC`
    - `IMPERIAL`
  - `DATE & CLOCK`
    - `TIME`
      - 副标题：当前时间
      - 24-hour ON：`17:07`
      - 24-hour OFF：`5:07 PM`
      - 点击进入 `TIME` 调节页
      - `HOUR`
        - min：`0`
        - max：`23`
      - `MINUTE`
        - min：`0`
        - max：`59`
    - `DATE`
      - 副标题：当前日期
      - 点击进入 `DATE` 调节页
      - `YEAR`
        - min：`2000`
        - max：`2099`
      - `MONTH`
        - min：`1`
        - max：`12`
      - `DAY`
        - min：`1`
        - max：`31`
    - `24-hour`
      - 行为：ON/OFF 切换
      - ON：24 小时制
      - OFF：12 小时制 AM/PM
      - 修改后刷新 `TIME` 摘要
    - `Date format`
      - 副标题：当前格式样例
      - 点击进入 `DATE FORMAT` 单选页
      - `mm/dd/yyyy`
      - `dd.mm.yyyy`
      - 选中后保存并返回 `DATE & CLOCK`
  - `LOG RATE`
    - `2s`
    - `5s`
    - `10s`
    - `30s`
    - 影响：轨迹记录点和 PLAN 图视觉刷新
  - `BLUETOOTH`
    - `OFF`
    - `ON`
  - `RESET TO DEFAULTS`

## 10. 弹窗与编辑

- `GAS MODAL`
  - 标题：`CONFIRM GAS`
  - 内容：气体名称、MOD
  - 风险提示：`OVER MOD`
  - 操作：`ENTER CONFIRM` / `ESC CANCEL`
- `CONFIRM MODAL`
  - `DIVE MODE`
  - `DISPLAY RESET`
  - `GAS CONFIG`
- `EDIT`
  - `MOD PO2`
  - `O2 PERCENT`
  - `HE PERCENT`
  - `DEPTH ALARM`
  - `TIME ALARM`
  - `NDL ALARM`
  - `TIME` 字段
  - `DATE` 字段
- `TEXT MODAL`
  - 默认文本提示

## 11. 组件库

### 11.1 核心驻留组件

- `COMP_NDL_STOP_1606`
- `COMP_DEPTH_1612`
- `COMP_DEPTH_1606`
- `COMP_DIVE_TIME_1606`
- `COMP_GAS_1606`
- `COMP_SYS_1606`

### 11.2 常用数据组件

- `COMP_TEMP_0806`
- `COMP_TIME_1606`
- `COMP_TTS_0806`
- `COMP_ASCENT_0806`
- `COMP_ASCENT_0812`
- `COMP_COMPASS_1612`
- `COMP_BATTERY_0806`
- `COMP_STOP_DEPTH_0806`
- `COMP_STOP_TIME_1606`
- `COMP_PPO2_0806`
- `COMP_HEADING_0806`
- `COMP_POD_0806`

### 11.3 减压 / 气体 / 毒性组件

- `COMP_SURF_GF_0806`
- `COMP_GF99_0806`
- `COMP_CNS_0806`
- `COMP_OTU_0806`
- `COMP_GF_0806`
- `COMP_MOD_0806`
- `COMP_CEILING_0806`
- `COMP_GAS_MIX_1606`
- `COMP_TISSUE_GF_4012`
- `COMP_TISSUE_RAW_4012`
- `COMP_GAS_DENS_0806`
- `COMP_FIO2_0806`

### 11.4 统计组件

- `COMP_DEPTH_MAX_0806`
- `COMP_DEPTH_AVG_0806`
- `COMP_TEMP_MIN_0806`
- `COMP_TEMP_AVG_0806`

### 11.5 传感器 / 系统组件

- `COMP_GYRO_2406`
- `COMP_BATT_V_0806`
- `COMP_BATT_TEMP_0806`
- `COMP_PRJ_TEMP_0806`
- `COMP_CHARGE_0806`
- `COMP_PRESSURE_0806`
- `COMP_NOFLY_0806`
- `COMP_ACCEL_2406`
- `COMP_MAG_2406`
- `COMP_MLX_2406`
- `COMP_TMAG_2406`
- `COMP_ATTITUDE_2406`
- `COMP_BLE_RSSI_0806`
- `COMP_CPU_0806`
- `COMP_FPS_0806`
- `COMP_SENSOR_STAT_1606`
- `COMP_EMPTY`

## 12. 运行数据

### 12.1 潜水核心

- `depth`
- `ndl`
- `ndl_stop_value`
- `ndl_bar_pct`
- `stop_type`
- `stop_depth_m`
- `stop_time_total_s`
- `stop_time_left_s`
- `in_stop_zone`
- `tts`
- `dive_time_s`
- `surface_time_s`
- `ascent_rate`

### 12.2 气体和瓶压

- `gas_name`
- `gas_active_idx`
- `gas_recommended_idx`
- `ppo2[5]`
- `gas_slot_name[5]`
- `gas_slot_o2_pct[5]`
- `gas_slot_he_pct[5]`
- `gas_slot_mod_m[5]`
- `gas_slot_max_ppo2[5]`
- `gas_slot_count`
- `gas_o2_pct`
- `gas_he_pct`
- `gas_density`
- `fio2_pct`
- `pod1_bar`
- `pod2_bar`
- `cylinder_count`
- `sac_rate`

### 12.3 减压和毒性

- `next_stop_m`
- `next_stop_min`
- `surf_gf`
- `gf99`
- `gf_low`
- `gf_high`
- `mod_m`
- `ceiling_m`
- `tissue_raw_pct[16]`
- `tissue_gf_pct[16]`
- `cns_pct`
- `otu`
- `deco_violation`

### 12.4 温度 / 电池 / 时间

- `temperature_c`
- `bat_temperature_c`
- `prj_temperature_c`
- `battery_voltage_v`
- `battery_pct`
- `charge_state`
- `min_temp`
- `max_temp`
- `avg_temp`
- `max_depth`
- `avg_depth`
- `sys_time_h`
- `sys_time_m`
- `sys_time_s`

### 12.5 传感器和系统

- `ambient_pressure_mbar`
- `nofly_time_min`
- `gyro_x/y/z_dps`
- `accel_x/y/z_g`
- `mag_x/y/z_ut`
- `mlx_x/y/z_ut`
- `tmag_x/y/z_ut`
- `tmag_ut`
- `pitch_deg`
- `roll_deg`
- `attitude_heading_deg`
- `ble_rssi_dbm`
- `cpu_load_pct`
- `fps`
- `sensor_status[16]`

### 12.6 状态开关

- `heading`
- `heading_locked`
- `heading_target`
- `strobe_on`
- `flashlight_on`
- `dirty_mask`

## 13. 告警系统

### 13.1 等级

- `ALARM_INFO`
- `ALARM_WARN`
- `ALARM_CRIT`

### 13.2 CRIT

| 文案 | 靶向组件 |
|---|---|
| ASCENT TOO FAST | `COMP_DEPTH_1606` |
| PO2 CRITICAL | `COMP_PPO2_0806` |
| PO2 TOO LOW | `COMP_PPO2_0806` |
| CEILING BROKEN | `COMP_NDL_STOP_1606` |
| ALGORITHM LOCKED | 无 |
| TANK EMPTY | `COMP_POD_0806` |
| BATTERY DEAD | `COMP_BATTERY_0806` |

### 13.3 WARN

| 文案 | 靶向组件 |
|---|---|
| HIGH PO2 | `COMP_PPO2_0806` |
| NDL LOW | `COMP_NDL_STOP_1606` |
| HIGH CNS | `COMP_CNS_0806` |
| HIGH OTU | `COMP_OTU_0806` |
| SAFETY BROKEN | `COMP_NDL_STOP_1606` |
| TURN PRESSURE | `COMP_POD_0806` |
| TANK PRESSURE DIFF | `COMP_POD_0806` |
| DEPTH LIMIT | `COMP_DEPTH_1606` |
| TIME LIMIT | `COMP_DIVE_TIME_1606` |
| BATTERY LOW | `COMP_BATTERY_0806` |
| POD LOST | 无可见靶向 |

### 13.4 INFO

| 文案 | 靶向组件 |
|---|---|
| SAFETY STOP ACTIVE | `COMP_NDL_STOP_1606` |
| BETTER GAS AVAILABLE | `COMP_GAS_1606` |
| STOP DONE | `COMP_NDL_STOP_1606` |
| CALIBRATE COMPASS | `COMP_HEADING_0806` |

### 13.5 告警节奏

| 参数 | 值 |
|---|---:|
| `ALARM_INFO_DISPLAY_MS` | `5000` |
| `ALARM_BANNER_ROTATE_MS` | `3000` |
| `ALARM_GAS_SWITCH_PROMPT_EXIT_DELTA_M` | `1.0m` |

### 13.6 视觉效果

- `ALARM_TARGET_EFFECT_NONE`
- `ALARM_TARGET_EFFECT_CRIT_FLASH`
- `ALARM_TARGET_EFFECT_WARN_BREATHE`

## 14. 数据刷新节奏

- `ui_update_task`
  - 周期：`50ms`
  - 推进告警视图
  - 消费 `dirty_mask`
- `DECO_REFRESH_MS`
  - 周期：`1000ms`
- `LOG RATE`
  - 可选：`2s` / `5s` / `10s` / `30s`
  - 函数：`dive_log_append_sampled()`
  - 影响：轨迹记录点和 PLAN 图视觉刷新
- 上升率阈值
  - `RATE_LEVEL1_THRESHOLD = 3.0 m/min`
  - `RATE_LEVEL2_THRESHOLD = 9.0 m/min`
  - `RATE_STILL_THRESHOLD = UI_ASCENT_RATE_STILL_DEADBAND_MPM`

## 15. 字体

| 字体 ID | 用途 |
|---|---|
| `FONT_ID_SMALL` | 20px，标签 / 单位 / Badge |
| `FONT_ID_TITLE` | 20px，菜单卡片标题 |
| `FONT_ID_MEDIUM` | 32px，普通数据 |
| `FONT_ID_LARGE` | 64px，深度大数 |
| `FONT_ID_HUGE` | 64px，大字 |
| `FONT_ID_NDL` | 48px，NDL / 减压时间 |

## 16. 代码边界

| 层 | 文件 |
|---|---|
| core | `ui_engine`, `data`, `ui_state`, `update_router`, `callbacks` |
| screen | `screen`, `layout_view`, `page_registry`, `overlay`, `dots`, `edit` |
| views | `menu_defs`, `menu_runtime`, `menu_actions`, `submenu_model`, `submenu_view`, `modal_view` |
| cards | `card_compass`, `card_deco`, `card_gas`, `card_plan`, `card_blank` |
| menus | `menu_info`, `menu_setup` |
| comp | `comp_view`, `comp_style`, `comp_update` |
| alarm | `alarm`, `alarm_view` |

## 17. 维护来源

| 信息 | 来源 |
|---|---|
| 页面 ID | `src/ui/screen/page_registry_types.h` |
| 菜单 ID / 菜单项 ID | `src/ui/views/menu_defs.h` |
| 默认布局 / 默认配置 | `src/ui/core/ui_engine.c` |
| 组件 ID | `src/ui/core/ui_defs.h` |
| 运行数据字段 | `src/ui/core/ui_types.h` |
| 告警 ID / 靶向组件 | `src/ui/alarm/alarm.h`, `src/ui/alarm/alarm.c` |

菜单业务逻辑仍然以 `menu_id_t` / `menu_item_id_t` 为准；本文档里的显示文案只用于阅读、产品沟通和信息架构维护。
