import threading
import time

import requests


class TelemetryPoller(threading.Thread):
    def __init__(self, url, interval_ms=500, secondary_interval_ms=2000, bus=None,
                 name="TelemetryPoller"):
        super().__init__(name=name, daemon=True)
        self.url = url.rstrip("/")
        self.interval = interval_ms / 1000.0
        self.secondary_interval = secondary_interval_ms / 1000.0
        self.bus = bus
        self._stop = threading.Event()
        self.last_data = None
        self.last_ok = None
        self.error = None

    def run(self):
        next_secondary = 0.0
        while not self._stop.wait(0):
            started = time.time()
            try:
                r = requests.get(self.url + "/radar-data", timeout=2.0)
                if r.status_code == 200:
                    data = r.json()
                    self.last_data = data
                    self.last_ok = time.time()
                    self.error = None
                    if self.bus:
                        self.bus.publish("telemetry", data)
                else:
                    self.error = "radar-data HTTP %d" % r.status_code
            except Exception as e:
                self.error = str(e)

            if time.time() >= next_secondary:
                next_secondary = time.time() + self.secondary_interval
                try:
                    r = requests.get(self.url + "/api/tft-messages", timeout=2.0)
                    if r.status_code == 200:
                        if self.bus:
                            self.bus.publish("tft_messages", r.json())
                except Exception:
                    pass

            elapsed = time.time() - started
            wait = self.interval - elapsed
            if wait > 0:
                self._stop.wait(wait)

    def stop(self):
        self._stop.set()
        self.join(timeout=5)
