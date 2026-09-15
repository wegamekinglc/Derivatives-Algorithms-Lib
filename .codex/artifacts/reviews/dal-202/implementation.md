# DAL-202 / F5: performance reports are advisory

Cheng Li's DAL-228 comment `01a0a25d-9f60-770c-8e14-43eae4184b7f` removes DAL CI performance gates. The implementation applies that decision to both required CI gates and the reporting workflow. Performance findings remain visible reference evidence; they no longer block CI, merging, or F5 delivery.

PR #372 remains open against master in its owner-set ready-for-review state. Parent DAL-202 still owns the existing DAL-229 independent testing → DAL-230 documentation/CHANGELOG decision → DAL-231 mandatory review sequence and eventual protected merge. This report does not claim independent F5 approval.

## Revisions and scope

- Baseline F4 master: `b4e8b56135b5cfcbbe2ddd8d753921dd40d6caa2`, tree `f531d1858b881d3cf352c05c4e461e34f4263502`.
- Starting F5 head: `ad3480c05006df5e65dec5f0f674c361b3c1ecd4`, tree `8f84d0528bc6451a620128d83598236ce2f96fa9`.
- Tested policy commit: `49b58e8b9804c43de2b2cb158b78bc22dfeb7031`, tree `8a8d60f06e4fa08411efea2e4865a4cd46bb4465`.
- Only this report changes after the tested policy commit. The final published SHA/tree is recorded in `repair/published-identity.json` and the delivery comment.

Twelve files implement the policy:

- `.github/workflows/cmake-linux.yml` and `.github/workflows/cmake-windows.yml`: remove `benchmark` from required aggregate-gate dependencies, environment inputs and success assertions. Linux's native/Python comparisons and both A/A diagnostic steps use `continue-on-error: true`.
- `.github/scripts/check_benchmark_regressions.py` and `.github/scripts/check_python_benchmark_regressions.py`: label their summaries advisory; calculations, exit codes, JSON schemas and evidence validation are unchanged.
- `.github/scripts/tests/test_classify_ci_changes.py` and `.github/scripts/tests/test_python_benchmark_regressions.py`: enforce the new dependency/step policy and retain checks for required correctness jobs, sampling, diagnostic triggers and artifact uploads.
- `.codex/references/benchmark-workflow.md`, `README.md`, `dal-python/benchmarks/README.md`, and the generic-joint-quote-risk, rate-node-risk and XCCY methodology notes: describe performance reporting as advisory.

This report is the thirteenth changed file. No C++ product, public API, binding, backend, executable benchmark workload, sample count, numerical validator or threshold changes. The earlier prepared-mode diagnostic and allocation-test registration remain intact.

## Behavior and design

Both stable required gate names remain unchanged. GitHub master protection currently requires only `Linux CI gate` and `Windows CI gate`, with strict base freshness; no rulesets add separate performance checks. The captured protection/ruleset records are in the evidence. No branch-protection mutation was necessary.

The aggregate gates still require their previous build, correctness, documentation and sanitizer dependencies. They no longer wait for the optional benchmark jobs. Benchmark build or data-validation errors may still fail an optional benchmark job and stay visible.

Performance comparison findings keep their original nonzero command result and failed report status. Step-level continuation makes those findings nonblocking. Python diagnostics and package preservation still test `steps.python-performance.outcome == 'failure'`, which retains the original outcome despite continuation. Both platforms retain evidence uploads, including failed runs, for 30 days.

The two-round, ten-alternating-samples-per-side, minimum-reduction and strict 4% rule remains a reporting reference. Existing failures were not relabelled as passes.

## RED → GREEN and verification

1. Added `test_performance_reports_do_not_block_required_gates`. RED: both platform subcases failed because their gates still depended on Benchmarks. GREEN: remove only that dependency, input and assertion; all 14 CI-path tests pass.
2. Updated the advisory-report contract while retaining sampling and evidence checks. RED: all four comparison/diagnostic steps lacked continuation. GREEN: add continuation to those four steps; all 14 Python comparison tests pass.
3. Full CI-tool suite: **161 tests run, 155 passed, 6 existing Windows PowerShell/.NET cases skipped**.
4. YAML parsing and actual Bash execution: **25 scenarios pass**. Both workflow dependency sets equal their prior sets minus Benchmarks. Success, failure, cancellation, skip and running benchmark outcomes do not block either gate; every required result still rejects failure; docs-only classifier behavior remains intact.
5. Documentation validation: **58 Markdown files pass**. Staged scope and whitespace checks pass.

Commands (repository cwd, except the evidence harness):

```sh
python3 -m unittest discover -s .github/scripts/tests -p test_classify_ci_changes.py -k test_performance_reports_do_not_block_required_gates -v
python3 -m unittest discover -s .github/scripts/tests -p test_classify_ci_changes.py -v
python3 -m unittest discover -s .github/scripts/tests -p test_python_benchmark_regressions.py -k test_linux_advisory_benchmark_job -v
python3 -m unittest discover -s .github/scripts/tests -p test_python_benchmark_regressions.py -v
python3 -m unittest discover -s .github/scripts/tests -v
python3 .github/scripts/check_docs.py
git diff --check
# Workspace cwd:
python3 repair/validate_ci_policy.py
```

The corresponding `repair/ci-gates-{red,green}`, `ci-reports-{red,green}`, `ci-tools-full`, `ci-policy-scenarios` and `docs-policy` JSON/log records retain commands, exit codes and output. A publication snapshot records hosted CI once; no CI polling or watching is used.

## Preserved F5 evidence and deferred experiment

The earlier review-repair archive was downloaded through Multica and verified: SHA256 `686f1d6621ea595551ed218fde2887e19a33ba2b217debfa743b32be34fc1197`, all **1,927** internal entries valid. It remains attached to DAL-228 thread `01a0a250-4d12-7bde-9b64-e2d83d8d475c`. Its report and prior failed candidates are historical evidence, not a current performance-gate requirement.

Before the new user instruction, this run prepared two private NextBlock overlays and a block-boundary test draft, and completed a clean stock native/Python performance build. The overlays and new boundary tests were **not compiled, tested, timed or selected**. They are preserved under `repair/variants/` and `repair/deferred-block-boundary-tests.patch`; no experimental AAD change ships. The performance-only investigation is deferred under the user's new policy. No full timing rerun was performed to obtain a pass.

Inherited complete comparisons on `0a7455c24afc77ef61e074bc0a06acd7a6323e14` remain **C++61/63 and Python87/90**: clear/rewind recording, compiled barrier AAD, mixed calibration diagnostics and quote-risk n5 analytic t120 exceeded the reference threshold. Attribution remains unresolved; these are advisory findings.

Inherited product correctness on `3de188ba0d2d5dc2778a20c22052c614f8d71cba`: native1753/1753 including Python402, Adept1740, CoDiPack1740, XAD1739; Clang ASan/UBSan403 plus allocation2; exact4/tiny6/allocation2; 27 lifetime combinations and33 legacy/parity/fuzz cases. Product source is unchanged by this policy revision. These are explicitly inherited results, not fresh C++/backend/sanitizer runs. Windows/MSVC execution remains hosted; the six local PowerShell skips are retained.

The current evidence archive contains this matching report, fresh policy validation, source/publication identities, protection records and the deferred experimental drafts. It references the immutable prior archive without repacking its logs or presenting them as fresh. Independent testing, documentation and review remain outstanding; F6 is not advanced.
