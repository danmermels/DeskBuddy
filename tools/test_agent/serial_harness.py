import re
import threading
import time

import serial

from config import resolve_serial_port


class SerialHarness(threading.Thread):
    def __init__(self, bus, cfg, name="SerialHarness"):
        super().__init__(name=name, daemon=True)
        self.bus = bus
        self.cfg = cfg
        self.port = resolve_serial_port(cfg)
        self.baud = cfg.get("serial_baud", 115200)
        self.ser = None
        self._stop = threading.Event()
        self.error = None
        self.connected = False

    def connect(self):
        if not self.port:
            self.error = "no serial port detected"
            return False
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.5)
            self.connected = True
            return True
        except Exception as e:
            self.error = str(e)
            return False

    def run(self):
        if not self.ser and not self.connect():
            return
        while not self._stop.is_set():
            try:
                line = self.ser.readline().decode("utf-8", errors="replace").rstrip("\r\n")
                if line:
                    self.bus.publish("serial_line", {"line": line, "ts": time.time()})
            except Exception as e:
                self.error = str(e)
                break

    def stop(self):
        self._stop.set()
        try:
            if self.ser:
                self.ser.close()
        except Exception:
            pass
        self.join(timeout=5)

    @staticmethod
    def wait_line(bus, pattern, timeout=10.0):
        rx = re.compile(pattern)
        return bus.wait_for("serial_line",
                            pred=lambda e: rx.search(e["data"].get("line", "")),
                            timeout=timeout)
