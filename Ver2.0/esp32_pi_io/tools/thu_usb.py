"""Thu ESP tu laptop/Orange Pi. Khong tu lap lich bom/doc va khong gui ThingsBoard."""
import argparse
import json
import threading
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, help='COM15 hoac /dev/serial/by-id/...')
    args = parser.parse_args()
    import serial  # pip install pyserial

    port = serial.Serial(args.port, 115200, timeout=0.2, write_timeout=1)
    lock = threading.Lock()
    pending = {}
    counter = 0
    session = 0
    boot_id = None
    stopped = threading.Event()

    def request(command, timeout=3, **fields):
        nonlocal counter, session, boot_id
        with lock:
            counter += 1
            number = counter
            frame = dict(id=number, cmd=command, **fields)
            if command != 'HELLO':
                if not session:
                    raise RuntimeError('Chua co session: nhap hello')
                frame['session'] = session
            waiter = threading.Event()
            slot = [waiter, None]
            pending[number] = slot
            try:
                port.write((json.dumps(frame, separators=(',', ':')) + '\n').encode())
            except Exception:
                pending.pop(number, None)
                raise
        try:
            if not waiter.wait(timeout):
                raise TimeoutError(f'{command} id={number}: qua han; khong tu gui lai lenh bat bom')
            response = slot[1]
            if command == 'HELLO' and response.get('ok'):
                with lock:
                    session = response['session']
                    boot_id = response['boot_id']
            return response
        finally:
            with lock:
                pending.pop(number, None)

    def receive():
        nonlocal session, boot_id
        buffer = bytearray()
        while not stopped.is_set():
            try:
                data = port.read(4096)
                if not data:
                    continue
                buffer.extend(data)
                while b'\n' in buffer:
                    raw, _, buffer = buffer.partition(b'\n')
                    try:
                        message = json.loads(raw)
                    except (ValueError, UnicodeDecodeError):
                        print('Bo qua dong khong phai JSON:', raw[:120])
                        continue
                    if not isinstance(message, dict):
                        continue
                    with lock:
                        if boot_id is not None and message.get('boot_id') != boot_id:
                            session = 0
                        if message.get('cmd') == 'PI_TIMEOUT':
                            session = 0
                        slot = pending.get(message.get('id'))
                        if slot and message.get('type') == 'response':
                            slot[1] = message
                            slot[0].set()
                    if not slot or message.get('type') == 'event':
                        print('EVENT:', json.dumps(message, ensure_ascii=False))
                if len(buffer) > 16384:
                    buffer.clear()
                    print('Bo dem RX qua dai; da bo dong loi')
            except (OSError, serial.SerialException) as error:
                print('Mat USB:', error)
                with lock:
                    session = 0
                stopped.set()
                return

    def heartbeat():
        while not stopped.wait(3):
            with lock:
                connected = bool(session)
            if not connected:
                continue
            try:
                answer = request('PING')
                if not answer.get('ok'):
                    print('PING loi:', answer)
            except (OSError, RuntimeError, TimeoutError) as error:
                print('PING loi:', error)

    rx = threading.Thread(target=receive, daemon=True)
    rx.start()
    hb = threading.Thread(target=heartbeat, daemon=True)
    try:
        time.sleep(2)  # Cho board co the reset khi mo USB
        print(json.dumps(request('HELLO'), ensure_ascii=False))
        hb.start()
        print('Lenh: read | status | relay 0 0 1 | stop | hello | quit')
        print('PING tu dong moi 3s. Chi bat bom khi ban nhap relay.')
        while not stopped.is_set():
            words = input('> ').strip().lower().split()
            if not words:
                continue
            if words == ['quit']:
                break
            try:
                if words[0] == 'relay' and len(words) == 4 and all(x in ('0', '1') for x in words[1:]):
                    answer = request('RELAY', sw1=words[1] == '1', sw2=words[2] == '1', sw3=words[3] == '1')
                elif len(words) == 1 and words[0] in ('read', 'status', 'stop', 'hello'):
                    answer = request(words[0].upper(), timeout=20 if words[0] == 'read' else 3)
                else:
                    print('Lenh khong hop le')
                    continue
                print(json.dumps(answer, indent=2, ensure_ascii=False))
            except (OSError, RuntimeError, TimeoutError) as error:
                print(error)
    except (KeyboardInterrupt, EOFError):
        pass
    finally:
        try:
            request('STOP', timeout=2)
        except Exception:
            pass  # ESP van tu tat sau PI_TIMEOUT_MS neu khong con heartbeat
        stopped.set()
        rx.join(timeout=1)
        if hb.is_alive():
            hb.join(timeout=4)
        port.close()


if __name__ == '__main__':
    main()
