import queue
import threading
import time
from collections import defaultdict, deque


class LiveBus:
    def __init__(self, history=200):
        self._lock = threading.Lock()
        self._subs = defaultdict(set)
        self._history = defaultdict(lambda: deque(maxlen=history))
        self._queue = queue.Queue()
        self._waiters = []

    def publish(self, etype, data=None):
        ev = {"type": etype, "ts": time.time(), "data": data if data is not None else {}}
        with self._lock:
            self._history[etype].append(ev)
            self._queue.put(ev)
            subs = list(self._subs.get(etype, ())) + list(self._subs.get("*", ()))
            waiters = list(self._waiters)
        for cb in subs:
            try:
                cb(ev)
            except Exception:
                pass
        for w in waiters:
            if w["type"] == etype and w["pred"](ev):
                w["event"] = ev
                w["done"].set()

    def subscribe(self, etype, callback):
        with self._lock:
            self._subs[etype].add(callback)
        return callback

    def unsubscribe(self, etype, callback):
        with self._lock:
            self._subs[etype].discard(callback)

    def wait_for(self, etype, pred=None, timeout=10.0):
        pred = pred or (lambda e: True)
        with self._lock:
            for ev in reversed(list(self._history[etype])):
                if pred(ev):
                    return ev
            done = threading.Event()
            waiter = {"type": etype, "pred": pred, "done": done, "event": None}
            self._waiters.append(waiter)
        done.wait(timeout)
        with self._lock:
            if waiter in self._waiters:
                self._waiters.remove(waiter)
        return waiter["event"]

    def recent(self, etype, n=20):
        with self._lock:
            return list(self._history[etype])[-n:]

    def clear(self, etype=None):
        with self._lock:
            if etype is None:
                self._history.clear()
            else:
                self._history.pop(etype, None)

    def drain(self):
        evs = []
        try:
            while True:
                evs.append(self._queue.get_nowait())
        except queue.Empty:
            pass
        return evs
