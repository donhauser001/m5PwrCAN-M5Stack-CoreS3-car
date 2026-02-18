/**
 * auto_tune.cpp — 自动 PID 网格搜索 (20 组, 每组 2 次)
 *
 * 在已知最优 Kp=17.5, Kd=3.0 附近做小范围网格扫描:
 *   Kp: 16.0, 17.0, 17.5, 18.0, 19.0  (5 档)
 *   Kd: 2.5, 2.75, 3.0, 3.25          (4 档)
 *   共 20 组, 每组 2 次取较差值 (保守评分)
 *
 * 关键: Ki 保留 0.5 (不再清零), 与实际运行条件一致
 * 流程: 启动平衡 → 等跌倒(或超时15s) → 记录 → 等2s → 下一次/下一组
 */

#include "auto_tune.h"
#include "config.h"
#include "globals.h"
#include "imu_balance.h"
#include "can_motor.h"
#include "web_control.h"
#include <Arduino.h>

static const float KP_LIST[] = {16.0f, 17.0f, 17.5f, 18.0f, 19.0f};
static const float KD_LIST[] = {2.5f, 2.75f, 3.0f, 3.25f};
static const int   KP_N     = sizeof(KP_LIST) / sizeof(KP_LIST[0]);
static const int   KD_N     = sizeof(KD_LIST) / sizeof(KD_LIST[0]);
static const int   COMBOS   = KP_N * KD_N;  // 20
static const int   TRIALS   = 2;
static const int   TOTAL_RUNS = COMBOS * TRIALS;  // 40

static const unsigned long WAIT_MS    = 2000;
static const unsigned long MAX_RUN_MS = 15000;

static struct {
    bool  active;
    int   combo;          // 当前组合 0..19
    int   trial;          // 当前试验 0..1
    unsigned long trialScores[2];
    unsigned long comboScores[20]; // 每组的最终评分 (两次中取较差值)
    unsigned long runStart;
    unsigned long fallTime;
    bool  running;
    bool  waiting;
    float savedKp, savedKd, savedKi;
    int   bestCombo;
    unsigned long bestScore;
    int   runsDone;
} at;

static float kpOf(int c) { return KP_LIST[c / KD_N]; }
static float kdOf(int c) { return KD_LIST[c % KD_N]; }

static void sendStatus(const char* extra) {
    char msg[256];
    snprintf(msg, sizeof(msg),
             "AT,Grid,Kp%.1f/Kd%.2f,%d/%d,Kp%.1f/Kd%.2f,%lu,%d/%d,%s",
             kpOf(at.combo), kdOf(at.combo),
             at.trial + 1, TRIALS,
             kpOf(at.bestCombo), kdOf(at.bestCombo), at.bestScore,
             at.runsDone, TOTAL_RUNS, extra);
    webBroadcastText(msg);
}

static void sendPidToWeb() {
    char msg[64];
    snprintf(msg, sizeof(msg), "P,%.1f,%.1f,%.2f", Kp, Ki, Kd);
    webBroadcastText(msg);
}

static void startTrial() {
    Kp = kpOf(at.combo);
    Kd = kdOf(at.combo);
    sendPidToWeb();

    diagMode    = true;
    fallen      = false;
    stableCount = STABLE_HOLD_COUNT;

    if (getMotorMode() != MODE_CURRENT)
        setMotorModeAll(MODE_CURRENT);

    activateBalance();
    pidIntegral = 12.0f;

    at.runStart = millis();
    at.running  = true;
    at.waiting  = false;

    sendStatus("RUN");
}

void autoTuneStart() {
    at.active    = true;
    at.savedKp   = Kp;
    at.savedKd   = Kd;
    at.savedKi   = Ki;
    at.running   = false;
    at.waiting   = false;
    at.combo     = 0;
    at.trial     = 0;
    at.bestCombo = 0;
    at.bestScore = 0;
    at.runsDone  = 0;
    memset(at.trialScores, 0, sizeof(at.trialScores));
    memset(at.comboScores, 0, sizeof(at.comboScores));

    startTrial();
}

void autoTuneStop() {
    if (!at.active) return;
    at.active  = false;
    at.running = false;
    at.waiting = false;
    Kp = at.savedKp;
    Kd = at.savedKd;
    Ki = at.savedKi;
    stopMotors();
    diagMode = true;
    fallen   = false;
    sendPidToWeb();
    sendStatus("STOP");
}

bool isAutoTuning() { return at.active; }

void autoTuneUpdate() {
    if (!at.active) return;
    unsigned long now = millis();

    if (at.running) {
        if (fallen) {
            at.trialScores[at.trial] = now - at.runStart;
            at.fallTime = now;
            at.running  = false;
            at.waiting  = true;
            char buf[32];
            snprintf(buf, sizeof(buf), "%lums", at.trialScores[at.trial]);
            sendStatus(buf);
        } else if ((now - at.runStart) >= MAX_RUN_MS) {
            at.trialScores[at.trial] = MAX_RUN_MS;
            diagMode = true;
            fallen   = false;
            stopMotors();
            pidIntegral = 0;
            at.fallTime = now;
            at.running  = false;
            at.waiting  = true;
            sendStatus("15s!");
        }
        return;
    }

    if (at.waiting) {
        if ((now - at.fallTime) < WAIT_MS) return;
        at.waiting = false;
        at.runsDone++;

        at.trial++;
        if (at.trial >= TRIALS) {
            unsigned long worst = at.trialScores[0];
            for (int i = 1; i < TRIALS; i++)
                if (at.trialScores[i] < worst) worst = at.trialScores[i];
            at.comboScores[at.combo] = worst;

            char buf[64];
            snprintf(buf, sizeof(buf), "=Kp%.1f/Kd%.2f:%lums",
                     kpOf(at.combo), kdOf(at.combo), worst);
            sendStatus(buf);

            if (worst > at.bestScore) {
                at.bestScore = worst;
                at.bestCombo = at.combo;
            }

            if (worst >= MAX_RUN_MS) {
                Kp = kpOf(at.bestCombo);
                Kd = kdOf(at.bestCombo);
                Ki = at.savedKi;
                at.active  = false;
                at.running = false;
                sendPidToWeb();
                sendStatus("DONE");
                return;
            }

            at.combo++;
            at.trial = 0;
            memset(at.trialScores, 0, sizeof(at.trialScores));

            if (at.combo >= COMBOS) {
                Kp = kpOf(at.bestCombo);
                Kd = kdOf(at.bestCombo);
                Ki = at.savedKi;
                at.active  = false;
                at.running = false;
                sendPidToWeb();
                sendStatus("DONE");
                return;
            }
        }

        startTrial();
    }
}
