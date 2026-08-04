import time

from assertions import AssertionError_ex, SkipTest

EVENT_RANGE = list(range(0, 7)) + list(range(8, 15))  # 0-6, 8-14 (7 is undefined)


def _expect_ok(resp, desc):
    if resp is None or not resp.get("ok"):
        raise AssertionError_ex("%s: ok=false (%s)" % (desc, resp.get("error") if resp else "no resp"))


def _trigger(ctx, n, mode="fallback", expect_mode=None):
    r = ctx.cmd("TRIGGER %d %s" % (n, mode))
    _expect_ok(r, "TRIGGER %d %s" % (n, mode))
    if int(r.get("triggered")) != n:
        raise AssertionError_ex("TRIGGER %d: triggered=%s" % (n, r.get("triggered")))
    if expect_mode is not None and r.get("mode") != expect_mode:
        raise AssertionError_ex("TRIGGER %d: mode=%s expected %s" % (n, r.get("mode"), expect_mode))


def test_trigger_all_events_fallback(ctx):
    for n in EVENT_RANGE:
        r = ctx.cmd("TRIGGER %d fallback" % n)
        _expect_ok(r, "TRIGGER %d fallback" % n)
        if int(r.get("triggered")) != n:
            raise AssertionError_ex("event %d: triggered=%s" % (n, r.get("triggered")))
        ctx.wait_for_log(lambda e: "TRIGGER event=%d mode=2" % n in e["data"].get("text", ""),
                         timeout=10, desc="dispatch log for %d" % n)
        time.sleep(0.3)


def test_event7_undefined_acceptance(ctx):
    # Event 7 is numerically accepted (0..EVENT_LATEHOURS_SIT) but has no define.
    r = ctx.cmd("TRIGGER 7 fallback")
    if r is None or not r.get("ok"):
        raise AssertionError_ex("TRIGGER 7 not accepted (quirk regression): %s" % (r or {}))
    if int(r.get("triggered")) != 7:
        raise AssertionError_ex("TRIGGER 7 triggered=%s" % r.get("triggered"))


def test_trigger_numeric_mode(ctx):
    _trigger(ctx, 2, mode="2", expect_mode=2)


def test_trigger_ai_mode_skipped_without_flag(ctx):
    if ctx.cfg.get("allow_ai"):
        raise SkipTest("covered by AI e2e")
    # fallback default is mode 0; verify plain TRIGGER uses mode 0
    r = ctx.cmd("TRIGGER 2")
    _expect_ok(r, "TRIGGER 2")
    if r.get("mode") != 0:
        raise AssertionError_ex("TRIGGER 2 mode=%s expected 0" % r.get("mode"))


def test_trigger_journal_echo(ctx):
    _trigger(ctx, 10, mode="fallback")
    before = time.time()
    ev = ctx.bus.wait_for("mqtt_echo",
                          pred=lambda e: e["ts"] >= before and len(e["data"].get("payload", "")) > 0,
                          timeout=20)
    if ev is None:
        raise AssertionError_ex("no MQTT echo published after JOURNAL trigger "
                                "(TFT history excludes eventType 10)")


def test_trigger_nagging(ctx):
    _trigger(ctx, 11, mode="fallback")


def test_trigger_page(ctx):
    _trigger(ctx, 13, mode="fallback")


def test_trigger_latehours(ctx):
    _trigger(ctx, 14, mode="fallback")


def test_ai_e2e(ctx):
    if not ctx.cfg.get("allow_ai"):
        raise SkipTest("--allow-ai not set")
    ctx.mqtt.bus.clear("ai_trace")
    r = ctx.cmd("TRIGGER 2 ai", timeout=30)
    _expect_ok(r, "TRIGGER 2 ai")
    if r.get("mode") != 1:
        raise AssertionError_ex("TRIGGER 2 ai mode=%s expected 1" % r.get("mode"))
    req = ctx.bus.wait_for("ai_trace", lambda e: e["data"].get("kind") == "request",
                           timeout=30, )
    if req is None:
        raise AssertionError_ex("no AI request trace published")
    resp = ctx.bus.wait_for("ai_trace", lambda e: e["data"].get("kind") == "response",
                            timeout=90, )
    if resp is None:
        raise AssertionError_ex("no AI response trace published")
