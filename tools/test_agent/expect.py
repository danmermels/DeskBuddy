import time


EVENT_TYPE_NAMES = {
    0: "first sit",
    1: "welcome back",
    2: "stretch",
    3: "focus end",
    4: "slacker",
    5: "streak beaten",
    6: "lunch reminder",
    8: "excessive breaks",
    9: "goal completed",
    10: "journal",
    11: "nagging",
    12: "task due",
    13: "page",
    14: "late hours",
}


class Expectation:
    """One expected outcome with the observed (received) evidence attached."""

    def __init__(self, name, ok, expected, received, detail=""):
        self.name = name
        self.ok = ok
        self.expected = expected
        self.received = received
        self.detail = detail

    def to_dict(self):
        return {"name": self.name, "ok": self.ok, "expected": self.expected,
                "received": self.received, "detail": self.detail}

    def __repr__(self):
        out = "[%s] %s\n    expected: %s\n    received: %s" % (
            "PASS" if self.ok else "FAIL", self.name, self.expected, self.received)
        if self.detail:
            out += "\n    detail: %s" % self.detail
        return out


class ScenarioRecorder:
    """Collects everything the device actually did while a scenario runs.

    - states: telemetry state transitions   [(ts, name), ...]
    - screens: TFT message history additions [(ts, text, event_type, is_ai), ...]
    - logs: MQTT log lines                  [(ts, category, text), ...]
    - status: MQTT status payloads          [(ts, payload), ...]
    """

    def __init__(self, ctx):
        self.ctx = ctx
        self.states = []
        self.screens = []
        self.logs = []
        self.status = []
        self.expectations = []
        self._seen_tft = set()
        self._suppress_tft = True
        self._last_state = None
        self._subs = []

    def start(self):
        self._subs = [
            ("telemetry", self._on_telemetry),
            ("tft_messages", self._on_tft),
            ("mqtt_log", self._on_log),
            ("mqtt_status", self._on_status),
        ]
        for etype, fn in self._subs:
            self.ctx.bus.subscribe(etype, fn)

    def stop(self):
        for etype, fn in self._subs:
            self.ctx.bus.unsubscribe(etype, fn)
        self._subs = []

    def reset(self):
        self.states = []
        self.screens = []
        self.logs = []
        self.status = []
        self._seen_tft = set()
        self._suppress_tft = True
        self._last_state = None

    def _on_telemetry(self, ev):
        st = ev["data"].get("state")
        if st and st != self._last_state:
            self.states.append((ev["ts"], st))
            self._last_state = st

    def _on_tft(self, ev):
        msgs = ev["data"].get("messages") or []
        if self._suppress_tft:
            for m in msgs:
                self._seen_tft.add((m.get("epoch"), m.get("text")))
            self._suppress_tft = False
            return
        for m in msgs:
            key = (m.get("epoch"), m.get("text"))
            if key not in self._seen_tft:
                self._seen_tft.add(key)
                self.screens.append((ev["ts"], m.get("text", ""),
                                     m.get("eventType"), bool(m.get("isAi"))))

    def _on_log(self, ev):
        d = ev["data"]
        self.logs.append((ev["ts"], d.get("category", ""), d.get("text", "")))

    def _on_status(self, ev):
        self.status.append((ev["ts"], ev["data"].get("payload", "")))


