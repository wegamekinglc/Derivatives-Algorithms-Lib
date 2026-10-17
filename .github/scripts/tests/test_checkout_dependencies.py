import importlib.util
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


if __name__ == "__main__":
    unittest.main()
