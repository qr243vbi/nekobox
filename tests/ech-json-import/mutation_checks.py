#!/usr/bin/env python3
"""Check focused ECH regressions using temporary source mutations only."""
from pathlib import Path
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
PROXY = "src/gharqad/configs/proxy/"
IMPORT = PROXY + "Json2Bean.cpp"
mutations = [
    ("wrong boolean accessor", IMPORT, "enabled.getBoolean()", "enabled.toBool()"),
    ("legacy overrides malformed nested", IMPORT, 'tls.contains("ech")', 'tls["ech"].isObject()'),
    ("enable when omitted", IMPORT, "enabled.getBoolean()", "enabled.getBoolean(true)"),
    ("ignore explicit false", IMPORT, "stream->enable_ech = enable_ech;", "stream->enable_ech = true; (void)enable_ech;"),
    ("lose nested config", IMPORT, "stream->ech_config = ech_config;", 'stream->ech_config = "";'),
    ("lose nested query name", IMPORT, "stream->query_server_name = query_server_name.getString();", 'stream->query_server_name = "";'),
    ("accept mixed config array", IMPORT, "if (!line.isString()) return false;", "(void)line;"),
]
for bean in ("AnyTLS", "Http", "Juicity", "Naive", "ShadowTLS", "TrojanVLESS", "TrustTunnel", "VMess"):
    mutations.append((f"ignore {bean} JSON rejection", PROXY + bean + "Bean.cpp", "if (!add_tls(stream, obj)) return false;", "add_tls(stream, obj);"))
# TrustTunnel has both JSON and YAML guards. Revert just the second occurrence.
mutations.append(("ignore TrustTunnel YAML rejection", PROXY + "TrustTunnelBean.cpp", "if (!add_tls(stream, obj)) return false;", "add_tls(stream, obj);"))


def main():
    for name, path, old, new in mutations:
        with tempfile.TemporaryDirectory(prefix="nekobox-ech-mutation-") as td:
            root = Path(td)
            files = list((ROOT / PROXY).glob("*.cpp")) + [
                ROOT / "src/gharqad/dataStore/ConfigData.cpp",
                ROOT / "src/gharqad/configs/sub/GroupUpdater.cpp",
                ROOT / "src/nekobox/configs/proxy/V2RayStreamSettings.hpp",
            ]
            for source in files:
                target = root / source.relative_to(ROOT)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)
            target = root / path
            source = target.read_text()
            if name == "ignore TrustTunnel YAML rejection":
                begin = source.index("bool TrustTunnelBean::TryParseYaml(")
                source = source[:begin] + source[begin:].replace(old, new, 1)
            else:
                if old not in source:
                    raise AssertionError(f"Missing mutation anchor: {name}")
                source = source.replace(old, new, 1)
            target.write_text(source)
            result = subprocess.run(["python3", str(HERE / "test_ech_json_import.py"), "--repo", str(root)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
            if result.returncode != 1 or " checks; " not in result.stdout or "FAIL:" not in result.stdout:
                print(result.stdout)
                raise SystemExit(f"Mutation was not detected by runtime assertions: {name}")
            print(name + ": " + result.stdout.splitlines()[-1], flush=True)
    print(f"PASS: all {len(mutations)} production-source mutations detected")


if __name__ == "__main__":
    main()
