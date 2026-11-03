#pragma once
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"RAWLITERAL(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, interactive-widget=resizes-content">
<title>LoRa Terminal // Meshly</title>
<link rel="stylesheet" href="style.css">
</head>
<body>

<div id="app">

  <!-- ===== HEADER / STATUS BAR ===== -->
  <header id="statusbar">
    <div class="statusbar-row">
      <div class="brand">Meshly<span class="cursor">_</span></div>
      <div class="conn">
        <span id="connDot" class="dot dot-off"></span>
        <span id="connText">LINK DOWN</span>
        <button id="rangeTestToggle" class="ghost-btn">RANGE TEST</button>
      </div>
    </div>

    <div class="statusbar-row meta">
      <div class="meta-item">NODE <span id="nodeAddr">--</span></div>
      <div class="meta-item">DEST <span id="destAddr">--</span></div>
      <div class="meta-item">NET <span id="networkId">--</span></div>
      <div class="meta-item">RSSI <span id="lastRssi">--</span> dBm</div>
      <div class="meta-item">SNR <span id="lastSnr">--</span></div>
    </div>
  </header>

  <!-- ===== MESSAGE LOG (chat view) ===== -->
  <main id="terminal">
    <div id="messages"></div>
  </main>

  <!-- ===== RANGE TEST (hidden until toggled) ===== -->
  <main id="rangeTest" class="hidden">
    <div class="rt-controls">
      <label class="rt-label">INTERVAL
        <select id="rtInterval">
          <option value="500">500ms</option>
          <option value="1000" selected>1000ms</option>
          <option value="2000">2000ms</option>
        </select>
      </label>
      <button id="rtStartBtn">START</button>
    </div>

    <div class="rt-stats">
      <div class="rt-stat"><span class="rt-stat-label">SENT</span><span id="rtSent">0</span></div>
      <div class="rt-stat"><span class="rt-stat-label">RECEIVED</span><span id="rtReceived">0</span></div>
      <div class="rt-stat"><span class="rt-stat-label">LOST</span><span id="rtLost">0</span></div>
      <div class="rt-stat"><span class="rt-stat-label">LOSS %</span><span id="rtLossPct">0%</span></div>
      <div class="rt-stat"><span class="rt-stat-label">LAST RSSI</span><span id="rtLastRssi">--</span></div>
      <div class="rt-stat"><span class="rt-stat-label">AVG RSSI</span><span id="rtAvgRssi">--</span></div>
      <div class="rt-stat"><span class="rt-stat-label">LAST SNR</span><span id="rtLastSnr">--</span></div>
      <div class="rt-stat"><span class="rt-stat-label">LAST RTT</span><span id="rtLastRtt">--</span></div>
    </div>

    <div id="rtLog"></div>
  </main>

  <!-- ===== INPUT ROW (chat view only) ===== -->
  <footer id="inputbar">
    <span class="prompt">&gt;</span>
    <input id="msgInput" type="text" autocomplete="off" autocorrect="off"
           autocapitalize="off" spellcheck="false"
           placeholder="type message and press ENTER" maxlength="240">
    <button id="sendBtn">SEND</button>
  </footer>

</div>

<script src="script.js"></script>
</body>
</html>

)RAWLITERAL";

const char STYLE_CSS[] PROGMEM = R"RAWLITERAL(
/* ==========================================================
   Meshly LoRa Terminal — black/green CLI aesthetic
   Minimal, fast, no unnecessary motion.
   ========================================================== */

:root {
  --bg:        #000000;
  --fg:        #33ff33;
  --fg-dim:    #1c9c1c;
  --fg-faint:  #0e5c0e;
  --accent:    #33ff33;
  --error:     #ff4444;
  --border:    #144a14;
  --font: "JetBrains Mono", "Fira Code", "Courier New", Courier, monospace;
}

* { box-sizing: border-box; }

html, body {
  margin: 0;
  padding: 0;
  height: 100%;
  width: 100%;
  overflow: hidden;   /* the page itself never scrolls */
  position: fixed;    /* pinned to viewport — immune to keyboard scroll-into-view */
  inset: 0;
  background: var(--bg);
  color: var(--fg);
  font-family: var(--font);
  font-size: 14px;
  -webkit-tap-highlight-color: transparent;
}

