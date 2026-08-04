import time

from assertions import AssertionError_ex


class HumanSimulator:
    """High-level human behaviours translated into radar SIM commands.

    Every verb is recorded on a wall-clock timeline so scenarios can report
    exactly what was simulated, alongside what the device actually did.
    """

    def __init__(self, ctx):
        self.ctx = ctx
        self.timeline = []

    # -- low level --------------------------------------------------------

    def _radar(self, dist, moving, present, action):
        r = self.ctx.cmd("SIM radar %d %d %d" % (dist, int(moving), int(present)))
        if r is None or not r.get("ok"):
            raise AssertionError_ex("SIM radar %d %d %d failed: %s" % (
                dist, int(moving), int(present), r))
        self._record(action, "radar d=%d moving=%d present=%d" % (dist, int(moving), int(present)))

    def _record(self, action, detail):
        self.timeline.append({"ts": time.time(), "action": action, "detail": detail})

    def _wait(self, seconds, why):
        if seconds and seconds > 0:
            time.sleep(seconds)
            self._record("wait", "%.1fs %s" % (seconds, why))

    # -- device context ----------------------------------------------------

    def thresholds(self):
        tele = self.ctx.latest_telemetry() or {}
        return (float(tele.get("focusDistLim") or 50),
                float(tele.get("motionRatioLim") or 15),
                float(tele.get("distLimit") or 120))

    def near_dist(self):
        focus_lim, _, _ = self.thresholds()
        return max(10, int(focus_lim * 0.5))

    def far_dist(self):
        focus_lim, _, _ = self.thresholds()
        return int(focus_lim) + 60

    def is_first_sit_today(self):
        r = self.ctx.cmd("GET system")
        return bool(r and r.get("firstSit"))

    def get_state(self):
        r = self.ctx.cmd("GET state")
        return r.get("state") if r else None

    # -- behaviours ----------------------------------------------------------

    def baseline_away(self):
        self._radar(0, 0, 0, "baseline_away")

    def sit(self, dist=None, wait=3.0):
        d = dist if dist is not None else self.near_dist()
        self._radar(d, 0, 1, "sit")
        self._wait(wait, "settle after sit")

    def lean_in(self, wait=2.0):
        self._radar(self.near_dist(), 0, 1, "lean_in")
        self._wait(wait, "settle after lean-in")

    def lean_away(self, wait=3.0):
        self._radar(self.far_dist(), 0, 1, "lean_away")
        self._wait(wait, "settle after lean-away")

    def fidget(self, seconds=35, dist=None):
        d = dist if dist is not None else self.near_dist()
        self._radar(d, 1, 1, "fidget_start")
        self._wait(seconds, "fidget for %ds" % seconds)
        self._radar(d, 0, 1, "fidget_stop")

    def fidget_away(self, seconds=35):
        self._radar(self.far_dist(), 1, 1, "fidget_away_start")
        self._wait(seconds, "fidget-away for %ds" % seconds)
        self._radar(self.far_dist(), 0, 1, "fidget_away_stop")

    def leave(self, wait=2.0):
        self._radar(0, 0, 0, "leave")
        self._wait(wait, "settle after leave")

    def walk_past(self):
        self._radar(self.far_dist(), 1, 1, "walk_past")
        self._wait(1.5, "passing through")
        self._radar(0, 0, 0, "walk_past_gone")

    def hold(self, seconds, why=""):
        self._record("hold", "%ds %s" % (seconds, why))
        self._wait(seconds, "hold %s" % why)

    def stop(self):
        self.ctx.sim_stop()
        self._record("sim_stop", "SIM stop sent")
