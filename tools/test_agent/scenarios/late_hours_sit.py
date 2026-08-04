"""Late-hours sit: a real break (>3 min) followed by a sit during late hours shows the late-hours message."""

NAME = "late_hours_sit"
DESC = ("Simulates a real break (> 3 min) and a sit during late hours. Checks the device shows "
        "the late-hours message (eventType 14) instead of a greeting. Approximates 'late hours' "
        "as 21:00-06:00; the learned workday may differ.")
WINDOW = [(0, 6), (21, 24)]
CADENCE_MIN = 60


def run(sim, exp):
    exp.state("Away", within=20, desc="baseline: device reports Away")
    sim.hold(200, "break away > 3 min minimum")
    sim.sit()
    exp.state("Focus", within=30, desc="returning classifies back to Focus")
    exp.screen(event_type=14, within=60, desc="late-hours sit message displayed")
