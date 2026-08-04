import re

from serial_harness import SerialHarness


class AssertionError_ex(Exception):
    pass


class SkipTest(Exception):
    pass


# Device state names (getPresenceStateName in main.cpp:162)
DEVICE_STATES = {
    "AWAY": "Away",
    "FOCUS": "Focus",
    "BUSY": "Busy",
    "DISTRACTED": "Distracted",
    "REGULAR": "Regular Activity",
}


def dstate(name):
    return DEVICE_STATES.get(name, name)


def wait_for_topic(bus, etype, pred=None, timeout=10.0, desc="topic event"):
    ev = bus.wait_for(etype, pred=pred, timeout=timeout)
    if ev is None:
        raise AssertionError_ex("Timed out waiting for %s (%.1fs)" % (desc, timeout))
    return ev


def wait_for_resp(bus, pred=None, timeout=10.0, desc="debug resp"):
    return wait_for_topic(bus, "mqtt_resp", pred=pred, timeout=timeout, desc=desc)


def wait_for_log(bus, pred=None, timeout=10.0, desc="log line"):
    return wait_for_topic(bus, "mqtt_log", pred=pred, timeout=timeout, desc=desc)


def wait_for_serial(bus, pattern, timeout=10.0, desc="serial line"):
    rx = re.compile(pattern)
    return wait_for_topic(bus, "serial_line",
                          pred=lambda e: rx.search(e["data"].get("line", "")),
                          timeout=timeout, desc=desc)


def expect_echo(bus, text, timeout=10.0):
    return wait_for_topic(bus, "mqtt_echo",
                          pred=lambda e: text in e["data"].get("payload", ""),
                          timeout=timeout, desc="echo '%s'" % text)


def ok_pred(resp, key="ok", expected=True):
    return resp.get(key) == expected


def state_pred(name):
    return lambda resp: resp.get("state") == name