body {
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 16px;
}

#app {
  display: flex;
  flex-direction: column;
  width: 100%;
  max-width: 480px;
  height: 100%;
  max-height: 640px;
  border: 1px solid var(--border);
}

/* on small screens, let the terminal use the full viewport —
   a floating bordered window isn't useful once it IS the screen */
@media (max-width: 480px) {
  body { padding: 0; }
  #app { max-width: 100%; max-height: 100%; border: none; }
}

/* ---------- header ---------- */
#statusbar {
  border-bottom: 1px solid var(--border);
  padding: 8px 12px;
  flex-shrink: 0;
}

.statusbar-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  flex-wrap: wrap;
  gap: 6px 14px;
}

.brand {
  font-weight: bold;
  letter-spacing: 1px;
}

.cursor {
  animation: blink 1s steps(1) infinite;
}
@keyframes blink { 50% { opacity: 0; } }

.conn {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 12px;
  color: var(--fg-dim);
}

.ghost-btn {
  background: transparent;
  border: 1px solid var(--fg-dim);
  color: var(--fg-dim);
  font-family: var(--font);
  font-size: 10px;
  letter-spacing: 1px;
  padding: 3px 8px;
  cursor: pointer;
}
.ghost-btn:active,
.ghost-btn.active {
  background: var(--fg-dim);
  color: #000;
}

.dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  display: inline-block;
}
.dot-off { background: var(--error); }
.dot-on  { background: var(--fg); box-shadow: 0 0 4px var(--fg); }

.meta {
  margin-top: 4px;
  font-size: 11px;
  color: var(--fg-dim);
}
.meta-item span {
  color: var(--fg);
}

/* ---------- message log ---------- */
#terminal {
  flex: 1;
  min-height: 0;
  overflow-y: auto;
  padding: 10px 12px;
}

#messages {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.msg {
  max-width: 78%;
  line-height: 1.4;
  word-wrap: break-word;
}

.msg-meta {
  font-size: 10px;
  color: var(--fg-faint);
  margin-bottom: 2px;
}

.msg-body {
  white-space: pre-wrap;
}

/* incoming: left side */
.msg.rx {
  align-self: flex-start;
  border-left: 2px solid var(--fg-dim);
  padding-left: 8px;
}

/* outgoing: right side */
.msg.tx {
  align-self: flex-end;
  text-align: right;
  border-right: 2px solid var(--fg-dim);
  padding-right: 8px;
}

.msg.tx.pending .msg-body { color: var(--fg-dim); }
.msg.tx.failed  .msg-body { color: var(--error); }
.msg.tx.failed  .msg-meta { color: var(--error); }

.msg.system {
  align-self: center;
  color: var(--fg-faint);
  font-size: 11px;
}

/* ---------- input bar ---------- */
#inputbar {
  display: flex;
  align-items: center;
  gap: 8px;
  border-top: 1px solid var(--border);
  padding: 8px 12px;
  flex-shrink: 0;
}

.prompt {
  color: var(--fg);
  font-weight: bold;
}

#msgInput {
  flex: 1;
  background: transparent;
  border: none;
  outline: none;
  color: var(--fg);
  font-family: var(--font);
  font-size: 14px;
  caret-color: var(--fg);
}

#msgInput::placeholder {
  color: var(--fg-faint);
}

#sendBtn {
  background: transparent;
  border: 1px solid var(--fg-dim);
  color: var(--fg);
  font-family: var(--font);
  font-size: 12px;
  padding: 6px 12px;
  cursor: pointer;
  letter-spacing: 1px;
}
#sendBtn:active {
  background: var(--fg-dim);
  color: #000;
}

/* ---------- scrollbar ---------- */
#terminal::-webkit-scrollbar { width: 6px; }
#terminal::-webkit-scrollbar-track { background: #000; }
#terminal::-webkit-scrollbar-thumb { background: var(--fg-faint); }

