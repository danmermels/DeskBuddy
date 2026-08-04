import importlib
import inspect
import json
import time
import traceback

from assertions import SkipTest, wait_for_log


class TestResult:
    def __init__(self, name, suite=None, status="pass", message="", duration=0.0):
        self.name = name
        self.suite = suite
        self.status = status  # pass | fail | skip
        self.message = message
        self.duration = duration

    def to_dict(self):
        return {"name": self.name, "suite": self.suite, "status": self.status,
                "message": self.message, "duration": round(self.duration, 3)}


class Context:
    def __init__(self, bus, mqtt, cfg, telemetry, serial, reporter):
        self.bus = bus
        self.mqtt = mqtt
        self.cfg = cfg
        self.telemetry = telemetry
        self.serial = serial
        self.reporter = reporter
        self.restores = []

    # -- command helpers -------------------------------------------------
    def cmd(self, text, timeout=None):
        t = timeout or self.cfg.get("default_timeout_s", 10)
        self.mqtt.bus.clear("mqtt_resp")
        sent = time.time()
        self.mqtt.cmd(text)
        ev = self.mqtt.wait_resp(pred=lambda e: e["data"].get("cmd") == text and e["ts"] >= sent,
                                 timeout=t)
        if ev is None:
            # device may have dropped the command; resend once (echoed cmd matches,
            # so a stale resp for identical text still describes a processed command)
            sent = time.time()
            self.mqtt.cmd(text)
            ev = self.mqtt.wait_resp(pred=lambda e: e["data"].get("cmd") == text and e["ts"] >= sent,
                                     timeout=t)
        return ev["data"] if ev else None

    def sim_stop(self):
        return self.cmd("SIM stop", timeout=8)

    def snapshot_config(self, keys):
        snap = {}
        for k in keys:
            r = self.cmd("GET config")
            if r:
                snap[k] = r.get(k)
        return snap

    def restore_config(self, snap):
        for k, v in snap.items():
            if v is not None:
                self.cmd("SET %s %s" % (k, v))
                time.sleep(0.3)

    # -- telemetry helpers ------------------------------------------------
    def latest_telemetry(self):
        return self.telemetry.last_data if self.telemetry else None

    def wait_telemetry(self, pred=None, timeout=10.0, desc="telemetry", anchor=False):
        if anchor:
            before = time.time()
            inner = pred or (lambda e: True)
            pred = lambda e: e["ts"] >= before and inner(e)
        return self.bus.wait_for("telemetry", pred=pred, timeout=timeout)

    def wait_for_log(self, pred=None, timeout=10.0, desc="log line"):
        return wait_for_log(self.bus, pred=pred, timeout=timeout, desc=desc)


def collect_suite_tests(suite_name):
    mod = importlib.import_module("suites.test_" + suite_name)
    tests = []
    for name, fn in sorted(vars(mod).items()):
        if name.startswith("test_") and callable(fn) and not getattr(fn, "_skip", False):
            slow = getattr(fn, "_slow", False)
            tests.append((name, fn, slow))
    return mod, tests


def run_suite(ctx, suite_name, pattern=None, include_slow=True):
    mod, tests = collect_suite_tests(suite_name)
    results = []
    for name, fn, slow in tests:
        if pattern and pattern.lower() not in name.lower():
            continue
        if slow and not include_slow:
            continue
        results.append(run_test(ctx, suite_name, name, fn))
    return results


def run_test(ctx, suite_name, name, fn):
    started = time.time()
    try:
        fn(ctx)
        res = TestResult(name, suite_name, "pass", "", time.time() - started)
    except SkipTest as e:
        res = TestResult(name, suite_name, "skip", str(e), time.time() - started)
    except AssertionError as e:
        res = TestResult(name, suite_name, "fail", str(e), time.time() - started)
    except Exception as e:
        res = TestResult(name, suite_name, "fail",
                         "%s: %s" % (type(e).__name__, e), time.time() - started)
        traceback.print_exc()
    ctx.reporter(res)
    return res


def run_pattern(ctx, pattern_fn):
    name = pattern_fn.__name__
    started = time.time()
    try:
        if getattr(pattern_fn, "_slow", False) and not ctx.cfg.get("slow"):
            raise SkipTest("slow pattern; pass --slow to run")
        pattern_fn(ctx)
        res = TestResult(name, "patterns", "pass", "", time.time() - started)
    except SkipTest as e:
        res = TestResult(name, "patterns", "skip", str(e), time.time() - started)
    except AssertionError as e:
        res = TestResult(name, "patterns", "fail", str(e), time.time() - started)
    except Exception as e:
        res = TestResult(name, "patterns", "fail",
                         "%s: %s" % (type(e).__name__, e), time.time() - started)
        traceback.print_exc()
    ctx.reporter(res)
    return res
