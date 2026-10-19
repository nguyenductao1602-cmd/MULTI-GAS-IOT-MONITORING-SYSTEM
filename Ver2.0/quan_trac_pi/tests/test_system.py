import copy
import json
import queue
import sqlite3
import tempfile
import threading
import unittest
from datetime import datetime, timezone, timedelta
from unittest.mock import patch
from concurrent.futures import Future
from quantrac.config import validate, public_config, SENSOR_IDS
from quantrac.storage import Store, flatten, now_ms
from quantrac.controller import Controller
from quantrac.link import DemoLink, SerialLink, LinkError
from quantrac.thingsboard import deliver

class Clock:
    def __init__(self): self.value=1000.
    def __call__(self): return self.value
    def advance(self,n=.05): self.value+=n

class SystemTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.store=Store(self.tmp.name)
        self.clock=Clock()
        self.c=Controller(self.store,clock=self.clock)
    def tearDown(self):
        if self.c.link:self.c.link.close()
        self.tmp.cleanup()
    def tick(self,n=1):
        for _ in range(n): self.c.step();self.clock.advance()
    def do(self,action,**fields):
        job=self.c.enqueue(action,**fields)
        for _ in range(100):
            self.tick()
            item=self.c.job(job)
            if item['state']!='pending': return item
        self.fail('job khong ket thuc: '+action)
    def connect(self):
        self.assertEqual(self.do('connect',mode='demo')['state'],'done')
        self.assertTrue(self.c.connected)
    def configure(self,**fields):
        self.assertEqual(self.do('configure',config=fields)['state'],'done')
    def test_idle_and_manual_auto_off(self):
        self.connect();self.tick(500)
        self.assertEqual(self.store.stats()['total'],0)
        self.assertNotIn('RELAY',self.c.link.commands)
        self.do('relay',relays=[False,False,True],seconds=.5)
        self.assertEqual(self.c.relays,[False,False,True])
        self.tick(15)
        self.assertEqual(self.c.relays,[False]*3)
        self.assertEqual(self.c.phase,'idle')
    def test_program_read_before_pump_and_stop(self):
        self.connect()
        self.configure(warmup_s=0,program=[{'action':'read','target':'ALL'},{'action':'pump','seconds':.5},{'action':'wait','seconds':.5}])
        self.do('start');self.tick(14)
        cmds=self.c.link.commands
        self.assertLess(cmds.index('READ'),cmds.index('RELAY'))
        self.do('stop');self.tick(30)
        self.assertFalse(self.c.running)
        self.assertEqual(self.c.relays,[False]*3)
        self.assertEqual(self.store.stats()['total'],1)
    def test_stop_during_read_does_not_restart_program(self):
        self.connect();self.configure(warmup_s=0,program=[{'action':'read','target':'ALL'},{'action':'pump','seconds':10}])
        self.do('start');self.tick(2)
        self.assertTrue(self.c.reading)
        self.do('stop');self.tick(30)
        self.assertNotIn('RELAY',self.c.link.commands)
        self.assertEqual(self.c.relays,[False]*3)
        self.assertEqual(self.store.stats()['total'],1)
    def test_targeted_sample_preserves_other_sensor_age(self):
        self.connect();self.do('read')
        times=dict(self.c.sample_times)
        self.do('read',target='BUONG_KHI.H2S')
        self.assertEqual(self.c.latest['target'],'BUONG_KHI.H2S')
        self.assertEqual(self.c.sample_times['MOI_TRUONG.AHT20'],times['MOI_TRUONG.AHT20'])
        row=self.store.history(1)[0]['data']
        self.assertNotIn('MOI_TRUONG',row)
        self.assertEqual(set(row['BUONG_KHI']['RS485']),{'H2S'})
    def test_own_interval_without_all_sensor_reads(self):
        self.connect()
        intervals=copy.deepcopy(self.c.config['sensor_intervals'])
        intervals['BUONG_KHI.H2S']={'enabled':True,'seconds':.5}
        self.configure(warmup_s=0,program=[{'action':'wait','seconds':100}],sensor_intervals=intervals)
        self.do('start');self.tick(35)
        self.assertGreaterEqual(self.store.stats()['total'],2)
        for row in self.store.history():
            self.assertEqual(row['data']['target'],'BUONG_KHI.H2S')
    def test_timeout_stops_and_reconnect_never_resumes(self):
        self.connect();self.configure(warmup_s=0,program=[{'action':'pump','seconds':100}])
        self.do('start');self.tick(5);old=self.c.link
        self.assertTrue(old.relays[2])
        self.clock.advance(16);self.tick()
        self.assertFalse(self.c.running);self.assertEqual(old.relays,[False]*3)
        self.clock.advance(6);self.tick(5)
        self.assertTrue(self.c.connected);self.assertFalse(self.c.running)
        self.assertNotIn('RELAY',self.c.link.commands)
    def test_fixed_time_only_once_per_day(self):
        self.connect();self.configure(warmup_s=0,schedule_mode='fixed',fixed_times=['08:00'],program=[{'action':'read','target':'ALL'}])
        with patch('quantrac.controller.datetime') as date:
            date.now.return_value=datetime(2026,10,19,8,0,tzinfo=timezone(timedelta(hours=7)))
            self.do('start');self.tick(80)
            self.assertEqual(self.c.cycles,1)
            date.now.return_value=datetime(2026,10,20,8,0,tzinfo=timezone(timedelta(hours=7)))
            self.tick(30);self.assertEqual(self.c.cycles,2)
    def test_invalid_config_and_locked_relay(self):
        for patch_value in [dict(warmup_s=-1),dict(read_interval_s=0),dict(warmup_s=float('nan')),dict(fixed_times=['25:00']),dict(program=[]),dict(debug=1)]:
            with self.assertRaises(ValueError):validate(patch_value)
        self.connect();self.c.enabled=[False,False,True]
        result=self.do('relay',relays=[True,False,True],seconds=1)
        self.assertEqual(result['state'],'error');self.assertEqual(self.c.relays,[False]*3)
        self.configure(relays=[True,False,True]);self.assertEqual(self.do('start')['state'],'error')
    def test_old_firmware_blocks_targeted_reads(self):
        self.connect();self.c.status['capabilities']=[]
        self.assertEqual(self.do('read',target='BUONG_KHI.H2S')['state'],'error')
        self.assertFalse(self.c.reading)
    def test_storage_dedupe_errors_and_mqtt_puback(self):
        demo=DemoLink(clock=self.clock);packet=demo.status('READ',1);packet.update(demo._data());packet['sample_seq']=1
        self.assertTrue(self.store.sample(packet,'serial'));self.assertFalse(self.store.sample(packet,'serial'))
        self.store.sample(packet,'demo')
        self.assertEqual(len(self.store.pending()),1)
        self.assertLess(flatten(packet)['BUONG_KHI_H2S_ppm'],0)
        packet['BUONG_KHI']['RS485']['H2S'].update(ok=False,ppm=None,error='NO_RESPONSE')
        self.assertNotIn('BUONG_KHI_H2S_ppm',flatten(packet))
        self.assertFalse(flatten(packet)['BUONG_KHI_H2S_ok'])
        class Info:
            rc=0
            def wait_for_publish(self,timeout):pass
            def is_published(self):return False
        class Client:
            def publish(self,topic,payload,**kw):
                self.payload=json.loads(payload);self.topic=topic;self.kw=kw;return Info()
        client=Client();row=self.store.pending()[0]
        with self.assertRaises(TimeoutError):deliver(self.store,row,client)
        self.assertEqual(len(self.store.pending()),1)
        self.assertEqual(client.payload['ts'],row['ts']);self.assertEqual(client.kw['qos'],1)
        with patch.object(Info,'is_published',return_value=True):deliver(self.store,row,client)
        self.assertEqual(len(self.store.pending()),0);self.assertEqual(self.store.stats()['sent'],1)
        reopened=Store(self.tmp.name);self.assertEqual(reopened.stats()['total'],2)
    def test_cleanup_never_deletes_unsent_real_data(self):
        self.connect();self.do('read');packet=self.store.history(1)[0]['data'];self.store.sample(packet,'serial')
        with self.store.db() as db:db.execute('UPDATE samples SET ts=0')
        self.store.cleanup()
        self.assertEqual(self.store.stats()['total'],1)
        self.assertEqual(self.store.stats()['pending'],1)
    def test_debug_and_token_redaction(self):
        config=validate({'tb':{'token':'private-secret'}})
        public=public_config(config)
        self.assertNotIn('private-secret',json.dumps(public));self.assertTrue(public['tb']['token_set'])
        self.connect();self.tick(150)
        self.assertFalse(any(e['code'].startswith('USB_') for e in self.store.events()))
    def test_sensor_errors_stay_null_and_chart_gap(self):
        self.connect();self.c.link.fault='rs485';self.do('read')
        packet=self.c.latest
        self.assertIsNone(packet['BUONG_KHI']['RS485']['H2S']['ppm'])
        series=self.store.series(source='demo')['series']['BUONG_KHI.H2S.ppm']
        self.assertIsNone(series[-1][1])

