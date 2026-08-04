"""Busy detection: Focus while still, Busy once sustained motion exceeds the ratio limit."""

NAME = "busy_detection"
DESC = ("Simulates sitting still until the device settles into Focus, then sustained typing "
        "in the focus zone and checks the classifier escalates to Busy.")
WINDOW = None
CADENCE_MIN = 45


def run(sim, exp):
    exp.state("Away", within=20, desc="baseline: device reports Away")
    sim.sit()
    exp.quiet_motion(within=210,
                     desc="motion baseline quiets while sitting still (180s motion window)")
    exp.state_now("Focus", within=45, desc="sitting still in focus zone classifies as Focus")
    sim.fidget(seconds=40)
    exp.state("Busy", within=90,
              desc="sustained motion in focus zone escalates to Busy (30s sticky confirm)")
