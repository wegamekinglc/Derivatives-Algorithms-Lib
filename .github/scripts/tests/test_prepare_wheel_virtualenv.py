"""Contracts for verified cibuildwheel bootstrap acquisition."""

import hashlib
import importlib.util
import io
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import urllib.error
import ssl


SCRIPT = Path(__file__).resolve().parents[1] / "prepare_wheel_virtualenv.py"
SPEC = importlib.util.spec_from_file_location("prepare_wheel_virtualenv", SCRIPT)
PREPARE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PREPARE)
PAYLOAD = b"verified virtualenv test bytes"
DIGEST = hashlib.sha256(PAYLOAD).hexdigest()


def http_error(code):
    return urllib.error.HTTPError(PREPARE.URLS[0], code, "test failure", {}, None)


class WheelVirtualenvTest(unittest.TestCase):
    def setUp(self):
        sleeper = patch.object(PREPARE.time, "sleep")
        self.sleep = sleeper.start()
        self.addCleanup(sleeper.stop)

    def test_transient_504_uses_same_asset_route_and_commits_verified_bytes(self):
        with tempfile.TemporaryDirectory() as temporary, patch.object(PREPARE, "VIRTUALENV_SHA256", DIGEST), patch.object(
            PREPARE.urllib.request, "urlopen", side_effect=[http_error(504), io.BytesIO(PAYLOAD)]
        ) as download:
            target = PREPARE.prepare(Path(temporary))
            self.assertEqual(target.name, "virtualenv-21.14.13.pyz")
            self.assertEqual(target.read_bytes(), PAYLOAD)
            self.assertEqual([call.args[0].full_url for call in download.call_args_list], list(PREPARE.URLS))
            self.assertEqual(list(Path(temporary).iterdir()), [target])
            self.sleep.assert_called_once_with(3)
            self.assertEqual(download.call_args.kwargs["timeout"], PREPARE.TIMEOUT)
            self.assertEqual(download.call_args.args[0].get_header("Accept"), "application/octet-stream")

    def test_exhausted_transient_failures_propagate_without_a_cache_entry(self):
        errors = [http_error(504) for _ in range(PREPARE.ATTEMPTS)]
        with tempfile.TemporaryDirectory() as temporary, patch.object(
            PREPARE.urllib.request, "urlopen", side_effect=errors
        ) as download:
            with self.assertRaises(urllib.error.HTTPError) as raised:
                PREPARE.prepare(Path(temporary))
            self.assertIs(raised.exception, errors[-1])
            self.assertEqual(download.call_count, PREPARE.ATTEMPTS)
            self.assertEqual([call.args[0] for call in self.sleep.call_args_list], [3, 6, 12, 24, 48])
            self.assertEqual(list(Path(temporary).iterdir()), [])

    def test_permanent_failure_is_not_retried(self):
        with tempfile.TemporaryDirectory() as temporary, patch.object(
            PREPARE.urllib.request, "urlopen", side_effect=http_error(404)
        ) as download:
            with self.assertRaises(urllib.error.HTTPError):
                PREPARE.prepare(Path(temporary))
            self.assertEqual(download.call_count, 1)
            self.assertEqual(list(Path(temporary).iterdir()), [])

    def test_bad_checksum_never_installs_or_retries(self):
        with tempfile.TemporaryDirectory() as temporary, patch.object(PREPARE, "VIRTUALENV_SHA256", DIGEST), patch.object(
            PREPARE.urllib.request, "urlopen", return_value=io.BytesIO(b"corrupt")
        ) as download:
            with self.assertRaisesRegex(ValueError, "SHA256 mismatch"):
                PREPARE.prepare(Path(temporary))
            self.assertEqual(download.call_count, 1)
            self.assertEqual(list(Path(temporary).iterdir()), [])

    def test_valid_cache_is_rechecked_and_reused_without_network(self):
        with tempfile.TemporaryDirectory() as temporary, patch.object(PREPARE, "VIRTUALENV_SHA256", DIGEST), patch.object(
            PREPARE.urllib.request, "urlopen"
        ) as download:
            target = Path(temporary) / ("virtualenv-%s.pyz" % PREPARE.VIRTUALENV_VERSION)
            target.write_bytes(PAYLOAD)
            self.assertEqual(PREPARE.prepare(Path(temporary)), target)
            download.assert_not_called()

    def test_corrupt_existing_cache_fails_without_relabeling_or_refetch(self):
        with tempfile.TemporaryDirectory() as temporary, patch.object(PREPARE, "VIRTUALENV_SHA256", DIGEST), patch.object(
            PREPARE.urllib.request, "urlopen"
        ) as download:
            target = Path(temporary) / ("virtualenv-%s.pyz" % PREPARE.VIRTUALENV_VERSION)
            target.write_bytes(b"old corrupt bytes")
            with self.assertRaisesRegex(ValueError, "SHA256 mismatch"):
                PREPARE.prepare(Path(temporary))
            download.assert_not_called()
            self.assertEqual(target.read_bytes(), b"old corrupt bytes")

    def test_socket_timeout_recovers_but_certificate_failure_does_not_retry(self):
        with tempfile.TemporaryDirectory() as temporary, patch.object(PREPARE, "VIRTUALENV_SHA256", DIGEST), patch.object(
            PREPARE.urllib.request, "urlopen", side_effect=[TimeoutError("timed out"), io.BytesIO(PAYLOAD)]
        ):
            self.assertEqual(PREPARE.prepare(Path(temporary)).read_bytes(), PAYLOAD)
        with tempfile.TemporaryDirectory() as temporary, patch.object(
            PREPARE.urllib.request, "urlopen", side_effect=urllib.error.URLError(ssl.SSLCertVerificationError("untrusted"))
        ) as download:
            with self.assertRaises(urllib.error.URLError):
                PREPARE.prepare(Path(temporary))
            self.assertEqual(download.call_count, 1)

    def test_interrupted_body_is_removed_before_retry(self):
        class Interrupted(io.BytesIO):
            def read(self, count=-1):
                chunk = super().read(count)
                if chunk:
                    return chunk
                raise ConnectionResetError("interrupted body")

        with tempfile.TemporaryDirectory() as temporary, patch.object(PREPARE, "VIRTUALENV_SHA256", DIGEST), patch.object(
            PREPARE.urllib.request, "urlopen", side_effect=[Interrupted(b"partial"), io.BytesIO(PAYLOAD)]
        ) as download:
            target = PREPARE.prepare(Path(temporary))
            self.assertEqual(download.call_count, 2)
            self.assertEqual(target.read_bytes(), PAYLOAD)
            self.assertEqual(list(Path(temporary).iterdir()), [target])

    def test_success_exports_exact_cache_to_following_action(self):
        with tempfile.TemporaryDirectory() as temporary, patch.object(PREPARE, "VIRTUALENV_SHA256", DIGEST), patch.object(
            PREPARE.urllib.request, "urlopen", return_value=io.BytesIO(PAYLOAD)
        ):
            root = Path(temporary)
            environment = root / "github.env"
            environment.write_text("EXISTING=preserved\n", encoding="utf-8")
            cache = root / "cache with spaces"
            with patch.dict(os.environ, {"GITHUB_ENV": str(environment)}):
                PREPARE.main(["--cache-dir", str(cache)])
            self.assertEqual(environment.read_text(encoding="utf-8"), "EXISTING=preserved\nCIBW_CACHE_PATH=%s\n" % cache.resolve())

    def test_failed_download_does_not_export_a_cache_or_hide_failure(self):
        with tempfile.TemporaryDirectory() as temporary, patch.object(
            PREPARE.urllib.request, "urlopen", side_effect=http_error(404)
        ):
            environment = Path(temporary) / "github.env"
            environment.write_text("EXISTING=preserved\n", encoding="utf-8")
            with patch.dict(os.environ, {"GITHUB_ENV": str(environment)}), self.assertRaises(urllib.error.HTTPError):
                PREPARE.main(["--cache-dir", str(Path(temporary) / "cache")])
            self.assertEqual(environment.read_text(encoding="utf-8"), "EXISTING=preserved\n")

    def test_both_workflows_prepare_before_the_same_pinned_action(self):
        root = SCRIPT.parents[2]
        for name in ("dal-python-ci.yml", "dal-python-release.yml"):
            text = (root / ".github/workflows" / name).read_text(encoding="utf-8")
            self.assertLess(text.index("python .github/scripts/prepare_wheel_virtualenv.py"), text.index("uses: pypa/cibuildwheel@"))
            self.assertIn("pypa/cibuildwheel@" + PREPARE.CIBUILDWHEEL_REVISION, text)
            preparation = text.split("- name: Prepare pinned wheel virtualenv", 1)[1].split("- name: Build and test wheels", 1)[0]
            self.assertIn("if: runner.os != 'Linux'", preparation)
            self.assertNotIn("continue-on-error", preparation)


if __name__ == "__main__":
    unittest.main()
