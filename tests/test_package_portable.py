import importlib.util
from pathlib import Path
import tempfile
import unittest
from zipfile import ZipFile

spec = importlib.util.spec_from_file_location(
    "package_portable", Path(__file__).resolve().parents[1] / "script/package_portable.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PortableArchiveTest(unittest.TestCase):
    def test_layout_and_source_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "windows64"
            (source / "пустая папка").mkdir(parents=True)
            (source / "nekobox.exe").write_bytes(b"fixture")
            output = root / "portable.zip"
            module.make_portable_archive(source, output)
            with ZipFile(output) as archive:
                self.assertEqual(set(archive.namelist()), {
                    "nekobox/", "nekobox/nekobox.exe", "nekobox/пустая папка/"})
                self.assertEqual(archive.read("nekobox/nekobox.exe"), b"fixture")
                self.assertIsNone(archive.testzip())
            self.assertTrue((source / "nekobox.exe").is_file())

    def test_output_inside_source_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):
                module.make_portable_archive(directory, Path(directory) / "recursive.zip")
