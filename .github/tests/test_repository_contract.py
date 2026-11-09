"""Apple window title bar preservation evidence, valid before and after a coordinated source bump."""

import importlib.util
from pathlib import Path
import re
import stat
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
BASE = "e3c8be9dc513ccecdd1363188d2bd01cf870b39c"
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location(
    "contract_source", ROOT / ".github/scripts/source_version.py"
)
source = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(source)


def original(path):
    return subprocess.check_output(["git", "-C", str(ROOT), "show", f"{BASE}:{path}"])


class RepositoryContractTests(unittest.TestCase):
    def test_existing_coordinated_contract_and_original_trigger(self):
        bump = original(".github/workflows/bump-version.yml").decode()
        header, body = bump.split("name: Bump version", 1)
        self.assertIn("MARKETING_VERSION/CURRENT_PROJECT_VERSION AND Info.plist", header)
        self.assertIn("CFBundleShortVersionString/CFBundleVersion", header)
        self.assertIn('KEY="MARKETING_VERSION"', body)
        self.assertIn('KEY="CURRENT_PROJECT_VERSION"', body)
        release = original(".github/workflows/release.yml").decode()
        self.assertIn("  push:\n    tags:\n      - 'v*.*.*'", release)
        self.assertNotIn("pull_request:", release)
        self.assertNotIn("branches:", release)

    def test_source_has_only_authorized_version_token_changes(self):
        current = source.read(ROOT)
        self.assertEqual(source.SETTING_COUNT, 6)
        for path in source.SOURCE_PATHS:
            with self.subTest(path=path):
                before = original(path)
                if path == source.LOCALIZED:
                    expected = before.replace(
                        b'CFBundleShortVersionString = "1.1.0";',
                        f'CFBundleShortVersionString = "{current}";'.encode(),
                    ).replace(
                        b"Apple window title bar version 1.1.0, Copyright 2021 miyako.",
                        f"Apple window title bar version {current}, Copyright 2021 miyako.".encode(),
                    )
                    self.assertNotIn(b"\n", before)
                    self.assertIn(b"\r", before)
                elif path == source.PROJECT:
                    expected = before.replace(
                        b"MARKETING_VERSION = 1.1.2;",
                        f"MARKETING_VERSION = {current};".encode(),
                    ).replace(
                        b"CURRENT_PROJECT_VERSION = 1.1.0;",
                        f"CURRENT_PROJECT_VERSION = {current};".encode(),
                    )
                else:
                    expected = before
                self.assertEqual((ROOT / path).read_bytes(), expected)
                mode = 0o644
                self.assertEqual(stat.S_IMODE((ROOT / path).stat().st_mode), mode)
        self.assertNotIn(b"<key>CFBundleVersion</key>", (ROOT / source.PLIST).read_bytes())

    def test_all_unrelated_original_files_preserved(self):
        paths = subprocess.check_output(
            ["git", "-C", str(ROOT), "ls-tree", "-r", "--name-only", BASE], text=True,
        ).splitlines()
        allowed = {*map(str, source.SOURCE_PATHS), "README.md",
                   ".github/workflows/bump-version.yml", ".github/workflows/release.yml"}
        for name in paths:
            if name in allowed:
                continue
            with self.subTest(path=name):
                self.assertEqual((ROOT / name).read_bytes(), original(name))
                entry = subprocess.check_output(
                    ["git", "-C", str(ROOT), "ls-tree", BASE, "--", name], text=True,
                )
                mode = int(entry.split()[0], 8) & 0o777
                self.assertEqual(stat.S_IMODE((ROOT / name).stat().st_mode), mode)

    def test_unmodified_native_steps_and_secret_names(self):
        before = original(".github/workflows/release.yml").decode()
        after = (ROOT / ".github/workflows/release.yml").read_text()

        def block(workflow, name):
            return workflow.split(f"      - name: {name}\n", 1)[1].split(
                "\n      - name:", 1
            )[0].rstrip()

        for name in ("import Developer ID certificate", "store notarytool credentials"):
            with self.subTest(step=name):
                self.assertEqual(block(before, name), block(after, name))
        secrets = r"\$\{\{ secrets\.([A-Z_]+) \}\}"
        self.assertEqual(re.findall(secrets, before), re.findall(secrets, after))
        for required in (
            'xcrun stapler staple "$DMG_PATH"', 'ARCHS="arm64 x86_64"',
            "-configuration Default",
            "MACOS_DEPLOYMENT_TARGET: '11.0'", "ONLY_ACTIVE_ARCH=NO",
            'ditto -c -k --keepParent "$BUNDLE_PATH" "$ZIP_PATH"',
            'hdiutil create -format UDBZ -volname "${{ env.PRODUCT_NAME }}" -srcfolder "$BUNDLE_PATH" "$DMG_PATH"',
        ):
            self.assertIn(required, before)
            self.assertIn(required, after)
        for required in ('notarize "$ZIP_PATH"', 'notarize "$DMG_PATH"',
                         'if [ "$STATUS" != "Accepted" ]; then'):
            self.assertIn(required, after)
        for required in ("CODE_SIGNING_ALLOWED=NO", "PlatformToolset=v143",
                         "build-windows:", "download-artifact@"):
            self.assertNotIn(required, after)
        self.assertIn('CONFIGURATION_BUILD_DIR="$PWD/$PLUGINS_DIR"', after)
        for required in (
            'codesign --verbose --deep --timestamp --force --options=runtime',
            'codesign --verify --deep --strict --verbose=2 "$BUNDLE_PATH"',
        ):
            self.assertIn(required, before)
            self.assertIn(required, after)


if __name__ == "__main__":
    unittest.main()
