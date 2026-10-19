"""Hang doi ben vung. Chi danh dau da gui sau MQTT PUBACK QoS 1."""
import copy
import json
import threading
import time
import uuid
from .storage import flatten, now_ms


def deliver(store, row, client):
    packet = json.dumps({'ts': row['ts'], 'values': flatten(row['data'])}, allow_nan=False)
    info = client.publish('v1/devices/me/telemetry', packet, qos=1, retain=False)
    if info.rc != 0:
        raise RuntimeError('MQTT chua nhan ban tin')
    info.wait_for_publish(timeout=5)
    if not info.is_published():
        raise TimeoutError('Chua nhan duoc MQTT PUBACK; giu lai ban ghi')
    store.mark_sent(row['id'])


class ThingsBoard:
    def __init__(self, store, controller):
        self.store, self.controller = store, controller
        self.stop_event = threading.Event()
        self.lock = threading.Lock()
        self.state = dict(status='disabled', last_error='', last_sent=None)
        self.thread = threading.Thread(target=self._run, name='ThingsBoard', daemon=True)
        self.client = None
        self.signature = None
        self.retry_at = 0

    def snapshot(self):
        with self.lock:
            return dict(self.state)

    def update(self, **fields):
        with self.lock:
            self.state.update(fields)

    def config(self):
        with self.controller.lock:
            return copy.deepcopy(self.controller.config['tb'])

    def start(self):
        self.thread.start()

    def disconnect(self):
        if self.client:
            try:
                self.client.disconnect()
                self.client.loop_stop()
            except Exception:
                pass
        self.client = None

    def _connect(self, config):
        try:
            import paho.mqtt.client as mqtt
        except ImportError:
            self.update(status='missing_dependency',last_error='Chay python -m pip install -r requirements.txt')
            return False
        ready = threading.Event()
        failed = []
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,client_id='quantrac-'+uuid.uuid4().hex[:12])
        client.username_pw_set(config['token'])
        if config['tls']:
            client.tls_set()  # Dung CA he thong, khong bo qua kiem tra chung chi
        def connected(c, userdata, flags, code, properties):
            if code != 0:
                failed.append('ThingsBoard tu choi ket noi; kiem tra token/host')
            ready.set()
        client.on_connect = connected
        self.client = client
        client.connect_async(config['host'],config['port'],keepalive=30)
        client.loop_start()
        self.update(status='connecting')
        if not ready.wait(5) or failed or not client.is_connected():
            raise RuntimeError(failed[0] if failed else 'Khong ket noi duoc MQTT server')
        self.update(status='online',last_error='')
        self.store.event('INFO','TB_CONNECTED','Da ket noi MQTT ThingsBoard')
        return True

    def _run(self):
        while not self.stop_event.wait(.5):
            try:
                config = self.config()
                signature = json.dumps(config,sort_keys=True)
                if signature != self.signature:
                    self.disconnect()
                    self.signature = signature
                    self.retry_at = 0
                if not config['enabled']:
                    self.update(status='disabled',last_error='')
                    continue
                if time.monotonic() < self.retry_at:
                    continue
                if not self.client or not self.client.is_connected():
                    self.disconnect()
                    if not self._connect(config):
                        self.retry_at = time.monotonic()+10
                        continue
                # Store.pending() KHONG lay mau demo. Mo phong khong bao gio gui TB.
                for row in self.store.pending():
                    if self.stop_event.is_set() or self.config() != config:
                        break
                    deliver(self.store,row,self.client)
                    self.update(last_sent=now_ms(),status='online',last_error='')
            except Exception as error:
                message = str(error)
                previous = self.snapshot().get('last_error')
                self.update(status='offline',last_error=message)
                if message != previous:
                    try:
                        self.store.event('WARN','TB_OFFLINE',message+'; so do van duoc luu tren may')
                    except Exception:
                        pass
                self.disconnect()
                self.retry_at = time.monotonic()+10
        self.disconnect()

    def shutdown(self):
        self.stop_event.set()
        self.thread.join(timeout=8)
