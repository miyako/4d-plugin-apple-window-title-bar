"""Compile actual first-party sources and the complete unmodified bundled SDK."""
import os
from pathlib import Path
import platform
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
BASELINE = "17dcef22f23617798eef46f910d8b8c3a3c282d7"


@unittest.skipUnless(platform.system() == "Darwin", "native Cocoa shim requires macOS")
class OwnershipTests(unittest.TestCase):
    def test_actual_sources_and_immutable_baseline(self):
        for name in ("4DPluginAPI.c", "4DPluginAPI.h", "PublicTypes.h",
                     "PrivateTypes.h", "EntryPoints.h", "Flags.h"):
            path = f"4D Plugin API/{name}"
            self.assertEqual((ROOT / path).read_bytes(), subprocess.check_output(
                ["git", "show", f"{BASELINE}:{path}"], cwd=ROOT), path)
        with tempfile.TemporaryDirectory(prefix=".ownership-tests-", dir=ROOT) as directory:
            output = Path(directory)
            baseline = output / "baseline.cpp"
            baseline.write_bytes(subprocess.check_output(
                ["git", "show", f"{BASELINE}:support/4DPlugin-JSON.cpp"], cwd=ROOT))
            common = [
                "clang++", "-std=c++11", "-x", "objective-c++",
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                "-Wno-deprecated-declarations", "-Wno-multichar",
                "-I", str(ROOT / "4D Plugin API"),
                "-I", str(ROOT / "4D Plugin API/Classes"),
                "-I", str(ROOT / "support"),
                str(ROOT / ".github/tests/ownership.cpp"),
                str(ROOT / "4D Plugin API/4DPluginAPI.c"),
                str(ROOT / "4DPlugin-Apple-window-title-bar.cpp"),
                *[str(ROOT / "4D Plugin API/Classes" / name) for name in
                  ("ARRAY_TEXT.cpp", "C_TEXT.cpp", "C_BLOB.cpp", "C_LONGINT.cpp")],
                "-framework", "Cocoa", "-framework", "CoreGraphics",
            ]
            env = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0",
                   "UBSAN_OPTIONS": "halt_on_error=1"}
            for label, source in (
                ("baseline", baseline), ("fixed", ROOT / "support/4DPlugin-JSON.cpp")
            ):
                executable = output / label
                built = subprocess.run(
                    [*common, str(source), "-o", str(executable)], cwd=ROOT,
                    text=True, capture_output=True, timeout=120)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                for empty in ("allocated", "null"):
                    for mode in ("setters", "getters", "arrays", "stringify"):
                        with self.subTest(source=label, mode=mode, empty=empty):
                            result = subprocess.run(
                                [str(executable), mode, empty], env=env,
                                text=True, capture_output=True, timeout=30)
                            expected = None
                            if label == "baseline" and mode == "setters":
                                expected = ("overwritten allocated empty string leak"
                                            if empty == "allocated" else "overwritten empty picture leak")
                            if label == "baseline" and mode == "getters":
                                expected = "owning string getter result leak"
                            if expected:
                                self.assertNotEqual(result.returncode, 0)
                                self.assertIn(expected, result.stderr)
                            else:
                                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                                self.assertIn("strings=0 handles=0 refs=0", result.stdout)


if __name__ == "__main__":
    unittest.main()
