# 潜水电脑 UI 信息架构

## DASH 主界面

- 固定数据区
  - NDL / 停留信息
    - 免减压时间，单位 min
    - 安全停留状态
    - 减压停留状态
    - 停留深度，单位 m
    - 停留剩余时间
    - 停留进度条
  - 当前深度
    - 深度数值，单位 m
    - 上升 / 下潜速率提示
    - 上升过快告警状态
  - 潜水时间
    - 当前潜水时长
  - 当前气体
    - 当前活动气体名称
    - 当前气体槽位
  - 系统信息
    - 电池状态
    - 温度状态
    - 设备状态
- 右侧页面区
  - INFO MENU [滑动进入]
  - COMPASS [滑动进入]
  - DECO [滑动进入]
  - PLAN [滑动进入]
  - GAS [滑动进入]
  - 自定义数据页 [滑动进入]
  - SETUP MENU [滑动进入]

## INFO MENU

- LAST DIVE [点击 -> 查看上一次潜水]
- DIVE PLAN [点击 -> 进入潜水计划]
- TISSUE & TOX [点击 -> 查看组织与氧毒性]
- GAS & CALC [点击 -> 查看气体与计算数据]
- SENSOR & DEVICE [点击 -> 查看传感器与设备信息]
- DIVE LOG [点击 -> 查看潜水日志]

### LAST DIVE

- 潜水摘要
  - 潜水编号
  - 日期
  - 开始时间
  - 结束时间
  - 潜水时长
  - 水面间隔
- 深度统计
  - 最大深度，单位 m
  - 平均深度，单位 m
- 生理与减压摘要
  - 起始 CNS，单位 %
  - 结束 CNS，单位 %
  - 减压模型
  - 潜水模式
- 气体与耗气摘要
  - 平均 SAC，单位 L/min
  - 气瓶起始压力
  - 气瓶结束压力

### DIVE PLAN

- 计划输入
  - 计划深度，单位 m [编辑]
  - 计划时间，单位 min [编辑]
  - 呼吸量 RMV，单位 L/min [编辑]
  - 使用气体
  - 保守度 / GF 设置
- 计划结果
  - NDL
  - TTS
  - 减压停留列表
  - 每站停留深度，单位 m
  - 每站停留时间，单位 min
  - 预计总时间
  - 预计气体消耗
- 操作
  - Exit [点击 -> 退出潜水计划]
  - Next [点击 -> 下一步]
  - Plan [点击 -> 开始计算]
  - More [点击 -> 查看更多结果]

### TISSUE & TOX

- 组织负荷
  - 16 个组织舱原始负荷
  - 16 个组织舱 GF 负荷
- 氧毒性
  - CNS，单位 %
  - OTU
- 减压压力
  - GF99
  - Surf GF
  - Ceiling，单位 m
  - NDL
  - TTS

### GAS & CALC

- 当前气体
  - 气体名称
  - 活动气体槽位
  - O2 比例，单位 %
  - He 比例，单位 %
  - PPO2，单位 bar
  - MOD，单位 m
  - FiO2
  - 气体密度
- 气体建议
  - 推荐气体
  - 更优气体提示
  - 是否超过 MOD

### SENSOR & DEVICE

- 电源
  - 电池电量，单位 %
  - 电池电压，单位 V
  - 充电状态
- 温度
  - 水温 / 主温度，单位 °C
  - 电池温度，单位 °C
  - 主板 / 光学温度，单位 °C
- 环境
  - 环境压力，单位 mbar
  - 禁飞时间，单位 min
- 姿态与运动
  - 俯仰角
  - 横滚角
  - 姿态航向
  - 加速度计数据
  - 陀螺仪数据
  - 磁力计数据
- 连接与性能
  - BLE 信号强度，单位 dBm
  - CPU 负载，单位 %
  - UI 帧率
  - 传感器状态

### DIVE LOG

- 日志列表
  - 潜水编号
  - 日期
  - 开始时间
  - 结束时间
  - 潜水时长
  - 最大深度，单位 m
  - 平均深度，单位 m
- 日志详情
  - 水面间隔
  - 水面压力，单位 mbar
  - 起始 CNS，单位 %
  - 结束 CNS，单位 %
  - 平均 SAC，单位 L/min
  - 潜水模式
  - 减压模型
  - 各气瓶起始压力
  - 各气瓶结束压力
- 操作
  - More [点击 -> 查看更多日志]
  - Edit [点击 -> 编辑日志]
  - Delete [点击 -> 删除确认]
  - Exit [点击 -> 返回]

## COMPASS 页面

- 指南针显示
  - 当前航向，单位 °
  - 航向刻度
  - 方向指示
- 航向锁定
  - 当前锁定状态
  - 目标航向，单位 °
  - 航向偏差，单位 °
  - 锁定航向 [点击 / 按键 -> 锁定当前航向]
  - 解除锁定 [点击 / 按键 -> 清除锁定]
