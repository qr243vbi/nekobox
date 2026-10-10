#!/usr/bin/env python3
"""Source-linked real-Qt diagnostic for the profile editor's ECH bindings.

Executes a narrow translation of source bindings with real Qt Python widgets and
modal input dialogs. It does NOT compile or execute NekoBox's C++ editor, build
its complete .ui, or use profile storage. See README.md for the exact boundary.
"""
import argparse
import importlib
import os
from pathlib import Path
import re
import subprocess
import sys
from types import SimpleNamespace
import unittest
import xml.etree.ElementTree as ET

EDITOR = "src/gharqad/ui/profile/dialog_edit_profile.cpp"
UI = "src/nekobox/ui/profile/dialog_edit_profile.ui"
MODEL = "src/nekobox/configs/proxy/V2RayStreamSettings.hpp"
ROOT = Path(__file__).resolve().parents[2]


def function(source, signature):
    if source.count(signature) != 1:
        raise ValueError(f"Expected one function: {signature}")
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth and end < len(source):
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    if depth:
        raise ValueError(f"Unterminated function: {signature}")
    return source[brace + 1:end - 1]


class Bindings:
    """Read binding destinations/values from source; never assume the fix exists."""
    def __init__(self, source, ui_xml, model):
        self.load = function(source, "void DialogEditProfile::typeSelected(")
        self.save = function(source, "bool DialogEditProfile::onEnd()")
        self.slots = {}
        # Fail closed if either small handler changes beyond this understood
        # grammar. The source decides the initial field, acceptance guard,
        # assignment target and whether a successful edit refreshes the cache.
        grammar = r'''\s*bool\s+ok;\s*
            auto\s+txt\s*=\s*QInputDialog::getMultiLineText\(this,\s*tr\("([^"]+)"\),\s*"",
            \s*CACHE\.(\w+),\s*&ok\);\s*
            if\s*\((!?ok)\)\s*\{\s*CACHE\.(\w+)\s*=\s*txt;\s*
            editor_cache_updated_impl\(\);\s*\}\s*'''
        for name in ("ech_config", "certificate"):
            body = function(source, f"void DialogEditProfile::on_{name}_edit_clicked()")
            match = re.fullmatch(grammar, body, re.VERBOSE)
            if not match:
                raise ValueError(f"Unsupported {name} handler; review the adapter")
            self.slots[name] = match.groups()
        self.defaults = {}
        for name, kind in (("ech_config", "QString"), ("query_server_name", "QString"),
                           ("enable_ech", "bool")):
            match = re.search(rf"\b{kind} {name} = (\"\"|false|true);", model)
            if not match:
                raise ValueError(f"Unsupported model default: {name}")
            self.defaults[name] = {"\"\"": "", "true": True, "false": False}[match[1]]
        widget = ET.fromstring(ui_xml).find(".//widget[@name='query_server_name']")
        if widget is None or widget.get("class") != "QLineEdit":
            raise ValueError("Expected the existing QSN QLineEdit")
        self.qsn_properties = {p.get("name"): p.findtext("bool") for p in widget.findall("property")}
        # This is the existing Naive policy, not a new policy for the fixture.
        visibility = re.search(r'auto is_not_naive = \(type != "naive"\);\s*'
                               r'if\s*\(security_visible\)\s*\{([^{}]*)\}', self.load)
        if not visibility:
            raise ValueError("Naive visibility policy changed; review the adapter")
        self.visibility = re.findall(r"ui->(\w+)->setVisible\(is_not_naive\);", visibility[1])
        # Keep the narrow bridge honest when a new, unsupported use is added.
        allowed_qsn = (
            r"ui->query_server_name->setText\(stream->\w+\);",
            r"ui->query_server_name->setVisible\(is_not_naive\);",
            r"stream->\w+ = ui->query_server_name->text\(\);",
        )
        for line in (self.load + self.save).splitlines():
            if "query_server_name" in line and "label_query_server_name" not in line:
                if not any(re.fullmatch(p, line.strip()) for p in allowed_qsn):
                    raise ValueError(f"Unsupported QSN binding: {line.strip()}")


