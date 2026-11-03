//By SciCraft

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "web_ui.h"   // INDEX_HTML / STYLE_CSS / SCRIPT_JS (PROGMEM)

// ----------------------------------------------------------------
// USER CONFIG
// ----------------------------------------------------------------
#define MY_ADDRESS         1        // this node's LoRa address
#define DEST_ADDRESS       2       // the other node's LoRa address
#define NETWORK_ID         18         // must match on both nodes
#define LORA_BAND          915000000  // 915000000 for RYLR998, 490000000 for RYLR498
#define CONFIGURE_LORA_ON_BOOT false  // set true only if you want the ESP32
                                       // to (re)push AT+ADDRESS/NETWORKID/BAND
                                       // on every boot. Usually not needed —
                                       // the module remembers settings in flash.

// UART pins between ESP32 and RYLR998 — must match your existing wiring
#define LORA_RX_PIN 17   // ESP32 RX2  <- RYLR998 TXD
#define LORA_TX_PIN 16   // ESP32 TX2  -> RYLR998 RXD
#define LORA_BAUD   115200

// WiFi Access Point
const char* AP_SSID = "NAME OF YOUR AP";
const char* AP_PASS = "PASSWORD"; // must be 8+ chars, change if you like

#define LED_RED_PIN    23
#define LED_GREEN_PIN  22
#define LED_BLUE_PIN   21
#define BUZZER_PIN     33

#define LED_ON  LOW  
#define LED_OFF HIGH

enum LedColor { LED_NONE, LED_RED, LED_GREEN, LED_BLUE };

bool     indicatorActive     = false;
uint32_t indicatorStartedAt  = 0;
uint32_t indicatorDurationMs = 0;

// how long each event flashes for — tweak to taste
const uint32_t MSG_RECEIVED_FLASH_MS   = 700;  // incoming chat message
const uint32_t RT_SUCCESS_FLASH_MS     = 120;  // range test packet received (short, these repeat fast)
const uint32_t RT_LOSS_FLASH_MS        = 200;  // range test packet lost
const uint32_t TX_SUCCESS_FLASH_MS     = 200;  // your chat message sent OK
const uint32_t TX_FAIL_FLASH_MS        = 500;  // your chat message failed/timed out
const uint32_t BOOT_FLASH_MS           = 150;  // startup confirmation blink

// ----------------------------------------------------------------
// Globals
// ----------------------------------------------------------------
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

HardwareSerial& LoRaSerial = Serial2;

String rxLineBuffer = "";

// Tracks the single AT+SEND we're currently waiting on +OK/+ERR for
bool     txPending    = false;
uint32_t txSentAt      = 0;
const uint32_t TX_TIMEOUT_MS = 4000;

// ----------------------------------------------------------------
// TX QUEUE — the RYLR998 only allows one AT+SEND in flight at a time,
// so chat sends and range-test pings/pongs both flow through this
// tiny FIFO instead of writing to the UART directly.
// ----------------------------------------------------------------
struct TxItem {
  int      dest;
  String   data;
  bool     isAutoReply;      // true = range-test PONG (don't ack the browser)
  bool     isRangeTestPing;  // true = range-test PING (track RTT on success)
  uint32_t seq;
};

#define TX_QUEUE_SIZE 6
TxItem   txQueue[TX_QUEUE_SIZE];
int      txQueueHead = 0, txQueueTail = 0, txQueueCount = 0;

TxItem   currentTx;
bool     currentTxValid = false;

bool queuePush(const TxItem &item) {
  if (txQueueCount >= TX_QUEUE_SIZE) return false; // drop if backed up
  txQueue[txQueueTail] = item;
  txQueueTail = (txQueueTail + 1) % TX_QUEUE_SIZE;
  txQueueCount++;
  return true;
}

