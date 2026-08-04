"""Presence classifier: Away <-> Focus driven by presence-only radar sim."""

NAME = "presence_classifier"
DESC = ("Simulates sitting down in the focus zone (motion-free) and leaving. Normalises the "
        "motion baseline first (the device keeps a 180 s motion window), then checks the "
        "presence classifies as Focus / Away.")
WINDOW = None
CADENCE_MIN = 10


def run(sim, exp):
    exp.state("Away", within=20, desc="baseline: device reports Away when nobody is at the desk")
    sim.sit()
    exp.quiet_motion(within=210,
                     desc="motion baseline quiets while sitting still (180s motion window)")
    exp.state_now("Focus", within=45, desc="quiet sitting in focus zone classifies as Focus")
    sim.leave()
    exp.state("Away", within=25, desc="leaving the desk classifies back to Away")
