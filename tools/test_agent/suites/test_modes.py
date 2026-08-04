import time

from assertions import AssertionError_ex, dstate

_slow = lambda fn: (setattr(fn, "_slow", True), fn)[1]


def _expect_ok(resp, desc):
    if resp is None or not resp.get("ok"):
        raise AssertionError_ex("%s: ok=false (%s)" % (desc, resp.get("error") if resp else "no resp"))


def _thresholds(ctx):
    tele = ctx.latest_telemetry()
    if not tele:
        tele = {}
    focus_lim = int(tele.get("focusDistLim") or 50)
    motion_lim = int(tele.get("motionRatioLim") or 15)
    return focus_lim, motion_lim


def test_telemetry_exposes_classifier_inputs(ctx):
    tele = ctx.wait_telemetry(timeout=10, desc="telemetry sample")
    data = tele["data"]
    for k in ("recentMotionRatio", "motionRatio", "focusDistLim", "motionRatioLim",
              "detectionDist", "state"):
        if k not in data:
            raise AssertionError_ex("telemetry missing %r (firmware field not exposed?)" % k)
    rmr = data.get("recentMotionRatio")
    if not (isinstance(rmr, (int, float)) and 0 <= rmr <= 100):
        raise AssertionError_ex("recentMotionRatio out of range: %r" % rmr)


@_slow
def test_recent_motion_ratio_tracks_sim_motion(ctx):
    focus_lim, motion_lim = _thresholds(ctx)
    near = max(10, focus_lim // 2)

    def ratio_above(lim):
        return lambda e: (e["data"].get("recentMotionRatio") or 0) > lim

    def ratio_below(lim):
        return lambda e: (e["data"].get("recentMotionRatio") or 101) < lim

    try:
        r = ctx.cmd("SIM radar %d 0 1" % near)
        _expect_ok(r, "SIM radar low-motion")
        try:
            ctx.wait_telemetry(ratio_below(motion_lim), timeout=200,
                               desc="ratio below limit", anchor=True)
        except AssertionError_ex:
            raise AssertionError_ex("recentMotionRatio did not drop below %d%%" % motion_lim)

        r = ctx.cmd("SIM radar %d 1 1" % near)
        _expect_ok(r, "SIM radar high-motion")
        try:
            ctx.wait_telemetry(ratio_above(motion_lim), timeout=200,
                               desc="ratio above limit", anchor=True)
        except AssertionError_ex:
            raise AssertionError_ex("recentMotionRatio did not rise above %d%%" % motion_lim)
    finally:
        ctx.sim_stop()


def _classify(ctx, dist, moving, expected, motion_lim):
    r = ctx.cmd("SIM radar %d %d 1" % (dist, 1 if moving else 0))
    _expect_ok(r, "SIM radar %d %d 1" % (dist, moving))
    want = dstate(expected)
    try:
        try:
            ctx.wait_telemetry(lambda e: e["data"].get("state") == want,
                               timeout=200, desc="state %s" % want, anchor=True)
        except AssertionError_ex:
            tele = ctx.latest_telemetry()
            raise AssertionError_ex(
                "expected %s; last telemetry state=%s dist=%s ratio=%s (motion_lim=%d)" %
                (want, (tele or {}).get("state"), (tele or {}).get("detectionDist"),
                 (tele or {}).get("recentMotionRatio"), motion_lim))
    finally:
        ctx.sim_stop()


@_slow
def test_classify_focus(ctx):
    focus_lim, motion_lim = _thresholds(ctx)
    _classify(ctx, max(10, focus_lim // 2), 0, "FOCUS", motion_lim)


@_slow
def test_classify_busy(ctx):
    focus_lim, motion_lim = _thresholds(ctx)
    _classify(ctx, max(10, focus_lim // 2), 1, "BUSY", motion_lim)


@_slow
def test_classify_distracted(ctx):
    # The far side ignores motion now: Distracted (relaxed) only fires after the
    # user has been continuously present-but-far for DISTRACTED_FAR_MIN_MS, which
    # a SIM run cannot wait out. Assert the motion component no longer triggers
    # it: far + high motion must stay Regular Activity.
    focus_lim, motion_lim = _thresholds(ctx)
    _classify(ctx, focus_lim + 60, 1, "REGULAR", motion_lim)


@_slow
def test_classify_regular(ctx):
    focus_lim, motion_lim = _thresholds(ctx)
    _classify(ctx, focus_lim + 60, 0, "REGULAR", motion_lim)