bool queuePop(TxItem &item) {
  if (txQueueCount == 0) return false;
  item = txQueue[txQueueHead];
  txQueueHead = (txQueueHead + 1) % TX_QUEUE_SIZE;
  txQueueCount--;
  return true;
}

// ----------------------------------------------------------------
// RANGE TEST state
// ----------------------------------------------------------------
bool     rangeTestActive   = false;
uint32_t rtIntervalMs      = 1000;
uint32_t rtSeq             = 0;
uint32_t rtLastPingQueuedAt = 0;
bool     rtAwaitingPong    = false;
uint32_t rtLastPingSeq     = 0;
uint32_t rtLastPingTime    = 0;
const uint32_t RT_TIMEOUT_MS = 3000; // how long to wait for a pong before counting it lost

// ----------------------------------------------------------------
// Forward declarations
// ----------------------------------------------------------------
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len);
void handleWsMessage(AsyncWebSocketClient *client, const String &msg);
void pollLoRaSerial();
void handleLoRaLine(String line);
void parseRcv(const String &payload);
void applyLoRaConfig();

void queueChatSend(int dest, const String &data);
void queueAutoReply(int dest, const String &data);
void queueRangeTestPing(int dest, const String &data, uint32_t seq);
void transmitNow(int dest, const String &data);
void serviceTxQueue();
void handleTxOutcome(bool ok, const String &errCode);
void serviceRangeTest();

void broadcastStatus();
void broadcastRx(int fromAddr, const String &data, int rssi, int snr);
void sendTxAck(bool ok, const String &errCode);
void broadcastRtSent(uint32_t seq);
void broadcastRtResult(uint32_t seq, int rssi, int snr, uint32_t rtt);
void broadcastRtTimeout(uint32_t seq);

void setLedColor(LedColor c);
void triggerIndicator(LedColor color, uint32_t durationMs, bool buzz);
void serviceIndicator();

// ==================================================================
// SETUP
// ==================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[BOOT] SciCraft LoRa Web Terminal V1");

  // --- LoRa UART ---
  LoRaSerial.begin(LORA_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  delay(100);
  if (CONFIGURE_LORA_ON_BOOT) {
    applyLoRaConfig();
  }

  // --- Physical feedback (LED + buzzer) ---
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  setLedColor(LED_NONE);
  digitalWrite(BUZZER_PIN, LOW);
  // quick blue double-blink so you know the board booted even with no
  triggerIndicator(LED_BLUE, BOOT_FLASH_MS, false);
  delay(BOOT_FLASH_MS + 100);
  triggerIndicator(LED_BLUE, BOOT_FLASH_MS, false);
  delay(BOOT_FLASH_MS + 100);
  setLedColor(LED_NONE);
  indicatorActive = false;

  // --- WiFi Access Point ---
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("[WIFI] AP started. Connect to \"");
  Serial.print(AP_SSID);
  Serial.print("\" -> http://");
  Serial.println(WiFi.softAPIP());

  // --- WebSocket ---
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // --- Web UI routes (served straight from flash/PROGMEM, no filesystem) ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/css", STYLE_CSS);
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "application/javascript", SCRIPT_JS);
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("[HTTP] Server started.");
}

// ==================================================================
// LOOP
// ==================================================================
void loop() {
  pollLoRaSerial();

  // resolve a stuck TX (module never replied +OK/+ERR)
  if (txPending && (millis() - txSentAt > TX_TIMEOUT_MS)) {
    handleTxOutcome(false, "TIMEOUT");
  }

  serviceTxQueue();
  serviceRangeTest();
  serviceIndicator();
  ws.cleanupClients();

  // V2 HOOK: poll a physical send button here, or refresh an OLED
  // with the same status fields sent to the browser in broadcastStatus().
}

