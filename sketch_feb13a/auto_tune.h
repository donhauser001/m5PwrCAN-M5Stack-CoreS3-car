#pragma once
/**
 * auto_tune.h — 自动参数扫描: 跌倒→等待→调参→重启, 逐步找到最优 Kp/Kd
 */

void autoTuneStart();
void autoTuneStop();
void autoTuneUpdate();   // 每次 loop 调用
bool isAutoTuning();
