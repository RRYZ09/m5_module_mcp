# m5-module-mcp

M5 Atom Echo のセンサーデータを Claude から呼び出せるツールとして提供する MCP サーバー。

- **ENV PRO (BME688)**: 気温・湿度・気圧・ガス抵抗
- **ATOMIC GPS Base**: GPS 座標・衛星数

## セットアップ

### 1. 依存パッケージのインストール

```bash
uv sync
```

### 2. 環境変数の設定

```bash
cp .env.sample .env
```

`.env` を編集して M5 Atom Echo の IP アドレスを設定する。

### 3. Claude Desktop の設定

`claude_desktop_config.json` に以下を追加：

```json
{
  "mcpServers": {
    "m5-module-mcp": {
      "command": "python",
      "args": ["/path/to/m5_module_mcp/server.py"],
      "env": {
        "M5_ATOM_ECHO_ENV_IP": "192.168.1.55",
        "M5_ATOM_ECHO_GPS_IP": "192.168.1.56"
      }
    }
  }
}
```

## 環境変数

| 変数名 | 説明 | デフォルト |
|--------|------|------------|
| `M5_ATOM_ECHO_ENV_IP` | ENV PRO 機の IP（カンマ区切りで複数指定可） | `192.168.1.55,192.168.8.55` |
| `M5_ATOM_ECHO_GPS_IP` | GPS 機の IP（カンマ区切りで複数指定可） | `192.168.1.56,192.168.8.56` |

複数 IP を指定した場合、最初に応答した IP を使用する。

## ボタンイベント (SSE)

ボタンを押すと M5 Atom Echo がSSE (Server-Sent Events) でイベントを送信する。

**エンドポイント**: `http://<IP>:81/events`

| 操作 | 動作 |
|------|------|
| 短押し | SSEイベント送信（`speaking: false`） |
| 長押し (1秒以上) | speaking 状態をトグルしてSSEイベント送信 |

**イベントフォーマット**:

```
data: {"speaking":true,"env":{"temperature":25.0,"humidity":60,"pressure":1013}}
```

| フィールド | 説明 |
|-----------|------|
| `speaking` | 発話中かどうか (bool) |
| `env.temperature` | 気温 (°C) |
| `env.humidity` | 湿度 (%) |
| `env.pressure` | 気圧 (hPa) |

**受信例 (JavaScript)**:

```js
const es = new EventSource('http://192.168.1.55:81/events');
es.onmessage = (e) => console.log(JSON.parse(e.data));
```

## ツール

### `get_env`

ENV PRO (BME688) から環境データを取得する。

| フィールド | 説明 |
|-----------|------|
| `temperature` | 気温 (°C) |
| `humidity` | 湿度 (%) |
| `pressure` | 気圧 (hPa) |
| `gas_resistance` | ガス抵抗 (Ω) |

### `get_gps`

ATOMIC GPS Base から GPS データを取得する。

| フィールド | 説明 |
|-----------|------|
| `fix` | GPS 測位できているか (bool) |
| `lat` | 緯度 |
| `lon` | 経度 |
| `alt` | 高度 (m) |
| `satellites` | 受信衛星数 |
| `hdop` | 水平精度 |
