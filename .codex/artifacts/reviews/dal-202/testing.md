# DAL-202 / F5 independent verification after master integration

Fresh independent Linux verification passes. No actionable product defect was
reproduced. This report covers accumulated F4 integration, legacy execution,
preparation, native tape allocation and advisory CI policy changes. Earlier
DAL-229 reports are historical evidence and do not approve this revision.

Performance findings are advisory under Cheng Li's DAL-228 instruction
`01a0a25d-9f60-770c-8e14-43eae4184b7f`. Historical C++ **61/63** and Python
**87/90** comparisons remain failed reference measurements. They are neither
relabeled as passes nor treated as CI, merge or F5 acceptance blockers. No full
timing rerun, calibration optimization or private NextBlock experiment was run.

## Revisions and changed scope

- Starting head: `ea8d5a26e6ecde0f95fe972aab4d20e81064f0a6`, tree
  `fd7e157b57381fb79cd16126e55ea4d78d20ea01`.
- Verified working tree recorded by test commit:
  `3bacea1f1671deaccb09617b60300820b5ab4711`, tree
  `d9fa401afaf0da665aa8c6b67c82cbc3319eb956`.
- F4 squash `b4e8b56135b5cfcbbe2ddd8d753921dd40d6caa2` is an ancestor.
- Product/public/binding sources equal correctness baseline `3de188ba`.
  The test commit changes only `.github/scripts/tests/test_classify_ci_changes.py`.
  This report is the only subsequent tracked change. Final published SHA/tree,
  clean status and the single CI snapshot are in the attached publication record
  and DAL-229 delivery comment.

