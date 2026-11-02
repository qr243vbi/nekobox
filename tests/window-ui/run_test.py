#!/usr/bin/env python3
"""Compile production window helpers and the real UI layout with Qt Widgets.
The custom animated status icon is replaced by QToolButton; no network/core
or user settings are used. Can also run a prebuilt MSVC test executable.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]

def prepare_ui(root):
    tree = ET.parse(ROOT/'src/nekobox/ui/mainwindow.ui')
    for widget in tree.getroot().iter('widget'):
        if widget.get('class') == 'StatusControlButton': widget.set('class', 'QToolButton')
    custom = tree.getroot().find('customwidgets')
    if custom is not None: tree.getroot().remove(custom)
    tree.write(root/'mainwindow.ui', encoding='utf-8', xml_declaration=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path)
    parser.add_argument('--uic', default='uic')
    parser.add_argument('--prepare-ui', type=Path)
    args = parser.parse_args()
    if args.prepare_ui:
        args.prepare_ui.mkdir(parents=True, exist_ok=True)
        prepare_ui(args.prepare_ui)
        return
    env = dict(os.environ, QT_QPA_PLATFORM='offscreen')
    if args.binary:
        subprocess.run([str(args.binary.resolve())], check=True, env=env, timeout=45)
        return
    with tempfile.TemporaryDirectory(prefix='nekobox-window-ui-') as directory:
        root = Path(directory)
        prepare_ui(root)
        subprocess.run([args.uic, str(root/'mainwindow.ui'), '-o', str(root/'ui_mainwindow.h')], check=True)
        flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Widgets'], text=True))
        exe = root/'ui-test'
        subprocess.run([os.environ.get('CXX','g++'), '-std=c++17', '-fPIC',
            '-I'+str(ROOT/'src'), '-I'+str(root), str(ROOT/'tests/window-ui/test_ui.cpp'),
            '-o', str(exe), *flags], check=True)
        subprocess.run([str(exe)], check=True, env=env, timeout=45)

if __name__ == '__main__': main()
