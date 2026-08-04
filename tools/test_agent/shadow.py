import json
import os
import time

from observer import Observer


class ShadowCheck:
    """One expected real-world behaviour that the shadow system validates."""

    def __init__(self, name, expected):
        self.name = name
        self.expected = expected
        self.status = "not-exercised"  # pending | pass | fail | not-exercised
        self.triggered_ts = None
        self.trigger_desc = ""
        self.received = ""
        self.detail = ""
        self.passes = 0
        self.fails = 0

    def to_dict(self):
        return {"name": self.name, "status": self.status,
                "expected": self.expected, "received": self.received,
                "trigger": self.trigger_desc,
                "triggered_ts": self.triggered_ts, "detail": self.detail,
                "passes": self.passes, "fails": self.fails}


class ShadowObserver(Observer):
    """Passive shadow system.

    Watches the REAL user's behaviour (never injects SIM commands) and checks
    that DeskBuddy actually behaves as expected - e.g. a stretch prompt after
    60 minutes of continuous sitting. Behaviours that never naturally occur
    during the observation are reported as "not-exercised".
    """

    STRETCH_INTERVAL_S = 3600          # 60 min continuous presence
    STRETCH_TOLERANCE_S = 300          # prompt may appear up to 5 min later
    BREAK_MIN_S = 180                  # >= 3 min away counts as a break
    CLASSIFY_CONTIG_S = 50             # sustained mismatch that matters
    DISTRACTED_FAR_MIN_S = 300         # must match firmware DISTRACTED_FAR_MIN_MS / 1000
    EVT_FIRST_SIT, EVT_WELCOME_BACK, EVT_STRETCH, EVT_LATEHOURS = 0, 1, 2, 14

    def __init__(self, ctx, duration=7200, report_dir=None):
        super().__init__(ctx, duration, report_dir=report_dir)
        self.checks = {
            "stretch_reminder": ShadowCheck(
                "stretch_reminder",
                "after %d min of continuous sitting, a stretch prompt (eventType 2) "
                "appears within %d min" % (self.STRETCH_INTERVAL_S // 60,
                                           self.STRETCH_TOLERANCE_S // 60)),
            "break_return_welcome": ShadowCheck(
                "break_return_welcome",
                "returning from a break >= %d min shows Welcome Back (eventType 1) "
                "or a late-hours message (eventType 14) within 90 s" % (self.BREAK_MIN_S // 60)),
            "first_sit_greeting": ShadowCheck(
                "first_sit_greeting",
                "the day's first sit shows the first-sit greeting (eventType 0) within 90 s"),
            "classifier_sane": ShadowCheck(
                "classifier_sane",
                "reported state matches distance+time thresholds (motion only "
                "splits the near zone into Focus/Busy); no sustained mismatches"),
            "motion_ratio_range": ShadowCheck(
                "motion_ratio_range",
                "recentMotionRatio always stays in [0,100]"),
        }
        # session tracking
        self.present_since = None       # ts of the observed Away->Present transition
        self.sit_stretch_mark = None    # next stretch due time for the current sit
        self.stretch_pending = None
        self.away_since = None
        self.break_pending = None
        self.first_sit_pending = None
        self.first_sit_today = None     # last known value from GET system
        self._first_sit_polled_at = 0
        # classification sanity
        self.classify = {"samples": 0, "contig": 0, "mismatches": 0}
        self.ratio_bad = []
        self._far_since = None   # ts when the user entered the far zone while present
        # screens
        self.screens = []
        self._seen_tft = set()
        self._tft_suppress = True

    # -- subscriptions ------------------------------------------------------

    def _on_telemetry(self, ev):
        Observer._on_telemetry(self, ev)
        d = ev["data"]
        ts = ev["ts"]
        st = d.get("state")

        rmr = d.get("recentMotionRatio")
        if isinstance(rmr, (int, float)) and not (0 <= rmr <= 100):
            self.ratio_bad.append((ts, rmr))
            chk = self.checks["motion_ratio_range"]
            chk.fails += 1
            chk.status = "fail"
            chk.triggered_ts = ts
            chk.trigger_desc = "ratio out of [0,100]"
            chk.received = "recentMotionRatio=%.1f at ts %.0f" % (rmr, ts)

        present = bool(st and st != "Away")
        if not present:
            self._far_since = None
        if present and self.present_since is None:
            self.present_since = ts
            self.sit_stretch_mark = ts + self.STRETCH_INTERVAL_S
            if self._first_sit_today_was_true(ts):
                self.first_sit_pending = {"since": ts}
        elif not present and self.present_since is not None:
            self.present_since = None
            self.sit_stretch_mark = None
            self.stretch_pending = None
            self.away_since = ts

        if present and self.away_since is not None:
            away_dur = ts - self.away_since
            self.away_since = None
            if away_dur >= self.BREAK_MIN_S:
                self.break_pending = {"since": ts}

        # sustained classification sanity
        dist = d.get("detectionDist")
        foc = d.get("focusDistLim")
        lim = d.get("motionRatioLim")
        if st and present and isinstance(dist, (int, float)) and \
                isinstance(foc, (int, float)) and isinstance(lim, (int, float)):
            expected = self._expected_state(ts, present, dist, rmr, foc, lim)
            self.classify["samples"] += 1
            if expected != st:
                self.classify["contig"] += 1
                if self.classify["contig"] >= self.CLASSIFY_CONTIG_S * 2:
                    self.classify["mismatches"] += 1
                    self.classify["contig"] = 0
                    chk = self.checks["classifier_sane"]
                    chk.fails += 1
                    chk.status = "fail"
                    chk.received = "sustained mismatch: reported=%s model=%s" % (st, expected)
            else:
                self.classify["contig"] = 0
        if self.classify["samples"] >= 20 and self.classify["mismatches"] == 0:
            chk = self.checks["classifier_sane"]
            chk.passes += 1
            chk.status = "pass"
            chk.received = "reported state matched distance/motion model across %d samples" % (
                self.classify["samples"])

    def _on_tft(self, ev):
        msgs = ev["data"].get("messages") or []
        if self._tft_suppress:
            for m in msgs:
                self._seen_tft.add((m.get("epoch"), m.get("text")))
            self._tft_suppress = False
            return
        for m in msgs:
            key = (m.get("epoch"), m.get("text"))
            if key not in self._seen_tft:
                self._seen_tft.add(key)
                self.screens.append((ev["ts"], m.get("text", ""), m.get("eventType")))

    def _expected_state(self, ts, present, dist, ratio, focus_lim, motion_lim):
        if not present:
            self._far_since = None
            return "Away"
        near = dist < focus_lim
        if near:
            self._far_since = None
            high = bool(ratio is not None and ratio > motion_lim)
            return "Busy" if high else "Focus"
        if self._far_since is None:
            self._far_since = ts
        if ts - self._far_since >= self.DISTRACTED_FAR_MIN_S:
            return "Distracted"
        return "Regular Activity"

    def _first_sit_today_was_true(self, ts):
        if self.first_sit_today is None or ts - self._first_sit_polled_at > 60:
            self._poll_first_sit()
        return self.first_sit_today is True

    def _poll_first_sit(self):
        self._first_sit_polled_at = time.time()
        try:
            r = self.ctx.cmd("GET system")
            self.first_sit_today = bool(r and r.get("firstSit"))
        except Exception:
            self.first_sit_today = None

    def _find_screen(self, start, end, event_type):
        for ts, txt, et in self.screens:
            if start <= ts <= end and et == event_type:
                return (ts, txt, et)
        return None

    # -- evaluation loop ------------------------------------------------------

    def _evaluate(self, now):
        self._resolve_stretch(now)
        self._resolve_break(now)
        self._resolve_first_sit(now)

    def _resolve_stretch(self, now):
        chk = self.checks["stretch_reminder"]
        if self.sit_stretch_mark is not None and self.stretch_pending is None and \
                now >= self.sit_stretch_mark:
            self.stretch_pending = {"mark": self.sit_stretch_mark,
                                    "deadline": now + self.STRETCH_TOLERANCE_S}
            chk.status = "pending"
            chk.triggered_ts = now
            chk.trigger_desc = "continuous sitting reached %d min" % (
                self.STRETCH_INTERVAL_S // 60)
        if self.stretch_pending is None:
            return
        w = self.stretch_pending
        hit = self._find_screen(w["mark"] - 90, now, self.EVT_STRETCH)
        if hit:
            chk.passes += 1
            chk.status = "pass"
            chk.received = "[stretch] %s@+%.0fs" % (hit[1][:60], hit[0] - w["mark"])
            self.sit_stretch_mark = w["mark"] + self.STRETCH_INTERVAL_S
            self.stretch_pending = None
        elif now >= w["deadline"]:
            chk.fails += 1
            if chk.passes == 0:
                chk.status = "fail"
            shown = "; ".join("[%s] %s" % (et, t[:40]) for _, t, et in self.screens
                              if t >= w["mark"] - 90) or "no screen messages"
            chk.received = "no stretch prompt within %ds; screens: %s" % (
                self.STRETCH_TOLERANCE_S, shown)
            self.sit_stretch_mark = w["mark"] + self.STRETCH_INTERVAL_S
            self.stretch_pending = None

    def _resolve_break(self, now):
        chk = self.checks["break_return_welcome"]
        if self.break_pending is None:
            return
        w = self.break_pending
        hit = self._find_screen(w["since"] - 5, now, self.EVT_WELCOME_BACK)
        if hit is None:
            hit = self._find_screen(w["since"] - 5, now, self.EVT_LATEHOURS)
        if hit:
            chk.passes += 1
            chk.status = "pass"
            chk.received = "[%s] %s" % (
                "welcome-back" if hit[2] == self.EVT_WELCOME_BACK else "late-hours",
                hit[1][:60])
            self.break_pending = None
        elif now >= w["since"] + 90:
            chk.fails += 1
            chk.status = "fail" if chk.passes == 0 else chk.status
            shown = "; ".join("[%s] %s" % (et, t[:40]) for _, t, et in self.screens
                              if t >= w["since"] - 5) or "no screen messages"
            chk.received = "no welcome/late-hours message after %d s break; screens: %s" % (
                self.BREAK_MIN_S, shown)
            self.break_pending = None

    def _resolve_first_sit(self, now):
        chk = self.checks["first_sit_greeting"]
        if self.first_sit_pending is None:
            return
        w = self.first_sit_pending
        h = time.localtime().tm_hour
        et = self.EVT_LATEHOURS if (h < 6 or h >= 21) else self.EVT_FIRST_SIT
        hit = self._find_screen(w["since"] - 5, now, et)
        if hit:
            chk.passes += 1
            chk.status = "pass"
            chk.received = "[eventType %d] %s" % (et, hit[1][:60])
            self.first_sit_pending = None
        elif now >= w["since"] + 90:
            chk.fails += 1
            chk.status = "fail" if chk.passes == 0 else chk.status
            chk.received = "no first-sit greeting within 90 s; screens: %s" % (
                "; ".join("[%s] %s" % (et, t[:40]) for _, t, et in self.screens
                          if t >= w["since"] - 5) or "none")
            self.first_sit_pending = None

    # -- main loop -------------------------------------------------------------

    def run(self, stop_event=None):
        bus = self.ctx.bus
        bus.subscribe("telemetry", self._on_telemetry)
        bus.subscribe("mqtt_log", self._on_log)
        bus.subscribe("ai_trace", self._on_ai)
        bus.subscribe("mqtt_conn", self._on_conn)
        bus.subscribe("serial_line", self._on_serial)
        bus.subscribe("tft_messages", self._on_tft)

        start = time.time()
        self.start_epoch = start
        last_eval = 0.0
        while time.time() - start < self.duration:
            if stop_event is not None and stop_event.is_set():
                break
            now = time.time()
            if now - last_eval >= 1.0:
                last_eval = now
                try:
                    self._evaluate(now)
                except Exception:
                    pass
            time.sleep(0.5)

        # finalize dwell / mqtt uptime (mirrors Observer.run)
        if self.state is not None and self.state_entered is not None:
            self.dwell[self.state] += time.time() - self.state_entered
        if self.mqtt_connected_at is not None:
            self.mqtt_total_uptime += time.time() - self.mqtt_connected_at

        profile = self.build_profile()
        path = os.path.join(self.report_dir, "shadow_%s.json" %
                            time.strftime("%Y%m%d_%H%M%S"))
        with open(path, "w", encoding="utf-8") as f:
            json.dump(profile, f, indent=2)
        return profile

    def build_profile(self):
        profile = Observer.build_profile(self)
        profile["mode"] = "passive-shadow"
        profile["shadow_checks"] = [c.to_dict() for c in self.checks.values()]
        return profile

    def checks_summary(self):
        rows = []
        for c in self.checks.values():
            rows.append({"name": c.name, "status": c.status, "expected": c.expected,
                         "received": c.received, "passes": c.passes, "fails": c.fails})
        return rows