- 校准状态
  - 未校准
  - 校准中
  - 校准完成
  - 需要校准提示

## DECO 页面

- 减压概览
  - NDL，单位 min
  - TTS，单位 min
  - Ceiling，单位 m
  - 当前停留类型
  - 当前停留深度，单位 m
  - 当前停留剩余时间
  - 是否处于停留区
- 组织与 GF
  - Tissue Raw
  - Tissue GF
  - GF99
  - Surf GF
  - GF Low
  - GF High
- 氧毒性
  - CNS，单位 %
  - OTU
- 状态
  - 无减压
  - 安全停留
  - 减压停留
  - 停留违规
  - 突破 Ceiling

## PLAN 页面

- 潜水轨迹
  - 当前潜水时间
  - 当前深度，单位 m
  - 历史轨迹曲线
  - 当前点
- 减压预测
  - 预计总时间
  - 减压停留列表
  - 停留深度，单位 m
  - 停留时长，单位 min
- 坐标轴
  - 时间轴
  - 深度轴
- 刷新规则
  - 按 LOG RATE 记录轨迹点
  - 按 LOG RATE 刷新 PLAN 图
  - LOG RATE 可选：2s / 5s / 10s / 30s

## GAS 页面

- 当前气体
  - 活动气体名称
  - 活动气体槽位
  - O2 比例，单位 %
  - He 比例，单位 %
  - PPO2，单位 bar
  - MOD，单位 m
  - 是否超过 MOD
- 气体列表
  - GAS 1
    - 名称
    - O2 比例，单位 %
    - He 比例，单位 %
    - MOD，单位 m
    - PPO2，单位 bar
  - GAS 2
  - GAS 3
  - GAS 4
  - GAS 5
- 操作
  - 选择气体 [点击 -> 打开 CONFIRM GAS 弹窗]
  - 推荐气体提示 [状态变化 -> 显示提示]

## 自定义数据页

- 自定义数据页 [滑动 -> 进入]
  - 页面用途：按产品需要组合多个数据模块，用于扩展默认主界面之外的信息展示
  - 页面标题
  - 数据模块组合
  - 数据模块排序
  - 数据模块显示 / 隐藏 [配置 -> 更新页面内容]
- 默认模块池
  - ALARM TARGETS
    - 当前深度，单位 m
    - 当前 PPO2，单位 bar
    - 电池电量，单位 %
    - 气瓶压力，单位 bar
    - NDL / 停留状态
    - CNS / OTU
    - 当前航向，单位 °
    - 当前时间 / 潜水时间
  - SENSOR PREVIEW
    - 传感器读数预览
    - 电源与环境状态
    - 连接与系统状态
- 模块归类建议
  - ALARM TARGETS：可作为正式默认自定义页保留
  - SENSOR PREVIEW：建议归入工程诊断 / 调试页，不作为项目经理版主信息架构展开
  - 其他默认模块：作为模块池存档，不进入主导航层级

## SETUP MENU

- GAS SWITCH [点击 -> 进入气体切换]
- CONSERVATISM [点击 -> 进入保守度设置]
- BRIGHTNESS [点击 -> 进入亮度设置]
- COMPASS CAL [点击 -> 进入指南针校准]
- LIGHT CONTROL [点击 -> 进入灯光控制]
- SYSTEM SETUP [点击 -> 进入系统设置]

### GAS SWITCH

- 气体槽
  - GAS 1 [点击 -> 打开 CONFIRM GAS 弹窗]
  - GAS 2 [点击 -> 打开 CONFIRM GAS 弹窗]
  - GAS 3 [点击 -> 打开 CONFIRM GAS 弹窗]
  - GAS 4 [点击 -> 打开 CONFIRM GAS 弹窗]
  - GAS 5 [点击 -> 打开 CONFIRM GAS 弹窗]
- 弹窗校验
  - 显示目标气体
  - 显示目标气体 MOD
  - 当前深度超过 MOD 时显示危险提示

### CONSERVATISM

- LOW
  - GF：40/95
  - LOW [点击 -> 立即应用]
- MED
  - GF：40/85
  - MED [点击 -> 立即应用]
- HIGH
  - GF：30/70
  - HIGH [点击 -> 立即应用]
- CUSTOM
  - GF：50/70
  - CUSTOM [点击 -> 立即应用]

### BRIGHTNESS

- LOW [点击 -> 应用亮度]
  - 亮度值：190
- MED [点击 -> 应用亮度]
  - 亮度值：212
- HIGH [点击 -> 应用亮度]
  - 亮度值：232
- MAX [点击 -> 应用亮度]
  - 亮度值：255

### COMPASS CAL

