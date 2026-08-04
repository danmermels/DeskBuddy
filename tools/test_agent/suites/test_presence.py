import time

from assertions import AssertionError_ex, dstate


def _expect_ok(resp, desc):
    if resp is None or not resp.get("ok"):
        raise AssertionError_ex("%s: ok=false (%s)" % (desc, resp.get("error") if resp else "no resp"))


def _sim_state(ctx, state):
    r = ctx.cmd("SIM state %s" % state)
    _expect_ok(r, "SIM state %s" % state)
    if r.get("overrideState") != dstate(state):
        raise AssertionError_ex("SIM state %s: overrideState=%s" % (state, r.get("overrideState")))


def _get_state_name(ctx):
    r = ctx.cmd("GET state")
    _expect_ok(r, "GET state")
    return r.get("state")


def _wait_state(ctx, name, timeout=15):
    ctx.wait_telemetry(lambda e: e["data"].get("state") == dstate(name),
                       timeout=timeout, desc="telemetry %s" % name, anchor=True)
    got = _get_state_name(ctx)
    if got != dstate(name):
        raise AssertionError_ex("expected %s, got %s" % (dstate(name), got))


def test_sim_state_focus(ctx):
    try:
        _sim_state(ctx, "FOCUS")
        _wait_state(ctx, "FOCUS")
    finally:
        ctx.sim_stop()


def test_sim_state_busy(ctx):
    try:
        _sim_state(ctx, "BUSY")
        _wait_state(ctx, "BUSY")
    finally:
        ctx.sim_stop()


def test_sim_state_away(ctx):
    try:
        _sim_state(ctx, "AWAY")
        _wait_state(ctx, "AWAY", timeout=20)
    finally:
        ctx.sim_stop()


def test_sim_state_regular(ctx):
    try:
        _sim_state(ctx, "REGULAR")
        _wait_state(ctx, "REGULAR")
    finally:
        ctx.sim_stop()


def test_sim_state_distracted(ctx):
    try:
        _sim_state(ctx, "DISTRACTED")
        _wait_state(ctx, "DISTRACTED")
    finally:
        ctx.sim_stop()


def test_sim_presets(ctx):
    try:
        for preset, state in (("focus", "FOCUS"), ("busy", "BUSY"),
                              ("distracted", "DISTRACTED"), ("away", None),
                              ("sit", None)):
            r = ctx.cmd("SIM %s" % preset)
            _expect_ok(r, "SIM %s" % preset)
            if state:
                _wait_state(ctx, state, timeout=15)
    finally:
        ctx.sim_stop()


def test_sim_radar_sit(ctx):
    try:
        r = ctx.cmd("SIM radar 80 0 1")
        _expect_ok(r, "SIM radar 80 0 1")
        time.sleep(1.0)
        g = ctx.cmd("GET radar")
        _expect_ok(g, "GET radar")
        if not g.get("sim"):
            raise AssertionError_ex("SIM not reported active")
        if not g.get("present"):
            raise AssertionError_ex("SIM present not set")
        if g.get("moving"):
            raise AssertionError_ex("SIM moving should be false")
        if abs(int(g.get("rawDist", 0)) - 80) > 5:
            raise AssertionError_ex("SIM dist mismatch: %s" % g.get("rawDist"))
    finally:
        ctx.sim_stop()


def test_sim_stop_clears(ctx):
    _sim_state(ctx, "FOCUS")
    r = ctx.cmd("SIM stop")
    _expect_ok(r, "SIM stop")
    time.sleep(1.0)
    s = ctx.cmd("GET system")
    _expect_ok(s, "GET system")
    if s.get("simActive"):
        raise AssertionError_ex("simActive still true after SIM stop")
