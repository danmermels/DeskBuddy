"""Raw-signal calibration for the two classifier thresholds.

Calibrates ``focusDistLim`` and ``motionRatioLim`` from the raw radar signal -
the same data the Web UI "Radar Signal History" chart plots (``/radar-data``).
No firmware-processed metrics are used: the calibrator samples
``rawDetectionDist``, ``movingTargetDetected`` and ``presenceDetected`` and
applies its own short-window filters tool-side.

For each of the four simple poses (Close, Far, Still, Moving) it watches the
filtered value and, once it has held a stable value for a sustained period,
records it and moves on - no fixed pose timings.

The firmware classifier must measure motion with the same window the calibrator
uses, so the calibrator first sets ``config.motionWindow`` (a runtime config,
default 180 s, unchanged behaviour until the calibrator lowers it) to
``MOTION_WINDOW_S`` before running the poses.

Thresholds are then computed as midpoints of the two axes:

* ``focusDistLim``   = midpoint(close, far) distance
* ``motionRatioLim`` = midpoint(still, moving) motion
"""

import statistics
import time


# --- window / filter constants ------------------------------------------------
MOTION_WINDOW_S = 20      # short motion window applied by the calibrator (also set in firmware)
DIST_WINDOW_S = 5         # rolling median window for raw distance
DIST_SETTLE_S = 6         # seconds the distance median must hold within tolerance
DIST_TOL_CM = 8           # max raw range for the distance to count as stable
MOTION_STILL_MAX = 25     # still pose: ratio must stay at/below this to be "still"
MOTION_MOVING_MIN = 50    # moving pose: ratio must stay at/above this to be "moving"
MOTION_HOLD_S = 3         # seconds the motion target condition must have held
POSE_TIMEOUT_S = 90       # give up on a pose that never stabilizes
START_DELAY_S = 1.0

# --- threshold computation constants -----------------------------------------
SEPARATION_MIN_CM = 30    # required far-close gap (cm)
SEPARATION_MIN_RATIO = 30 # required moving-still gap (%)
PADDING_CM = 10           # headroom each side of the focus boundary
PADDING_PCT = 10          # headroom each side of the motion limit

RECOMMENDED_ORDER = ("Close", "Far", "Still", "Moving")

POSE_SPECS = {
    "Close": {
        "kind": "distance",
        "instruct": ("Lean as close to the radar as your natural close working position "
                     "(e.g. reaching for the monitor) and hold it - records your near "
                     "distance. I will move on once it reads a stable value."),
        "locks": "near distance",
    },
    "Far": {
        "kind": "distance",
        "instruct": ("Sit back at your natural farthest working position and hold it - "
                     "records your far distance. I will move on once it reads a stable "
                     "value."),
        "locks": "far distance",
    },
    "Still": {
        "kind": "motion", "target": "still",
        "instruct": ("Sit at your normal position, hands off the keyboard, perfectly "
                     "still for a moment - records your motion floor. The short motion "
                     "window takes up to %ds to settle, so hold still until I move on."
                     % MOTION_WINDOW_S),
        "locks": "motion baseline",
    },
    "Moving": {
        "kind": "motion", "target": "moving",
        "instruct": ("Sit at your normal position and type / move continuously for a "
                     "moment - records your motion ceiling."),
        "locks": "motion ceiling",
    },
}

# Firmware mood classifier model (main.cpp):
#   near + still  -> Focus          far (any motion) -> Regular Activity until the
#   near + moving -> Busy           user has been continuously present-but-far for
#                                   DISTRACTED_FAR_MIN_S, then Distracted.
# Motion only splits the NEAR zone; the far side is distance + time.
MODEL_STATES = {("near", "low"): "Focus", ("near", "high"): "Busy",
                ("far", "low"): "Regular Activity", ("far", "high"): "Regular Activity"}


class CalibrationError(Exception):
    pass


class RawCapture:
    """Collects raw radar samples from the bus for a calibration pose."""

    def __init__(self, bus):
        self.bus = bus
        self.samples = []          # list of {"ts", "dist_raw", "moving", "present"}
        self._cb = self._on_telemetry

    def start(self):
        self.samples = []
        self.bus.subscribe("telemetry", self._cb)

    def stop(self):
        self.bus.unsubscribe("telemetry", self._cb)

    def clear(self):
        self.samples = []

    def _on_telemetry(self, ev):
        d = ev["data"]
        moving = d.get("movingTargetDetected")
        self.samples.append({
            "ts": ev["ts"],
            "dist_raw": d.get("rawDetectionDist"),
            "moving": bool(moving) if isinstance(moving, (bool, int)) else None,
            "present": d.get("presenceDetected"),
        })