// ==================================================================
// WEBSOCKET EVENTS
// ==================================================================
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client #%u connected\n", client->id());
    broadcastStatus(); // let the new client know current state immediately

  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client #%u disconnected\n", client->id());

  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len &&
        info->opcode == WS_TEXT) {
      String msg;
      msg.reserve(len);
      for (size_t i = 0; i < len; i++) msg += (char)data[i];
      handleWsMessage(client, msg);
    }
  }
}

void handleWsMessage(AsyncWebSocketClient *client, const String &msg) {
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, msg);
  if (err) {
    Serial.println("[WS] Bad JSON from client");
    return;
  }

  const char* type = doc["type"];
  if (type == nullptr) return;

  if (strcmp(type, "send") == 0) {
    int dest = doc["dest"] | DEST_ADDRESS;
    String data = doc["data"] | "";
    if (data.length() == 0) return;
    if (data.length() > 240) data = data.substring(0, 240); // RYLR998 payload limit
    queueChatSend(dest, data);

  } else if (strcmp(type, "get_status") == 0) {
    broadcastStatus();

  } else if (strcmp(type, "rt_start") == 0) {
    rangeTestActive = true;
    rtIntervalMs = doc["interval"] | 1000;
    rtSeq = 0;
    rtAwaitingPong = false;
    rtLastPingQueuedAt = 0; // fire the first ping immediately
    Serial.println("[RT] Range test started");

  } else if (strcmp(type, "rt_stop") == 0) {
    rangeTestActive = false;
    rtAwaitingPong = false;
    Serial.println("[RT] Range test stopped");
  }
}

// ==================================================================
// LoRa UART HANDLING
// ==================================================================
void pollLoRaSerial() {
  while (LoRaSerial.available()) {
    char c = LoRaSerial.read();
    if (c == '\n') {
      handleLoRaLine(rxLineBuffer);
      rxLineBuffer = "";
    } else if (c != '\r') {
      rxLineBuffer += c;
      if (rxLineBuffer.length() > 300) rxLineBuffer = ""; // safety
    }
  }
}

void handleLoRaLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  Serial.print("[LORA] ");
  Serial.println(line);

  if (line.startsWith("+RCV=")) {
    parseRcv(line.substring(5));

  } else if (line.startsWith("+OK")) {
    if (txPending) handleTxOutcome(true, "");

  } else if (line.startsWith("+ERR=")) {
    if (txPending) handleTxOutcome(false, line.substring(5));

  } else if (line.startsWith("+READY")) {
    Serial.println("[LORA] Module reset/ready.");
    broadcastStatus();
  }
  // +RESET, +ADDRESS=, +NETWORKID=, +BAND= etc. can be handled here
  // later if you add a settings panel in V2.
}

// RYLR998 receive format: +RCV=<Address>,<Length>,<Data>,<RSSI>,<SNR>
// Data can itself contain commas, so we must use <Length> to slice it
// out precisely rather than naively splitting on ',' everywhere.
void parseRcv(const String &payload) {
  int c1 = payload.indexOf(',');
  if (c1 < 0) return;
  int addr = payload.substring(0, c1).toInt();

  int c2 = payload.indexOf(',', c1 + 1);
  if (c2 < 0) return;
  int dataLen = payload.substring(c1 + 1, c2).toInt();

  int dataStart = c2 + 1;
  int dataEnd = dataStart + dataLen;
  if (dataEnd > (int)payload.length()) return; // malformed / truncated frame

  String data = payload.substring(dataStart, dataEnd);
  String rest = payload.substring(dataEnd); // ",RSSI,SNR"

  int c3 = rest.indexOf(',', 1);
  if (c3 < 0) return;
  int rssi = rest.substring(1, c3).toInt();
  int snr  = rest.substring(c3 + 1).toInt();

  // --- Range test protocol packets are intercepted here so they
  //     never show up as regular chat messages ---
  if (data.startsWith("RTPING:")) {
    uint32_t seq = (uint32_t) data.substring(7).toInt();
    queueAutoReply(addr, "RTPONG:" + String(seq));
    return;
  }

  if (data.startsWith("RTPONG:")) {
    uint32_t seq = (uint32_t) data.substring(7).toInt();
    if (rtAwaitingPong && seq == rtLastPingSeq) {
      uint32_t rtt = millis() - rtLastPingTime;
      rtAwaitingPong = false;
      broadcastRtResult(seq, rssi, snr, rtt);
      triggerIndicator(LED_GREEN, RT_SUCCESS_FLASH_MS, true);
    }
    return;
  }

  broadcastRx(addr, data, rssi, snr);
  triggerIndicator(LED_GREEN, MSG_RECEIVED_FLASH_MS, true);
}