/* ---------- mobile ---------- */
@media (max-width: 480px) {
  .msg { max-width: 90%; }
  .meta { font-size: 10px; }
}

/* ---------- utility ---------- */
.hidden { display: none !important; }

/* ---------- range test panel ---------- */
#rangeTest {
  flex: 1;
  min-height: 0;
  overflow-y: auto;
  padding: 10px 12px;
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.rt-controls {
  display: flex;
  align-items: center;
  gap: 10px;
}

.rt-label {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 11px;
  color: var(--fg-dim);
}

#rtInterval {
  background: #000;
  color: var(--fg);
  border: 1px solid var(--fg-dim);
  font-family: var(--font);
  font-size: 12px;
  padding: 3px 4px;
}

#rtStartBtn {
  margin-left: auto;
  background: transparent;
  border: 1px solid var(--fg-dim);
  color: var(--fg);
  font-family: var(--font);
  font-size: 12px;
  letter-spacing: 1px;
  padding: 6px 14px;
  cursor: pointer;
}
#rtStartBtn.running {
  background: var(--fg);
  color: #000;
  border-color: var(--fg);
}

.rt-stats {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 8px;
  border: 1px solid var(--border);
  padding: 10px;
}

.rt-stat {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.rt-stat-label {
  font-size: 10px;
  color: var(--fg-faint);
  letter-spacing: 1px;
}

.rt-stat span:last-child {
  font-size: 15px;
  color: var(--fg);
}

#rtLog {
  flex: 1;
  min-height: 0;
  overflow-y: auto;
  font-size: 11px;
  color: var(--fg-dim);
  display: flex;
  flex-direction: column; /* normal order — newest is inserted first in JS, not flipped via CSS */
  gap: 2px;
}

#rtLog .rt-log-line.lost { color: var(--error); }

)RAWLITERAL";

const char SCRIPT_JS[] PROGMEM = R"RAWLITERAL(
/* ==========================================================
   Meshly LoRa Terminal — client logic
   Talks to the ESP32 over a single WebSocket using a small
   JSON protocol:

   ESP32 -> browser:
     {"type":"status","connected":true,"nodeAddr":100,"destAddr":200,"networkId":18}
     {"type":"rx","from":200,"data":"hello","rssi":-99,"snr":40}
     {"type":"tx_ack","ok":true}
     {"type":"tx_ack","ok":false,"error":"TIMEOUT"}

   browser -> ESP32:
     {"type":"send","dest":200,"data":"hello"}
     {"type":"get_status"}
   ========================================================== */

