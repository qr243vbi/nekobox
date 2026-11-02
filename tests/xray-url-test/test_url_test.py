#!/usr/bin/env python3
"""Compile the production Qt URL tester and exercise an actual VLESS/XHTTP/TLS tunnel.
Only synthetic credentials, loopback listeners and a temporary certificate are used.
"""
import argparse
import copy
import hashlib
import http.server
import json
import os
from pathlib import Path
import shlex
import socket
import ssl
import subprocess
import tempfile
import threading
import time

REPO = Path(__file__).resolve().parents[2]

class Handler(http.server.BaseHTTPRequestHandler):
    hits = []
    def do_GET(self):
        self.hits.append(self.path)
        if self.path == '/hang':
            time.sleep(2)
            return
        if self.path in ('/redirect', '/loop'):
            self.send_response(302)
            self.send_header('Location', '/loop' if self.path == '/loop' else '/ok')
        else:
            self.send_response(204)
        self.send_header('Content-Length', '0')
        self.end_headers()
    def log_message(self, *args):
        pass

def port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xray', type=Path, required=True)
    parser.add_argument('--binary', type=Path, help='Use an already compiled tester (Windows smoke test)')
    args = parser.parse_args()
    xray = str(args.xray.resolve())
    flags = [] if args.binary else shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core', 'Qt6Network'], text=True))
    with tempfile.TemporaryDirectory(prefix='nekobox-url-regression-') as directory:
        root = Path(directory)
        binary = args.binary.resolve() if args.binary else root / 'url-test'
        if not args.binary: subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++17', '-fPIC', '-pthread',
                        '-I' + str(REPO / 'src'), str(REPO / 'tests/xray-url-test/main.cpp'),
                        str(REPO / 'src/gharqad/sys/XrayURLTest.cpp'), '-o', str(binary), *flags], check=True)
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
                        '-subj', '/CN=localhost', '-addext', 'subjectAltName=DNS:localhost,IP:127.0.0.1', '-keyout', str(root/'key.pem'), '-out', str(root/'cert.pem')],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        pin = hashlib.sha256(ssl.PEM_cert_to_DER_cert((root/'cert.pem').read_text())).hexdigest()
        httpd = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        threading.Thread(target=httpd.serve_forever, daemon=True).start()
        https = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        tls.load_cert_chain(root/'cert.pem', root/'key.pem')
        https.socket = tls.wrap_socket(https.socket, server_side=True)
        threading.Thread(target=https.serve_forever, daemon=True).start()
        remote_port = port()
        uuid = 'bc094a59-1516-4c5c-885c-7ad17cb40f10'
        server = {'log': {'loglevel':'warning'}, 'inbounds': [{
            'listen':'127.0.0.1', 'port':remote_port, 'protocol':'vless',
            'settings':{'clients':[{'id':uuid}], 'decryption':'none'},
            'streamSettings':{'network':'xhttp', 'security':'tls',
                'tlsSettings':{'alpn':['h2'], 'certificates':[{
                    'certificateFile':str(root/'cert.pem'), 'keyFile':str(root/'key.pem')}]},
                'xhttpSettings':{'path':'/url-test'}}}],
            'outbounds':[{'protocol':'freedom', 'settings':{'finalRules':[{
                'action':'allow', 'network':'tcp', 'ip':['127.0.0.1/32'], 'port':f'{httpd.server_port},{https.server_port}'}]}}]}
        client = {'log':{'loglevel':'warning'}, 'inbounds':[{
            'listen':'127.0.0.1', 'port':0, 'protocol':'socks', 'settings':{'auth':'noauth'}}],
            'outbounds':[{'protocol':'vless', 'settings':{'vnext':[{'address':'127.0.0.1',
                'port':remote_port, 'users':[{'id':uuid, 'encryption':'none'}]}]},
                'streamSettings':{'network':'xhttp', 'security':'tls',
                    'tlsSettings':{'serverName':'localhost', 'alpn':['h2'], 'pinnedPeerCertSha256':pin},
                    'xhttpSettings':{'path':'/url-test'}}}]}
        # Record the actual child PID/config to verify cleanup after every outcome.
        wrapper = root/'test-xray'
        wrapper.write_text(f'''#!/bin/sh
echo "$$" > {shlex.quote(str(root/'child.pid'))}
echo "$5" > {shlex.quote(str(root/'child.config'))}
exec {shlex.quote(xray)} "$@"
''')
        wrapper.chmod(0o700)
        test_program = str(wrapper) if os.name == 'posix' else xray
        server_file = root/'server.json'
        server_file.write_text(json.dumps(server))
        with (root/'server.log').open('w') as log:
            process = subprocess.Popen([xray, 'run', '-c', str(server_file)], stdout=log, stderr=subprocess.STDOUT)
            try:
                for _ in range(100):
                    try:
                        with socket.create_connection(('127.0.0.1', remote_port), timeout=.1): break
                    except OSError:
                        assert process.poll() is None, (root/'server.log').read_text()
                        time.sleep(.05)
                else: raise AssertionError('Xray server not ready')
                # An occupied saved SOCKS port must neither break the test nor be killed.
                with socket.socket() as active:
                    active.bind(('127.0.0.1', 0)); active.listen()
                    client['inbounds'][0]['port'] = active.getsockname()[1]
                    def run(path='/ok', timeout=4000, cancel=-1, program=test_program, config=client, secure=False, trust=True):
                        for record in ('child.pid', 'child.config'):
                            (root/record).unlink(missing_ok=True)
                        file = root/'client.json'; file.write_text(json.dumps(config))
                        before = time.monotonic()
                        env = os.environ.copy()
                        env.pop('NEKOBOX_TEST_CA', None)
                        if secure and trust: env['NEKOBOX_TEST_CA'] = str(root/'cert.pem')
                        url = f'https://127.0.0.1:{https.server_port}{path}' if secure else f'http://127.0.0.1:{httpd.server_port}{path}'
                        output = subprocess.check_output([str(binary), program, str(file),
                            url, str(timeout), str(cancel)], text=True, timeout=12, env=env)
                        result = json.loads(output)
                        if (root/'child.pid').exists():
                            pid = int((root/'child.pid').read_text())
                            try:
                                os.kill(pid, 0)
                            except ProcessLookupError:
                                pass
                            else:
                                raise AssertionError(f'Test Xray process leaked: {pid}')
                            assert not Path((root/'child.config').read_text().strip()).exists(), 'Temporary config leaked'
                        print(path, result, flush=True)
                        return result, time.monotonic()-before
                    result, _ = run()
                    assert result['latency'] > 0 and not result['error'], result
                    assert '/ok' in Handler.hits, 'No tunneled HTTP request'
                    result, _ = run(secure=True)
                    assert result['latency'] > 0 and not result['error'], result
                    result, _ = run(secure=True, trust=False)
                    assert result['latency'] < 0 and result['error'], result
                    result, _ = run('/redirect')
                    assert result['latency'] > 0 and not result['error'], result
                    result, _ = run('/loop')
                    assert result['latency'] < 0 and result['error'], result
                    result, duration = run('/hang', timeout=250)
                    assert 'timed out' in result['error'] and duration < 2, result
                    result, duration = run('/hang', cancel=150)
                    assert result['cancelled'] and duration < 2, result
                    result, _ = run(cancel=0)
                    assert result['cancelled'], result
                    result, _ = run(program=str(root/'missing-xray'))
                    assert 'Cannot start' in result['error'], result
                    bad = copy.deepcopy(client)
                    bad['outbounds'][0]['streamSettings']['tlsSettings']['pinnedPeerCertSha256'] = '00'*32
                    result, _ = run(timeout=600, config=bad)
                    assert result['latency'] < 0 and result['error'], result
                    invalid = copy.deepcopy(client); invalid['outbounds'][0]['protocol'] = 'invalid'
                    result, duration = run(config=invalid)
                    assert result['latency'] < 0 and 'did not start' in result['error'] and duration < 3, result
                    assert active.getsockname()[1] == client['inbounds'][0]['port']
                    assert process.poll() is None, 'Active Xray server was stopped'
            finally:
                process.terminate()
                process.wait(timeout=5)
                httpd.shutdown(); httpd.server_close()
                https.shutdown(); https.server_close()
    print('PASS: real Xray HTTP/HTTPS URL test, certificate verification, redirect, timeout, cancellation, errors and active-port isolation')

if __name__ == '__main__':
    main()
