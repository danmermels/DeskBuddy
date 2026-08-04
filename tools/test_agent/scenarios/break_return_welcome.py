"""Break -> return: a real break (>3 min) followed by sitting triggers the welcome-back message."""

from scenario_runner import SkipScenario, current_hour, in_window

NAME = "break_return_welcome"
DESC = ("Simulates leaving the desk for a real break (> 3 min), returning and sitting. "
        "Checks the device shows the Welcome Back message (eventType 1) and that a present "
        "state is reached.")
WINDOW = None
CADENCE_MIN = 60


def run(sim, exp):
    if sim.is_first_sit_today():
        raise SkipScenario(
            "firstSitToday=true - a first-sit greeting would fire instead of Welcome Back; "
            "run this after the day's first sit or re-arm with 'SET config firstSitToday 0'")
    if in_window([(0, 6), (21, 24)]):
        raise SkipScenario(
            "late hours now (%02d:00) - a late-hours sit message (eventType 14) would fire "
            "instead; run late_hours_sit instead" % current_hour())
    exp.state("Away", within=20, desc="baseline: device reports Away")
    sim.hold(200, "break away > 3 min minimum")
    sim.sit()
    exp.quiet_motion(within=210,
                     desc="motion baseline quiets while sitting still (180s motion window)")
    exp.state_present(within=45, desc="returning reaches a present state (not Away)")
    exp.screen(event_type=1, within=60, desc="welcome-back message displayed on return")