(function () {
  const messagesEl = document.getElementById("messages");
  const terminalEl = document.getElementById("terminal");
  const inputEl = document.getElementById("msgInput");
  const sendBtn = document.getElementById("sendBtn");
  const inputbarEl = document.getElementById("inputbar");

  const connDot = document.getElementById("connDot");
  const connText = document.getElementById("connText");
  const nodeAddrEl = document.getElementById("nodeAddr");
  const destAddrEl = document.getElementById("destAddr");
  const networkIdEl = document.getElementById("networkId");
  const lastRssiEl = document.getElementById("lastRssi");
  const lastSnrEl = document.getElementById("lastSnr");

  // range test elements
  const rangeTestToggle = document.getElementById("rangeTestToggle");
  const rangeTestPanel = document.getElementById("rangeTest");
  const rtStartBtn = document.getElementById("rtStartBtn");
  const rtIntervalSel = document.getElementById("rtInterval");
  const rtLogEl = document.getElementById("rtLog");
  const rtSentEl = document.getElementById("rtSent");
  const rtReceivedEl = document.getElementById("rtReceived");
  const rtLostEl = document.getElementById("rtLost");
  const rtLossPctEl = document.getElementById("rtLossPct");
  const rtLastRssiEl = document.getElementById("rtLastRssi");
  const rtAvgRssiEl = document.getElementById("rtAvgRssi");
  const rtLastSnrEl = document.getElementById("rtLastSnr");
  const rtLastRttEl = document.getElementById("rtLastRtt");

  let ws = null;
  let destAddress = null;       // learned from ESP32 status message
  let pendingQueue = [];        // outgoing <div> elements awaiting ack
  let reconnectTimer = null;

  // range test running stats (client-side aggregation)
  let rt = {
    active: false,
    sent: 0,
    received: 0,
    lost: 0,
    rssiSum: 0,
    rssiCount: 0,
  };

  // -------------------- helpers --------------------
  function timestamp() {
    const d = new Date();
    const p = (n) => String(n).padStart(2, "0");
    return `${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`;
  }

  function scrollToBottom() {
    terminalEl.scrollTop = terminalEl.scrollHeight;
  }

  function addSystemLine(text) {
    const el = document.createElement("div");
    el.className = "msg system";
    el.innerHTML = `<div class="msg-body">-- ${escapeHtml(text)} --</div>`;
    messagesEl.appendChild(el);
    scrollToBottom();
  }

  function escapeHtml(str) {
    const div = document.createElement("div");
    div.textContent = str;
    return div.innerHTML;
  }

  function setConnected(isConnected) {
    connDot.className = "dot " + (isConnected ? "dot-on" : "dot-off");
    connText.textContent = isConnected ? "LINK UP" : "LINK DOWN";
  }

  // -------------------- rendering --------------------
  function renderRx(fromAddr, data, rssi, snr) {
    const el = document.createElement("div");
    el.className = "msg rx";
    el.innerHTML = `
      <div class="msg-meta">[${timestamp()}] FROM ${fromAddr} · RSSI ${rssi}dBm · SNR ${snr}</div>
      <div class="msg-body">${escapeHtml(data)}</div>
    `;
    messagesEl.appendChild(el);
    scrollToBottom();

    lastRssiEl.textContent = rssi;
    lastSnrEl.textContent = snr;
  }

  function renderTxPending(data) {
    const el = document.createElement("div");
    el.className = "msg tx pending";
    el.innerHTML = `
      <div class="msg-meta">[${timestamp()}] TO ${destAddress ?? "?"} · sending...</div>
      <div class="msg-body">${escapeHtml(data)}</div>
    `;
    messagesEl.appendChild(el);
    scrollToBottom();
    return el;
  }

  function resolveTxAck(ok, errorCode) {
    const el = pendingQueue.shift();
    if (!el) return;

    el.classList.remove("pending");
    const metaEl = el.querySelector(".msg-meta");
    const text = metaEl.textContent.split("·")[0]; // keep "[time] TO x"

    if (ok) {
      metaEl.textContent = `${text}· sent OK`;
    } else {
      el.classList.add("failed");
      metaEl.textContent = `${text}· FAILED${errorCode ? " (" + errorCode + ")" : ""}`;
    }
  }

  // -------------------- websocket --------------------
  function connect() {
    const proto = location.protocol === "https:" ? "wss" : "ws";
    ws = new WebSocket(`${proto}://${location.host}/ws`);

    ws.onopen = () => {
      setConnected(true);
      addSystemLine("connected to node");
      ws.send(JSON.stringify({ type: "get_status" }));
      if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
    };

    ws.onclose = () => {
      setConnected(false);
      addSystemLine("disconnected — retrying");
      reconnectTimer = setTimeout(connect, 1500);
    };

    ws.onerror = () => {
      ws.close();
    };

    ws.onmessage = (evt) => {
      let msg;
      try { msg = JSON.parse(evt.data); } catch (e) { return; }

      switch (msg.type) {
        case "status":
          nodeAddrEl.textContent = msg.nodeAddr;
          destAddrEl.textContent = msg.destAddr;
          networkIdEl.textContent = msg.networkId;
          destAddress = msg.destAddr;
          break;

        case "rx":
          renderRx(msg.from, msg.data, msg.rssi, msg.snr);
          break;

        case "tx_ack":
          resolveTxAck(msg.ok, msg.error);
          break;

        case "rt_sent":
          rt.sent++;
          rtSentEl.textContent = rt.sent;
          updateRtLossPct();
          break;

        case "rt_result":
          rt.received++;
          rt.rssiSum += msg.rssi;
          rt.rssiCount++;
          rtReceivedEl.textContent = rt.received;
          rtLastRssiEl.textContent = msg.rssi;
          rtAvgRssiEl.textContent = Math.round(rt.rssiSum / rt.rssiCount);
          rtLastSnrEl.textContent = msg.snr;
          rtLastRttEl.textContent = msg.rtt + "ms";
          updateRtLossPct();
          addRtLogLine(`#${msg.seq}  RSSI ${msg.rssi}dBm  SNR ${msg.snr}  RTT ${msg.rtt}ms`, false);
          break;

        case "rt_timeout":
          rt.lost++;
          rtLostEl.textContent = rt.lost;
          updateRtLossPct();
          addRtLogLine(`#${msg.seq}  LOST`, true);
          break;
      }
    };
  }

  // -------------------- sending --------------------
  function sendCurrentInput() {
    const text = inputEl.value.trim();
    if (!text || !ws || ws.readyState !== WebSocket.OPEN) return;

    ws.send(JSON.stringify({ type: "send", dest: destAddress, data: text }));
    pendingQueue.push(renderTxPending(text));

    inputEl.value = "";
    inputEl.focus();
  }

  sendBtn.addEventListener("click", sendCurrentInput);
  inputEl.addEventListener("keydown", (e) => {
    if (e.key === "Enter") sendCurrentInput();
  });

  // -------------------- range test --------------------
  function updateRtLossPct() {
    const total = rt.received + rt.lost;
    const pct = total === 0 ? 0 : Math.round((rt.lost / total) * 100);
    rtLossPctEl.textContent = pct + "%";
  }

  function addRtLogLine(text, lost) {
    const el = document.createElement("div");
    el.className = "rt-log-line" + (lost ? " lost" : "");
    el.textContent = `[${timestamp()}] ${text}`;
    // insert at the actual top of the DOM (not relying on CSS
    // column-reverse, which has inconsistent scrollTop behavior
    // across browsers) so newest is always literally element #1
    rtLogEl.insertBefore(el, rtLogEl.firstChild);
    rtLogEl.scrollTop = 0;
    // cap log length so it doesn't grow forever during a long test —
    // oldest entries are now at the end
    while (rtLogEl.children.length > 100) {
      rtLogEl.removeChild(rtLogEl.lastChild);
    }
  }

  function resetRtStats() {
    rt = { active: rt.active, sent: 0, received: 0, lost: 0, rssiSum: 0, rssiCount: 0 };
    rtSentEl.textContent = "0";
    rtReceivedEl.textContent = "0";
    rtLostEl.textContent = "0";
    rtLossPctEl.textContent = "0%";
    rtLastRssiEl.textContent = "--";
    rtAvgRssiEl.textContent = "--";
    rtLastSnrEl.textContent = "--";
    rtLastRttEl.textContent = "--";
    rtLogEl.innerHTML = "";
  }

  function showRangeTestPanel(show) {
    rangeTestPanel.classList.toggle("hidden", !show);
    terminalEl.classList.toggle("hidden", show);
    inputbarEl.classList.toggle("hidden", show);
    rangeTestToggle.classList.toggle("active", show);
  }

  rangeTestToggle.addEventListener("click", () => {
    const showing = !rangeTestPanel.classList.contains("hidden");
    showRangeTestPanel(!showing);
  });

  rtStartBtn.addEventListener("click", () => {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;

    if (!rt.active) {
      resetRtStats();
      rt.active = true;
      rtStartBtn.textContent = "STOP";
      rtStartBtn.classList.add("running");
      ws.send(JSON.stringify({
        type: "rt_start",
        interval: parseInt(rtIntervalSel.value, 10),
      }));
    } else {
      rt.active = false;
      rtStartBtn.textContent = "START";
      rtStartBtn.classList.remove("running");
      ws.send(JSON.stringify({ type: "rt_stop" }));
    }
  });

  // -------------------- boot --------------------
  setConnected(false);
  connect();
})();

)RAWLITERAL";