- AUTO CAL
  - 当前状态
    - IDLE
    - RUNNING
    - READY
  - AUTO CAL [点击 -> 开始校准]
- RESET AUTO CAL [点击 -> 重置校准]

### LIGHT CONTROL

- LIGHT ON/OFF [点击 -> 开关灯光]
- LIGHT MODE [点击 -> 切换灯光模式]
- RED COLOR [点击 -> 进入红色亮度]
  - 10% [点击 -> 应用]
  - 30% [点击 -> 应用]
  - 50% [点击 -> 应用]
  - 70% [点击 -> 应用]
  - 100% [点击 -> 应用]
- GREEN COLOR [点击 -> 进入绿色亮度]
  - 10% [点击 -> 应用]
  - 30% [点击 -> 应用]
  - 50% [点击 -> 应用]
  - 70% [点击 -> 应用]
  - 100% [点击 -> 应用]
- BLUE COLOR [点击 -> 进入蓝色亮度]
  - 10% [点击 -> 应用]
  - 30% [点击 -> 应用]
  - 50% [点击 -> 应用]
  - 70% [点击 -> 应用]
  - 100% [点击 -> 应用]
- WHITE COLOR [点击 -> 进入白色亮度]
  - 10% [点击 -> 应用]
  - 30% [点击 -> 应用]
  - 50% [点击 -> 应用]
  - 70% [点击 -> 应用]
  - 100% [点击 -> 应用]

### SYSTEM SETUP

- VERSION
  - 当前软件版本
- MODE SETUP [点击 -> 潜水模式设置]
- DIVE SETUP [点击 -> 潜水参数设置]
- AI SETUP [点击 -> 气瓶 / AI 设置]
- ALERTS SETUP [点击 -> 告警设置]
- DISPLAY [点击 -> 显示设置]

## MODE SETUP

### AIR

- AIR [点击 -> 进入 AIR 气体配置]
- AIR 气体配置
  - PPO2 [编辑]
  - CONFIRM [点击 -> 确认并激活 AIR]

### NITROX

- NITROX [点击 -> 进入 NITROX 气体配置]
- NITROX 气体配置
  - O2 PERCENT [编辑]
    - 默认：32%
    - 最小：21%
    - 最大：40%
    - 步进：1%
  - PPO2 [编辑]
  - CONFIRM [点击 -> 确认并激活 NITROX]

### 3 GAS

- GAS 1 [点击 -> 编辑 GAS 1]
  - O2 PERCENT [编辑]
  - PPO2 [编辑]
  - SAVE GAS CONFIG [点击 -> 保存]
- GAS 2 [点击 -> 编辑 GAS 2]
  - O2 PERCENT [编辑]
  - PPO2 [编辑]
  - SAVE GAS CONFIG [点击 -> 保存]
- GAS 3 [点击 -> 编辑 GAS 3]
  - O2 PERCENT [编辑]
  - PPO2 [编辑]
  - SAVE GAS CONFIG [点击 -> 保存]
- CONFIRM [点击 -> 确认并激活 3 GAS]

### OC Tech

- G1 TRIMIX [点击 -> 编辑 G1]
- G2 TRIMIX [点击 -> 编辑 G2]
- G3 TRIMIX [点击 -> 编辑 G3]
- G4 TRIMIX [点击 -> 编辑 G4]
- G5 TRIMIX [点击 -> 编辑 G5]
- 每个 TRIMIX 气体配置
  - O2 PERCENT [编辑]
    - 最小：8%
    - 最大：100% - He%
    - 步进：1%
  - HE PERCENT [编辑]
    - 最小：0%
    - 最大：100% - O2%
    - 步进：1%
  - PPO2 [编辑]
  - SAVE GAS CONFIG [点击 -> 保存]
- CONFIRM & ACTIVATE [点击 -> 确认并激活 OC Tech]

## DIVE SETUP

- SALINITY [点击 -> 循环切换]
  - FRESH
  - SALT
  - EN13319
- MOD PO2 [点击 -> 编辑]
  - 默认：1.4
  - 最小：1.0
  - 最大：1.6
  - 步进：0.1
- SAFETY STOP [点击 -> 循环切换]
  - OFF
  - 3min
  - 4min
  - 5min
- LAST DECO [点击 -> 循环切换]
  - 3m
  - 6m
- ALTITUDE [点击 -> 循环切换]
  - AUTO
  - ALT1
  - ALT2
  - ALT3

## AI SETUP

- T1 MAIN [点击 -> 循环切换]
  - UNPAIRED
  - PAIRING
  - PAIRED
- T2 BUDDY [点击 -> 循环切换]
  - UNPAIRED
  - PAIRING
  - PAIRED
- GTR MODE [点击 -> 切换]
  - OFF
  - ON

## ALERTS SETUP

- DEPTH ALARM [点击 -> 编辑]
  - 默认：40m
  - 最小：10m
  - 最大：150m
  - 步进：10m
