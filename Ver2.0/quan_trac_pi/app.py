"""Giao dien quan trac da khi. Chay python app.py --open tren Windows hoac Ubuntu."""
import argparse
import csv
import hmac
import io
import json
import secrets
import threading
import urllib.parse
import webbrowser
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from quantrac.controller import Controller
from quantrac.link import list_ports
from quantrac.storage import Store, now_ms
from quantrac.thingsboard import ThingsBoard

ROOT = Path(__file__).resolve().parent
SENSORS = [('MOI_TRUONG','AHT20',['temperature_c','humidity_rh']),
           ('MOI_TRUONG','BMP280',['temperature_c','pressure_hpa']),
           ('BUONG_KHI','AHT20',['temperature_c','humidity_rh']),
           ('BUONG_KHI','BMP280',['temperature_c','pressure_hpa']),
           ('BUONG_KHI','SCD30',['temperature_c','humidity_rh','co2_ppm'])]
CSV_KEYS = [f'{g}_{s}_{k}' for g,s,keys in SENSORS for k in keys+['ok','error']]
CSV_KEYS += [f'BUONG_KHI_{name}_{k}' for name in ('CH3SH','CO','H2S','NH3') for k in ('ppm','ok','error')]
CSV_KEYS += ['RELAY1','RELAY2','RELAY3','boot_id','sample_seq']


