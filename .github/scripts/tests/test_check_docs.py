"""Tests for the DAL documentation checker."""

import copy
import importlib.util
from pathlib import Path
import tempfile
import tomllib
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "check_docs.py"
SPEC = importlib.util.spec_from_file_location("check_docs", SCRIPT)
CHECK_DOCS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK_DOCS)


class AgentDocsTest(unittest.TestCase):
    def test_all_docs_cover_codex_skills_and_references(self):
        relative = {path.relative_to(CHECK_DOCS.ROOT).as_posix() for path in CHECK_DOCS.ALL_DOCS}

        self.assertIn(".codex/references/code-style.md", relative)
        self.assertIn(".codex/artifacts/README.md", relative)
        self.assertIn(".github/copilot-instructions.md", relative)

    def test_agent_docs_cover_agent_facing_guides(self):
        expected = {
            "AGENTS.md",
            "CLAUDE.md",
            ".github/copilot-instructions.md",
            ".codex/README.md",
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
        text = "```bash\n./dal-python/build_wheel.sh\n```"

        self.assertEqual(
            CHECK_DOCS.agent_referenced_paths(text), [(2, "dal-python/build_wheel.sh")]
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

    def test_runtime_and_local_paths_are_allowed(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)

            errors = self._check(
                root,
                "Local Claude state uses `.claude/settings.local.json` and "
                "`.claude/worktrees/`.",
            )

            self.assertEqual(errors, [])

    def test_current_agent_docs_have_no_missing_referenced_paths(self):
        errors: list[str] = []
        CHECK_DOCS.check_agent_paths(CHECK_DOCS.AGENT_DOCS, CHECK_DOCS.ROOT, errors)
        self.assertEqual(errors, [])


class AgentDocsStandardChecksTest(unittest.TestCase):
    def test_math_macro_check_ignores_inline_code_examples(self):
        with tempfile.TemporaryDirectory() as tmp:
            document = Path(tmp) / "example.md"
            document.write_text("Use `\\operatorname{x}` only as a literal example.\n", encoding="utf-8")
            errors: list[str] = []

            CHECK_DOCS.check_math_macros((document,), errors)

            self.assertEqual(errors, [])

    def test_current_agent_docs_pass_standard_checks(self):
        errors: list[str] = []
        CHECK_DOCS.check_links(CHECK_DOCS.AGENT_DOCS, errors)
        CHECK_DOCS.check_tables(CHECK_DOCS.AGENT_DOCS, errors)
        CHECK_DOCS.check_whitespace(CHECK_DOCS.AGENT_DOCS, errors)
        CHECK_DOCS.check_math_macros(CHECK_DOCS.AGENT_DOCS, errors)
        CHECK_DOCS.check_stale_commands(CHECK_DOCS.AGENT_DOCS, errors)

        self.assertEqual(errors, [])


class SemanticConsistencyTest(unittest.TestCase):
    def test_historical_work_products_are_rejected_under_docs(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            historical = root / "docs/superpowers/plans/old.md"
            historical.parent.mkdir(parents=True)
            historical.write_text("# Old plan\n", encoding="utf-8")
            errors: list[str] = []

            CHECK_DOCS.check_current_state_doc_locations(errors, root)

            self.assertEqual(len(errors), 1)
            self.assertIn("docs/superpowers/plans/old.md", errors[0])

    def test_current_repository_semantic_contracts_pass(self):
        errors: list[str] = []
        CHECK_DOCS.check_current_state_doc_locations(errors)
        CHECK_DOCS.check_ci_compiler_inventory(errors)
        CHECK_DOCS.check_benchmark_inventory(errors)
        CHECK_DOCS.check_repository_workflows(errors)

        self.assertEqual(errors, [])

    def test_benchmark_inventory_rejects_documented_count_drift(self):
        errors: list[str] = []
        CHECK_DOCS.check_benchmark_inventory_texts(
            "set(DAL_BENCHMARK_TARGETS\n    tape_perf\n    rate_risk_perf)",
            'BENCHMARKS = ("tape_perf", "rate_risk_perf")',
            "- `tape_perf`: tape\n- `rate_risk_perf`: risk\n",
            "(1 total: tape, rate_risk)",
            errors,
        )

        self.assertTrue(any("declares 1 total" in error for error in errors))

    def test_benchmark_inventory_rejects_gate_reference_drift(self):
        errors: list[str] = []
        CHECK_DOCS.check_benchmark_inventory_texts(
            "set(DAL_BENCHMARK_TARGETS\n    tape_perf\n    rate_risk_perf)",
            'BENCHMARKS = ("tape_perf", "rate_risk_perf")',
            "- `tape_perf`: tape\n",
            "(2 total: tape, rate_risk)",
            errors,
        )

        self.assertTrue(any("gated target inventory drift" in error for error in errors))


class PythonReleaseContractTest(unittest.TestCase):
    def metadata(self) -> dict:
        with (CHECK_DOCS.ROOT / "dal-python/pyproject.toml").open("rb") as stream:
            return tomllib.load(stream)

    def document_texts(self) -> dict[str, str]:
        return {
            "readme": (CHECK_DOCS.ROOT / "dal-python/README.md").read_text(encoding="utf-8"),
            "installation": (CHECK_DOCS.ROOT / "docs/installation.md").read_text(
                encoding="utf-8"
            ),
            "changelog": (CHECK_DOCS.ROOT / "CHANGELOG.md").read_text(encoding="utf-8"),
        }

    def test_requires_cpython_classifier(self):
        metadata = self.metadata()
        metadata["project"]["classifiers"].remove(
            "Programming Language :: Python :: Implementation :: CPython"
        )
        errors: list[str] = []

        CHECK_DOCS.check_python_project_metadata(errors, metadata)

        self.assertTrue(any("CPython implementation classifier" in error for error in errors))

    def test_rejects_duplicate_configured_selector(self):
        metadata = copy.deepcopy(self.metadata())
        metadata["tool"]["cibuildwheel"]["build"].append("cp39-*")
        errors: list[str] = []

        CHECK_DOCS.check_cibuildwheel_config(errors, metadata)

        self.assertTrue(any("unique" in error for error in errors))

    def test_build_linux_python_option_is_enforced_in_installation_docs(self):
        build_script = (CHECK_DOCS.ROOT / "build_linux.sh").read_text(encoding="utf-8")
        installation = self.document_texts()["installation"]
        self.assertIn("| `--python`", installation)
        errors: list[str] = []

        CHECK_DOCS.check_build_script_options(
            build_script,
            installation.replace("| `--python`", "| `--missing`", 1),
            errors,
        )

        self.assertTrue(any("option table drift" in error for error in errors))

    def test_current_public_docs_match_python_release_contract(self):
        texts = self.document_texts()
        errors: list[str] = []

        CHECK_DOCS.check_python_release_document_texts(
            texts["readme"], texts["installation"], texts["changelog"], errors
        )

        self.assertEqual(errors, [])

    def test_rejects_stale_public_python_release_claims(self):
        texts = self.document_texts()
        mutations = (
            ("readme", "CPython 3.9-3.13", "CPython 3.10-3.13"),
            ("readme", "ten wheels", "eight wheels"),
            ("installation", "four wheels", "three wheels"),
            ("changelog", "ten CPython-specific wheels", "eight wheels"),
        )
        for document, old, new in mutations:
            with self.subTest(document=document, replacement=new):
                mutated = dict(texts)
                self.assertIn(old, mutated[document])
                mutated[document] = mutated[document].replace(old, new, 1)
                errors: list[str] = []

                CHECK_DOCS.check_python_release_document_texts(
                    mutated["readme"],
                    mutated["installation"],
                    mutated["changelog"],
                    errors,
                )

                self.assertNotEqual(errors, [])

    def test_current_workflow_projections_match_event_contract(self):
        workflow = (
            CHECK_DOCS.ROOT / ".github/workflows/dal-python-release.yml"
        ).read_text(encoding="utf-8")
        errors: list[str] = []

        CHECK_DOCS.check_python_release_projections(errors, self.metadata(), workflow)

        self.assertEqual(errors, [])

    def test_projection_parsers_preserve_validation_boundaries(self):
        errors: list[str] = []

        self.assertEqual(
            CHECK_DOCS.parse_build_projection("cp39-* cp313-*", "pull_request", errors),
            ("cp39", "cp313"),
        )
        self.assertEqual(
            CHECK_DOCS.parse_verify_projection("cp39,cp313", "pull_request", errors),
            ("cp39", "cp313"),
        )
        self.assertEqual(errors, [])

        self.assertIsNone(
            CHECK_DOCS.parse_verify_projection("cp39,,cp313", "pull_request", errors)
        )
        self.assertTrue(any("nonempty and unique" in error for error in errors))

    def test_workflow_runs_fresh_cp39_smoke(self):
        workflow = (
            CHECK_DOCS.ROOT / ".github/workflows/dal-python-release.yml"
        ).read_text(encoding="utf-8")

        self.assertIn("dal-python/scripts/smoke_installed_wheel.py", workflow)
        self.assertIn("uv run --isolated --no-project --python 3.9", workflow)
        self.assertIn(".github/scripts/check_dal_python_syntax.py", workflow)

    def test_workflow_executes_powershell_helper_contracts_on_windows(self):
        workflow = (
            CHECK_DOCS.ROOT / ".github/workflows/dal-python-release.yml"
        ).read_text(encoding="utf-8")

        self.assertIn("Test executable PowerShell helper contracts", workflow)
        self.assertIn("test_python_helpers_powershell.py", workflow)
        self.assertIn("matrix.platform == 'windows-amd64'", workflow)
        self.assertLess(
            workflow.index("Test executable PowerShell helper contracts"),
            workflow.index("Build and test wheels"),
        )

    def test_rejects_independent_workflow_projection_drift(self):
        workflow = (
            CHECK_DOCS.ROOT / ".github/workflows/dal-python-release.yml"
        ).read_text(encoding="utf-8")
        mutations = (
            ("cp39-* cp313-*", "cp313-*"),
            ("cp39,cp313", "cp313"),
            ("cp39-* cp310-* cp311-* cp312-* cp313-*", "cp39-* cp311-* cp312-* cp313-*"),
            ("cp39,cp310,cp311,cp312,cp313", "cp39,cp310,cp311,cp312,cp38"),
            ("cp39-* cp313-*", "cp39-* cp39-* cp313-*"),
            ("cp39,cp313", "cp39,,cp313"),
        )
        for old, new in mutations:
            with self.subTest(new=new):
                mutated = workflow.replace(old, new, 1)
                errors: list[str] = []
                CHECK_DOCS.check_python_release_projections(errors, self.metadata(), mutated)
                self.assertNotEqual(errors, [])


if __name__ == "__main__":
    unittest.main()
