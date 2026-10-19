"""Mot vong dieu khien so huu lich va heartbeat. Khong sleep cho het chu ky bom."""
import copy
import math
import queue
import threading
import time
from .config import validate, public_config, SENSOR_IDS
from datetime import datetime
from .link import SerialLink, DemoLink, LinkError
from .storage import now_ms

class Controller:
    def __init__(self, store, clock=time.monotonic, serial_factory=SerialLink):
        self.store, self.clock, self.serial_factory = store, clock, serial_factory
        self.config = validate(store.load_config() or {})
        self.lock = threading.RLock()
        self.actions = queue.PriorityQueue()
        self.jobs = {}
        self.job_seq = 0
        self.link = None
        self.pending = []
        self.want_connection = False
        self.connect_due = 0
        self.hello_due = None
        self.connect_job = None
        self.next_ping = 0
        self.mode = 'demo'
        self.connected = self.connecting = self.running = self.reading = False
        self.relays = [None] * 3
        self.enabled = [False] * 3
        self.phase = 'idle'
        self.deadline = None
        self.next_read = 0
        self.next_sensor = {k:0 for k in SENSOR_IDS}
        self.sample_times = {}
        self.step_index = None
        self.fixed_seen = set()
        self.generation = 0
        self.latest = None
        self.latest_ts = None
        self.status = {}
        self.sensor_health = {}
        self.last_error = ''
        self.last_progress = clock()
        self.cycles = 0
        self.stop_event = threading.Event()
        self.thread = None
        self.next_cleanup = clock() + 3600

    def log(self, level, code, message):
        self.store.event(level, code, message)

    def start_thread(self):
        self.thread = threading.Thread(target=self._run, name='Dieu-khien-va-heartbeat', daemon=True)
        self.thread.start()

    def enqueue(self, action, **fields):
        if action not in ('connect','disconnect','start','stop','read','relay','configure','simulate'):
            raise ValueError('Lenh giao dien khong hop le')
        with self.lock:
            if self.actions.qsize() >= 30:
                raise ValueError('Dang co nhieu lenh cho, vui long doi')
            self.job_seq += 1
            number = self.job_seq
            self.jobs[number] = {'id': number, 'action': action, 'state': 'pending'}
            while len(self.jobs) > 300:
                del self.jobs[next(iter(self.jobs))]
            self.actions.put((0 if action in ('stop','disconnect') else 10, number, action, fields))
        return number

    def job(self, number):
        with self.lock:
            return copy.deepcopy(self.jobs.get(number))

    def done(self, number, error=None):
        if number and number in self.jobs:
            self.jobs[number].update(state='error' if error else 'done', error=str(error) if error else None)

    def snapshot(self):
        with self.lock:
            return dict(connected=self.connected, connecting=self.connecting or (self.want_connection and not self.connected), mode=self.mode,
                        running=self.running, reading=self.reading, phase=self.phase,
                        countdown_s=max(0, self.deadline-self.clock()) if self.deadline is not None else None,
                        relays=list(self.relays), relay_enabled=list(self.enabled),
                        config=public_config(self.config), status=copy.deepcopy(self.status),
                        latest=copy.deepcopy(self.latest), latest_ts=self.latest_ts,
                        last_error=self.last_error, cycles=self.cycles,
                        controller_age_s=max(0,self.clock()-self.last_progress),
                        step_index=self.step_index,sample_times=dict(self.sample_times))

    def _run(self):
        while not self.stop_event.is_set():
            self.step()
            self.stop_event.wait(.05)

    def step(self):
        with self.lock:
            try:
                self.last_progress = self.clock()
                for _ in range(5):
                    try:
                        _, number, action, fields = self.actions.get_nowait()
                    except queue.Empty:
                        break
                    try:
                        self._action(number, action, fields)
                    except (ValueError, LinkError) as error:
                        self.done(number,error)
                        self.last_error = str(error)
                if self.link:
                    self.link.poll()
                    for _ in range(50):
                        try:
                            kind, value = self.link.events.get_nowait()
                        except queue.Empty:
                            break
                        if kind == 'fault':
                            self._fault(value)
                            break
                        if kind == 'packet':
                            self._status(value)
                self._poll_pending()
                if self.link and self.hello_due is not None and self.clock() >= self.hello_due:
                    self.hello_due = None
                    self._send('HELLO',self._hello,job=self.connect_job)
                    self.connect_job = None
                if not self.link and self.want_connection and self.clock() >= self.connect_due:
                    self._open()
                if self.connected and self.clock() >= self.next_ping:
                    self.next_ping = self.clock()+3
                    self._send('PING')
                if self.connected:
                    self._schedule()
                if self.clock() >= self.next_cleanup:
                    self.store.cleanup()
                    self.next_cleanup = self.clock()+3600
            except Exception as error:
                # Loi o controller/luu tru khong duoc de heartbeat gia han bom trong nen.
                self._fault(f'Loi dieu khien/luu tru: {error}')

    def _status(self, packet):
        if 'relays' in packet and isinstance(packet['relays'],list) and len(packet['relays']) == 3:
            self.relays = [v if type(v) is bool else None for v in packet['relays']]
        if 'relay_enabled' in packet:
            flags = packet['relay_enabled']
            if isinstance(flags,list) and len(flags)==3 and all(type(v) is bool for v in flags):
                self.enabled = flags
        for key in ('boot_id','session','uptime_ms','watchdog_ok','lcd_ok','bus1_ok','bus2_ok','uart1_ok','reset_reason','dropped_frames','capabilities'):
            if key in packet:
                self.status[key] = packet[key]

    def _send(self, command, callback=None, job=None, generation=None, **fields):
        if not self.link:
            raise LinkError('Chua ket noi ESP')
        if self.config['debug']:
            self.log('DEBUG','USB_TX',f'{command} {fields}')
        future = self.link.request(command,timeout=20 if command=='READ' else 4,**fields)
        self.pending.append((future,command,callback,job,generation))

    def _poll_pending(self):
        for item in list(self.pending):
            future, command, callback, job, generation = item
            if not future.done():
                continue
            self.pending.remove(item)
            try:
                packet = future.result()
                if packet.get('ok') is not True:
                    raise LinkError(f'{command}: {packet.get("error","INVALID_RESPONSE")}')
                if callback and (generation is None or generation==self.generation):
                    callback(packet)
                if self.config['debug']:
                    self.log('DEBUG','USB_RX',f'{command} id={packet.get("id")} ok={packet.get("ok")}')
                self.done(job)
            except Exception as error:
                self.done(job,error)
                self._fault(str(error))
                break

    def _open(self):
        try:
            self.link = (DemoLink if self.mode=='demo' else self.serial_factory)(self.config['port'],clock=self.clock)
            self.connecting = True
            self.connected = False
            self.hello_due = self.clock() + (0 if self.mode=='demo' else 1.5)
        except Exception as error:
            self.link = None
            self.connecting = False
            self.connect_due = self.clock()+5
            message = f'Khong mo duoc {self.config["port"]}: {error}'
            if message != self.last_error:
                self.log('ERROR','USB_OPEN_FAILED',message)
            self.last_error = message
            self.done(self.connect_job,message)
            self.connect_job = None

    def _hello(self, packet):
        self._status(packet)
        self.connected, self.connecting = True, False
        self.running = False
        self.phase = 'idle'
        self.deadline = None
        self.next_ping = self.clock()+3
        self.last_error = ''
        self.log('INFO','CONNECTED',f'{self.mode}: boot_id={packet.get("boot_id")}, reset_reason={packet.get("reset_reason")}; bom tat, cho lenh bat dau')

    def _fault(self, message):
        self.generation += 1
        self.step_index = None
        self.running = self.reading = self.connected = self.connecting = False
        self.phase, self.deadline = 'fault', None
        self.last_error = str(message)
        link, self.link = self.link, None
        if link:
            try:
                if link.session:
                    link.request('STOP')  # Best effort; khi khong ACK thi khong tu nhan bom da tat
            except Exception:
                pass
            try:
                link.close()
            except Exception:
                pass
        self.relays = [None]*3
        for _,_,_,job,_ in self.pending:
            self.done(job,message)
        self.pending.clear()
        self.done(self.connect_job,message)
        self.connect_job = None
        self.hello_due = None
        self.connect_due = self.clock()+5
        try:
            self.log('ERROR','LINK_OR_CONTROLLER_ERROR',str(message))
        except Exception:
            pass

    def _stop(self, job=None, disconnect=False):
        self.generation += 1
        self.step_index = None
        self.running = False
        self.deadline = None
        # Lenh dang cho truoc STOP khong duoc bat bom lai ngay sau STOP.
        while True:
            try:
                _, number, _, _ = self.actions.get_nowait()
                self.done(number,'Da huy boi lenh Dung')
            except queue.Empty:
                break
        if disconnect:
            self.want_connection = False
        if not self.connected:
            self.want_connection = False
            if self.link:
                self.link.close()
                self.link = None
            self.connecting = False
            self.hello_due = None
            for _,_,_,pending_job,_ in self.pending: self.done(pending_job,'Da huy ket noi')
            self.pending.clear()
            self.done(self.connect_job,'Da huy ket noi')
            self.connect_job = None
            self.phase = 'idle'
            self.done(job)
            return
        self.phase = 'stopping'
        def stopped(packet):
            self._status(packet)
            self.phase = 'idle'
            self.log('INFO','STOP','Da nhan ACK tat tat ca relay')
            if disconnect:
                self.link.close()
                self.link = None
                self.connected = False
        self._send('STOP',stopped,job=job,generation=self.generation)

    def _action(self, number, action, fields):
        if action == 'connect':
            if self.link or self.want_connection:
                raise ValueError('Hay ngat ket noi hien tai truoc')
            mode = fields.get('mode','demo')
            if mode not in ('demo','serial'):
                raise ValueError('Che do ket noi khong hop le')
            if 'port' in fields:
                self.config = validate({'port':fields['port']},self.config)
                self.store.save_config(self.config)
            self.mode = mode
            self.latest = self.latest_ts = None
            self.sensor_health.clear()
            self.sample_times.clear()
            self.status = {}
            self.want_connection = True
            self.connect_job = number
            self._open()
            return
        if action in ('stop','disconnect'):
            self._stop(number,action=='disconnect')
            return
        if action == 'configure':
            if self.running or self.reading or any(v is True for v in self.relays):
                raise ValueError('Dung bom va cho doc xong truoc khi luu cau hinh')
            updated = validate(fields.get('config'),self.config)
            self.store.save_config(updated)
            self.config = updated
            self.log('INFO','CONFIG_SAVED','Da luu cau hinh; khong tu chay lai')
            self.done(number)
            return
        if not self.connected:
            raise ValueError('Hay ket noi ESP hoac che do mo phong truoc')
        if action == 'simulate':
            if self.mode != 'demo':
                raise ValueError('Chi thu loi trong che do mo phong')
            fault = fields.get('fault')
            if fault not in ('none','i2c_chamber','rs485','reset'):
                raise ValueError('Loai loi mo phong khong hop le')
            if fault == 'reset':
                self._fault('Mo phong: ESP reset')
            else:
                self.link.fault = fault
                self.log('INFO','SIMULATION_FAULT',f'Mo phong: {fault}; bam Doc mot lan de xem')
            self.done(number)
            return
        if action == 'start':
            if self.running or self.reading or self.phase != 'idle' or any(v is True for v in self.relays):
                raise ValueError('Dung thao tac hien tai truoc khi bat dau')
            uses_pump = any(step['action']=='pump' and step['seconds']>0 for step in self.config['program'])
            if uses_pump and any(v and not self.enabled[i] for i,v in enumerate(self.config['relays'])):
                raise ValueError('Relay dang bi khoa boi SW tren ESP')
            if uses_pump and not any(self.config['relays']):
                raise ValueError('Chon it nhat mot bom cho buoc Chay bom')
            targeted = any(step['action']=='read' and step['target']!='ALL' for step in self.config['program']) or any(v['enabled'] for v in self.config['sensor_intervals'].values())
            if targeted and 'read_sensor' not in self.status.get('capabilities',[]):
                raise ValueError('Can nap ESP firmware moi co read_sensor de doc tung cam bien')
            self.generation += 1
            self.running = True
            self.step_index = None
            self.phase = 'warmup'
            self.deadline = self.clock()+self.config['warmup_s']
            self.cycles = 0
            self.last_error = ''
            self.log('INFO','RUN_STARTED','Bat dau lich; cac bom duoc chon bat dong thoi')
            self.done(number)
            return
        if action == 'read':
            if self.running:
                raise ValueError('Dung che do tu dong truoc khi doc thu cong')
            self._read(number,target=fields.get('target','ALL'))
            return
        if action == 'relay':
            if self.running or self.reading or self.phase not in ('idle','manual'):
                raise ValueError('Dung chu ky/cho doc xong truoc khi thu relay')
            flags = fields.get('relays')
            seconds = fields.get('seconds',5)
            if not isinstance(flags,list) or len(flags)!=3 or any(type(v) is not bool for v in flags):
                raise ValueError('Can ba gia tri true/false cho relay')
            if type(seconds) not in (int,float) or not math.isfinite(seconds) or not .5<=seconds<=600:
                raise ValueError('Thoi gian thu relay: 0.5..600 giay')
            if any(v and not self.enabled[i] for i,v in enumerate(flags)):
                raise ValueError('Relay duoc chon dang bi khoa boi SW tren ESP')
            generation = self.generation
            def applied(packet):
                self._status(packet)
                self.phase = 'manual' if any(flags) else 'idle'
                self.deadline = self.clock()+seconds if any(flags) else None
                self.log('INFO','MANUAL_RELAY',f'Thu relay {flags} trong {seconds}s')
            self._send('RELAY',applied,job=number,generation=generation,**{f'sw{i+1}':v for i,v in enumerate(flags)})
            return
        raise ValueError('Lenh chua duoc ho tro')


    def _read(self, job=None, target='ALL', advance=False):
        if target not in ['ALL']+SENSOR_IDS:
            raise ValueError('Cam bien duoc chon khong hop le')
        if target!='ALL' and 'read_sensor' not in self.status.get('capabilities',[]):
            raise ValueError('Firmware ESP hien tai chi ho tro doc tat ca; hay nap ban moi')
        if self.reading:
            raise ValueError('Dang doc cam bien, vui long doi')
        generation = self.generation
        self.reading = True
        def received(packet):
            self.reading = False
            present = {}
            errors = []
            for group in ('MOI_TRUONG','BUONG_KHI'):
                if group not in packet:
                    continue
                if not isinstance(packet[group],dict):
                    raise ValueError('Du lieu nhom cam bien khong hop le')
                for name,item in packet[group].items():
                    entries = item.items() if name=='RS485' and isinstance(item,dict) else [(name,item)]
                    for sensor,data in entries:
                        key=f'{group}.{sensor}'
                        if key not in SENSOR_IDS or not isinstance(data,dict) or type(data.get('ok')) is not bool:
                            raise ValueError('Du lieu cam bien khong hop le')
                        present[key]=data
                        error='OK' if data['ok'] else str(data.get('error','UNKNOWN'))
                        if error!='OK': errors.append(key)
                        if self.sensor_health.get(key)!=error:
                            self.log('INFO' if error=='OK' else 'WARN','SENSOR_STATE',f'{key}: {error}')
                            self.sensor_health[key]=error
            if not present or (target!='ALL' and set(present)!={target}):
                raise ValueError('ESP khong tra dung cam bien da yeu cau')
            self.store.sample(packet,self.mode)
            stamp=now_ms()
            if self.latest is None: self.latest={}
            for key,value in packet.items():
                if key not in ('MOI_TRUONG','BUONG_KHI'): self.latest[key]=value
            for group in ('MOI_TRUONG','BUONG_KHI'):
                for name,data in packet.get(group,{}).items():
                    section=self.latest.setdefault(group,{})
                    if name=='RS485': section.setdefault(name,{}).update(data)
                    else: section[name]=data
            self.latest_ts=stamp
            for key in present: self.sample_times[key]=stamp
            if self.config['debug']:
                self.log('DEBUG','SAMPLE_SAVED',f'Mau #{packet["sample_seq"]}; target={target}; {len(errors)} loi')
            if self.running and generation==self.generation:
                if errors and self.config['stop_on_sensor_error']:
                    self._stop()
                    self.last_error='Dung do loi cam bien: '+', '.join(errors)
                elif advance:
                    self._advance()
        self._send('READ',received,job=job,**({'target':target} if target!='ALL' else {}))
        now=self.clock()
        if target=='ALL':
            self.next_read=now+self.config['read_interval_s']
            for key,value in self.config['sensor_intervals'].items(): self.next_sensor[key]=now+value['seconds']
        else:
            self.next_sensor[target]=now+self.config['sensor_intervals'][target]['seconds']

    def _begin_program(self):
        self.cycles+=1
        self.step_index=0
        self.phase='program_next'
        self.deadline=None
        self.log('INFO','PROGRAM_START',f'Bat dau chu ky {self.cycles}')

    def _advance(self):
        self.step_index+=1
        self.deadline=None
        if self.step_index<len(self.config['program']):
            self.phase='program_next'
            return
        self.phase='program_finishing'
        def finished(packet):
            self._status(packet)
            self.step_index=None
            self.phase='between' if self.config['schedule_mode']=='cycle' else 'fixed_wait'
            self.deadline=self.clock()+.5 if self.phase=='between' else None
            self.log('INFO','PROGRAM_DONE',f'Ket thuc chu ky {self.cycles}; bom tat')
        self._send('STOP',finished,generation=self.generation)

    def _execute_step(self):
        step=self.config['program'][self.step_index]
        generation=self.generation
        if step['action']=='read':
            if self.reading: return
            self.phase='reading'
            self._read(target=step['target'],advance=True)
        elif step['action']=='wait':
            self.phase='wait'
            self.deadline=self.clock()+step['seconds']
        elif step['action']=='pump':
            if step['seconds']==0:
                self._advance()
                return
            self.phase='applying'
            def pumping(packet):
                self._status(packet)
                self.phase='pump'
                self.deadline=self.clock()+step['seconds']
                self.log('INFO','PUMP_ON',f'Buoc {self.step_index+1}: {self.config["relays"]}')
            self._send('RELAY',pumping,generation=generation,**{f'sw{i+1}':v for i,v in enumerate(self.config['relays'])})

    def _schedule(self):
        now=self.clock()
        if self.phase=='manual' and self.deadline is not None and now>=self.deadline:
            self._stop()
            return
        if not self.running: return
        if self.phase=='warmup':
            if now<self.deadline: return
            self.next_read=now
            self.next_sensor={k:now for k in SENSOR_IDS}
            if self.config['schedule_mode']=='fixed':
                self.phase='fixed_wait'
                self.deadline=None
            else:
                self._begin_program()
        if self.phase=='fixed_wait':
            local=datetime.now().astimezone()
            day=local.strftime('%Y-%m-%d')
            key=local.strftime('%Y-%m-%d %H:%M')
            self.fixed_seen={k for k in self.fixed_seen if k.startswith(day)}
            if local.strftime('%H:%M') in self.config['fixed_times'] and key not in self.fixed_seen:
                self.fixed_seen.add(key)
                self._begin_program()
        elif self.phase=='between' and now>=self.deadline:
            self._begin_program()
        elif self.phase=='wait' and now>=self.deadline:
            self._advance()
        elif self.phase=='pump' and now>=self.deadline:
            self.phase='applying_stop'
            self.deadline=None
            def off(packet):
                self._status(packet)
                self.log('INFO','PUMP_OFF','Da tat bom, chuyen buoc tiep theo')
                self._advance()
            self._send('STOP',off,generation=self.generation)
        if self.phase=='program_next': self._execute_step()
        if self.phase in ('warmup','applying','applying_stop','program_finishing') or self.reading:
            return
        if self.config['read_mode']=='interval' and now>=self.next_read:
            self._read()
            return
        due=[(self.next_sensor[key],key) for key,value in self.config['sensor_intervals'].items() if value['enabled'] and now>=self.next_sensor[key]]
        if due:
            self._read(target=min(due)[1])

    def shutdown(self):
        self.want_connection=False
        self.enqueue('stop')
        end=time.monotonic()+2
        while self.connected and time.monotonic()<end:
            if not self.thread:
                self.step()
            if self.phase=='idle' and self.relays==[False]*3:
                break
            time.sleep(.05)
        self.stop_event.set()
        if self.thread:
            self.thread.join(timeout=3)
        if self.link:
            self.link.close()
            self.link=None