def make_server(controller, store, thingsboard, port=8765):
    csrf = secrets.token_urlsafe(32)
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass
        def respond_headers(self, code, content_type, length=None):
            self.send_response(code)
            self.send_header('Content-Type',content_type)
            self.send_header('Cache-Control','no-store')
            self.send_header('X-Content-Type-Options','nosniff')
            self.send_header('Content-Security-Policy',"default-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; frame-ancestors 'none'; object-src 'none'")
            if length is not None:
                self.send_header('Content-Length',str(length))
            self.end_headers()
        def send_json(self, data, code=200):
            raw = json.dumps(data,ensure_ascii=False,allow_nan=False).encode('utf-8')
            self.respond_headers(code,'application/json; charset=utf-8',len(raw))
            self.wfile.write(raw)
        def valid_host(self):
            try:
                host=urllib.parse.urlsplit('http://'+self.headers.get('Host','')).hostname
                return host in ('localhost','127.0.0.1')
            except ValueError:
                return False
        def do_GET(self):
            if not self.valid_host():
                self.send_json({'error':'Host khong hop le'},403)
                return
            try:
                path = urllib.parse.urlsplit(self.path)
                params = urllib.parse.parse_qs(path.query)
                if path.path == '/api/state':
                    self.send_json(dict(controller.snapshot(),stats=store.stats(),thingsboard=thingsboard.snapshot(),csrf=csrf,server_time=now_ms(),server_clock=datetime.now().astimezone().strftime('%H:%M:%S  %d/%m/%Y  %z')))
                elif path.path == '/api/ports':
                    self.send_json(list_ports())
                elif path.path == '/api/history':
                    self.send_json(store.history(int(params.get('limit',['120'])[0]),params.get('source',['all'])[0]))
                elif path.path == '/api/series':
                    self.send_json(store.series(float(params.get('hours',['1'])[0]),params.get('source',['demo'])[0]))
                elif path.path == '/api/events':
                    self.send_json(store.events(int(params.get('limit',['100'])[0])))
                elif path.path.startswith('/api/jobs/'):
                    job = controller.job(int(path.path.rsplit('/',1)[1]))
                    self.send_json(job if job else {'error':'Khong tim thay lenh'},200 if job else 404)
                elif path.path == '/api/export.csv':
                    self._export(params.get('source',['all'])[0])
                elif path.path in ('/','/index.html','/app.js','/style.css','/favicon.svg'):
                    name = 'index.html' if path.path=='/' else path.path[1:]
                    raw = (ROOT/'web'/name).read_bytes()
                    mime = 'image/svg+xml' if name.endswith('.svg') else 'text/html' if name.endswith('.html') else 'text/css' if name.endswith('.css') else 'text/javascript'
                    self.respond_headers(200,mime+'; charset=utf-8',len(raw))
                    self.wfile.write(raw)
                else:
                    self.send_json({'error':'Khong tim thay'},404)
            except (BrokenPipeError, ConnectionResetError):
                pass
            except (ValueError,TypeError):
                self.send_json({'error':'Tham so khong hop le'},400)
            except Exception:
                self.send_json({'error':'Khong doc duoc du lieu luu tru'},500)
        def do_POST(self):
            if not self.valid_host():
                self.send_json({'error':'Host khong hop le'},403)
                return
            origin = self.headers.get('Origin')
            if origin and origin != 'http://'+self.headers.get('Host',''):
                self.send_json({'error':'Origin khong hop le'},403)
                return
            if not hmac.compare_digest(self.headers.get('X-App-Token',''),csrf):
                self.send_json({'error':'Vui long tai lai giao dien'},403)
                return
            try:
                length = int(self.headers.get('Content-Length','0'))
                if not 0 < length <= 16384:
                    raise ValueError('Noi dung lenh qua dai hoac rong')
                body = json.loads(self.rfile.read(length),parse_constant=lambda _: (_ for _ in ()).throw(ValueError('So khong hop le')))
                if not isinstance(body,dict):
                    raise ValueError('Can doi tuong JSON')
                if self.path != '/api/action':
                    self.send_json({'error':'Khong tim thay'},404)
                    return
                action = body.pop('action',None)
                number = controller.enqueue(action,**body)
                self.send_json({'job_id':number},202)
            except (ValueError,TypeError,UnicodeDecodeError) as error:
                self.send_json({'error':str(error)},400)
        def _export(self, source):
            self.send_response(200)
            self.send_header('Content-Type','text/csv; charset=utf-8')
            self.send_header('Content-Disposition','attachment; filename="quan-trac.csv"')
            self.end_headers()
            self.wfile.write(b'\xef\xbb\xbf')
            stream = io.StringIO()
            writer = csv.writer(stream)
            def line(values):
                writer.writerow(["'"+v if isinstance(v,str) and v.startswith(('=','+','-','@')) else v for v in values])
                self.wfile.write(stream.getvalue().encode('utf-8'))
                stream.seek(0); stream.truncate(0)
            line(['time_utc','source']+CSV_KEYS)
            for ts, mode, values in store.export_rows(source):
                stamp = datetime.fromtimestamp(ts/1000,timezone.utc).isoformat()
                line([stamp,mode]+[values.get(k,'') for k in CSV_KEYS])
    server = ThreadingHTTPServer(('127.0.0.1',port),Handler)
    server.daemon_threads = True
    return server


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',type=int,default=8765)
    parser.add_argument('--data-dir',type=Path,default=ROOT/'data')
    parser.add_argument('--demo',action='store_true',help='Ket noi mo phong, khong tu bat bom/doc')
    parser.add_argument('--open',action='store_true',help='Mo trinh duyet')
    args = parser.parse_args()
    store = Store(args.data_dir)
    controller = Controller(store)
    uploader = ThingsBoard(store,controller)
    server = make_server(controller,store,uploader,args.port)
    controller.start_thread()
    uploader.start()
    if args.demo:
        controller.enqueue('connect',mode='demo')
    address = f'http://127.0.0.1:{server.server_port}'
    print(f'QUAN TRAC DA KHI: {address}',flush=True)
    print(f'Du lieu: {store.path}',flush=True)
    print('Dong trinh duyet khong dung lich. Nhan Ctrl+C tai day de tat ung dung.',flush=True)
    if args.open:
        webbrowser.open(address)
    try:
        server.serve_forever(poll_interval=.2)
    except KeyboardInterrupt:
        pass
    finally:
        controller.shutdown()
        uploader.shutdown()
        server.server_close()

if __name__=='__main__':
    main()
