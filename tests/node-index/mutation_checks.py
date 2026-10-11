#!/usr/bin/env python3
"""Verify the recursion guard and runtime checks detect independent overload defects."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from test_node_index import function

HERE = Path(__file__).resolve().parent
FILES = (
    "src/gharqad/dataStore/ConfigData.cpp",
    "src/gharqad/dataStore/ConfigItem.cpp",
    "src/nekobox/dataStore/Configs.hpp",
)
CONST = "const Node & Node::at(const EnumFieldName& t) const"
MUTABLE = "Node & Node::at(const EnumFieldName& t) {"
FORWARD = "return this->at(QList<EnumFieldName>{t});"


def replace_body(text, signature, statement):
    original = function(text, signature)
    if original.count(FORWARD) != 1:
        raise ValueError(f"Expected corrected forwarding body: {signature}")
    return text.replace(original, original.replace(FORWARD, statement), 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=HERE.parents[1])
    parser.add_argument("--qt-root", type=Path, default=os.environ.get("QT6_ROOT"))
    args = parser.parse_args()
    pristine = {path: (args.repo / path).read_text() for path in FILES}
    source = pristine[FILES[0]]
    mutations = [
        ("both single-key overloads recurse", (CONST, MUTABLE), "return this->at({t});", "compiler"),
        ("only const single-key overload recurses", (CONST,), "return this->at({t});", "compiler"),
        ("only mutable single-key overload recurses", (MUTABLE,), "return this->at({t});", "compiler"),
        ("const overload discards requested key", (CONST,), 'return this->at(QList<EnumFieldName>{EnumFieldName("wrong-key")});', "runtime"),
        ("mutable overload discards requested key", (MUTABLE,), 'return this->at(QList<EnumFieldName>{EnumFieldName("wrong-key")});', "runtime"),
        ("const overload selects numeric access", (CONST,), "return this->at(size_t(0));", "runtime"),
        ("mutable overload selects numeric access", (MUTABLE,), "return this->at(size_t(0));", "runtime"),
    ]
    command = [sys.executable, str(HERE / "test_node_index.py")]
    if args.qt_root:
        command += ["--qt-root", str(args.qt_root)]
    # Mutations discard t intentionally. Silence that test-created warning only;
    # otherwise compiler rejection could be mistaken for a runtime assertion failure.
    env = os.environ.copy()
    env["EXTRA_CXXFLAGS"] = env.get("EXTRA_CXXFLAGS", "") + " -Wno-unused-parameter"
    with tempfile.TemporaryDirectory(prefix="nekobox-node-index-mutations-") as td:
        root = Path(td)
        for path, contents in pristine.items():
            dest = root / path
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text(contents)
        run = lambda: subprocess.run(command + ["--repo", str(root)], env=env, capture_output=True, text=True, timeout=90)
        control = run()
        print("CONTROL:\n" + control.stdout + control.stderr, end="", flush=True)
        if control.returncode != 0 or "149/149 checks passed" not in control.stdout:
            print("ERROR: unmutated control must pass before mutation scoring.")
            return 2
        detected = 0
        for name, signatures, replacement, mode in mutations:
            mutated = source
            for signature in signatures:
                mutated = replace_body(mutated, signature, replacement)
            (root / FILES[0]).write_text(mutated)
            result = run()
            output = result.stdout + result.stderr
            if mode == "compiler":
                caught = (result.returncode == 1 and "FAIL: compiler diagnosed infinite recursion" in output
                          and output.count("error: infinite recursion detected") == len(signatures)
                          and "RESULT:" not in output and "ERROR:" not in output)
            else:
                caught = result.returncode == 1 and "RESULT:" in output and "FAIL: " in output and "ERROR:" not in output
            print(("DETECTED" if caught else "MISSED/ERROR") + f" ({mode}): {name}", flush=True)
            if not caught:
                print(output, end="", flush=True)
            detected += caught
    print(f"RESULT: {detected}/{len(mutations)} mutations detected", flush=True)
    return 0 if detected == len(mutations) else 1


if __name__ == "__main__":
    raise SystemExit(main())
