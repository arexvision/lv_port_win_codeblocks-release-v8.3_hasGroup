#ifndef Buhlmann_h
#define Buhlmann_h


// ========== 调试开关（测试工程专用）==========
// #define DEBUG_GF99          // 关闭详细调试，只看结果
// #define DEBUG_CNS_OTU       // 调试氧中毒
#define DEBUG_LEVEL 1          // 1=简单, 2=详细, 3=完整

#define COMPARTMENT_COUNT 16
#define MAX_GASES 4             // 最多支持4种气体（底部气、行程气、减压气/氧气）

// ========== 水体类型枚举 ==========
enum WaterType {
  WATER_SALT = 0,    // 海水（默认，10.06 m/bar）
  WATER_FRESH = 1    // 淡水（10.30 m/bar）
};

// ========== 气体结构体（支持 Trimix 三混气）==========
struct Gas {
  float oxygenFraction;    // 氧气比例（0.21 = 21%）
  float heliumFraction;    // 氦气比例（0 = Nitrox，>0 = Trimix）
  float nitrogenFraction;  // 氮气比例（自动计算 = 1 - O2 - He）
  bool enabled;            // 是否启用
  float modDepth;          // 最大操作深度 MOD（米，基于 PPO2 限制）
  float minDepth;          // 最小使用深度（米，用于减压气）
  
  // 默认构造：空气
  Gas() : oxygenFraction(0.21f), heliumFraction(0.0f), nitrogenFraction(0.79f), 
          enabled(true), modDepth(56.6f), minDepth(0.0f) {}
  
  // Nitrox 构造（无氦气）
  Gas(float o2) : oxygenFraction(o2), heliumFraction(0.0f), nitrogenFraction(1.0f - o2),
                  enabled(true), modDepth(56.6f), minDepth(0.0f) {}
  
  // Trimix 构造（O2 + He）
  Gas(float o2, float he) : oxygenFraction(o2), heliumFraction(he), nitrogenFraction(1.0f - o2 - he),
                            enabled(true), modDepth(56.6f), minDepth(0.0f) {}
};

// 减压阶梯与缓冲（单位：米）
#define DECO_STEP_METERS 3.0f
#define DECO_FINAL_STOP_METERS 6.0f
#define DECO_BUFFER_METERS 0.0f  // 默认 buffer 设为 0.0，可调整为 1.0 以增加缓冲

enum ascendRates
{
  DESCEND = -1,
  ASCEND_OK = 0,
  ASCEND_SLOW = 1,
  ASCEND_NORMAL = 2,
  ASCEND_ATTENTION = 3,
  ASCEND_CRITICAL = 4,
  ASCEND_DANGER = 5,
};

struct DiveResult {
  float compartmentPartialPressures[COMPARTMENT_COUNT];
  float maxDepthInMeters = 0;
  unsigned int durationInSeconds = 0;
  long noFlyTimeInMinutes = 1;
  bool wasDecoDive = false;
  unsigned long previousDiveDateTimestamp = 0;
};

// ========== 减压站序列数据结构（Perdix 2 手册逻辑）==========
// 单个减压站信息
struct DecoStop {
  float depth;           // 停留深度（米，3米倍数）
  int totalTime;        // 总停留时间（秒）
  int remainingTime;    // 剩余停留时间（秒）
  int elapsedTime;      // 已停留时间（秒）
  bool isCompleted;      // 是否已完成
  float targetGF;       // 该站对应的GF值（线性渐变）
};

// 减压站序列（最多16站，覆盖0-60米）
#define BUHLMANN_MAX_DECO_STOPS 16
struct DecoStopSequence {
  DecoStop stops[BUHLMANN_MAX_DECO_STOPS];
  int stopCount;        // 实际站数量
  int currentStopIdx;   // 当前待完成的站索引（-1表示无减压义务）
};

