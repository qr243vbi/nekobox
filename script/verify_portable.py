"""Validate the delivered portable ZIP without launching the application."""
import hashlib
import struct
from pathlib import Path
import zipfile

archives = list(Path("deployment").glob("*windows64.zip"))
if len(archives) != 1:
    raise SystemExit(f"Expected one Windows x64 ZIP, found {archives}")
archive = archives[0]
with zipfile.ZipFile(archive) as package:
    damaged = package.testzip()
    if damaged:
        raise SystemExit(f"ZIP CRC check failed: {damaged}")
    names = set(package.namelist())
    required = {
        "nekobox/nekobox.exe", "nekobox/nekobox_core.exe", "nekobox/xray.exe",
        "nekobox/Qt6Core.dll", "nekobox/Qt6Gui.dll", "nekobox/Qt6Widgets.dll",
        "nekobox/platforms/qwindows.dll", "nekobox/libcronet.dll",
        "nekobox/srslist.json", "nekobox/public/languages.txt",
        "nekobox/msvcp140.dll", "nekobox/vcruntime140.dll",
        "nekobox/NekoBox-Portable.cmd",
    }
    missing = required - names
    if missing:
        raise SystemExit(f"Portable archive is missing: {sorted(missing)}")
    for name in required:
        if package.getinfo(name).file_size == 0:
            raise SystemExit(f"Empty required file: {name}")
    for name in names:
        if name.lower().endswith((".exe", ".dll")):
            with package.open(name) as binary:
                if binary.read(2) != b"MZ":
                    raise SystemExit(f"Invalid executable header: {name}")
    for name in ("nekobox/nekobox.exe", "nekobox/nekobox_core.exe", "nekobox/xray.exe"):
        with package.open(name) as binary:
            binary.seek(0x3C)
            pe_offset = struct.unpack("<I", binary.read(4))[0]
            binary.seek(pe_offset)
            if binary.read(4) != b"PE\0\0" or binary.read(2) != b"\x64\x86":
                raise SystemExit(f"Expected Windows x64 executable: {name}")
with archive.open("rb") as source:
    digest = hashlib.file_digest(source, "sha256").hexdigest()
archive.with_suffix(archive.suffix + ".sha256").write_text(
    f"{digest}  {archive.name}\n", encoding="ascii"
)
print(f"Verified {archive}: SHA-256 {digest}")
