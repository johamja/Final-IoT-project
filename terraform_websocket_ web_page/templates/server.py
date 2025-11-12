import asyncio
import json
import os
from pathlib import Path
from typing import Dict, Optional, Set

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI(title="WS Bridge: PC(web) -> Server(puente) -> LILYGO(ESP32)")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Paths: try to serve /app/pagian web/index.html (container layout)
REPO_ROOT = Path(__file__).resolve().parents[1]
PAGE_DIR = REPO_ROOT / "pagian web"
INDEX_HTML = PAGE_DIR / "index.html"

class TankConnection:
    def __init__(self, tank_id: str, ws: WebSocket):
        self.tank_id = tank_id
        self.ws = ws
        self.lock = asyncio.Lock()

TANKS: Dict[str, TankConnection] = {}

class ClientConnection:
    def __init__(self, ws: WebSocket):
        self.ws = ws
        self.tank_id: Optional[str] = None

CLIENTS: Set[ClientConnection] = set()
CLIENTS_LOCK = asyncio.Lock()

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

ACTION_MAP = {
    "forward": "forward",
    "backward": "backward",
    "left": "left",
    "right": "right",
    "stop": "stop",
    "speed": "setspeed",
    "setspeed": "setspeed",
}

@app.get("/")
async def serve_index() -> HTMLResponse:
    if INDEX_HTML.exists():
        return HTMLResponse(INDEX_HTML.read_text(encoding="utf-8"))
    return HTMLResponse("<html><body><h3>WS Bridge</h3><p>Index not found.</p></body></html>")

@app.get("/health")
async def health():
    return {"status": "ok", "tanks": list(TANKS.keys())}

@app.websocket("/ws/tank/{tank_id}")
async def ws_tank_endpoint(websocket: WebSocket, tank_id: str):
    await websocket.accept()
    conn = TankConnection(tank_id, websocket)
    prev = TANKS.get(tank_id)
    if prev is not None:
        try:
            await prev.ws.close()
        except Exception:
            pass
    TANKS[tank_id] = conn
    await broadcast_to_clients_for_tank(tank_id, {"type": "tank_online", "tankId": tank_id})
    try:
        while True:
            msg = await websocket.receive_text()
            try:
                data = json.loads(msg)
            except json.JSONDecodeError:
                await broadcast_to_clients_for_tank(tank_id, {"type": "status", "tankId": tank_id, "raw": msg})
                continue
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
        cur = TANKS.get(tank_id)
        if cur is conn:
            TANKS.pop(tank_id, None)
        await broadcast_to_clients_for_tank(tank_id, {"type": "tank_offline", "tankId": tank_id})
        try:
            await websocket.close()
        except Exception:
            pass

@app.websocket("/ws/control")
async def ws_control_endpoint(websocket: WebSocket):
    await websocket.accept()
    client = ClientConnection(websocket)
    async with CLIENTS_LOCK:
        CLIENTS.add(client)
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
            if mtype == "select":
                tank_id = payload.get("tankId")
                client.tank_id = tank_id
                await safe_send_json(websocket, {"type": "selected", "tankId": tank_id, "online": tank_id in TANKS})
                continue
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
                if command == "setspeed":
                    cmd_obj["leftSpeed"] = int(left) if left is not None else 0
                    cmd_obj["rightSpeed"] = int(right) if right is not None else 0
                else:
                    if left is not None:
                        cmd_obj["leftSpeed"] = int(left)
                    if right is not None:
                        cmd_obj["rightSpeed"] = int(right)
                try:
                    async with tank.lock:
                        await tank.ws.send_text(json.dumps(cmd_obj))
                    await safe_send_json(websocket, {"type": "ack", "tankId": tank_id, "command": command})
                except Exception as e:
                    await safe_send_json(websocket, {"type": "error", "error": "send_failed", "detail": str(e)})
                continue
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
    port = int(os.getenv("PORT", "8000"))
    uvicorn.run(app, host="0.0.0.0", port=port)
