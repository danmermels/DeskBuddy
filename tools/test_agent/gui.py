import argparse
import json
import os
import sys
import threading
import time
import tkinter as tk
from collections import deque

import customtkinter as ctk

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from config import load_config
from livebus import LiveBus
from mqtt_harness import MqttHarness
from telemetry import TelemetryPoller
import runner
from runner import Context, run_suite


def _load_patterns():
    import patterns
    return [fn for _, fn in sorted(vars(patterns).items())
            if _.startswith("pattern_") and callable(fn)]

# --- palette --------------------------------------------------------------
BG       = "#16161e"
PANEL    = "#1f1f2b"
GRID     = "#2c2c3c"
FG       = "#e2e8f0"
FG_DIM   = "#94a3b8"
DIST_CLR = "#22d3ee"
RATIO_CLR= "#e879f9"
ZONE_FOCUS = "#10b981"
ZONE_BUSY  = "#f59e0b"
ZONE_DISTR = "#ef4444"
ZONE_REG   = "#3b82f6"

MOOD_COLORS = {
    "Focus": ZONE_FOCUS, "Busy": ZONE_BUSY,
    "Distracted": ZONE_DISTR, "Normal": ZONE_REG,
    "Regular Activity": ZONE_REG,  # device-reported name for Normal
}

MAX_WINDOW_S = 30 * 60
MAX_SAMPLES = MAX_WINDOW_S * 2 + 20


