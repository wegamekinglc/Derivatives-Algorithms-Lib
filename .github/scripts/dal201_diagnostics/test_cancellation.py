import ctypes
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

import diagnostics as diag


def is_live(pid):
    try:
        return Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()[0] != "Z"
    except FileNotFoundError:
        return False


def await_file(path, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if path.exists() and path.read_text().strip():
            return path.read_text()
        time.sleep(0.01)
    raise AssertionError(f"worker did not become ready: {path}")


class CancellationTest(unittest.TestCase):
    def exercise_exit(self, signum=None, *, error=None, repeat_signal=False, timeout=False):
        # Adopt only this test's orphaned descendants so failure paths can reap them.
        libc = ctypes.CDLL(None, use_errno=True)
        previous = ctypes.c_int()
        self.assertEqual(libc.prctl(37, ctypes.byref(previous), 0, 0, 0), 0)
        self.assertEqual(libc.prctl(36, 1, 0, 0, 0), 0)
        parent = sentinel = None
        pids = []
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "output"
            ready = root / "descendant-ready"
            worker_ready = root / "worker-ready"
            cleanup_ready = root / "cleanup-ready"
            cleanup_release = root / "cleanup-release"
            descendant = (
                "import os,signal,time; from pathlib import Path; "
                "signal.signal(signal.SIGINT,signal.SIG_IGN); "
                "signal.signal(signal.SIGTERM,signal.SIG_IGN); "
                f"Path({str(ready)!r}).write_text(str(os.getpid())); time.sleep(60)"
            )
            worker = (
                "import json,os,subprocess,sys,time; from pathlib import Path\n"
                f"Path({str(worker_ready)!r}).write_text(str(os.getpid()))\n"
                f"child=subprocess.Popen([sys.executable,'-c',{descendant!r}])\n"
                f"while not Path({str(ready)!r}).exists(): time.sleep(0.01)\n"
                "print(json.dumps([os.getpid(),child.pid]),flush=True)\n"
                "print('partial stderr',file=sys.stderr,flush=True)\n"
                + ("time.sleep(60)\n" if signum or error or timeout else "")
            )
            driver = (
                "import sys,time; from pathlib import Path\n"
                f"sys.path.insert(0,{str(diag.HERE)!r})\n"
                "import diagnostics as d\n"
                "d.verify_artifact=lambda *args: None\n"
                "d.prepare_packages=lambda *args: None\n"
                f"d.preflight=lambda packages,output: d.require_process(d.run_process([sys.executable,'-c',{worker!r}],output/'worker',timeout={0.3 if timeout else 30}))\n"
                f"sys.argv=['diagnostics','--artifact','.', '--source','.', '--output',{str(output)!r},'--preflight-only']\n"
            )
            if error:
                driver += (
                    "def fail_wait(*args):\n"
                    f"    while not Path({str(output / 'worker/stderr.txt')!r}).read_text(): time.sleep(0.01)\n"
                    f"    raise {error}('injected wait failure')\n"
                    "d.wait_process=fail_wait\n"
                )
            if repeat_signal:
                driver += (
                    "collect=d.collect_process_group\n"
                    "def paused_collect(process):\n"
                    f"    Path({str(cleanup_ready)!r}).write_text('ready')\n"
                    f"    while not Path({str(cleanup_release)!r}).exists(): time.sleep(0.01)\n"
                    "    collect(process)\n"
                    "d.collect_process_group=paused_collect\n"
                )
            driver += "raise SystemExit(d.main())\n"
            try:
                sentinel = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"], start_new_session=True)
                with (root / "parent.log").open("wb") as log:
                    parent = subprocess.Popen([sys.executable, "-c", driver], stdout=log, stderr=log, start_new_session=True)
                    pids = json.loads(await_file(output / "worker/stdout.txt"))
                    start = time.monotonic()
                    if signum:
                        os.killpg(parent.pid, signum)
                    if repeat_signal:
                        await_file(cleanup_ready)
                        os.killpg(parent.pid, signal.SIGINT)
                        cleanup_release.write_text("continue")
                    parent.wait(timeout=7)
                observed = {"signal": signum, "returncode": parent.returncode,
                            "exit_seconds": time.monotonic() - start,
                            "live_pids": [pid for pid in pids if is_live(pid)],
                            "process": json.loads((output / "worker/process.json").read_text()),
                            "summary": json.loads((output / "summary.json").read_text())}
                (root / "observed.json").write_text(json.dumps(observed, indent=2) + "\n")
                self.assertEqual(observed["live_pids"], [], observed)
                self.assertIsNone(sentinel.poll(), "unrelated process was killed")
                interrupted = signum or (signal.SIGINT if error == "KeyboardInterrupt" else None)
                expected = "interrupted" if interrupted else "timeout" if timeout else "failed" if error else "passed"
                self.assertEqual(observed["process"]["status"], expected)
                self.assertEqual(observed["summary"]["status"], "interrupted" if interrupted else "failed" if error or timeout else "preflight-passed")
                self.assertEqual(parent.returncode, 128 + interrupted if interrupted else 1 if error or timeout else 0)
                self.assertIn("partial stderr", (output / "worker/stderr.txt").read_text())
                self.assertEqual(json.loads((output / "worker/stdout.txt").read_text()), pids)
                manifest = json.loads((output / "manifest.json").read_text())["files"]
                self.assertIn("worker/process.json", manifest)
                self.assertIn("summary.json", manifest)
                self.assertEqual(manifest, {path.relative_to(output).as_posix(): diag.sha256(path)
                                          for path in output.rglob("*") if path.is_file() and path.name != "manifest.json"})
                with self.assertRaises(ChildProcessError):
                    os.waitpid(pids[0], os.WNOHANG)
            finally:
                if parent and parent.poll() is None:
                    os.killpg(parent.pid, signal.SIGKILL)
                    parent.wait()
                if worker_ready.exists():
                    group = int(worker_ready.read_text())
                    try:
                        os.killpg(group, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    try:
                        while True:
                            os.waitpid(-group, 0)
                    except ChildProcessError:
                        pass
                if sentinel:
                    sentinel.kill()
                    sentinel.wait()
                self.assertEqual(libc.prctl(36, previous.value, 0, 0, 0), 0)
                if evidence := os.environ.get("DAL201_TEST_EVIDENCE"):
                    shutil.copytree(root, Path(evidence) / self._testMethodName)

    def test_parent_sigint_collects_descendant_and_final_evidence(self):
        self.exercise_exit(signal.SIGINT)

    def test_parent_sigterm_collects_descendant_and_final_evidence(self):
        self.exercise_exit(signal.SIGTERM)

    def test_normal_worker_exit_collects_remaining_descendant(self):
        self.exercise_exit()

    def test_second_signal_during_cleanup_preserves_first_cancellation(self):
        self.exercise_exit(signal.SIGTERM, repeat_signal=True)

    def test_keyboard_interrupt_exception_collects_group(self):
        self.exercise_exit(error="KeyboardInterrupt")

    def test_system_exit_exception_collects_group(self):
        self.exercise_exit(error="SystemExit")

    def test_timeout_collects_descendant_and_final_evidence(self):
        self.exercise_exit(timeout=True)

    def test_cancellation_during_manifest_finishes_with_consistent_hashes(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "output"
            original_manifest = diag.manifest
            def cancel_manifest(path):
                os.kill(os.getpid(), signal.SIGTERM)
                original_manifest(path)
            args = ["diagnostics", "--artifact", ".", "--source", ".", "--output", str(output), "--preflight-only"]
            with patch.object(sys, "argv", args), patch.object(diag, "verify_artifact"), \
                    patch.object(diag, "prepare_packages"), patch.object(diag, "preflight"), \
                    patch.object(diag, "manifest", cancel_manifest):
                result = diag.main()
            self.assertEqual(result, 143)
            self.assertEqual(json.loads((output / "summary.json").read_text())["status"], "interrupted")
            hashes = json.loads((output / "manifest.json").read_text())["files"]
            self.assertEqual(hashes["summary.json"], diag.sha256(output / "summary.json"))
