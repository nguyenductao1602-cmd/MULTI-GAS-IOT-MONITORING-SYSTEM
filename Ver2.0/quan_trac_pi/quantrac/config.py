import copy
import math
SENSOR_IDS = ['MOI_TRUONG.AHT20','MOI_TRUONG.BMP280','BUONG_KHI.AHT20','BUONG_KHI.BMP280','BUONG_KHI.SCD30','BUONG_KHI.CH3SH','BUONG_KHI.CO','BUONG_KHI.H2S','BUONG_KHI.NH3']
DEFAULTS = {
    'port': 'COM15', 'warmup_s': 60, 'read_interval_s': 60, 'read_mode': 'program',
    'schedule_mode': 'cycle', 'fixed_times': ['08:00'], 'debug': False,
    'program': [{'action':'pump','seconds':30}, {'action':'wait','seconds':10}, {'action':'read','target':'ALL'}, {'action':'wait','seconds':60}],
    'sensor_intervals': {k:{'enabled':False,'seconds':60} for k in SENSOR_IDS},
    'relays': [False, False, True], 'stop_on_sensor_error': False,
    'tb': {'enabled': False, 'host': '', 'port': 8883, 'tls': True, 'token': ''},
}
def validate(patch, previous=None):
    if not isinstance(patch, dict) or set(patch) - set(DEFAULTS):
        raise ValueError('Cau hinh khong hop le')
    result = copy.deepcopy(previous or DEFAULTS)
    for key, value in patch.items():
        if key == 'tb':
            if not isinstance(value, dict) or set(value) - set(DEFAULTS['tb']):
                raise ValueError('Cau hinh ThingsBoard khong hop le')
            for field, item in value.items():
                if field == 'token' and item == '':
                    continue
                result['tb'][field] = item
        else:
            result[key] = value
    for key in ('warmup_s', 'read_interval_s'):
        v = result[key]
        low = .5 if key == 'read_interval_s' else 0
        if isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v) or not low <= v <= 604800:
            raise ValueError(f'{key}: nhap so tu {low} den 604800 giay')
    if result['read_mode'] not in ('program', 'interval'):
        raise ValueError('Kieu lich doc khong hop le')
    if not isinstance(result['relays'], list) or len(result['relays']) != 3 or any(type(v) is not bool for v in result['relays']):
        raise ValueError('Can dung ba lua chon relay true/false')
    if type(result['stop_on_sensor_error']) is not bool:
        raise ValueError('Lua chon dung khi loi phai la true/false')
    if not isinstance(result['port'], str) or not result['port'].strip() or len(result['port']) > 256:
        raise ValueError('Ten cong USB khong hop le')
    result['port'] = result['port'].strip()
    import re
    if result['schedule_mode'] not in ('cycle','fixed'):
        raise ValueError('Kieu lich bom khong hop le')
    if type(result['debug']) is not bool:
        raise ValueError('Debug phai la true/false')
    times = result['fixed_times']
    if not isinstance(times,list) or not 1<=len(times)<=48 or any(not isinstance(t,str) or not re.fullmatch(r'(?:[01]\d|2[0-3]):[0-5]\d',t) for t in times):
        raise ValueError('Gio co dinh theo HH:MM, vi du 08:00')
    result['fixed_times'] = sorted(set(times))
    program = result['program']
    if not isinstance(program,list) or not 1<=len(program)<=40:
        raise ValueError('Co che can tu 1 den 40 buoc')
    for step in program:
        if not isinstance(step,dict) or step.get('action') not in ('pump','wait','read'):
            raise ValueError('Buoc co che khong hop le')
        if step['action']=='read':
            if step.get('target') not in ['ALL']+SENSOR_IDS or set(step)-{'action','target'}:
                raise ValueError('Dich doc trong co che khong hop le')
        else:
            v=step.get('seconds')
            if type(v) not in (int,float) or not math.isfinite(v) or not 0<=v<=604800 or set(step)-{'action','seconds'}:
                raise ValueError('Thoi gian buoc phai tu 0 den 604800s')
    intervals = result['sensor_intervals']
    if not isinstance(intervals,dict) or set(intervals)!=set(SENSOR_IDS):
        raise ValueError('Danh sach chu ky cam bien khong hop le')
    for value in intervals.values():
        if not isinstance(value,dict) or set(value)!={'enabled','seconds'} or type(value['enabled']) is not bool or type(value['seconds']) not in (int,float) or not math.isfinite(value['seconds']) or not .5<=value['seconds']<=604800:
            raise ValueError('Chu ky cam bien phai >=0.5s va co enabled true/false')
    tb = result['tb']
    if type(tb['enabled']) is not bool or type(tb['tls']) is not bool:
        raise ValueError('Lua chon MQTT khong hop le')
    if type(tb['port']) is not int or not 1 <= tb['port'] <= 65535:
        raise ValueError('Cong MQTT phai nam trong 1..65535')
    if not isinstance(tb['host'], str) or len(tb['host']) > 253 or any(c in tb['host'] for c in '/\\:@ \r\n'):
        raise ValueError('MQTT host chi gom hostname/IP, khong co https:// hoac duong dan')
    if not isinstance(tb['token'], str) or len(tb['token']) > 1024:
        raise ValueError('Access token khong hop le')
    if tb['enabled'] and (not tb['host'] or not tb['token']):
        raise ValueError('Can MQTT host va Device Access Token de bat ThingsBoard')
    return result

def public_config(config):
    result = copy.deepcopy(config)
    result['tb']['token_set'] = bool(result['tb']['token'])
    result['tb']['token'] = ''
    return result
