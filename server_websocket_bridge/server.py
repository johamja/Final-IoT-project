import asyncio
import json
import os
from pathlib import Path
from typing import Dict, Optional, Set

from fastapi import FastAPI, WebSocket, WebSocketDisconnect, Request
from fastapi.responses import HTMLResponse, FileResponse
from fastapi.middleware.cors import CORSMiddleware

# ----------------------------------------------------------------------------
# App setup
# ----------------------------------------------------------------------------
app = FastAPI(title="WS Bridge: PC(web) -> Server(puente) -> LILYGO(ESP32)")

# CORS (allow local dev from any origin; tighten in prod)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Paths
REPO_ROOT = Path(__file__).resolve().parents[1]
PAGE_DIR = REPO_ROOT / "pagian web"
INDEX_HTML = PAGE_DIR / "index.html"

# ----------------------------------------------------------------------------
# Connection registries
# ----------------------------------------------------------------------------
class TankConnection:
    def __init__(self, tank_id: str, ws: WebSocket):
        self.tank_id = tank_id
        self.ws = ws
        self.lock = asyncio.Lock()  # serialize sends per-connection

# Map of tank_id -> TankConnection (ESP32)
TANKS: Dict[str, TankConnection] = {}

# Clients (browser controllers). We also track which tank a client controls.
class ClientConnection:
    def __init__(self, ws: WebSocket):
        self.ws = ws
        self.tank_id: Optional[str] = None

CLIENTS: Set[ClientConnection] = set()
CLIENTS_LOCK = asyncio.Lock()

# ----------------------------------------------------------------------------
# Helpers
# ----------------------------------------------------------------------------
async def safe_send_json(ws: WebSocket, payload: dict):
    try:
        await ws.send_text(json.dumps(payload))
    except Exception:
        pass

async def broadcast_to_clients_for_tank(tank_id: str, message: dict):
    async with CLIENTS_LOCK:
        to_remove: Set[ClientConnection] = set()
        for c in CLIENTS:
            if c.tank_id == tank_id:
                try:
                    await c.ws.send_text(json.dumps(message))
                except Exception:
                    to_remove.add(c)
        for c in to_remove:
            CLIENTS.discard(c)

# Translate browser UI actions to ESP32 expected commands
# Browser may send action "speed" but ESP expects "setspeed"
ACTION_MAP = {
    "forward": "forward",
    "backward": "backward",
    "left": "left",
    "right": "right",
    "stop": "stop",
    "speed": "setspeed",
    "setspeed": "setspeed",
}

# ----------------------------------------------------------------------------
# HTTP endpoints
# ----------------------------------------------------------------------------
@app.get("/")
async def serve_index() -> HTMLResponse:
    if INDEX_HTML.exists():
        return HTMLResponse(INDEX_HTML.read_text(encoding="utf-8"))
    # Fallback minimal page if file missing
    return HTMLResponse("""
    <html><body>
      <h2>WS Bridge</h2>
      <p>Index not found. Ensure 'pagian web/index.html' exists.</p>
    </body></html>
    """)

@app.get("/health")
async def health():
    return {"status": "ok", "tanks": list(TANKS.keys())}

