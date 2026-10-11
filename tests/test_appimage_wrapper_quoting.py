"""Offline checks of the two AppImage wrappers' path/argument construction.

Run: python3 -m unittest discover -s tests -p 'test_appimage_wrapper_quoting.py' -v
APPIMAGE_DEPLOY_SCRIPT can select a baseline or mutation of deploy_linux64.sh.
Only the two heredocs are rendered. Their exec commands are replaced with a
printf recorder before testing: no AppImage, core, GUI, or deploy script runs.
"""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = Path(os.environ.get("APPIMAGE_DEPLOY_SCRIPT", ROOT / "script/deploy_linux64.sh"))
SHELLS = ["/bin/sh", shutil.which("bash")]
APPIMAGE = "/downloads/App Image 'quoted' \"double\" [*?].AppImage"
ARGUMENTS = ["", "core", "--name=two words", "single' double\"", "*?[]",
             "$(printf never-executed)", "line\nbreak", r"back\slash"]


def render_wrapper(name):
    source = SCRIPT.read_text()
    matches = list(re.finditer(
        rf"^cat << (?P<delimiter>'?EOF'?) > {re.escape(name)}\n(?P<body>.*?)^EOF\n",
        source, re.MULTILINE | re.DOTALL,
    ))
    if len(matches) != 1:
        raise AssertionError(f"Expected exactly one {name} wrapper heredoc")
    match = matches[0]
    # Render only the extracted heredoc with the build-time executable name.
    # In particular, do not source or execute the release deployment script.
    result = subprocess.run(
        [shutil.which("bash"), "-c",
         "cat << " + match["delimiter"] + "\n" + match["body"] + "EOF\n"],
        env={"PATH": os.defpath, "EXECUTABLE_NAME": "nekobox",
             "APPIMAGE": "build-time value must not leak",
             "NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE": "build-time selector must not leak"},
        capture_output=True, text=True, timeout=5, check=True,
    )
    if result.stderr:
        raise AssertionError(result.stderr)
    return result.stdout


class AppImageWrapperQuotingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if any(shell is None or not Path(shell).exists() for shell in SHELLS):
            raise unittest.SkipTest("These checks require /bin/sh and Bash")
        cls.wrappers = {name: render_wrapper(name) for name in ("nekobox_core", "AppRun")}

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="nekobox-wrapper-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        # Ensure an unquoted glob has real matches, so it cannot pass by chance.
        for directory in ("globa1", "globb2"):
            (self.root / directory).mkdir()
            for name in self.wrappers:
                (self.root / directory / name).touch()

    def record(self, shell, name, argv0, args=(), selector=None, appimage=APPIMAGE):
        body = self.wrappers[name]
        lines = body.splitlines()
        self.assertEqual(lines[0], "#!/bin/sh")
        self.assertEqual(sum(line.startswith("exec ") for line in lines), 1)
        self.assertTrue(all(line.startswith(("#!", "export NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE=", "exec "))
                            for line in lines), "Unexpected command in wrapper; review before executing")
        # Preserve the production command's argument expressions verbatim, but
        # record them as NUL-separated strings instead of running its target.
        recorder = body.replace("\nexec ", "\nprintf '%s\\0' ", 1)
        env = {"PATH": os.defpath}
        if appimage is not None:
            env["APPIMAGE"] = appimage
        if selector is not None:
            env["NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE"] = selector
        result = subprocess.run([shell, "-c", recorder, str(argv0), *args], cwd=self.root,
                                env=env, capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(result.stderr, b"")
        self.assertTrue(result.stdout.endswith(b"\0"))
        return result.stdout[:-1].decode().split("\0")

    def variants(self):
        return (("nekobox_core", None), ("AppRun", None), ("AppRun", ""),
                ("AppRun", "custom core 'quoted' \"double\" [*?]"))

    def expected(self, name, argv0, args=(), selector=None, appimage=APPIMAGE):
        target = ".nekobox_core_binary_file" if name == "nekobox_core" else (selector or "nekobox")
        prefix = [str(Path(argv0).parent / target)]
        if name == "nekobox_core":
            prefix += ["-argv0", appimage or ""]
        return prefix + list(args)

    def test_wrapper_paths_preserve_spaces_quotes_and_globs(self):
        directories = ("ordinary", "two words", "tab\tpath", "line\nbreak",
                       "single'and\"double", "glob[ab]*", r"back\slash")
        for shell in SHELLS:
            for name, selector in self.variants():
                for directory in directories:
                    argv0 = self.root / directory / name
                    with self.subTest(shell=shell, name=name, selector=selector, directory=directory):
                        self.assertEqual(self.record(shell, name, argv0, ARGUMENTS, selector),
                                         self.expected(name, argv0, ARGUMENTS, selector))

    def test_no_arguments_does_not_add_an_empty_argument(self):
        for shell in SHELLS:
            for name, selector in self.variants():
                argv0 = self.root / "ordinary" / name
                with self.subTest(shell=shell, name=name, selector=selector):
                    self.assertEqual(self.record(shell, name, argv0, selector=selector),
                                     self.expected(name, argv0, selector=selector))

    def test_relative_wrapper_paths_preserve_arguments(self):
        for shell in SHELLS:
            for name, selector in self.variants():
                argv0 = Path("relative path") / name
                with self.subTest(shell=shell, name=name, selector=selector):
                    self.assertEqual(self.record(shell, name, argv0, ARGUMENTS, selector),
                                     self.expected(name, argv0, ARGUMENTS, selector))

    def test_core_preserves_empty_appimage_value(self):
        for shell in SHELLS:
            for appimage in (None, ""):
                argv0 = self.root / "ordinary" / "nekobox_core"
                with self.subTest(shell=shell, appimage=appimage):
                    self.assertEqual(self.record(shell, "nekobox_core", argv0, ARGUMENTS, appimage=appimage),
                                     self.expected("nekobox_core", argv0, ARGUMENTS, appimage=appimage))


if __name__ == "__main__":
    unittest.main()
