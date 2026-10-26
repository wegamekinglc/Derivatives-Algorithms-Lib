import importlib.util
import fcntl
import json
import os
from pathlib import Path
import signal
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('dispatch_execution', ROOT / 'dal425-controlled-execution.py')
execution = importlib.util.module_from_spec(spec)
spec.loader.exec_module(execution)


class DispatchTest(unittest.TestCase):
    def test_worker_interpreter_is_fixed_even_if_current_executable_changes(self):
        with patch.object(sys, 'executable', '/untrusted/python'):
            self.assertEqual(execution._argv('ok')[0], '/usr/bin/python3')

    def test_fingerprint_retains_nested_code_and_canonicalizes_set_constants(self):
        original = compile('def value(): return 1\n', 'fixed.py', 'exec')
        changed = compile('def value(): return 2\n', 'fixed.py', 'exec')
        self.assertNotEqual(execution._code_data(original), execution._code_data(changed))
        first = original.replace(co_consts=(frozenset(('a', 'b', 'c')),))
        second = original.replace(co_consts=(frozenset(('c', 'b', 'a')),))
        self.assertEqual(execution._code_data(first), execution._code_data(second))

    def test_invalid_sealed_helper_ticket_is_zero_dispatch(self):
        process = execution._process
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fd = os.memfd_create('test-invalid-ticket', os.MFD_ALLOW_SEALING | os.MFD_CLOEXEC)
            try:
                os.write(fd, b'{}')
                fcntl.fcntl(fd, fcntl.F_ADD_SEALS, process.SEALS)
                argv = ['/usr/bin/python3', '-I', '-S', '-B', str(ROOT / 'dal425-controlled-profile.py'),
                        '--child', 'profile', str(root), str(fd)]
                with patch.object(process, '_spawn_pipe') as spawn:
                    with self.assertRaisesRegex(ValueError, 'ticket binding'):
                        process.spawn_helper(argv, root, fd)
                    spawn.assert_not_called()
            finally:
                os.close(fd)

    def test_parent_fd_failure_after_spawn_reaps_its_child(self):
        process = execution._process
        stream = Mock()
        def close(fd):
            if fd == 33:
                raise OSError('close injection')
        with patch.object(process.os, 'pipe2', return_value=(31, 32)), \
                patch.object(process.os, 'fdopen', return_value=stream), \
                patch.object(process.os, 'open', return_value=33), \
                patch.object(process.os, 'close', side_effect=close), \
                patch.object(process, '_file_actions', return_value=[]), \
                patch.object(process.os, 'posix_spawn', return_value=999999), \
                patch.object(process.os, 'kill') as kill, \
                patch.object(process.os, 'wait4', return_value=(999999, 9, (0,) * 16)) as wait:
            with self.assertRaisesRegex(OSError, 'close injection'):
                process._spawn_pipe(['/usr/bin/python3'], Path.cwd(), ())
            kill.assert_called_once_with(999999, signal.SIGKILL)
            wait.assert_called_once_with(999999, 0)
            stream.close.assert_called_once()

    def test_file_actions_close_unrelated_inheritable_descriptor(self):
        process = execution._process
        with patch.object(process, '_inherited_fds', return_value=[3, 4, 5, 6]):
            actions = process._file_actions(4, 5, (3,))
        self.assertIn((os.POSIX_SPAWN_DUP2, 3, 3), actions)
        self.assertNotIn((os.POSIX_SPAWN_CLOSE, 3), actions)
        for fd in (4, 5, 6):
            self.assertIn((os.POSIX_SPAWN_CLOSE, fd), actions)

    def test_worker_rejects_foreign_source_before_spawn(self):
        process = execution._process
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fd = os.memfd_create('test-foreign-source', os.MFD_ALLOW_SEALING | os.MFD_CLOEXEC)
            try:
                os.write(fd, b'foreign source')
                fcntl.fcntl(fd, fcntl.F_ADD_SEALS, process.SEALS)
                with patch.object(process, '_spawn_pipe') as spawn:
                    with self.assertRaisesRegex(ValueError, 'fixed sealed worker source'):
                        process.spawn_worker(execution._argv('ok', fd), root, fd)
                    spawn.assert_not_called()
            finally:
                os.close(fd)

    def test_fixed_worker_zero_and_nonzero_exits_have_owned_wait4(self):
        for action, expected, output in [('ok', 0, b'ok\n'), ('nonzero', 7, b'')]:
            with self.subTest(action=action), tempfile.TemporaryDirectory(dir=ROOT) as directory:
                self.check_worker(Path(directory), action, expected, output)

    def test_sealed_launcher_initializes_without_an_extra_exec(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as directory:
            root = Path(directory)
            log = (root / 'launcher.log').open('xb')
            child = SimpleNamespace(pid=None, stdout=log)
            old_handler = signal.signal(signal.SIGALRM, self.timeout)
            reaped = False
            try:
                argv = ['/usr/bin/python3', '-I', '-S', '-B', str(ROOT / 'dal425-controlled-launch.py'), 'init', str(root), 'ok']
                actions = [(os.POSIX_SPAWN_DUP2, log.fileno(), 1), (os.POSIX_SPAWN_DUP2, log.fileno(), 2)]
                child.pid = os.posix_spawn('/usr/bin/python3', argv, execution.ENV, file_actions=actions)
                signal.setitimer(signal.ITIMER_REAL, 3)
                raw = os.wait4(child.pid, 0)
                reaped = True
                signal.setitimer(signal.ITIMER_REAL, 0)
                self.assertEqual(raw[1], 0, (root / 'launcher.log').read_text())
                record = json.loads((root / 'launcher.log').read_bytes())
                self.assertIs(record['initialized'], True)
                self.assertTrue((root / 'ledger.jsonl').is_file())
                print('sealed-launcher-init raw-wait4', (raw[0], raw[1], tuple(raw[2])))
            finally:
                if child.pid is not None:
                    self.close_worker(child, None, reaped)
                else:
                    log.close()
                signal.setitimer(signal.ITIMER_REAL, 0)
                signal.signal(signal.SIGALRM, old_handler)

    def check_worker(self, root, action, expected, output):
        previous = Path.cwd()
        old_handler = signal.signal(signal.SIGALRM, self.timeout)
        child = None
        snapshot = None
        reaped = False
        try:
            os.chdir(root)
            child, snapshot, argv = execution._spawn(root, action, .2)
            signal.setitimer(signal.ITIMER_REAL, 3)
            raw = os.wait4(child.pid, 0)
            reaped = True
            signal.setitimer(signal.ITIMER_REAL, 0)
            self.assertEqual(raw[0], child.pid)
            self.assertEqual(os.waitstatus_to_exitcode(raw[1]), expected)
            self.assertEqual(len(raw[2]), 16)
            self.assertEqual(child.stdout.read(4097), output)
            print('fixed-worker', action, 'argv', argv, 'raw-wait4', (raw[0], raw[1], tuple(raw[2])))
        finally:
            self.close_worker(child, snapshot, reaped)
            signal.setitimer(signal.ITIMER_REAL, 0)
            signal.signal(signal.SIGALRM, old_handler)
            os.chdir(previous)

    @staticmethod
    def timeout(signum, frame):
        raise TimeoutError('fixed dispatch control deadline')

    @staticmethod
    def close_worker(child, snapshot, reaped):
        if child is not None:
            if not reaped:
                os.kill(child.pid, signal.SIGKILL)
                os.wait4(child.pid, 0)
            child.stdout.close()
        if snapshot is not None:
            os.close(snapshot)


if __name__ == '__main__':
    unittest.main()