class MoodChart:
    def __init__(self, master, width=920, height=430):
        self.frame = ctk.CTkFrame(master, fg_color=PANEL, corner_radius=10)
        self.buffer = deque(maxlen=MAX_SAMPLES)  # (t, dist, ratio)
        self.window_s = 300
        self.focus_dist_lim = 50.0
        self.motion_ratio_lim = 15.0
        self.dist_limit = 160.0
        self.state = "Away"

        self.canvas = __import__("tkinter").Canvas(
            self.frame, width=width, height=height, bg=BG,
            highlightthickness=0)
        self.canvas.pack(fill="both", expand=True)
        self.frame.bind("<Configure>", lambda e: self._redraw())

    # -- data ----------------------------------------------------------------
    def push(self, tele):
        t = time.time()
        dist = tele.get("detectionDist")
        ratio = tele.get("recentMotionRatio")
        if ratio is None:
            ratio = tele.get("motionRatio")
        dist = float(dist) if dist is not None else None
        ratio = float(ratio) if ratio is not None else None
        if dist is not None or ratio is not None:
            self.buffer.append((t, dist, ratio))
        for key, attr in (("focusDistLim", "focus_dist_lim"),
                          ("motionRatioLim", "motion_ratio_lim"),
                          ("distLimit", "dist_limit")):
            v = tele.get(key)
            if v is not None:
                setattr(self, attr, float(v))
        if tele.get("state"):
            self.state = tele["state"]

    def set_window(self, seconds):
        self.window_s = seconds
        self._redraw()

    # -- drawing --------------------------------------------------------------
    def _redraw(self):
        c = self.canvas
        c.delete("all")
        w = max(c.winfo_width(), 60)
        h = max(c.winfo_height(), 60)
        ml, mr, mt, mb = 46, 50, 30, 26
        pw, ph = w - ml - mr, h - mt - mb

        now = time.time()
        t0 = now - self.window_s

        # --- background mood zones (near/far x low/high bands) ---
        maxd = max(self.dist_limit * 1.1, 200.0)
        y_dist = lambda v: mt + ph - (v / maxd) * ph
        y_ratio = lambda v: mt + ph - (v / 100.0) * ph
        dy = y_dist(min(self.focus_dist_lim, maxd))
        ry = y_ratio(self.motion_ratio_lim)
        low_line, high_line = sorted((dy, ry))
        bands = []
        bands.append((mt, low_line, self._zone_for(True, True)))       # below both
        bands.append((low_line, high_line, self._zone_for(dy < ry, ry < dy)))
        bands.append((high_line, mt + ph, self._zone_for(False, False)))  # above both
        for y0, y1, zone in bands:
            if y1 - y0 < 2:
                continue
            fill = MOOD_COLORS.get(zone)
            c.create_rectangle(ml, y0, ml + pw, y1,
                               fill=fill, stipple="gray50", outline="")

        # --- grid ---
        for i in range(5):
            gx = ml + pw * i / 4
            c.create_line(gx, mt, gx, mt + ph, fill=GRID, dash=(2, 4))
            lbl = self._fmt_clock(t0 + (now - t0) * i / 4)
            c.create_text(gx, mt + ph + 12, text=lbl, fill=FG_DIM, font=("Consolas", 8))
        for i in range(5):
            gy = mt + ph * i / 4
            c.create_line(ml, gy, ml + pw, gy, fill=GRID, dash=(2, 4))

        # --- threshold lines ---
        c.create_line(ml, dy, ml + pw, dy, fill=DIST_CLR, dash=(6, 3), width=1)
        c.create_line(ml, ry, ml + pw, ry, fill=RATIO_CLR, dash=(6, 3), width=1)

        # --- series ---
        pts = [p for p in self.buffer if p[0] >= t0 - 1]
        step = max(1, len(pts) // max(pw, 1) + 1)
        dist_poly, ratio_poly = [], []
        for i in range(0, len(pts), step):
            t, d, r = pts[i]
            x = ml + pw * (t - t0) / self.window_s
            if d is not None:
                dist_poly.append((x, y_dist(min(d, maxd))))
            if r is not None:
                ratio_poly.append((x, y_ratio(max(0.0, min(100.0, r)))))
        if len(dist_poly) > 1:
            c.create_line(*[v for p in dist_poly for v in p],
                          fill=DIST_CLR, width=2, smooth=True)
        if len(ratio_poly) > 1:
            c.create_line(*[v for p in ratio_poly for v in p],
                          fill=RATIO_CLR, width=2, smooth=True)

        # --- axes ---
        for i in range(5):
            v = maxd * i / 4
            y = y_dist(v)
            c.create_text(ml - 6, y, text="%d" % v, fill=FG_DIM,
                          anchor="e", font=("Consolas", 8))
            v = 100 - 100 * i / 4
            y = y_ratio(v)
            c.create_text(ml + pw + 6, y, text="%d" % v, fill=FG_DIM,
                          anchor="w", font=("Consolas", 8))
        c.create_text(ml + 8, mt - 18, text="distance (cm)", fill=DIST_CLR,
                      anchor="w", font=("Consolas", 9, "bold"))
        c.create_text(ml + pw - 8, mt - 18, text="motion ratio (%)", fill=RATIO_CLR,
                      anchor="e", font=("Consolas", 9, "bold"))

        # --- current values ---
        if pts:
            t, d, r = pts[-1]
            rows = []
            if d is not None:
                rows.append("dist %.0f cm" % d)
            if r is not None:
                rows.append("ratio %.0f%%" % r)
            for i, txt in enumerate(rows):
                c.create_text(ml + pw, mt + 16 + i * 14, text=txt,
                              fill=FG, anchor="ne", font=("Consolas", 9))

        # --- mood chip ---
        col = MOOD_COLORS.get(self.state, "#64748b")
        chip_w = 14 + len(self.state) * 7
        c.create_rectangle(ml, mt, ml + chip_w, mt + 18, fill=col, outline="")
        c.create_text(ml + chip_w / 2, mt + 9, text=self.state,
                      fill="#0f172a", font=("Consolas", 9, "bold"))

        # --- zone legend (2x2) ---
        lx, ly = ml + 10, mt + 22
        items = [("Focus", ZONE_FOCUS), ("Busy", ZONE_BUSY),
                 ("Normal", ZONE_REG), ("Distracted", ZONE_DISTR)]
        c.create_text(lx, ly - 2, text="mood zones", fill=FG_DIM,
                      anchor="w", font=("Consolas", 8))
        for i, (name, colr) in enumerate(items):
            x = lx + (i % 2) * 110
            y = ly + 14 + (i // 2) * 15
            c.create_rectangle(x, y, x + 8, y + 8, fill=colr, outline="")
            c.create_text(x + 12, y + 4, text=name, fill=FG_DIM,
                          anchor="w", font=("Consolas", 8))

    @staticmethod
    def _zone_for(near, low):
        if near and low:
            return "Focus"
        if near:
            return "Busy"
        if low:
            return "Normal"
        return "Distracted"

    @staticmethod
    def _fmt_clock(t):
        lt = time.localtime(t)
        return "%02d:%02d:%02d" % (lt.tm_hour, lt.tm_min, lt.tm_sec)


class TestRunnerThread(threading.Thread):
    def __init__(self, ctx, suites, slow, patterns):
        super().__init__(name="GUITestRunner", daemon=True)
        self.ctx = ctx
        self.suites = suites
        self.slow = slow
        self.patterns = patterns
        self.done = threading.Event()

    def run(self):
        try:
            for s in self.suites:
                run_suite(self.ctx, s, include_slow=self.slow)
            if self.patterns:
                self.ctx.sim_stop()
                for pfn in _load_patterns():
                    runner.run_pattern(self.ctx, pfn)
        finally:
            self.done.set()


class ScenarioRunnerThread(threading.Thread):
    def __init__(self, ctx, mod, force=False):
        super().__init__(name="GUIScenarioRunner", daemon=True)
        self.ctx = ctx
        self.mod = mod
        self.force = force
        self.report = None
        self.done = threading.Event()

    def run(self):
        try:
            from scenario_runner import run_scenario
            self.report = run_scenario(self.ctx, self.mod, force=self.force, verbose=False)
        except Exception as e:
            self.report = {"scenario": getattr(self.mod, "NAME", "?"), "window": "?",
                           "verdict": "error", "reason": "%s: %s" % (type(e).__name__, e),
                           "duration_s": 0.0, "expectations": [], "timeline": [],
                           "received": None}
        finally:
            self.done.set()


class ShadowRunnerThread(threading.Thread):
    def __init__(self, ctx, duration=600):
        super().__init__(name="GUIShadowRunner", daemon=True)
        self.ctx = ctx
        self.duration = duration
        self.report = None
        self.done = threading.Event()

    def run(self):
        try:
            from shadow import ShadowObserver
            obs = ShadowObserver(self.ctx, duration=self.duration)
            self.report = obs.run()
        except Exception as e:
            self.report = {"error": "%s: %s" % (type(e).__name__, e)}
        finally:
            self.done.set()


class OracleRunnerThread(threading.Thread):
    def __init__(self, profile, model=None):
        super().__init__(name="GUIOracleRunner", daemon=True)
        self.profile = profile
        self.model = model
        self.result = None
        self.done = threading.Event()

    def run(self):
        try:
            from oracle import analyze_profile
            self.result = analyze_profile(self.profile, model=self.model)
        except Exception as e:
            self.result = "Oracle analysis failed: %s" % e
        finally:
            self.done.set()


class CalibrationScriptThread(threading.Thread):
    """Runs the raw-signal calibration: sets the firmware motion window to the
    calibrator's short window, then runs Close / Far / Still / Moving, watching
    each pose's filtered value until it stabilizes before moving on."""
    def __init__(self, bus, mqtt, behaviours):
        super().__init__(name="GUICalibration", daemon=True)
        self.bus = bus
        self.mqtt = mqtt
        self.behaviours = behaviours
        self.current = None
        self.progress = "starting..."
        self.instruct = ""
        self.value = ""          # live filtered value while waiting for stability
        self.captures = {}       # name -> pose result dict
        self.raw = {}            # name -> list of raw samples
        self.done = threading.Event()

    def run(self):
        try:
            from calibrate import (MOTION_WINDOW_S, START_DELAY_S, POSE_SPECS,
                                   RawCapture, run_pose)
            self.progress = "setting classifier motion window to %ds..." % MOTION_WINDOW_S
            self.mqtt.cmd("SET config.motionWindow %d" % MOTION_WINDOW_S)
            cap = RawCapture(self.bus)
            cap.start()
            try:
                for name in self.behaviours:
                    self.current = name
                    self.instruct = POSE_SPECS[name]["instruct"]
                    cap.clear()
                    self.progress = "%s: get into position - starting in %ds" % (
                        name, START_DELAY_S)
                    time.sleep(START_DELAY_S)
                    self.captures[name] = run_pose(cap, name, progress=self._on_progress)
                    self.raw[name] = list(cap.samples)
            finally:
                cap.stop()
        except Exception as e:
            for name in self.behaviours:
                if name not in self.captures:
                    self.captures[name] = {"name": name,
                                           "error": "%s: %s" % (type(e).__name__, e)}
        finally:
            self.done.set()

    def _on_progress(self, info):
        v = info["value"]
        if info["kind"] == "distance":
            disp = ("stable %.0f cm" % v) if (info["stable"] and v) else \
                   (("~%.0f cm" % v) if v else "waiting for distance...")
        else:
            disp = ("%.0f%% (stable)" % v) if (info["stable"] and v is not None) else \
                   (("~%.0f%%" % v) if v is not None else "waiting for motion...")
        self.value = disp
        self.progress = "%s: holding - %s (%.0fs)" % (info["name"], disp, info["elapsed"])


class App(ctk.CTk):
    def __init__(self, cfg):
        super().__init__()
        self.cfg = cfg
        self.title("DeskBuddy Test Agent")
        self.geometry("1200x820")
        ctk.set_appearance_mode("dark")

        self.bus = LiveBus()
        self.mqtt = MqttHarness(self.bus, broker=cfg["broker"], port=cfg["broker_port"])
        self.mqtt.start()
        self.tele = TelemetryPoller(cfg["http_url"], cfg.get("telemetry_interval_ms", 500),
                                    cfg.get("secondary_interval_ms", 2000), self.bus)
        self.tele.start()

        self.ser = None
        if cfg.get("serial_enabled"):
            try:
                from serial_harness import SerialHarness
                self.ser = SerialHarness(self.bus, cfg)
                self.ser.start()
            except Exception:
                self.ser = None

        self.reporter = lambda res: self.bus.publish("test_result", res.to_dict())
        self.ctx = Context(self.bus, self.mqtt, cfg, self.tele, self.ser, self.reporter)
        self.runner_thread = None
        self.scenario_thread = None
        self.shadow_thread = None
        self.oracle_thread = None
        self.cal_thread = None
        self.cal_results = {}
        self.cal_captures = {}
        self.cal_raw = {}
        self.last_shadow_profile = None
        self.scenario_mods = {}
        self._load_scenario_mods()

        self._queues = {k: deque() for k in (
            "telemetry", "mqtt_resp", "mqtt_log", "mqtt_echo", "ai_trace",
            "serial_line", "test_result", "mqtt_conn", "heap")}
        for k in self._queues:
            self.bus.subscribe(k, lambda e, k=k: self._queues[k].append(e))

        self._build_ui()

        self._tick()
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # -- ui -------------------------------------------------------------------
    def _build_ui(self):
        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(0, weight=1)

        left = ctk.CTkFrame(self, width=240, corner_radius=0, fg_color=PANEL)
        left.grid(row=0, column=0, sticky="nsew")
        left.grid_propagate(False)

        ctk.CTkLabel(left, text="DeskBuddy", font=ctk.CTkFont(size=18, weight="bold"),
                     text_color=DIST_CLR).pack(pady=(14, 2))
        self.status_lbl = ctk.CTkLabel(left, text="connecting...", text_color=FG_DIM)
        self.status_lbl.pack(pady=(0, 10))

        ctk.CTkLabel(left, text="Command", font=ctk.CTkFont(size=12, weight="bold"),
                     text_color=FG).pack(anchor="w", padx=12)
        self.cmd_entry = ctk.CTkEntry(left, placeholder_text="e.g. GET state, SIM sit 40")
        self.cmd_entry.pack(fill="x", padx=12, pady=(2, 6))
        self.cmd_entry.bind("<Return>", lambda e: self._send_cmd())
        ctk.CTkButton(left, text="Send", command=self._send_cmd,
                      fg_color=DIST_CLR, text_color="#0f172a").pack(fill="x", padx=12, pady=(0, 10))

        ctk.CTkLabel(left, text="Quick actions", font=ctk.CTkFont(size=12, weight="bold"),
                     text_color=FG).pack(anchor="w", padx=12)
        q = ctk.CTkFrame(left, fg_color="transparent")
        q.pack(fill="x", padx=12)
        for text, cmd in (("GET state", "GET state"), ("GET stats", "GET stats"),
                          ("SIM sit", "SIM sit 40"), ("SIM away", "SIM state AWAY"),
                          ("SIM stop", "SIM stop"), ("TRIGGER 2", "TRIGGER 2 fallback")):
            ctk.CTkButton(q, text=text, command=lambda c=cmd: self._send(c),
                          height=26, fg_color=GRID, text_color=FG,
                          font=ctk.CTkFont(size=11)).pack(fill="x", pady=2)

        ctk.CTkLabel(left, text="Suites", font=ctk.CTkFont(size=12, weight="bold"),
                     text_color=FG).pack(anchor="w", padx=12, pady=(12, 2))
        self.suite_var = ctk.StringVar(value="protocol")
        self.suite_combo = ctk.CTkOptionMenu(
            left, values=["protocol", "presence", "modes", "behaviour", "all"],
            variable=self.suite_var, fg_color=GRID, button_color=GRID,
            text_color=FG).pack(fill="x", padx=12, pady=(0, 4))
        self.slow_var = ctk.BooleanVar(value=False)
        ctk.CTkCheckBox(left, text="include slow tests", variable=self.slow_var,
                        text_color=FG_DIM).pack(anchor="w", padx=12)
        self.patterns_var = ctk.BooleanVar(value=False)
        ctk.CTkCheckBox(left, text="run patterns", variable=self.patterns_var,
                        text_color=FG_DIM).pack(anchor="w", padx=12)
        ctk.CTkButton(left, text="Run", command=self._run_tests,
                      fg_color=ZONE_FOCUS, text_color="#0f172a").pack(fill="x", padx=12, pady=8)
        self.run_lbl = ctk.CTkLabel(left, text="", text_color=FG_DIM)
        self.run_lbl.pack(anchor="w", padx=12)

        # --- tabs ---------------------------------------------------------
        tabs = ctk.CTkTabview(self, fg_color=BG)
        tabs.grid(row=0, column=1, sticky="nsew", padx=(4, 0), pady=0)
        for name in ("Telemetry", "Mood Chart", "Command / Response",
                     "Log Stream", "Serial", "AI Pipeline", "Test Results",
                     "Scenarios", "Oracle", "Calibration", "Health"):
            tabs.add(name)

        self.chart = MoodChart(tabs.tab("Mood Chart"), width=920, height=430)
        self.chart.frame.pack(fill="both", expand=True, padx=8, pady=8)
        win_row = ctk.CTkFrame(tabs.tab("Mood Chart"), fg_color="transparent")
        win_row.pack(pady=(0, 6))
        for s, w in (("5m", 300), ("10m", 600), ("30m", 1800)):
            ctk.CTkButton(win_row, text=s, width=60, height=26,
                          command=lambda w=w: self.chart.set_window(w),
                          fg_color=GRID, text_color=FG).pack(side="left", padx=4)
        ctk.CTkButton(win_row, text="Pop out", width=80, height=26,
                      command=self._popout_chart, fg_color=GRID,
                      text_color=FG).pack(side="left", padx=4)

        self.tele_tab = self._build_telemetry(tabs.tab("Telemetry"))
        self.cmd_text = ctk.CTkTextbox(tabs.tab("Command / Response"), wrap="none",
                                       fg_color=BG, text_color=FG)
        self.cmd_text.pack(fill="both", expand=True, padx=6, pady=6)
        self.log_text = ctk.CTkTextbox(tabs.tab("Log Stream"), wrap="none",
                                       fg_color=BG, text_color=FG)
        self.log_text.pack(fill="both", expand=True, padx=6, pady=6)
        serial_row = ctk.CTkFrame(tabs.tab("Serial"), fg_color="transparent")
        serial_row.pack(fill="x", padx=6, pady=(6, 0))
        self.serial_btn = ctk.CTkButton(
            serial_row, text="Disconnect serial" if self.ser else "Connect serial",
            command=self._toggle_serial, width=140, height=26, fg_color=GRID,
            text_color=FG, font=ctk.CTkFont(size=11))
        self.serial_btn.pack(side="left")
        ctk.CTkLabel(serial_row, text="raw device serial output (MQTT logs already cover status)",
                     text_color=FG_DIM, font=ctk.CTkFont(size=11)).pack(side="left", padx=10)
        self.serial_text = ctk.CTkTextbox(tabs.tab("Serial"), wrap="none",
                                          fg_color=BG, text_color=FG)
        self.serial_text.pack(fill="both", expand=True, padx=6, pady=6)
        if not self.ser:
            self.serial_text.insert("end", "(serial not connected)\n")
        self.ai_text = ctk.CTkTextbox(tabs.tab("AI Pipeline"), wrap="word",
                                      fg_color=BG, text_color=FG)
        self.ai_text.pack(fill="both", expand=True, padx=6, pady=6)
        self.results_text = ctk.CTkTextbox(tabs.tab("Test Results"), wrap="none",
                                           fg_color=BG, text_color=FG)
        self.results_text.pack(fill="both", expand=True, padx=6, pady=6)
        self._build_scenarios(tabs.tab("Scenarios"))
        self._build_oracle(tabs.tab("Oracle"))
        self._build_calibration(tabs.tab("Calibration"))
        self.health_text = ctk.CTkTextbox(tabs.tab("Health"), wrap="none",
                                          fg_color=BG, text_color=FG)
        self.health_text.pack(fill="both", expand=True, padx=6, pady=6)

    def _build_telemetry(self, tab):
        grid = ctk.CTkFrame(tab, fg_color="transparent")
        grid.pack(fill="both", expand=True, padx=10, pady=10)
        fields = ["state", "detectionDist", "motionRatio", "recentMotionRatio",
                  "focusDistLim", "motionRatioLim", "sessionDistAvg",
                  "focusTime", "sessionDeskTime"]
        widgets = {}
        for i, name in enumerate(fields):
            r, c = divmod(i, 2)
            box = ctk.CTkFrame(grid, fg_color=PANEL, corner_radius=8)
            box.grid(row=r, column=c, sticky="ew", padx=6, pady=6)
            box.columnconfigure(1, weight=1)
            ctk.CTkLabel(box, text=name, text_color=FG_DIM,
                         font=ctk.CTkFont(size=11)).grid(row=0, column=0, sticky="w", padx=10)
            val = ctk.CTkLabel(box, text="-", text_color=FG,
                               font=ctk.CTkFont(size=14, weight="bold"))
            val.grid(row=1, column=0, columnspan=2, sticky="w", padx=10, pady=(0, 8))
            widgets[name] = val
            grid.columnconfigure(c, weight=1)
        return widgets

    # -- actions ---------------------------------------------------------------
    def _load_scenario_mods(self):
        try:
            from scenario_runner import load_scenario_modules, describe_scenario
            self.scenario_mods = {n: describe_scenario(m)
                                  for n, m in load_scenario_modules().items()}
        except Exception:
            self.scenario_mods = {}

    def _build_scenarios(self, tab):
        top = ctk.CTkFrame(tab, fg_color="transparent")
        top.pack(fill="x", padx=8, pady=(8, 2))
        self.mode_var = ctk.StringVar(value="active")
        ctk.CTkSegmentedButton(top, values=["active", "passive"],
                               variable=self.mode_var, command=self._on_mode_change,
                               fg_color=GRID, selected_color=ZONE_FOCUS,
                               selected_hover_color=ZONE_BUSY, text_color=FG,
                               width=140).pack(side="left", padx=(0, 10))
        names = sorted(self.scenario_mods)
        self.scen_var = ctk.StringVar(value=names[0] if names else "")
        ctk.CTkLabel(top, text="Scenario", text_color=FG_DIM).pack(side="left", padx=(0, 4))
        self.scen_menu = ctk.CTkOptionMenu(top, values=names, variable=self.scen_var,
                                           fg_color=GRID, button_color=GRID, text_color=FG,
                                           width=190)
        self.scen_menu.pack(side="left", padx=(0, 8))
        self.scen_force = ctk.BooleanVar(value=False)
        self.scen_force_box = ctk.CTkCheckBox(top, text="force (ignore window)",
                                              variable=self.scen_force, text_color=FG_DIM)
        self.scen_force_box.pack(side="left", padx=(0, 8))
        ctk.CTkLabel(top, text="min", text_color=FG_DIM).pack(side="left")
        self.shadow_min = ctk.StringVar(value="10")
        self.shadow_min_entry = ctk.CTkEntry(top, textvariable=self.shadow_min, width=48,
                                             fg_color=GRID, text_color=FG)
        self.shadow_min_entry.pack(side="left", padx=(4, 8))
        self.shadow_min_entry.configure(state="disabled")
        ctk.CTkButton(top, text="Run", command=self._run_scenario,
                      fg_color=ZONE_FOCUS, text_color="#0f172a", width=80
                      ).pack(side="left", padx=(0, 8))
        self.scen_lbl = ctk.CTkLabel(top, text="", text_color=FG_DIM)
        self.scen_lbl.pack(side="left")

        self.scen_list = ctk.CTkTextbox(tab, wrap="none", fg_color=BG, text_color=FG,
                                        height=120)
        self.scen_list.pack(fill="x", padx=8, pady=4)
        self.scen_text = ctk.CTkTextbox(tab, wrap="none", fg_color=BG, text_color=FG)
        self.scen_text.pack(fill="both", expand=True, padx=8, pady=4)
        self.scen_text.insert("end", "(select a scenario and press Run)\n")
        self._refresh_scenario_list()

    def _refresh_scenario_list(self):
        lines = []
        for n in sorted(self.scenario_mods):
            m = self.scenario_mods[n]
            lines.append("%-22s window=%-20s cadence=%3d min" %
                         (n, m["window"] or "always", m["cadence_min"]))
        self.scen_list.delete("1.0", "end")
        self.scen_list.insert("end", "\n".join(lines) + "\n")

    def _run_scenario(self):
        if self.mode_var.get() == "passive":
            self._run_shadow()
            return
        name = self.scen_var.get()
        if not name or (self.scenario_thread and self.scenario_thread.is_alive()):
            return
        try:
            from scenario_runner import load_scenario_modules
            mod = load_scenario_modules().get(name)
        except Exception:
            mod = None
        if mod is None:
            self.scen_lbl.configure(text="unknown scenario", text_color=ZONE_DISTR)
            return
        self.scen_lbl.configure(text="running %s..." % name, text_color=FG_DIM)
        self.scen_text.delete("1.0", "end")
        self.scen_text.insert("end", "running %s...\n" % name)
        self.scenario_thread = ScenarioRunnerThread(self.ctx, mod,
                                                    force=self.scen_force.get())
        self.scenario_thread.start()

    def _run_shadow(self):
        if self.shadow_thread and self.shadow_thread.is_alive():
            return
        try:
            minutes = max(1, int(float(self.shadow_min.get())))
        except ValueError:
            minutes = 10
        self.scen_lbl.configure(text="shadow observing %d min..." % minutes,
                                text_color=FG_DIM)
        self.scen_text.delete("1.0", "end")
        self.scen_text.insert(
            "end", "shadow observation (%d min): watching for stretch reminder, break-return\n"
            "welcome, first-sit greeting, classifier sanity, motion ratio range.\n"
            "No SIM commands are injected - behave normally.\n\n" % minutes)
        self.shadow_thread = ShadowRunnerThread(self.ctx, duration=minutes * 60)
        self.shadow_thread.start()

    def _on_mode_change(self, _=None):
        passive = self.mode_var.get() == "passive"
        self.scen_menu.configure(state="disabled" if passive else "normal")
        self.scen_force.set(False)
        self.scen_force_box.configure(state="disabled" if passive else "normal")
        self.shadow_min_entry.configure(state="normal" if passive else "disabled")

    def _check_scenario(self):
        th = self.scenario_thread
        if th is not None and th.done.is_set():
            rep = th.report or {}
            verdict = rep.get("verdict", "?")
            color = {"pass": ZONE_FOCUS, "fail": ZONE_DISTR,
                     "skip": FG_DIM, "error": ZONE_DISTR}.get(verdict, FG)
            npass = sum(1 for e in rep.get("expectations", []) if e["ok"])
            self.scen_lbl.configure(
                text="%s: %s (%d/%d exps, %.0fs)" % (
                    rep.get("scenario"), verdict, npass,
                    len(rep.get("expectations", [])), rep.get("duration_s", 0)),
                text_color=color)
            try:
                from scenario_runner import format_report
                self.scen_text.delete("1.0", "end")
                self.scen_text.insert("end", format_report(rep))
            except Exception:
                pass
            self.scenario_thread = None
        sh = self.shadow_thread
        if sh is not None and sh.done.is_set():
            rep = sh.report or {}
            if rep.get("error"):
                self.scen_lbl.configure(text="shadow error: %s" % rep["error"],
                                        text_color=ZONE_DISTR)
            else:
                rows = rep.get("shadow_checks", [])
                npass = sum(1 for r in rows if r["status"] == "pass")
                nfail = sum(1 for r in rows if r["status"] == "fail")
                nne = sum(1 for r in rows if r["status"] == "not-exercised")
                npen = sum(1 for r in rows if r["status"] == "pending")
                self.scen_lbl.configure(
                    text="shadow: %d pass, %d fail, %d not-exercised, %d pending (%.0fs)" % (
                        npass, nfail, nne, npen, rep.get("duration_s", 0)),
                    text_color=ZONE_FOCUS if nfail == 0 else ZONE_DISTR)
                lines = []
                for r in rows:
                    lines.append("%-22s %-14s %s" % (r["name"], r["status"], r["expected"]))
                    if r.get("received"):
                        lines.append("%-22s %-14s received: %s" % ("", "", r["received"]))
                self.scen_text.delete("1.0", "end")
                self.scen_text.insert("end", "\n".join(lines) + "\n")
            self.last_shadow_profile = rep
            if self.oracle_var.get():
                self._run_oracle()
            self.shadow_thread = None

    def _build_oracle(self, tab):
        top = ctk.CTkFrame(tab, fg_color="transparent")
        top.pack(fill="x", padx=8, pady=(8, 2))
        self.oracle_var = ctk.BooleanVar(value=False)
        self.oracle_box = ctk.CTkCheckBox(top, text="LLM oracle analysis",
                                          variable=self.oracle_var, text_color=FG_DIM)
        self.oracle_box.pack(side="left", padx=(0, 10))
        try:
            from oracle import DEFAULT_MODEL
            default_model = DEFAULT_MODEL
        except Exception:
            default_model = "llama-3.3-70b-versatile"
        self.oracle_model = ctk.StringVar(value=default_model)
        ctk.CTkLabel(top, text="model", text_color=FG_DIM).pack(side="left", padx=(0, 4))
        ctk.CTkEntry(top, textvariable=self.oracle_model, width=230,
                     fg_color=GRID, text_color=FG).pack(side="left", padx=(0, 10))
        ctk.CTkButton(top, text="Analyze last profile", command=self._run_oracle,
                      fg_color=ZONE_FOCUS, text_color="#0f172a"
                      ).pack(side="left", padx=(0, 10))
        self.oracle_lbl = ctk.CTkLabel(top, text="Groq key: checking...", text_color=FG_DIM)
        self.oracle_lbl.pack(side="left")
        self._refresh_oracle_key()

        keyrow = ctk.CTkFrame(tab, fg_color="transparent")
        keyrow.pack(fill="x", padx=8, pady=(0, 2))
        ctk.CTkLabel(keyrow, text="Groq key", text_color=FG_DIM).pack(side="left", padx=(0, 4))
        self.oracle_key_entry = ctk.CTkEntry(keyrow, show="*", width=340,
                                             fg_color=GRID, text_color=FG)
        self.oracle_key_entry.pack(side="left", padx=(0, 8))
        ctk.CTkButton(keyrow, text="Save key", command=self._save_oracle_key, width=90,
                      fg_color=GRID, text_color=FG).pack(side="left", padx=(0, 8))
        ctk.CTkLabel(keyrow, text="stored in tools/test_agent/oracle.key (gitignored)",
                     text_color=FG_DIM).pack(side="left")

        self.oracle_text = ctk.CTkTextbox(tab, wrap="word", fg_color=BG, text_color=FG)
        self.oracle_text.pack(fill="both", expand=True, padx=8, pady=4)
        self.oracle_text.insert("end", "(no analysis yet.\n\n"
                                "1. Run a passive shadow in the Scenarios tab and enable the "
                                "'LLM oracle analysis' switch.\n"
                                "2. Or press 'Analyze last profile' after a shadow run completes.\n"
                                "3. The key is read from GROQ_API_KEY env var, else oracle.key.)\n")

    def _refresh_oracle_key(self):
        try:
            from oracle import key_status
            src = key_status()
        except Exception:
            src = None
        ok = src is not None
        self.oracle_lbl.configure(
            text="Groq key: %s" % ("set (%s)" % src if ok
                                   else "not set (paste below and Save, or set GROQ_API_KEY)"),
            text_color=ZONE_FOCUS if ok else ZONE_DISTR)

    def _save_oracle_key(self):
        key = self.oracle_key_entry.get().strip()
        if not key:
            self.oracle_lbl.configure(text="no key entered", text_color=ZONE_DISTR)
            return
        try:
            from oracle import save_key
            saved = save_key(key)
        except Exception as e:
            self.oracle_lbl.configure(text="save failed: %s" % e, text_color=ZONE_DISTR)
            return
        if saved:
            self.oracle_key_entry.delete(0, "end")
            self._refresh_oracle_key()
        else:
            self.oracle_lbl.configure(text="no key entered", text_color=ZONE_DISTR)

    def _run_oracle(self):
        if self.oracle_thread and self.oracle_thread.is_alive():
            return
        if self.last_shadow_profile is None:
            self.oracle_lbl.configure(text="no profile yet: run a passive shadow first",
                                      text_color=ZONE_DISTR)
            return
        model = self.oracle_model.get().strip() or None
        self.oracle_lbl.configure(text="analyzing...", text_color=FG_DIM)
        self.oracle_text.delete("1.0", "end")
        self.oracle_text.insert("end", "running LLM analysis of last shadow profile...\n")
        self.oracle_thread = OracleRunnerThread(self.last_shadow_profile, model=model)
        self.oracle_thread.start()

    def _check_oracle(self):
        th = self.oracle_thread
        if th is None or not th.done.is_set():
            return
        self.oracle_text.delete("1.0", "end")
        self.oracle_text.insert("end", (th.result or "(no result)") + "\n")
        self.oracle_lbl.configure(text="done", text_color=ZONE_FOCUS)
        self.oracle_thread = None

    # -- calibration --------------------------------------------------------

    def _build_calibration(self, tab):
        try:
            from calibrate import MOTION_WINDOW_S, RECOMMENDED_ORDER
            self.cal_win_s = MOTION_WINDOW_S
            self.cal_order = RECOMMENDED_ORDER
        except Exception:
            self.cal_win_s = 20
            self.cal_order = ("Close", "Far", "Still", "Moving")
        self.last_cal_report = None
        self.cal_pass = []
        self.cal_merged = []
        top = ctk.CTkFrame(tab, fg_color="transparent")
        top.pack(fill="x", padx=8, pady=(8, 2))
        ctk.CTkLabel(top,
                     text="One button runs the full calibration from the raw radar signal "
                     "(the same data the Web UI Radar Signal History chart plots). Poses: "
                     "%s. Each pose is watched until its filtered value stabilizes, then "
                     "the next begins. The classifier motion window is set to %ds and the "
                     "focus distance + motion ratio limits are computed from the captured "
                     "values; if a pose's data does not make sense, you will be asked to "
                     "re-capture just that pose."
                     % (" -> ".join(self.cal_order), self.cal_win_s),
                     text_color=FG_DIM, wraplength=860, justify="left").pack(anchor="w")
        self.cal_instr = ctk.CTkLabel(tab, text="", text_color=FG, wraplength=880,
                                      justify="left")
        self.cal_instr.pack(anchor="w", padx=10, pady=(6, 2))
        self.cal_run_btn = ctk.CTkButton(tab, text="Start calibration", width=200,
                                         command=self._run_calibration, fg_color=ZONE_FOCUS,
                                         text_color="#0f172a",
                                         font=ctk.CTkFont(size=13, weight="bold"))
        self.cal_run_btn.pack(anchor="w", padx=8, pady=(4, 2))
        self.cal_lbl = ctk.CTkLabel(tab, text="idle", text_color=FG_DIM)
        self.cal_lbl.pack(anchor="w", padx=10, pady=(2, 2))
        self.cal_text = ctk.CTkTextbox(tab, wrap="none", fg_color=BG, text_color=FG)
        self.cal_text.pack(fill="both", expand=True, padx=8, pady=4)
        self.cal_apply = ctk.CTkButton(tab, text="Apply computed thresholds", width=230,
                                       command=self._apply_calibration, fg_color=ZONE_BUSY,
                                       text_color="#0f172a")
        self.cal_apply.pack(anchor="w", padx=8, pady=(0, 6))
        self.cal_apply.configure(state="disabled")
        self._refresh_cal_thresholds()
        self.cal_text.insert("end",
                             "How it works:\n"
                             "  1. Press 'Start calibration'. The classifier motion window is\n"
                             "     set to %ds (config.motionWindow) so the device measures\n"
                             "     motion the same way the calibrator does.\n"
                             "  2. For each pose, read the instruction, get into position and\n"
                             "     hold it. The calibrator watches the raw radar signal and\n"
                             "     moves on as soon as the value is stable.\n"
                             "  3. Pose order is %s. Close/Far lock the focus distance limit,\n"
                             "     Still/Moving lock the motion ratio limit.\n"
                             "  4. Thresholds are computed at the end and offered for apply.\n"
                             "  5. If a pose's data does not make sense, you will be asked to\n"
                             "     re-capture just that pose.\n"
                             % (self.cal_win_s, " -> ".join(self.cal_order)))

    def _refresh_cal_thresholds(self):
        d = (self.tele.last_data or {}) if self.tele else {}
        fl = d.get("focusDistLim")
        ml = d.get("motionRatioLim")
        mw = d.get("motionWindow")
        self.cal_lbl.configure(
            text="idle - focusDistLim=%s  motionRatioLim=%s  motionWindow=%s"
                 % (fl if fl is not None else "?",
                    ml if ml is not None else "?",
                    mw if mw is not None else "?"))

    def _run_calibration(self):
        if self.cal_thread and self.cal_thread.is_alive():
            return
        behaviours = self.cal_pass or list(self.cal_order)
        self.cal_pass = behaviours
        self.cal_merged = []
        self.cal_captures = {}
        self.cal_raw = {}
        self.cal_run_btn.configure(text="Calibrating...", state="disabled")
        self.cal_apply.configure(state="disabled")
        self.cal_instr.configure(text="Calibration running - follow each instruction. I watch "
                                      "the raw radar signal and move on once the value "
                                      "stabilizes.")
        self.cal_text.delete("1.0", "end")
        self.cal_text.insert("end", "calibration pass starting: %s\n" % " -> ".join(behaviours))
        self.cal_thread = CalibrationScriptThread(self.bus, self.mqtt, behaviours)
        self.cal_thread.start()

    def _check_calibration(self):
        th = self.cal_thread
        if th is None:
            return
        if not th.done.is_set():
            self.cal_lbl.configure(text=th.progress, text_color=FG_DIM)
            if th.current:
                self.cal_instr.configure(text="[%s] %s\nlive: %s"
                                         % (th.current, th.instruct, th.value))
            return
        self.cal_captures = dict(getattr(th, "captures", {}) or {})
        self.cal_raw = dict(getattr(th, "raw", {}) or {})
        self.cal_thread = None
        self._finalize_calibration_pass()

    def _finalize_calibration_pass(self):
        from calibrate import retry_set, compute_thresholds, derived_states
        self.cal_pass = retry_set(self.cal_captures)
        merged, warnings, locks = compute_thresholds(self.cal_captures,
                                                     motion_window=self.cal_win_s)
        self.cal_merged = merged
        lines = []
        lines.append("================== CALIBRATION PASS RESULTS ==================")
        for name in self.cal_order:
            c = self.cal_captures.get(name)
            if c:
                lines.append("")
                lines.extend(self._cal_report_lines(c))
        lines.append("")
        lines.append("locked variables:")
        for lk in locks:
            lines.append("  - " + lk)
        states = derived_states(self.cal_captures)
        if states and not self.cal_pass:
            lines.append("")
            lines.append("with these limits the classifier reports:")
            for st, combo, why in states:
                lines.append("  - %-16s %-14s (%s)" % (st, combo, why))
        if warnings:
            lines.append("")
            lines.append("combined warnings:")
            for w in warnings:
                lines.append("  - " + w)
        lines.append("")
        if self.cal_pass:
            lines.append("!!! DATA DID NOT MAKE SENSE FOR: %s !!!" % ", ".join(self.cal_pass))
            lines.append("")
            lines.append("A pose is re-captured when it failed to stabilize, or the computed")
            lines.append("boundary would leave no headroom. Common fixes:")
            lines.append("  - distance poses: lean in much closer / lean further back.")
            lines.append("  - motion poses: sit perfectly still / move much more vigorously.")
            lines.append("")
            lines.append("Press the button to re-capture these poses.")
            self.cal_run_btn.configure(text="Re-capture: %s" % ", ".join(self.cal_pass),
                                       state="normal")
            self.cal_apply.configure(state="disabled")
            self.cal_lbl.configure(text="re-capture needed", text_color=ZONE_DISTR)
        else:
            lines.append("CALIBRATION COMPLETE - thresholds computed from your data:")
            if merged:
                for p in merged:
                    lines.append("  %s" % p["cmd"])
            else:
                lines.append("  (limits are already at the computed values - nothing to apply)")
            self.cal_run_btn.configure(text="Run calibration again", state="normal")
            self.cal_apply.configure(state="normal" if merged else "disabled")
            self.cal_lbl.configure(text="idle", text_color=FG_DIM)
        self.cal_text.delete("1.0", "end")
        self.cal_text.insert("end", "\n".join(lines) + "\n")

    def _cal_report_lines(self, c):
        lines = []
        name = c.get("name", "?")
        err = c.get("error")
        if err:
            lines.append("== %s (failed) ==" % name)
            lines.append("  reason: %s" % err)
            return lines
        lines.append("== %s (ok) ==" % name)
        if c.get("kind") == "distance":
            lines.append("distance: stable %.0f cm after %.1fs"
                         % (c.get("dist"), c.get("stable_after")))
        else:
            lines.append("motion:   stable %.0f%% after %.1fs (%ds window)"
                         % (c.get("ratio"), c.get("stable_after"), self.cal_win_s))
        n = len(self.cal_raw.get(name, []))
        lines.append("raw samples: %d" % n)
        return lines

    def _apply_calibration(self):
        props = getattr(self, "cal_merged", [])
        if not props:
            self.cal_lbl.configure(text="no computed thresholds to apply", text_color=ZONE_DISTR)
            return
        for p in props:
            self._send(p["cmd"])
            self.cal_text.insert("end", "applied: %s\n" % p["cmd"])
        self.cal_lbl.configure(text="applied %d setting(s)" % len(props),
                               text_color=ZONE_FOCUS)
        self.cal_apply.configure(state="disabled")

    def _send(self, text):
        self._append(self.cmd_text, "SEND: %s\n" % text)
        self.mqtt.cmd(text)

    def _send_cmd(self):
        text = self.cmd_entry.get().strip()
        if text:
            self.cmd_entry.delete(0, "end")
            self._send(text)

    def _run_tests(self):
        if self.runner_thread and self.runner_thread.is_alive():
            return
        suites = ["protocol", "presence", "modes", "behaviour"] \
            if self.suite_var.get() == "all" else [self.suite_var.get()]
        self.results_text.delete("1.0", "end")
        self.results_text.insert("end", "starting: %s (slow=%s, patterns=%s)\n"
                                % (", ".join(suites), self.slow_var.get(), self.patterns_var.get()))
        self.run_lbl.configure(text="running...")
        self.runner_thread = TestRunnerThread(self.ctx, suites, self.slow_var.get(),
                                              self.patterns_var.get())
        self.runner_thread.start()

    def _popout_chart(self):
        top = ctk.CTkToplevel(self)
        top.title("Mood Chart")
        top.geometry("960x520")
        chart = MoodChart(top, width=920, height=430)
        chart.frame.pack(fill="both", expand=True, padx=8, pady=8)
        self._popout_chart_ref = chart
        for p in self.chart.buffer:
            chart.buffer.append(p)
        chart.focus_dist_lim = self.chart.focus_dist_lim
        chart.motion_ratio_lim = self.chart.motion_ratio_lim
        chart.dist_limit = self.chart.dist_limit
        chart.state = self.chart.state
        self.bus.subscribe("telemetry", lambda e: chart.push(e["data"]))
        self.bus.subscribe("telemetry", lambda e, c=chart: c._redraw())

    # -- bus draining ------------------------------------------------------------
    def _tick(self):
        for k, q in self._queues.items():
            handler = getattr(self, "_on_" + k, None)
            if handler is None:
                continue
            while q:
                handler(q.popleft())
        self._check_scenario()
        self._check_oracle()
        self._check_calibration()
        self.chart._redraw()
        self.after(100, self._tick)

    def _on_telemetry(self, ev):
        d = ev["data"]
        self.chart.push(d)
        m = self.tele_tab
        vals = {"state": d.get("state", "-")}
        for k in ("detectionDist", "motionRatio", "recentMotionRatio",
                  "focusDistLim", "motionRatioLim", "sessionDistAvg"):
            v = d.get(k)
            vals[k] = "-" if v is None else ("%.0f" % v)
        for k, lbl in m.items():
            v = vals.get(k, "-")
            color = FG
            if k == "state":
                color = MOOD_COLORS.get(v, FG)
            lbl.configure(text=v, text_color=color)

    def _on_mqtt_resp(self, ev):
        d = ev["data"]
        payload = json.dumps({k: v for k, v in d.items() if k != "_raw"})
        self._append(self.cmd_text, "RESP: %s\n" % payload)

    def _on_mqtt_log(self, ev):
        self._append(self.log_text, "[%s] %s\n" % (ev["data"].get("category", "?"),
                                                   ev["data"].get("text", "")), cap=2000)

    def _on_mqtt_echo(self, ev):
        self._append(self.log_text, "[ECHO] %s\n" % ev["data"].get("payload", ""), cap=2000)

    def _toggle_serial(self):
        if self.ser:
            self.ser.stop()
            self.ser = None
            self.serial_btn.configure(text="Connect serial")
            self._append(self.serial_text, "(serial disconnected)\n")
            return
        try:
            from serial_harness import SerialHarness
            s = SerialHarness(self.bus, self.cfg)
            if s.connect():
                s.start()
                self.ser = s
                self.serial_btn.configure(text="Disconnect serial")
                self._append(self.serial_text, "(serial connected: %s)\n" % s.port)
            else:
                self._append(self.serial_text, "(serial failed: %s)\n" % s.error)
        except Exception as e:
            self._append(self.serial_text, "(serial failed: %s)\n" % e)

    def _on_serial_line(self, ev):
        self._append(self.serial_text, "%s\n" % ev["data"].get("line", ""), cap=2000)

    def _on_ai_trace(self, ev):
        d = ev["data"]
        self._append(self.ai_text, "[%s] %s\n\n" % (d.get("kind", "?"), d.get("text", "")), cap=500)

    def _on_test_result(self, ev):
        r = ev["data"]
        mark = {"pass": "[PASS]", "fail": "[FAIL]", "skip": "[SKIP]"}.get(r["status"], "?")
        self._append(self.results_text, "%s %s (%.1fs) %s\n"
                     % (mark, r.get("name"), r.get("duration", 0), r.get("message", "")))
        if r["status"] == "fail":
            self.run_lbl.configure(text="finished: FAILURES PRESENT")
        elif r["status"] == "skip":
            pass
        else:
            self.run_lbl.configure(text="finished: all pass")

    def _on_mqtt_conn(self, ev):
        ok = ev["data"].get("connected")
        self.status_lbl.configure(text="MQTT: %s" % ("up" if ok else "DOWN"),
                                  text_color=ZONE_FOCUS if ok else ZONE_DISTR)

    def _on_heap(self, ev):
        pass

    @staticmethod
    def _append(widget, text, cap=None):
        widget.insert("end", text)
        if cap:
            line_count = int(widget.index("end-1c").split(".")[0])
            if line_count > cap:
                widget.delete("1.0", "%d.0" % (line_count - cap))

    def _on_close(self):
        try:
            self.mqtt.cmd("SIM stop")
        except Exception:
            pass
        self.tele.stop()
        if self.ser:
            self.ser.stop()
        self.mqtt.stop()
        self.destroy()


def main():
    ap = argparse.ArgumentParser(description="DeskBuddy test agent GUI")
    ap.add_argument("--broker", default=None)
    ap.add_argument("--serial", default=None)
    args = ap.parse_args()
    cfg = load_config()
    if args.broker:
        cfg["broker"] = args.broker
    if args.serial:
        cfg["serial_port"] = args.serial
        cfg["serial_enabled"] = True
    app = App(cfg)
    app.mainloop()


if __name__ == "__main__":
    main()
