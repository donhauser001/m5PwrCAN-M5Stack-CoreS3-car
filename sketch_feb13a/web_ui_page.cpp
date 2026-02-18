#include "web_ui_page.h"

const char WEB_INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>自平衡机器人控制台</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
:root {
  --bg: #0b1219;
  --panel: #151f2b;
  --panel-soft: #101925;
  --line: #253244;
  --text: #d9e7f3;
  --muted: #8aa0b5;
  --ok: #57c785;
  --warn: #f0b429;
  --err: #ef6b6b;
  --a1: #4fc3f7;
  --a2: #ffc857;
  --a3: #f687b3;
  --a4: #72e4b6;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  font-family: "Segoe UI", "PingFang SC", "Microsoft YaHei", sans-serif;
  color: var(--text);
  background: radial-gradient(circle at top right, #172334 0%, #0b1219 56%);
  min-height: 100vh;
}
.container {
  display: grid;
  grid-template-columns: 360px 1fr;
  gap: 10px;
  padding: 10px;
  height: 100vh;
}
.left-panel, .right-panel {
  min-height: 0;
  display: flex;
  flex-direction: column;
  gap: 10px;
}
.left-panel { overflow-y: auto; }
.panel {
  background: linear-gradient(180deg, var(--panel) 0%, var(--panel-soft) 100%);
  border: 1px solid var(--line);
  border-radius: 10px;
  padding: 10px;
}
h2 { font-size: 18px; color: var(--a1); letter-spacing: 0.5px; }
h3 {
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-size: 13px;
  color: var(--a1);
  border-bottom: 1px solid rgba(255,255,255,0.06);
  padding-bottom: 6px;
  margin-bottom: 8px;
}
.status-bar { display: flex; flex-wrap: wrap; gap: 8px; font-size: 12px; }
.badge {
  padding: 4px 10px;
  border-radius: 14px;
  border: 1px solid var(--line);
  background: #1a2431;
  color: var(--muted);
}
.badge.on { color: #08140f; background: var(--ok); border-color: transparent; }
.badge.warn { color: #1f1400; background: var(--warn); border-color: transparent; }
.badge.err { color: #2a0303; background: var(--err); border-color: transparent; }

.view-3d {
  position: relative;
  height: 230px;
  border: 1px solid var(--line);
  border-radius: 8px;
  overflow: hidden;
  background: radial-gradient(circle at center, #17243a 0%, #0a1118 70%);
}
canvas#c3d { width: 100%; height: 100%; display: block; }
.overlay-info {
  position: absolute;
  top: 8px;
  left: 8px;
  font-size: 12px;
  color: #b7cada;
  line-height: 1.35;
  background: rgba(0,0,0,0.25);
  border: 1px solid rgba(255,255,255,0.08);
  border-radius: 6px;
  padding: 4px 6px;
}
.key-hint {
  position: absolute;
  right: 8px;
  bottom: 8px;
  font-size: 11px;
  text-align: right;
  color: #8da2b8;
}

.kpi-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 6px;
  font-size: 12px;
}
.kpi {
  border: 1px solid var(--line);
  border-radius: 6px;
  padding: 6px;
  background: rgba(0,0,0,0.15);
}
.kpi .label { color: var(--muted); font-size: 11px; }
.kpi .value { color: #f7f3d8; font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; font-size: 14px; }

.pid-ctrl { display: flex; flex-direction: column; gap: 8px; }
.slider-row { display: flex; align-items: center; gap: 8px; }
.slider-row label { width: 24px; color: var(--muted); font-size: 12px; }
.slider-row input { flex: 1; accent-color: var(--a1); }
.slider-row .val { width: 50px; text-align: right; color: #f6cf65; font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; font-size: 12px; }

.btn-row { display: flex; flex-wrap: wrap; gap: 8px; }
.btn {
  flex: 1;
  min-width: 88px;
  padding: 10px 12px;
  border: none;
  border-radius: 6px;
  font-size: 13px;
  font-weight: 700;
  color: white;
  cursor: pointer;
}
.btn:active { transform: scale(0.98); }
.btn.cal { background: #e69500; }
.btn.run { background: #268a5f; }
.btn.stop { background: #be4040; }
.btn.sm {
  flex: initial;
  min-width: initial;
  padding: 4px 8px;
  font-size: 11px;
  background: #2a3748;
}

.pc-keys { display: grid; grid-template-columns: repeat(3, 34px); gap: 5px; justify-content: center; }
.key {
  width: 34px;
  height: 34px;
  border-radius: 6px;
  background: #293647;
  color: #c8d7e6;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 12px;
}
.key.active { background: var(--a1); color: #00111e; }
.key.w { grid-column: 2; }
.key.a { grid-column: 1; grid-row: 2; }
.key.s { grid-column: 2; grid-row: 2; }
.key.d { grid-column: 3; grid-row: 2; }

.header-strip {
  display: grid;
  grid-template-columns: repeat(5, minmax(0, 1fr));
  gap: 8px;
  font-size: 12px;
}
.header-cell {
  border: 1px solid var(--line);
  border-radius: 8px;
  background: rgba(0,0,0,0.2);
  padding: 6px;
}
.header-cell .l { color: var(--muted); font-size: 11px; }
.header-cell .v { font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; color: #ffe59c; }

.chart-grid {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 8px;
}
.chart-box {
  border: 1px solid var(--line);
  border-radius: 8px;
  background: rgba(0,0,0,0.18);
  padding: 6px;
  height: 180px;
}
.chart-title {
  font-size: 11px;
  color: var(--muted);
  margin-bottom: 4px;
}

.monitor-panel { flex: 1; min-height: 0; }
.monitor-table-wrap {
  height: 100%;
  border: 1px solid var(--line);
  border-radius: 8px;
  overflow: auto;
  background: #090f16;
}
.monitor-table {
  width: 100%;
  border-collapse: collapse;
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
  font-size: 11px;
  min-width: 1360px;
}
.monitor-table th {
  position: sticky;
  top: 0;
  z-index: 1;
  background: #151f2b;
  color: var(--muted);
  padding: 4px 6px;
  text-align: right;
  border-bottom: 1px solid var(--line);
  white-space: nowrap;
}
.monitor-table td {
  padding: 3px 6px;
  text-align: right;
  border-bottom: 1px solid #101824;
  white-space: nowrap;
}
.monitor-table tr:hover { background: #101923; }
tr.run-marker td {
  text-align: left;
  font-weight: 700;
  color: #d6e7ff;
  background: #1d2b3e;
  border-bottom: 1px solid #2c415d;
}
tr.run-end td {
  color: #ffe0a8;
  background: #2a1f13;
  border-bottom: 1px solid #5d4329;
}

::-webkit-scrollbar { width: 8px; height: 8px; }
::-webkit-scrollbar-thumb { background: #2a3748; border-radius: 5px; }
::-webkit-scrollbar-track { background: #0e1621; }

@media (max-width: 1180px) {
  .container { grid-template-columns: 1fr; height: auto; }
  .left-panel { max-height: none; overflow: visible; }
  .chart-grid { grid-template-columns: 1fr; }
  .header-strip { grid-template-columns: repeat(2, minmax(0, 1fr)); }
}
</style>
</head>
<body>
<div class="container">
  <div class="left-panel">
    <h2>自平衡机器人</h2>

    <div class="status-bar">
      <div id="ws-st" class="badge">未连接</div>
      <div id="bot-st" class="badge">待机</div>
      <div id="mode-st" class="badge">DIAG</div>
      <div id="bat-st" class="badge">-- V</div>
    </div>

    <div class="view-3d">
      <canvas id="c3d"></canvas>
      <div class="overlay-info">
        Pitch: <span id="val-p">0.0</span>°<br>
        Roll: <span id="val-r">0.0</span>°<br>
        Yaw: <span id="val-y">0.0</span>°<br>
        Gyro: <span id="val-g">0.0</span>°/s
      </div>
      <div class="key-hint">WASD 控制<br>Q 启动 / E 急停</div>
    </div>

    <div class="pc-keys">
      <div class="key w" id="k-w">W</div>
      <div class="key a" id="k-a">A</div>
      <div class="key s" id="k-s">S</div>
      <div class="key d" id="k-d">D</div>
    </div>

    <div class="panel">
      <h3>实时关键量</h3>
      <div class="kpi-grid">
        <div class="kpi"><div class="label">整车重量</div><div class="value" id="kpi-weight">-- g</div></div>
        <div class="kpi"><div class="label">车轮直径</div><div class="value" id="kpi-wheel">-- mm</div></div>
        <div class="kpi"><div class="label">目标角</div><div class="value" id="kpi-target">0.00°</div></div>
        <div class="kpi"><div class="label">PID 输出</div><div class="value" id="kpi-pid">0.0</div></div>
        <div class="kpi"><div class="label">线速度</div><div class="value" id="kpi-speed">0.000 m/s</div></div>
        <div class="kpi"><div class="label">路程</div><div class="value" id="kpi-dist">0.000 m</div></div>
        <div class="kpi"><div class="label">Cmd RPM</div><div class="value" id="kpi-cmd">0 / 0</div></div>
        <div class="kpi"><div class="label">Act RPM</div><div class="value" id="kpi-act">0 / 0</div></div>
      </div>
    </div>

    <div class="panel">
      <h3>PID 参数调节</h3>
      <div class="pid-ctrl">
        <div class="slider-row"><label>Kp</label><input type="range" id="rp" min="0" max="40" step="0.5" value="12"><div class="val" id="vp">12.0</div></div>
        <div class="slider-row"><label>Ki</label><input type="range" id="ri" min="0" max="10" step="0.1" value="0"><div class="val" id="vi">0.0</div></div>
        <div class="slider-row"><label>Kd</label><input type="range" id="rd" min="0" max="10" step="0.05" value="0.5"><div class="val" id="vd">0.5</div></div>
      </div>
    </div>

    <div class="btn-row">
      <button class="btn run" onclick="send('S')">启动平衡</button>
      <button class="btn stop" onclick="send('E')">急停</button>
      <button class="btn cal" id="at-btn" onclick="toggleAutoTune()">自动调参</button>
    </div>

    <div class="panel" id="at-panel" style="display:none">
      <h3>自动调参 <button class="btn sm" onclick="send('AX')">停止</button></h3>
      <div style="font-size:12px;line-height:1.6">
        <div>阶段: <span id="at-phase" style="color:var(--a1)">--</span></div>
        <div>当前值: <span id="at-cur" style="color:var(--a2)">--</span> &nbsp; 试次: <span id="at-trial">--</span></div>
        <div>最佳值: <span id="at-best" style="color:var(--ok)">--</span> &nbsp; 存活: <span id="at-score" style="color:var(--ok)">--</span></div>
        <div>进度: <span id="at-prog">--</span></div>
        <div style="margin-top:4px">
          <div style="height:6px;background:#1a2431;border-radius:3px;overflow:hidden">
            <div id="at-bar" style="height:100%;width:0%;background:var(--a1);transition:width 0.3s"></div>
          </div>
        </div>
        <div id="at-log" style="margin-top:6px;max-height:100px;overflow-y:auto;font-family:monospace;font-size:11px;color:var(--muted)"></div>
      </div>
    </div>
  </div>

  <div class="right-panel">
    <div class="header-strip">
      <div class="header-cell"><div class="l">K</div><div class="v" id="k-val">--</div></div>
      <div class="header-cell"><div class="l">目标角</div><div class="v" id="t-val">0.0°</div></div>
      <div class="header-cell"><div class="l">电压 (R/L)</div><div class="v" id="vin-val">-- / -- V</div></div>
      <div class="header-cell"><div class="l">电流 (R/L)</div><div class="v" id="cur-val">-- / -- mA</div></div>
      <div class="header-cell"><div class="l">温度 (R/L)</div><div class="v" id="tmp-val">-- / -- C</div></div>
    </div>

    <div class="panel">
      <h3>多维实时可视化</h3>
      <div class="chart-grid">
        <div class="chart-box"><div class="chart-title">姿态: Pitch / Target / Roll</div><canvas id="chart-angle"></canvas></div>
        <div class="chart-box"><div class="chart-title">控制: PID / Gyro / Speed</div><canvas id="chart-ctrl"></canvas></div>
        <div class="chart-box"><div class="chart-title">电机: Cmd/Act RPM + Current</div><canvas id="chart-motor"></canvas></div>
      </div>
    </div>

    <div class="panel monitor-panel">
      <h3>
        闭环日志（每次站立到跌倒完整保留）
        <div>
          <button class="btn sm" onclick="copyTable()">复制全部</button>
          <button class="btn sm" onclick="copyLastRun()">复制最近闭环</button>
          <button class="btn sm" onclick="clearTable()">清空</button>
        </div>
      </h3>
      <div class="monitor-table-wrap" id="table-wrap">
        <table class="monitor-table" id="monitor-table">
          <thead>
          <tr>
            <th>#</th><th>Run</th><th>t(ms)</th><th>age(ms)</th><th>dt(ms)</th>
            <th>Pitch</th><th>Target</th><th>Gyro</th><th>PID</th><th>dtCtrl</th>
            <th>uRaw</th><th>uClamp</th><th>uDZ</th><th>uSendR</th><th>uSendL</th>
            <th>CmdR</th><th>CmdL</th><th>ActR</th><th>ActL</th>
            <th>Speed(m/s)</th><th>Dist(m)</th>
            <th>VinR</th><th>VinL</th><th>CurR</th><th>CurL</th>
            <th>TmpR</th><th>TmpL</th><th>State</th>
          </tr>
          </thead>
          <tbody id="monitor-body"></tbody>
        </table>
      </div>
    </div>
  </div>
</div>

<script>
const tableBody = document.getElementById('monitor-body');
const tableWrap = document.getElementById('table-wrap');
const wsSt = document.getElementById('ws-st');
const botSt = document.getElementById('bot-st');
const modeSt = document.getElementById('mode-st');

const state = {
  pitch: 0, roll: 0, yaw: 0, gyro: 0,
  target: 0, pid: 0,
  fallen: false, diag: true, bench: false,
  cmdR: 0, cmdL: 0, actR: 0, actL: 0,
  speed: 0, dist: 0,
  vinR: 0, vinL: 0, curR: 0, curL: 0, tmpR: 0, tmpL: 0
};

let ws;
let rowCount = 0;
let runId = 0;
let runActive = false;
let runPendingStart = false;
let runStartMs = 0;
let runStartDist = 0;
let runLastMs = 0;
let lastClosedRunId = 0;
let runStats = null;
let prevDiag = true;
let prevFallen = false;
let prevBench = false;
let pendingCloseReason = '';

function fmt(v, n=2) {
  const x = Number(v);
  return Number.isFinite(x) ? x.toFixed(n) : '--';
}

function addRunMarker(text, run, cls='run-marker') {
  const tr = document.createElement('tr');
  tr.className = `${cls} run-${run}`;
  tr.innerHTML = `<td colspan="28">${text}</td>`;
  tableBody.appendChild(tr);
}

function startRun() {
  runId += 1;
  runActive = true;
  pendingCloseReason = '';
  runPendingStart = true;
  runStartMs = 0;
  runStartDist = state.dist;
  runLastMs = 0;
  runStats = { maxPitch: 0, maxPid: 0, maxSpeed: 0, rows: 0 };
  addRunMarker(`RUN ${runId} START | waiting first sample... | pitch=${fmt(state.pitch,2)} | pid=${fmt(state.pid,1)}`, runId);
}

function closeRun(reason, ms = 0) {
  if (!runActive) return;
  const endMs = runLastMs || ms || runStartMs;
  const duration = Math.max(0, endMs - runStartMs);
  const deltaDist = state.dist - runStartDist;
  addRunMarker(
    `RUN ${runId} END(${reason}) | duration=${duration}ms | rows=${runStats.rows} | max|pitch|=${fmt(runStats.maxPitch,2)}deg | max|pid|=${fmt(runStats.maxPid,1)} | max|speed|=${fmt(runStats.maxSpeed,3)}m/s | dist=${fmt(deltaDist,4)}m`,
    runId,
    'run-marker run-end'
  );
  runActive = false;
  runPendingStart = false;
  pendingCloseReason = '';
  lastClosedRunId = runId;
  runStats = null;
}

function addTableRow(s) {
  if (!runActive) return;
  if (runPendingStart) {
    runStartMs = s.ms;
    runLastMs = s.ms;
    runStartDist = s.dist;
    runPendingStart = false;
  }

  rowCount += 1;
  runStats.rows += 1;
  runStats.maxPitch = Math.max(runStats.maxPitch, Math.abs(s.pitch));
  runStats.maxPid = Math.max(runStats.maxPid, Math.abs(s.pid));
  runStats.maxSpeed = Math.max(runStats.maxSpeed, Math.abs(s.speed));

  const ageMs = runStartMs ? (s.ms - runStartMs) : 0;
  const dtMs = runLastMs ? (s.ms - runLastMs) : 0;
  runLastMs = s.ms;

  const tr = document.createElement('tr');
  tr.className = `run-${runId}`;
  tr.innerHTML = `
    <td>${rowCount}</td>
    <td>${runId}</td>
    <td>${s.ms}</td>
    <td>${ageMs}</td>
    <td>${dtMs}</td>
    <td>${fmt(s.pitch,2)}</td>
    <td>${fmt(s.target,2)}</td>
    <td>${fmt(s.gyro,2)}</td>
    <td>${fmt(s.pid,1)}</td>
    <td>${fmt(s.ctrlDt,2)}</td>
    <td>${fmt(s.uRaw,1)}</td>
    <td>${fmt(s.uClamp,1)}</td>
    <td>${fmt(s.uDz,0)}</td>
    <td>${s.uSendR}</td>
    <td>${s.uSendL}</td>
    <td>${s.cmdR}</td>
    <td>${s.cmdL}</td>
    <td>${s.actR}</td>
    <td>${s.actL}</td>
    <td>${fmt(s.speed,3)}</td>
    <td>${fmt(s.dist,4)}</td>
    <td>${fmt(s.vinR,2)}</td>
    <td>${fmt(s.vinL,2)}</td>
    <td>${fmt(s.curR,1)}</td>
    <td>${fmt(s.curL,1)}</td>
    <td>${fmt(s.tmpR,1)}</td>
    <td>${fmt(s.tmpL,1)}</td>
    <td>${s.bench ? 'BENCH' : (s.fallen ? 'FALLEN' : (s.diag ? 'DIAG' : 'RUN'))}</td>
  `;
  const stickBottom = tableWrap.scrollTop + tableWrap.clientHeight >= tableWrap.scrollHeight - 32;
  tableBody.appendChild(tr);
  if (stickBottom) tableWrap.scrollTop = tableWrap.scrollHeight;
}

function clearTable() {
  tableBody.innerHTML = '';
  rowCount = 0;
  runId = 0;
  runActive = false;
  runPendingStart = false;
  pendingCloseReason = '';
  runStats = null;
  lastClosedRunId = 0;
}

function copyTextWithFallback(text) {
  if (navigator.clipboard && window.isSecureContext) {
    return navigator.clipboard.writeText(text).then(() => true).catch(() => false);
  }

  return new Promise((resolve) => {
    try {
      const ta = document.createElement('textarea');
      ta.value = text;
      ta.setAttribute('readonly', '');
      ta.style.position = 'fixed';
      ta.style.top = '-9999px';
      ta.style.left = '-9999px';
      document.body.appendChild(ta);
      ta.focus();
      ta.select();
      const ok = document.execCommand('copy');
      document.body.removeChild(ta);
      resolve(!!ok);
    } catch (e) {
      resolve(false);
    }
  });
}

function copyRows(rows, title) {
  if (!rows.length) return;
  const out = [];
  if (title) out.push(`# ${title}`);
  rows.forEach((tr) => {
    if (tr.classList.contains('run-marker')) {
      out.push(`\n${tr.innerText}\n`);
    } else {
      out.push(Array.from(tr.children).map((td) => td.innerText).join('\t'));
    }
  });
  const text = out.join('\n');
  copyTextWithFallback(text).then((ok) => {
    if (ok) {
      alert('已复制到剪贴板');
    } else {
      alert('当前环境禁止剪贴板访问，请在 HTTPS 页面或现代浏览器中重试');
    }
  });
}

function copyTable() {
  copyRows(Array.from(tableBody.children), 'All runs');
}

function copyLastRun() {
  const target = runActive ? runId : lastClosedRunId;
  if (!target) return;
  const rows = Array.from(tableBody.children).filter((tr) => tr.classList.contains(`run-${target}`));
  copyRows(rows, `Run ${target}`);
}

const HISTORY = 180;
const labels = Array(HISTORY).fill('');

function mkChart(id, datasets, yAxes) {
  const ctx = document.getElementById(id).getContext('2d');
  return new Chart(ctx, {
    type: 'line',
    data: { labels: labels.slice(), datasets },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: false,
      interaction: { intersect: false, mode: 'index' },
      scales: yAxes,
      plugins: {
        legend: { labels: { color: '#9ab1c6', boxWidth: 10, font: { size: 10 } } }
      },
      elements: { point: { radius: 0 } }
    }
  });
}

const chartAngle = mkChart('chart-angle', [
  { label: 'Pitch', data: Array(HISTORY).fill(0), borderColor: '#4fc3f7', tension: 0.25, borderWidth: 1.6, yAxisID: 'y' },
  { label: 'Target', data: Array(HISTORY).fill(0), borderColor: '#ffc857', tension: 0.25, borderWidth: 1.4, yAxisID: 'y' },
  { label: 'Roll', data: Array(HISTORY).fill(0), borderColor: '#f687b3', tension: 0.25, borderWidth: 1.2, yAxisID: 'y' }
], {
  x: { display: false },
  y: { grid: { color: '#1f2b3a' }, ticks: { color: '#6f8499', font: { size: 10 } } }
});

const chartCtrl = mkChart('chart-ctrl', [
  { label: 'PID', data: Array(HISTORY).fill(0), borderColor: '#ffc857', tension: 0.2, borderWidth: 1.5, yAxisID: 'y' },
  { label: 'Gyro', data: Array(HISTORY).fill(0), borderColor: '#4fc3f7', tension: 0.2, borderWidth: 1.2, yAxisID: 'y' },
  { label: 'Speed(m/s)', data: Array(HISTORY).fill(0), borderColor: '#72e4b6', tension: 0.2, borderWidth: 1.2, yAxisID: 'y1' }
], {
  x: { display: false },
  y: { grid: { color: '#1f2b3a' }, ticks: { color: '#6f8499', font: { size: 10 } } },
  y1: { position: 'right', grid: { drawOnChartArea: false }, ticks: { color: '#6f8499', font: { size: 10 } } }
});

const chartMotor = mkChart('chart-motor', [
  { label: 'CmdR', data: Array(HISTORY).fill(0), borderColor: '#4fc3f7', tension: 0.2, borderWidth: 1.3, yAxisID: 'y' },
  { label: 'CmdL', data: Array(HISTORY).fill(0), borderColor: '#7cc8ff', tension: 0.2, borderWidth: 1.3, yAxisID: 'y' },
  { label: 'ActR', data: Array(HISTORY).fill(0), borderColor: '#ffc857', tension: 0.2, borderWidth: 1.2, yAxisID: 'y' },
  { label: 'ActL', data: Array(HISTORY).fill(0), borderColor: '#f6a64e', tension: 0.2, borderWidth: 1.2, yAxisID: 'y' },
  { label: 'CurR(mA)', data: Array(HISTORY).fill(0), borderColor: '#72e4b6', tension: 0.2, borderWidth: 1.1, yAxisID: 'y1' },
  { label: 'CurL(mA)', data: Array(HISTORY).fill(0), borderColor: '#3fcf9d', tension: 0.2, borderWidth: 1.1, yAxisID: 'y1' }
], {
  x: { display: false },
  y: { grid: { color: '#1f2b3a' }, ticks: { color: '#6f8499', font: { size: 10 } } },
  y1: { position: 'right', grid: { drawOnChartArea: false }, ticks: { color: '#6f8499', font: { size: 10 } } }
});

function pushChart(chart, values) {
  chart.data.datasets.forEach((ds, i) => {
    ds.data.push(values[i]);
    ds.data.shift();
  });
  chart.update('none');
}

function updateIndicators() {
  document.getElementById('val-p').textContent = fmt(state.pitch, 2);
  document.getElementById('val-r').textContent = fmt(state.roll, 2);
  document.getElementById('val-y').textContent = fmt(state.yaw, 1);
  document.getElementById('val-g').textContent = fmt(state.gyro, 2);

  document.getElementById('kpi-target').textContent = `${fmt(state.target,2)}°`;
  document.getElementById('kpi-pid').textContent = fmt(state.pid, 1);
  document.getElementById('kpi-speed').textContent = `${fmt(state.speed,3)} m/s`;
  document.getElementById('kpi-dist').textContent = `${fmt(state.dist,4)} m`;
  document.getElementById('kpi-cmd').textContent = `${state.cmdR} / ${state.cmdL}`;
  document.getElementById('kpi-act').textContent = `${state.actR} / ${state.actL}`;

  document.getElementById('t-val').textContent = `${fmt(state.target, 2)}°`;
  document.getElementById('vin-val').textContent = `${fmt(state.vinR,2)} / ${fmt(state.vinL,2)} V`;
  document.getElementById('cur-val').textContent = `${fmt(state.curR,1)} / ${fmt(state.curL,1)} mA`;
  document.getElementById('tmp-val').textContent = `${fmt(state.tmpR,1)} / ${fmt(state.tmpL,1)} C`;

  if (state.bench) {
    botSt.textContent = '架空阶跃';
    botSt.className = 'badge warn';
  } else if (state.fallen) {
    botSt.textContent = '已跌倒';
    botSt.className = 'badge err';
  } else if (state.diag) {
    botSt.textContent = '待启动';
    botSt.className = 'badge warn';
  } else {
    botSt.textContent = '平衡中';
    botSt.className = 'badge on';
  }

  modeSt.textContent = state.bench ? 'BENCH' : (state.diag ? 'DIAG' : 'RUN');
  modeSt.className = state.bench ? 'badge warn' : (state.diag ? 'badge warn' : 'badge on');
}

const canvas = document.getElementById('c3d');
const ctx3d = canvas.getContext('2d');
let W, H, CX, CY;
const P3 = (x,y,z) => ({x,y,z});
const rotX = (p, a) => { const c=Math.cos(a), s=Math.sin(a); return P3(p.x, p.y*c - p.z*s, p.y*s + p.z*c); };
const rotY = (p, a) => { const c=Math.cos(a), s=Math.sin(a); return P3(p.x*c + p.z*s, p.y, -p.x*s + p.z*c); };
const rotZ = (p, a) => { const c=Math.cos(a), s=Math.sin(a); return P3(p.x*c - p.y*s, p.x*s + p.y*c, p.z); };
const project = (p) => { const scale = 300 / (420 + p.z); return { x: CX + p.x * scale, y: CY - p.y * scale }; };
const box = [
  P3(-30, 40, 10), P3(30, 40, 10), P3(30, -40, 10), P3(-30, -40, 10),
  P3(-30, 40, -10), P3(30, 40, -10), P3(30, -40, -10), P3(-30, -40, -10)
];
const axes = [
  {s:P3(0,0,0), e:P3(60,0,0), c:'#f44336'},
  {s:P3(0,0,0), e:P3(0,60,0), c:'#4caf50'},
  {s:P3(0,0,0), e:P3(0,0,60), c:'#2196f3'}
];

function resize3d() {
  W = canvas.width = canvas.parentElement.clientWidth;
  H = canvas.height = canvas.parentElement.clientHeight;
  CX = W * 0.5;
  CY = H * 0.52;
}
window.addEventListener('resize', resize3d);
resize3d();

function drawLine(p1, p2, color, width=1) {
  ctx3d.strokeStyle = color;
  ctx3d.lineWidth = width;
  ctx3d.beginPath();
  ctx3d.moveTo(p1.x, p1.y);
  ctx3d.lineTo(p2.x, p2.y);
  ctx3d.stroke();
}

function render3d() {
  ctx3d.clearRect(0, 0, W, H);
  const p = state.pitch * Math.PI / 180;
  const r = state.roll * Math.PI / 180;
  const y = state.yaw * Math.PI / 180;
  const transform = (pt) => {
    let t = rotX(pt, p);
    t = rotZ(t, -r);
    t = rotY(t, -y);   // 取反：rotY 实现为顺时针，陀螺 yaw 为右手逆时针，符号对齐
    t = rotX(t, 0.28);
    t = rotY(t, 0.48);
    return t;
  };
  axes.forEach((axis) => drawLine(project(transform(axis.s)), project(transform(axis.e)), axis.c, 2));
  const v = box.map((pt) => project(transform(pt)));
  [[0,1],[1,2],[2,3],[3,0],[4,5],[5,6],[6,7],[7,4],[0,4],[1,5],[2,6],[3,7]]
    .forEach((pair) => drawLine(v[pair[0]], v[pair[1]], '#6fc8ff', 1.1));
  requestAnimationFrame(render3d);
}
render3d();

function connect() {
  ws = new WebSocket(`ws://${location.hostname}:81/`);
  ws.onopen = () => {
    wsSt.textContent = '已连接';
    wsSt.className = 'badge on';
  };
  ws.onclose = () => {
    wsSt.textContent = '断开重连...';
    wsSt.className = 'badge err';
    setTimeout(connect, 1500);
  };

  ws.onmessage = (e) => {
    const d = e.data;

    if (d.startsWith('A,')) {
      const p = d.split(',');
      state.pitch = parseFloat(p[1]);
      state.roll = parseFloat(p[2]);
      state.yaw = parseFloat(p[3]);
      state.pid = parseFloat(p[4]);
      state.fallen = p[5] === '1';
      state.diag = p[6] === '1';
      state.target = parseFloat(p[7]);
      state.gyro = parseFloat(p[8]);
      state.speed = parseFloat(p[9]) / 1000.0;
      state.dist = parseFloat(p[10]) / 1000.0;
      state.cmdR = parseInt(p[11]);
      state.cmdL = parseInt(p[12]);
      state.actR = parseInt(p[13]);
      state.actL = parseInt(p[14]);
      state.bench = p[15] === '1';

      // BENCH 模式也记录完整 run（用于静态阶跃实验）
      if (!runActive && state.bench && !prevBench) {
        startRun();
      }
      if (!runActive && !state.bench && !state.diag && !state.fallen && (prevDiag || prevFallen)) {
        startRun();
      }

      // BENCH 退出时结束 run
      if (runActive && prevBench && !state.bench) {
        pendingCloseReason = 'BENCH_STOP';
      }
      // 平衡 run 的结束条件（不含 BENCH）
      if (runActive && !state.bench && (state.fallen || state.diag) && !(prevDiag && state.diag)) {
        pendingCloseReason = state.fallen ? 'FALLEN' : 'STOP';
      }
      prevDiag = state.diag;
      prevFallen = state.fallen;
      prevBench = state.bench;

      pushChart(chartAngle, [state.pitch, state.target, state.roll]);
      pushChart(chartCtrl, [state.pid, state.gyro, state.speed]);
      pushChart(chartMotor, [state.cmdR, state.cmdL, state.actR, state.actL, state.curR, state.curL]);

      updateIndicators();

    } else if (d.startsWith('T,')) {
      const p = d.split(',');
      const sample = {
        ms: parseInt(p[1]),
        pitch: parseFloat(p[2]),
        target: parseFloat(p[3]),
        gyro: parseFloat(p[4]),
        pid: parseFloat(p[5]),
        cmdR: parseInt(p[6]),
        cmdL: parseInt(p[7]),
        actR: parseInt(p[8]),
        actL: parseInt(p[9]),
        speed: parseFloat(p[10]),
        dist: parseFloat(p[11]),
        vinR: parseFloat(p[12]),
        vinL: parseFloat(p[13]),
        curR: parseFloat(p[14]),
        curL: parseFloat(p[15]),
        tmpR: parseFloat(p[16]),
        tmpL: parseFloat(p[17]),
        fallen: p[18] === '1',
        diag: p[19] === '1',
        ctrlDt: p[20] ? parseFloat(p[20]) : 0,
        uRaw: p[21] ? parseFloat(p[21]) : 0,
        uClamp: p[22] ? parseFloat(p[22]) : 0,
        uDz: p[23] ? parseFloat(p[23]) : 0,
        uSendR: p[24] ? parseInt(p[24]) : 0,
        uSendL: p[25] ? parseInt(p[25]) : 0,
        bench: p[26] === '1'
      };
      state.vinR = sample.vinR;
      state.vinL = sample.vinL;
      state.curR = sample.curR;
      state.curL = sample.curL;
      state.tmpR = sample.tmpR;
      state.tmpL = sample.tmpL;
      state.bench = sample.bench;
      addTableRow(sample);

      // 先写入最后一个采样点，再结束本次闭环，避免终点样本丢失
      if (runActive && pendingCloseReason && (sample.fallen || sample.diag)) {
        closeRun(pendingCloseReason, sample.ms);
      }
      updateIndicators();

    } else if (d.startsWith('M,')) {
      const p = d.split(',');
      state.cmdR = parseInt(p[1]);
      state.cmdL = parseInt(p[2]);
      state.actR = parseInt(p[3]);
      state.actL = parseInt(p[4]);
      state.vinR = parseFloat(p[5]);
      state.vinL = parseFloat(p[6]);
      state.curR = parseFloat(p[7]);
      state.curL = parseFloat(p[8]);
      state.tmpR = parseFloat(p[9]);
      state.tmpL = parseFloat(p[10]);

      const vAvg = (state.vinR + state.vinL) * 0.5;
      document.getElementById('bat-st').textContent = `${fmt(vAvg,2)} V`;
      document.getElementById('bat-st').className = vAvg < 10 ? 'badge err' : 'badge';
      updateIndicators();

    } else if (d.startsWith('P,')) {
      const p = d.split(',');
      updatePidUI(p[1], p[2], p[3]);

    } else if (d.startsWith('AT,')) {
      handleAutoTune(d);

    } else if (d.startsWith('C,')) {
      const p = d.split(',');
      document.getElementById('kpi-weight').textContent = `${p[1]} g`;
      document.getElementById('kpi-wheel').textContent = `${p[2]} mm`;
    }
  };
}

function send(msg) {
  if (ws && ws.readyState === 1) ws.send(msg);
}


const rp = document.getElementById('rp');
const ri = document.getElementById('ri');
const rd = document.getElementById('rd');
const vp = document.getElementById('vp');
const vi = document.getElementById('vi');
const vd = document.getElementById('vd');

function updatePidUI(p, i, d) {
  rp.value = p; vp.textContent = parseFloat(p).toFixed(1);
  ri.value = i; vi.textContent = parseFloat(i).toFixed(1);
  rd.value = d; vd.textContent = parseFloat(d).toFixed(2);
  document.getElementById('k-val').textContent = `${parseFloat(p).toFixed(1)}, ${parseFloat(i).toFixed(1)}, ${parseFloat(d).toFixed(2)}`;
}

function onPidChange() {
  vp.textContent = parseFloat(rp.value).toFixed(1);
  vi.textContent = parseFloat(ri.value).toFixed(1);
  vd.textContent = parseFloat(rd.value).toFixed(2);
  send(`P,${rp.value},${ri.value},${rd.value}`);
}
rp.oninput = ri.oninput = rd.oninput = onPidChange;

const keys = { w:0, a:0, s:0, d:0 };
const keyMap = {
  'w':'w', 'a':'a', 's':'s', 'd':'d',
  'ArrowUp':'w', 'ArrowDown':'s', 'ArrowLeft':'a', 'ArrowRight':'d'
};

function updateKeys() {
  let x = 0, y = 0;
  if (keys.w) y += 1;
  if (keys.s) y -= 1;
  if (keys.a) x -= 1;
  if (keys.d) x += 1;

  document.getElementById('k-w').className = 'key w' + (keys.w ? ' active' : '');
  document.getElementById('k-s').className = 'key s' + (keys.s ? ' active' : '');
  document.getElementById('k-a').className = 'key a' + (keys.a ? ' active' : '');
  document.getElementById('k-d').className = 'key d' + (keys.d ? ' active' : '');

  const joyX = x * 60;
  const joyY = y * 80;
  document.getElementById('t-val').textContent = `${(joyY * 4.0 / 100.0).toFixed(2)}°`;
  send(`J,${joyX},${joyY}`);
}

window.addEventListener('keydown', (e) => {
  const k = keyMap[e.key] || keyMap[e.key.toLowerCase()];
  if (k) {
    if (!keys[k]) {
      keys[k] = 1;
      updateKeys();
    }
  } else if (e.key === 'q' || e.key === 'Q') {
    send('S');
  } else if (e.key === 'e' || e.key === 'E' || e.code === 'Space') {
    send('E');
  }
});

window.addEventListener('keyup', (e) => {
  const k = keyMap[e.key] || keyMap[e.key.toLowerCase()];
  if (k) {
    keys[k] = 0;
    updateKeys();
  }
});

let autoTuning = false;

function toggleAutoTune() {
  if (autoTuning) {
    send('AX');
  } else {
    if (!confirm('开始自动调参？机器人将自动反复启停约5-10分钟。')) return;
    send('AT');
  }
}

function handleAutoTune(d) {
  // AT,phase,curVal,trial/total,bestVal,bestMedian,done/total,status
  const p = d.substring(3).split(',');
  const phase = p[0];
  const curVal = p[1];
  const trial = p[2];
  const bestVal = p[3];
  const bestMedian = p[4];
  const progress = p[5];
  const status = p[6] || '';

  const panel = document.getElementById('at-panel');
  const btn = document.getElementById('at-btn');

  if (status === 'DONE' || status === 'STOP') {
    autoTuning = false;
    btn.textContent = '自动调参';
    btn.className = 'btn cal';
    if (status === 'DONE') {
      panel.style.display = '';
      document.getElementById('at-phase').textContent = '完成!';
      document.getElementById('at-cur').textContent = '--';
      document.getElementById('at-trial').textContent = '--';
      document.getElementById('at-bar').style.width = '100%';
      addAtLog('调参完成: Kp=' + parseFloat(rp.value).toFixed(1) + ' Kd=' + parseFloat(rd.value).toFixed(2));
    } else {
      addAtLog('已手动停止');
    }
    return;
  }

  autoTuning = true;
  btn.textContent = '停止调参';
  btn.className = 'btn stop';
  panel.style.display = '';

  const phaseMap = { 'Kp': 'Kp 粗调', 'Kd': 'Kd 粗调', 'Kp*': 'Kp 精调', 'Grid': '网格搜索' };
  document.getElementById('at-phase').textContent = phaseMap[phase] || phase;
  document.getElementById('at-cur').textContent = curVal;
  document.getElementById('at-trial').textContent = trial;
  document.getElementById('at-best').textContent = bestVal;
  document.getElementById('at-score').textContent = bestMedian + 'ms';
  document.getElementById('at-prog').textContent = progress;

  const progParts = progress.split('/');
  if (progParts.length === 2) {
    const pct = Math.min(100, (parseInt(progParts[0]) / parseInt(progParts[1])) * 100);
    document.getElementById('at-bar').style.width = pct + '%';
  }

  if (status.startsWith('=')) {
    addAtLog(phase + status);
  }
}

function addAtLog(text) {
  const log = document.getElementById('at-log');
  const line = document.createElement('div');
  line.textContent = text;
  log.appendChild(line);
  log.scrollTop = log.scrollHeight;
}

connect();
</script>
</body>
</html>
)rawliteral";
