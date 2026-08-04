import json
import time

import paho.mqtt.client as mqtt

TOPIC_CMD = "deskbuddy/debug/cmd"
TOPIC_RESP = "deskbuddy/debug/resp"
TOPIC_ECHO = "deskbuddy/echo"
TOPIC_STATUS = "deskbuddy/status"
TOPIC_HEAP = "deskbuddy/heap"
LOG_PREFIX = "deskbuddy/log/"
AI_PREFIX = "deskbuddy/debug/ai/"


class MqttHarness:
    def __init__(self, bus, broker="192.168.15.18", port=1883,
                 client_id="deskbuddy-test-agent", subscribe="deskbuddy/#"):
        self.bus = bus
        self.broker = broker
        self.port = port
        self.client_id = client_id
        self.subscribe_topic = subscribe
        self.client = mqtt.Client(client_id=client_id)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self.client.on_disconnect = self._on_disconnect
        self.connected = False

    def start(self, timeout=10.0):
        self.client.connect_async(self.broker, self.port, 60)
        self.client.loop_start()
        deadline = time.time() + timeout
        while not self.connected and time.time() < deadline:
            time.sleep(0.1)
        return self.connected

    def stop(self):
        try:
            self.client.loop_stop()
            self.client.disconnect()
        except Exception:
            pass

    def _on_connect(self, client, userdata, flags, rc):
        self.connected = rc == 0
        if self.connected:
            self.client.subscribe(self.subscribe_topic)
        if self.bus:
            self.bus.publish("mqtt_conn", {"connected": self.connected, "rc": rc})

    def _on_disconnect(self, client, userdata, rc):
        self.connected = False
        if self.bus:
            self.bus.publish("mqtt_conn", {"connected": False, "rc": rc})

    def _on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode("utf-8", errors="replace")
        if topic == TOPIC_RESP:
            try:
                data = json.loads(payload)
            except Exception:
                data = {"_raw": payload}
            data.setdefault("_raw", payload)
            if self.bus:
                self.bus.publish("mqtt_resp", data)
        elif topic == TOPIC_ECHO:
            if self.bus:
                self.bus.publish("mqtt_echo", {"payload": payload, "ts": time.time()})
        elif topic == TOPIC_STATUS:
            if self.bus:
                self.bus.publish("mqtt_status", {"payload": payload, "ts": time.time()})
        elif topic.startswith(LOG_PREFIX):
            cat = topic[len(LOG_PREFIX):]
            if self.bus:
                self.bus.publish("mqtt_log", {"category": cat, "text": payload, "ts": time.time()})
        elif topic.startswith(AI_PREFIX):
            kind = topic[len(AI_PREFIX):]
            if self.bus:
                self.bus.publish("ai_trace", {"kind": kind, "text": payload, "ts": time.time()})
        elif topic == TOPIC_HEAP:
            if self.bus:
                self.bus.publish("heap", {"payload": payload, "ts": time.time()})

    def publish(self, topic, payload, qos=0):
        return self.client.publish(topic, payload, qos=qos)

    def cmd(self, text):
        self.client.publish(TOPIC_CMD, text, qos=1)

    def wait_resp(self, pred=None, timeout=10.0):
        return self.bus.wait_for("mqtt_resp", pred=pred, timeout=timeout)

    def wait_log(self, pred=None, timeout=10.0):
        return self.bus.wait_for("mqtt_log", pred=pred, timeout=timeout)