class SerialProtocolTest(unittest.TestCase):
    def link(self):
        link=SerialLink.__new__(SerialLink)
        link.clock=Clock();link.lock=threading.RLock();link.closed=False
        link.session=0;link.boot_id=None;link.next_id=0;link.pending={};link.events=queue.SimpleQueue()
        class Port:
            def __init__(self):self.frames=[]
            def write(self,raw):self.frames.append(json.loads(raw));return len(raw)
        link.port=Port()
        return link
    def test_interleaved_read_ping_and_session_check(self):
        link=self.link();hello=link.request('HELLO')
        link._packet(dict(type='response',cmd='HELLO',id=1,session=20,boot_id=5,protocol=1,ok=True))
        self.assertTrue(hello.result()['ok'])
        read=link.request('READ',target='BUONG_KHI.H2S');ping=link.request('PING')
        link._packet(dict(type='accepted',cmd='READ',id=2,session=20,boot_id=5,ok=True));self.assertFalse(read.done())
        link._packet(dict(type='response',cmd='PING',id=3,session=21,boot_id=5,ok=True));self.assertFalse(ping.done())
        link._packet(dict(type='response',cmd='PING',id=3,session=20,boot_id=5,ok=True));self.assertTrue(ping.done())
        link._packet(dict(type='response',cmd='READ',id=2,session=20,boot_id=5,ok=True));self.assertTrue(read.done())
        self.assertEqual([x['id'] for x in link.port.frames],[1,2,3])
        self.assertEqual(link.port.frames[1]['target'],'BUONG_KHI.H2S')
    def test_timeout_and_reset_cancel_pending(self):
        link=self.link();f=link.request('HELLO');link.clock.advance(5);link.poll()
        self.assertRaises(LinkError,f.result)
        link.session=20;link.boot_id=5;f=link.request('READ')
        link._packet(dict(type='event',cmd='READY',id=0,session=0,boot_id=6))
        self.assertRaises(LinkError,f.result);self.assertEqual(link.session,0)