# ----------------------------------------------------------------------------
# WebSocket: IoT (ESP32) connects here
# ----------------------------------------------------------------------------
@app.websocket("/ws/tank/{tank_id}")
async def ws_tank_endpoint(websocket: WebSocket, tank_id: str):
    await websocket.accept()
    conn = TankConnection(tank_id, websocket)

    # Replace existing connection for this tank if present
    prev = TANKS.get(tank_id)
    if prev is not None:
        try:
            await prev.ws.close()
        except Exception:
            pass
    TANKS[tank_id] = conn

    # Notify controllers that this tank came online
    await broadcast_to_clients_for_tank(tank_id, {"type": "tank_online", "tankId": tank_id})

    try:
        while True:
            msg = await websocket.receive_text()
            # ESP32 can send status payloads; forward to interested clients
            try:
                data = json.loads(msg)
            except json.JSONDecodeError:
                # Forward opaque text as status message
                await broadcast_to_clients_for_tank(tank_id, {"type": "status", "tankId": tank_id, "raw": msg})
                continue

            # Normalize and forward status
            if isinstance(data, dict):
                if data.get("type") == "status":
                    data["tankId"] = tank_id
                    await broadcast_to_clients_for_tank(tank_id, data)
                else:
                    await broadcast_to_clients_for_tank(tank_id, {"type": "status", "tankId": tank_id, "data": data})
            else:
                await broadcast_to_clients_for_tank(tank_id, {"type": "status", "tankId": tank_id, "data": data})
    except WebSocketDisconnect:
        pass
    except Exception:
        pass
    finally:
        # Clean registry and notify clients
        cur = TANKS.get(tank_id)
        if cur is conn:
            TANKS.pop(tank_id, None)
        await broadcast_to_clients_for_tank(tank_id, {"type": "tank_offline", "tankId": tank_id})
        try:
            await websocket.close()
        except Exception:
            pass

# ----------------------------------------------------------------------------
# WebSocket: Browser controller connects here
# ----------------------------------------------------------------------------
@app.websocket("/ws/control")
async def ws_control_endpoint(websocket: WebSocket):
    await websocket.accept()
    client = ClientConnection(websocket)
    async with CLIENTS_LOCK:
        CLIENTS.add(client)

    # Initial hello
    await safe_send_json(websocket, {"type": "hello", "tanksOnline": list(TANKS.keys())})

    try:
        while True:
            msg = await websocket.receive_text()
            try:
                payload = json.loads(msg)
            except json.JSONDecodeError:
                await safe_send_json(websocket, {"type": "error", "error": "invalid_json"})
                continue

            mtype = payload.get("type")

            # Client selects a tank to control
            if mtype == "select":
                tank_id = payload.get("tankId")
                client.tank_id = tank_id
                await safe_send_json(websocket, {"type": "selected", "tankId": tank_id, "online": tank_id in TANKS})
                continue

            # Command from UI -> forward to ESP32 as expected JSON
            if mtype == "cmd":
                tank_id = payload.get("tankId") or client.tank_id
                if not tank_id:
                    await safe_send_json(websocket, {"type": "error", "error": "no_tank_selected"})
                    continue
                tank = TANKS.get(tank_id)
                if not tank:
                    await safe_send_json(websocket, {"type": "error", "error": "tank_offline", "tankId": tank_id})
                    continue

                action = (payload.get("action") or "").lower()
                command = ACTION_MAP.get(action, "stop")
                left = payload.get("leftSpeed")
                right = payload.get("rightSpeed")

                cmd_obj = {"command": command}
                # For setspeed include speeds; for other commands include if provided
                if command == "setspeed":
                    # default to 0 if not provided to avoid stale speeds
                    cmd_obj["leftSpeed"] = int(left) if left is not None else 0
                    cmd_obj["rightSpeed"] = int(right) if right is not None else 0
                else:
                    if left is not None:
                        cmd_obj["leftSpeed"] = int(left)
                    if right is not None:
                        cmd_obj["rightSpeed"] = int(right)

                # Send to ESP32
                try:
                    async with tank.lock:
                        await tank.ws.send_text(json.dumps(cmd_obj))
                    await safe_send_json(websocket, {"type": "ack", "tankId": tank_id, "command": command})
                except Exception as e:
                    await safe_send_json(websocket, {"type": "error", "error": "send_failed", "detail": str(e)})
                continue

            # Unknown messages
            await safe_send_json(websocket, {"type": "error", "error": "unknown_type", "received": mtype})

    except WebSocketDisconnect:
        pass
    except Exception:
        pass
    finally:
        async with CLIENTS_LOCK:
            CLIENTS.discard(client)
        try:
            await websocket.close()
        except Exception:
            pass


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=int(os.getenv("PORT", "8000")))