class EditorBridge:
    """Only the relevant widget/cache boundary; no app or persistence stand-ins."""
    def __init__(self, stream, kind="vless"):
        self.stream = stream
        self.parent = QtWidgets.QDialog()
        self.widgets = {
            "query_server_name": QtWidgets.QLineEdit(self.parent),
            "enable_ech": QtWidgets.QCheckBox(self.parent),
            "insecure": QtWidgets.QCheckBox(self.parent),
        }
        self.widgets["query_server_name"].setReadOnly(BINDINGS.qsn_properties.get("readOnly") == "true")
        self.widgets["query_server_name"].setEnabled(BINDINGS.qsn_properties.get("enabled") != "false")
        self.cache = SimpleNamespace(certificate="", ech_config="")
        self.refreshes = 0
        for widget, setter, field in re.findall(
                r"ui->(\w+)->(setText|setChecked)\(stream->(\w+)\);", BINDINGS.load):
            if widget in self.widgets:
                getattr(self.widgets[widget], setter)(getattr(stream, field))
        for target, field in re.findall(r"CACHE\.(\w+) = stream->(\w+);", BINDINGS.load):
            if hasattr(self.cache, target):
                setattr(self.cache, target, getattr(stream, field))
        for name in BINDINGS.visibility:
            if name in self.widgets:
                self.widgets[name].setVisible(kind != "naive")

    def save(self):
        for target, widget, getter in re.findall(
                r"stream->(\w+) = ui->(\w+)->(text|isChecked)\(\);", BINDINGS.save):
            if widget in self.widgets:
                setattr(self.stream, target, getattr(self.widgets[widget], getter)())
        for target, field in re.findall(r"stream->(\w+) = CACHE\.(\w+);", BINDINGS.save):
            if hasattr(self.cache, field):
                setattr(self.stream, target, getattr(self.cache, field))

    def edit(self, name, accept=True, replacement=None):
        title, initial_field, guard, target = BINDINGS.slots[name]
        initial_seen, errors = [], []

        def interact():
            try:
                dialog = APP.activeModalWidget()
                if not isinstance(dialog, QtWidgets.QInputDialog):
                    raise AssertionError("The real Qt input dialog was not active")
                initial_seen.append(dialog.textValue())
                if replacement is not None:
                    dialog.setTextValue(replacement)
                dialog.accept() if accept else dialog.reject()
            except Exception as error:
                errors.append(error)
                if APP.activeModalWidget():
                    APP.activeModalWidget().reject()

        timer = QtCore.QTimer()
        timer.setSingleShot(True)
        timer.timeout.connect(interact)
        timer.start(0)
        value, ok = QtWidgets.QInputDialog.getMultiLineText(
            self.parent, title, "", getattr(self.cache, initial_field))
        timer.stop()
        if errors:
            raise errors[0]
        if ok if guard == "ok" else not ok:
            setattr(self.cache, target, value)
            self.refreshes += 1
        if len(initial_seen) != 1:
            raise AssertionError("Qt dialog interaction did not run exactly once")
        return initial_seen[0]

    def close(self):
        self.parent.close()
        self.parent.deleteLater()
        APP.processEvents()


def stream(**values):
    data = dict(certificate="CERTIFICATE-SENTINEL", allow_insecure=False,
                **BINDINGS.defaults)
    data.update(values)
    return SimpleNamespace(**data)