class Expect:
    """Imperative expectation DSL used inside scenario `run(sim, exp)`.

    Each call anchors at the moment it runs, blocks up to `within` seconds
    watching the recorder, then emits an Expectation carrying both the
    declared expectation and the received evidence.
    """

    def __init__(self, rec):
        self.rec = rec

    # -- helpers ----------------------------------------------------------

    def _states_since(self, t0):
        out = []
        for ts, st in self.rec.states:
            if ts >= t0:
                out.append("%s@+%.1fs" % (st, ts - t0))
        return out

    def _screens_since(self, t0):
        out = []
        for ts, txt, et, ai in self.rec.screens:
            if ts >= t0:
                label = EVENT_TYPE_NAMES.get(et, "event %s" % et)
                out.append("[%s] %s" % (label, (txt or "")[:80]))
        return out

    def _logs_since(self, t0):
        out = []
        for ts, cat, txt in self.rec.logs:
            if ts >= t0:
                out.append("%s: %s" % (cat, (txt or "")[:100]))
        return out

    def _emit(self, exp):
        self.rec.expectations.append(exp)
        return exp

    # -- positive expectations ---------------------------------------------

    def state(self, name, within=15.0, hold=None, desc=None):
        desc = desc or ("state %r" % name)
        t0 = time.time()
        deadline = t0 + within
        found = None
        while time.time() < deadline:
            for ts, st in self.rec.states:
                if t0 <= ts <= time.time() and st == name:
                    found = (ts, st)
                    break
            if found:
                break
            time.sleep(0.1)
        if not found:
            return self._emit(Expectation(
                desc, False,
                "transition to %r within %.1fs" % (name, within),
                "; ".join(self._states_since(t0)) or "no state change",
                "timeout %.1fs" % within))
        ts, st = found
        if hold is not None:
            h_end = ts + hold
            while time.time() < h_end:
                away = [x for x in self.rec.states
                        if ts < x[0] <= time.time() and x[1] != name]
                if away:
                    return self._emit(Expectation(
                        desc, False,
                        "state %r held for %.1fs" % (name, hold),
                        "; ".join(self._states_since(t0)),
                        "left at +%.1fs (moved to %s)" % (away[0][0] - ts, away[0][1])))
                time.sleep(0.1)
        return self._emit(Expectation(
            desc, True,
            "transition to %r within %.1fs" % (name, within),
            "%s@+%.1fs" % (name, ts - t0)))

    def maybe_state(self, names, within=20.0, desc=None):
        desc = desc or ("state in %r" % (list(names),))
        t0 = time.time()
        deadline = t0 + within
        found = None
        while time.time() < deadline:
            for ts, st in self.rec.states:
                if t0 <= ts <= time.time() and st in names:
                    found = (ts, st)
                    break
            if found:
                break
            time.sleep(0.1)
        if not found:
            return self._emit(Expectation(
                desc, False,
                "transition to one of %r within %.1fs" % (list(names), within),
                "; ".join(self._states_since(t0)) or "no state change",
                "timeout %.1fs" % within))
        return self._emit(Expectation(
            desc, True,
            "transition to one of %r within %.1fs" % (list(names), within),
            "%s@+%.1fs" % (found[1], found[0] - t0)))

    def state_now(self, name, within=20.0, desc=None):
        """Waits for the telemetry state field to read `name` (current value, not a transition)."""
        desc = desc or ("state is %r" % name)
        t0 = time.time()
        deadline = t0 + within
        last = None
        while time.time() < deadline:
            tele = self.rec.ctx.latest_telemetry() or {}
            last = tele.get("state")
            if last == name:
                return self._emit(Expectation(
                    desc, True,
                    "telemetry state reads %r within %.1fs" % (name, within),
                    "%s@+%.1fs" % (name, time.time() - t0)))
            time.sleep(0.5)
        return self._emit(Expectation(
            desc, False,
            "telemetry state reads %r within %.1fs" % (name, within),
            "state stayed %r" % last, "timeout %.1fs" % within))

    def quiet_motion(self, within=210.0, desc=None):
        """Waits for recentMotionRatio to drop to or below the configured limit.

        The device accumulates motion over a 180 s window, so after a busy
        period the ratio takes up to ~3 minutes to decay even when sitting
        still. Scenarios that assume a quiet baseline should call this first.
        """
        tele = self.rec.ctx.latest_telemetry() or {}
        motion_lim = int(tele.get("motionRatioLim") or 15)
        desc = desc or ("motion ratio quiets (<= %d%%)" % motion_lim)
        t0 = time.time()
        deadline = t0 + within
        last = None
        while time.time() < deadline:
            t = self.rec.ctx.latest_telemetry() or {}
            r = t.get("recentMotionRatio")
            if r is not None:
                last = int(r)
                if last <= motion_lim:
                    return self._emit(Expectation(
                        desc, True,
                        "recentMotionRatio <= %d%% within %.1fs" % (motion_lim, within),
                        "ratio=%d%%@+%.1fs" % (last, time.time() - t0)))
            time.sleep(1.0)
        return self._emit(Expectation(
            desc, False,
            "recentMotionRatio <= %d%% within %.1fs" % (motion_lim, within),
            "ratio never dropped below %d%% (last=%s)" % (motion_lim, last),
            "timeout %.1fs" % within))

    def state_present(self, within=20.0, desc=None):
        """Waits for the telemetry state to leave Away (any present state)."""
        desc = desc or "a present state is reached (not Away)"
        t0 = time.time()
        deadline = t0 + within
        last = None
        while time.time() < deadline:
            tele = self.rec.ctx.latest_telemetry() or {}
            last = tele.get("state")
            if last and last != "Away":
                return self._emit(Expectation(
                    desc, True,
                    "telemetry reports a present state within %.1fs" % within,
                    "%s@+%.1fs" % (last, time.time() - t0)))
            time.sleep(0.5)
        return self._emit(Expectation(
            desc, False,
            "telemetry reports a present state within %.1fs" % within,
            "state stayed %r" % last, "timeout %.1fs" % within))

    def screen(self, event_type=None, text=None, within=40.0, desc=None):
        parts = []
        if event_type is not None:
            parts.append("event=%s" % EVENT_TYPE_NAMES.get(event_type, event_type))
        if text is not None:
            parts.append("text~%r" % text)
        desc = desc or ("screen message %s" % ("+".join(parts) if parts else "any"))
        t0 = time.time()
        deadline = t0 + within
        found = None
        while time.time() < deadline:
            for ts, txt, et, ai in self.rec.screens:
                if t0 <= ts <= time.time():
                    if event_type is not None and et != event_type:
                        continue
                    if text is not None and text.lower() not in (txt or "").lower():
                        continue
                    found = (ts, txt, et)
                    break
            if found:
                break
            time.sleep(0.1)
        if not found:
            return self._emit(Expectation(
                desc, False,
                "screen message (%s) within %.1fs" % ("+".join(parts) if parts else "any", within),
                "; ".join(self._screens_since(t0)) or "no screen message",
                "timeout %.1fs" % within))
        label = EVENT_TYPE_NAMES.get(found[2], "event %s" % found[2])
        return self._emit(Expectation(
            desc, True,
            "screen message (%s) within %.1fs" % ("+".join(parts) if parts else "any", within),
            "[%s] %s@+%.1fs" % (label, (found[1] or "")[:80], found[0] - t0)))

    def log(self, category=None, contains=None, within=25.0, desc=None):
        desc = desc or ("log%s" % (("/" + category) if category else ""))
        t0 = time.time()
        deadline = t0 + within
        found = None
        while time.time() < deadline:
            for ts, cat, txt in self.rec.logs:
                if t0 <= ts <= time.time():
                    if category is not None and cat != category:
                        continue
                    if contains is not None and contains.lower() not in (txt or "").lower():
                        continue
                    found = (ts, cat, txt)
                    break
            if found:
                break
            time.sleep(0.1)
        if not found:
            return self._emit(Expectation(
                desc, False,
                "log %s within %.1fs" % (("matching " + (contains or "")) if contains
                                         else (category or "any"), within),
                "; ".join(self._logs_since(t0)) or "no log line",
                "timeout %.1fs" % within))
        return self._emit(Expectation(
            desc, True,
            "log %s within %.1fs" % (("matching " + (contains or "")) if contains
                                     else (category or "any"), within),
            "%s: %s@+%.1fs" % (found[1], (found[2] or "")[:100], found[0] - t0)))

    # -- negative expectations ----------------------------------------------

    def no_state(self, name, within=15.0, desc=None):
        desc = desc or ("state %r absent" % name)
        t0 = time.time()
        deadline = t0 + within
        bad = None
        while time.time() < deadline:
            for ts, st in self.rec.states:
                if t0 <= ts <= time.time() and st == name:
                    bad = (ts, st)
                    break
            if bad:
                break
            time.sleep(0.1)
        if bad:
            return self._emit(Expectation(
                desc, False,
                "no transition to %r within %.1fs" % (name, within),
                "%s@+%.1fs" % (bad[1], bad[0] - t0)))
        return self._emit(Expectation(
            desc, True,
            "no transition to %r within %.1fs" % (name, within),
            "none observed in %.1fs" % within))

    def no_screen(self, event_type=None, text=None, within=30.0, desc=None):
        desc = desc or "no screen message"
        t0 = time.time()
        deadline = t0 + within
        bad = None
        while time.time() < deadline:
            for ts, txt, et, ai in self.rec.screens:
                if t0 <= ts <= time.time():
                    if event_type is not None and et != event_type:
                        continue
                    if text is not None and text.lower() not in (txt or "").lower():
                        continue
                    bad = (ts, txt, et)
                    break
            if bad:
                break
            time.sleep(0.1)
        if bad:
            return self._emit(Expectation(
                desc, False,
                "no matching screen message within %.1fs" % within,
                "[%s] %s@+%.1fs" % (EVENT_TYPE_NAMES.get(bad[2], bad[2]),
                                     (bad[1] or "")[:80], bad[0] - t0)))
        return self._emit(Expectation(
            desc, True,
            "no matching screen message within %.1fs" % within,
            "none observed in %.1fs" % within))

    def no_log(self, category=None, contains=None, within=20.0, desc=None):
        desc = desc or "no log line"
        t0 = time.time()
        deadline = t0 + within
        bad = None
        while time.time() < deadline:
            for ts, cat, txt in self.rec.logs:
                if t0 <= ts <= time.time():
                    if category is not None and cat != category:
                        continue
                    if contains is not None and contains.lower() not in (txt or "").lower():
                        continue
                    bad = (ts, cat, txt)
                    break
            if bad:
                break
            time.sleep(0.1)
        if bad:
            return self._emit(Expectation(
                desc, False,
                "no matching log within %.1fs" % within,
                "%s: %s@+%.1fs" % (bad[1], (bad[2] or "")[:100], bad[0] - t0)))
        return self._emit(Expectation(
            desc, True,
            "no matching log within %.1fs" % within,
            "none observed in %.1fs" % within))
