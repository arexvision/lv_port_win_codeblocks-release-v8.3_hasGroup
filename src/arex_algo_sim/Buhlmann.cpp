#include "rtthread.h"
#include "rtdevice.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Arduino 兼容宏
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define map(value, fromLow, fromHigh, toLow, toHigh) \
    ((value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow)
#define byte uint8_t

#include "Buhlmann.h"

// 构造函数：初始化 Buhlmann 算法参数（16个组织舱的半衰期、M值系数、GF默认值等）
Buhlmann::Buhlmann(float waterVapourPressureCorrection) {
	_waterVapourPressureCorrection = waterVapourPressureCorrection;

	// 默认参数
	_nitrogenRateInGas = 0.79;  // 空气中氮气比例：79%
	_oxygenRateInGas = 0.21;    // 空气中氧气比例：21%
	_seaLevelAtmosphericPressure = 1013.25;  // 海平面大气压：1013.25 mbar（标准大气压）
	_previousDiveResult = NULL;
	_wasDecoDive = false;
	_diveStartTimestamp = 0;
	
	_lastCeilingPressureMbar = _seaLevelAtmosphericPressure;
	_lastCeilingCompartmentIndex = -1;
	
	// ========== 初始化环境设置 ==========
	_altitudeMeters = 0.0f;           // 默认海平面
	_waterType = WATER_SALT;          // 默认海水
	_depthPerBar = 10.0f;             // 原来是 海水：10.06 m/bar，改成 10.0f 和 Java 保持一致
	
	// ========== 初始化多气体支持 ==========
	_activeGasIndex = 0;              // 默认使用第一个气体槽位
	_bottomPPO2 = 1.4f;               // 底部 PPO2 上限
	_decoPPO2 = 1.6f;                 // 减压 PPO2 上限
	
	// 初始化气体槽位（默认都是空气）
	for (int i = 0; i < MAX_GASES; i++) {
		_gases[i].oxygenFraction = 0.21f;
		_gases[i].heliumFraction = 0.0f;   // 空气无氦气
		_gases[i].nitrogenFraction = 0.79f;
		_gases[i].enabled = (i == 0);  // 只启用第一个槽位
		_gases[i].modDepth = calculateMOD(_bottomPPO2);
		_gases[i].minDepth = 0.0f;
	}
	
	// ========== 初始化减压站序列管理变量 ==========
	_decoSequence.stopCount = 0;
	_decoSequence.currentStopIdx = -1;
	_firstStopDepth = 0.0f;
	_effectiveCeiling = 0.0f;
	_isMissedDeco = false;

	//Coefficients of the ZH-L16C-GF algorithm  16 个组织舱的 "氮气半衰期"
	_halfTimesNitrogen[0] = 5.0;    // ZH-L16C 标准值
	_halfTimesNitrogen[1] = 8.0;
	_halfTimesNitrogen[2] = 12.5;
	_halfTimesNitrogen[3] = 18.5;
	_halfTimesNitrogen[4] = 27.0;
	_halfTimesNitrogen[5] = 38.3;
	_halfTimesNitrogen[6] = 54.3;
	_halfTimesNitrogen[7] = 77.0;
	_halfTimesNitrogen[8] = 109.0;
	_halfTimesNitrogen[9] = 146.0;
	_halfTimesNitrogen[10] = 187.0;
	_halfTimesNitrogen[11] = 239.0;
	_halfTimesNitrogen[12] = 305.0;
	_halfTimesNitrogen[13] = 390.0;
	_halfTimesNitrogen[14] = 498.0;
	_halfTimesNitrogen[15] = 635.0;

	// M 值系数 A（bar）
	_aValuesNitrogen[0] = 1.1696;
	_aValuesNitrogen[1] = 1.0000;
	_aValuesNitrogen[2] = 0.8618;
	_aValuesNitrogen[3] = 0.7562;
	_aValuesNitrogen[4] = 0.6200;
	_aValuesNitrogen[5] = 0.5043;
	_aValuesNitrogen[6] = 0.4410;
	_aValuesNitrogen[7] = 0.4000;
	_aValuesNitrogen[8] = 0.3750;
	_aValuesNitrogen[9] = 0.3500;
	_aValuesNitrogen[10] = 0.3295;
	_aValuesNitrogen[11] = 0.3065;
	_aValuesNitrogen[12] = 0.2835;
	_aValuesNitrogen[13] = 0.2610;
	_aValuesNitrogen[14] = 0.2480;
	_aValuesNitrogen[15] = 0.2327;

	// M 值系数 B（无量纲）
	_bValuesNitrogen[0] = 0.5578;
	_bValuesNitrogen[1] = 0.6514;
	_bValuesNitrogen[2] = 0.7222;
	_bValuesNitrogen[3] = 0.7826;  // 与Java对齐（标准ZHL-16C值）
	_bValuesNitrogen[4] = 0.8125;
	_bValuesNitrogen[5] = 0.8434;
	_bValuesNitrogen[6] = 0.8693;
	_bValuesNitrogen[7] = 0.8910;
	_bValuesNitrogen[8] = 0.9092;
	_bValuesNitrogen[9] = 0.9222;
	_bValuesNitrogen[10] = 0.9319;
	_bValuesNitrogen[11] = 0.9403;
	_bValuesNitrogen[12] = 0.9477;
	_bValuesNitrogen[13] = 0.9544;
	_bValuesNitrogen[14] = 0.9602;
	_bValuesNitrogen[15] = 0.9653;

	// ========== 氦气参数表（ZHL-16C）==========
	// He 半衰期（分钟）- 约为 N2 的 1/2.65
	_halfTimesHelium[0] = 1.88;
	_halfTimesHelium[1] = 3.02;
	_halfTimesHelium[2] = 4.72;
	_halfTimesHelium[3] = 6.99;
	_halfTimesHelium[4] = 10.21;
	_halfTimesHelium[5] = 14.48;
	_halfTimesHelium[6] = 20.53;
	_halfTimesHelium[7] = 29.11;
	_halfTimesHelium[8] = 41.20;
	_halfTimesHelium[9] = 55.19;
	_halfTimesHelium[10] = 70.69;
	_halfTimesHelium[11] = 90.34;
	_halfTimesHelium[12] = 115.29;
	_halfTimesHelium[13] = 147.42;
	_halfTimesHelium[14] = 188.24;
	_halfTimesHelium[15] = 240.03;

	// He 的 a 系数（bar）
	_aValuesHelium[0] = 1.6189;
	_aValuesHelium[1] = 1.3830;
	_aValuesHelium[2] = 1.1919;
	_aValuesHelium[3] = 1.0458;
	_aValuesHelium[4] = 0.9220;
	_aValuesHelium[5] = 0.8205;
	_aValuesHelium[6] = 0.7305;
	_aValuesHelium[7] = 0.6502;
	_aValuesHelium[8] = 0.5950;
	_aValuesHelium[9] = 0.5545;
	_aValuesHelium[10] = 0.5333;
	_aValuesHelium[11] = 0.5189;
	_aValuesHelium[12] = 0.5181;
	_aValuesHelium[13] = 0.5176;
	_aValuesHelium[14] = 0.5172;
	_aValuesHelium[15] = 0.5119;

	// He 的 b 系数（无量纲）
	_bValuesHelium[0] = 0.4770;
	_bValuesHelium[1] = 0.5747;
	_bValuesHelium[2] = 0.6527;
	_bValuesHelium[3] = 0.7223;
	_bValuesHelium[4] = 0.7582;
	_bValuesHelium[5] = 0.7957;
	_bValuesHelium[6] = 0.8279;
	_bValuesHelium[7] = 0.8553;
	_bValuesHelium[8] = 0.8757;
	_bValuesHelium[9] = 0.8903;
	_bValuesHelium[10] = 0.8997;
	_bValuesHelium[11] = 0.9073;
	_bValuesHelium[12] = 0.9122;
	_bValuesHelium[13] = 0.9171;
	_bValuesHelium[14] = 0.9217;
	_bValuesHelium[15] = 0.9267;

	// 初始化 He 载荷为 0（水面呼吸空气无氦气）
	for (int i = 0; i < COMPARTMENT_COUNT; i++) {
		_compartmentHePartialPressures[i] = 0.0f;
	}

	_currentDiveDuration = 0;
	_currentDepth = 0;
	_lastPressure = 1000.0f;  // 初始化为海平面气压
	_maxDepth = 0;
	_cumulativeCNS = 0.0f;
	_cumulativeOTU = 0.0f;
	
	// 梯度因子默认值（Deep-Stops 理论）
	_gfLow = 0.40f;   // GF Low = 40%（生成深停，保守策略）
	_gfHigh = 0.85f;  // GF High = 85%（出水时保留25%安全边际）
	_gfLowDepthPressure = _seaLevelAtmosphericPressure;  // 与Java对齐：初始为首停=水面气压（0米），后续由getDeepestStop动态更新为最深ceiling
	_finalStopDepthMeters = DECO_DEFAULT_FINAL_STOP_METERS;
	
	// GF99 颜色/阈值追踪（0=normal,1=yellow,2=red）
	_safetyStopEnabled = true;
	_safetyStopDepthMeters = 3.0f;
	_safetyStopDurationSeconds = 180;
	_safetyStopArmDepthMeters = 10.0f;
	_safetyStopZoneHalfWidthMeters = 1.5f;
	_safetyStopSurfaceResetDepthMeters = 0.8f;
	_safetyStopArmed = false;
	_safetyStopCompleted = false;
	_safetyStopElapsedSeconds = 0;

	_lastGF99WarningLevel = 0;
	_lastAllowedHighPressureMbar = _seaLevelAtmosphericPressure;
	_lastAllowedOrigPressureMbar = _seaLevelAtmosphericPressure;
	
	// 诊断/调参默认值
	_fastCompartmentMaxIndex = 3;        // 默认使用 0..3 作为快组织
	
	// 单位设置默认值
	_isUnitMetric = true;                // 默认使用米
}

/////////////////////
// Utility methods //
/////////////////////

/**
 * @brief 计算深度（从压力）
 * @param pressure 环境压力（mbar）
 * @return 深度（米）
 * @details 公式：depth = (P - P_surface) / (1000 / depthPerBar)
 *          海水: 1000/10.06 ≈ 99.4 mbar/m
 *          淡水: 1000/10.30 ≈ 97.1 mbar/m
 */
float Buhlmann::calculateDepthFromPressure(float pressure) {
	float depthInMeter = 0;
	if (pressure > _seaLevelAtmosphericPressure) {
		// mbar/m = 1000 / depthPerBar
		float mbarPerMeter = 1000.0f / _depthPerBar;
		depthInMeter = (pressure - _seaLevelAtmosphericPressure) / mbarPerMeter;
	}
	return depthInMeter;
}

/**
 * @brief 计算压力（从深度）
 * @param depth 深度（米）
 * @return 环境压力（mbar）
 * @details 公式：P = P_surface + depth × (1000 / depthPerBar)
 */
float Buhlmann::calculateHydrostaticPressureFromDepth(float depth) {
	// mbar/m = 1000 / depthPerBar
	float mbarPerMeter = 1000.0f / _depthPerBar;
	return _seaLevelAtmosphericPressure + (mbarPerMeter * depth);
}

// 查找浮点数组中的最大值
float Buhlmann::maxSearch(float array[], int size) {
	float max = array[0];
	for (int i=1; i < size; i++) {
		if (max < array[i]) {
			max = array[i];
		}
	}
	return max;
}

// 查找整数数组中的最小值
int Buhlmann::minSearch(int array[], int size) {
	int min = array[0];
	for (int i=1; i<size; i++) {
		if (min > array[i]) {
			min = array[i];
		}
	}
	return min;
}

/////////////////////////
// Getters and setters //
/////////////////////////

// 获取指定组织舱的半衰期（分钟转秒）
float Buhlmann::getCompartmentHalfTimeInSeconds(int compartmentIndex) {
	return _halfTimesNitrogen[compartmentIndex] * 60;
}

// 获取指定组织舱的 He 半衰期（秒）
float Buhlmann::getCompartmentHeHalfTimeInSeconds(int compartmentIndex) {
	return _halfTimesHelium[compartmentIndex] * 60;
}

// 获取指定组织舱的当前氮气分压（mbar）
float Buhlmann::getCompartmentPartialPressure(int compartmentIndex) {
	return _compartmentCurrentPartialPressures[compartmentIndex];
}

// 获取指定组织舱的当前氦气分压（mbar）
float Buhlmann::getCompartmentHePartialPressure(int compartmentIndex) {
	return _compartmentHePartialPressures[compartmentIndex];
}

// 设置指定组织舱的氮气分压（mbar）
void Buhlmann::setCompartmentPartialPressure(int compartmentIndex, float partialPressure) {
	_compartmentCurrentPartialPressures[compartmentIndex] = partialPressure;
}

// 设置指定组织舱的氦气分压（mbar）
void Buhlmann::setCompartmentHePartialPressure(int compartmentIndex, float partialPressure) {
	_compartmentHePartialPressures[compartmentIndex] = partialPressure;
}

// 获取指定组织舱的总惰性气体载荷（N2 + He，mbar）
float Buhlmann::getCompartmentTotalInertLoad(int compartmentIndex) {
	return _compartmentCurrentPartialPressures[compartmentIndex] + _compartmentHePartialPressures[compartmentIndex];
}

// 获取组合 a 系数（按 N2/He 载荷比例加权）
float Buhlmann::getCompartmentCombinedA(int compartmentIndex) {
	float n2Load = _compartmentCurrentPartialPressures[compartmentIndex];
	float heLoad = _compartmentHePartialPressures[compartmentIndex];
	float totalLoad = n2Load + heLoad;
	
	if (totalLoad < 0.001f) {
		return _aValuesNitrogen[compartmentIndex];  // 避免除零
	}
	
	return (_aValuesNitrogen[compartmentIndex] * n2Load + _aValuesHelium[compartmentIndex] * heLoad) / totalLoad;
}

// 获取组合 b 系数（按 N2/He 载荷比例加权）
float Buhlmann::getCompartmentCombinedB(int compartmentIndex) {
	float n2Load = _compartmentCurrentPartialPressures[compartmentIndex];
	float heLoad = _compartmentHePartialPressures[compartmentIndex];
	float totalLoad = n2Load + heLoad;
	
	if (totalLoad < 0.001f) {
		return _bValuesNitrogen[compartmentIndex];  // 避免除零
	}
	
	return (_bValuesNitrogen[compartmentIndex] * n2Load + _bValuesHelium[compartmentIndex] * heLoad) / totalLoad;
}

// 获取当前舱氦气分压数组
void Buhlmann::getCurrentCompartmentHePressures(float hePressures[COMPARTMENT_COUNT]) {
	for (int i = 0; i < COMPARTMENT_COUNT; i++) {
		hePressures[i] = _compartmentHePartialPressures[i];
	}
}

// 获取单个舱 N2 分压（mbar）- public 接口
float Buhlmann::getCompartmentN2Pressure(int compartmentIndex) {
	if (compartmentIndex < 0 || compartmentIndex >= COMPARTMENT_COUNT) return 0.0f;
	return _compartmentCurrentPartialPressures[compartmentIndex];
}

// 获取单个舱 He 分压（mbar）- public 接口
float Buhlmann::getCompartmentHePressure(int compartmentIndex) {
	if (compartmentIndex < 0 || compartmentIndex >= COMPARTMENT_COUNT) return 0.0f;
	return _compartmentHePartialPressures[compartmentIndex];
}

// 获取最近一次计算的上升天花板压力（mbar）
float Buhlmann::getLastCeilingPressure() {
	return _lastCeilingPressureMbar;
}

 // 获取最近一次计算中限制上升的组织舱索引（0-15，-1表示无效）
int Buhlmann::getLastCeilingCompartmentIndex() {
	return _lastCeilingCompartmentIndex;
}

// ========== 诊断与调参实现 ==========
void Buhlmann::printFirstStopDiagnostics(float gfLow) {
#ifdef RT_USING_SERIAL
    rt_kprintf("[DIAG] FirstStop per-compartment (total load mbar, allowedP mbar, depth m):\n");
    for (int i = 0; i < COMPARTMENT_COUNT; ++i) {
        float totalLoad = getCompartmentTotalInertLoad(i); // N2 + He (mbar)
        float allowedP = getAscendToPartialPressureForCompartmentWithGF(i, totalLoad, gfLow); // mbar
        float depth = calculateDepthFromPressure(allowedP);
        rt_kprintf("[C%02d] N2=%.2f He=%.2f Total=%.2f mbar allowedP=%.1f mbar depth=%.2fm a=%.4f b=%.4f\n",
                      i, getCompartmentPartialPressure(i), getCompartmentHePartialPressure(i),
                      totalLoad, allowedP, depth, 
                      getCompartmentCombinedA(i), getCompartmentCombinedB(i));
    }
#endif
}

// 新增诊断：打印 CEIL 与 首停对比，并显示每个舱在 GF High / GF Low 下的允许深度
void Buhlmann::printCeilVsFirstStopDebug() {
#ifdef RT_USING_SERIAL
    float ceilingPressure = _lastCeilingPressureMbar;
    float ceilingDepth = calculateDepthFromPressure(ceilingPressure);
    rt_kprintf("[CEIL/STOP] CEIL: %.2fm (mbar:%.1f)  FIRSTSTOP: %.2fm  GF Low:%.2f GF High:%.2f\n",
                  ceilingDepth, ceilingPressure, _firstStopDepth, _gfLow, _gfHigh);

    rt_kprintf("[Per-compartment depths at GF High / GF Low]:\n");
    for (int i = 0; i < COMPARTMENT_COUNT; ++i) {
        float totalLoad = getCompartmentTotalInertLoad(i);
        float pHigh = getAscendToPartialPressureForCompartmentWithGF(i, totalLoad, _gfHigh);
        float pLow  = getAscendToPartialPressureForCompartmentWithGF(i, totalLoad, _gfLow);
        float dHigh = calculateDepthFromPressure(pHigh);
        float dLow  = calculateDepthFromPressure(pLow);
        rt_kprintf("C%02d: N2=%.2f He=%.2f Total=%.2f mbar | depth@High=%.2fm depth@Low=%.2fm a=%.4f b=%.4f\n",
                      i, getCompartmentPartialPressure(i), getCompartmentHePartialPressure(i),
                      totalLoad, dHigh, dLow, 
                      getCompartmentCombinedA(i), getCompartmentCombinedB(i));
    }
#endif
}

void Buhlmann::setFastCompartmentMaxIndex(int maxIndex) {
    if (maxIndex < 0) maxIndex = 0;
    if (maxIndex >= COMPARTMENT_COUNT) maxIndex = COMPARTMENT_COUNT - 1;
    _fastCompartmentMaxIndex = maxIndex;
}

int Buhlmann::getFastCompartmentMaxIndex() {
    return _fastCompartmentMaxIndex;
}

// 设置海平面大气压力（mbar，默认1013.2）
void Buhlmann::setSeaLevelAtmosphericPressure(float seaLevelAtmosphericPressure) {
	_seaLevelAtmosphericPressure = seaLevelAtmosphericPressure;
}

// 设置混合气体中的氮气比例（默认0.78，即78%）
void Buhlmann::setNitrogenRateInGas(float nitrogenRateInGas) {
	_nitrogenRateInGas = nitrogenRateInGas;
}

// 设置混合气体中的氧气比例，自动计算氮气比例
void Buhlmann::setOxygenRateInGas(float oxygenRateInGas) {
	if (oxygenRateInGas < 0.21f) oxygenRateInGas = 0.21f;  // 最低 21%（空气）
	if (oxygenRateInGas > 1.0f) oxygenRateInGas = 1.0f;    // 最高 100%（纯氧）
	_oxygenRateInGas = oxygenRateInGas;
	_nitrogenRateInGas = 1.0f - oxygenRateInGas;  // 氮气 = 1 - 氧气（忽略其他气体）
}

// 获取当前氧气比例
float Buhlmann::getOxygenRateInGas() {
	return _oxygenRateInGas;
}

// 获取当前氮气比例
float Buhlmann::getNitrogenRateInGas() {
	return _nitrogenRateInGas;
}

// 计算最大操作深度 MOD（Maximum Operating Depth）
// maxPPO2: 最大允许氧分压，默认 1.4 bar（休闲潜水），技术潜水可用 1.6 bar
float Buhlmann::calculateMOD(float maxPPO2) {
	// MOD = (maxPPO2 / O2%) - 1 bar，转换为深度
	// 公式：depth = (maxPPO2 / O2% - surfacePressure) * depthPerBar
	float modPressureBar = maxPPO2 / _oxygenRateInGas;
	float modDepth = (modPressureBar - _seaLevelAtmosphericPressure / 1000.0f) * _depthPerBar;
	if (modDepth < 0) modDepth = 0;
	return modDepth;
}

// ========== 环境设置接口实现 ==========

/**
 * @brief 设置海拔高度，自动计算对应的大气压
 * @param altitudeMeters 海拔高度（米）
 * @details 使用气压高度公式：P = P0 × (1 - altitude/44330)^5.255
 * * 【注意】为了与 Java 模拟器对齐，此处 P0 采用理想值 1000.0 mbar (1.0 bar)
 * 而不是真实物理学标准海平面气压 1013.25 mbar。
 */
void Buhlmann::setAltitude(float altitudeMeters) {
  if (altitudeMeters < 0) altitudeMeters = 0;
  if (altitudeMeters > 5000) altitudeMeters = 5000;  // 限制最大海拔 5000m
  
  _altitudeMeters = altitudeMeters;
  
  // 气压高度公式：P = P0 × (1 - h/44330)^5.255
  // 【修改点】：P0 = 1000.0 mbar（与 Java 的 1.0 bar 保持完全一致）
  float pressureRatio = pow(1.0f - altitudeMeters / 44330.0f, 5.255f);
  _seaLevelAtmosphericPressure = 1000.0f * pressureRatio;
  
  // 更新所有气体的 MOD（因为大气压变了）
  for (int i = 0; i < MAX_GASES; i++) {
    if (_gases[i].enabled) {
      float ppo2Limit = (i == 0) ? _bottomPPO2 : _decoPPO2;
      float modPressureBar = ppo2Limit / _gases[i].oxygenFraction;
      _gases[i].modDepth = (modPressureBar - _seaLevelAtmosphericPressure / 1000.0f) * _depthPerBar;
      if (_gases[i].modDepth < 0) _gases[i].modDepth = 0;
    }
  }
}

float Buhlmann::getAltitude() {
	return _altitudeMeters;
}

/**
 * @brief 设置水体类型（海水/淡水）
 * @param type 水体类型枚举
 * @details 影响深度-压力换算系数
 * - 海水密度 ≈ 1.025 g/cm³，10.06 m/bar
 * - 淡水密度 ≈ 1.000 g/cm³，10.30 m/bar
 */
void Buhlmann::setWaterType(WaterType type) {
	_waterType = type;
	
	if (type == WATER_FRESH) {
		_depthPerBar = 10.30f;  // 淡水
	} else {
		_depthPerBar = 10.0f;  // 海水（默认）
	}
	
	// 更新所有气体的 MOD（因为深度换算系数变了）
	for (int i = 0; i < MAX_GASES; i++) {
		if (_gases[i].enabled) {
			float ppo2Limit = (i == 0) ? _bottomPPO2 : _decoPPO2;
			float modPressureBar = ppo2Limit / _gases[i].oxygenFraction;
			_gases[i].modDepth = (modPressureBar - _seaLevelAtmosphericPressure / 1000.0f) * _depthPerBar;
			if (_gases[i].modDepth < 0) _gases[i].modDepth = 0;
		}
	}
}

WaterType Buhlmann::getWaterType() {
	return _waterType;
}

float Buhlmann::getSurfacePressure() {
	return _seaLevelAtmosphericPressure;
}

float Buhlmann::getDepthPerBar() {
	return _depthPerBar;
}

// ========== 多气体支持接口实现 ==========

/**
 * @brief 设置指定槽位的气体配置（支持 Trimix）
 * @param gasIndex 气体槽位（0-2）
 * @param oxygenFraction 氧气比例（0.21-1.0）
 * @param heliumFraction 氦气比例（0-0.8），0=Nitrox，>0=Trimix
 * @param enabled 是否启用
 * @param modPPO2 用于计算 MOD 的 PPO2 限制
 */
void Buhlmann::setGas(int gasIndex, float oxygenFraction, float heliumFraction, bool enabled, float modPPO2) {
	if (gasIndex < 0 || gasIndex >= MAX_GASES) return;
	
	// 限制氧气比例范围
	if (oxygenFraction < 0.21f) oxygenFraction = 0.21f;
	if (oxygenFraction > 1.0f) oxygenFraction = 1.0f;
	
	// 限制氦气比例范围
	if (heliumFraction < 0.0f) heliumFraction = 0.0f;
	if (heliumFraction > 0.8f) heliumFraction = 0.8f;
	
	// 确保 O2 + He <= 1.0
	if (oxygenFraction + heliumFraction > 1.0f) {
		heliumFraction = 1.0f - oxygenFraction;
	}
	
	_gases[gasIndex].oxygenFraction = oxygenFraction;
	_gases[gasIndex].heliumFraction = heliumFraction;
	_gases[gasIndex].nitrogenFraction = 1.0f - oxygenFraction - heliumFraction;
	_gases[gasIndex].enabled = enabled;
	
	// 计算 MOD
	float modPressureBar = modPPO2 / oxygenFraction;
	_gases[gasIndex].modDepth = (modPressureBar - _seaLevelAtmosphericPressure / 1000.0f) * _depthPerBar;
	if (_gases[gasIndex].modDepth < 0) _gases[gasIndex].modDepth = 0;
	
	// 如果是当前使用的气体，同步更新全局气体比例
	if (gasIndex == _activeGasIndex) {
		_oxygenRateInGas = oxygenFraction;
		_nitrogenRateInGas = _gases[gasIndex].nitrogenFraction;
	}
}

Gas Buhlmann::getGas(int gasIndex) {
	if (gasIndex < 0 || gasIndex >= MAX_GASES) {
		return Gas();  // 返回默认空气
	}
	return _gases[gasIndex];
}

/**
 * @brief 设置当前使用的气体槽位
 * @param gasIndex 气体槽位（0-2）
 * @details 切换气体时自动更新全局氧/氮比例
 */
void Buhlmann::setActiveGas(int gasIndex) {
	if (gasIndex < 0 || gasIndex >= MAX_GASES) return;
	if (!_gases[gasIndex].enabled) return;  // 不能切换到未启用的气体
	
	_activeGasIndex = gasIndex;
	_oxygenRateInGas = _gases[gasIndex].oxygenFraction;
	_nitrogenRateInGas = _gases[gasIndex].nitrogenFraction;
}

int Buhlmann::getActiveGas() {
	return _activeGasIndex;
}

/**
 * @brief 根据深度获取最佳气体槽位
 * @param depthMeters 当前深度（米）
 * @return 最佳气体槽位（0-2），-1 表示没有合适的气体
 * @details 选择逻辑：
 * 1. 气体必须启用
 * 2. 当前深度 <= MOD（不超过最大操作深度）
 * 3. 当前深度 >= minDepth（不低于最小使用深度）
 * 4. 在满足条件的气体中，选择氧气比例最高的（更快排氮）
 */
int Buhlmann::getBestGasForDepth(float depthMeters) {
	int bestGasIndex = -1;
	float bestO2Fraction = 0.0f;
	
	for (int i = 0; i < MAX_GASES; i++) {
		if (!_gases[i].enabled) continue;
		
		// 检查深度范围
		if (depthMeters > _gases[i].modDepth) continue;  // 超过 MOD
		if (depthMeters < _gases[i].minDepth) continue;  // 低于最小深度
		
		// 选择氧气比例最高的
		if (_gases[i].oxygenFraction > bestO2Fraction) {
			bestO2Fraction = _gases[i].oxygenFraction;
			bestGasIndex = i;
		}
	}
	
	return bestGasIndex;
}

/**
 * @brief 检查是否有更好的气体可用
 * @param depthMeters 当前深度（米）
 * @return true 如果有比当前气体更好的选择
 */
bool Buhlmann::hasBetterGasAvailable(float depthMeters) {
	int bestGas = getBestGasForDepth(depthMeters);
	if (bestGas < 0) return false;
	
	// 如果最佳气体不是当前气体，且氧气比例更高，则有更好选择
	return (bestGas != _activeGasIndex && 
	        _gases[bestGas].oxygenFraction > _gases[_activeGasIndex].oxygenFraction);
}

bool Buhlmann::checkICDRisk(int targetGasIndex, float currentDepthMeters) {
	if (targetGasIndex < 0 || targetGasIndex >= MAX_GASES) return false;
	if (_activeGasIndex == targetGasIndex) return false;

	Gas currentGas = _gases[_activeGasIndex];
	Gas targetGas = _gases[targetGasIndex];
	if (targetGas.heliumFraction >= currentGas.heliumFraction) {
		return false;
	}

	float ambientPressureBar = calculateHydrostaticPressureFromDepth(currentDepthMeters) / 1000.0f;
	float deltaN2 = (targetGas.nitrogenFraction - currentGas.nitrogenFraction) * ambientPressureBar;
	return deltaN2 > 0.4f;
}

// ========== PPO2 限制设置实现 ==========

void Buhlmann::setBottomPPO2(float ppo2) {
	if (ppo2 < 1.0f) ppo2 = 1.0f;
	if (ppo2 > 1.6f) ppo2 = 1.6f;
	_bottomPPO2 = ppo2;
	
	// 更新底部气体（槽位0）的 MOD
	if (_gases[0].enabled) {
		float modPressureBar = ppo2 / _gases[0].oxygenFraction;
		_gases[0].modDepth = (modPressureBar - _seaLevelAtmosphericPressure / 1000.0f) * _depthPerBar;
		if (_gases[0].modDepth < 0) _gases[0].modDepth = 0;
	}
}

float Buhlmann::getBottomPPO2() {
	return _bottomPPO2;
}

void Buhlmann::setDecoPPO2(float ppo2) {
	if (ppo2 < 1.0f) ppo2 = 1.0f;
	if (ppo2 > 1.6f) ppo2 = 1.6f;
	_decoPPO2 = ppo2;
	
	// 更新减压气体（槽位1、2）的 MOD
	for (int i = 1; i < MAX_GASES; i++) {
		if (_gases[i].enabled) {
			float modPressureBar = ppo2 / _gases[i].oxygenFraction;
			_gases[i].modDepth = (modPressureBar - _seaLevelAtmosphericPressure / 1000.0f) * _depthPerBar;
			if (_gases[i].modDepth < 0) _gases[i].modDepth = 0;
		}
	}
}

float Buhlmann::getDecoPPO2() {
	return _decoPPO2;
}

void Buhlmann::setSafetyStopConfig(float depthMeters, int durationSeconds) {
	if (depthMeters < 3.0f) {
		depthMeters = 3.0f;
	}
	if (depthMeters > 6.0f) {
		depthMeters = 6.0f;
	}
	if (durationSeconds < 0) {
		durationSeconds = 0;
	}
	if (durationSeconds > 600) {
		durationSeconds = 600;
	}
	_safetyStopDepthMeters = depthMeters;
	_safetyStopDurationSeconds = durationSeconds;
	_safetyStopEnabled = (durationSeconds > 0);
	resetSafetyStopState(false);
}

float Buhlmann::getSafetyStopDepth() {
	return _safetyStopDepthMeters;
}

int Buhlmann::getSafetyStopDurationSeconds() {
	return _safetyStopDurationSeconds;
}

// ========== GF Low/GF High 设置/获取方法 ==========

/**
 * @brief 设置首停梯度因子（GF Low）
 * @param gfLow 梯度因子（0.0-1.0），推荐值 0.2-0.4
 * 
 * GF Low 控制首停深度：
 * - 0.2 (20%): 保守策略，生成较深的首停（Deep Stop）
 * - 0.3 (30%): 标准策略
 * - 0.4 (40%): 激进策略，首停较浅
 */
void Buhlmann::setGFLow(float gfLow) {
	if (gfLow >= 0.0f && gfLow <= 1.0f) {
		_gfLow = gfLow;
	}
}

/**
 * @brief 设置出水梯度因子（GF High）
 * @param gfHigh 梯度因子（0.0-1.0），推荐值 0.75-0.95
 * 
 * GF High 控制出水时的安全边际：
 * - 0.75 (75%): 保守策略，保留25%安全边际
 * - 0.85 (85%): 标准策略
 * - 0.95 (95%): 激进策略，接近M值极限
 */
void Buhlmann::setGFHigh(float gfHigh) {
	if (gfHigh >= 0.0f && gfHigh <= 1.0f) {
		_gfHigh = gfHigh;
	}
}

// 获取当前 GF Low 值（首停梯度因子，0.0-1.0）
float Buhlmann::getGFLow() {
	return _gfLow;
}

// 获取当前 GF High 值（出水梯度因子，0.0-1.0）
float Buhlmann::getGFHigh() {
	return _gfHigh;
}

// 获取海平面大气压力（mbar）
float Buhlmann::getSeaLevelAtmosphericPressure() {
    return _seaLevelAtmosphericPressure;
}

void Buhlmann::setFinalStopDepth(float depthMeters) {
	if (fabs(depthMeters - 3.0f) < 0.01f ||
	    fabs(depthMeters - 4.5f) < 0.01f ||
	    fabs(depthMeters - 6.0f) < 0.01f ||
	    fabs(depthMeters - 9.0f) < 0.01f) {
		_finalStopDepthMeters = depthMeters;
	}
}

float Buhlmann::getFinalStopDepth() {
	return _finalStopDepthMeters;
}

/////////////////////////////////
// Calculation related methods //
/////////////////////////////////

//计算 "肺部氮气分压（mbar）  公式 = 氮气比例 ×(当前压力 - 水蒸气压力修正)
float Buhlmann::calculateNitrogenPartialPressureInLung(float currentPressure) {
	// 返回 mbar 单位（Buhlmann 算法内部使用 mbar）
	// 使用当前活动气体的氮气比例
	return _gases[_activeGasIndex].nitrogenFraction * (currentPressure - _waterVapourPressureCorrection);
}

//计算 "氧分压（PPO2，bar）
float Buhlmann::calculateOxygenPartialPressure(float currentPressure) {
	// 使用当前活动气体的氧气比例
	float oxygenRateInGas = _gases[_activeGasIndex].oxygenFraction;
	
	// 计算氧分压（bar）：氧气百分比 × (当前压力 - 水蒸气压力) / 1000
	return oxygenRateInGas * (currentPressure - _waterVapourPressureCorrection) / 1000.0f;
}

//计算 "肺部氦气分压（mbar）  公式 = 氦气比例 ×(当前压力 - 水蒸气压力修正)
float Buhlmann::calculateHeliumPartialPressureInLung(float currentPressure) {
	// 返回 mbar 单位
	return _gases[_activeGasIndex].heliumFraction * (currentPressure - _waterVapourPressureCorrection);
}

// 计算指定环境压力下的 GF 值（可指定单个组织舱或所有舱的最大值）
// useGFCorrection: true=使用GF High修正后的M值（与NDL同步），false=标准M值（用于GF99/SurfGF）
float Buhlmann::calculateGFAtAmbientPressure(float ambientPressureMbar, int compartmentIndex, bool useGFCorrection)
{
	float ambientPressureBar = ambientPressureMbar / 1000.0f;

	// 与 calculateNitrogenPartialPressureInLung 对齐的环境氮分压（去除水蒸气压修正）
	float ambientNitrogenPressure = _nitrogenRateInGas * (ambientPressureBar - _waterVapourPressureCorrection / 1000.0f);

	// 检查组织室是否初始化
	bool isInitialized = false;
	for (int i = 0; i < COMPARTMENT_COUNT; i++)
	{
		if (_compartmentCurrentPartialPressures[i] > 100.0f)  // 100 mbar = 0.1 bar
		{
			isInitialized = true;
			break;
		}
	}
	if (!isInitialized)
	{
		return 0.0f;
	}

	float maxGF = 0.0f;
	int startIdx = 0;
	int endIdx = COMPARTMENT_COUNT;

	// 如果指定了舱索引，仅计算该舱
	if (compartmentIndex >= 0 && compartmentIndex < COMPARTMENT_COUNT)
	{
		startIdx = compartmentIndex;
		endIdx = compartmentIndex + 1;
	}

	for (int i = startIdx; i < endIdx; i++)
	{
		float tissuePressure = _compartmentCurrentPartialPressures[i] / 1000.0f;  // mbar -> bar
		if (tissuePressure > 0.00001f)
		{
			float mValuePressure;
			
			if (useGFCorrection)
			{
				// 使用 GF High 修正后的 M 值（与 NDL 计算保持一致）
				// 标准 M 值：M_standard = a + P_amb / b
				// GF High 修正后的允许组织分压上限：M_GF = P_amb × (GF/b - GF + 1) + GF × a
				float a = _aValuesNitrogen[i];
				float b = _bValuesNitrogen[i];
				float mValueStandard = a + (ambientPressureBar / b);
				float mValueGF = ambientPressureBar * (_gfHigh / b - _gfHigh + 1.0f) + _gfHigh * a;
				
				// GF@ceiling 计算：当前组织分压相对于标准 M 值的比例，但以 GF High 作为上限
				// 当组织分压达到修正后 M 值（M_GF）时，NDL=0
				// 此时应该让 GF@ceiling = GF_High
				// 方法：先计算占标准 M 值的比例，然后按 GF High 缩放
				float numerator = tissuePressure - ambientNitrogenPressure;
				float denominator = mValueStandard - ambientNitrogenPressure;

				if (denominator > 0.001f)
				{
					// 计算占标准 M 值的比例（0.0-1.0）
					float ratioToStandard = numerator / denominator;
					
					// 计算占修正后 M 值的比例（0.0-1.0）
					float denominatorGF = mValueGF - ambientNitrogenPressure;
					if (denominatorGF > 0.001f)
					{
						float ratioToGF = numerator / denominatorGF;
						
						// GF@ceiling = 占修正后 M 值的比例 × GF_High × 100%
						// 这样当组织分压 = M_GF 时，GF@ceiling = GF_High
						float gf = ratioToGF * _gfHigh * 100.0f;
						if (gf > maxGF)
						{
							maxGF = gf;
						}
					}
				}
			}
			else
			{
				// 标准 M 值（用于 GF99/SurfGF）
				mValuePressure = _aValuesNitrogen[i] + (ambientPressureBar / _bValuesNitrogen[i]);
				
				float numerator = tissuePressure - ambientNitrogenPressure;
				float denominator = mValuePressure - ambientNitrogenPressure;

				if (denominator > 0.001f)
				{
					float gf = (numerator / denominator) * 100.0f;
					if (gf > maxGF)
					{
						maxGF = gf;
					}
				}
			}
		}
	}

	if (maxGF < 0.0f) maxGF = 0.0f;
	if (maxGF > 999.0f) maxGF = 999.0f;

	return maxGF;
}
 
/*计算每个组织舱的 "当前氮气分压  组织舱气体吸收 / 排出 新分压 
|= 旧分压 + (肺部分压 - 旧分压) × (1 - 2^(-时间/半衰期))
|P_new = P_old + (P_lung - P_old) × (1 - 2^(-t/halfTime))
*/
float Buhlmann::calculateCompartmentInertGasPartialPressure(long timeInSeconds, float halfTimeInSeconds, float currentInertGasPartialPressureInCompartment, float currentInertGasPartialPressureInLung) {
	return currentInertGasPartialPressureInCompartment + (currentInertGasPartialPressureInLung - currentInertGasPartialPressureInCompartment) * (1 - pow(2, -timeInSeconds/halfTimeInSeconds));
}

/**
 * @brief Schreiner 公式：计算组织分压（支持压力变化）
 * @param compartmentIndex 组织舱索引 (0-15)
 * @param pAlvStart 起始肺部氮分压 (mbar)
 * @param R 肺部氮分压变化率 (mbar/秒)
 * @param timeInSeconds 时间 (秒)
 * @return 新的组织氮分压 (mbar)
 * @details Schreiner 方程用于处理压力变化情况（上升/下降）
 */
float Buhlmann::calculateCompartmentPressureSchreiner(int compartmentIndex, float pAlvStart, float R, float timeInSeconds) {
	if (compartmentIndex < 0 || compartmentIndex >= COMPARTMENT_COUNT) {
		return 0.0f;
	}
	
	// 预计算的 k 值（1/秒）
	static const double kValues[16] = {
		0.00231049060186648,  // compartment 0
		0.00144405662616655,  // compartment 1
		0.00092419624074659,  // compartment 2
		0.00062445691942338,  // compartment 3
		0.00042786862997528,  // compartment 4
		0.00030163062687552,  // compartment 5
		0.00021275235744627,  // compartment 6
		0.00015003185726406,  // compartment 7
		0.00010598580742507,  // compartment 8
		0.00007912639047488,  // compartment 9
		0.00006177782357932,  // compartment 10
		0.00004833662347001,  // compartment 11
		0.00003787689511257,  // compartment 12
		0.00002962167438290,  // compartment 13
		0.00002319769680589,  // compartment 14
		0.00001819283938478   // compartment 15
	};
	double k = kValues[compartmentIndex];
	
	double pTissue0 = (double)getCompartmentPartialPressure(compartmentIndex);
	double pAlv = (double)pAlvStart;
	double Rd = (double)R;
	double t = (double)timeInSeconds;
	
	// Schreiner 公式：
	// P_new = pAlv + R×(time - 1/k) - (pAlv - pTissue0 - R/k) × exp(-k×time)
	double pNew = pAlv + Rd * (t - 1.0/k) 
	            - (pAlv - pTissue0 - Rd/k) * exp(-k * t);
	
	return (float)pNew;
}

/**
 * @brief Schreiner 公式（He 版本）：计算组织氦气分压
 * @param compartmentIndex 组织舱索引 (0-15)
 * @param pAlvHeStart 起始肺部氦分压 (mbar)
 * @param R_He 肺部氦分压变化率 (mbar/秒)
 * @param timeInSeconds 时间 (秒)
 * @return 新的组织氦分压 (mbar)
 */
float Buhlmann::calculateCompartmentHePressureSchreiner(int compartmentIndex, float pAlvHeStart, float R_He, float timeInSeconds) {
	if (compartmentIndex < 0 || compartmentIndex >= COMPARTMENT_COUNT) {
		return 0.0f;
	}
	
	// He 的 k 值（1/秒）- 使用Java TissueModel中的精确常数
	// 与Java对齐：k = ln(2) / (halfTimeMinutes * 60)
	static const double heKValues[16] = {
		0.00614598235,   // 1.88 min
		0.00382663477,   // 3.02 min
		0.00244882028,   // 4.72 min
		0.00165387312,   // 6.99 min
		0.00113167927,   // 10.21 min
		0.00079837464,   // 14.48 min
		0.00056329683,   // 20.53 min
		0.00039709542,   // 29.11 min
		0.00028040032,   // 41.20 min
		0.00020925596,   // 55.19 min
		0.00016340649,   // 70.69 min
		0.00012795530,   // 90.34 min
		0.00010027770,   // 115.29 min
		0.00007838195,   // 147.42 min
		0.00006139454,   // 188.24 min
		0.00004813085    // 240.03 min
	};
	double k = heKValues[compartmentIndex];
	
	double pTissue0 = (double)getCompartmentHePartialPressure(compartmentIndex);
	double pAlv = (double)pAlvHeStart;
	double Rd = (double)R_He;
	double t = (double)timeInSeconds;
	
	// Schreiner 公式
	double pNew = pAlv + Rd * (t - 1.0/k) 
	            - (pAlv - pTissue0 - Rd/k) * exp(-k * t);
	
	// He 载荷不能为负
	if (pNew < 0) pNew = 0;
	
	return (float)pNew;
}

// 计算指定组织舱能安全上升到的环境压力（使用组合 a/b 系数，支持 Trimix，mbar）
float Buhlmann::getAscendToPartialPressureForCompartment(int compartmentIndex, float compartmentPartialPressure) {
	// ZHL-16C算法：P_tolerated = a + P_ambient / b
	// 反推：P_ambient = (P_tolerated - a) * b
	// 使用组合系数（按 N2/He 载荷比例加权）
	float combinedA = getCompartmentCombinedA(compartmentIndex);
	float combinedB = getCompartmentCombinedB(compartmentIndex);
	
	float tissuePressureBar = compartmentPartialPressure / 1000.0f;  // mbar → bar
	float ambientPressureBar = (tissuePressureBar - combinedA) * combinedB;
	return ambientPressureBar * 1000.0f;  // bar → mbar
}

/**
 * @brief 使用 GF 修正后的 M 值计算允许上升到的环境压力（Deep-Stops.pdf 图4标准公式）
 * @param compartmentIndex 组织舱索引 (0-15)
 * @param compartmentPartialPressure 组织氮气分压 (mbar)
 * @param currentGF 当前使用的梯度因子 (0.0-1.0)
 * @return 允许上升到的环境压力 (mbar)
 * 
 * 公式推导（Deep-Stops.pdf 图4）：
 * 1. GF修正后的允许组织分压：P_tissue_limit = P_amb × (GF/b - GF + 1) + GF × a
 * 2. 反推允许上升到的环境压力：P_amb_limit = (P_tissue - GF × a) / (GF/b - GF + 1)
 * 
 * 当 P_amb_limit > 海平面大气压时，需要减压停
 */
float Buhlmann::getAscendToPartialPressureForCompartmentWithGF(int compartmentIndex, float compartmentPartialPressure, float currentGF) {
	if (compartmentIndex < 0 || compartmentIndex >= COMPARTMENT_COUNT) {
		return 0.0f;
	}
	
	// 1. 转换单位：mbar → bar
	float tissuePressureBar = compartmentPartialPressure / 1000.0f;
	
	// 2. 获取组合 Buhlmann 系数（按 N2/He 载荷比例加权）
	float a = getCompartmentCombinedA(compartmentIndex);
	float b = getCompartmentCombinedB(compartmentIndex);
	
	// 3. Deep-Stops.pdf 图4 标准公式反推
	//    已知组织分压和GF，反推允许上升到的环境压力
	//    P_amb_limit = (P_tissue - GF × a) / (GF/b - GF + 1)
	float denominator = currentGF / b - currentGF + 1.0f;
	
	// ========== 防止除以零（仅保留最基本的安全检查）==========
	if (denominator < 0.0001f) {
		denominator = 0.0001f;
	}
	
	float ambientPressureLimitBar = (tissuePressureBar - currentGF * a) / denominator;
	
	// 4. 转换回 mbar 单位
	return ambientPressureLimitBar * 1000.0f;
}

// 判断是否需要减压（允许上升压力 > 海平面大气压）
bool Buhlmann::isDecoNeeded(float ascendToPartialPressure) {
	return ascendToPartialPressure > _seaLevelAtmosphericPressure;
}

/**
 * @brief 计算当前减压停深度对应的 GF 值（GF 线性渐变，Deep-Stops.pdf 图4核心公式）
 * @param currentStopDepth 当前减压停深度 (米)
 * @param firstStopDepth 首停深度 (米)
 * @param finalStopDepth 最终停深度 (米，通常是 3米 或 水面0米)
 * @return 当前深度对应的 GF 值 (0.0-1.0)
 * 
 * 公式（Deep-Stops.pdf 图4）：
 * GF_slope = (GF_High - GF_Low) / (最终停深度 - 首停深度)
 * GF_current = GF_slope × 当前停深度 + GF_High
 * 
 * 效果：
 * - 在首停深度：GF = GF_Low（最保守，深停）
 * - 在水面：GF = GF_High（标准保守度）
 * - 中间深度：线性插值（平滑梯度）
 * 
 * 示例（Deep-Stops.pdf 图3）：
 * - GF Low = 0.2, GF High = 0.75
 * - 首停54米 → GF = 0.2 (20%)
 * - 30米停 → GF = 0.4 (40%)
 * - 水面0米 → GF = 0.75 (75%)
 */
float Buhlmann::calculateCurrentGF(float currentStopDepth, float firstStopDepth, float finalStopDepth) {
	// 边界检查：首停深度必须 > 最终停深度，否则返回 GF High
	if (firstStopDepth <= finalStopDepth) {
		return _gfHigh;
	}

	// 深度范围检查
	if (currentStopDepth >= firstStopDepth) {
		// 在首停深度或更深，使用 GF Low
		return _gfLow;
	} else if (currentStopDepth <= finalStopDepth) {
		// 在最终停深度或更浅（水面），使用 GF High
		return _gfHigh;
	}

	// ========== 修正：使用标准线性插值公式 ==========
	//
	// 数学原理：
	// - 在 firstStopDepth (深) 时，GF = GF_Low
	// - 在 finalStopDepth (浅) 时，GF = GF_High
	// - 中间线性过渡
	//
	// 公式：GF = GF_Low + (GF_High - GF_Low) × (firstStop - current) / (firstStop - finalStop)
	//
	// 验证：
	// - current = firstStop: ratio = 0, GF = GF_Low ✓
	// - current = finalStop: ratio = 1, GF = GF_High ✓

	float depthRange = firstStopDepth - finalStopDepth;  // 总深度范围（正值）
	float depthFromFirst = firstStopDepth - currentStopDepth;  // 距首停的距离（正值）
	float ratio = depthFromFirst / depthRange;  // 0.0 到 1.0

	float currentGF = _gfLow + (_gfHigh - _gfLow) * ratio;

	// 安全约束（防止浮点误差）
	return constrain(currentGF, _gfLow, _gfHigh);
}

/**
 * @brief 计算免减压时间（NDL）- 返回分钟数
 * @param compartmentIndex 组织舱索引（0-15）
 * @param currentPressure 当前环境压力（mbar）
 * @return 还能安全停留的分钟数
 * @details 使用 Haldane 时间公式：t = -(1/k) × ln((pTol - pAlv) / (pTissue0 - pAlv))
 */
int Buhlmann::getMinutesNeededTillDeco(int compartmentIndex, float currentPressure) {
	double pTissue0 = (double)getCompartmentPartialPressure(compartmentIndex) / 1000.0;  // mbar -> bar
	double pAlv = (double)calculateNitrogenPartialPressureInLung(currentPressure) / 1000.0;  // mbar -> bar
	
	// 计算 k 值 (1/秒)
	double halfTimeMinutes = (double)_halfTimesNitrogen[compartmentIndex];
	double k = 0.693147180559945 / (halfTimeMinutes * 60.0);
	
	// 计算 GF High 修正后的允许组织分压上限 pTol (bar)
	double a = (double)_aValuesNitrogen[compartmentIndex];
	double b = (double)_bValuesNitrogen[compartmentIndex];
	double gf = (double)_gfHigh;
	double pAmb = (double)_seaLevelAtmosphericPressure / 1000.0;  // mbar -> bar
	
	double pTol = a * gf + ((pAmb * (gf - gf * b + b)) / b);
	
	// 检查当前组织分压是否已经超过 pTol
	if (pTissue0 >= pTol) {
		return 0;
	}
	
	// 检查 pTol 是否可达
	if (pTol >= pAlv) {
		return 999;
	}
	
	// Haldane 时间公式
	double logArg = (pTol - pAlv) / (pTissue0 - pAlv);
	if (logArg <= 0) {
		return 999;
	}
	
	double timeInSeconds = -(1.0 / k) * log(logArg);
	int minutes = (int)(timeInSeconds / 60.0);
	
	if (minutes < 0) minutes = 0;
	if (minutes > 999) minutes = 999;
	
	return minutes;
}

/**
 * @brief 计算 Trimix 免减压时间（NDL） - 二分法高精度求解
 */
double Buhlmann::getSecondsNeededTillDecoDouble(int compartmentIndex, float currentPressure) {
    float tissueN2 = getCompartmentN2Pressure(compartmentIndex);
    float tissueHe = getCompartmentHePressure(compartmentIndex);
    float lungN2 = calculateNitrogenPartialPressureInLung(currentPressure);
    float lungHe = calculateHeliumPartialPressureInLung(currentPressure);
    
    float currentTotal = tissueN2 + tissueHe;
    float currentPressureBar = _seaLevelAtmosphericPressure / 1000.0f; // NDL 永远以冲出水面为基准

    // 获取当前的组合 a, b (基于此刻比例)
    float a0 = getCompartmentCombinedA(compartmentIndex);
    float b0 = getCompartmentCombinedB(compartmentIndex);
    float pTol0 = (a0 * _gfHigh + ((currentPressureBar * (_gfHigh - _gfHigh * b0 + b0)) / b0)) * 1000.0f;

    // 1. 如果现在就已经超标，NDL = 0
    if (currentTotal >= pTol0) return 0.0;

    // 2. 如果无限期待下去，最终组织达到肺部环境平衡时，是否超标？
    float a_inf = (_aValuesNitrogen[compartmentIndex] * lungN2 + _aValuesHelium[compartmentIndex] * lungHe) / (lungN2 + lungHe + 0.0001f);
    float b_inf = (_bValuesNitrogen[compartmentIndex] * lungN2 + _bValuesHelium[compartmentIndex] * lungHe) / (lungN2 + lungHe + 0.0001f);
    float pTol_inf = (a_inf * _gfHigh + ((currentPressureBar * (_gfHigh - _gfHigh * b_inf + b_inf)) / b_inf)) * 1000.0f;
    
    // 如果肺压最终平衡点都比极限低，说明在这个深度永远免减压 (NDL = 99+)
    if ((lungN2 + lungHe) <= pTol_inf) return 5940.0; // 99 mins

    // 3. 二分查找突破红线的时间 (范围: 0 到 99 分钟)
    double t_low = 0.0;
    double t_high = 5940.0;
    double t_ans = 5940.0;

    // 15次二分即可精确到 ~0.18秒
    for (int iter = 0; iter < 15; iter++) {
        double t_mid = (t_low + t_high) / 2.0;
        
        float simN2 = calculateCompartmentInertGasPartialPressure(t_mid, getCompartmentHalfTimeInSeconds(compartmentIndex), tissueN2, lungN2);
        float simHe = calculateCompartmentInertGasPartialPressure(t_mid, getCompartmentHeHalfTimeInSeconds(compartmentIndex), tissueHe, lungHe);
        float simTotal = simN2 + simHe;
        
        float a = (_aValuesNitrogen[compartmentIndex] * simN2 + _aValuesHelium[compartmentIndex] * simHe) / (simTotal + 0.0001f);
        float b = (_bValuesNitrogen[compartmentIndex] * simN2 + _bValuesHelium[compartmentIndex] * simHe) / (simTotal + 0.0001f);
        float pTol = (a * _gfHigh + ((currentPressureBar * (_gfHigh - _gfHigh * b + b)) / b)) * 1000.0f;

        if (simTotal >= pTol) {
            t_high = t_mid; // 在 t_mid 之前就已经越线了，缩小上限
            t_ans = t_mid;
        } else {
            t_low = t_mid;  // 还没越线，安全时间更长，提高下限
        }
    }
    
    return t_ans;
}


// 兼容旧接口
int Buhlmann::getSecondsNeededTillDeco(int compartmentIndex, float currentPressure) {
	return (int)getSecondsNeededTillDecoDouble(compartmentIndex, currentPressure);
}

// 计算在指定压力下停留所需的时间（分钟），用于减压停时间计算
int Buhlmann::calculateMinutesRequiredToReachCertainPressure(float targetPressure) {
	float ascentCeilingArray[COMPARTMENT_COUNT];
	int requiredMinutes = 0;
	bool stopTimeCalculation = false;
	
	// *** 安全限制：防止无限循环 ***
	const int MAX_DECO_MINUTES = 999;  // 最大减压时间：999分钟

	while (!stopTimeCalculation) {
		requiredMinutes = requiredMinutes +1;
		
		// *** 安全检查：避免无限循环 ***
		if (requiredMinutes >= MAX_DECO_MINUTES) {
			#ifdef DEBUG_DECO
				rt_kprintf("[DECO警告] 减压时间超限! targetPressure=%.1f mbar\n", targetPressure);
			#endif
			return MAX_DECO_MINUTES;
		}

		// ========== 减压停阶段：应用 GF 线性渐变（Deep-Stops.pdf 图3）==========
		// 文档明确：GF 仅用于减压停阶段，用于生成深停序列和控制梯度
		float currentDepthForGF = calculateDepthFromPressure(targetPressure);
		float firstDecoStop = _currentDepth * 0.8f;  // 首停=当前深度的80%
		if (firstDecoStop < 3.0f) firstDecoStop = 3.0f;
		float currentGF = calculateCurrentGF(currentDepthForGF, firstDecoStop, 3.0f);

		for (int i=0; i < COMPARTMENT_COUNT; i++) {
			//Calculate and store the partial pressure for the current compartment
			float compartmentPressure = calculateCompartmentInertGasPartialPressure(
						requiredMinutes * 60,
						getCompartmentHalfTimeInSeconds(i),
						getCompartmentPartialPressure(i),
						calculateNitrogenPartialPressureInLung(targetPressure));
			
			// ========== 使用 GF 修正的允许组织分压上限 ==========
			ascentCeilingArray[i] = getAscendToPartialPressureForCompartmentWithGF(i, compartmentPressure, currentGF);
		}

		//All Ascent Ceiling has to be lower than the Aircraft Cabin Pressure
		float maximum = maxSearch(ascentCeilingArray, COMPARTMENT_COUNT);
		if (maximum < targetPressure) {
			stopTimeCalculation = true;
		}
	}
	return requiredMinutes;
}

// 计算上升速率等级（OK/SLOW/NORMAL/ATTENTION/CRITICAL/DANGER），用于安全监控
int Buhlmann::calculateAscentRate(float timeSpentInLevelInSeconds, float previousDepthInMeter, float currentDepthInMeter) {
	if (previousDepthInMeter > currentDepthInMeter) {

		float ascendInMeter = previousDepthInMeter - currentDepthInMeter;
		float rate = ascendInMeter / timeSpentInLevelInSeconds;

		float threshold;
		if (currentDepthInMeter <= 10) {
			//We are above 10 meters
			threshold = 0.1666666666666667;
		} else if (20 > currentDepthInMeter && currentDepthInMeter > 10) {
			//We are between 10 and 20 meters
			threshold = 0.2;
		} else {
			//We are below 18 meters
			threshold = 0.3;
		}

		if (rate <= (0.6f * threshold)) {
			return ASCEND_OK;
		} else if (rate <= (0.8f * threshold)) {
			return ASCEND_SLOW;
		} else if (rate <= (0.95f * threshold)) {
			return ASCEND_NORMAL;
		} else if (rate <= (1.05f * threshold)) {
			return ASCEND_ATTENTION;
		} else if (rate <= (1.2f * threshold)) {
			return ASCEND_CRITICAL;
		} else {
			return ASCEND_DANGER;
		}
	} else {
		return DESCEND;
	}
}

///////////////////
// Dive Progress //
///////////////////

// 减压计算核心流程：
// 1) startDive：设置初始舱压力、时间基准、最大深度等。
// 2) progressDive：用本次时间片的压力更新16个舱的氮分压，判定是否需要减压，
//    并给出 NDL/减压停信息。
// 3) stopDive：结束后计算脱饱和时间和 No-Fly 时间，返回本次潜水结果。
// 4) surfaceInterval：两次潜水间的水面休息，按时间衰减组织分压。
// 计算水面休息期间的组织氮分压衰减，返回新的潜水结果
DiveResult* Buhlmann::surfaceInterval(long surfaceIntervalInMinutes, DiveResult* previousDiveResult) {

	//Set the given compartment pressure data
    resetSafetyStopState(false);

    for (byte j = 0; j < COMPARTMENT_COUNT; j++) {
    	_compartmentCurrentPartialPressures[j] = previousDiveResult->compartmentPartialPressures[j];
    }

    float initialPartialPressureWithoutDive = calculateNitrogenPartialPressureInLung(_seaLevelAtmosphericPressure);

	//For each compartment calculate the new partial pressures after spent X amount of time on the surface
	for (int i=0; i < COMPARTMENT_COUNT; i++) {
		setCompartmentPartialPressure(i, calculateCompartmentInertGasPartialPressure(
				surfaceIntervalInMinutes * 60,
				getCompartmentHalfTimeInSeconds(i),
				getCompartmentPartialPressure(i),
				initialPartialPressureWithoutDive));
	}

	//If the partial pressure of the compartment becomes less than the no-dive initial partial pressure - set it as the initial partial pressure
	for (int i=0; i < COMPARTMENT_COUNT; i++) {
		if (getCompartmentPartialPressure(i) < initialPartialPressureWithoutDive) {
			setCompartmentPartialPressure(i, initialPartialPressureWithoutDive);
		}
	}

	DiveResult* diveResult = new DiveResult;
    for (byte j = 0; j < COMPARTMENT_COUNT; j++) {
    	diveResult->compartmentPartialPressures[j] = _compartmentCurrentPartialPressures[j];
    }
    diveResult->maxDepthInMeters = 0;
	diveResult->durationInSeconds = surfaceIntervalInMinutes * 60;
	if (previousDiveResult->noFlyTimeInMinutes > surfaceIntervalInMinutes) {
		diveResult->noFlyTimeInMinutes = previousDiveResult->noFlyTimeInMinutes - surfaceIntervalInMinutes;
	} else {
		diveResult->noFlyTimeInMinutes = 0;
	}
	//Copy over surface time and deco information
	diveResult->wasDecoDive = previousDiveResult->wasDecoDive;
	diveResult->previousDiveDateTimestamp = previousDiveResult->previousDiveDateTimestamp;
	return diveResult;
}

// 初始化所有组织舱为海平面平衡状态（陆地状态），返回初始化的潜水结果
DiveResult* Buhlmann::initializeCompartments() {

	// Serial.println("Compartment initialization - START");
    DiveResult* diveResult = new DiveResult;
    diveResult->maxDepthInMeters = 0;
    diveResult->durationInSeconds = 0;
    diveResult->noFlyTimeInMinutes = 0;

    //Initialize the compartments with the initial partial pressure value - it assumes that the inert gases were cleared off from the compartments
    for (int i=0; i < COMPARTMENT_COUNT; i++) {
        diveResult->compartmentPartialPressures[i] = calculateNitrogenPartialPressureInLung(_seaLevelAtmosphericPressure);
        // He 载荷初始化为 0（水面呼吸空气无氦气）
        _compartmentHePartialPressures[i] = 0.0f;
    }

    // Serial.println(F("Compartment initialization - DONE"));
    _lastCeilingPressureMbar = _seaLevelAtmosphericPressure;
    _lastCeilingCompartmentIndex = -1;
    
    // ========== 初始化减压站序列 ==========
    _decoSequence.stopCount = 0;
    _decoSequence.currentStopIdx = -1;
    _firstStopDepth = 0.0f;
    _effectiveCeiling = 0.0f;
    _isMissedDeco = false;
    resetSafetyStopState(false);
    
    return diveResult;
}

// 开始一次新潜水：设置初始组织分压、重置时间/深度/减压标志
void Buhlmann::startDive(DiveResult* previousDiveResult, unsigned long diveStartTimestamp) {
	_previousDiveResult = previousDiveResult;
	_diveStartTimestamp = diveStartTimestamp;

	//Set dive duration calculation to zero
    _currentDiveDuration = 0;

    //Set the deco indicator to false
    _wasDecoDive = false;

    //Reset maximum depth
    _maxDepth = 0;

    // 重置天花板记录（新一次潜水从海平面起算）
    _lastCeilingPressureMbar = _seaLevelAtmosphericPressure;
    _lastCeilingCompartmentIndex = -1;
    
    // ========== 重置减压站序列（新一次潜水）==========
    _decoSequence.stopCount = 0;
    _decoSequence.currentStopIdx = -1;
    _firstStopDepth = 0.0f;
    _effectiveCeiling = 0.0f;
    _isMissedDeco = false;
    
    // 重置首停深度压力（与Java对齐）
    _gfLowDepthPressure = _seaLevelAtmosphericPressure;
    resetSafetyStopState(false);

    //Initialize the compartments based on the previous compartment partial pressure values
    for (byte j = 0; j < COMPARTMENT_COUNT; j++) {
    	_compartmentCurrentPartialPressures[j] = previousDiveResult->compartmentPartialPressures[j];
    }
    
    // 初始化上一次压力为海平面（用于 Schreiner 公式）
    _lastPressure = 1000.0f;
}

// 推进潜水进度：更新组织分压、判断减压需求、计算 NDL 或减压停信息
void Buhlmann::resetSafetyStopState(bool completed) {
    _safetyStopArmed = false;
    _safetyStopCompleted = completed;
    _safetyStopElapsedSeconds = 0;
}

void Buhlmann::updateSafetyStop(DiveInfo &diveInfo, unsigned int timeSpentInLevel, bool decoActive) {
    diveInfo.stopType = BUHLMANN_STOP_NONE;
    diveInfo.stopDepthMeters = 0.0f;
    diveInfo.stopTimeTotalSeconds = 0;
    diveInfo.stopTimeRemainingSeconds = 0;
    diveInfo.inStopZone = false;

    if (_currentDepth <= _safetyStopSurfaceResetDepthMeters) {
        resetSafetyStopState(false);
        return;
    }

    if (decoActive) {
        resetSafetyStopState(true);
        return;
    }

    if (!_safetyStopEnabled || _safetyStopDurationSeconds <= 0 || _safetyStopCompleted) {
        return;
    }

    if (_currentDepth > _safetyStopArmDepthMeters) {
        if (!_safetyStopArmed) {
            _safetyStopArmed = true;
            _safetyStopElapsedSeconds = 0;
        } else if (_safetyStopElapsedSeconds > 0) {
            _safetyStopElapsedSeconds = 0;
        }
    }

    if (!_safetyStopArmed) {
        return;
    }

    bool inZone = fabsf(_currentDepth - _safetyStopDepthMeters) <= _safetyStopZoneHalfWidthMeters;
    if (inZone) {
        _safetyStopElapsedSeconds += (int)timeSpentInLevel;
        if (_safetyStopElapsedSeconds >= _safetyStopDurationSeconds) {
            resetSafetyStopState(true);
            return;
        }
    }

    int remainingSeconds = _safetyStopDurationSeconds - _safetyStopElapsedSeconds;
    if (remainingSeconds < 0) {
        remainingSeconds = 0;
    }

    diveInfo.stopType = BUHLMANN_STOP_SAFETY;
    diveInfo.stopDepthMeters = _safetyStopDepthMeters;
    diveInfo.stopTimeTotalSeconds = _safetyStopDurationSeconds;
    diveInfo.stopTimeRemainingSeconds = remainingSeconds;
    diveInfo.inStopZone = inZone;
    diveInfo.ttsSeconds += remainingSeconds;
}

DiveInfo Buhlmann::progressDive(float currentPressure, unsigned int duration) {

	DiveInfo diveInfo;
	// 默认值，防止 timeSpentInLevel==0 时返回未初始化数据
	diveInfo.ascendRate = ASCEND_OK;
	diveInfo.decoNeeded = false;
	diveInfo.minutesToDeco = 999;
	diveInfo.decoStopInMeters = 0;
	diveInfo.decoStopDurationInMinutes = 0;
	diveInfo.decoStopDurationInSeconds = 0;
	diveInfo.stopType = BUHLMANN_STOP_NONE;
	diveInfo.stopDepthMeters = 0.0f;
	diveInfo.stopTimeTotalSeconds = 0;
	diveInfo.stopTimeRemainingSeconds = 0;
	diveInfo.inStopZone = false;
	diveInfo.ttsSeconds = 0;
	// ========== 初始化新增字段 ==========
	diveInfo.decoSequence.stopCount = 0;
	diveInfo.decoSequence.currentStopIdx = -1;
	diveInfo.isMissedDecoStop = false;
	diveInfo.effectiveCeiling = 0.0f;

    unsigned int timeSpentInLevel = duration; // 现在duration是增量时间
    if (timeSpentInLevel > 0) {

        //Calculate ascent - descent depth here
        int ascendRate = calculateAscentRate(timeSpentInLevel, _currentDepth, calculateDepthFromPressure(currentPressure));

		//更新累计时间
        _currentDiveDuration = _currentDiveDuration + timeSpentInLevel;
        _currentDepth = calculateDepthFromPressure(currentPressure);

        //Calculate the maximum depth
        if (_maxDepth < _currentDepth) {
            _maxDepth = _currentDepth;
        }

        diveInfo.ascendRate = ascendRate;

        bool isDecoNeededArray[COMPARTMENT_COUNT];
        int minutesNeededToDecoArray[COMPARTMENT_COUNT];
        float ascentCeilingArray[COMPARTMENT_COUNT];
        float ascentCeilingGFLowArray[COMPARTMENT_COUNT];  // 用于天花板显示的GF_Low天花板

        // 使用 Schreiner 公式更新组织分压（N2 + He）
        // R 是肺部分压的变化率（mbar/秒）
        float pAlvN2Start = calculateNitrogenPartialPressureInLung(_lastPressure);
        float pAlvN2End = calculateNitrogenPartialPressureInLung(currentPressure);
        float pAlvHeStart = calculateHeliumPartialPressureInLung(_lastPressure);
        float pAlvHeEnd = calculateHeliumPartialPressureInLung(currentPressure);
        
        float R_N2 = 0.0f;
        float R_He = 0.0f;
        if (timeSpentInLevel > 0) {
            R_N2 = (pAlvN2End - pAlvN2Start) / (float)timeSpentInLevel;
            R_He = (pAlvHeEnd - pAlvHeStart) / (float)timeSpentInLevel;
        }

        for (int i=0; i < COMPARTMENT_COUNT; i++) {
            // 更新 N2 载荷
            float newN2Pressure = calculateCompartmentPressureSchreiner(i, pAlvN2Start, R_N2, (float)timeSpentInLevel);
            setCompartmentPartialPressure(i, newN2Pressure);
            
            // 更新 He 载荷
            float newHePressure = calculateCompartmentHePressureSchreiner(i, pAlvHeStart, R_He, (float)timeSpentInLevel);
            setCompartmentHePartialPressure(i, newHePressure);

            // ========== NDL 阶段：用 GF High（对应出水安全），减压时天花板记录用 GF_Low ==========
            // 使用总惰性气体载荷（N2 + He）计算 ceiling
            float totalInertLoad = getCompartmentTotalInertLoad(i);
            // 用于NDL判定：GF_High（宽松参照，与Java getNDL一致）
            ascentCeilingArray[i] = getAscendToPartialPressureForCompartmentWithGF(
                    i, totalInertLoad, _gfHigh);
            // 用于天花板显示：GF_Low（保守参照，与Java getCurrentCeilingWithGFLow一致）
            ascentCeilingGFLowArray[i] = getAscendToPartialPressureForCompartmentWithGF(
                    i, totalInertLoad, _gfLow);

            // 判断是否需要减压（允许上升压力 > 海平面大气压）
            isDecoNeededArray[i] = isDecoNeeded(ascentCeilingArray[i]);
            if (!isDecoNeededArray[i]) 
			{
                //Calculate minutes can be spent in the current depth
                minutesNeededToDecoArray[i] = getMinutesNeededTillDeco(i, currentPressure);
            } 
			else 
			{
                minutesNeededToDecoArray[i] = 0;
                //Calculate first deco stop
                float firstDecoStop = calculateDepthFromPressure(ascentCeilingArray[i]);
            }
        }

        //Calculate if DECO is needed for all compartments
        bool decoNeeded = false;
        for (int i=0; i < COMPARTMENT_COUNT; i++) {
            if (isDecoNeededArray[i]) {
                decoNeeded = true;
                break;
            }
        }

        // 记录本次最大上升天花板（mbar）及对应舱，用于 GF@ceiling（单一限制舱）
        // 与Java对齐：使用 GF_Low 的天花板（保守，用于显示）
        float ascentCeiling = ascentCeilingGFLowArray[0];
        int ascentCeilingIdx = 0;
        for (int i = 1; i < COMPARTMENT_COUNT; i++) {
            if (ascentCeilingGFLowArray[i] > ascentCeiling) {
                ascentCeiling = ascentCeilingGFLowArray[i];
                ascentCeilingIdx = i;
            }
        }
        _lastCeilingPressureMbar = ascentCeiling;
        _lastCeilingCompartmentIndex = ascentCeilingIdx;

        diveInfo.decoNeeded = decoNeeded;
        // 检查是否有有效的减压序列
        bool hasValidDecoSequence = (_decoSequence.stopCount > 0 && _decoSequence.currentStopIdx >= 0);

        if (!decoNeeded && !hasValidDecoSequence)
		{
        	// 真正的非减压状态：显示NDL信息
        	int minutesToDeco = minSearch(minutesNeededToDecoArray, COMPARTMENT_COUNT);
        	diveInfo.minutesToDeco = minutesToDeco;

            // 非减压时天花板视为海平面，避免沿用历史值
            _lastCeilingPressureMbar = _seaLevelAtmosphericPressure;
            _lastCeilingCompartmentIndex = -1;

            // 重置序列（从未进入减压状态）
            _decoSequence.currentStopIdx = -1;
            _decoSequence.stopCount = 0;
            _isMissedDeco = false;
            _effectiveCeiling = 0.0f;
            
            // ========== 非减压状态也计算 TTS（只是上升时间）==========
            // TTS = 从当前深度直接上升到水面的时间
            // 上升速度：10 米/分钟 = 6 秒/米
            const float ASCENT_RATE_SECONDS_PER_METER = 6.0f;  // 10m/min = 6s/m
            diveInfo.ttsSeconds = (int)(_currentDepth * ASCENT_RATE_SECONDS_PER_METER);
        } else {
        	//The dive became a DECO dive
        	_wasDecoDive = true;

            //Find the deepest ascent ceiling (the biggest pressure)
            ascentCeiling = maxSearch(ascentCeilingArray, COMPARTMENT_COUNT);

            // 每次都更新首停深度压力（只会变深，不会变浅）
            // 与Java对齐：使用总惰性气体载荷（N2 + He）而非单独的N2分压
            float deepStopPressure = _seaLevelAtmosphericPressure;
            for (int i = 0; i < COMPARTMENT_COUNT; i++) {
                float totalLoad = getCompartmentTotalInertLoad(i);  // N2 + He 总载荷
                float a = getCompartmentCombinedA(i);               // 混合a系数
                float b = getCompartmentCombinedB(i);               // 混合b系数
                float tissuePressureBar = totalLoad / 1000.0f;      // mbar → bar
                float allowedPressureBar = ((tissuePressureBar - _gfLow * a) * b) / (_gfLow - _gfLow * b + b);
                float allowedPressure = allowedPressureBar * 1000.0f;
                if (allowedPressure > deepStopPressure) {
                    deepStopPressure = allowedPressure;
                }
            }
            // 首停深度只会变深，不会变浅（保守策略）
            if (deepStopPressure > _gfLowDepthPressure) {
                _gfLowDepthPressure = deepStopPressure;
            }

            // 每次 progressDive 都重新生成减压序列（动态更新）
            generateDecoSequence(currentPressure, _currentDepth);
            
            // 2. 如果已生成序列，先动态重算当前站所需停留时间（基于当前组织分压），再更新当前站状态
            if (_decoSequence.currentStopIdx >= 0 && _decoSequence.currentStopIdx < _decoSequence.stopCount) {
                // 动态重算当前站的 remainingTime（每次 progress 调用都会重新计算）
                DecoStop& currentStop = _decoSequence.stops[_decoSequence.currentStopIdx];
                float stopPressure = calculateHydrostaticPressureFromDepth(currentStop.depth);
                float nextStopDepth = currentStop.depth - 3.0f;
                float nextStopPressure = (nextStopDepth <= 0) ? _seaLevelAtmosphericPressure : calculateHydrostaticPressureFromDepth(nextStopDepth);

                // 1. 获取当前真实的双轨载荷 (N2 和 He 独立)
                float tempN2[COMPARTMENT_COUNT];
                float tempHe[COMPARTMENT_COUNT];
                for (int i = 0; i < COMPARTMENT_COUNT; i++) {
                    tempN2[i] = getCompartmentN2Pressure(i);
                    tempHe[i] = getCompartmentHePressure(i);
                }

                // 2. 预测当前站应该吸什么气 (上帝视角切气)
                int simGasIndex = getBestGasForDepth(currentStop.depth);
                if (simGasIndex < 0) simGasIndex = _activeGasIndex;

                // ========== 修复：使用支持 Trimix 的全新 6 参数重算函数 ==========
                int requiredSec = calculateDecoStopDuration(stopPressure, nextStopPressure, currentStop.targetGF, tempN2, tempHe, getGas(simGasIndex));
                currentStop.remainingTime = requiredSec;  // 直接使用动态计算的值，不再减去 timeSpentInLevel
                if (currentStop.totalTime < requiredSec) {
                    currentStop.totalTime = requiredSec;
                }
                // 记录到成员，主循环统一按照打印间隔输出，避免频繁打印
                _lastDynamicRequiredSeconds = requiredSec;
                _lastDynamicStopDepthMeters = currentStop.depth;
                _lastDynamicCalcMillis = rt_tick_get();

                // ========== 修复：updateCurrentDecoStop 只负责站切换判断，不减时间 ==========
                updateCurrentDecoStopSwitchOnly(_currentDepth);
            }
            
            // 3. 检查是否跳过当前减压站（违规）
            if (isDecoStopMissed(_currentDepth)) {
                _isMissedDeco = true;
                // 锁定有效天花板为当前跳过的站深度
                if (_decoSequence.currentStopIdx >= 0 && _decoSequence.currentStopIdx < _decoSequence.stopCount) {
                    _effectiveCeiling = _decoSequence.stops[_decoSequence.currentStopIdx].depth;
                }
            }
            
            // 4. 填充 DiveInfo：当前需停留的深度和剩余时间
            if (_decoSequence.currentStopIdx >= 0 && _decoSequence.currentStopIdx < _decoSequence.stopCount) {
                DecoStop& currentStop = _decoSequence.stops[_decoSequence.currentStopIdx];
                diveInfo.decoStopInMeters = (int)currentStop.depth;
                diveInfo.decoStopDurationInMinutes = (currentStop.remainingTime + 59) / 60;  // 向上取整到分钟
                diveInfo.decoStopDurationInSeconds = currentStop.remainingTime;
                
                // 填充完整序列信息
                diveInfo.decoSequence = _decoSequence;
                diveInfo.isMissedDecoStop = _isMissedDeco;
                diveInfo.effectiveCeiling = _effectiveCeiling;
            } else {
                // 所有站已完成，但仍在减压状态（可能是刚完成最后一站）
                diveInfo.decoStopInMeters = 0;
                diveInfo.decoStopDurationInMinutes = 0;
                diveInfo.decoStopDurationInSeconds = 0;
                diveInfo.decoSequence = _decoSequence;
                diveInfo.isMissedDecoStop = false;
                diveInfo.effectiveCeiling = 0.0f;
            }
        }

        // ========== 最终检查：优先显示减压序列信息 ==========
        // 无论当前decoNeeded状态，如果有有效的减压序列，就显示减压站信息
        hasValidDecoSequence = (_decoSequence.stopCount > 0 && _decoSequence.currentStopIdx >= 0);
        updateSafetyStop(diveInfo, timeSpentInLevel, hasValidDecoSequence || decoNeeded);
        if (hasValidDecoSequence) {
            DecoStop& currentStop = _decoSequence.stops[_decoSequence.currentStopIdx];
            diveInfo.minutesToDeco = 0;  // 有减压序列时NDL为0
            diveInfo.decoStopInMeters = (int)currentStop.depth;
            diveInfo.decoStopDurationInMinutes = (currentStop.remainingTime + 59) / 60;
            diveInfo.decoStopDurationInSeconds = currentStop.remainingTime;
            diveInfo.decoSequence = _decoSequence;
            diveInfo.isMissedDecoStop = _isMissedDeco;
            diveInfo.effectiveCeiling = _effectiveCeiling;
            diveInfo.stopType = BUHLMANN_STOP_DECO;
            diveInfo.stopDepthMeters = currentStop.depth;
            diveInfo.stopTimeTotalSeconds = currentStop.totalTime;
            if (diveInfo.stopTimeTotalSeconds < currentStop.remainingTime) {
                diveInfo.stopTimeTotalSeconds = currentStop.remainingTime;
            }
            diveInfo.stopTimeRemainingSeconds = currentStop.remainingTime;
            diveInfo.inStopZone = fabsf(_currentDepth - currentStop.depth) <= _safetyStopZoneHalfWidthMeters;
            
            // ========== 计算 TTS（包含上升时间，标准 10m/min）==========
            // TTS = 上升时间 + 所有站的停留时间
            // 上升速度：10 米/分钟 = 6 秒/米
            const float ASCENT_RATE_SECONDS_PER_METER = 6.0f;  // 10m/min = 6s/m
            
            int ttsSeconds = 0;
            
            // 1. 从当前深度上升到第一个减压站的时间
            if (_decoSequence.stopCount > 0) {
                float firstStopDepth = _decoSequence.stops[0].depth;
                if (_currentDepth > firstStopDepth) {
                    ttsSeconds += (int)((_currentDepth - firstStopDepth) * ASCENT_RATE_SECONDS_PER_METER);
                }
            }
            
            // 2. 所有减压站的停留时间 + 站间上升时间
            for (int i = 0; i < _decoSequence.stopCount; i++) {
                // 停留时间
                ttsSeconds += _decoSequence.stops[i].remainingTime;
                
                // 上升到下一站的时间（3m 间隔 = 18 秒）
                if (i < _decoSequence.stopCount - 1) {
                    float currentStopDepth = _decoSequence.stops[i].depth;
                    float nextStopDepth = _decoSequence.stops[i + 1].depth;
                    if (currentStopDepth > nextStopDepth) {
                        ttsSeconds += (int)((currentStopDepth - nextStopDepth) * ASCENT_RATE_SECONDS_PER_METER);
                    }
                }
            }
            
            // 3. 从最后一个减压站（通常是 3m）上升到水面的时间
            if (_decoSequence.stopCount > 0) {
                float lastStopDepth = _decoSequence.stops[_decoSequence.stopCount - 1].depth;
                ttsSeconds += (int)(lastStopDepth * ASCENT_RATE_SECONDS_PER_METER);
            }
            
            diveInfo.ttsSeconds = ttsSeconds;
        }

        //Serial.println("");
    }
    
    // 更新上一次压力（用于下次 Schreiner 计算）
    _lastPressure = currentPressure;
    
    return diveInfo;
}

// 结束潜水：计算脱饱和时间和 No-Fly 时间，返回本次潜水结果
DiveResult* Buhlmann::stopDive(unsigned long diveStopTimestamp) {

#ifdef RT_USING_SERIAL
	rt_kprintf("Buhmann - Stop Dive\n");
#endif

	long desaturationTimeInMinutes = calculateDesaturationTime(1.02);
    long noFlyTimeInMinutes = calculateNoFlyTime(desaturationTimeInMinutes);

    DiveResult* diveResult = new DiveResult;
    for (byte j = 0; j < COMPARTMENT_COUNT; j++) {
    	diveResult->compartmentPartialPressures[j] = _compartmentCurrentPartialPressures[j];
    }
    diveResult->maxDepthInMeters = _maxDepth;
    diveResult->durationInSeconds = _currentDiveDuration;
    diveResult->noFlyTimeInMinutes = noFlyTimeInMinutes;
    diveResult->wasDecoDive = _wasDecoDive;
    diveResult->previousDiveDateTimestamp = diveStopTimestamp;

    return diveResult;
}

// 计算脱饱和时间：所有组织舱回到基线（limitPercentage 倍）所需的最大时间（分钟）
long Buhlmann::calculateDesaturationTime(float limitPercentage) {

	//先算基线肺部氮分压
	float initialPartialPressure = calculateNitrogenPartialPressureInLung(_seaLevelAtmosphericPressure);
	float desatPartialPressureLimit = initialPartialPressure * limitPercentage;

	long desaturationTimeInMinutes = 1;
	for (int i=0; i < COMPARTMENT_COUNT; i++) {
		long timeInMinutes = 1;

		//取当前舱内氮分压
		float currentPartialPressure = getCompartmentPartialPressure(i);
#ifdef RT_USING_SERIAL
        rt_kprintf("%d compartment partial pressure: %.2f\n", i, currentPartialPressure);
#endif

		while (desatPartialPressureLimit < calculateCompartmentInertGasPartialPressure(timeInMinutes * 60,
				getCompartmentHalfTimeInSeconds(i), currentPartialPressure, initialPartialPressure)) {
			timeInMinutes++;
		}

#ifdef RT_USING_SERIAL
        rt_kprintf(">> desaturation time: %ld\n", timeInMinutes);
#endif

		//Maximum search to find the maximum desaturation time
		if (desaturationTimeInMinutes < timeInMinutes) {
			desaturationTimeInMinutes = timeInMinutes;
		}
	}

#ifdef RT_USING_SERIAL
    rt_kprintf("Desaturation time: %ld\n", desaturationTimeInMinutes);
#endif

	return desaturationTimeInMinutes;
}

/**
 * Definitions:
 * ************
 * Repetitive dive: The end of the previous dive was within 24 hours of the start of the current dive.
 * No-deco dive: There was no decompression stop calculated during the dive.
 *
 * Calculation rules:
 * ******************
 *
 * Non-repetitive AND No-deco dive:
 *     Desaturation time >  12 hours => No Fly time = Desaturation time
 *     Desaturation time <= 12 hours => No Fly time = 12 hours
 *
 * Repetitive OR Decompression dive:
 *     Desaturation time >  24 hours => No Fly time = Desaturation time
 *     Desaturation time <= 24 hours => No Fly time = 24 hours
 *
 * @param desaturationTimeInMinutes   	//脱饱和时间（分钟）
 * @return The calculated No Fly time 	//计算得到的 No Fly 时间（分钟）
 */
long Buhlmann::calculateNoFlyTime(long desaturationTimeInMinutes) {

	bool isRepetitiveDive = false;
	bool isDecoDive = false;

	//Test if the previous dive ended within 24 hours(1 day) = 24 × 60 × 60 = 86400 seconds
	if ((_diveStartTimestamp - _previousDiveResult->previousDiveDateTimestamp) < 86400) {
		isRepetitiveDive = true;
	}

#ifdef RT_USING_SERIAL
    rt_kprintf("Was DECO dive? %s\n", _wasDecoDive ? "Yes" : "No");
    rt_kprintf("Previous dive was DECO dive? %s\n", _previousDiveResult->wasDecoDive ? "Yes" : "No");
#endif

	//Test if this or the previous dive was a decompression dive
	if (_wasDecoDive || _previousDiveResult->wasDecoDive) {
		isDecoDive = true;
	}
	long noFlyTimeInMinutes = 1;
	if (isRepetitiveDive || isDecoDive) {
		if (desaturationTimeInMinutes > 1440) { //24 hours
			noFlyTimeInMinutes = desaturationTimeInMinutes;
		} else {
			noFlyTimeInMinutes = 1440;
		}
	} else {
		if (desaturationTimeInMinutes > 720) { //12 hours
			noFlyTimeInMinutes = desaturationTimeInMinutes;
		} else {
			noFlyTimeInMinutes = 720;
		}
	}

#ifdef RT_USING_SERIAL
    rt_kprintf("No Fly time: %ld, Is deco? %d, Is repetitive? %d\n", 
                noFlyTimeInMinutes, isDecoDive, isRepetitiveDive);
#endif

	return noFlyTimeInMinutes;
}

/**
 * 获取当前所有分组的氮气分压值
 * @param compartmentPressures 用于存储分压值的数组，大小应为COMPARTMENT_COUNT
 */
void Buhlmann::getCurrentCompartmentPressures(float compartmentPressures[COMPARTMENT_COUNT]) {
	for (int i = 0; i < COMPARTMENT_COUNT; i++) {
		compartmentPressures[i] = _compartmentCurrentPartialPressures[i];
	}
}

/**
 * 获取指定组织分组的M值压力
 * @param compartmentIndex 组织分组索引 (0-15)
 * @return M值压力 (mbar)
 * M = a + P_ambient / b
 */
float Buhlmann::getCompartmentMValuePressure(int compartmentIndex) {
	if (compartmentIndex < 0 || compartmentIndex >= COMPARTMENT_COUNT) {
		return 0.0f;
	}
	return getAscendToPartialPressureForCompartment(compartmentIndex, _compartmentCurrentPartialPressures[compartmentIndex]);
}

// ==============================================================================
// CNS氧中毒计算（基于NOAA CNS表）
// ==============================================================================

/**
 * @brief 根据PPO2获取CNS累积速率（%/分钟）
 * @param ppo2 氧分压（bar，1 bar ≈ 0.987 atm）
 * @return CNS累积速率（%/分钟）
 * 
 * 基于NOAA CNS Percentage Exposure Table（官方标准）：
 * PPO2 (atm) | 最大暴露时间 | CNS速率(%/min) | 说明
 * ------------------------------------------------------------
 *   ≤0.50    |    无限制    |    0.000      | 安全区域
 *   0.60     |   720 min    |    0.139      | 
 *   0.70     |   570 min    |    0.175      | 
 *   0.80     |   450 min    |    0.222      | 
 *   0.90     |   360 min    |    0.278      | 
 *   1.00     |   300 min    |    0.333      | 
 *   1.10     |   240 min    |    0.417      | 
 *   1.20     |   210 min    |    0.476      | 
 *   1.25     |   195 min    |    0.513      | ← 新增
 *   1.30     |   180 min    |    0.556      | 
 *   1.35     |   165 min    |    0.606      | ← 新增
 *   1.40     |   150 min    |    0.667      | 
 *   1.45     |   135 min    |    0.741      | ← 新增
 *   1.50     |   120 min    |    0.833      | 
 *   1.55     |    82 min    |    1.220      | ← 新增（危险区域）
 *   1.60     |    45 min    |    2.222      | 极度危险
 *   >1.60    |   禁止潜水   |    5.000      | 致命风险
 * 
 * 注意：PPO2单位为bar，NOAA表格为atm，两者近似相等（误差<2%）
 */
static float getCNSRatePerMinute(float ppo2)
{
	// 安全区域
	if (ppo2 <= 0.50f) return 0.0f;
	
	// 常规潜水区域（0.50 - 1.40 bar）
	if (ppo2 <= 0.60f) return 0.139f;   // 720分钟
	if (ppo2 <= 0.70f) return 0.175f;   // 570分钟
	if (ppo2 <= 0.80f) return 0.222f;   // 450分钟
	if (ppo2 <= 0.90f) return 0.278f;   // 360分钟
	if (ppo2 <= 1.00f) return 0.333f;   // 300分钟
	if (ppo2 <= 1.10f) return 0.417f;   // 240分钟
	if (ppo2 <= 1.20f) return 0.476f;   // 210分钟
	if (ppo2 <= 1.25f) return 0.513f;   // 195分钟
	if (ppo2 <= 1.30f) return 0.556f;   // 180分钟
	if (ppo2 <= 1.35f) return 0.606f;   // 165分钟
	if (ppo2 <= 1.40f) return 0.667f;   // 150分钟
	
	// 警告区域（1.40 - 1.60 bar）
	if (ppo2 <= 1.45f) return 0.741f;   // 135分钟
	if (ppo2 <= 1.50f) return 0.833f;   // 120分钟
	if (ppo2 <= 1.55f) return 1.220f;   // 82分钟（危险）
	if (ppo2 <= 1.60f) return 2.222f;   // 45分钟（极度危险）
	
	// 禁止区域（>1.60 bar）
	return 5.0f;  // 致命风险，快速累积
}

/**
 * @brief 更新累积CNS值
 * @param ppo2 当前氧分压（bar）
 * @param timeInMinutes 暴露时间（分钟）
 */
void Buhlmann::updateCNS(float ppo2, float timeInMinutes)
{
	float cnsRate = getCNSRatePerMinute(ppo2);
	float cnsIncrease = cnsRate * timeInMinutes;
	_cumulativeCNS += cnsIncrease;
	
	// CNS累积不能超过999%（实际上超过200%就极度危险了）
	if (_cumulativeCNS > 999.0f)
	{
		_cumulativeCNS = 999.0f;
	}
}

/**
 * @brief 获取当前累积CNS百分比
 * @return CNS百分比
 */
float Buhlmann::getCumulativeCNS()
{
	return _cumulativeCNS;
}

/**
 * @brief 重置CNS累积（出水后24小时CNS会自然衰减）
 */
void Buhlmann::resetCNS()
{
	_cumulativeCNS = 0.0f;
}

// 更新累积 OTU（氧中毒单位），基于 PPO2 和时间累积
void Buhlmann::updateOTU(float ppo2, float timeInMinutes)
{
	if (ppo2 <= 0.5f) return;
	
	float otuRate = pow((ppo2 - 0.5f), 0.83f);
	_cumulativeOTU += otuRate * timeInMinutes;
	
	if (_cumulativeOTU > 999.0f) _cumulativeOTU = 999.0f;
	
	#ifdef DEBUG_CNS_OTU
		#ifdef RT_USING_SERIAL
            rt_kprintf("[OTU] %.1f | PPO2=%.2f Rate=%.3f\n", _cumulativeOTU, ppo2, otuRate);
        #endif
	#endif
}

// 获取当前累积 OTU 值
float Buhlmann::getCumulativeOTU()
{
	return _cumulativeOTU;
}

// 重置 OTU 累积值
void Buhlmann::resetOTU()
{
	_cumulativeOTU = 0.0f;
}


// 计算 GF99：当前深度下的实时组织超饱和度百分比
float Buhlmann::calculateGF99()
{
    // 1. 获取当前真实水压 (由 progressDive 更新的 _lastPressure)
    float currentPressureBar = _lastPressure / 1000.0f;

    // 2. 计算当前肺部惰性气体总分压 (N2 + He)
    float inertGasFraction = _gases[_activeGasIndex].nitrogenFraction + _gases[_activeGasIndex].heliumFraction;
    float ambientInertPressure = inertGasFraction * (currentPressureBar - _waterVapourPressureCorrection / 1000.0f);

    // 组织是否已初始化
    bool isInitialized = false;
    for (int i = 0; i < COMPARTMENT_COUNT; i++)
    {
        if (getCompartmentTotalInertLoad(i) > 100.0f) // 100 mbar
        {
            isInitialized = true;
            break;
        }
    }
    if (!isInitialized) return 0.0f;

    float maxGFpercent = 0.0f;

    for (int i = 0; i < COMPARTMENT_COUNT; i++)
    {
        // 3. 使用总惰性气体载荷 (N2 + He)
        float tissuePressure = getCompartmentTotalInertLoad(i) / 1000.0f; // mbar → bar
        
        // 如果组织内气压小于等于外部气压，说明还在吸气，GF为0
        if (tissuePressure <= ambientInertPressure) continue;

        // 4. 使用 N2/He 加权的组合 M 值系数
        float a = getCompartmentCombinedA(i);
        float b = getCompartmentCombinedB(i);

        // 5. 计算当前深度下的原始 M 值极限 (GF = 1.0)
        float allowedOrig = currentPressureBar / b + a;

        // 6. GFHigh 修正后的安全红线 (仅用于UI警告)
        float allowedHigh = currentPressureBar * (_gfHigh / b - _gfHigh + 1.0f) + _gfHigh * a;

        // 7. GF99 核心方程: (组织压 - 环境压) / (极限压 - 环境压)
				float numerator = tissuePressure - currentPressureBar;
				float denominator = allowedOrig - currentPressureBar;
				
        if (denominator > 0.0001f)
        {
            float gfPercent = (numerator / denominator) * 100.0f;
            if (gfPercent > maxGFpercent) {
                maxGFpercent = gfPercent;
                
                // 记录对应舱的 allowedHigh/allowedOrig（以 mbar 为单位），供 UI 提取
                _lastAllowedHighPressureMbar = allowedHigh * 1000.0f;
                _lastAllowedOrigPressureMbar = allowedOrig * 1000.0f;
                
                // 计算警示等级：先判红色（超过原始M），再判黄色（超过GFHigh）
                if (tissuePressure > allowedOrig) {
                    _lastGF99WarningLevel = 2; // red
                } else if (tissuePressure > allowedHigh) {
                    _lastGF99WarningLevel = 1; // yellow
                } else {
                    _lastGF99WarningLevel = 0; // normal
                }
            }
        }
    }

    if (maxGFpercent < 0.0f) maxGFpercent = 0.0f;
    if (maxGFpercent > 999.0f) maxGFpercent = 999.0f;

    return maxGFpercent;
}

/**
 * @brief 计算 SurfGF（水面梯度因子）
 * @return 水面GF百分比
 * 含义：假设潜水员此刻瞬间上升到水面，体内余氮占水面M值的百分比
 */
float Buhlmann::calculateSurfaceGF()
{
    // 1. 强行使用水面大气压作为计算环境
    float surfacePressureBar = _seaLevelAtmosphericPressure / 1000.0f;
    
    // 2. 肺部总惰性气体分压 (水面)
    float inertGasFraction = _gases[_activeGasIndex].nitrogenFraction + _gases[_activeGasIndex].heliumFraction;
    float surfaceInertPressure = inertGasFraction * (surfacePressureBar - _waterVapourPressureCorrection / 1000.0f);

    bool isInitialized = false;
    for (int i = 0; i < COMPARTMENT_COUNT; i++)
    {
        if (getCompartmentTotalInertLoad(i) > 100.0f)
        {
            isInitialized = true;
            break;
        }
    }
    if (!isInitialized) return 0.0f;

    float maxGFpercent = 0.0f;

    for (int i = 0; i < COMPARTMENT_COUNT; i++)
    {
        // 3. 取出当前总惰性气压
        float tissuePressure = getCompartmentTotalInertLoad(i) / 1000.0f;
        
        float a = getCompartmentCombinedA(i);
        float b = getCompartmentCombinedB(i);

        // 4. 水面环境下的理论红线 (M-Value at Surface)
        float allowedOrig = surfacePressureBar / b + a;

        // 5. 核心计算 (不再使用 0.95 魔改系数，还原纯粹物理公式)
        float numerator = tissuePressure - surfaceInertPressure;
        float denominator = allowedOrig - surfaceInertPressure;

        if (denominator > 0.0001f)
        {
            float gfPercent = (numerator / denominator) * 100.0f;
            if (gfPercent > maxGFpercent)
            {
                maxGFpercent = gfPercent;
            }
        }
    }

    // 负数置零（表示绝对安全）
    if (maxGFpercent < 0.0f) maxGFpercent = 0.0f;
    if (maxGFpercent > 999.0f) maxGFpercent = 999.0f;

    return maxGFpercent;
}

// 获取最近一次 GF99 的警示等级（0=normal,1=yellow,2=red）
int Buhlmann::getLastGF99WarningLevel() {
	return _lastGF99WarningLevel;
}

// 获取最近一次计算中对应的 allowedHigh（mbar）
float Buhlmann::getLastAllowedHighPressureMbar() {
	return _lastAllowedHighPressureMbar;
}

// 获取最近一次计算中对应的 allowedOrig（mbar）
float Buhlmann::getLastAllowedOrigPressureMbar() {
	return _lastAllowedOrigPressureMbar;
}

// 获取最近一次动态计算的当前站所需秒数
int Buhlmann::getLastDynamicRequiredSeconds() {
	return _lastDynamicRequiredSeconds;
}

// 获取最近一次动态计算的当前站深度（米）
float Buhlmann::getLastDynamicStopDepthMeters() {
	return _lastDynamicStopDepthMeters;
}

// 获取最近一次动态计算时间（ms）
unsigned long Buhlmann::getLastDynamicCalcMillis() {
	return _lastDynamicCalcMillis;
}

// ========== 减压站序列生成与管理（Perdix 2 手册逻辑）==========

// 生成完整减压站序列（从首停深度到3米，按3米间隔）
void Buhlmann::generateDecoSequence(float currentPressure, float currentDepth) {
    // 减压站深度数组（3米间隔，0-120米）
    static const int decoStopDepths[] = {
        0,  3,  6,  9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39,
        42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81, 84, 87,
        90, 93, 96, 99, 102, 105, 108, 111, 114, 117, 120
    };
    static const int decoStopCount = sizeof(decoStopDepths) / sizeof(decoStopDepths[0]);

    // 1. 寻找首停深度
    int firstStopIdx = 1;
    for (int i = 1; i < decoStopCount; i++) {
        float stopPressure = calculateHydrostaticPressureFromDepth((float)decoStopDepths[i]);
        if (_gfLowDepthPressure <= stopPressure) {
            firstStopIdx = i;
            break;
        }
        if (i == decoStopCount - 1) firstStopIdx = i;
    }
    
    _firstStopDepth = (float)decoStopDepths[firstStopIdx];
    _decoSequence.stopCount = 0;
    _decoSequence.currentStopIdx = 0;
    
    // 2. [核心修复] 初始化 N2 和 He 的双轨独立临时数组
    float tempN2[COMPARTMENT_COUNT];
    float tempHe[COMPARTMENT_COUNT];
    for (int i = 0; i < COMPARTMENT_COUNT; i++) {
        tempN2[i] = getCompartmentN2Pressure(i);
        tempHe[i] = getCompartmentHePressure(i);
    }
    
    const float surfacePressure = _seaLevelAtmosphericPressure;
    
    // 3. 从深到浅推演每一个站
    for (int stopIdx = firstStopIdx; stopIdx >= 1 && _decoSequence.stopCount < MAX_DECO_STOPS; stopIdx--) {
        float currentStopDepth = (float)decoStopDepths[stopIdx];
        float currentStopPressure = calculateHydrostaticPressureFromDepth(currentStopDepth);
        
        // [核心修复] 上帝视角预测：在这个深度，我该换什么气？
        int simGasIndex = getBestGasForDepth(currentStopDepth);
        if (simGasIndex < 0) simGasIndex = _activeGasIndex; // 兜底
        Gas simGas = getGas(simGasIndex);
        
        // 下一站参数
        float nextStopDepth = (float)decoStopDepths[stopIdx - 1];
        float nextStopPressure = (nextStopDepth <= 0) ? surfacePressure : calculateHydrostaticPressureFromDepth(nextStopDepth);
        
        // 计算下一站 GF 渐变
        float nextStopGF = _gfHigh;
        if (_gfLowDepthPressure > surfacePressure) {
            nextStopGF = _gfHigh - ((_gfHigh - _gfLow) / (_gfLowDepthPressure - surfacePressure) * (nextStopPressure - surfacePressure));
        }
        nextStopGF = constrain(nextStopGF, _gfLow, _gfHigh);
        
        // 4. 计算本站停留时间 (传入独立 N2/He 状态和预测气体)
        int stopTimeSec = calculateDecoStopDuration(currentStopPressure, nextStopPressure, nextStopGF, tempN2, tempHe, simGas);

        if (stopTimeSec > 0) {
            _decoSequence.stops[_decoSequence.stopCount].depth = currentStopDepth;
            _decoSequence.stops[_decoSequence.stopCount].totalTime = stopTimeSec;
            _decoSequence.stops[_decoSequence.stopCount].remainingTime = stopTimeSec;
            _decoSequence.stops[_decoSequence.stopCount].isCompleted = false;
            _decoSequence.stops[_decoSequence.stopCount].targetGF = nextStopGF;
            _decoSequence.stopCount++;

            // 5. [核心修复] 双轨独立衰减模拟
            float lungN2 = simGas.nitrogenFraction * (currentStopPressure - _waterVapourPressureCorrection);
            float lungHe = simGas.heliumFraction   * (currentStopPressure - _waterVapourPressureCorrection);

            for (int i = 0; i < COMPARTMENT_COUNT; i++) {
                tempN2[i] = calculateCompartmentInertGasPartialPressure(stopTimeSec, getCompartmentHalfTimeInSeconds(i), tempN2[i], lungN2);
                tempHe[i] = calculateCompartmentInertGasPartialPressure(stopTimeSec, getCompartmentHeHalfTimeInSeconds(i), tempHe[i], lungHe);
            }
        }
    }
    
    // 初始化当前站状态
    if (_decoSequence.stopCount > 0) {
        _decoSequence.currentStopIdx = 0;
        _effectiveCeiling = _decoSequence.stops[0].depth;
        _lastGeneratedFirstStopDepth = _decoSequence.stops[0].depth;
        _lastDecoRegenMillis = rt_tick_get();
    } else {
        _decoSequence.currentStopIdx = -1;
        _effectiveCeiling = 0.0f;
    }
}

/**
 * @brief 使用二分法求解减压站停留时间 (完美解决双气体超越方程)
 */
int Buhlmann::calculateDecoStopDuration(float stopPressure, float nextStopPressure, float nextStopGF, float tempN2[], float tempHe[], Gas simGas) {
    float lungN2 = simGas.nitrogenFraction * (stopPressure - _waterVapourPressureCorrection);
    float lungHe = simGas.heliumFraction   * (stopPressure - _waterVapourPressureCorrection);
    
    int maxStopTimeSec = 0;
    
    for (int i = 0; i < COMPARTMENT_COUNT; i++) {
        // 先进行粗筛：在这个站待无限久，能过下一站的关吗？
        float a_inf = (_aValuesNitrogen[i] * lungN2 + _aValuesHelium[i] * lungHe) / (lungN2 + lungHe + 0.0001f);
        float b_inf = (_bValuesNitrogen[i] * lungN2 + _bValuesHelium[i] * lungHe) / (lungN2 + lungHe + 0.0001f);
        float nextStopPressureBar = nextStopPressure / 1000.0f;
        float pTol_inf = (a_inf * nextStopGF + ((nextStopPressureBar * (nextStopGF - nextStopGF * b_inf + b_inf)) / b_inf)) * 1000.0f;
        
        // 如果当前载荷已经过关，无需停留
        float currentTotal = tempN2[i] + tempHe[i];
        if (currentTotal <= pTol_inf && currentTotal <= ((_aValuesNitrogen[i]*nextStopGF + nextStopPressureBar/b_inf)*1000.0f)) {
            continue; // 直接测试下一个舱
        }

        // [核心算法：二分查找法] 寻找刚好的达标时间
        double t_low = 0.0;
        double t_high = 999.0 * 60.0; // 最大允许 999 分钟
        double t_ans = 0.0;

        // 20次二分迭代，可以将 999 分钟的时间误差缩小到 ~0.05秒！性能开销极低。
        for (int iter = 0; iter < 20; iter++) {
            double t_mid = (t_low + t_high) / 2.0;
            
            // 计算经过 t_mid 秒后的各自载荷
            float simN2 = calculateCompartmentInertGasPartialPressure(t_mid, getCompartmentHalfTimeInSeconds(i), tempN2[i], lungN2);
            float simHe = calculateCompartmentInertGasPartialPressure(t_mid, getCompartmentHeHalfTimeInSeconds(i), tempHe[i], lungHe);
            float simTotal = simN2 + simHe;
            
            // 动态重算该时刻的 M 值加权系数
            float a = (_aValuesNitrogen[i] * simN2 + _aValuesHelium[i] * simHe) / (simTotal + 0.0001f);
            float b = (_bValuesNitrogen[i] * simN2 + _bValuesHelium[i] * simHe) / (simTotal + 0.0001f);
            
            float pTolBar = a * nextStopGF + ((nextStopPressureBar * (nextStopGF - nextStopGF * b + b)) / b);
            float pTol = pTolBar * 1000.0f;
            
            if (simTotal > pTol) {
                t_low = t_mid; // 还没排够，延长时间下限
            } else {
                t_high = t_mid; // 排够了，尝试缩短时间上限
                t_ans = t_mid;
            }
        }
        
        if ((int)t_ans > maxStopTimeSec) {
            maxStopTimeSec = (int)t_ans;
        }
    }
    
    return maxStopTimeSec;
}

// 只负责判断站切换，不处理时间
void Buhlmann::updateCurrentDecoStopSwitchOnly(float currentDepth) {
	if (_decoSequence.currentStopIdx < 0 || _decoSequence.currentStopIdx >= _decoSequence.stopCount) {
		return;
	}

	DecoStop& currentStop = _decoSequence.stops[_decoSequence.currentStopIdx];
	float depthDiff = fabs(currentDepth - currentStop.depth);

	// 在±1.5米窗口内，检查是否完成
	if (depthDiff <= 1.5f && currentStop.remainingTime <= 0) {
		currentStop.isCompleted = true;
		_decoSequence.currentStopIdx++;

		if (_decoSequence.currentStopIdx >= _decoSequence.stopCount) {
			_decoSequence.currentStopIdx = -1;
			_effectiveCeiling = 0.0f;
			_isMissedDeco = false;
		} else {
			_effectiveCeiling = _decoSequence.stops[_decoSequence.currentStopIdx].depth;
			_isMissedDeco = false;
		}
	}
}

// 更新当前减压站状态（停留有效性校验，±1.5米窗口）
void Buhlmann::updateCurrentDecoStop(float currentDepth, unsigned int timeSpentInLevel) {
	if (_decoSequence.currentStopIdx < 0 || _decoSequence.currentStopIdx >= _decoSequence.stopCount) {
		return;
	}
	
	DecoStop& currentStop = _decoSequence.stops[_decoSequence.currentStopIdx];
	float depthDiff = fabs(currentDepth - currentStop.depth);
	
	// 1. 有效停留：在±1.5米内，减少剩余时间
	if (depthDiff <= 1.5f) {
		// 减少剩余时间（但不超过已用时间）
		if (currentStop.remainingTime > (int)timeSpentInLevel) {
			currentStop.remainingTime -= timeSpentInLevel;
		} else {
			currentStop.remainingTime = 0;
		}
		
		// 2. 该站完成：切换到下一站，更新有效天花板
		if (currentStop.remainingTime == 0) {
			currentStop.isCompleted = true;
			_decoSequence.currentStopIdx++;
			
			// 3. 所有站完成：解除锁定
			if (_decoSequence.currentStopIdx >= _decoSequence.stopCount) {
				_decoSequence.currentStopIdx = -1;
				_effectiveCeiling = 0.0f;
				_isMissedDeco = false;
			} else {
				// 4. 未完成：锁定有效天花板为下一站深度
				_effectiveCeiling = _decoSequence.stops[_decoSequence.currentStopIdx].depth;
				_isMissedDeco = false;  // 完成当前站，取消违规标志
			}
		}
	}
	// 注意：超出±1.5米窗口时不累加时间，但也不重置已完成状态（无惩罚设计）
}

// 判断是否跳过当前减压站（违规）
bool Buhlmann::isDecoStopMissed(float currentDepth) {
	if (_decoSequence.currentStopIdx < 0 || _decoSequence.currentStopIdx >= _decoSequence.stopCount) {
		return false;
	}
	
	DecoStop& currentStop = _decoSequence.stops[_decoSequence.currentStopIdx];
	
	// 向上跳过：当前深度 < 当前站深度 - 1.5米，且未完成该站
	// 注意：向下移动（currentDepth > currentStop.depth + 1.5f）不算违规，可能是波浪/意外
	if (currentDepth < (currentStop.depth - 1.5f) && !currentStop.isCompleted) {
		return true;
	}
	
	return false;
}

// 重新计算当前减压站的剩余时间（基于当前真实的组织分压状态）
void Buhlmann::recalculateCurrentDecoStopDuration() {
    if (_decoSequence.currentStopIdx < 0 || _decoSequence.currentStopIdx >= _decoSequence.stopCount) {
        return;
    }

    DecoStop& currentStop = _decoSequence.stops[_decoSequence.currentStopIdx];
    float stopPressure = calculateHydrostaticPressureFromDepth(currentStop.depth);
    float nextStopDepth = currentStop.depth - 3.0f;
    float nextStopPressure = (nextStopDepth <= 0) ? _seaLevelAtmosphericPressure : calculateHydrostaticPressureFromDepth(nextStopDepth);

    // 获取当前真实的双轨载荷
    float tempN2[COMPARTMENT_COUNT];
    float tempHe[COMPARTMENT_COUNT];
    for (int i = 0; i < COMPARTMENT_COUNT; i++) {
        tempN2[i] = getCompartmentN2Pressure(i);
        tempHe[i] = getCompartmentHePressure(i);
    }
    
    // 获取当前站应该吸的气
    int simGasIndex = getBestGasForDepth(currentStop.depth);
    if (simGasIndex < 0) simGasIndex = _activeGasIndex;

    int requiredSec = calculateDecoStopDuration(stopPressure, nextStopPressure, currentStop.targetGF, tempN2, tempHe, getGas(simGasIndex));
    
    currentStop.totalTime = requiredSec;
    currentStop.remainingTime = requiredSec; // 因为是基于"当前"瞬间的重算，所需即剩余
    if (currentStop.remainingTime < 0) currentStop.remainingTime = 0;

    _lastDynamicRequiredSeconds = currentStop.remainingTime;
    _lastDynamicStopDepthMeters = currentStop.depth;
    _lastDynamicCalcMillis = rt_tick_get();
}

/**
 * @brief 计算首停深度（基于 Erik Baker 深停理论）
 * @param currentDepth 当前深度（米）
 * @param gfLow GF Low 值
 * @return 首停深度（米）
 * @details 基于 GF Low 计算所有舱的允许上升压力，取最严格的
 */
float Buhlmann::calculateFirstStopDepthWithGF(float currentDepth, float gfLow) {
    // 1) 基于 GF Low 计算所有舱的允许上升压力，取最严格的（最深的）
    float maxAllowedP_low = _seaLevelAtmosphericPressure;
    for (int i = 0; i < COMPARTMENT_COUNT; ++i) {
        float compartmentPressure = getCompartmentPartialPressure(i);
        float allowedAscendPressure = getAscendToPartialPressureForCompartmentWithGF(
            i, compartmentPressure, gfLow);
        if (allowedAscendPressure > maxAllowedP_low) maxAllowedP_low = allowedAscendPressure;
    }
    float ceilingDepthLow = calculateDepthFromPressure(maxAllowedP_low);

    // 2) 向上取整到 3m 阶梯
    float firstStopDepth = 3.0f;  // 最小首停深度
    if (ceilingDepthLow > 3.0f) {
        // 向上取整到 3m 阶梯
        firstStopDepth = ceil(ceilingDepthLow / 3.0f) * 3.0f;
    }

    // 3) 基本边界保护：不能超过当前深度 - 3m
    float maxAllowedFirst = currentDepth - 3.0f;
    if (firstStopDepth > maxAllowedFirst) {
        if (maxAllowedFirst < 3.0f) firstStopDepth = 3.0f;
        else firstStopDepth = maxAllowedFirst;
    }

    return firstStopDepth;
}

// ========== 立即打印请求接口实现 ==========
void Buhlmann::requestImmediatePrint() {
	_requestImmediatePrint = true;
}

bool Buhlmann::consumeImmediatePrintRequest() {
	bool v = _requestImmediatePrint;
	_requestImmediatePrint = false;
	return v;
}

/**
 * GF 渐变计算测试函数
 * 用于验证 calculateCurrentGF() 函数的正确性
 */
void Buhlmann::testGFCalculation() {
#ifdef RT_USING_SERIAL
    rt_kprintf("===== GF 渐变计算测试 =====\n");

    float firstStop = 21.0f;
    float finalStop = 3.0f;

    rt_kprintf("GF Low: %.2f, GF High: %.2f\n", _gfLow, _gfHigh);
    rt_kprintf("首停: %.1fm, 最终停: %.1fm\n", firstStop, finalStop);
    rt_kprintf("深度(m)  计算GF   预期GF   状态\n");
    rt_kprintf("─────────────────────────────────\n");

    for (float depth = firstStop; depth >= finalStop; depth -= 3.0f) {
        float calculatedGF = calculateCurrentGF(depth, firstStop, finalStop);

        // 计算预期值
        float ratio = (firstStop - depth) / (firstStop - finalStop);
        float expectedGF = _gfLow + (_gfHigh - _gfLow) * ratio;

        bool isCorrect = fabs(calculatedGF - expectedGF) < 0.001f;

        rt_kprintf("%5.1f    %.3f    %.3f    %s\n",
                      depth, calculatedGF, expectedGF,
                      isCorrect ? "OK" : "FAIL");
    }
    rt_kprintf("=============================\n");
#endif
}

// ========== 单位切换功能实现 ==========

void Buhlmann::setUnitMetric(bool isMetric) {
	_isUnitMetric = isMetric;
}

bool Buhlmann::isUnitMetric() {
	return _isUnitMetric;
}

float Buhlmann::convertDepthForDisplay(float depthInMeters) {
	if (_isUnitMetric) {
		return depthInMeters;
	} else {
		return depthInMeters * 3.3f;  // 米转英尺
	}
}

const char* Buhlmann::getDepthUnitString() {
	return _isUnitMetric ? "m" : "ft";
}