class HTTPTest(unittest.TestCase):
    def setUp(self):
        from app import make_server
        self.tmp=tempfile.TemporaryDirectory();self.store=Store(self.tmp.name);self.c=Controller(self.store)
        class TB:
            def snapshot(self):return {'status':'disabled','last_error':''}
        self.server=make_server(self.c,self.store,TB(),0)
        self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start();self.c.start_thread()
        self.url=f'http://127.0.0.1:{self.server.server_port}'
    def tearDown(self):
        self.server.shutdown();self.server.server_close();self.c.shutdown();self.thread.join();self.tmp.cleanup()
    def get(self,path):
        import urllib.request
        return urllib.request.urlopen(self.url+path,timeout=5)
    def action(self,body):
        import urllib.request,time
        state=json.load(self.get('/api/state'))
        req=urllib.request.Request(self.url+'/api/action',data=json.dumps(body).encode(),headers={'Content-Type':'application/json','X-App-Token':state['csrf']})
        result=json.load(urllib.request.urlopen(req));number=result['job_id']
        for _ in range(100):
            job=json.load(self.get('/api/jobs/'+str(number)))
            if job['state']!='pending':return job
            time.sleep(.02)
        self.fail('HTTP job qua han')
    def test_pages_controls_csrf_and_export(self):
        import urllib.request,urllib.error
        self.assertIn('Trạm quan trắc',self.get('/').read().decode())
        self.assertEqual(self.get('/favicon.svg').status,200)
        req=urllib.request.Request(self.url+'/api/action',data=b'{"action":"start"}',headers={'Content-Type':'application/json'})
        with self.assertRaises(urllib.error.HTTPError) as failed:urllib.request.urlopen(req)
        self.assertEqual(failed.exception.code,403);failed.exception.close()
        self.assertEqual(self.action({'action':'connect','mode':'demo'})['state'],'done')
        self.assertEqual(self.action({'action':'read'})['state'],'done')
        self.assertEqual(len(json.load(self.get('/api/series?source=demo'))['series']),15)
        csv_text=self.get('/api/export.csv?source=demo').read().decode('utf-8-sig')
        self.assertIn('BUONG_KHI_H2S_ppm',csv_text);self.assertIn('demo',csv_text)
        self.assertEqual(len(self.get('/api/export.csv?source=serial').read().splitlines()),1)
        req=urllib.request.Request(self.url+'/api/state',headers={'Host':'evil.example'})
        with self.assertRaises(urllib.error.HTTPError) as failed:urllib.request.urlopen(req)
        self.assertEqual(failed.exception.code,403);failed.exception.close()

if __name__=='__main__':unittest.main()
