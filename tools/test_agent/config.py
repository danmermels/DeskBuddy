import json
import os

CONFIG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")

DEFAULTS = {
    "broker": "192.168.15.18",
    "broker_port": 1883,
    "http_url": "http://192.168.15.160",
    "telemetry_interval_ms": 500,
    "secondary_interval_ms": 2000,
    "serial_port": "auto",
    "serial_baud": 115200,
    "serial_enabled": False,
    "probe_interval_s": 30,
    "default_timeout_s": 10,
    "allow_ai": False,
    "mqtt_client_id": "deskbuddy-test-agent",
}


def load_config(path=CONFIG_FILE):
    cfg = dict(DEFAULTS)
    if os.path.exists(path):
        try:
            with open(path, "r", encoding="utf-8") as f:
                cfg.update(json.load(f))
        except Exception:
            pass
    return cfg


def save_config(cfg, path=CONFIG_FILE):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2)


def detect_serial_port():
    try:
        import serial.tools.list_ports as ports
    except ImportError:
        return None
    for p in ports.comports():
        vid = (p.vid or 0) & 0xFFFF
        if vid in (0x303A, 0x10C4, 0x1A86):
            return p.device
    return None


def resolve_serial_port(cfg):
    if cfg.get("serial_port") and cfg["serial_port"] not in ("auto", ""):
        return cfg["serial_port"]
    return detect_serial_port()
