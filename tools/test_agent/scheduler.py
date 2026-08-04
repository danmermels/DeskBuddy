import json
import os
import time

from scenario_runner import (next_window_open, run_scenario, window_str,
                             load_scenario_modules)


class ScenarioScheduler:
    """Continuously runs scenarios at their real-world test hours.

    Always-on scenarios fire every CADENCE_MIN. Windowed scenarios (morning
    greeting, late hours) only fire while the device's real clock is inside
    their window, snapped to cadence intervals. Every run is appended to a
    per-day JSONL result log so results accumulate over the day.
    """

    def __init__(self, ctx, report_dir=None, verbose=True, poll_s=20):
        self.ctx = ctx
        self.report_dir = report_dir or os.path.join(
            os.path.dirname(os.path.abspath(__file__)), "reports")
        os.makedirs(self.report_dir, exist_ok=True)
        self.verbose = verbose
        self.poll_s = poll_s
        self.stopped = False
        self.mods = load_scenario_modules()
        now = time.time()
        self.state = {}
        for name, mod in self.mods.items():
            nxt = self._next_due(mod, now + 5)
            self.state[name] = {
                "desc": getattr(mod, "DESC", ""),
                "window": window_str(getattr(mod, "WINDOW", None)),
                "cadence_min": getattr(mod, "CADENCE_MIN", 60),
                "next_due": nxt,
                "last_run": None,
                "last_verdict": None,
                "last_reason": "",
                "runs": 0,
            }

    def _next_due(self, mod, after):
        cad_s = getattr(mod, "CADENCE_MIN", 60) * 60
        base = (after or time.time()) + cad_s
        nxt = next_window_open(getattr(mod, "WINDOW", None), base)
        return nxt if nxt is not None else None

    # -- execution --------------------------------------------------------

    def run_daemon(self):
        print("scenario daemon: %d scenarios, polling every %ds" %
              (len(self.mods), self.poll_s))
        for name, st in self.state.items():
            print("  %-22s window=%-22s cadence=%3dm next=%.0f" %
                  (name, st["window"], st["cadence_min"],
                   (st["next_due"] or 0) - time.time()))
        while not self.stopped:
            try:
                self._tick()
            except Exception as e:
                print("daemon tick error: %s" % e)
            time.sleep(self.poll_s)
        print("scenario daemon stopped")

    def _tick(self):
        now = time.time()
        due = [n for n, st in self.state.items()
               if st["next_due"] and now >= st["next_due"]]
        due.sort(key=lambda n: self.state[n]["next_due"])
        for name in due:
            if self.stopped:
                break
            self._run_one(name)

    def _run_one(self, name):
        mod = self.mods[name]
        print("\n>> [%s] %s | window %s | %s" % (
            time.strftime("%H:%M:%S"), name, self.state[name]["window"],
            time.strftime("%Y-%m-%d")))
        report = run_scenario(self.ctx, mod, verbose=self.verbose)
        self._record(name, report)

    def run_now(self, name, force=False):
        mod = self.mods.get(name)
        if mod is None:
            raise KeyError("unknown scenario %r (have: %s)" % (name, sorted(self.mods)))
        report = run_scenario(self.ctx, mod, force=force, verbose=self.verbose)
        self._record(name, report)
        self.state[name]["next_due"] = self._next_due(mod, time.time())
        return report

    # -- recording --------------------------------------------------------

    def _record(self, name, report):
        npass = sum(1 for e in report.get("expectations", []) if e["ok"])
        st = self.state[name]
        st["last_run"] = time.time()
        st["last_verdict"] = report["verdict"]
        st["last_reason"] = report.get("reason", "")
        st["runs"] += 1
        print("[%s] verdict=%s pass=%d/%d duration=%.0fs%s" % (
            name, report["verdict"], npass, len(report.get("expectations", [])),
            report.get("duration_s", 0),
            ("  (%s)" % report["reason"]) if report.get("reason") else ""))

        line = {
            "ts": time.time(),
            "date": time.strftime("%Y-%m-%d"),
            "scenario": name,
            "window": report["window"],
            "verdict": report["verdict"],
            "reason": report.get("reason", ""),
            "duration_s": report.get("duration_s"),
            "n_expectations": len(report.get("expectations", [])),
            "n_pass": npass,
            "expectations": report.get("expectations"),
            "received": report.get("received"),
        }
        path = os.path.join(self.report_dir,
                            "scenario_results_%s.jsonl" % time.strftime("%Y%m%d"))
        with open(path, "a", encoding="utf-8") as f:
            f.write(json.dumps(line) + "\n")
        if self.verbose and report["verdict"] in ("fail", "error"):
            from scenario_runner import print_report
            print_report(report)

    def summary(self):
        out = []
        for name, st in self.state.items():
            out.append({"name": name, **st})
        return out
