import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from config import load_config, resolve_serial_port
from livebus import LiveBus
from mqtt_harness import MqttHarness
from telemetry import TelemetryPoller
import runner
from runner import Context, run_suite, run_pattern


def make_reporter(bus):
    def reporter(res):
        bus.publish("test_result", res.to_dict())
    return reporter


def build_env(cfg, bus, use_serial):
    mqtt = MqttHarness(bus, broker=cfg["broker"], port=cfg["broker_port"])
    ok = mqtt.start()
    if not ok:
        raise SystemExit("Failed to connect to MQTT broker %s:%s" % (cfg["broker"], cfg["broker_port"]))

    tele = TelemetryPoller(cfg["http_url"], cfg.get("telemetry_interval_ms", 500),
                           cfg.get("secondary_interval_ms", 2000), bus)
    tele.start()

    ser = None
    if use_serial:
        from serial_harness import SerialHarness
        ser = SerialHarness(bus, cfg)
        ser.start()

    return mqtt, tele, ser


def load_patterns():
    import patterns
    return [fn for _, fn in sorted(vars(patterns).items())
            if _.startswith("pattern_") and callable(fn)]


def main():
    ap = argparse.ArgumentParser(description="DeskBuddy test agent")
    ap.add_argument("--mode", choices=["active", "passive", "scenario", "daemon"],
                    default="active")
    ap.add_argument("--suite", action="append", default=None,
                    help="suite name(s): protocol, presence, modes, behaviour")
    ap.add_argument("--pattern", default=None, help="substring filter for test names")
    ap.add_argument("--slow", action="store_true", help="include slow (classification) tests")
    ap.add_argument("--run-patterns", action="store_true", help="run presence patterns")
    ap.add_argument("--scenario", action="append", default=None,
                    help="scenario name(s) to run in scenario mode (default: all)")
    ap.add_argument("--force", action="store_true",
                    help="run scenarios even if their time window is closed")
    ap.add_argument("--scenario-report", default=None,
                    help="directory for daemon scenario / shadow result logs")
    ap.add_argument("--poll", type=int, default=None, help="daemon poll interval (s)")
    ap.add_argument("--duration", type=int, default=300,
                    help="passive shadow observation seconds")
    ap.add_argument("--probe", type=int, default=None, help="passive probe interval (s)")
    ap.add_argument("--allow-ai", action="store_true", help="enable live Groq AI e2e test")
    ap.add_argument("--oracle", action="store_true", help="LLM analysis of passive transcript")
    ap.add_argument("--serial", default=None, help="serial port override")
    ap.add_argument("--broker", default=None, help="MQTT broker override")
    ap.add_argument("--report", default=None, help="write JSON report to path")
    args = ap.parse_args()

    cfg = load_config()
    if args.broker:
        cfg["broker"] = args.broker
    if args.serial:
        cfg["serial_port"] = args.serial
    if args.probe:
        cfg["probe_interval_s"] = args.probe
    cfg["allow_ai"] = args.allow_ai
    cfg["slow"] = args.slow

    bus = LiveBus()
    reporter = make_reporter(bus)
    serial_requested = cfg.get("serial_enabled") or bool(args.serial)
    serial_port = resolve_serial_port(cfg) if serial_requested else None
    use_serial = serial_port is not None

    print("Connecting MQTT=%s:%s  HTTP=%s  serial=%s" %
          (cfg["broker"], cfg["broker_port"], cfg["http_url"],
           serial_port if use_serial else "off"))
    mqtt, tele, ser = build_env(cfg, bus, use_serial)
    ctx = Context(bus, mqtt, cfg, tele, ser, reporter)

    results = []

    try:
        if args.mode == "active":
            suites = args.suite or ["protocol", "presence", "modes", "behaviour"]
            for s in suites:
                print("== suite: %s ==" % s)
                results.extend(run_suite(ctx, s, pattern=args.pattern, include_slow=args.slow))
            if args.run_patterns:
                print("== patterns ==")
                ctx.sim_stop()
                for pfn in load_patterns():
                    results.append(run_pattern(ctx, pfn))
        elif args.mode == "scenario":
            from scenario_runner import load_scenario_modules, print_report, run_scenario
            mods = load_scenario_modules()
            wanted = args.scenario or sorted(mods)
            reports = []
            for name in wanted:
                if name not in mods:
                    print("unknown scenario %r (have: %s)" % (name, sorted(mods)))
                    continue
                print("== scenario: %s ==" % name)
                rep = run_scenario(ctx, mods[name], force=args.force, verbose=True)
                print_report(rep)
                reports.append(rep)
            results.extend([r for r in reports if r["verdict"] == "fail"])
            if args.report:
                os.makedirs(os.path.dirname(os.path.abspath(args.report)), exist_ok=True)
                with open(args.report, "w", encoding="utf-8") as f:
                    json.dump(reports, f, indent=2)
                print("report written to %s" % args.report)
        elif args.mode == "daemon":
            from scheduler import ScenarioScheduler
            sched = ScenarioScheduler(ctx, report_dir=args.scenario_report,
                                      poll_s=args.poll or 20)
            try:
                sched.run_daemon()
            except KeyboardInterrupt:
                print("\nstopping daemon")
            finally:
                sched.stopped = True
        else:
            from shadow import ShadowObserver
            obs = ShadowObserver(ctx, duration=args.duration,
                                 report_dir=args.scenario_report)
            profile = obs.run()
            print("== passive shadow observation complete ==")
            for row in obs.checks_summary():
                print("  [%-14s] %-22s %s" % (row["status"], row["name"],
                                              row["received"] or row["expected"]))
            print(json.dumps(profile.get("summary", {}), indent=2))
            if args.report:
                os.makedirs(os.path.dirname(os.path.abspath(args.report)), exist_ok=True)
                with open(args.report, "w", encoding="utf-8") as f:
                    json.dump(profile, f, indent=2)
                print("report written to %s" % args.report)
            if args.oracle:
                from oracle import analyze_profile
                print(analyze_profile(profile, cfg))
    finally:
        tele.stop()
        if ser:
            ser.stop()
        mqtt.stop()

    if args.report and args.mode == "active":
        os.makedirs(os.path.dirname(os.path.abspath(args.report)), exist_ok=True)
        with open(args.report, "w", encoding="utf-8") as f:
            json.dump([r.to_dict() for r in results], f, indent=2)
        print("report written to %s" % args.report)

    if results:
        npass = sum(1 for r in results
                    if getattr(r, "status", r.get("verdict")) in ("pass", "skip"))
        nfail = sum(1 for r in results
                    if getattr(r, "status", r.get("verdict")) == "fail")
        nskip = sum(1 for r in results if getattr(r, "status", None) == "skip")
        if any(isinstance(r, dict) for r in results):
            npass = sum(1 for r in results if r.get("verdict") != "fail")
            nskip = 0
        print("summary: %d pass, %d fail, %d skip" % (npass, nfail, nskip))
        sys.exit(1 if nfail else 0)


if __name__ == "__main__":
    main()