class ECHBindings(unittest.TestCase):
    def editor(self, value=None, kind="vless"):
        result = EditorBridge(value or stream(), kind)
        self.addCleanup(result.close)
        return result

    def test_ech_opens_own_multiline_value_and_accepts_unchanged(self):
        value = stream(ech_config="ECH-CONFIG-LINE-1\nECH-CONFIG-LINE-2")
        editor = self.editor(value)
        self.assertEqual(editor.edit("ech_config"), value.ech_config)
        self.assertEqual(editor.cache.ech_config, value.ech_config)
        self.assertEqual(editor.cache.certificate, value.certificate)
        editor.save()
        self.assertEqual(value.ech_config, "ECH-CONFIG-LINE-1\nECH-CONFIG-LINE-2")
        self.assertEqual(self.editor(value).edit("ech_config"), value.ech_config)

    def test_ech_cancel_preserves_cache_and_does_not_refresh(self):
        editor = self.editor(stream(ech_config="ECH-ORIGINAL"))
        editor.edit("ech_config", accept=False, replacement="ECH-CANCELLED")
        self.assertEqual(editor.cache.ech_config, "ECH-ORIGINAL")
        self.assertEqual(editor.cache.certificate, "CERTIFICATE-SENTINEL")
        self.assertEqual(editor.refreshes, 0)

    def test_ech_edit_and_clear_leave_certificate_unchanged(self):
        value = stream(ech_config="ECH-ORIGINAL", enable_ech=True)
        editor = self.editor(value)
        for text in ("ECH-EDITED\nSECOND-LINE", ""):
            editor.edit("ech_config", replacement=text)
            editor.save()
            self.assertEqual(value.ech_config, text)
            self.assertEqual(value.certificate, "CERTIFICATE-SENTINEL")
            self.assertTrue(value.enable_ech)
            self.assertFalse(value.allow_insecure)
        self.assertEqual(editor.refreshes, 2)

    def test_certificate_edit_uses_certificate_and_leaves_ech_unchanged(self):
        editor = self.editor(stream(ech_config="ECH-SENTINEL"))
        self.assertEqual(editor.edit("certificate", replacement="CERT-EDITED"), "CERTIFICATE-SENTINEL")
        self.assertEqual(editor.cache.certificate, "CERT-EDITED")
        self.assertEqual(editor.cache.ech_config, "ECH-SENTINEL")

    def test_qsn_load_and_unchanged_save_round_trip(self):
        value = stream(query_server_name="query.example.invalid")
        editor = self.editor(value)
        self.assertEqual(editor.widgets["query_server_name"].text(), value.query_server_name)
        self.assertFalse(editor.widgets["query_server_name"].isReadOnly())
        self.assertTrue(editor.widgets["query_server_name"].isEnabled())
        editor.save()
        self.assertEqual(value.query_server_name, "query.example.invalid")

    def test_qsn_edit_and_clear_round_trip(self):
        value = stream(query_server_name="old.example.invalid")
        for text in ("new.example.invalid", ""):
            editor = self.editor(value)
            editor.widgets["query_server_name"].setText(text)
            editor.save()
            self.assertEqual(value.query_server_name, text)
            self.assertEqual(self.editor(value).widgets["query_server_name"].text(), text)

    def test_naive_hidden_qsn_survives_unchanged_save(self):
        value = stream(query_server_name="hidden.example.invalid", enable_ech=True)
        editor = self.editor(value, kind="naive")
        self.assertTrue(editor.widgets["query_server_name"].isHidden())
        self.assertEqual(editor.widgets["query_server_name"].text(), value.query_server_name)
        editor.save()
        self.assertEqual(value.query_server_name, "hidden.example.invalid")
        self.assertTrue(value.enable_ech)

    def test_disabled_ech_keeps_query_and_does_not_weaken_tls(self):
        value = stream(query_server_name="disabled.example.invalid", ech_config="ECH-DISABLED")
        editor = self.editor(value)
        editor.edit("ech_config")
        editor.save()
        self.assertFalse(value.enable_ech)
        self.assertFalse(value.allow_insecure)
        self.assertEqual(value.query_server_name, "disabled.example.invalid")
        self.assertEqual(value.ech_config, "ECH-DISABLED")

    def test_empty_defaults_do_not_copy_certificate_into_ech(self):
        value = stream()
        editor = self.editor(value)
        self.assertEqual(editor.edit("ech_config"), "")
        self.assertEqual(editor.widgets["query_server_name"].text(), "")
        editor.save()
        self.assertEqual(value.ech_config, "")
        self.assertEqual(value.query_server_name, "")
        self.assertFalse(value.enable_ech)
        self.assertFalse(value.allow_insecure)

    def test_query_only_ech_preserves_existing_enable_and_tls_flags(self):
        for enabled in (False, True):
            for insecure in (False, True):
                with self.subTest(enabled=enabled, insecure=insecure):
                    value = stream(query_server_name="query-only.example.invalid",
                                   enable_ech=enabled, allow_insecure=insecure)
                    editor = self.editor(value)
                    self.assertEqual(editor.widgets["enable_ech"].isChecked(), enabled)
                    self.assertEqual(editor.widgets["insecure"].isChecked(), insecure)
                    editor.save()
                    self.assertEqual(value.query_server_name, "query-only.example.invalid")
                    self.assertEqual(value.ech_config, "")
                    self.assertEqual(value.enable_ech, enabled)
                    self.assertEqual(value.allow_insecure, insecure)


def main():
    global QtCore, QtWidgets, APP, BINDINGS
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=ROOT)
    parser.add_argument("--ref", help="Read this immutable revision instead of working files")
    args, remaining = parser.parse_known_args()

    def read(path):
        if args.ref:
            return subprocess.check_output(["git", "-C", str(args.repo), "show",
                                            f"{args.ref}:{path}"], text=True)
        return (args.repo / path).read_text()

    BINDINGS = Bindings(read(EDITOR), read(UI), read(MODEL))
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    for binding in ("PySide6", "PyQt6", "PySide2", "PyQt5"):
        try:
            QtCore = importlib.import_module(binding + ".QtCore")
            QtWidgets = importlib.import_module(binding + ".QtWidgets")
            break
        except ImportError:
            continue
    else:
        raise SystemExit("No Qt Python binding installed; nothing tested")
    APP = QtWidgets.QApplication([])
    print(f"SOURCE: {args.ref or 'working files'} in {args.repo}", flush=True)
    print(f"MODE: source-linked Python bridge, real {binding}/Qt {QtCore.qVersion()}; NOT C++ execution", flush=True)
    unittest.main(argv=[sys.argv[0]] + remaining)


if __name__ == "__main__":
    main()
