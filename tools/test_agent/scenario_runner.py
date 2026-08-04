import importlib
import os
import pkgutil
import sys
import time
import traceback

from expect import Expect, ScenarioRecorder


class SkipScenario(Exception):
    pass


def current_hour():
    return time.localtime().tm_hour


def in_window(window, hour=None):
    """window: None (always) or list of (start_hour, end_hour) pairs."""
    if not window:
        return True
    h = current_hour() if hour is None else hour
    for start, end in window:
        if start <= h < end:
            return True
    return False


def window_str(window):
    if not window:
        return "always"
    return ", ".join("%02d:00-%02d:00" % (s, e) for s, e in window)


def next_window_open(window, after=None):
    """Earliest epoch >= after where the window is open (None = always)."""
    after = after or time.time()
    if not window:
        return after
    base = int(after // 3600) * 3600
    for step in range(8 * 24):
        t = base + step * 3600
        if t < after:
            continue
        if in_window(window, time.localtime(t).tm_hour):
            return t
    return None


def load_scenario_modules():
    mods = {}
    root = os.path.dirname(os.path.abspath(__file__))
    if root not in sys.path:
        sys.path.insert(0, root)
    import scenarios
    for m in pkgutil.iter_modules(scenarios.__path__):
        mod = importlib.import_module("scenarios." + m.name)
        name = getattr(mod, "NAME", m.name)
        mods[name] = mod
    return mods


def describe_scenario(mod):
    return {
        "name": getattr(mod, "NAME", mod.__name__),
        "desc": getattr(mod, "DESC", ""),
        "window": getattr(mod, "WINDOW", None),
        "cadence_min": getattr(mod, "CADENCE_MIN", 60),
        "requires_first_sit": getattr(mod, "REQUIRES_FIRST_SIT", False),
    }


def run_scenario(ctx, mod, force=False, verbose=True):
    """Executes one scenario module and returns its report dict."""
    meta = describe_scenario(mod)
    name = meta["name"]
    window = meta["window"]

    if not in_window(window) and not force:
        return {
            "scenario": name, "desc": meta["desc"], "window": window_str(window),
            "verdict": "skip", "reason": "window closed (now %02d:00, window %s)" %
            (current_hour(), window_str(window)),
            "duration_s": 0.0, "expectations": [], "timeline": [], "received": None,
        }

    rec = ScenarioRecorder(ctx)
    sim = __import__("simulate").HumanSimulator(ctx)
    exp = Expect(rec)
    rec.start()
    started = time.time()
    report = {
        "scenario": name, "desc": meta["desc"], "window": window_str(window),
        "started_epoch": started, "verdict": "error", "reason": "",
        "duration_s": 0.0, "expectations": [], "timeline": [], "received": None,
    }
    try:
        sim.baseline_away()
        try:
            mod.run(sim, exp)
            verdict = "pass" if all(e.ok for e in rec.expectations) else "fail"
            reason = ""
        except SkipScenario as e:
            verdict = "skip"
            reason = str(e)
        except Exception as e:
            verdict = "error"
            reason = "%s: %s" % (type(e).__name__, e)
            if verbose:
                traceback.print_exc()
    finally:
        try:
            sim.stop()
        except Exception:
            pass
        rec.stop()

    report["verdict"] = verdict
    report["reason"] = reason
    report["duration_s"] = round(time.time() - started, 1)
    report["expectations"] = [e.to_dict() for e in rec.expectations]
    report["timeline"] = sim.timeline
    report["received"] = {
        "states": [{"ts": ts, "state": st} for ts, st in rec.states],
        "screens": [{"ts": ts, "text": txt[:96], "eventType": et, "isAi": ai}
                    for ts, txt, et, ai in rec.screens],
        "logs": [{"ts": ts, "category": cat, "text": txt[:200]}
                 for ts, cat, txt in rec.logs],
    }
    return report


def format_report(report):
    lines = []
    lines.append("== scenario: %s (%s) ==" % (report["scenario"], report["window"]))
    if report.get("verdict") == "skip":
        lines.append("SKIP: %s" % report.get("reason"))
        return "\n".join(lines)
    lines.append("verdict: %s  duration: %.1fs" % (report["verdict"], report["duration_s"]))
    if report.get("reason"):
        lines.append("reason: %s" % report["reason"])
    for e in report.get("expectations", []):
        mark = "PASS" if e["ok"] else "FAIL"
        lines.append("[%s] %s" % (mark, e["name"]))
        lines.append("      expected: %s" % e["expected"])
        lines.append("      received: %s" % e["received"])
        if e["detail"]:
            lines.append("      detail:   %s" % e["detail"])
    started = report.get("started_epoch") or 0
    lines.append("received timeline:")
    for st in report.get("received", {}).get("states", []):
        lines.append("  state %s @%.1fs" % (st["state"], st["ts"] - started))
    for sc in report.get("received", {}).get("screens", []):
        lines.append("  screen [%s] %s @%.1fs" % (sc["eventType"], sc["text"],
                                                  sc["ts"] - started))
    for lg in report.get("received", {}).get("logs", []):
        lines.append("  log %s: %s @%.1fs" % (lg["category"], lg["text"],
                                              lg["ts"] - started))
    return "\n".join(lines)


def print_report(report):
    print(format_report(report))