struct DiveInfo {
  int ascendRate;
  bool decoNeeded;
  int minutesToDeco;
  int decoStopInMeters;
  int decoStopDurationInMinutes;
  int decoStopDurationInSeconds;   // 当前减压站剩余时间（秒，供 UI 实时倒计时）
  int ttsSeconds;                 // TTS (Time To Surface) 总上升时间（秒）
  // ========== 新增：完整减压站序列支持 ==========
  DecoStopSequence decoSequence;  // 完整减压站序列
  bool isMissedDecoStop;          // 是否跳过当前减压站（违规）
  float effectiveCeiling;         // 有效天花板深度（米，用于锁定）
  bool isAlgorithmLocked;         // 算法是否被锁定（3分钟未能完成减压）
  // ========== 显示相关数据（由buhlmann_task计算）==========
  float ppo2;                     // 氧分压
  float cns;                      // CNS百分比
  float otu;                      // OTU值
  float gf99;                     // GF99
  float surfGF;                   // 水面GF
  float compartmentPressures[COMPARTMENT_COUNT];  // 16舱氮分压
};

class Buhlmann {
public:
  // waterVapourPressureCorrection: 水蒸气压修正（mbar）
  Buhlmann(float waterVapourPressureCorrection);

  // ========== 环境设置接口（海拔、水体类型）==========
  // 设置海拔高度（米），自动计算对应的大气压
  void setAltitude(float altitudeMeters);
  float getAltitude();                      // 获取当前海拔设置（米）
  
  // 设置水体类型（海水/淡水），影响深度-压力换算
  void setWaterType(WaterType type);
  WaterType getWaterType();                 // 获取当前水体类型
  
  // 获取当前水面大气压（mbar），受海拔影响
  float getSurfacePressure();
  
  // 获取每 bar 对应的深度（米），受水体类型影响
  float getDepthPerBar();

  // ========== 多气体支持接口（最多3种气体）==========
  void setGas(int gasIndex, float oxygenFraction, float heliumFraction = 0.0f, bool enabled = true, float modPPO2 = 1.4f);
  Gas getGas(int gasIndex);
  
  void setActiveGas(int gasIndex);
  int getActiveGas();                       // 获取当前使用的气体槽位
  
  int getBestGasForDepth(float depthMeters);

  // 【修改】加入 isAscending 参数，默认 true 兼容旧代码
  bool hasBetterGasAvailable(float depthMeters, bool isAscending = true);

  // 【新增】等压反向扩散 (ICD) 风险检查
  bool checkICDRisk(int targetGasIndex, float currentDepthMeters);
  
  // ========== PPO2 限制设置 ==========
  void setBottomPPO2(float ppo2);
  float getBottomPPO2();
  
  void setDecoPPO2(float ppo2);
  float getDecoPPO2();

  // ========== 原有气体接口（保持兼容）==========
  void setSeaLevelAtmosphericPressure(float seaLevelAtmosphericPressure); // 设置海平面大气压
  void setNitrogenRateInGas(float nitrogenRateInGas);                    // 设置气体中的氮气比例
  void setOxygenRateInGas(float oxygenRateInGas);                        // 设置气体中的氧气比例（自动计算氮气）
  float getOxygenRateInGas();                                            // 获取当前氧气比例
  float getNitrogenRateInGas();                                          // 获取当前氮气比例
  float calculateMOD(float maxPPO2 = 1.4f);                              // 计算最大操作深度（米）

  // ========== GF Low/GF High 设置接口 ==========
  void setGFLow(float gfLow);    // 设置首停梯度因子（0.0-1.0）
  void setGFHigh(float gfHigh);  // 设置出水梯度因子（0.0-1.0）
  float getGFLow();              // 获取当前 GF Low
  float getGFHigh();             // 获取当前 GF High
  float getSeaLevelAtmosphericPressure(); // 获取海平面大气压设置

  // 深度/压力换算
  float calculateDepthFromPressure(float pressure);        // 环境压力 → 深度
  float calculateHydrostaticPressureFromDepth(float depth); // 深度 → 环境压力

  // 常用数组工具
  void copyArray(float destination[], float source[]);
  float maxSearch(float array[], int size);
  float minSearch(float array[], int size);
  int maxSearch(int array[], int size);
  int minSearch(int array[], int size);

  // 潜水流程
  DiveResult* surfaceInterval(long surfaceIntervalInMinutes, DiveResult* previousDiveResult);
  DiveResult* initializeCompartments();                                                      
  void startDive(DiveResult* previousDiveResult, unsigned long diveStartTimestamp);          
  DiveInfo progressDive(float currentPressure, unsigned int duration);                       
  DiveResult* stopDive(unsigned long diveStopTimestamp);                                     
  
  float getLastCeilingPressure();                                                            
  int getLastCeilingCompartmentIndex();                                                      
  float calculateGFAtAmbientPressure(float ambientPressureMbar, int compartmentIndex = -1, bool useGFCorrection = false);

