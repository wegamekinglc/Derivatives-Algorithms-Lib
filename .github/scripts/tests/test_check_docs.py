"""Tests for the DAL documentation checker."""

import importlib.util
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "check_docs.py"
SPEC = importlib.util.spec_from_file_location("check_docs", SCRIPT)
CHECK_DOCS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK_DOCS)


class AgentDocsTest(unittest.TestCase):
    def test_agent_docs_cover_agent_facing_guides(self):
        expected = {
            "AGENTS.md",
            "CLAUDE.md",
            ".github/copilot-instructions.md",
            ".codex/skills/dal-agent-team/references/shared-rules.md",
        }

        actual = {path.relative_to(CHECK_DOCS.ROOT).as_posix() for path in CHECK_DOCS.AGENT_DOCS}

        self.assertEqual(actual, expected)

    def test_agent_docs_exist(self):
        for document in CHECK_DOCS.AGENT_DOCS:
            self.assertTrue(document.is_file(), document)

    def test_stale_table_flags_repo_root_test_binaries(self):
        self.assertIn("bin/dal_cpp_tests", CHECK_DOCS.STALE_DOCUMENTATION)
        self.assertIn("bin/dal_public_tests", CHECK_DOCS.STALE_DOCUMENTATION)


class AgentReferencedPathsTest(unittest.TestCase):
    def test_extracts_backticked_repo_path(self):
        text = "Core numerics live in `dal-cpp/dal/math/` here."

        self.assertEqual(CHECK_DOCS.agent_referenced_paths(text), [(1, "dal-cpp/dal/math")])

    def test_extracts_fenced_command_path(self):
        text = "```bash\n./dal-web/scripts/setup-playwright.sh\n```"

        self.assertEqual(
            CHECK_DOCS.agent_referenced_paths(text), [(2, "dal-web/scripts/setup-playwright.sh")]
        )

    def test_ignores_prose_without_code_formatting(self):
        text = "tests, docs/changelog, generated files, and dal-cpp/dal/math/ in prose"

        self.assertEqual(CHECK_DOCS.agent_referenced_paths(text), [])

    def test_extracts_root_level_files(self):
        text = "Edit `CMakePresets.json`, see `README.md`, and run `build_linux.sh`."

        self.assertEqual(
            CHECK_DOCS.agent_referenced_paths(text),
            [(1, "CMakePresets.json"), (1, "README.md"), (1, "build_linux.sh")],
        )

    def test_reports_line_numbers(self):
        text = "first line\nsecond `docs/methodology/aad.md` line"

        self.assertEqual(
            CHECK_DOCS.agent_referenced_paths(text), [(2, "docs/methodology/aad.md")]
        )

    def test_skips_build_tree_artifacts(self):
        text = (
            "run `./build/Release-linux/dal-cpp/dal_cpp_tests` "
            "or `build/stage/Release-linux/bin` here"
        )

        self.assertEqual(CHECK_DOCS.agent_referenced_paths(text), [])

    def test_skips_placeholders_and_globs(self):
        text = (
            "`build/stage/<preset>/bin/`, `.codex/skills/dal-*`, "
            "`dal-cpp/tests/<module>/test_<name>.cpp`, `${sourceDir}/build`, "
            "`dal-cpp/dal/auto/MG_*_enum.{hpp,inc}`, `.codex/artifacts/specs/<slug>.md`"
        )

        self.assertEqual(CHECK_DOCS.agent_referenced_paths(text), [])

    def test_skips_home_and_url_paths(self):
        text = "sync into `~/.codex/skills/` and see https://example.com/dal-cpp/x"

        self.assertEqual(CHECK_DOCS.agent_referenced_paths(text), [])

    def test_skips_flags_and_plain_words(self):
        text = "use `--gtest_filter=InterpTest.*`, `test_output.txt`, or `Makefile`"

        self.assertEqual(CHECK_DOCS.agent_referenced_paths(text), [])

    def test_strips_trailing_sentence_punctuation(self):
        text = "Presets live in `CMakePresets.json.`"

        self.assertEqual(CHECK_DOCS.agent_referenced_paths(text), [(1, "CMakePresets.json")])


class CheckAgentPathsTest(unittest.TestCase):
    def _check(self, root: Path, text: str) -> list[str]:
        document = root / "AGENTS.md"
        document.write_text(text, encoding="utf-8")
        errors: list[str] = []
        CHECK_DOCS.check_agent_paths((document,), root, errors)
        return errors

    def test_existing_paths_pass(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "docs" / "methodology").mkdir(parents=True)

            errors = self._check(root, "see `docs/methodology/` here")

            self.assertEqual(errors, [])

    def test_missing_path_is_flagged_with_line(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "docs").mkdir()

            errors = self._check(root, "first\nsee `docs/missing.md` now")

            self.assertEqual(len(errors), 1)
            self.assertIn("AGENTS.md:2", errors[0])
            self.assertIn("docs/missing.md", errors[0])

    def test_runtime_created_paths_are_allowed(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)

            errors = self._check(root, "SQLite file under `dal-web/backend/.data/` here")

            self.assertEqual(errors, [])

    def test_current_agent_docs_have_no_missing_referenced_paths(self):
        errors: list[str] = []
        CHECK_DOCS.check_agent_paths(CHECK_DOCS.AGENT_DOCS, CHECK_DOCS.ROOT, errors)
        self.assertEqual(errors, [])


class AgentDocsStandardChecksTest(unittest.TestCase):
    def test_current_agent_docs_pass_standard_checks(self):
        errors: list[str] = []
        CHECK_DOCS.check_links(CHECK_DOCS.AGENT_DOCS, errors)
        CHECK_DOCS.check_tables(CHECK_DOCS.AGENT_DOCS, errors)
        CHECK_DOCS.check_whitespace(CHECK_DOCS.AGENT_DOCS, errors)
        CHECK_DOCS.check_math_macros(CHECK_DOCS.AGENT_DOCS, errors)
        CHECK_DOCS.check_stale_commands(CHECK_DOCS.AGENT_DOCS, errors)

        self.assertEqual(errors, [])


if __name__ == "__main__":
    unittest.main()
