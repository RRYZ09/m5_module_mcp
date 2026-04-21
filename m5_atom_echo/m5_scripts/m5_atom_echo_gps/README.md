# M5 Atom Echo + ATOMIC GPS Base v2.0 (AT6668)

M5 Atom Echo に ATOMIC GPS Base v2.0 を接続した Arduino スケッチ。  
ボタンで GPS 座標を日本語音声で読み上げ、HTTP API でデータを取得できる。

## ハードウェア構成

| デバイス | 型番 |
|---|---|
| メイン | M5 Atom Echo |
| GPS | ATOMIC GPS Base v2.0 (AT6668) |

ATOMIC GPS Base は Atom Echo の底面コネクターに接続する。

## ピン割り当て

| 用途 | ピン |
|---|---|
| GPS UART RX | G26 |
| GPS UART TX | G32 |
| スピーカー BCK | G19 |
| スピーカー LRCK | G33 |
| スピーカー DOUT | G22 |

GPS ボーレート: **115200 bps**

## 固定 IP

ゲートウェイの末尾を `.56` に固定。例: `192.168.1.56`

## 使い方

### ボタン

| 操作 | 動作 |
|---|---|
| 1回押す | GPS 座標・高度・衛星数を日本語で読み上げ → 再生終了で自動停止 |
| 再生中に押す | 読み上げを即時停止 |

GPS が未取得の場合は「GPS を取得できませんでした」と読み上げる。  
GPS fix は屋外か窓際でないと取得できない。

### HTTP API

| エンドポイント | メソッド | レスポンス | 説明 |
|---|---|---|---|
| `/sensors` | GET | JSON | GPS データを返す |
| `/help` | GET | HTML | 使い方ページ |

#### `/sensors` レスポンス例

```json
{
  "gps": {
    "fix": true,
    "lat": 35.6812,
    "lon": 139.7671,
    "alt": 10.0,
    "satellites": 8,
    "hdop": 1.2
  }
}
```

### SSE (Server-Sent Events) — port 81

ボタン押下時にリアルタイムでイベントを受信できる。

```javascript
const es = new EventSource('http://192.168.1.56:81/events');
es.onmessage = e => console.log(JSON.parse(e.data));

// イベント例:
// { speaking: true, gps: { fix: true, lat: 35.6812, lon: 139.7671, satellites: 8 } }
```

## ライブラリ・ボード設定

→ [共通設定 README](../../README.md) を参照
