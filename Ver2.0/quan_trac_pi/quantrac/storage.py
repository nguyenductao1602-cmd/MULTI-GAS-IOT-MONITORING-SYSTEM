import contextlib
import json
import math
import sqlite3
import time
from pathlib import Path

def now_ms():
    return int(time.time() * 1000)

def flatten(packet):
    values = {'boot_id': str(packet.get('boot_id', '')), 'sample_seq': packet.get('sample_seq', 0)}
    for group in ('MOI_TRUONG', 'BUONG_KHI'):
        for sensor, item in packet.get(group, {}).items():
            entries = item.items() if sensor == 'RS485' else [(sensor, item)]
            for name, data in entries:
                if not isinstance(data, dict):
                    continue
                prefix = f'{group}_{name}'
                values[prefix + '_ok'] = data.get('ok') is True
                values[prefix + '_error'] = str(data.get('error', 'UNKNOWN'))
                if data.get('ok') is True:
                    for key in ('temperature_c', 'humidity_rh', 'pressure_hpa', 'co2_ppm', 'ppm'):
                        value = data.get(key)
                        if type(value) in (int, float) and math.isfinite(value):
                            values[f'{prefix}_{key}'] = value
    for i, value in enumerate(packet.get('relays', []), 1):
        if type(value) is bool:
            values[f'RELAY{i}'] = value
    return values