// ---- Public "send something" API — all roads lead through the queue ----
void queueChatSend(int dest, const String &data) {
  TxItem item = { dest, data, false, false, 0 };
  if (!queuePush(item)) Serial.println("[TX] Queue full, chat message dropped");
}

void queueAutoReply(int dest, const String &data) {
  TxItem item = { dest, data, true, false, 0 };
  if (!queuePush(item)) Serial.println("[TX] Queue full, auto-reply dropped");
}

void queueRangeTestPing(int dest, const String &data, uint32_t seq) {
  TxItem item = { dest, data, true, true, seq };
  if (!queuePush(item)) Serial.println("[TX] Queue full, range-test ping dropped");
}

void transmitNow(int dest, const String &data) {
  String cmd = "AT+SEND=" + String(dest) + "," + String(data.length()) + "," + data;
  LoRaSerial.print(cmd);
  LoRaSerial.print("\r\n");

  txPending = true;
  txSentAt = millis();

  Serial.print("[LORA] TX -> ");
  Serial.println(cmd);
}

// Called every loop() — if nothing is currently in flight, send the
// next queued item (if any).
void serviceTxQueue() {
  if (txPending) return;
  if (currentTxValid) currentTxValid = false; // previous item fully resolved

  TxItem item;
  if (queuePop(item)) {
    currentTx = item;
    currentTxValid = true;
    transmitNow(item.dest, item.data);
  }
}

// Called when the module replies +OK / +ERR / TIMEOUT to whatever
// we last transmitted.
void handleTxOutcome(bool ok, const String &errCode) {
  txPending = false;

  if (currentTxValid) {
    TxItem item = currentTx;
    currentTxValid = false;

    if (!item.isAutoReply) {
      sendTxAck(ok, errCode); // only chat sends get an ack shown in the log
      if (ok) {
        triggerIndicator(LED_BLUE, TX_SUCCESS_FLASH_MS, false);
      } else {
        triggerIndicator(LED_RED, TX_FAIL_FLASH_MS, true);
      }
    }

    if (item.isRangeTestPing) {
      if (ok) {
        // transmission completed — start the RTT clock, wait for RTPONG
        rtLastPingSeq = item.seq;
        rtLastPingTime = millis();
        rtAwaitingPong = true;
      } else {
        broadcastRtTimeout(item.seq); // failed to even get out over the air
      }
    }
  }

  serviceTxQueue();
}

void applyLoRaConfig() {
  LoRaSerial.print("AT+ADDRESS=" + String(MY_ADDRESS) + "\r\n");
  delay(200);
  LoRaSerial.print("AT+NETWORKID=" + String(NETWORK_ID) + "\r\n");
  delay(200);
  LoRaSerial.print("AT+BAND=" + String(LORA_BAND) + "\r\n");
  delay(200);
}

// ==================================================================
// RANGE TEST — periodic ping scheduler
// ==================================================================
void serviceRangeTest() {
  if (!rangeTestActive) return;
  uint32_t now = millis();

  // outstanding ping took too long -> count it as lost, free up the slot
  if (rtAwaitingPong && (now - rtLastPingTime > RT_TIMEOUT_MS)) {
    broadcastRtTimeout(rtLastPingSeq);
    rtAwaitingPong = false;
  }

  // fire the next ping once the previous one is resolved and the
  // interval has elapsed
  if (!rtAwaitingPong && (now - rtLastPingQueuedAt >= rtIntervalMs)) {
    rtSeq++;
    queueRangeTestPing(DEST_ADDRESS, "RTPING:" + String(rtSeq), rtSeq);
    rtLastPingQueuedAt = now;
    broadcastRtSent(rtSeq);
  }
}

