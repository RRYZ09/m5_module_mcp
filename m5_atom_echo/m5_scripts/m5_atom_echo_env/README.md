# M5 Atom Echo + ENV PRO (BME688)

M5 Atom Echo に ENV PRO センサーを接続した Arduino スケッチ。  
ボタンで気温・湿度・気圧を日本語音声で読み上げ、HTTP API でデータを取得できる。

## ハードウェア構成

| デバイス | 型番 |
|---|---|
| メイン | M5 Atom Echo |
| 環境センサー | ENV PRO Unit (BME688) |

## ピン割り当て

| 用途 | ピン |
|---|---|
| BME688 SDA (Grove) | G26 |
| BME688 SCL (Grove) | G32 |
| スピーカー BCK | G19 |
| スピーカー LRCK | G33 |
| スピーカー DOUT | G22 |

ENV PRO は Atom Echo の Grove ポート (G26/G32) に接続する。

## 固定 IP

ゲートウェイの末尾を `.55` に固定。例: `192.168.1.55`

## 使い方

### ボタン

| 操作 | 動作 |
|---|---|
| 1回押す | 気温・湿度・気圧を日本語で読み上げ → 再生終了で自動停止 |
| 再生中に押す | 読み上げを即時停止 |

### HTTP API

| エンドポイント | メソッド | レスポンス | 説明 |
|---|---|---|---|
| `/sensors` | GET | JSON | 気温・湿度・気圧を返す |
| `/help` | GET | HTML | 使い方ページ |

#### `/sensors` レスポンス例

```json
{
  "env": {
    "temperature": 24.5,
    "humidity": 60.0,
    "pressure": 1013.25,
    "gas_resistance": 50000
  }
}
```

### SSE (Server-Sent Events) — port 81

ボタン押下時にリアルタイムでイベントを受信できる。

```javascript
const es = new EventSource('http://192.168.1.55:81/events');
es.onmessage = e => console.log(JSON.parse(e.data));

// イベント例:
// { speaking: true, env: { temperature: 25.4, humidity: 49, pressure: 1009 } }
```

## ライブラリ・ボード設定

→ [共通設定 README](../../README.md) を参照
