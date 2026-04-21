# M5 Atom Echo

M5 Atom Echo を使った Arduino スケッチ集。  
ボタンでセンサー値を日本語音声で読み上げ、HTTP API でデータを取得できる。

## スケッチ一覧

| スケッチ | 接続センサー | 固定 IP |
|---|---|---|
| [m5_atom_echo_env](./m5_scripts/m5_atom_echo_env/) | ENV PRO (BME688) | `.55` |
| [m5_atom_echo_gps](./m5_scripts/m5_atom_echo_gps/) | ATOMIC GPS Base v2.0 (AT6668) | `.56` |

## 共通ハードウェア

| 項目 | 内容 |
|---|---|
| メイン | M5 Atom Echo (ESP32-PICO-D4, PSRAM なし) |
| スピーカー | NS4168 I2S アンプ内蔵 |
| 接続方式 | Grove ポート (G26/G32) または底面コネクター |

## 共通ピン割り当て

| 用途 | ピン |
|---|---|
| スピーカー BCK | G19 |
| スピーカー LRCK | G33 |
| スピーカー DOUT | G22 |

## 必要なライブラリ

Arduino IDE のライブラリマネージャ (`スケッチ → ライブラリをインクルード → ライブラリを管理`) でインストール。

| ライブラリ名 | 作者 | 用途 | 対象スケッチ |
|---|---|---|---|
| ESP32-audioI2S | schreibfaul1 | Google TTS 音声再生 | 両方 |
| TinyGPS++ | Mikal Hart | GPS データ解析 | GPS |
| Adafruit BME680 Library | Adafruit | BME688 センサー | ENV PRO |
| Adafruit Unified Sensor | Adafruit | BME680 依存ライブラリ | ENV PRO |

> Adafruit BME680 インストール時に「依存ライブラリもインストールしますか？」と聞かれたら **Install All** を選ぶ。

## ボード設定

| 項目 | 設定値 |
|---|---|
| ボードマネージャURL | `https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json` |
| ボード | M5Stack Arduino → **M5Atom** |
| Partition Scheme | Default |
| Upload Speed | 1500000 |

ボードマネージャURL の追加: `ファイル → 環境設定 → 追加のボードマネージャURL`

## ESP32-audioI2S ライブラリの修正 (必須)

M5 Atom Echo は PSRAM を搭載していないため、ライブラリのソースを直接修正する必要がある。  
ライブラリのパス: `~/Arduino/libraries/ESP32-audioI2S-master/src/`

### 1. Audio.cpp — バッファサイズ縮小 (34〜35行目付近)

```cpp
// 変更後
constexpr size_t m_outbuffSize = 4608;           // 元: 4608*2
constexpr size_t m_samplesBuff48KSize = m_outbuffSize * 2; // 元: *8
```

### 2. Audio.cpp — AudioBuffer コンストラクタ (80行目付近)

```cpp
AudioBuffer::AudioBuffer() {
    m_mainBuffSize = 1024 * 16;  // 元: デフォルト大 (PSRAM 想定)
    m_resBuffSize  = 1024 * 2;
}
```

### 3. Audio.cpp — PSRAM チェック削除 (setPinout 内, 5820行目付近)

```cpp
// 変更後: goto exit を削除し、PSRAM なしでも続行させる
m_f_psramFound = psramInit(); // M5 Atom Echo は PSRAM なし。alloc() は自動で malloc にフォールバックするため続行する
// 削除した行: if (!m_f_psramFound) { result = false; goto exit; }
```

### 4. audiolib_structs.hpp — DMA バッファ縮小 (末尾付近)

```cpp
#define DMA_DESC_NUM  16   // 元: 32
#define DMA_FRAME_NUM 64   // 元: 192
```

### 5. Audio.cpp — TTS の Stream-lost 誤検知を防ぐ (processWebStream 内, 4214行目付近)

Google TTS はラジオ等のライブストリームと異なり有限ファイルを返す。  
バッファが一時的に空になっても「接続切れ」と誤判定して再接続ループになるのを防ぐ。