  // 氮/氧/氦分压计算
  float calculateNitrogenPartialPressureInLung(float currentPressure); 
  float calculateHeliumPartialPressureInLung(float currentPressure);   
  float calculateOxygenPartialPressure(float currentPressure);         
  long calculateDesaturationTime(float limitPercentage);               
  long calculateNoFlyTime(long desaturationTimeInMinutes);             
  
  // CNS 氧中毒
  void updateCNS(float ppo2, float timeInMinutes);  
  float getCumulativeCNS();                         
  void resetCNS();                                  
  
  // OTU 氧中毒
  void updateOTU(float ppo2, float timeInMinutes);  
  float getCumulativeOTU();                         
  void resetOTU();                                  
  
  // GF 相关
  float calculateGF99();                            
  float calculateSurfaceGF();                       
  
  // dynamic deco info getters
  int getLastDynamicRequiredSeconds();
  float getLastDynamicStopDepthMeters();
  unsigned long getLastDynamicCalcMillis();

  void requestImmediatePrint();
  bool consumeImmediatePrintRequest();

  // ========== 单位切换功能 ==========
  void setUnitMetric(bool isMetric);  
  bool isUnitMetric();                
  float convertDepthForDisplay(float depthInMeters);  
  const char* getDepthUnitString();   

  // 首停深度计算
  float calculateFirstStopDepthWithGF(float currentDepth, float gfLow);
  int getLastGF99WarningLevel();                    
  float getLastAllowedHighPressureMbar();           
  float getLastAllowedOrigPressureMbar();           
    
  // 获取当前舱压力/M 值/NDL
  void getCurrentCompartmentPressures(float compartmentPressures[COMPARTMENT_COUNT]); 
  void getCurrentCompartmentHePressures(float hePressures[COMPARTMENT_COUNT]);        
  float getCompartmentN2Pressure(int compartmentIndex);                               
  float getCompartmentHePressure(int compartmentIndex);                               
  float getCompartmentTotalInertLoad(int compartmentIndex);                           
  float getCompartmentCombinedA(int compartmentIndex);                                
  float getCompartmentCombinedB(int compartmentIndex);                                
  float getCompartmentMValuePressure(int compartmentIndex);                           
  int getSecondsNeededTillDeco(int compartmentIndex, float currentPressure);          
  double getSecondsNeededTillDecoDouble(int compartmentIndex, float currentPressure); 

  // ========== 诊断与调参接口 ==========
  void printFirstStopDiagnostics(float gfLow);
  void printCeilVsFirstStopDebug();

  void setFastCompartmentMaxIndex(int maxIndex);
  int getFastCompartmentMaxIndex();

  void testGFCalculation();  

  // ========== 减压序列访问接口 ==========
  DecoStopSequence& getDecoSequence() { return _decoSequence; }
  void setCurrentDecoStopIdx(int idx) {
      if (idx >= -1 && idx < _decoSequence.stopCount) {
          _decoSequence.currentStopIdx = idx;
          if (idx >= 0) {
              _effectiveCeiling = _decoSequence.stops[idx].depth;
          }
      }
  }

  void recalculateCurrentDecoStopDuration();
  float getAscendToPartialPressureForCompartmentWithGF(int compartmentIndex, float compartmentPartialPressure, float currentGF);

private:
  float _seaLevelAtmosphericPressure;
  float _nitrogenRateInGas;
  float _oxygenRateInGas;           // 氧气比例（0.21 = 21%）
  float _waterVapourPressureCorrection;

  // ========== 环境设置成员 ==========
  float _altitudeMeters;            // 海拔高度（米）
  WaterType _waterType;             // 水体类型（海水/淡水）
  float _depthPerBar;               // 每 bar 对应的深度（米）
  
  // ========== 多气体支持成员 ==========
  Gas _gases[MAX_GASES];            // 气体配置数组（最多3种）
  int _activeGasIndex;              // 当前使用的气体槽位
  float _bottomPPO2;                // 底部阶段 PPO2 上限
  float _decoPPO2;                  // 减压阶段 PPO2 上限

  // 诊断/调参成员
  int _fastCompartmentMaxIndex;     // 使用 0..N 的快组织范围
  float _deepDiveCorrectionSlope;   // 深潜修正斜率（每米）
  float _deepDiveCorrectionMax;     // 深潜修正最大倍数
  bool _requestImmediatePrint;      // 请求主循环立即打印标志
  
