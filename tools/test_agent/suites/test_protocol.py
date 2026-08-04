import time

from assertions import AssertionError_ex, DEVICE_STATES


def _expect_ok(resp, desc):
    if resp is None:
        raise AssertionError_ex("%s: no response" % desc)
    if not resp.get("ok"):
        raise AssertionError_ex("%s: ok=false (%s)" % (desc, resp.get("error")))


def _expect_bad(resp, desc):
    if resp is None:
        raise AssertionError_ex("%s: no response" % desc)
    if resp.get("ok"):
        raise AssertionError_ex("%s: expected ok=false, got ok=true" % desc)


def test_get_system(ctx):
    r = ctx.cmd("GET system")
    _expect_ok(r, "GET system")
    for k in ("freeHeap", "minHeap", "uptime", "wifiRssi", "wifiStatus", "simActive"):
        if k not in r:
            raise AssertionError_ex("GET system missing key %r" % k)


def test_get_state(ctx):
    r = ctx.cmd("GET state")
    _expect_ok(r, "GET state")
    if r.get("state") not in set(DEVICE_STATES.values()):
        raise AssertionError_ex("GET state unknown state %r" % r.get("state"))


def test_get_radar(ctx):
    r = ctx.cmd("GET radar")
    _expect_ok(r, "GET radar")
    for k in ("rawDist", "filtDist", "present", "moving", "static", "sim"):
        if k not in r:
            raise AssertionError_ex("GET radar missing key %r" % k)


def test_get_filters(ctx):
    r = ctx.cmd("GET filters")
    _expect_ok(r, "GET filters")
    for k in ("filtDist", "filterWindow", "distAvg", "distCount"):
        if k not in r:
            raise AssertionError_ex("GET filters missing key %r" % k)


def test_get_stats(ctx):
    r = ctx.cmd("GET stats")
    _expect_ok(r, "GET stats")
    for k in ("deskTime", "focusTime", "breakTime", "breakCount", "score"):
        if k not in r:
            raise AssertionError_ex("GET stats missing key %r" % k)


def test_get_config(ctx):
    r = ctx.cmd("GET config")
    _expect_ok(r, "GET config")
    for k in ("aiMode", "aiPersona", "targetHours", "focusDistLim", "motionRatioLim"):
        if k not in r:
            raise AssertionError_ex("GET config missing key %r" % k)


def test_get_session(ctx):
    r = ctx.cmd("GET session")
    _expect_ok(r, "GET session")
    for k in ("deskTime", "motionTime", "distAvg", "distCount"):
        if k not in r:
            raise AssertionError_ex("GET session missing key %r" % k)


def test_get_time(ctx):
    r = ctx.cmd("GET time")
    _expect_ok(r, "GET time")
    for k in ("epoch", "hour", "minute", "ntpSet"):
        if k not in r:
            raise AssertionError_ex("GET time missing key %r" % k)


def test_get_generic_key(ctx):
    r = ctx.cmd("GET focusDistLim")
    _expect_ok(r, "GET focusDistLim")
    if "focusDistLim" not in r:
        raise AssertionError_ex("generic GET missing key")


def test_get_unknown_target(ctx):
    _expect_bad(ctx.cmd("GET bogus"), "GET bogus")


def test_set_roundtrip(ctx):
    orig = ctx.cmd("GET config")
    if not orig or "targetHours" not in orig:
        raise AssertionError_ex("could not read targetHours baseline")
    baseline = orig["targetHours"]
    trial = 7.5 if abs(float(baseline) - 7.5) > 0.01 else 8.5
    r = ctx.cmd("SET config.targetHours %.1f" % trial)
    _expect_ok(r, "SET config.targetHours")
    try:
        time.sleep(0.5)
        chk = ctx.cmd("GET config")
        _expect_ok(chk, "GET config after SET")
        if abs(float(chk.get("targetHours", 0)) - trial) > 0.01:
            raise AssertionError_ex("targetHours not roundtripped: %s" % chk.get("targetHours"))
    finally:
        ctx.cmd("SET config.targetHours %s" % baseline)
        time.sleep(0.5)


def test_set_invalid_key(ctx):
    _expect_bad(ctx.cmd("SET bogus 123"), "SET bogus")


def test_trigger_invalid_event(ctx):
    _expect_bad(ctx.cmd("TRIGGER 99"), "TRIGGER 99")


def test_trigger_bad_name(ctx):
    _expect_bad(ctx.cmd("TRIGGER NOPE"), "TRIGGER NOPE")


def test_unknown_command(ctx):
    _expect_bad(ctx.cmd("FOO bar"), "FOO bar")


def test_sim_usage_errors(ctx):
    _expect_bad(ctx.cmd("SIM radar 40 1"), "SIM radar missing present arg")
    _expect_bad(ctx.cmd("SIM radar 40"), "SIM radar missing args")
    _expect_bad(ctx.cmd("SIM state NOPE"), "SIM state bad name")
    _expect_bad(ctx.cmd("SIM bogus"), "SIM bogus")