Publication uses existing `feature/dal-202-compiled-observations` and
[PR #372](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/372),
base master. Its owner-set ready state is preserved. This tester changes no
product, workflow, documentation, benchmark, threshold or protection file.

## Running existing tests

All results below were executed in this run, rather than copied from DAL-228.

| Configuration / check                                             | Fresh result               |
|-------------------------------------------------------------------|---------------------------:|
| GCC 14 Release native full configure/build/install/CTest          | 1753/1753                  |
| Python 3.13 pytest, within one of those CTest entries             | 402/402                    |
| Native focused script/simulation/AAD/compiler/visitor             | 457/457                    |
| Adept / CoDiPack / XAD focused suite                              | 456/456; 456/456; 455/455  |
| Separate `AADTapeTest.*`, native / Adept / CoDiPack / XAD         | 12/12; 4/4; 4/4; 4/4       |
| Clang 21 ASan/UBSan, `AAD*:Script*:MonteCarlo*:MCSimulation*`     | 403/403                    |
| Registered isolated allocation cases under ASan/UBSan             | 2/2                        |
| Original exact-boundary / signed tiny-divisor probes              | 4/4; 6/6                   |
| Independently rebuilt isolated allocation probe                   | 2/2                        |
| Installed-package consumer                                        | 1/1                        |
| GCC static, GCC shared, Clang sanitized static consumers with LTO | 3/3                        |
| Existing CI-tool suite before new tests                           | 161 run; 155 pass, 6 skips |
| Final CI-tool suite                                               | 163 run; 157 pass, 6 skips |
| Documentation structure validation                                | 58 Markdown files          |

Fresh full-workflow summary:

```text
100% tests passed, 0 tests failed out of 1753
Total Test time (real) = 26.87 sec
```

The full workflow includes public API and portable Excel contracts, Python, and
building/installing examples. Benchmarks are disabled. Python's 402 is not added
to the CTest count. No previous root `test_output.txt` existed; attached
`full-linux.log` and `full-ctest-cases.log` come from this fresh build.

The historical focused filter excludes `AADTapeTest`, so it is explicitly run
separately on every backend. Native coverage includes three-input allocation,
rollover/reuse, aliased operands, multiple adjoints and caller-owned tapes.
Native-only root/default-Number tests account for two focused cases; Adept adds
its operand-stack lifetime test and CoDiPack its thread-lifetime isolation test.
XAD adds neither. Eight further tape tests are native-only. No selected C++ test
is skipped. XML and `suite-summary.json` record actual inventories/differences.

The verified original consumer sources are freshly compiled. A shared consumer
allocates three-input nodes through the public tape header; its caller mixes
local/external allocation and checks node, derivative, pointer and multi-adjoint
counts. All three builds report identical layout:
`368 8 0 8 16 88 160 232 304`. LTO applies to consumer translation units; DAL
libraries are separately built Release/static/shared/sanitized libraries.
The installed-package consumer independently resolves installed CMake targets.
ASan enables leak detection and UBSan halts on error. No sanitizer finding.

## F5 matrix and independent oracles

All **33** ScriptCompiledParity/ScriptCompiledParityFuzz cases pass on all four
backends. The unchanged lifetime test executes all **27** combinations of
threads `{1,2,4}`, history sequence `{80,90,80}`, three payoff roots and **8193
paths**. These are internal scenarios, not 27 separately discovered Google Tests.

| Contract        | Oracle and fresh coverage                                                                                                                                                                                                                                                   |
|-----------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| T06/T09/T10/T21 | Retained F=120 and unrelated sample=999 yield independent 200/160/120/160 path payoffs. Discounted zero-Gaussian formulas check every model/parameter risk and artifact lifetime.                                                                                           |
| T18             | Historical SCALE*H gives PV=160*exp(-r*T), SCALE risk=80*exp(-r*T), rate risk=-T*PV, T=10/DAYS_PER_YEAR; zero model risks and no fixing risk key.                                                                                                                           |
| T19/T20         | Direct historical seeds, constant roots and terminal path roots retain correct risks. Past barriers use hard comparisons below/at/above thresholds; past PAYS is evaluated/discarded. Nonlinear history repricing rebuilds preparation.                                     |
| T22             | Five smooth strikes and both paths retain w=(80-K+0.1)/0.2, PV=160*w*discount and K risk=-800*discount. Same-epsilon fuzzy double agrees with AAD; central differences use strike +/-0.0001 with tolerance 1e-5.                                                            |
| T23             | All 27 lifetime scenarios pass. Separate BS/Dupire references rebuild and record every path, checking risks across batches and repeated calls. Tape/root tests challenge stale adjoints and allocation rollover.                                                            |
| T16/T27         | Nonexpired dead-branch missing history fails across exact/fuzzy tree/compiled modes. Eager AND/OR, legacy SPOT, FIX/default binding, deduplication and today policies remain covered.                                                                                       |
| T31             | Last-history, model and compilation failures submit zero workers. Compiler seams recover; path/numerical failures drain accepted tasks before returning.                                                                                                                    |
| T32             | Throwing history/index seams after preparation and stable observation/evaluator storage pass. Allocation instrumentation detects ordinary/aligned positive controls, then zero C++ allocations over 8193 exact/fuzzy tree/compiled double evaluations, including the first. |

Exact arithmetic retains adjacent floats around 80, six comparisons, signed
zero/subnormal values and live `1e14*1e-14` parameter dependencies. The original
unoptimized exact probe passes independently of tree/compiled agreement.

Signed tiny fuzzy divisors retain analytic PV/SCALE risk, including computed
denominators, fractional weights and nested cross-event state. Existing nested
oracles use `y=x*(3-w-w*w)` and check primal, SCALE/rate risk and smooth central
differences across explicit/default epsilon settings. Oracles and tolerances
are unchanged: deterministic price relative 1e-12, analytic risks 1e-10,
normalized same-path MC 1e-8. No derivative is claimed at a hard switch.

Prepared-mode tests still assert the full AAD/smoothing/compiled-tree diagnostic,
both directions, nullopt defaults, matching execution and expired behavior.
The allocation fixture remains registered in CTest. Inspected production code
retains syntax-wide collection and sealed history before workers, hard local
AAD replay before the tape mark, live ConstVar inputs, final IF metadata before
compilation and path-local terminal-root reuse.

## Authoring tests and challenging the CI evidence

Added two tests to `CiWorkflowFastPathTest`:

- `test_gate_shell_requires_success_for_every_correctness_job`
- `test_gate_shell_docs_only_accepts_skipped_builds_and_checks_documentation`

These execute both workflows' actual Bash gate bodies and actual environment
bindings, using independent expected correctness job lists. **86 scenarios pass**:
success/failure/cancelled/skipped/running/empty benchmark states; every
nonsuccess required result; docs-only with heavy jobs really skipped;
classifier/documentation failures on that path; and rejection of skipped builds
on the code path. Both gates run on Ubuntu. The new tests skip Windows hosts or
missing Bash. The six local skips are existing PowerShell/.NET tests.

The previous 25-scenario harness left heavy jobs successful in its docs-only
checks and tested required failures only as `failure`. New coverage closes
those gaps. Independent evidence also compares both complete YAML documents
against parent `ad3480c0`, permitting only the specified advisory changes.
Other jobs/dependencies/step settings match. Four continuation steps, original
`outcome`-based diagnostic/package conditions, always-run uploads and 30-day
retention remain intact.

Regression sensitivity is recorded using temporary workflow copies:

- Original parent: **10 expected failures** for nonsuccess benchmark outcomes.
- Current revision: **86 scenarios pass**.
- Three variants removing Linux classifier, Windows classifier, or Linux
  documentation checks: **10 expected failures each**.

All **430 executions** and RED/GREEN logs are retained. Comparing complete Python
ASTs after only explicitly reviewed display-string substitutions confirms
unchanged numerical validators, calculations, schemas and command exit codes
in both comparison tools. The full CI-tool suite exercises failed/nonfinite/
incomplete evidence, strict 4% reference thresholds, sample counts and retained
failure reports. No full benchmark workload is run for this policy verification.

## Repairing failures

No production repair or failing-test repair was needed. Intentional RED/mutation
failures characterize regression sensitivity, not delivered-code failures.
The evidence-only YAML harness initially assumed every step had a name;
unnamed checkout steps exposed that harness error, corrected before successful
comparison. Existing Clang dangling-else warnings remain in raw logs; this
sanitizer build does not claim warning-clean verification.

## Commands and provenance

From the repository root:

```bash
git submodule update --init --recursive
NUM_CORES=12 ADDITIONAL_CMAKE_FLAGS='-DCMAKE_CXX_COMPILER=g++-14 -DCMAKE_C_COMPILER=gcc-14 -DDAL_BUILD_EXCEL_PORTABLE_TESTS=ON -DDAL_EXCEL_BUILD_TESTS=ON' bash ./build_linux.sh --python 3.13 > test_output.txt 2>&1
build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='Script*:*Simulation*:*AADTest*:*Compiler*:*DomainProc*:*IFProcessor*:*PastEvaluator*:*Smoothing*:*VarIndexer*'
build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='AADTapeTest.*'
python3 -m unittest discover -s .github/scripts/tests -v
python3 .github/scripts/check_docs.py
git diff --check
```

Workspace scripts `evidence/run_checks.py`, `verify_oracles_consumers.py` and
`verify_policy.py`, plus `commands.md` and per-command JSON, retain exact backend
configure/build/test commands, compiler/link arguments, directories, environment,
exits and filters. Attachments contain raw logs, XML, 837 source hashes,
binary hashes, caches and compile commands. Final test source/report are
checked against Git; publication identity is separate to avoid a circular SHA.

Both inherited report/archive/checksum sets were downloaded through Multica:

- Policy SHA256 `0b0bfd1759f4944e03066fde6e7b7354df45fec40b6aafb6006af5bcd421b5ef`;
  **49 internal hashes verified**.
- Product SHA256 `686f1d6621ea595551ed218fde2887e19a33ba2b217debfa743b32be34fc1197`;
  **1927 internal hashes verified**.

Original oracle/consumer bytes are reused unchanged and freshly compiled/run.
Prior full alternate-backend counts 1740/1740/1739 and timing measurements remain
inherited evidence. This run claims focused alternate-backend execution only.

## Limits and handoff

No local Windows/MSVC/XLL or PowerShell/.NET execution. Executing the Windows
workflow's Ubuntu gate is not Windows product verification. Alternate-backend
public/Python/Excel full suites and complete timings were not run. Examples
were built/installed, not exhaustively executed. Allocation instrumentation
counts C++ allocations during double evaluation, not arbitrary malloc or all
AAD tape allocation behavior. The compiler-failure seam injects immediately
before bytecode construction.

DAL-229 returns **in_review** for parent acceptance. DAL-230 owns the current
documentation/CHANGELOG decision; mandatory DAL-231 review and protected merge
remain outstanding. A single post-push CI snapshot records that moment, not
final required-check acceptance. No watch/poll, merge, closing intent, issue
closure, replacement peer chain or F6 advancement is performed here.