  // 单位设置
  bool _isUnitMetric;               // true=米, false=英尺

  float _halfTimesNitrogen[COMPARTMENT_COUNT];
  float _aValuesNitrogen[COMPARTMENT_COUNT];
  float _bValuesNitrogen[COMPARTMENT_COUNT];

  // ========== 氦气参数表（ZHL-16C）==========
  float _halfTimesHelium[COMPARTMENT_COUNT];   // He 半衰期
  float _aValuesHelium[COMPARTMENT_COUNT];     // He 的 a 系数
  float _bValuesHelium[COMPARTMENT_COUNT];     // He 的 b 系数

  DiveResult* _previousDiveResult;
  long _diveStartTimestamp;

  float _compartmentCurrentPartialPressures[COMPARTMENT_COUNT];
  float _compartmentHePartialPressures[COMPARTMENT_COUNT];  // He 载荷
  unsigned int _currentDiveDuration; //In seconds
  float _currentDepth;
  float _lastPressure;  // 上一次的环境压力
  float _maxDepth;
  bool _wasDecoDive;
  float _lastCeilingPressureMbar; 
  int _lastCeilingCompartmentIndex; 
  
  // GF99 警示/阈值追踪
  int _lastGF99WarningLevel;               
  float _lastAllowedHighPressureMbar;      
  float _lastAllowedOrigPressureMbar;      
  
  float _cumulativeCNS;  
  float _cumulativeOTU;  
  int _lastDynamicRequiredSeconds;    
  float _lastDynamicStopDepthMeters;  
  unsigned long _lastDynamicCalcMillis; 
  
  // ========== GF Low/GF High（梯度因子控制参数）==========
  float _gfLow;   
  float _gfHigh;  
  float _gfLowDepthPressure;  
  
  // ========== 减压站序列管理（Perdix 2 手册逻辑）==========
  DecoStopSequence _decoSequence;  
  float _firstStopDepth;           
  float _effectiveCeiling;         
  bool _isMissedDeco;              
  bool _isAlgorithmLocked;         
  unsigned long _missedDecoStartMillis; 

  float getCompartmentHalfTimeInSeconds(int compartmentIndex);
  float getCompartmentHeHalfTimeInSeconds(int compartmentIndex);  
  float getCompartmentPartialPressure(int compartmentIndex);
  float getCompartmentHePartialPressure(int compartmentIndex);    
  void setCompartmentPartialPressure(int compartmentIndex, float partialPressure);
  void setCompartmentHePartialPressure(int compartmentIndex, float partialPressure);  

  float calculateCompartmentInertGasPartialPressure(long timeInSeconds, float halfTimeInSeconds, float currentInertGasPartialPressureInCompartment, float currentInertGasPartialPressureInLung);
  float calculateCompartmentPressureSchreiner(int compartmentIndex, float pAlvStart, float R, float timeInSeconds);
  float calculateCompartmentHePressureSchreiner(int compartmentIndex, float pAlvHeStart, float R_He, float timeInSeconds);
  float getAscendToPartialPressureForCompartment(int compartmentIndex, float compartmentPartialPressure);
  
  bool isDecoNeeded(float ascendToPartialPressure);
  int getMinutesNeededTillDeco(int compartmentIndex, float currentPressure);
  int calculateMinutesRequiredToReachCertainPressure(float targetPressure);
  int calculateAscentRate(float timeSpentInLevelInSeconds, float previousDepthInMeter, float currentDepthInMeter);
  
  float calculateCurrentGF(float currentStopDepth, float firstStopDepth, float finalStopDepth);
  
  // ========== 减压站序列生成与管理 ==========
  void generateDecoSequence(float currentPressure, float currentDepth);  
  void updateCurrentDecoStop(float currentDepth, unsigned int timeSpentInLevel);  
  void updateCurrentDecoStopSwitchOnly(float currentDepth);  
  bool isDecoStopMissed(float currentDepth);  

  // 🚨 注意这行！已更新为支持双轨气体衰减与切换气预测的签名
  int calculateDecoStopDuration(float stopPressure, float nextStopPressure, float nextStopGF, float tempN2[], float tempHe[], Gas simGas);  

  unsigned long _lastDecoRegenMillis;    
  float _lastGeneratedFirstStopDepth;    
};

#endif
