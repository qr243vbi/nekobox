#!/usr/bin/env python3
"""Execute verbatim production refresh methods with offline dependency adapters.

REFRESH_PAINTING_SOURCE can select an unmodified baseline mainwindow.cpp.
REFRESH_PAINTING_QT=1 also forwards production table calls to a real Qt widget.
The full application, native Qt C++ types and real profile storage are not used.
"""
import ctypes
import hashlib
import importlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SOURCE = Path(os.environ.get("REFRESH_PAINTING_SOURCE", str(ROOT / "src/gharqad/ui/mainwindow.cpp")))
CASES = (
    "missing_group_full", "missing_group_single", "missing_group_direct_helper",
    "oversized_name", "oversized_address", "oversized_type", "oversized_latency",
    "oversized_traffic", "oversized_descending", "oversized_raw", "oversized_id",
    "boundary_12000", "missing_id_small", "missing_id_3000", "empty_group",
    "normal_full", "normal_single", "sort_ascending", "sort_descending",
)
CALLBACK = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_int)


class RefreshPaintingRegression(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="refresh-painting-")
        cls.addClassCleanup(cls.temporary.cleanup)
        build = Path(cls.temporary.name)
        source = SOURCE.read_bytes()
        start = b"void MainWindow::refresh_proxy_list_impl(const int &id,"
        end = b"/*\nvoid MainWindow::refresh_table_item("
        if source.count(start) != 1 or source.count(end) != 1:
            raise AssertionError("Production extraction boundaries changed; review the harness")
        production = source[source.index(start):source.index(end)]
        if production.count(b"void MainWindow::refresh_proxy_list_impl_refresh_data(") != 1:
            raise AssertionError("Production helper changed; review the harness")
        (build / "production_refresh.inc").write_bytes(production)
        manifest = {
            "source": str(SOURCE), "source_sha256": hashlib.sha256(source).hexdigest(),
            "extracted_sha256": hashlib.sha256(production).hexdigest(),
            "extraction": "Both refresh methods and intervening comparator copied verbatim; no rewriting.",
            "dependencies": "Synthetic QString/profiles/window; production GroupSort.hpp; optional live Qt widget callbacks.",
        }
        print(json.dumps(manifest, indent=2), flush=True)
        binary = build / "refresh_painting.so"
        command = shlex.split(os.environ.get("CXX", "c++")) + [
            "-std=c++20", "-O0", "-g", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-parameter", "-Wno-unused-variable", "-fPIC", "-shared",
            *shlex.split(os.environ.get("EXTRA_CXXFLAGS", "")),
            "-I", str(ROOT / "src"), "-I", str(HERE), "-I", str(build),
            str(HERE / "cases.cpp"), "-o", str(binary),
        ]
        subprocess.run(command, check=True)
        cls.library = ctypes.CDLL(str(binary))
        cls.library.run_case.argtypes = [ctypes.c_char_p, CALLBACK]
        cls.library.run_case.restype = ctypes.c_char_p
        cls.QtWidgets = None
        if os.environ.get("REFRESH_PAINTING_QT") == "1":
            os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
            runtime = build / "runtime"
            runtime.mkdir(mode=0o700)
            os.environ["XDG_RUNTIME_DIR"] = str(runtime)
            os.environ["XDG_CACHE_HOME"] = str(build / "cache")
            for binding in ("PySide6", "PyQt6", "PyQt5", "PySide2"):
                try:
                    cls.QtWidgets = importlib.import_module(binding + ".QtWidgets")
                    cls.QtCore = importlib.import_module(binding + ".QtCore")
                    break
                except ImportError:
                    pass
            if cls.QtWidgets is None:
                raise RuntimeError("Qt widget verification requested but no Python binding is installed")
            cls.app = cls.QtWidgets.QApplication.instance() or cls.QtWidgets.QApplication([])
            class PaintProbe(cls.QtWidgets.QTableView):
                def __init__(self):
                    super().__init__()
                    self.paints = 0

                def paintEvent(self, event):
                    self.paints += 1
                    super().paintEvent(event)
            cls.PaintProbe = PaintProbe
            print(f"Live table boundary: {binding}, Qt {cls.QtCore.qVersion()}", flush=True)

    def run_case(self, name):
        callback_errors = []
        table = self.PaintProbe() if self.QtWidgets else None
        if table is not None:
            table.show()
            self.app.processEvents()
            self.assertGreater(table.paints, 0, "Qt offscreen paint probe did not initialize")
        def receive(kind, value):
            try:
                if table is not None:
                    if kind == 1:
                        table.setUpdatesEnabled(bool(value))
                    elif kind == 2:
                        table.blockSignals(bool(value))
            except Exception as error:
                callback_errors.append(str(error))
        callback = CALLBACK(receive)
        result = self.library.run_case(name.encode(), callback).decode()
        try:
            self.assertEqual(callback_errors, [], "Qt callback bridge failed")
            if table is not None:
                self.assertTrue(table.updatesEnabled(), "real Qt table was left with updates disabled")
                self.assertTrue(table.viewport().updatesEnabled(), "real Qt viewport was left disabled")
                self.assertFalse(table.signalsBlocked(), "real Qt table was left with signals blocked")
                paints_before = table.paints
                table.viewport().repaint()
                self.assertGreater(table.paints, paints_before, "real Qt table stopped repainting")
            self.assertEqual(result, "", result)
        finally:
            if table is not None:
                table.deleteLater()
                self.app.processEvents()


for case in CASES:
    setattr(RefreshPaintingRegression, "test_" + case,
            lambda self, name=case: self.run_case(name))

if __name__ == "__main__":
    unittest.main(verbosity=2)
