import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / "checkout_dependencies.py"
SPEC = importlib.util.spec_from_file_location("checkout_dependencies", SCRIPT)
checkout = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checkout)
PIN = "a" * 40


class CheckoutDependenciesTests(unittest.TestCase):
    def test_gitlink_identity_is_read_from_the_selected_worktree(self):
        with mock.patch.object(checkout, "git", return_value=f"160000 commit {PIN}\tdal-cpp/externals/eigen\n") as git:
            self.assertEqual(checkout.eigen_pin(Path("baseline")), PIN)
            git.assert_called_once_with(Path("baseline"), "ls-tree", "HEAD", "--", checkout.EIGEN_PATH, capture=True)

    def test_malformed_or_non_gitlink_identity_is_rejected(self):
        for entry in ["", f"100644 blob {PIN}\tdal-cpp/externals/eigen", "160000 commit short\tdal-cpp/externals/eigen"]:
            with self.subTest(entry=entry), mock.patch.object(checkout, "git", return_value=entry):
                with self.assertRaises(ValueError):
                    checkout.eigen_pin(Path("source"))

    def test_successful_primary_fetch_does_not_contact_the_mirror(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with mock.patch.object(checkout, "eigen_pin", return_value=PIN), mock.patch.object(checkout, "git", return_value=PIN) as git:
                checkout.checkout_eigen(root)
            fetches = [call.args for call in git.call_args_list if "fetch" in call.args]
            self.assertEqual(len(fetches), 1)
            self.assertEqual(fetches[0][-2:], (checkout.EIGEN_PRIMARY, PIN))

    def test_failed_primary_fetch_uses_the_identical_pin_in_the_mirror(self):
        def run(root, *args, **kwargs):
            if "fetch" in args and checkout.EIGEN_PRIMARY in args:
                raise subprocess.CalledProcessError(128, args)
            return PIN

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with mock.patch.object(checkout, "eigen_pin", return_value=PIN), mock.patch.object(checkout, "git", side_effect=run) as git:
                checkout.checkout_eigen(root)
            fetches = [call.args for call in git.call_args_list if "fetch" in call.args]
            self.assertEqual([call[-2:] for call in fetches], [(checkout.EIGEN_PRIMARY, PIN), (checkout.EIGEN_MIRROR, PIN)])
            self.assertIn(mock.call(root / checkout.EIGEN_PATH, "checkout", "--detach", PIN), git.call_args_list)

    def test_both_fetch_failures_fail_the_job(self):
        def run(root, *args, **kwargs):
            if "fetch" in args:
                raise subprocess.CalledProcessError(128, args)
            return PIN

        with tempfile.TemporaryDirectory() as directory:
            with mock.patch.object(checkout, "eigen_pin", return_value=PIN), mock.patch.object(checkout, "git", side_effect=run):
                with self.assertRaises(subprocess.CalledProcessError):
                    checkout.checkout_eigen(Path(directory))

    def test_checked_out_identity_mismatch_is_fatal(self):
        with tempfile.TemporaryDirectory() as directory:
            with mock.patch.object(checkout, "eigen_pin", return_value=PIN), mock.patch.object(checkout, "git", return_value="b" * 40):
                with self.assertRaises(RuntimeError):
                    checkout.checkout_eigen(Path(directory))

    def test_origin_metadata_failure_is_fatal_without_trying_the_mirror(self):
        failure = subprocess.CalledProcessError(128, ["git", "config"])

        def run(root, *args, **kwargs):
            if "config" in args:
                raise failure
            return PIN

        with tempfile.TemporaryDirectory() as directory:
            with mock.patch.object(checkout, "eigen_pin", return_value=PIN), mock.patch.object(checkout, "git", side_effect=run) as git:
                with self.assertRaises(subprocess.CalledProcessError) as raised:
                    checkout.checkout_eigen(Path(directory))
            self.assertIs(raised.exception, failure)
            fetches = [call.args for call in git.call_args_list if "fetch" in call.args]
            self.assertEqual([call[-2:] for call in fetches], [(checkout.EIGEN_PRIMARY, PIN)])

    def test_other_submodules_keep_recursive_checkout_and_ssh_rewriting(self):
        root = Path("source")
        with mock.patch.object(checkout, "git") as git, mock.patch.object(checkout, "checkout_eigen") as eigen:
            checkout.checkout_dependencies(root)
        calls = [call.args for call in git.call_args_list]
        self.assertIn((root, "submodule", "sync", "--recursive"), calls)
        update = next(call for call in calls if "update" in call)
        self.assertIn("--recursive", update)
        self.assertIn("--init", update)
        self.assertIn("submodule.dal-cpp/externals/eigen.update=none", update)
        self.assertIn("url.https://github.com/.insteadOf=git@github.com:", update)
        eigen.assert_called_once_with(root)


class CheckoutDependenciesGitTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.workspace = Path(temporary.name)
        configuration = self.workspace / "global.gitconfig"
        configuration.write_text("", encoding="utf-8")
        environment = mock.patch.dict(os.environ, {
            "GIT_CONFIG_GLOBAL": str(configuration), "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_ALLOW_PROTOCOL": "file", "GIT_AUTHOR_DATE": "2026-01-01T00:00:00Z",
            "GIT_COMMITTER_DATE": "2026-01-01T00:00:00Z",
        })
        environment.start()
        self.addCleanup(environment.stop)

    def git(self, root, *args):
        return subprocess.check_output(["git", "-C", str(root), *args], text=True, stderr=subprocess.STDOUT).strip()

    def commit(self, root, message):
        self.git(root, "-c", "user.name=CI Fixture", "-c", "user.email=fixture@example.invalid", "commit", "-m", message)

    def fixture(self, name, existing, use_mirror):
        directory = self.workspace / name
        directory.mkdir()
        upstream = directory / "primary"
        self.git(directory, "init", str(upstream))
        (upstream / "payload").write_bytes(b"pinned fixture\n")
        self.git(upstream, "add", "payload")
        self.commit(upstream, "Pinned fixture")
        pin = self.git(upstream, "rev-parse", "HEAD")
        (upstream / "payload").write_bytes(b"unrequested later revision\n")
        self.git(upstream, "add", "payload")
        self.commit(upstream, "Later fixture")
        mirror = directory / "mirror.git"
        self.git(directory, "clone", "--bare", str(upstream), str(mirror))
        primary = (directory / "unavailable").as_uri() if use_mirror else upstream.as_uri()
        root = directory / "worktree"
        self.git(directory, "init", str(root))
        (root / ".gitmodules").write_text(
            '[submodule "dal-cpp/externals/eigen"]\n\tpath = %s\n\turl = %s\n' % (checkout.EIGEN_PATH, primary),
            encoding="utf-8",
        )
        self.git(root, "add", ".gitmodules")
        self.git(root, "update-index", "--add", "--cacheinfo", "160000", pin, checkout.EIGEN_PATH)
        self.commit(root, "Pin Eigen fixture")
        if existing:
            destination = root / checkout.EIGEN_PATH
            destination.parent.mkdir(parents=True)
            self.git(root, "clone", str(upstream), str(destination))
            if existing == "missing":
                self.git(destination, "config", "--local", "--unset", "remote.origin.url")
            else:
                self.git(destination, "config", "--local", "remote.origin.url", str(directory / "stale-origin"))
            self.assertNotEqual(self.git(destination, "rev-parse", "HEAD"), pin)
        return root, pin, primary, mirror.as_uri()

    def assert_checkout_and_cleanup(self, existing):
        for use_mirror in (False, True):
            with self.subTest(existing=existing, mirror=use_mirror):
                root, pin, primary, mirror = self.fixture(str(use_mirror), existing, use_mirror)
                with mock.patch.object(checkout, "EIGEN_PRIMARY", primary), mock.patch.object(
                    checkout, "EIGEN_MIRROR", mirror
                ), mock.patch.object(checkout, "git", wraps=checkout.git) as git:
                    checkout.checkout_eigen(root)
                cleanup = subprocess.run([
                    "git", "-C", str(root), "submodule", "foreach", "--recursive",
                    "git", "config", "--local", "--show-origin", "--name-only", "--get-regexp", "remote.origin.url",
                ], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                self.assertEqual(cleanup.returncode, 0, cleanup.stdout + cleanup.stderr)
                self.assertIn("remote.origin.url", cleanup.stdout)
                destination = root / checkout.EIGEN_PATH
                self.assertEqual(self.git(destination, "config", "--local", "--get", "remote.origin.url"), mirror if use_mirror else primary)
                self.assertEqual(self.git(destination, "rev-parse", "HEAD"), pin)
                self.assertEqual(self.git(destination, "rev-parse", "--abbrev-ref", "HEAD"), "HEAD")
                self.assertEqual((destination / "payload").read_bytes(), b"pinned fixture\n")
                self.assertEqual(self.git(root, "status", "--porcelain"), "")
                fetches = [call.args[-2:] for call in git.call_args_list if "fetch" in call.args]
                self.assertEqual(fetches, [(primary, pin), (mirror, pin)] if use_mirror else [(primary, pin)])

    def test_new_checkout_primary_and_mirror_keep_pin_and_support_recursive_cleanup(self):
        self.assert_checkout_and_cleanup(existing=False)

    def test_existing_checkout_primary_and_mirror_keep_pin_and_support_recursive_cleanup(self):
        self.assert_checkout_and_cleanup(existing=True)

    def test_existing_checkout_without_origin_supports_primary_and_mirror_cleanup(self):
        self.assert_checkout_and_cleanup(existing="missing")


if __name__ == "__main__":
    unittest.main()
