// ============================================================
// ESP32-S3 SERVO TELEMETRY & CONTROL CENTER - DUAL SERVO ENGINE
// ============================================================

(function () {
  'use strict';

  // State Management for Dual Servos
  const state = {
    connected: false,
    wsConnected: false,
    port: 'COM3',
    ip: '192.168.4.1',
    s1: {
      pin: 1,
      currentPwm: 90,
      targetPwm: 90,
      state: 'Sẵn sàng',
      running: false,
      cycle: 0,
      cycles: 1,
      stop: 90,
      forward: 110,
      reverse: 70,
      forwardMs: 1000,
      reverseMs: 1000,
      pauseMs: 500
    },
    s2: {
      pin: 2,
      currentPwm: 90,
      targetPwm: 90,
      state: 'Sẵn sàng',
      running: false,
      cycle: 0,
      cycles: 1,
      stop: 90,
      forward: 110,
      reverse: 70,
      forwardMs: 1000,
      reverseMs: 1000,
      pauseMs: 500
    },
    ir: {
      pin: 10,
      txPin: 11,
      detected: false,
      raw: 1,
      count: 0,
      blockMs: 0,
      autoTrigger: 0,
      txEnabled: true
    },
    oscilloscopeHistory: [] // [{ t, s1Pwm, s2Pwm }]
  };

  // DOM Elements
  const el = {
    // IR Deck
    irStatusCard: document.getElementById('ir-status-card'),
    irRadarCircle: document.getElementById('ir-radar-circle'),
    irIcon: document.getElementById('ir-icon'),
    irStatusMain: document.getElementById('ir-status-main'),
    irStatusSub: document.getElementById('ir-status-sub'),
    irTagRaw: document.getElementById('ir-tag-raw'),
    irTagBlock: document.getElementById('ir-tag-block'),
    irCounterNumber: document.getElementById('ir-counter-number'),
    irTxBadge: document.getElementById('ir-tx-badge'),
    irLastTimeText: document.getElementById('ir-last-time-text'),
    btnIrReset: document.getElementById('btn-ir-reset'),
    btnIrSimulate: document.getElementById('btn-ir-simulate'),
    btnIrToggleTx: document.getElementById('btn-ir-toggle-tx'),
    btnAutoOff: document.getElementById('btn-auto-off'),
    btnAutoS1: document.getElementById('btn-auto-s1'),
    btnAutoS2: document.getElementById('btn-auto-s2'),
    btnAutoBoth: document.getElementById('btn-auto-both'),
    // Header
    pillSerial: document.getElementById('pill-serial'),
    dotSerial: document.getElementById('dot-serial'),
    valPort: document.getElementById('val-port'),
    pillWs: document.getElementById('pill-ws'),
    dotWs: document.getElementById('dot-ws'),
    valWs: document.getElementById('val-ws'),
    valIp: document.getElementById('val-ip'),
    btnReboot: document.getElementById('btn-reboot'),
    btnEstop: document.getElementById('btn-estop'),

    // Top Action Deck
    topBtnAllOpen: document.getElementById('top-btn-all-open'),
    topBtnAllClose: document.getElementById('top-btn-all-close'),
    topBtnAllStart: document.getElementById('top-btn-all-start'),
    topBtnAllStop: document.getElementById('top-btn-all-stop'),

    topBtnS1Open: document.getElementById('top-btn-s1-open'),
    topBtnS1Close: document.getElementById('top-btn-s1-close'),
    topBtnS1Start: document.getElementById('top-btn-s1-start'),
    topBtnS1Stop: document.getElementById('top-btn-s1-stop'),
    topBtnS1TestFwd: document.getElementById('top-btn-s1-test-fwd'),
    topBtnS1TestRev: document.getElementById('top-btn-s1-test-rev'),

    topBtnS2Open: document.getElementById('top-btn-s2-open'),
    topBtnS2Close: document.getElementById('top-btn-s2-close'),
    topBtnS2Start: document.getElementById('top-btn-s2-start'),
    topBtnS2Stop: document.getElementById('top-btn-s2-stop'),
    topBtnS2TestFwd: document.getElementById('top-btn-s2-test-fwd'),
    topBtnS2TestRev: document.getElementById('top-btn-s2-test-rev'),

    // Stat Cards
    badgeS1State: document.getElementById('badge-s1-state'),
    cardS1Val: document.getElementById('card-s1-val'),
    cardS1Sub: document.getElementById('card-s1-sub'),
    barS1Cycle: document.getElementById('bar-s1-cycle'),

    badgeS2State: document.getElementById('badge-s2-state'),
    cardS2Val: document.getElementById('card-s2-val'),
    cardS2Sub: document.getElementById('card-s2-sub'),
    barS2Cycle: document.getElementById('bar-s2-cycle'),
    cardClamp: document.getElementById('card-clamp'),

    // Dual Gauges
    gaugeCanvas1: document.getElementById('servoGaugeCanvas1'),
    s1GaugePwmBadge: document.getElementById('s1-gauge-pwm-badge'),
    s1GaugeAngleText: document.getElementById('s1-gauge-angle-text'),
    s1GaugeStatePill: document.getElementById('s1-gauge-state-pill'),

    gaugeCanvas2: document.getElementById('servoGaugeCanvas2'),
    s2GaugePwmBadge: document.getElementById('s2-gauge-pwm-badge'),
    s2GaugeAngleText: document.getElementById('s2-gauge-angle-text'),
    s2GaugeStatePill: document.getElementById('s2-gauge-state-pill'),

    // State machine & Scope
    stateTimerDisplay: document.getElementById('state-timer-display'),
    scopeCanvas: document.getElementById('scopeCanvas'),
    stateNodes: {
      idle: document.getElementById('node-idle'),
      fwd: document.getElementById('node-fwd'),
      pause1: document.getElementById('node-pause1'),
      rev: document.getElementById('node-rev'),
      complete: document.getElementById('node-complete')
    },

    // Servo 1 Deck
    btnS1Open: document.getElementById('btn-s1-open'),
    btnS1Close: document.getElementById('btn-s1-close'),
    btnS1Start: document.getElementById('btn-s1-start'),
    btnS1Stop: document.getElementById('btn-s1-stop'),
    btnS1TestFwd: document.getElementById('btn-s1-test-fwd'),
    btnS1TestRev: document.getElementById('btn-s1-test-rev'),
    sliderStop1: document.getElementById('slider-stop-1'),
    s1CalibValDisplay: document.getElementById('s1-calib-val-display'),
    btnS1CalibTest: document.getElementById('btn-s1-calib-test'),
    btnS1CalibSave: document.getElementById('btn-s1-calib-save'),

    // Servo 2 Deck
    btnS2Open: document.getElementById('btn-s2-open'),
    btnS2Close: document.getElementById('btn-s2-close'),
    btnS2Start: document.getElementById('btn-s2-start'),
    btnS2Stop: document.getElementById('btn-s2-stop'),
    btnS2TestFwd: document.getElementById('btn-s2-test-fwd'),
    btnS2TestRev: document.getElementById('btn-s2-test-rev'),
    sliderStop2: document.getElementById('slider-stop-2'),
    s2CalibValDisplay: document.getElementById('s2-calib-val-display'),
    btnS2CalibTest: document.getElementById('btn-s2-calib-test'),
    btnS2CalibSave: document.getElementById('btn-s2-calib-save'),

    // Config Tabs & Form
    tabBtnS1: document.getElementById('tab-btn-s1'),
    tabBtnS2: document.getElementById('tab-btn-s2'),
    cfgPaneS1: document.getElementById('cfg-pane-s1'),
    cfgPaneS2: document.getElementById('cfg-pane-s2'),

    cfgFwdSpeed1: document.getElementById('cfg-fwd-speed-1'),
    cfgFwdSpeedVal1: document.getElementById('cfg-fwd-speed-val-1'),
    cfgRevSpeed1: document.getElementById('cfg-rev-speed-1'),
    cfgRevSpeedVal1: document.getElementById('cfg-rev-speed-val-1'),
    cfgFwdMs1: document.getElementById('cfg-fwd-ms-1'),
    cfgRevMs1: document.getElementById('cfg-rev-ms-1'),
    cfgPauseMs1: document.getElementById('cfg-pause-ms-1'),
    cfgCycles1: document.getElementById('cfg-cycles-1'),

    cfgFwdSpeed2: document.getElementById('cfg-fwd-speed-2'),
    cfgFwdSpeedVal2: document.getElementById('cfg-fwd-speed-val-2'),
    cfgRevSpeed2: document.getElementById('cfg-rev-speed-2'),
    cfgRevSpeedVal2: document.getElementById('cfg-rev-speed-val-2'),
    cfgFwdMs2: document.getElementById('cfg-fwd-ms-2'),
    cfgRevMs2: document.getElementById('cfg-rev-ms-2'),
    cfgPauseMs2: document.getElementById('cfg-pause-ms-2'),
    cfgCycles2: document.getElementById('cfg-cycles-2'),

    configForm: document.getElementById('config-form'),
    btnResetConfig: document.getElementById('btn-reset-config'),

    // HIL & Terminal
    btnRunHil: document.getElementById('btn-run-hil'),
    hilSummaryText: document.getElementById('hil-summary-text'),
    termBody: document.getElementById('term-body'),
    termInput: document.getElementById('term-input'),
    btnTermSend: document.getElementById('btn-term-send'),
    chkAutoscroll: document.getElementById('chk-autoscroll'),
    btnClearTerm: document.getElementById('btn-clear-term'),
    toastContainer: document.getElementById('toast-container')
  };

  // Toast Notification System
  function showToast(message, type = 'info', duration = 3000) {
    if (!el.toastContainer) return;
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.innerHTML = `<span>${message}</span>`;
    el.toastContainer.appendChild(toast);
    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'translateX(30px)';
      toast.style.transition = 'all 0.3s ease';
      setTimeout(() => toast.remove(), 300);
    }, duration);
  }

  // ============================================================
  // WEBSOCKET COMMUNICATION
  // ============================================================
  let ws = null;
  let reconnectTimer = null;

  function connectWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;

    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
      state.wsConnected = true;
      if (el.valWs) el.valWs.textContent = 'ONLINE';
      if (el.dotWs) el.dotWs.className = 'pulse-dot green';
      addTerminalLine('[SYSTEM] WebSocket Stream Connected.', 'system');
      if (reconnectTimer) {
        clearInterval(reconnectTimer);
        reconnectTimer = null;
      }
    };

    ws.onclose = () => {
      state.wsConnected = false;
      if (el.valWs) el.valWs.textContent = 'RECONNECTING';
      if (el.dotWs) el.dotWs.className = 'pulse-dot amber';
      if (!reconnectTimer) {
        reconnectTimer = setInterval(connectWebSocket, 2000);
      }
    };

    ws.onerror = () => {
      state.wsConnected = false;
    };

    ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        handleWsMessage(msg);
      } catch (e) {
        console.error('Error parsing WS message:', e);
      }
    };
  }

  function sendCommand(cmd) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ action: 'cmd', cmd: cmd }));
      addTerminalLine(`>> ${cmd}`, 'system');
    } else {
      fetch('/api/cmd', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ cmd: cmd })
      })
      .then(res => res.json())
      .then(res => {
        addTerminalLine(`<< ${JSON.stringify(res)}`, 'esp');
      })
      .catch(err => {
        showToast('Lỗi gửi lệnh: ' + err.message, 'error');
      });
    }
  }

  function handleWsMessage(msg) {
    if (msg.type === 'telemetry') {
      updateTelemetry(msg.data);
    } else if (msg.type === 'log') {
      let level = msg.level || 'esp';
      if (msg.line.includes('[SERVO') || msg.line.includes('[MOTION]')) level = 'system';
      if (msg.line.includes('[ERROR]') || msg.line.includes('error')) level = 'error';
      addTerminalLine(msg.line, level);
    } else if (msg.type === 'hil_event') {
      handleHilEvent(msg);
    } else if (msg.type === 'cmd_response') {
      if (msg.res && msg.res.message) {
        showToast(msg.res.message, msg.res.ok ? 'success' : 'error');
      }
    }
  }

  // ============================================================
  // TELEMETRY & UI UPDATER
  // ============================================================
  let configLoadedOnce = false;

  function updateTelemetry(data) {
    state.connected = data.connected !== false;
    state.port = data.port || 'COM3';
    state.ip = data.ip || '192.168.4.1';

    // Parse S1 data
    const s1Data = data.s1 || {
      state: data.state || 'Sẵn sàng',
      running: !!data.running,
      cycle: data.cycle || 0,
      cycles: data.cycles || 1,
      stop: data.servoStop ?? 90,
      forward: data.servoForward ?? 110,
      reverse: data.servoReverse ?? 70,
      forwardMs: data.forwardMs || 1000,
      reverseMs: data.reverseMs || 1000,
      pauseMs: data.pauseMs || 500
    };

    // Parse S2 data
    const s2Data = data.s2 || {
      state: 'Sẵn sàng',
      running: false,
      cycle: 0,
      cycles: 1,
      stop: 90,
      forward: 110,
      reverse: 70,
      forwardMs: 1000,
      reverseMs: 1000,
      pauseMs: 500
    };

    Object.assign(state.s1, s1Data);
    Object.assign(state.s2, s2Data);

    // Update connection status
    if (el.valPort) el.valPort.textContent = `${state.port} (115200)`;
    if (el.valIp) el.valIp.textContent = state.ip;
    if (el.dotSerial) {
      el.dotSerial.className = state.connected ? 'pulse-dot green' : 'pulse-dot red';
    }

    // --- Update S1 Card & Target PWM ---
    if (!state.s1.running) {
      state.s1.targetPwm = state.s1.stop;
      if (state.s1.state.includes('mở') || state.s1.state.includes('Đã mở')) {
        el.cardS1Val.textContent = 'OPENED (CLAMP)';
        el.cardS1Sub.textContent = `GPIO 1 • PWM Detached • Line Grounded (${state.s1.stop}°)`;
      } else if (state.s1.state.includes('đóng') || state.s1.state.includes('Đã đóng')) {
        el.cardS1Val.textContent = 'CLOSED (CLAMP)';
        el.cardS1Sub.textContent = `GPIO 1 • PWM Detached • Line Grounded (${state.s1.stop}°)`;
      } else {
        el.cardS1Val.textContent = 'IDLE (CLAMP)';
        el.cardS1Sub.textContent = `GPIO 1 • PWM Detached • Line Grounded (${state.s1.stop}°)`;
      }
    } else {
      if (state.s1.state.includes('thuận') || state.s1.state.includes('mở')) {
        state.s1.targetPwm = state.s1.forward;
        el.cardS1Val.textContent = 'QUAY THUẬN / MỞ';
        el.cardS1Sub.textContent = `GPIO 1 • LEDC Ch0 • ${state.s1.forward}° CCW`;
      } else if (state.s1.state.includes('nghịch') || state.s1.state.includes('đóng')) {
        state.s1.targetPwm = state.s1.reverse;
        el.cardS1Val.textContent = 'QUAY NGHỊCH / ĐÓNG';
        el.cardS1Sub.textContent = `GPIO 1 • LEDC Ch0 • ${state.s1.reverse}° CW`;
      } else {
        state.s1.targetPwm = state.s1.stop;
        el.cardS1Val.textContent = 'TẠM DỪNG';
        el.cardS1Sub.textContent = `GPIO 1 • Active Ground Clamped`;
      }
    }
    el.badgeS1State.textContent = state.s1.state.toUpperCase();
    const s1Pct = state.s1.cycles > 0 ? Math.min(100, Math.round((state.s1.cycle / state.s1.cycles) * 100)) : 0;
    el.barS1Cycle.style.width = `${s1Pct}%`;
    el.s1GaugePwmBadge.textContent = `${Math.round(state.s1.targetPwm)}° (${state.s1.state})`;

    // --- Update S2 Card & Target PWM ---
    if (!state.s2.running) {
      state.s2.targetPwm = state.s2.stop;
      if (state.s2.state.includes('mở') || state.s2.state.includes('Đã mở')) {
        el.cardS2Val.textContent = 'OPENED (CLAMP)';
        el.cardS2Sub.textContent = `GPIO 2 • PWM Detached • Line Grounded (${state.s2.stop}°)`;
      } else if (state.s2.state.includes('đóng') || state.s2.state.includes('Đã đóng')) {
        el.cardS2Val.textContent = 'CLOSED (CLAMP)';
        el.cardS2Sub.textContent = `GPIO 2 • PWM Detached • Line Grounded (${state.s2.stop}°)`;
      } else {
        el.cardS2Val.textContent = 'IDLE (CLAMP)';
        el.cardS2Sub.textContent = `GPIO 2 • PWM Detached • Line Grounded (${state.s2.stop}°)`;
      }
    } else {
      if (state.s2.state.includes('thuận') || state.s2.state.includes('mở')) {
        state.s2.targetPwm = state.s2.forward;
        el.cardS2Val.textContent = 'QUAY THUẬN / MỞ';
        el.cardS2Sub.textContent = `GPIO 2 • LEDC Ch1 • ${state.s2.forward}° CCW`;
      } else if (state.s2.state.includes('nghịch') || state.s2.state.includes('đóng')) {
        state.s2.targetPwm = state.s2.reverse;
        el.cardS2Val.textContent = 'QUAY NGHỊCH / ĐÓNG';
        el.cardS2Sub.textContent = `GPIO 2 • LEDC Ch1 • ${state.s2.reverse}° CW`;
      } else {
        state.s2.targetPwm = state.s2.stop;
        el.cardS2Val.textContent = 'TẠM DỪNG';
        el.cardS2Sub.textContent = `GPIO 2 • Active Ground Clamped`;
      }
    }
    el.badgeS2State.textContent = state.s2.state.toUpperCase();
    const s2Pct = state.s2.cycles > 0 ? Math.min(100, Math.round((state.s2.cycle / state.s2.cycles) * 100)) : 0;
    el.barS2Cycle.style.width = `${s2Pct}%`;
    el.s2GaugePwmBadge.textContent = `${Math.round(state.s2.targetPwm)}° (${state.s2.state})`;

    // Timer display text
    el.stateTimerDisplay.textContent = `S1: ${state.s1.state} | S2: ${state.s2.state}`;

    // --- Update IR Bowl Detection Deck ---
    if (data.ir) {
      const prevCount = state.ir.count;
      Object.assign(state.ir, data.ir);

      // Beam status card & radar circle
      if (el.irStatusCard && el.irRadarCircle && el.irStatusMain) {
        if (state.ir.detected) {
          el.irStatusCard.classList.add('detected');
          el.irRadarCircle.classList.add('detected');
          el.irStatusMain.classList.add('detected');
          el.irStatusMain.textContent = 'PHÁT HIỆN TÔ! (BOWL DETECTED)';
          if (el.irIcon) el.irIcon.textContent = '🍲';
          if (el.irStatusSub) el.irStatusSub.textContent = `Tô đang đi qua vùng quét cảm biến (GPIO ${state.ir.pin})`;
        } else {
          el.irStatusCard.classList.remove('detected');
          el.irRadarCircle.classList.remove('detected');
          el.irStatusMain.classList.remove('detected');
          el.irStatusMain.textContent = 'THÔNG SUỐT (CLEAR)';
          if (el.irIcon) el.irIcon.textContent = '◎';
          if (el.irStatusSub) el.irStatusSub.textContent = 'Chùm tia thông suốt - Sẵn sàng nhận diện khi tô đi qua';
        }
      }

      if (el.irTagRaw) {
        el.irTagRaw.textContent = `RAW: ${state.ir.raw === 0 ? 'LOW (0)' : 'HIGH (1)'}`;
      }
      if (el.irTagBlock) {
        el.irTagBlock.textContent = `THỜI GIAN CHE: ${state.ir.blockMs || 0} ms`;
      }

      // Counter
      if (el.irCounterNumber) {
        el.irCounterNumber.textContent = String(state.ir.count || 0).padStart(4, '0');
      }

      if (state.ir.count > prevCount && prevCount > 0) {
        if (el.irLastTimeText) {
          el.irLastTimeText.textContent = `Lần cuối: ${new Date().toLocaleTimeString()}`;
        }
        showToast(`🥣 Tô #${state.ir.count} vừa đi qua!`, 'info');
      }

      // TX LED badge
      if (el.irTxBadge) {
        if (state.ir.txEnabled) {
          el.irTxBadge.className = 'badge-mini';
          el.irTxBadge.textContent = 'TX: BẬT';
        } else {
          el.irTxBadge.className = 'badge-mini off';
          el.irTxBadge.textContent = 'TX: TẮT';
        }
      }

      // Auto trigger mode buttons
      const autoMode = state.ir.autoTrigger || 0;
      [
        { btn: el.btnAutoOff, mode: 0 },
        { btn: el.btnAutoS1, mode: 1 },
        { btn: el.btnAutoS2, mode: 2 },
        { btn: el.btnAutoBoth, mode: 3 }
      ].forEach(item => {
        if (item.btn) {
          if (item.mode === autoMode) {
            item.btn.classList.add('active');
          } else {
            item.btn.classList.remove('active');
          }
        }
      });
    }

    // Update State Machine Active Node (reflects active runner)
    updateStateMachineVisualizer();

    // Record Oscilloscope point
    const now = Date.now();
    state.oscilloscopeHistory.push({
      t: now,
      s1Pwm: state.s1.targetPwm,
      s2Pwm: state.s2.targetPwm
    });
    if (state.oscilloscopeHistory.length > 300) {
      state.oscilloscopeHistory.shift();
    }

    // Populate configuration form once
    if (!configLoadedOnce && (data.servoForward || (data.s1 && data.s1.forward))) {
      configLoadedOnce = true;
      // S1 Form
      el.sliderStop1.value = state.s1.stop;
      el.s1CalibValDisplay.textContent = `${state.s1.stop}°`;
      el.cfgFwdSpeed1.value = state.s1.forward;
      el.cfgFwdSpeedVal1.textContent = state.s1.forward;
      el.cfgRevSpeed1.value = state.s1.reverse;
      el.cfgRevSpeedVal1.textContent = state.s1.reverse;
      el.cfgFwdMs1.value = state.s1.forwardMs;
      el.cfgRevMs1.value = state.s1.reverseMs;
      el.cfgPauseMs1.value = state.s1.pauseMs;
      el.cfgCycles1.value = state.s1.cycles;

      // S2 Form
      el.sliderStop2.value = state.s2.stop;
      el.s2CalibValDisplay.textContent = `${state.s2.stop}°`;
      el.cfgFwdSpeed2.value = state.s2.forward;
      el.cfgFwdSpeedVal2.textContent = state.s2.forward;
      el.cfgRevSpeed2.value = state.s2.reverse;
      el.cfgRevSpeedVal2.textContent = state.s2.reverse;
      el.cfgFwdMs2.value = state.s2.forwardMs;
      el.cfgRevMs2.value = state.s2.reverseMs;
      el.cfgPauseMs2.value = state.s2.pauseMs;
      el.cfgCycles2.value = state.s2.cycles;
    }
  }

  function updateStateMachineVisualizer() {
    const nodes = el.stateNodes;
    Object.values(nodes).forEach(n => n && n.classList.remove('active'));

    const st1 = state.s1.state;
    const st2 = state.s2.state;
    const anyRunning = state.s1.running || state.s2.running;

    if (!anyRunning) {
      if (st1.includes('Hoàn thành') || st2.includes('Hoàn thành')) {
        nodes.complete.classList.add('active');
      } else {
        nodes.idle.classList.add('active');
      }
      return;
    }

    const activeSt = state.s1.running ? st1 : st2;
    if (activeSt.includes('thuận') || activeSt.includes('mở')) {
      nodes.fwd.classList.add('active');
    } else if (activeSt.includes('nghịch') || activeSt.includes('đóng')) {
      nodes.rev.classList.add('active');
    } else if (activeSt.includes('Tạm dừng') || activeSt.includes('dừng')) {
      nodes.pause1.classList.add('active');
    } else {
      nodes.idle.classList.add('active');
    }
  }

  // ============================================================
  // DUAL GAUGE RENDER LOOPS
  // ============================================================
  const ctx1 = el.gaugeCanvas1 ? el.gaugeCanvas1.getContext('2d') : null;
  const ctx2 = el.gaugeCanvas2 ? el.gaugeCanvas2.getContext('2d') : null;
  const scopeCtx = el.scopeCanvas ? el.scopeCanvas.getContext('2d') : null;

  function drawSingleGauge(ctx, canvas, sObj, angleTextEl, statePillEl, primaryColor, accentColor) {
    if (!ctx || !canvas) return;

    // Smooth lerp needle
    sObj.currentPwm += (sObj.targetPwm - sObj.currentPwm) * 0.15;

    const w = canvas.width;
    const h = canvas.height;
    const cx = w / 2;
    const cy = h / 2;
    const r = w * 0.40;

    ctx.clearRect(0, 0, w, h);

    // Background track ring
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, Math.PI * 2);
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
    ctx.lineWidth = 10;
    ctx.stroke();

    function degToRad(pwmDeg) {
      const angle = -140 + (pwmDeg / 180) * 280;
      return (angle - 90) * (Math.PI / 180);
    }

    // Forward Zone (0 - 88°)
    ctx.beginPath();
    ctx.arc(cx, cy, r, degToRad(0), degToRad(88));
    ctx.strokeStyle = 'rgba(0, 242, 254, 0.45)';
    ctx.lineWidth = 8;
    ctx.stroke();

    // Neutral Deadband Zone (88 - 92°)
    ctx.beginPath();
    ctx.arc(cx, cy, r, degToRad(88), degToRad(92));
    ctx.strokeStyle = '#10b981';
    ctx.lineWidth = 12;
    ctx.stroke();

    // Reverse Zone (92 - 180°)
    ctx.beginPath();
    ctx.arc(cx, cy, r, degToRad(92), degToRad(180));
    ctx.strokeStyle = 'rgba(245, 158, 11, 0.45)';
    ctx.lineWidth = 8;
    ctx.stroke();

    // Needle
    const needleRad = degToRad(sObj.currentPwm);
    const needleLen = r - 12;
    const nx = cx + needleLen * Math.cos(needleRad);
    const ny = cy + needleLen * Math.sin(needleRad);

    ctx.save();
    ctx.shadowColor = Math.abs(sObj.currentPwm - 90) < 3 ? '#10b981' : (sObj.currentPwm < 90 ? primaryColor : '#f59e0b');
    ctx.shadowBlur = 10;

    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(nx, ny);
    ctx.strokeStyle = ctx.shadowColor;
    ctx.lineWidth = 3.5;
    ctx.lineCap = 'round';
    ctx.stroke();
    ctx.restore();

    // Center Hub Ring
    ctx.beginPath();
    ctx.arc(cx, cy, 10, 0, Math.PI * 2);
    ctx.fillStyle = '#0c1220';
    ctx.fill();
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.2)';
    ctx.lineWidth = 2;
    ctx.stroke();

    // Digital Readouts
    if (angleTextEl) angleTextEl.textContent = `${Math.round(sObj.currentPwm)}°`;
    if (statePillEl) {
      if (Math.abs(sObj.currentPwm - 90) < 3) {
        statePillEl.textContent = 'CLAMP (STOP)';
        statePillEl.style.color = '#10b981';
      } else if (sObj.currentPwm < 90) {
        const pct = Math.round(((90 - sObj.currentPwm) / 90) * 100);
        statePillEl.textContent = `QUAY THUẬN (${pct}%)`;
        statePillEl.style.color = primaryColor;
      } else {
        const pct = Math.round(((sObj.currentPwm - 90) / 90) * 100);
        statePillEl.textContent = `QUAY NGHỊCH (${pct}%)`;
        statePillEl.style.color = '#f59e0b';
      }
    }
  }

  function renderDualGauges() {
    drawSingleGauge(ctx1, el.gaugeCanvas1, state.s1, el.s1GaugeAngleText, el.s1GaugeStatePill, '#00f2fe', '#00f2fe');
    drawSingleGauge(ctx2, el.gaugeCanvas2, state.s2, el.s2GaugeAngleText, el.s2GaugeStatePill, '#10b981', '#10b981');
    requestAnimationFrame(renderDualGauges);
  }

  function renderOscilloscope() {
    if (!scopeCtx || !el.scopeCanvas) return;
    const w = el.scopeCanvas.width;
    const h = el.scopeCanvas.height;

    scopeCtx.fillStyle = '#03060d';
    scopeCtx.fillRect(0, 0, w, h);

    // Draw Grid lines
    scopeCtx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
    scopeCtx.lineWidth = 1;
    for (let y = 20; y < h; y += 25) {
      scopeCtx.beginPath();
      scopeCtx.moveTo(0, y);
      scopeCtx.lineTo(w, y);
      scopeCtx.stroke();
    }
    for (let x = 30; x < w; x += 60) {
      scopeCtx.beginPath();
      scopeCtx.moveTo(x, 0);
      scopeCtx.lineTo(x, h);
      scopeCtx.stroke();
    }

    // Neutral Center Line (90°)
    const midY = h / 2;
    scopeCtx.beginPath();
    scopeCtx.setLineDash([4, 4]);
    scopeCtx.moveTo(0, midY);
    scopeCtx.lineTo(w, midY);
    scopeCtx.strokeStyle = 'rgba(255, 255, 255, 0.15)';
    scopeCtx.stroke();
    scopeCtx.setLineDash([]);

    const history = state.oscilloscopeHistory;
    if (history.length > 1) {
      const step = w / 300;

      // Plot Channel 1: Servo 1 (Cyan)
      scopeCtx.beginPath();
      for (let i = 0; i < history.length; i++) {
        const pt = history[i];
        const x = (300 - history.length + i) * step;
        const y = h - (pt.s1Pwm / 180) * h;
        if (i === 0) scopeCtx.moveTo(x, y);
        else scopeCtx.lineTo(x, y);
      }
      scopeCtx.strokeStyle = '#00f2fe';
      scopeCtx.lineWidth = 2;
      scopeCtx.stroke();

      // Plot Channel 2: Servo 2 (Emerald)
      scopeCtx.beginPath();
      for (let i = 0; i < history.length; i++) {
        const pt = history[i];
        const x = (300 - history.length + i) * step;
        const y = h - (pt.s2Pwm / 180) * h;
        if (i === 0) scopeCtx.moveTo(x, y);
        else scopeCtx.lineTo(x, y);
      }
      scopeCtx.strokeStyle = '#10b981';
      scopeCtx.lineWidth = 2;
      scopeCtx.stroke();
    }

    // Text labels
    scopeCtx.font = '9px "JetBrains Mono"';
    scopeCtx.fillStyle = '#64748b';
    scopeCtx.fillText('180° REV', 6, 14);
    scopeCtx.fillText('90° STOP', 6, midY - 4);
    scopeCtx.fillText('0° FWD', 6, h - 6);
  }

  setInterval(renderOscilloscope, 100);

  // ============================================================
  // HIL EVENT HANDLER
  // ============================================================
  function handleHilEvent(ev) {
    const b5 = document.getElementById('badge-p5');
    const b6 = document.getElementById('badge-p6');
    const b7 = document.getElementById('badge-p7');
    const b8 = document.getElementById('badge-p8');
    const b9 = document.getElementById('badge-p9');

    if (ev.phase === 'phase_5' && b5) {
      b5.className = 'phase-badge running';
      b5.textContent = 'RUNNING';
    } else if (ev.phase === 'phase_6' && b6) {
      if (b5) { b5.className = 'phase-badge pass'; b5.textContent = 'PASS'; }
      b6.className = 'phase-badge running';
      b6.textContent = 'RUNNING';
    } else if (ev.phase === 'phase_7' && b7) {
      if (b6) { b6.className = 'phase-badge pass'; b6.textContent = 'PASS'; }
      b7.className = 'phase-badge running';
      b7.textContent = 'RUNNING';
    } else if (ev.phase === 'phase_8' && b8) {
      if (b7) { b7.className = 'phase-badge pass'; b7.textContent = 'PASS'; }
      b8.className = 'phase-badge running';
      b8.textContent = 'RUNNING';
    } else if (ev.phase === 'phase_9' && b9) {
      if (b8) { b8.className = 'phase-badge pass'; b8.textContent = 'PASS'; }
      b9.className = 'phase-badge running';
      b9.textContent = 'RUNNING';
    } else if (ev.phase === 'complete') {
      ['p5', 'p6', 'p7', 'p8', 'p9'].forEach(p => {
        const b = document.getElementById(`badge-${p}`);
        if (b) { b.className = 'phase-badge pass'; b.textContent = 'PASS'; }
      });
      if (el.hilSummaryText) el.hilSummaryText.textContent = ev.message;
      showToast(ev.message, ev.status === 'PASS' ? 'success' : 'error', 5000);
    }
  }

  // ============================================================
  // TERMINAL HELPER
  // ============================================================
  function addTerminalLine(text, level = 'esp') {
    if (!el.termBody) return;
    const line = document.createElement('div');
    line.className = `term-line ${level}`;
    const timeStr = new Date().toLocaleTimeString();
    line.textContent = `[${timeStr}] ${text}`;
    el.termBody.appendChild(line);

    if (el.chkAutoscroll && el.chkAutoscroll.checked) {
      el.termBody.scrollTop = el.termBody.scrollHeight;
    }
  }

  // ============================================================
  // EVENT LISTENERS
  // ============================================================
  function initEventListeners() {
    // Master Commands
    if (el.topBtnAllOpen) el.topBtnAllOpen.addEventListener('click', () => sendCommand('ALL_OPEN'));
    if (el.topBtnAllClose) el.topBtnAllClose.addEventListener('click', () => sendCommand('ALL_CLOSE'));
    if (el.topBtnAllStart) el.topBtnAllStart.addEventListener('click', () => sendCommand('ALL_START'));
    if (el.topBtnAllStop) el.topBtnAllStop.addEventListener('click', () => sendCommand('ALL_STOP'));
    if (el.btnEstop) el.btnEstop.addEventListener('click', () => sendCommand('ALL_STOP'));

    // Servo 1 Commands (Top Deck & Panel)
    const s1OpenHandler = () => sendCommand('S1_OPEN');
    const s1CloseHandler = () => sendCommand('S1_CLOSE');
    const s1StartHandler = () => sendCommand('S1_START');
    const s1StopHandler = () => sendCommand('S1_STOP');
    const s1TestFwdHandler = () => sendCommand('S1_TEST_FWD');
    const s1TestRevHandler = () => sendCommand('S1_TEST_REV');

    if (el.topBtnS1Open) el.topBtnS1Open.addEventListener('click', s1OpenHandler);
    if (el.btnS1Open) el.btnS1Open.addEventListener('click', s1OpenHandler);
    if (el.topBtnS1Close) el.topBtnS1Close.addEventListener('click', s1CloseHandler);
    if (el.btnS1Close) el.btnS1Close.addEventListener('click', s1CloseHandler);
    if (el.topBtnS1Start) el.topBtnS1Start.addEventListener('click', s1StartHandler);
    if (el.btnS1Start) el.btnS1Start.addEventListener('click', s1StartHandler);
    if (el.topBtnS1Stop) el.topBtnS1Stop.addEventListener('click', s1StopHandler);
    if (el.btnS1Stop) el.btnS1Stop.addEventListener('click', s1StopHandler);
    if (el.topBtnS1TestFwd) el.topBtnS1TestFwd.addEventListener('click', s1TestFwdHandler);
    if (el.btnS1TestFwd) el.btnS1TestFwd.addEventListener('click', s1TestFwdHandler);
    if (el.topBtnS1TestRev) el.topBtnS1TestRev.addEventListener('click', s1TestRevHandler);
    if (el.btnS1TestRev) el.btnS1TestRev.addEventListener('click', s1TestRevHandler);

    // Servo 2 Commands (Top Deck & Panel)
    const s2OpenHandler = () => sendCommand('S2_OPEN');
    const s2CloseHandler = () => sendCommand('S2_CLOSE');
    const s2StartHandler = () => sendCommand('S2_START');
    const s2StopHandler = () => sendCommand('S2_STOP');
    const s2TestFwdHandler = () => sendCommand('S2_TEST_FWD');
    const s2TestRevHandler = () => sendCommand('S2_TEST_REV');

    if (el.topBtnS2Open) el.topBtnS2Open.addEventListener('click', s2OpenHandler);
    if (el.btnS2Open) el.btnS2Open.addEventListener('click', s2OpenHandler);
    if (el.topBtnS2Close) el.topBtnS2Close.addEventListener('click', s2CloseHandler);
    if (el.btnS2Close) el.btnS2Close.addEventListener('click', s2CloseHandler);
    if (el.topBtnS2Start) el.topBtnS2Start.addEventListener('click', s2StartHandler);
    if (el.btnS2Start) el.btnS2Start.addEventListener('click', s2StartHandler);
    if (el.topBtnS2Stop) el.topBtnS2Stop.addEventListener('click', s2StopHandler);
    if (el.btnS2Stop) el.btnS2Stop.addEventListener('click', s2StopHandler);
    if (el.topBtnS2TestFwd) el.topBtnS2TestFwd.addEventListener('click', s2TestFwdHandler);
    if (el.btnS2TestFwd) el.btnS2TestFwd.addEventListener('click', s2TestFwdHandler);
    if (el.topBtnS2TestRev) el.topBtnS2TestRev.addEventListener('click', s2TestRevHandler);
    if (el.btnS2TestRev) el.btnS2TestRev.addEventListener('click', s2TestRevHandler);

    // IR Bowl Detection Controls
    if (el.btnIrReset) {
      el.btnIrReset.addEventListener('click', () => {
        sendCommand('IR_RESET');
        showToast('Đã đặt lại bộ đếm tô về 0', 'info');
      });
    }

    if (el.btnIrSimulate) {
      el.btnIrSimulate.addEventListener('click', () => {
        fetch('/api/ir/simulate', { method: 'POST' })
          .then(res => res.json())
          .then(() => showToast('Đang kích hoạt xung mô phỏng tô đi qua...', 'info'))
          .catch(err => showToast('Lỗi: ' + err.message, 'error'));
      });
    }

    if (el.btnIrToggleTx) {
      el.btnIrToggleTx.addEventListener('click', () => {
        const newTx = !state.ir.txEnabled;
        sendCommand(newTx ? 'IR_TX_ON' : 'IR_TX_OFF');
        showToast(`Đã chuyển TX LED sang: ${newTx ? 'BẬT' : 'TẮT'}`, 'info');
      });
    }

    [
      { btn: el.btnAutoOff, mode: 0, label: 'TẮT TỰ ĐỘNG' },
      { btn: el.btnAutoS1, mode: 1, label: 'TỰ ĐỘNG MỞ SERVO 1' },
      { btn: el.btnAutoS2, mode: 2, label: 'TỰ ĐỘNG MỞ SERVO 2' },
      { btn: el.btnAutoBoth, mode: 3, label: 'TỰ ĐỘNG MỞ CẢ 2 SERVO' }
    ].forEach(item => {
      if (item.btn) {
        item.btn.addEventListener('click', () => {
          sendCommand(`IR_AUTO ${item.mode}`);
          showToast(`Chế độ cảm biến: ${item.label}`, 'success');
        });
      }
    });

    // Hard Reboot
    if (el.btnReboot) {
      el.btnReboot.addEventListener('click', () => {
        if (confirm('Bạn có chắc muốn Reset phần cứng ESP32-S3 qua DTR/RTS?')) {
          fetch('/api/reboot', { method: 'POST' });
          showToast('Đã gửi tín hiệu Hard Reset đến ESP32-S3', 'info');
        }
      });
    }

    // Stop Slider Calibration S1
    if (el.sliderStop1) {
      el.sliderStop1.addEventListener('input', (e) => {
        if (el.s1CalibValDisplay) el.s1CalibValDisplay.textContent = `${e.target.value}°`;
      });
    }
    if (el.btnS1CalibTest) {
      el.btnS1CalibTest.addEventListener('click', () => {
        const val = el.sliderStop1.value;
        sendCommand(`S1_CALIB ${val}`);
        showToast(`Đang test điểm dừng S1: ${val}°`, 'info');
      });
    }
    if (el.btnS1CalibSave) {
      el.btnS1CalibSave.addEventListener('click', () => {
        const val = el.sliderStop1.value;
        sendCommand(`CONFIG_SET stop1=${val}`);
        showToast(`Đã lưu điểm dừng S1: ${val}° vào NVS Flash`, 'success');
      });
    }

    // Stop Slider Calibration S2
    if (el.sliderStop2) {
      el.sliderStop2.addEventListener('input', (e) => {
        if (el.s2CalibValDisplay) el.s2CalibValDisplay.textContent = `${e.target.value}°`;
      });
    }
    if (el.btnS2CalibTest) {
      el.btnS2CalibTest.addEventListener('click', () => {
        const val = el.sliderStop2.value;
        sendCommand(`S2_CALIB ${val}`);
        showToast(`Đang test điểm dừng S2: ${val}°`, 'info');
      });
    }
    if (el.btnS2CalibSave) {
      el.btnS2CalibSave.addEventListener('click', () => {
        const val = el.sliderStop2.value;
        sendCommand(`CONFIG_SET stop2=${val}`);
        showToast(`Đã lưu điểm dừng S2: ${val}° vào NVS Flash`, 'success');
      });
    }

    // Config Tab Switching
    if (el.tabBtnS1 && el.tabBtnS2) {
      el.tabBtnS1.addEventListener('click', () => {
        el.tabBtnS1.classList.add('active');
        el.tabBtnS2.classList.remove('active');
        el.cfgPaneS1.style.display = 'block';
        el.cfgPaneS2.style.display = 'none';
      });
      el.tabBtnS2.addEventListener('click', () => {
        el.tabBtnS2.classList.add('active');
        el.tabBtnS1.classList.remove('active');
        el.cfgPaneS2.style.display = 'block';
        el.cfgPaneS1.style.display = 'none';
      });
    }

    // Form value bubble bindings
    if (el.cfgFwdSpeed1) el.cfgFwdSpeed1.addEventListener('input', (e) => el.cfgFwdSpeedVal1.textContent = e.target.value);
    if (el.cfgRevSpeed1) el.cfgRevSpeed1.addEventListener('input', (e) => el.cfgRevSpeedVal1.textContent = e.target.value);
    if (el.cfgFwdSpeed2) el.cfgFwdSpeed2.addEventListener('input', (e) => el.cfgFwdSpeedVal2.textContent = e.target.value);
    if (el.cfgRevSpeed2) el.cfgRevSpeed2.addEventListener('input', (e) => el.cfgRevSpeedVal2.textContent = e.target.value);

    // Save All Config Form (S1 & S2)
    if (el.configForm) {
      el.configForm.addEventListener('submit', (e) => {
        e.preventDefault();
        const stop1 = el.sliderStop1 ? el.sliderStop1.value : state.s1.stop;
        const fwd1 = el.cfgFwdSpeed1.value;
        const rev1 = el.cfgRevSpeed1.value;
        const fwdMs1 = el.cfgFwdMs1.value;
        const revMs1 = el.cfgRevMs1.value;
        const pauseMs1 = el.cfgPauseMs1.value;
        const cyc1 = el.cfgCycles1.value;

        const stop2 = el.sliderStop2 ? el.sliderStop2.value : state.s2.stop;
        const fwd2 = el.cfgFwdSpeed2.value;
        const rev2 = el.cfgRevSpeed2.value;
        const fwdMs2 = el.cfgFwdMs2.value;
        const revMs2 = el.cfgRevMs2.value;
        const pauseMs2 = el.cfgPauseMs2.value;
        const cyc2 = el.cfgCycles2.value;

        const cmd = `CONFIG_SET stop1=${stop1} forward1=${fwd1} reverse1=${rev1} forwardMs1=${fwdMs1} reverseMs1=${revMs1} pauseMs1=${pauseMs1} cycles1=${cyc1} stop2=${stop2} forward2=${fwd2} reverse2=${rev2} forwardMs2=${fwdMs2} reverseMs2=${revMs2} pauseMs2=${pauseMs2} cycles2=${cyc2}`;
        sendCommand(cmd);
        showToast('Đã lưu toàn bộ cấu hình Servo 1 & Servo 2 vào NVS Flash', 'success');
      });
    }

    // Reset Defaults
    if (el.btnResetConfig) {
      el.btnResetConfig.addEventListener('click', () => {
        if (confirm('Khôi phục cấu hình mặc định ban đầu cho cả 2 Servo?')) {
          sendCommand('RESET');
          showToast('Đã khôi phục cấu hình mặc định cả 2 Servo', 'info');
        }
      });
    }

    // Run HIL Suite
    if (el.btnRunHil) {
      el.btnRunHil.addEventListener('click', () => {
        fetch('/api/run-hil', { method: 'POST' })
          .then(res => res.json())
          .then(() => showToast('Đang thực thi HIL Test Suite trên COM3...', 'info'))
          .catch(err => showToast('Lỗi: ' + err.message, 'error'));
      });
    }

    // Terminal Input
    function handleTermSubmit() {
      const cmd = el.termInput.value.trim();
      if (cmd) {
        sendCommand(cmd);
        el.termInput.value = '';
      }
    }

    if (el.btnTermSend) el.btnTermSend.addEventListener('click', handleTermSubmit);
    if (el.termInput) {
      el.termInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') handleTermSubmit();
      });
    }
    if (el.btnClearTerm) {
      el.btnClearTerm.addEventListener('click', () => {
        el.termBody.innerHTML = '<div class="term-line system">[SYSTEM] Console Cleared.</div>';
      });
    }
  }

  // ============================================================
  // INITIALIZATION
  // ============================================================
  window.addEventListener('DOMContentLoaded', () => {
    connectWebSocket();
    initEventListeners();
    requestAnimationFrame(renderDualGauges);
  });

})();