def _num(v):
    return isinstance(v, (int, float)) and v > 0


def _rolling_median_dm(samples, window_s=DIST_WINDOW_S, now_ts=None):
    """Median of raw distance over the last ``window_s`` seconds (cm)."""
    if not samples:
        return None
    end = now_ts if now_ts is not None else samples[-1]["ts"]
    vals = [s["dist_raw"] for s in samples if end - s["ts"] <= window_s and _num(s["dist_raw"])]
    if not vals:
        return None
    return statistics.median(vals)


def rolling_motion_ratio(samples, window_s=MOTION_WINDOW_S, now_ts=None):
    """% of samples with the moving flag over the last ``window_s`` seconds."""
    if not samples:
        return None
    end = now_ts if now_ts is not None else samples[-1]["ts"]
    mv = [s["moving"] for s in samples if end - s["ts"] <= window_s and s["moving"] is not None]
    if not mv:
        return None
    return round(100.0 * sum(1 for m in mv if m) / len(mv), 1)


def distance_stable(samples, window_s=DIST_WINDOW_S, hold_s=DIST_SETTLE_S,
                    tol_cm=DIST_TOL_CM):
    """Returns (median_cm, is_stable) for the raw distance.

    Stable when the rolling median matches the median of the last ``hold_s``
    seconds and the raw values in that window stay within ``2 * tol_cm``.
    """
    med = _rolling_median_dm(samples, window_s)
    if med is None or not samples:
        return med, False
    end = samples[-1]["ts"]
    hold_vals = [s["dist_raw"] for s in samples if end - s["ts"] <= hold_s and _num(s["dist_raw"])]
    if len(hold_vals) < max(3, int(hold_s * 1.5)):
        return med, False
    med_hold = statistics.median(hold_vals)
    stable = (abs(med_hold - med) <= tol_cm and
              (max(hold_vals) - min(hold_vals)) <= 2 * tol_cm)
    return med, stable


def motion_stable(samples, target="still", window_s=MOTION_WINDOW_S, hold_s=MOTION_HOLD_S):
    """Returns (ratio_pct, is_stable) for a motion pose.

    ``target`` is "still" or "moving". The ratio over the last ``window_s``
    seconds must have satisfied the target for the last ``hold_s`` seconds.
    The still pose additionally needs a full ``window_s`` of history, otherwise
    an empty pre-pose buffer would read as perfectly still.
    """
    ratio = rolling_motion_ratio(samples, window_s)
    if ratio is None or not samples:
        return ratio, False
    if target == "still":
        if samples[-1]["ts"] - samples[0]["ts"] < window_s:
            return ratio, False
        if ratio > MOTION_STILL_MAX:
            return ratio, False
    else:
        if ratio < MOTION_MOVING_MIN:
            return ratio, False
    end = samples[-1]["ts"]
    within = [s for s in samples if end - s["ts"] <= hold_s]
    for s in within:
        r = rolling_motion_ratio(samples, window_s, now_ts=s["ts"])
        if r is None:
            continue
        if target == "still" and r > MOTION_STILL_MAX:
            return ratio, False
        if target == "moving" and r < MOTION_MOVING_MIN:
            return ratio, False
    return ratio, True


def run_pose(cap, name, progress=None, timeout=POSE_TIMEOUT_S):
    """Watches the raw stream until a stable value is found, then returns it.

    ``progress(info)`` is called with a dict of the live filtered value so the
    UI can display it. Raises :class:`CalibrationError` on timeout.
    """
    spec = POSE_SPECS[name]
    start = time.time()
    while True:
        elapsed = time.time() - start
        if elapsed > timeout:
            raise CalibrationError(
                "pose did not stabilize within %.0fs: %s" % (timeout, name))
        if spec["kind"] == "distance":
            value, stable = distance_stable(cap.samples)
        else:
            value, stable = motion_stable(cap.samples, target=spec["target"])
        if progress:
            progress({"name": name, "kind": spec["kind"], "value": value,
                      "stable": stable, "elapsed": elapsed})
        if stable:
            result = {"name": name, "kind": spec["kind"],
                      "stable_after": round(elapsed, 1)}
            if spec["kind"] == "distance":
                result["dist"] = round(value, 1)
            else:
                result["ratio"] = value
            return result
        time.sleep(0.2)


