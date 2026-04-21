/*
 * M5 Atom Echo + ENV PRO (BME688)
 * ボタン長押し (1秒) → 気温・湿度・気圧を日本語で読み上げ / 止める
 * ボタン短押し      → SSE イベント送信のみ (音声なし)
 * HTTP GET /sensors → センサーデータを JSON で返す
 * HTTP GET /help    → 使い方を返す
 * TCP port 81       → SSE: ボタン押下イベントをプッシュ配信
 *
 * [必要ライブラリ]
 *   ESP32-audioI2S      by schreibfaul1
 *   Adafruit BME680 Library + Adafruit Unified Sensor
 *
 * [ピン割り当て]
 *   BME688 I2C: SDA=G26, SCL=G32  (Grove ポート)
 *   Speaker   : BCK=G19, LRCK=G33, DOUT=G22
 *
 * [固定 IP]
 *   ゲートウェイの末尾を .55 に固定
 */

#include <M5Atom.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <Audio.h>
#include "credentials.h"

// ---- BME688: Grove G26=SDA, G32=SCL ----
#define I2C_SDA 26
#define I2C_SCL 32

// ---- スピーカー NS4168 ----
#define SPK_BCK  19
#define SPK_LRCK 33
#define SPK_DOUT 22

// ---- Static IP (last octet = 55) ----
#define STATIC_IP_LAST_OCTET 55

static IPAddress staticIP, gateway;
static IPAddress subnet(255, 255, 255, 0);
static IPAddress dns1(8, 8, 8, 8), dns2(8, 8, 4, 4);

Adafruit_BME680 bme;
Audio           audio;
WebServer       server(80);
WiFiServer      sseServer(81);

static bool bmeOk      = false;
static bool isSpeaking = false;
static volatile bool speechDone = false;  // audio_eof_speech から通知

static WiFiClient sseClients[4];
static int        sseCount = 0;

bool connectWiFi();
void speakStatus();
void handleSensors();
void handleHelp();
void handleSseClients();
void sendSSEEvent(bool speaking);

// ---------------------------------------------------------------------------
void setup() {
  M5.begin(true, false, true);
  Serial.begin(115200);
  delay(500);

  M5.dis.drawpix(0, 0x0000ff);  // 青: 起動中

  // BME688 — Grove G26/G32
  Serial.println("[INIT] BME688...");
  Wire.begin(I2C_SDA, I2C_SCL);
  if      (bme.begin(0x77)) bmeOk = true;
  else if (bme.begin(0x76)) bmeOk = true;
  if (!bmeOk) {
    Serial.println("[BME688] not found — check wiring");
  } else {
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
    Serial.println("[BME688] OK");
  }

  // スピーカー
  Serial.println("[INIT] Audio...");
  audio.setPinout(SPK_BCK, SPK_LRCK, SPK_DOUT);
  audio.setVolume(21);
  audio.forceMono(true);    // NS4168 は右チャンネルのみ受信するため両チャンネルを同一にする
  Audio::audio_info_callback = [](Audio::msg_t m) {
    Serial.printf("[Audio +%lums] evt=%d %s\n", millis(), (int)m.e, m.msg ? m.msg : "");
    if (m.e == Audio::evt_eof) speechDone = true;
  };
  Serial.println("[INIT] Audio OK");

  // WiFi
  if (connectWiFi()) {
    server.on("/sensors", HTTP_GET, handleSensors);
    server.on("/help",    HTTP_GET, handleHelp);
    server.begin();
    sseServer.begin();
    Serial.printf("[HTTP] http://%s/sensors\n", WiFi.localIP().toString().c_str());
    Serial.printf("[HTTP] http://%s/help\n",    WiFi.localIP().toString().c_str());
    Serial.printf("[SSE]  http://%s:81/events\n", WiFi.localIP().toString().c_str());
    M5.dis.drawpix(0, 0x00ff00);  // 緑: 準備完了
  } else {
    M5.dis.drawpix(0, 0xff0000);  // 赤: WiFi 失敗
    Serial.println("[WiFi] all AP failed");
  }
}

// ---------------------------------------------------------------------------
void loop() {
  M5.update();
  server.handleClient();
  handleSseClients();

  // ボタン: 長押し → 読み上げ ON/OFF、短押し → SSE 送信のみ
  if (M5.Btn.wasReleasefor(1000)) {
    isSpeaking = !isSpeaking;
    if (isSpeaking) {
      M5.dis.drawpix(0, 0xffff00);  // 黄: 読み上げ中
      speakStatus();
    } else {
      audio.stopSong();
      M5.dis.drawpix(0, 0x00ff00);
      Serial.println("[BTN] long press → stopped");
    }
    sendSSEEvent(isSpeaking);
  } else if (M5.Btn.wasReleased()) {
    sendSSEEvent(isSpeaking);
    Serial.println("[BTN] short press → SSE only");
  }

  // 読み上げ終了したら自動で止まる (eof_speech コールバック経由で確実に検知)
  if (isSpeaking && speechDone) {
    speechDone = false;
    isSpeaking = false;
    M5.dis.drawpix(0, 0x00ff00);
    sendSSEEvent(false);
  }

  audio.loop();
}

