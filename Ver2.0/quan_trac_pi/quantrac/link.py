import json
import math
import queue
import random
import threading
import time
from concurrent.futures import Future

class LinkError(RuntimeError):
    pass

def list_ports():
    try:
        from serial.tools import list_ports as ports
        return [{'port': p.device, 'description': p.description} for p in ports.comports()]
    except ImportError:
        return []

class SerialLink:
    def __init__(self, port, clock=time.monotonic):
        try:
            import serial
        except ImportError:
            raise LinkError('Chua cai pyserial. Chay python -m pip install -r requirements.txt') from None
        self.clock = clock
        self.port = serial.Serial(port, 115200, timeout=.1, write_timeout=.5)
        self.session = 0
        self.boot_id = None
        self.next_id = 0
        self.pending = {}
        self.lock = threading.RLock()
        self.events = queue.SimpleQueue()
        self.closed = False
        self.reader = threading.Thread(target=self._receive, name='USB-RX', daemon=True)
        self.reader.start()
    def request(self, command, timeout=3, **fields):
        with self.lock:
            if self.closed:
                raise LinkError('Cong USB da dong')
            if command != 'HELLO' and not self.session:
                raise LinkError('Can HELLO lai voi ESP')
            self.next_id += 1
            if self.next_id > 0xFFFFFFFF:
                raise LinkError('Can ket noi lai de tao session moi')
            frame = dict(id=self.next_id, cmd=command, **fields)
            if command != 'HELLO':
                frame['session'] = self.session
            future = Future()
            self.pending[self.next_id] = (future, self.clock() + timeout, command)
            try:
                raw = (json.dumps(frame, separators=(',', ':'), allow_nan=False) + '\n').encode()
                if len(raw) > 385 or self.port.write(raw) != len(raw):
                    raise LinkError('Khong gui duoc tron lenh USB')
            except Exception as error:
                self.pending.pop(self.next_id, None)
                future.set_exception(LinkError(str(error)))
            return future
    def _receive(self):
        buffer = bytearray()
        try:
            while not self.closed:
                chunk = self.port.read(4096)
                if not chunk:
                    continue
                buffer.extend(chunk)
                while b'\n' in buffer:
                    line, _, buffer = buffer.partition(b'\n')
                    try:
                        packet = json.loads(line, parse_constant=lambda _: (_ for _ in ()).throw(ValueError()))
                    except (ValueError, UnicodeDecodeError):
                        continue
                    self._packet(packet)
                if len(buffer) > 65536:
                    raise LinkError('Dong USB qua dai/khong co ky tu xuong dong')
        except Exception as error:
            if not self.closed:
                self.events.put(('fault', f'Mat USB: {error}'))
                self._cancel(str(error))
    def _packet(self, packet):
        if not isinstance(packet, dict) or packet.get('type') not in ('response', 'event', 'accepted'):
            return
        with self.lock:
            item = self.pending.get(packet.get('id'))
            hello = item and item[2] == 'HELLO'
            if self.boot_id is not None and packet.get('boot_id') != self.boot_id and not hello:
                self.events.put(('fault', 'ESP vua khoi dong lai / boot_id thay doi'))
                self.session = 0
                self._cancel('ESP da reset')
                return
            if packet.get('cmd') == 'PI_TIMEOUT':
                self.events.put(('fault', 'ESP bao het thoi han heartbeat, da tat bom'))
                self.session = 0
                self._cancel('Session da het han')
                return
            if packet.get('type') == 'response' and item:
                if packet.get('cmd') != item[2]:
                    return
                if not hello and packet.get('session') != self.session:
                    return
                if hello and packet.get('ok'):
                    if type(packet.get('session')) is not int or not packet['session'] or packet.get('protocol') != 1:
                        item[0].set_exception(LinkError('ESP khong dung protocol 1'))
                        del self.pending[packet['id']]
                        return
                    self.session = packet['session']
                    self.boot_id = packet.get('boot_id')
                del self.pending[packet['id']]
                item[0].set_result(packet)
            self.events.put(('packet', packet))
    def poll(self):
        with self.lock:
            for number, (future, deadline, command) in list(self.pending.items()):
                if self.clock() >= deadline:
                    del self.pending[number]
                    future.set_exception(LinkError(f'{command}: ESP khong phan hoi dung han'))
    def _cancel(self, message):
        with self.lock:
            for future, _, _ in self.pending.values():
                if not future.done():
                    future.set_exception(LinkError(message))
            self.pending.clear()
    def close(self):
        self.closed = True
        self._cancel('Da dong ket noi')
        self.port.close()
        if threading.current_thread() is not self.reader:
            self.reader.join(timeout=1)