def compute_thresholds(captures, motion_window=MOTION_WINDOW_S):
    """Merges the four captures into midpoint threshold recommendations.

    Returns ``(merged, warnings, locks)``: SET commands to apply, why something
    could not be locked, and a human-readable summary of what was locked.
    """
    merged = []
    warnings = []
    locks = []

    close = captures.get("Close") or {}
    far = captures.get("Far") or {}
    still = captures.get("Still") or {}
    moving = captures.get("Moving") or {}

    locks.append("motion window set to %ds (config.motionWindow)" % motion_window)

    # ---- focusDistLim: midpoint of close vs far ----
    if "dist" in close and "dist" in far:
        gap = far["dist"] - close["dist"]
        need = SEPARATION_MIN_CM + 2 * PADDING_CM
        if gap >= need:
            fl = int(round((close["dist"] + far["dist"]) / 2))
            locks.append("close %.0f cm vs far %.0f cm -> focusDistLim %d cm "
                         "(>= %d cm headroom each side)"
                         % (close["dist"], far["dist"], fl, PADDING_CM))
            merged.append({"key": "focusDistLim", "proposed": fl,
                           "cmd": "SET config.focusDistLim %d" % fl})
        else:
            warnings.append(
                "close (%.0f cm) and far (%.0f cm) are only %.0f cm apart (< %d cm "
                "required); lean in much closer / lean further back, then re-capture"
                % (close["dist"], far["dist"], gap, need))
            locks.append("focusDistLim NOT lockable (near/far too close)")
    else:
        locks.append("focusDistLim needs the Close and Far captures")

    # ---- motionRatioLim: midpoint of still vs moving ----
    if "ratio" in still and "ratio" in moving:
        gap = moving["ratio"] - still["ratio"]
        need = SEPARATION_MIN_RATIO + 2 * PADDING_PCT
        if gap >= need:
            ml = int(round((still["ratio"] + moving["ratio"]) / 2))
            locks.append("still %.0f%% vs moving %.0f%% -> motionRatioLim %d%% "
                         "(>= %d%% headroom each side)"
                         % (still["ratio"], moving["ratio"], ml, PADDING_PCT))
            merged.append({"key": "motionRatioLim", "proposed": ml,
                           "cmd": "SET config.motionRatioLim %d" % ml})
        else:
            warnings.append(
                "still (%.0f%%) and moving (%.0f%%) are only %.0f%% apart (< %d%% "
                "required); sit perfectly still / move much more vigorously, then "
                "re-capture" % (still["ratio"], moving["ratio"], gap, need))
            locks.append("motionRatioLim NOT lockable (still/moving too close)")
    else:
        locks.append("motionRatioLim needs the Still and Moving captures")

    return merged, warnings, locks


def derived_states(captures):
    """The states the new limits produce, as sanity check lines.

    Motion only splits the near zone (Focus/Busy). The far side ignores motion:
    Regular Activity until the user has been continuously present-but-far for
    DISTRACTED_FAR_MIN_MS, then the relaxed (Distracted) mood.
    """
    close = (captures.get("Close") or {}).get("dist")
    far = (captures.get("Far") or {}).get("dist")
    still = (captures.get("Still") or {}).get("ratio")
    moving = (captures.get("Moving") or {}).get("ratio")
    if close is None or far is None or still is None or moving is None:
        return []
    return [
        ("Focus", "close + still", "%.0f cm < limit, %.0f%% <= limit" % (close, still)),
        ("Busy", "close + moving", "%.0f cm < limit, %.0f%% > limit" % (close, moving)),
        ("Regular Activity", "far", "%.0f cm >= limit (until the far-timer)" % far),
        ("Distracted", "far + time", "%.0f cm >= limit for >= DISTRACTED_FAR_MIN_S" % far),
    ]


def retry_set(captures):
    """Poses whose data did not make sense and must be re-captured."""
    retry = []
    for name in RECOMMENDED_ORDER:
        c = captures.get(name)
        if not c or c.get("error"):
            retry.append(name)
    try:
        _merged, warnings, locks = compute_thresholds(captures)
    except Exception:
        warnings, locks = [], []
    for lk in locks:
        if "focusDistLim NOT lockable" in lk:
            for n in ("Close", "Far"):
                if n not in retry:
                    retry.append(n)
        if "motionRatioLim NOT lockable" in lk:
            for n in ("Still", "Moving"):
                if n not in retry:
                    retry.append(n)
    return retry
