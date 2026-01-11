#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import argparse
import fileinput


PKG_ORIGINAL = "github.com/quic-go/quic-go"
PKG_NEW = "github.com/sagernet/quic-go"

EXTENSIONS = [".go", ".mod"]

parser = argparse.ArgumentParser()
parser.add_argument("-r", "--reverse", action="store_true")
args = parser.parse_args()


def replace_line(line):
    if args.reverse:
        return line.replace(PKG_NEW, PKG_ORIGINAL)
    return line.replace(PKG_ORIGINAL, PKG_NEW)


for dirpath, dirnames, filenames in os.walk("."):
    # Skip hidden directories like .git
    dirnames[:] = [d for d in dirnames if not d[0] == "."]
    filenames = [f for f in filenames if os.path.splitext(f)[1] in EXTENSIONS]
    for filename in filenames:
        file_path = os.path.join(dirpath, filename)
        with fileinput.FileInput(file_path, inplace=True) as file:
            for line in file:
                print(replace_line(line), end="")