```cpp
// 変更後: !m_f_tts && を追加し、TTS 中は streamDetection をスキップ
if (!m_f_tts && !m_f_allDataReceived)
    if (streamDetection(m_pwst.availableBytes)) return;
if (!m_pwst.f_clientIsConnected) {
    if (m_f_tts && !m_f_allDataReceived) m_f_allDataReceived = true;
} // connection closed (Google TTS)
```

### 6. Audio.cpp — TTS の再生開始閾値を引き上げ (processWebStream 内, 4239行目付近)

TTS はバッファに十分データが溜まってから再生開始することで途切れを防ぐ。

```cpp
// 変更後
bool streamReady = m_f_tts ? (m_f_allDataReceived || InBuff.bufferFilled() >= InBuff.getBufsize() / 2)
                           : (InBuff.bufferFilled() * 2 > m_pwst.maxFrameSize);
if (streamReady && !m_f_stream) {
    info(*this, evt_info, "stream ready");
    m_f_stream = true;
}
```

### スケッチ側の設定 (setup() 内)

ライブラリの旧バージョンのフリー関数 `audio_info()` / `audio_eof_speech()` はこのバージョンでは呼ばれない。  
`Audio::audio_info_callback` に std::function で登録する必要がある。

```cpp
audio.setPinout(SPK_BCK, SPK_LRCK, SPK_DOUT);
audio.setVolume(21);
audio.forceMono(true);  // NS4168 は右チャンネルのみ受信するため両チャンネルを同一にする
Audio::audio_info_callback = [](Audio::msg_t m) {
    Serial.printf("[Audio +%lums] evt=%d %s\n", millis(), (int)m.e, m.msg ? m.msg : "");
    if (m.e == Audio::evt_eof) speechDone = true;
};
```

## 既知の問題 / TODO

### OOM: MP3 デコーダー初期化失敗 (断続的)

WiFi 接続後にヒープが断片化し、MP3 デコーダーの `m_out16` バッファ (18432 バイト) の確保に失敗することがある。

```
OOM: failed to allocate 18432 bytes for m_out16
[Audio] Closing web stream "translate.google.com.vn"
```

**原因:** `mp3_decoder.cpp` の `m_out16.alloc_array(4608 * 2, "m_out16")` は WiFi スタックが
ヒープを断片化した後に呼ばれるため、空き総量 (~91KB) があっても連続した 18KB ブロックが
取れない場合がある。2 回目の試みでは成功することが多い (断片化パターンによる)。

**TODO:** `mp3_decoder.cpp` の `4608 * 2` を `1152 * 2` (1 フレーム分) に削減し、
不要な余裕バッファを排除する。合わせて `m_outbuffSize` との整合性を確認する。

---

## credentials.h の設定

各スケッチフォルダに `credentials.h` を作成する (`.gitignore` で除外済み)。

```cpp
struct WifiCredential { const char* ssid; const char* password; };
static const WifiCredential WIFI_LIST[] = {
    { "SSID_1", "PASSWORD_1" },
    { "SSID_2", "PASSWORD_2" },
    { "SSID_3", "PASSWORD_3" },
};
static const int WIFI_COUNT = sizeof(WIFI_LIST) / sizeof(WIFI_LIST[0]);
static const unsigned long WIFI_TIMEOUT_MS = 10000;
```

## 共通 LED の意味

| 色 | 状態 |
|---|---|
| 青 | 起動中 |
| 緑 | 待機中 |
| 黄 | 読み上げ中 |
| 赤 | WiFi 接続失敗 |

## 共通 HTTP API

| エンドポイント | ポート | 説明 |
|---|---|---|
| `GET /sensors` | 80 | センサーデータを JSON で返す |
| `GET /help` | 80 | 使い方ページ (HTML) |
| SSE 接続 | 81 | ボタン押下イベントをプッシュ配信 |

## 注意事項

- TTS は Google の API を使用するため WiFi 接続が必要。
- GPS の fix は屋外か窓際でないと取得できない。起動直後は `"fix": false` が正常。
- 音声認識（ウェイクワード）は非搭載。ボタン操作のみ。