// ==================================================================
// MESSAGES OUT TO BROWSER(S)
// ==================================================================
void broadcastStatus() {
  StaticJsonDocument<256> doc;
  doc["type"] = "status";
  doc["connected"] = true;
  doc["nodeAddr"] = MY_ADDRESS;
  doc["destAddr"] = DEST_ADDRESS;
  doc["networkId"] = NETWORK_ID;
  doc["band"] = LORA_BAND;

  String out;
  serializeJson(doc, out);
  ws.textAll(out);
}

void broadcastRx(int fromAddr, const String &data, int rssi, int snr) {
  StaticJsonDocument<512> doc;
  doc["type"] = "rx";
  doc["from"] = fromAddr;
  doc["data"] = data;
  doc["rssi"] = rssi;
  doc["snr"] = snr;

  String out;
  serializeJson(doc, out);
  ws.textAll(out);
}

void sendTxAck(bool ok, const String &errCode) {
  StaticJsonDocument<256> doc;
  doc["type"] = "tx_ack";
  doc["ok"] = ok;
  if (!ok) doc["error"] = errCode;

  String out;
  serializeJson(doc, out);
  ws.textAll(out);
}

void broadcastRtSent(uint32_t seq) {
  StaticJsonDocument<128> doc;
  doc["type"] = "rt_sent";
  doc["seq"] = seq;

  String out;
  serializeJson(doc, out);
  ws.textAll(out);
}

void broadcastRtResult(uint32_t seq, int rssi, int snr, uint32_t rtt) {
  StaticJsonDocument<192> doc;
  doc["type"] = "rt_result";
  doc["seq"] = seq;
  doc["rssi"] = rssi;
  doc["snr"] = snr;
  doc["rtt"] = rtt;

  String out;
  serializeJson(doc, out);
  ws.textAll(out);
}

void broadcastRtTimeout(uint32_t seq) {
  triggerIndicator(LED_RED, RT_LOSS_FLASH_MS, false);

  StaticJsonDocument<128> doc;
  doc["type"] = "rt_timeout";
  doc["seq"] = seq;

  String out;
  serializeJson(doc, out);
  ws.textAll(out);
}

// ==================================================================
// PHYSICAL FEEDBACK — common-anode RGB LED + active buzzer
// ==================================================================
void setLedColor(LedColor c) {
  digitalWrite(LED_RED_PIN,   (c == LED_RED)   ? LED_ON : LED_OFF);
  digitalWrite(LED_GREEN_PIN, (c == LED_GREEN) ? LED_ON : LED_OFF);
  digitalWrite(LED_BLUE_PIN,  (c == LED_BLUE)  ? LED_ON : LED_OFF);
}

// Starts (or restarts) a timed flash. Non-blocking — serviceIndicator()
// in loop() turns it back off once the duration elapses, so this never
// stalls LoRa UART reads or the WebSocket.
void triggerIndicator(LedColor color, uint32_t durationMs, bool buzz) {
  setLedColor(color);
  digitalWrite(BUZZER_PIN, buzz ? HIGH : LOW);
  indicatorActive = true;
  indicatorStartedAt = millis();
  indicatorDurationMs = durationMs;
}

void serviceIndicator() {
  if (!indicatorActive) return;
  if (millis() - indicatorStartedAt >= indicatorDurationMs) {
    setLedColor(LED_NONE);
    digitalWrite(BUZZER_PIN, LOW);
    indicatorActive = false;
  }
}