class Store:
    def __init__(self, directory):
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=True)
        self.path = self.directory / 'quan_trac.sqlite3'
        with self.db() as db:
            db.execute('PRAGMA journal_mode=WAL')
            db.executescript("""
                CREATE TABLE IF NOT EXISTS config (id INTEGER PRIMARY KEY CHECK(id=1), body TEXT NOT NULL);
                CREATE TABLE IF NOT EXISTS samples (
                    id INTEGER PRIMARY KEY, ts INTEGER NOT NULL, source TEXT NOT NULL,
                    boot_id TEXT NOT NULL, seq INTEGER NOT NULL, body TEXT NOT NULL,
                    sent_at INTEGER, UNIQUE(source,boot_id,seq));
                CREATE INDEX IF NOT EXISTS sample_time ON samples(source,ts);
                CREATE INDEX IF NOT EXISTS sample_pending ON samples(source,sent_at,id);
                CREATE TABLE IF NOT EXISTS events (
                    id INTEGER PRIMARY KEY, ts INTEGER NOT NULL, level TEXT NOT NULL,
                    code TEXT NOT NULL, message TEXT NOT NULL);
            """)
    @contextlib.contextmanager
    def db(self):
        conn = sqlite3.connect(self.path, timeout=2)
        conn.row_factory = sqlite3.Row
        conn.execute('PRAGMA synchronous=FULL')
        try:
            with conn:
                yield conn
        finally:
            conn.close()
    def load_config(self):
        with self.db() as db:
            row = db.execute('SELECT body FROM config WHERE id=1').fetchone()
        return json.loads(row[0]) if row else None
    def save_config(self, config):
        with self.db() as db:
            db.execute('INSERT OR REPLACE INTO config VALUES(1,?)', (json.dumps(config, allow_nan=False),))
    def event(self, level, code, message):
        with self.db() as db:
            db.execute('INSERT INTO events(ts,level,code,message) VALUES(?,?,?,?)',
                       (now_ms(), level, code, str(message)[:1200]))
    def sample(self, packet, source):
        if source not in ('serial', 'demo') or type(packet.get('sample_seq')) is not int or packet['sample_seq'] < 1:
            raise ValueError('Mau do khong hop le')
        body = json.dumps(packet, ensure_ascii=False, allow_nan=False)
        with self.db() as db:
            cursor = db.execute('INSERT OR IGNORE INTO samples(ts,source,boot_id,seq,body) VALUES(?,?,?,?,?)',
                                (now_ms(), source, str(packet['boot_id']), packet['sample_seq'], body))
            return cursor.rowcount == 1
    def history(self, limit=120, source='all'):
        limit = min(max(int(limit), 1), 1000)
        with self.db() as db:
            clause, args = (' WHERE source=?', [source]) if source in ('demo', 'serial') else ('', [])
            rows = db.execute('SELECT * FROM samples' + clause + ' ORDER BY id DESC LIMIT ?', args + [limit]).fetchall()
        return [dict(id=r['id'], ts=r['ts'], source=r['source'], sent=bool(r['sent_at']), data=json.loads(r['body'])) for r in rows]
    def events(self, limit=100):
        with self.db() as db:
            rows = db.execute('SELECT * FROM events ORDER BY id DESC LIMIT ?', (min(max(int(limit), 1), 500),)).fetchall()
        return [dict(row) for row in rows]
    def stats(self):
        with self.db() as db:
            row = db.execute("SELECT COUNT(*) AS total, SUM(source='serial' AND sent_at IS NULL) AS pending, SUM(source='serial' AND sent_at IS NOT NULL) AS sent FROM samples").fetchone()
        size = sum(p.stat().st_size for p in self.directory.glob('quan_trac.sqlite3*') if p.is_file())
        return dict(total=row['total'], pending=row['pending'] or 0, sent=row['sent'] or 0, size_bytes=size)
    def pending(self, limit=25):
        with self.db() as db:
            rows = db.execute("SELECT id,ts,body FROM samples WHERE source='serial' AND sent_at IS NULL ORDER BY id LIMIT ?", (limit,)).fetchall()
        return [dict(id=r['id'], ts=r['ts'], data=json.loads(r['body'])) for r in rows]
    def mark_sent(self, row_id):
        with self.db() as db:
            db.execute("UPDATE samples SET sent_at=? WHERE id=? AND source='serial'", (now_ms(), row_id))
    def cleanup(self, days=90):
        cutoff = now_ms() - days * 86400000
        with self.db() as db:
            db.execute("DELETE FROM samples WHERE ts<? AND (sent_at IS NOT NULL OR source='demo')", (cutoff,))
            db.execute('DELETE FROM events WHERE ts<?', (cutoff,))
        with self.db() as db:
            db.execute('PRAGMA wal_checkpoint(PASSIVE)')
    def export_rows(self, source='all'):
        with self.db() as db:
            clause, args = (' WHERE source=?', (source,)) if source in ('serial', 'demo') else ('', ())
            cursor = db.execute('SELECT ts,source,body FROM samples' + clause + ' ORDER BY id', args)
            for row in cursor:
                yield row['ts'], row['source'], flatten(json.loads(row['body']))

    def series(self, hours=1, source='demo'):
        hours=max(1,min(float(hours),168))
        end=now_ms(); start=end-int(hours*3600000); bucket=max(1000,int(hours*3600000/240))
        points={}
        with self.db() as db:
            rows=db.execute('SELECT ts,body FROM samples WHERE source=? AND ts>=? ORDER BY ts DESC LIMIT 50001',(source,start)).fetchall()
        truncated=len(rows)>50000
        for row in reversed(rows[:50000]):
            packet=json.loads(row['body'])
            for group in ('MOI_TRUONG','BUONG_KHI'):
                for sensor,item in packet.get(group,{}).items():
                    items=item.items() if sensor=='RS485' else [(sensor,item)]
                    for name,data in items:
                        for key in ('temperature_c','humidity_rh','pressure_hpa','co2_ppm','ppm'):
                            if key not in data: continue
                            value=data[key] if data.get('ok') is True else None
                            if type(value) not in (int,float) or not math.isfinite(value): value=None
                            series=f'{group}.{name}.{key}'
                            points.setdefault(series,{})[row['ts']//bucket]=[row['ts'],value]
        return dict(start=start,end=end,truncated=truncated,series={key:list(value.values()) for key,value in points.items()})