class DemoLink:
    def __init__(self, port='', clock=time.monotonic):
        self.clock = clock
        self.started = clock()
        self.boot_id = random.randrange(1, 0xFFFFFFFF)
        self.session = 0
        self.events = queue.SimpleQueue()
        self.relays = [False, False, False]
        self.enabled = [True, True, True]
        self.closed = False
        self.sequence = 0
        self.next_id = 0
        self.last_contact = clock()
        self.jobs = []
        self.fault = 'none'
        self.commands = []
        self.read_targets = {}
    def status(self, command, number):
        return dict(type='response', id=number, cmd=command, protocol=1, ok=True,
                    session=self.session, boot_id=self.boot_id,
                    uptime_ms=int((self.clock() - self.started) * 1000) & 0xFFFFFFFF,
                    relays=list(self.relays), relay_enabled=self.enabled,
                    watchdog_ok=True, lcd_ok=True, bus1_ok=True, bus2_ok=True,
                    uart1_ok=True, dropped_frames=0, reset_reason=1, pi_timeout_ms=15000, capabilities=['read_sensor'])
    def request(self, command, timeout=3, **fields):
        future = Future()
        if self.closed:
            future.set_exception(LinkError('Mo phong da dong'))
            return future
        if command != 'HELLO' and not self.session:
            future.set_exception(LinkError('Can HELLO'))
            return future
        self.next_id += 1
        self.commands.append(command)
        if command == 'HELLO':
            self.session = random.randrange(1, 0xFFFFFFFF)
            self.relays = [False] * 3
        elif command == 'STOP':
            self.relays = [False] * 3
        elif command == 'RELAY':
            self.relays = [fields[f'sw{i}'] for i in (1, 2, 3)]
        self.last_contact = self.clock()
        packet = self.status(command, self.next_id)
        due = self.clock()
        if command == 'READ':
            if any(j[2].get('cmd') == 'READ' for j in self.jobs):
                packet.update(ok=False, error='BUSY')
            else:
                due += .35
        if command=='READ':
            self.read_targets[packet['id']]=fields.get('target','ALL')
        self.jobs.append((due, future, packet))
        return future
    def poll(self):
        if self.session and self.clock() - self.last_contact >= 15:
            self.relays = [False] * 3
            self.events.put(('fault', 'Mo phong: ESP da tat bom vi mat heartbeat'))
            self.session = 0
        waiting = []
        for due, future, packet in self.jobs:
            if future.done():
                continue
            if self.clock() < due:
                waiting.append((due, future, packet))
                continue
            if packet['cmd'] == 'READ' and packet['ok']:
                self.sequence += 1
                packet = self.status('READ', packet['id'])
                packet['sample_seq'] = self.sequence
                packet['read_start_ms'] = packet['uptime_ms'] - 350
                packet['read_end_ms'] = packet['uptime_ms']
                packet['relay_at_start'] = list(self.relays)
                values=self._data()
                target=self.read_targets.pop(packet['id'],'ALL')
                packet['target']=target
                if target!='ALL':
                    group,name=target.split('.')
                    if name in ('CH3SH','CO','H2S','NH3'):
                        values={group:{'RS485':{name:values[group]['RS485'][name]}}}
                    else:
                        values={group:{name:values[group][name]}}
                packet.update(values)
            future.set_result(packet)
            self.events.put(('packet', packet))
        self.jobs = waiting
    def _data(self):
        wave = math.sin(self.sequence / 4)
        def sensor(**fields):
            return dict(ok=True, error='OK', **fields)
        def region(chamber=False):
            value = dict(AHT20=sensor(temperature_c=31.4 + wave * .3, humidity_rh=82.2 + wave),
                         BMP280=sensor(temperature_c=31.8 + wave * .2, pressure_hpa=1010 + wave))
            if chamber:
                value['SCD30'] = sensor(temperature_c=31.3, humidity_rh=83.1, co2_ppm=520 + 26 * wave)
            return value
        outside, inside = region(), region(True)
        inside['RS485'] = {name: dict(sensor(ppm=value), address=i, gas_type=code)
                          for i, (name, code, value) in enumerate([
                              ('CH3SH', 41, .02 + .01 * wave), ('CO', 2, .13 + .02 * wave),
                              ('H2S', 9, -.71 + .02 * wave), ('NH3', 11, .2 + .03 * wave)], 1)}
        if self.fault == 'i2c_chamber':
            for name in ('AHT20', 'BMP280', 'SCD30'):
                inside[name] = {k: (False if k == 'ok' else 'I2C_TIMEOUT' if k == 'error' else None) for k in inside[name]}
        if self.fault == 'rs485':
            for value in inside['RS485'].values():
                value.update(ok=False, error='NO_RESPONSE', ppm=None)
        return dict(MOI_TRUONG=outside, BUONG_KHI=inside)
    def close(self):
        self.relays = [False] * 3
        self.closed = True
        for _, future, _ in self.jobs:
            if not future.done():
                future.set_exception(LinkError('Dong mo phong'))
        self.jobs.clear()
