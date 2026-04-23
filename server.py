"""
M5 Module MCP Server

2台の M5 Atom Echo のセンサーデータを Claude から呼び出せるツールとして提供する MCP サーバー。
  - ENV PRO (BME688): 気温・湿度・気圧
  - ATOMIC GPS Base:  GPS 座標・衛星数

使い方:
  python server.py

環境変数 (カンマ区切りで複数 IP を指定するとフォールバック):
  M5_ATOM_ECHO_ENV_IP  ENV PRO 機の IP (デフォルト: 192.168.1.55,192.168.8.55)
  M5_ATOM_ECHO_GPS_IP  GPS 機の IP     (デフォルト: 192.168.1.56,192.168.8.56)
"""

import os
import sys
import httpx
import logging
from mcp.server.fastmcp import FastMCP

logging.basicConfig(level=logging.INFO, handlers=[logging.StreamHandler(sys.stderr)])
logger = logging.getLogger(__name__)

ENV_IPS = [ip.strip() for ip in os.environ.get("M5_ATOM_ECHO_ENV_IP", "192.168.1.55,192.168.8.55").split(",")]
GPS_IPS = [ip.strip() for ip in os.environ.get("M5_ATOM_ECHO_GPS_IP", "192.168.1.56,192.168.8.56").split(",")]

mcp = FastMCP("m5-module-mcp")


async def _fetch(ips: list[str], path: str) -> dict:
    last_error = {}
    for ip in ips:
        try:
            async with httpx.AsyncClient(timeout=3.0) as client:
                res = await client.get(f"http://{ip}{path}")
                res.raise_for_status()
                return res.json()
        except httpx.ConnectError:
            last_error = {"error": f"M5 Atom Echo ({ip}) に接続できません"}
        except httpx.TimeoutException:
            last_error = {"error": f"M5 Atom Echo ({ip}) タイムアウト"}
        except Exception as e:
            last_error = {"error": str(e)}
    return last_error


@mcp.tool()
async def get_env() -> dict:
    """
    ENV PRO (BME688) から気温・湿度・気圧・ガス抵抗を取得する。

    Returns:
        env.temperature   : 気温 (°C)
        env.humidity      : 湿度 (%)
        env.pressure      : 気圧 (hPa)
        env.gas_resistance: ガス抵抗 (Ω)
    """
    data = await _fetch(ENV_IPS, "/sensors")
    if "error" in data:
        return data
    return data.get("env", data)


@mcp.tool()
async def get_ble_rssi() -> dict:
    """
    ENV PRO 機の BLE スキャン結果から Garmin ウォッチとスマホの電波強度を取得する。
    Garmin は心拍転送モード（ハートレートブロードキャスト）が必要。

    Returns:
        scanning        : スキャン実行中かどうか
        garmin.rssi     : Garmin の RSSI (dBm)、未検出なら 0
        garmin.found    : 直近のスキャンで見つかったか
        phone.rssi      : スマホの RSSI (dBm)、未検出なら 0
        phone.found     : 直近のスキャンで見つかったか
    """
    return await _fetch(ENV_IPS, "/ble_rssi")


@mcp.tool()
async def get_gps() -> dict:
    """
    ATOMIC GPS Base から GPS データを取得する。

    Returns:
        gps.fix       : GPS 測位できているか (bool)
        gps.lat       : 緯度
        gps.lon       : 経度
        gps.alt       : 高度 (m)
        gps.satellites: 受信衛星数
        gps.hdop      : 水平精度
    """
    data = await _fetch(GPS_IPS, "/sensors")
    if "error" in data:
        return data
    return data.get("gps", data)


if __name__ == "__main__":
    logger.info(f"ENV PRO IPs: {ENV_IPS}")
    logger.info(f"GPS IPs:     {GPS_IPS}")
    mcp.run(transport="stdio")
