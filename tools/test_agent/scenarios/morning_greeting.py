"""Morning greeting: the first sit of the day triggers the first-sit greeting."""

from scenario_runner import SkipScenario

NAME = "morning_greeting"
DESC = ("Simulates the day's first sit during morning hours and checks the device shows the "
        "first-sit greeting (eventType 0). WARNING: running this consumes the day's first-sit "
        "greeting (firstSitToday goes false); re-arm with 'SET config firstSitToday 1' to re-test.")
WINDOW = [(5, 12)]
CADENCE_MIN = 90


def run(sim, exp):
    if not sim.is_first_sit_today():
        raise SkipScenario(
            "firstSitToday=false (device already had its first sit today) - nothing to validate")
    exp.state("Away", within=20, desc="baseline: device reports Away")
    sim.sit()
    exp.state("Focus", within=30, desc="first sit classifies to Focus (overnight presence debounce)")
    exp.log(category="STATE", contains="Away->Present", within=30,
            desc="STATE log records the sit transition")
    exp.screen(event_type=0, within=60, desc="first-sit greeting shown on screen")
