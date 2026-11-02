#!/usr/bin/env python3
"""Exercise the packaged core's real Start/Stop RPC and Xray child process.
No GUI, TUN, user profiles or remote services: traffic stays on loopback.
Uses the application's binary Thrift protocol without third-party Python packages.
"""
import argparse
import copy
import http.server
import json
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import threading
import time


def receive(sock, size):
    result = b''
    while len(result) < size:
        part = sock.recv(size - len(result))
        if not part:
            raise ConnectionError('Unexpected end of core RPC response')
        result += part
    return result


def value(sock, kind):
    if kind == 2:
        return receive(sock, 1) != b'\0'
    if kind == 8:
        return struct.unpack('!i', receive(sock, 4))[0]
    if kind == 11:
        length = struct.unpack('!i', receive(sock, 4))[0]
        assert 0 <= length < 2**24, length
        return receive(sock, length).decode('utf-8')
    if kind == 12:
        fields = {}
        while True:
            field_kind = receive(sock, 1)[0]
            if field_kind == 0:
                return fields
            field_id = struct.unpack('!h', receive(sock, 2))[0]
            fields[field_id] = value(sock, field_kind)
    raise AssertionError(f'Unexpected Thrift response type: {kind}')


def string(text):
    data = text.encode('utf-8')
    return struct.pack('!i', len(data)) + data


def field(kind, number, payload):
    return struct.pack('!bh', kind, number) + payload


def rpc(port, method, fields):
    message = (struct.pack('!I', 0x80010001) + string(method) + struct.pack('!i', 1) +
               field(12, 1, b''.join(fields) + b'\0') + b'\0')
    with socket.create_connection(('127.0.0.1', port), timeout=15) as sock:
        sock.sendall(message)
        assert struct.unpack('!I', receive(sock, 4))[0] == 0x80010002, 'RPC did not return a reply'
        assert value(sock, 11) == method
        assert value(sock, 8) == 1
        result = value(sock, 12)
        assert 0 in result and isinstance(result[0], dict), result
        return result[0].get(1, '')


def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def listening(port):
    try:
        with socket.create_connection(('127.0.0.1', port), timeout=.1):
            return True
    except OSError:
        return False


def assert_closed(port):
    deadline = time.monotonic() + 3
    while listening(port) and time.monotonic() < deadline:
        time.sleep(.05)
    assert not listening(port), f'Listener {port} leaked after failed start/stop'


PAYLOAD = b'nekobox-core-xray-start-ok'
class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Length', str(len(PAYLOAD)))
        self.end_headers()
        self.wfile.write(PAYLOAD)
    def log_message(self, *args):
        pass


def fetch(proxy_port, target_port):
    with socket.create_connection(('127.0.0.1', proxy_port), timeout=5) as sock:
        sock.sendall(b'\x05\x01\x00')
        assert receive(sock, 2) == b'\x05\x00'
        sock.sendall(b'\x05\x01\x00\x01' + socket.inet_aton('127.0.0.1') + struct.pack('!H', target_port))
        reply = receive(sock, 4)
        assert reply[1] == 0, reply
        assert reply[3] == 1, reply
        receive(sock, 6)
        sock.sendall(b'GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
        data = b''
        while PAYLOAD not in data:
            part = sock.recv(4096)
            if not part:
                break
            data += part
        assert PAYLOAD in data, data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, required=True)
    parser.add_argument('--xray', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='nekobox core start ') as directory:
        root = Path(directory)
        settings = root/'settings тест'
        settings.mkdir()
        rpc_port, proxy_port, xray_port = free_port(), free_port(), free_port()
        httpd = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        threading.Thread(target=httpd.serve_forever, daemon=True).start()
        config = {'log':{'level':'error'}, 'inbounds':[{
            'type':'mixed', 'tag':'mixed-in', 'listen':'127.0.0.1', 'listen_port':proxy_port}],
            'outbounds':[{'type':'socks', 'tag':'proxy', 'server':'127.0.0.1', 'server_port':xray_port,
                          'version':'5'}], 'route':{'final':'proxy'}}
        xray = {'log':{'loglevel':'warning'}, 'inbounds':[{
            'listen':'127.0.0.1', 'port':xray_port, 'protocol':'socks', 'settings':{'auth':'noauth'}}],
            'outbounds':[{'protocol':'freedom', 'settings':{'finalRules':[{
                'action':'allow', 'network':'tcp', 'ip':['127.0.0.1/32'], 'port':str(httpd.server_port)}]}}]}
        def start(extra=xray, core_config=config, program=None):
            return rpc(rpc_port, 'Start', [
                field(11, 1, string(json.dumps(core_config))), field(2, 2, b'\x01'),
                field(2, 3, b'\x01'), field(11, 4, string(str(program or args.xray.resolve()))),
                field(11, 5, string('run -format json -c %s')),
                field(11, 6, string(json.dumps(extra))), field(11, 7, string(str(settings))),
                field(2, 8, b'\x00')])
        def stop():
            assert not rpc(rpc_port, 'Stop', [field(2, 1, b'\x00')])
        with (root/'core.log').open('w', encoding='utf-8') as log:
            core = subprocess.Popen([str(args.core.resolve()), 'core', '-port', str(rpc_port),
                '-ruleset-cache-directory', str(root/'rules')], cwd=root, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 15
                while not listening(rpc_port) and time.monotonic() < deadline:
                    assert core.poll() is None, (root/'core.log').read_text(errors='replace')
                    time.sleep(.05)
                assert listening(rpc_port), 'Core RPC did not start'
                bad = copy.deepcopy(xray)
                bad['outbounds'][0]['protocol'] = 'invalid'
                error = start(extra=bad)
                assert error and 'external core' in error.lower(), error
                assert_closed(proxy_port); assert_closed(xray_port)
                print('PASS: Xray startup error returned over RPC; no active main proxy', flush=True)
                error = start()
                assert not error, error
                assert (settings/'extra.json').is_file(), 'Xray did not receive a .json file'
                assert not (settings/'extra.conf').exists(), 'Legacy .conf used for Xray'
                fetch(proxy_port, httpd.server_port)
                print('PASS: HTTP payload traversed main core -> Xray SOCKS -> HTTP server', flush=True)
                assert 'already started' in start()
                fetch(proxy_port, httpd.server_port)
                print('PASS: rejected duplicate start preserves the active connection', flush=True)
                stop()
                assert_closed(proxy_port); assert_closed(xray_port)
                bad_core = {'outbounds':[{'type':'nonexistent-outbound'}]}
                error = start(core_config=bad_core)
                assert error, 'Invalid main core config accepted'
                assert_closed(proxy_port); assert_closed(xray_port)
                print('PASS: main core config failure cleans up the already started Xray', flush=True)
                error = start(program=root/'missing-xray.exe')
                assert error, 'Missing external executable accepted'
                stop()
                assert_closed(proxy_port); assert_closed(xray_port)
                assert not start(), 'A failed start prevents later recovery'
                fetch(proxy_port, httpd.server_port)
                stop()
                assert_closed(proxy_port); assert_closed(xray_port)
                print('PASS: missing executable, repeated stop and restart recovery', flush=True)
            except BaseException:
                print((root/'core.log').read_text(errors='replace'), flush=True)
                raise
            finally:
                try:
                    if core.poll() is None: stop()
                except (OSError, AssertionError):
                    pass
                core.terminate()
                core.wait(timeout=5)
                httpd.shutdown(); httpd.server_close()
    print('PASS: normal Start/Stop RPC, JSON filename, forwarding and failure cleanup')


if __name__ == '__main__':
    main()
