"""Package deployed Windows files without renaming their directory."""
import argparse
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


def make_portable_archive(source, output):
    source, output = Path(source).resolve(), Path(output).resolve()
    if not source.is_dir():
        raise ValueError(f"Deployment directory does not exist: {source}")
    if output.is_relative_to(source):
        raise ValueError("Archive must be outside the deployment directory")
    with ZipFile(output, "w", ZIP_DEFLATED, compresslevel=9) as archive:
        archive.write(source, "nekobox/")
        for path in sorted(source.rglob("*")):
            archive.write(path, "nekobox/" + path.relative_to(source).as_posix())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source")
    parser.add_argument("output")
    args = parser.parse_args()
    make_portable_archive(args.source, args.output)
