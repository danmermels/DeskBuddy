import time

from assertions import AssertionError_ex, SkipTest, dstate

slow = lambda fn: (setattr(fn, "_slow", True), fn)[1]


def _expect_ok(resp, desc):
    if resp is None or not resp.get("ok"):
        raise AssertionError_ex("%s: ok=false (%s)" % (desc, resp.get("error") if resp else "no resp"))


def _force_away(ctx, timeout=25):
    # The real radar may already report presence; force a clean Away baseline so
    # SIM sit produces a real Away->Present transition and its STATE log.
    _expect_ok(ctx.cmd("SIM state AWAY"), "SIM state AWAY")
    ctx.wait_telemetry(lambda e: e["data"].get("state") == dstate("AWAY"),
                       timeout=timeout, desc="telemetry Away", anchor=True)
    time.sleep(1)


def _sit(ctx, dist=40):
    _force_away(ctx)
    _expect_ok(ctx.cmd("SIM sit %d" % dist), "SIM sit")


def _await_log(ctx, fragment, timeout=20, what="log"):
    ev = ctx.mqtt.wait_log(lambda e: fragment in e["data"].get("text", ""), timeout=timeout)
    if ev is None:
        raise AssertionError_ex("timed out waiting for %s log containing %r" % (what, fragment))
    return ev


def pattern_work_session(ctx):
    ctx.mqtt.bus.clear("mqtt_log")
    try:
        _sit(ctx)
        _await_log(ctx, "Away->Present", timeout=20, what="sit-down")
        time.sleep(3)
        ctx.cmd("TRIGGER 2 fallback")
        s = ctx.cmd("GET stats")
        _expect_ok(s, "GET stats")
    finally:
        ctx.sim_stop()


def pattern_interrupted_session(ctx):
    ctx.mqtt.bus.clear("mqtt_log")
    try:
        _sit(ctx)
        _await_log(ctx, "Away->Present", timeout=20)
        ctx.cmd("SIM away")
        _await_log(ctx, "Present->Away", timeout=25)
        ctx.cmd("SIM sit")
        _await_log(ctx, "Away->Present", timeout=20, what="return sit-down")
    finally:
        ctx.sim_stop()


def pattern_short_visit(ctx):
    ctx.mqtt.bus.clear("mqtt_log")
    try:
        _sit(ctx)
        _await_log(ctx, "Away->Present", timeout=20)
        ctx.cmd("SIM away")
        _await_log(ctx, "Present->Away", timeout=25)
        time.sleep(1.5)
        ctx.cmd("SIM sit")
        _await_log(ctx, "Ignored brief return", timeout=20, what="brief-return handling")
    finally:
        ctx.sim_stop()


@slow
def pattern_lunch_break(ctx):
    # Real 3-minute break (BREAK_MINIMUM_MS=180000) to register a break count.
    ctx.mqtt.bus.clear("mqtt_log")
    try:
        before = ctx.cmd("GET stats")
        _expect_ok(before, "GET stats before")
        _sit(ctx)
        _await_log(ctx, "Away->Present", timeout=20)
        ctx.cmd("SIM away")
        _await_log(ctx, "Present->Away", timeout=25)
        _await_log(ctx, "Real session completed", timeout=25)
        time.sleep(185)
        ctx.cmd("SIM sit")
        _await_log(ctx, "Handled standard break return", timeout=25, what="break return")
        after = ctx.cmd("GET stats")
        _expect_ok(after, "GET stats after")
        if int(after.get("breakCount", 0)) <= int(before.get("breakCount", 0)):
            raise AssertionError_ex("breakCount did not increment: %s -> %s" %
                                    (before.get("breakCount"), after.get("breakCount")))
    finally:
        ctx.sim_stop()


def pattern_late_hours_sit(ctx):
    # NOTE: SIM time <epoch> is a firmware no-op (simulatedEpoch is never read),
    # so late-hours is exercised via the LATEHOURS_SIT event fallback instead.
    ctx.mqtt.bus.clear("mqtt_log")
    try:
        _sit(ctx)
        _await_log(ctx, "Away->Present", timeout=20)
        r = ctx.cmd("TRIGGER 14 fallback")
        _expect_ok(r, "TRIGGER 14 fallback")
        ev = ctx.bus.wait_for("tft_messages",
                              pred=lambda e: any(m.get("eventType") == 14
                                                 for m in e["data"].get("messages", [])),
                              timeout=20)
        if ev is None:
            raise AssertionError_ex("no TFT message with eventType 14")
    finally:
        ctx.sim_stop()


@slow
def pattern_day_rollover(ctx):
    # Real midnight rollover only; SIM time cannot fast-forward (firmware no-op).
    t = ctx.cmd("GET time")
    _expect_ok(t, "GET time")
    if t.get("hour") in (23, 0, 1, 2):
        ctx.mqtt.bus.clear("mqtt_log")
        try:
            _sit(ctx)
            _await_log(ctx, "rollover", timeout=60, what="rollover log")
        finally:
            ctx.sim_stop()
    else:
        raise SkipTest("not near midnight; SIM time is a firmware no-op")
