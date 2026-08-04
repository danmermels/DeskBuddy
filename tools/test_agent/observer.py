import json
import os
import time
from collections import Counter, defaultdict


class Observer:
    def __init__(self, ctx, duration=300, probe_interval=30, report_dir=None):
        self.ctx = ctx
        self.duration = duration
        self.probe_interval = probe_interval
        self.report_dir = report_dir or os.path.join(
            os.path.dirname(os.path.abspath(__file__)), "reports")
        os.makedirs(self.report_dir, exist_ok=True)

        self.state = None
        self.state_entered = None
        self.dwell = Counter()
        self.transitions = []
        self.sessions = []
        self.session_open = None
        self.ai_req = 0
        self.ai_resp = 0
        self.ai_latencies = []
        self.ai_errors = 0
        self.log_counts = Counter()
        self.mqtt_drops = 0
        self.mqtt_connected_at = None
        self.mqtt_total_uptime = 0.0
        self.events = []
        self.serial_lines = []
        self.start_epoch = None

    def _on_telemetry(self, ev):
        d = ev["data"]
        st = d.get("state")
        now = ev["ts"]
        if st != self.state:
            prev = self.state
            if self.state is not None and self.state_entered is not None:
                self.dwell[self.state] += now - self.state_entered
            if prev is not None:
                self.transitions.append({"from": prev, "to": st, "ts": now})
                self.events.append({"type": "state", "from": prev, "to": st, "ts": now})
            self.state = st
            self.state_entered = now

    def _on_log(self, ev):
        d = ev["data"]
        cat = d.get("category", "?")
        self.log_counts[cat] += 1
        text = d.get("text", "")
        if "error" in text.lower() or "fail" in text.lower():
            self.ai_errors += 1
        if cat in ("STATE", "BEHAVIOUR"):
            self.events.append({"type": "log", "category": cat, "text": text[:160],
                                "ts": ev["ts"]})

    def _on_ai(self, ev):
        kind = ev["data"].get("kind")
        if kind == "request":
            self.ai_req += 1
            self._ai_last_req = ev["ts"]
        elif kind == "response":
            self.ai_resp += 1
            if getattr(self, "_ai_last_req", None):
                self.ai_latencies.append(ev["ts"] - self._ai_last_req)

    def _on_conn(self, ev):
        d = ev["data"]
        if d.get("connected"):
            if self.mqtt_connected_at is None:
                self.mqtt_connected_at = ev["ts"]
        else:
            if self.mqtt_connected_at is not None:
                self.mqtt_total_uptime += ev["ts"] - self.mqtt_connected_at
                self.mqtt_connected_at = None
            self.mqtt_drops += 1

    def _on_serial(self, ev):
        line = ev["data"].get("line", "")
        if line and len(self.serial_lines) < 500:
            self.serial_lines.append({"line": line[:200], "ts": ev["ts"]})

    def run(self):
        bus = self.ctx.bus
        bus.subscribe("telemetry", self._on_telemetry)
        bus.subscribe("mqtt_log", self._on_log)
        bus.subscribe("ai_trace", self._on_ai)
        bus.subscribe("mqtt_conn", self._on_conn)
        bus.subscribe("serial_line", self._on_serial)

        start = time.time()
        self.start_epoch = start
        if self.ctx.telemetry and self.ctx.telemetry.last_ok:
            print("telemetry connected (error=%s)" % self.ctx.telemetry.error)
        while time.time() - start < self.duration:
            time.sleep(0.5)

        # finalize
        if self.state is not None and self.state_entered is not None:
            self.dwell[self.state] += time.time() - self.state_entered
        if self.mqtt_connected_at is not None:
            self.mqtt_total_uptime += time.time() - self.mqtt_connected_at

        profile = self.build_profile()
        path = os.path.join(self.report_dir, "profile_%s.json" %
                            time.strftime("%Y%m%d_%H%M%S"))
        with open(path, "w", encoding="utf-8") as f:
            json.dump(profile, f, indent=2)
        return profile

    def build_profile(self):
        summary = {}
        if self.state is not None and self.dwell:
            total = sum(self.dwell.values())
            summary["state_dwell_seconds"] = dict(self.dwell)
            summary["state_dwell_pct"] = {k: round(100.0 * v / total, 1)
                                          for k, v in self.dwell.items()}
        summary["state_transition_count"] = len(self.transitions)
        summary["log_counts_by_category"] = dict(self.log_counts)
        summary["ai_requests"] = self.ai_req
        summary["ai_responses"] = self.ai_resp
        summary["ai_avg_latency_s"] = (round(sum(self.ai_latencies) / len(self.ai_latencies), 1)
                                       if self.ai_latencies else None)
        summary["ai_error_lines"] = self.ai_errors
        summary["mqtt_drops"] = self.mqtt_drops
        summary["mqtt_uptime_s"] = round(self.mqtt_total_uptime, 1)
        summary["telemetry_last_error"] = self.ctx.telemetry.error if self.ctx.telemetry else None

        anomalies = []
        flapping = sum(1 for i in range(1, len(self.transitions))
                       if self.transitions[i]["from"] == self.transitions[i - 1]["to"])
        if flapping >= 3:
            anomalies.append("state flapping detected (%d rapid reversals)" % flapping)
        if self.ai_req and self.ai_resp == 0:
            anomalies.append("AI requests with no responses (pipeline stall?)")
        if self.ai_resp and self.ai_req and self.ai_resp < self.ai_req:
            anomalies.append("AI responses (%d) < requests (%d)" % (self.ai_resp, self.ai_req))
        if self.mqtt_drops:
            anomalies.append("MQTT connection drops: %d" % self.mqtt_drops)
        if self.ctx.telemetry and self.ctx.telemetry.error:
            anomalies.append("telemetry poll errors: %s" % self.ctx.telemetry.error)

        return {
            "mode": "passive",
            "started_epoch": self.start_epoch,
            "duration_s": self.duration,
            "summary": summary,
            "anomalies": anomalies,
            "transitions": self.transitions,
            "events": self.events[-200:],
            "serial_lines": self.serial_lines,
        }