- TIME ALARM [点击 -> 编辑]
  - 默认：60min
  - 最小：10min
  - 最大：300min
  - 步进：10min
- LOW NDL ALARM [点击 -> 编辑]
  - 默认：5min

## DISPLAY

- UNITS [点击 -> 切换]
  - METRIC
  - IMPERIAL
- Time/date [点击 -> 进入日期时间设置]
- LOG RATE [点击 -> 循环切换]
  - 2s
  - 5s
  - 10s
  - 30s
  - 用于轨迹记录点和 PLAN 图刷新
- BLUETOOTH [点击 -> 切换]
  - OFF
  - ON
- RESET TO DEFAULTS [点击 -> 打开确认弹窗]

### Time/date

- YEAR [编辑]
  - 最小：2000
  - 最大：2099
  - 步进：1
- MONTH [编辑]
  - 最小：1
  - 最大：12
  - 步进：1
- DAY [编辑]
  - 最小：1
  - 最大：31
  - 步进：1
- HOUR [编辑]
  - 最小：0
  - 最大：23
  - 步进：1
- MINUTE [编辑]
  - 最小：0
  - 最大：59
  - 步进：1
- 24-hour [点击 -> 切换]
  - ON
  - OFF
  - ON 时显示 24 小时制
  - OFF 时显示 12 小时制 AM / PM
- Date format [点击 -> 进入日期格式选择]
  - mm/dd/yyyy [点击 -> 应用]
  - dd.mm.yyyy [点击 -> 应用]

## CONFIRM GAS 弹窗

- 弹窗内容
  - 目标气体名称
  - MOD，单位 m
  - 当前深度
  - 是否超过 MOD
- 操作
  - Confirm [点击 -> 切换气体]
  - Cancel [点击 -> 取消切换]
- 状态
  - 正常可切换
  - 超过 MOD 风险提示

## RESET DEFAULTS 确认弹窗

- 弹窗内容
  - 重置显示设置
  - 恢复单位、日志频率、蓝牙、告警阈值等默认值
- 操作
  - Confirm [点击 -> 恢复默认]
  - Cancel [点击 -> 取消]

## 告警横幅

- CRITICAL 告警
  - ASCENT TOO FAST
  - PO2 CRITICAL
  - PO2 TOO LOW
  - CEILING BROKEN
  - ALGORITHM LOCKED
  - TANK EMPTY
  - BATTERY DEAD
- WARNING 告警
  - HIGH PO2
  - NDL LOW
  - HIGH CNS
  - HIGH OTU
  - SAFETY BROKEN
  - TURN PRESSURE
  - TANK PRESSURE DIFF
  - DEPTH LIMIT
  - TIME LIMIT
  - BATTERY LOW
  - POD LOST
- INFO 提示
  - SAFETY STOP ACTIVE
  - BETTER GAS AVAILABLE
  - STOP DONE
  - CALIBRATE COMPASS
- 告警交互
  - 当前告警确认 [点击 -> 确认当前告警]
  - 更优气体提示确认 [点击 -> 进入气体切换确认]

## 全局可配置项汇总

- 潜水模式
  - AIR
  - NITROX
  - 3 GAS
  - OC Tech
- 气体参数
  - O2 百分比
  - He 百分比
  - PPO2
  - MOD
  - 气体槽数量
  - 活动气体
- 减压参数
  - Conservatism
  - GF Low
  - GF High
  - MOD PO2
  - Last Deco Stop
  - Safety Stop
- 环境参数
  - Salinity
  - Altitude
- 告警参数
  - Depth Alarm
  - Time Alarm
  - Low NDL Alarm
- 显示参数
  - Units
  - Date
  - Clock
  - 24-hour
  - Date format
  - Log Rate
  - Brightness
  - Bluetooth
- 外设参数
  - Compass Calibration
  - Light Power
  - Light Mode
  - Light Color Level
  - AI Tank State
  - GTR Mode

## 页面导航关系

- DASH 主界面
  - INFO MENU [滑动 -> 进入]
    - LAST DIVE
    - DIVE PLAN
    - TISSUE & TOX
    - GAS & CALC
    - SENSOR & DEVICE
    - DIVE LOG
  - COMPASS [滑动 -> 进入]
  - DECO [滑动 -> 进入]
  - PLAN [滑动 -> 进入]
  - GAS [滑动 -> 进入]
  - 自定义数据页 [滑动 -> 进入，可选默认页]
  - SETUP MENU [滑动 -> 进入]
    - GAS SWITCH
    - CONSERVATISM
    - BRIGHTNESS
    - COMPASS CAL
    - LIGHT CONTROL
    - SYSTEM SETUP
      - MODE SETUP
      - DIVE SETUP
      - AI SETUP
      - ALERTS SETUP
      - DISPLAY
