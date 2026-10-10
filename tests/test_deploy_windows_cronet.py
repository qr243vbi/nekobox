"""Offline regression tests for the Windows Cronet packaging boundary.

Run with Python 3, Bash, jq, and the coreutils already used by the deploy script:
    python3 -m unittest discover -s tests -p 'test_deploy_windows_cronet.py' -v

CRONET_DEPLOY_SCRIPT may select an unmodified script for baseline comparison.
Network calls and archive extraction are deterministic local mocks. The DLL
fixtures are inert bytes with a DOS magic prefix, never executable libraries.
"""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = Path(os.environ.get("CRONET_DEPLOY_SCRIPT", ROOT / "script/deploy_windows.sh")).resolve()
DUMMY_DLL = b"MZ\x00inert Cronet test fixture, not a usable DLL\n"

MOCK_CURL = r'''#!/usr/bin/env python3
import json
import os
from pathlib import Path
import signal
import sys

args = sys.argv[1:]
with open(os.environ["MOCK_LOG"], "a") as log:
    log.write(json.dumps(args) + "\n")
url = next((arg for arg in args if arg.startswith("https://")), "")
if "\r" in url or "\n" in url:
    sys.exit("curl: (3) URL rejected: Malformed input to a URL function")
fail_http = any(arg == "--fail" or (arg.startswith("-") and not arg.startswith("--") and "f" in arg[1:]) for arg in args)
output = Path(args[args.index("-o") + 1]) if "-o" in args else None
scenario = os.environ["MOCK_SCENARIO"]
status = 0
if url == "https://api.github.com/repos/SagerNet/cronet-go/releases/latest":
    bodies = {
        "metadata_http": '{"message":"Not Found"}',
        "metadata_invalid": '{',
        "metadata_missing": '{}',
        "metadata_null": '{"tag_name":null}',
        "metadata_empty": '{"tag_name":""}',
        "metadata_number": '{"tag_name":123}',
    }
    body = bodies.get(scenario, '{"tag_name":"v1.2.3"}').encode()
    if scenario == "metadata_http" and fail_http:
        status, body = 22, b""
    if scenario == "metadata_transfer":
        status = 18  # A transfer can fail even after producing parseable JSON.
elif url.startswith("https://github.com/SagerNet/cronet-go/releases/download/"):
    body = b"MZ\x00inert Cronet test fixture, not a usable DLL\n"
    if scenario == "asset_http":
        body = b"Not Found"
        if fail_http:
            status, body = 22, b""
    elif scenario in ("asset_transfer", "asset_terminate"):
        status, body = 18, b"MZpartial"
    elif scenario == "asset_empty":
        body = b""
    elif scenario == "asset_html":
        body = b"<html>upstream error</html>"
elif url.startswith("https://github.com/XTLS/Xray-core/releases/download/"):
    body = b"inert archive fixture"
else:
    sys.exit("Unexpected curl URL: " + url)
if output is None:
    sys.stdout.buffer.write(body)
else:
    output.write_bytes(body)
if scenario == "asset_terminate" and "/releases/download/" in url:
    os.kill(os.getppid(), signal.SIGTERM)
sys.exit(status)
'''

MOCK_WINDOWS_JQ = r'''#!/usr/bin/env python3
import os
import subprocess
import sys

result = subprocess.run([os.environ["MOCK_REAL_JQ"], *sys.argv[1:]],
                        input=sys.stdin.buffer.read(), capture_output=True)
sys.stdout.buffer.write(result.stdout.replace(b"\n", b"\r\n"))
sys.stderr.buffer.write(result.stderr)
sys.exit(result.returncode)
'''

MOCK_7Z = r'''#!/usr/bin/env python3
from pathlib import Path
import sys

dest = Path(next(arg[2:] for arg in sys.argv[1:] if arg.startswith("-o")))
# Only this harmless shell stub is executed by the deployment version check.
stub = dest / "xray.exe"
stub.write_text("#!/bin/sh\nprintf 'Xray 26.9.9 test fixture\\n'\n")
stub.chmod(0o755)
'''


class CronetPackagingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for command in ("bash", "jq", "python3", "head", "mktemp", "cp", "mv", "rm", "mkdir", "touch", "grep"):
            if shutil.which(command) is None:
                raise unittest.SkipTest("Required test dependency is missing: " + command)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="nekobox cronet test ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for folder in ("script", "build", "res/public", "mock-bin", "deployment"):
            (self.root / folder).mkdir(parents=True, exist_ok=True)
        shutil.copy2(SCRIPT, self.root / "script/deploy_windows.sh")
        shutil.copy2(ROOT / "script/env_deploy.sh", self.root / "script/env_deploy.sh")
        for file in ("srslist.json", "build/nekobox.exe", "build/nekobox_core.exe", "build/test.qm", "build/icu-test.dll", "res/languages.txt"):
            (self.root / file).write_text("inert fixture\n")
        for name, body in (("curl", MOCK_CURL), ("7z", MOCK_7Z)):
            file = self.root / "mock-bin" / name
            file.write_text(body)
            file.chmod(0o755)
        self.log = self.root / "curl.log"

    def run_deploy(self, scenario="success", arch="x86_64"):
        env = os.environ.copy()
        env.update({
            "PATH": str(self.root / "mock-bin") + os.pathsep + env["PATH"],
            "NEKOBOX_ENV_DEPLOYED": "yes",
            "SRC_ROOT": str(self.root),
            "BUILD": str(self.root / "build"),
            "DEPLOYMENT": str(self.root / "deployment"),
            "EXECUTABLE_NAME": "nekobox",
            "INPUT_VERSION": "test",
            "version_standalone": "nekobox-test",
            "COMPILER": "MinGW",
            "SKIP_UPX": "true",
            "SKIP_NSIS": "true",
            "SKIP_ZIP": "true",
            "MOCK_SCENARIO": scenario,
            "MOCK_LOG": str(self.log),
            "MOCK_REAL_JQ": shutil.which("jq"),
        })
        # Match the workflow's sourced-script invocation, including its set -e.
        return subprocess.run(
            ["bash", "-c", 'source script/deploy_windows.sh "$1"', "test", arch],
            cwd=self.root, env=env, text=True, capture_output=True, timeout=15,
        )

    def cronet_requests(self):
        calls = self.log.read_text().splitlines() if self.log.exists() else []
        return [json.loads(call) for call in calls if "SagerNet/cronet-go" in call]

    def assert_rejected(self, result):
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(list(self.root.glob("deployment/**/libcronet.dll")), [])
        self.assertEqual(list(self.root.glob("libcronet-windows-*.dll*")), [])

    def test_http_error_metadata_stops_before_asset_download(self):
        self.assert_rejected(self.run_deploy("metadata_http"))
        self.assertEqual(len(self.cronet_requests()), 1)

    def assert_metadata_rejected(self, scenario):
        self.assert_rejected(self.run_deploy(scenario))
        self.assertEqual(len(self.cronet_requests()), 1)

    def test_invalid_json_stops_before_asset_download(self):
        self.assert_metadata_rejected("metadata_invalid")

    def test_missing_tag_stops_before_asset_download(self):
        self.assert_metadata_rejected("metadata_missing")

    def test_null_tag_stops_before_asset_download(self):
        self.assert_metadata_rejected("metadata_null")

    def test_empty_tag_stops_before_asset_download(self):
        self.assert_metadata_rejected("metadata_empty")

    def test_numeric_tag_stops_before_asset_download(self):
        self.assert_metadata_rejected("metadata_number")

    def test_failed_metadata_transfer_is_not_hidden_by_jq(self):
        self.assert_rejected(self.run_deploy("metadata_transfer"))
        self.assertEqual(len(self.cronet_requests()), 1)

    def test_http_error_asset_is_never_cached_or_packaged(self):
        self.assert_rejected(self.run_deploy("asset_http"))

    def test_failed_asset_transfer_cleans_partial_download(self):
        self.assert_rejected(self.run_deploy("asset_transfer"))

    def test_terminated_asset_transfer_cleans_partial_download(self):
        self.assert_rejected(self.run_deploy("asset_terminate"))

    def test_native_windows_jq_line_endings_do_not_corrupt_url(self):
        mock = self.root / "mock-bin/jq"
        mock.write_text(MOCK_WINDOWS_JQ)
        mock.chmod(0o755)
        result = self.run_deploy()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.root / "deployment/nekobox-test-windows64/libcronet.dll").read_bytes(), DUMMY_DLL)

    def test_successful_empty_response_is_rejected(self):
        self.assert_rejected(self.run_deploy("asset_empty"))

    def test_successful_html_response_is_rejected(self):
        self.assert_rejected(self.run_deploy("asset_html"))

    def test_poisoned_cache_is_refreshed_before_packaging(self):
        cache = self.root / "libcronet-windows-amd64.dll"
        cache.write_bytes(b"Not Found")
        result = self.run_deploy()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(cache.read_bytes(), DUMMY_DLL)
        self.assertEqual((self.root / "deployment/nekobox-test-windows64/libcronet.dll").read_bytes(), DUMMY_DLL)

    def test_empty_cache_is_refreshed_before_packaging(self):
        cache = self.root / "libcronet-windows-amd64.dll"
        cache.touch()
        result = self.run_deploy()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(cache.read_bytes(), DUMMY_DLL)

    def test_header_checked_cache_is_reused_without_network(self):
        (self.root / "libcronet-windows-amd64.dll").write_bytes(DUMMY_DLL)
        result = self.run_deploy()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.cronet_requests(), [])
        self.assertEqual((self.root / "deployment/nekobox-test-windows64/libcronet.dll").read_bytes(), DUMMY_DLL)

    def test_amd64_download_is_promoted_and_packaged(self):
        result = self.run_deploy()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.root / "libcronet-windows-amd64.dll").read_bytes(), DUMMY_DLL)
        self.assertEqual((self.root / "deployment/nekobox-test-windows64/libcronet.dll").read_bytes(), DUMMY_DLL)
        self.assertEqual(len(list(self.root.glob("libcronet-windows-*.dll*"))), 1)
        self.assertIn("https://github.com/SagerNet/cronet-go/releases/download/v1.2.3/libcronet-windows-amd64.dll", self.cronet_requests()[1])

    def test_arm64_download_uses_matching_asset(self):
        result = self.run_deploy(arch="arm64")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.root / "deployment/nekobox-test-windows-arm64/libcronet.dll").read_bytes(), DUMMY_DLL)
        self.assertIn("https://github.com/SagerNet/cronet-go/releases/download/v1.2.3/libcronet-windows-arm64.dll", self.cronet_requests()[1])

    def test_x86_skips_cronet(self):
        result = self.run_deploy(arch="x86")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.cronet_requests(), [])
        self.assertEqual(list(self.root.glob("deployment/**/libcronet.dll")), [])

    def test_retry_after_failed_transfer_downloads_fresh_bytes(self):
        self.assertNotEqual(self.run_deploy("asset_transfer").returncode, 0)
        result = self.run_deploy()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.root / "deployment/nekobox-test-windows64/libcronet.dll").read_bytes(), DUMMY_DLL)


if __name__ == "__main__":
    unittest.main()