// ---------------------------------------------------------------------------
void handleSensors() {
  bme.performReading();

  char json[192];
  snprintf(json, sizeof(json),
    "{"
      "\"env\":{"
        "\"temperature\":%.2f,"
        "\"humidity\":%.2f,"
        "\"pressure\":%.2f,"
        "\"gas_resistance\":%.0f"
      "}"
    "}",
    bmeOk ? bme.temperature    : 0.0,
    bmeOk ? bme.humidity       : 0.0,
    bmeOk ? bme.pressure/100.0 : 0.0,
    bmeOk ? bme.gas_resistance : 0.0
  );
  server.send(200, "application/json", json);
}

// ---------------------------------------------------------------------------
void handleHelp() {
  String ip = WiFi.localIP().toString();
  String html =
    "<html><head><meta charset='utf-8'>"
    "<style>body{font-family:sans-serif;padding:2em;max-width:700px;}"
    "code{background:#f0f0f0;padding:2px 6px;border-radius:4px;}"
    "td{padding:6px 12px;}pre{background:#f8f8f8;padding:1em;border-radius:6px;overflow:auto;}"
    "</style></head><body>"
    "<h2>M5 Atom Echo (ENV PRO) — HTTP API</h2>"
    "<table border='1' cellspacing='0'>"
    "<tr><th>エンドポイント</th><th>説明</th><th>レスポンス</th></tr>"
    "<tr><td><code>GET /sensors</code></td><td>気温・湿度・気圧を取得</td><td>JSON</td></tr>"
    "<tr><td><code>GET /help</code></td><td>この使い方ページ</td><td>HTML</td></tr>"
    "<tr><td><code>TCP :" + ip + ":81</code></td><td>ボタン押下イベント (SSE)</td><td>text/event-stream</td></tr>"
    "</table>"
    "<h3>/sensors レスポンス例</h3>"
    "<pre>{\n"
    "  \"env\": { \"temperature\": 24.5, \"humidity\": 60.0,\n"
    "            \"pressure\": 1013.25, \"gas_resistance\": 50000 }\n"
    "}</pre>"
    "<h3>SSE イベント (port 81)</h3>"
    "<pre>const es = new EventSource('http://" + ip + ":81/events');\n"
    "es.onmessage = e => console.log(JSON.parse(e.data));</pre>"
    "<p>IP アドレス: <strong>" + ip + "</strong></p>"
    "</body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}


// ---------------------------------------------------------------------------
void handleSseClients() {
  WiFiClient c = sseServer.available();
  if (!c) return;

  unsigned long t = millis();
  while (!c.available() && millis() - t < 500) delay(1);
  while (c.available()) c.read();

  c.print(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/event-stream\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: keep-alive\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "\r\n"
  );
  c.print(": connected\n\n");
  c.flush();

  int j = 0;
  for (int i = 0; i < sseCount; i++) {
    if (sseClients[i].connected()) sseClients[j++] = sseClients[i];
  }
  sseCount = j;

  if (sseCount < 4) {
    sseClients[sseCount++] = c;
    Serial.printf("[SSE] client connected (%d total)\n", sseCount);
  } else {
    c.stop();
  }
}

// ---------------------------------------------------------------------------
void sendSSEEvent(bool speaking) {
  if (sseCount == 0) return;

  char buf[256];
  snprintf(buf, sizeof(buf),
    "data: {\"speaking\":%s,"
    "\"env\":{\"temperature\":%.1f,\"humidity\":%.0f,\"pressure\":%.0f}}\n\n",
    speaking ? "true" : "false",
    bmeOk ? bme.temperature    : 0.0,
    bmeOk ? bme.humidity       : 0.0,
    bmeOk ? bme.pressure/100.0 : 0.0
  );

  int j = 0;
  for (int i = 0; i < sseCount; i++) {
    if (sseClients[i].connected()) {
      sseClients[i].print(buf);
      sseClients[i].flush();
      sseClients[j++] = sseClients[i];
    }
  }
  sseCount = j;
}

// ---------------------------------------------------------------------------
void speakStatus() {
  char text[128] = "";

  if (bmeOk && bme.performReading()) {
    snprintf(text, sizeof(text),
             "%.1f度、%.0fパー、気圧%.0f",
             bme.temperature, bme.humidity, bme.pressure / 100.0);
  }

  if (strlen(text) == 0) {
    strncpy(text, "データを取得できませんでした", sizeof(text) - 1);
  }

  Serial.printf("[TTS] %s\n", text);
  bool ok = audio.connecttospeech(text, "ja");
  Serial.printf("[TTS] connect=%s  running=%s  heap=%u\n",
                ok ? "OK" : "FAIL",
                audio.isRunning() ? "yes" : "no",
                ESP.getFreeHeap());
}

// ---------------------------------------------------------------------------
bool connectWiFi() {
  for (int i = 0; i < WIFI_COUNT; i++) {
    Serial.printf("[WiFi] trying %s ...\n", WIFI_LIST[i].ssid);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_LIST[i].ssid, WIFI_LIST[i].password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
      delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
      gateway  = WiFi.gatewayIP();
      staticIP = IPAddress(gateway[0], gateway[1], gateway[2], STATIC_IP_LAST_OCTET);
      WiFi.disconnect(true);
      WiFi.config(staticIP, gateway, subnet, dns1, dns2);
      WiFi.begin(WIFI_LIST[i].ssid, WIFI_LIST[i].password);

      start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
        delay(200);
      }
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] connected: %s  IP=%s\n",
                      WIFI_LIST[i].ssid, WiFi.localIP().toString().c_str());
        return true;
      }
    }
    Serial.printf("[WiFi] %s failed, next...\n", WIFI_LIST[i].ssid);
  }
  return false;
}
