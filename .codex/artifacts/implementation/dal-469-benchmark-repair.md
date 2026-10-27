# DAL-469 benchmark investigation

Status: the second remote diagnostic completed setup/build and failed the Python
vanilla performance gate; its script identities remained stable. The six PR
analysis findings are addressed locally for focused reviewer revalidation.
Final-head CI/Codacy must be reported from their own snapshot; local analysis
does not establish remote policy clearance. The performance root cause remains
inconclusive and the original CI failures remain valid. Algorithms and gate
policy are unchanged; the issue remains `in_progress`. Earlier sections retain
their original handoff context.

## PR analysis findings and remote-result classification

The latest handoff explicitly includes all six Codacy findings on
`485faeb7ce5382e1d55c97413ff25203467b09c2` (check `122027191528`). Its ordinary
Linux/Windows CI passed and no unresolved review thread was reported. This
change is limited to two private diagnostic scripts, their targeted tests and
this record; it does not dispatch a workflow or merge the PR.

Five findings were reproduced with Lizard 1.23.0, matching Codacy's complexity
values and unchanged limit 8. Helpers now separate original-package verification
from probes, environment collection from evidence copying, numerical preflights
from measurement summaries, SIGILL investigation from per-command profiles,
and compiler inventory/installation from actual-version verification.

| Function         | Before | After |
|------------------|--------|-------|
| choose_packages  | 9      | 7     |
| prepare          | 11     | 4     |
| measure          | 9      | 4     |
| profile          | 9      | 4     |
| select_compilers | 16     | 5     |

Every function in both production scripts and the changed test file is at most
7. Order and evidence contracts remain: verify both original hashes before
either probe; snapshot both sides before identity/preflight checks; run both
measurement targets and retain failures; check post-sampling identity; profile
after gate timing. Compiler failure/fallback behavior and each profiler's status
keys/output paths are unchanged.

The sixth finding is the Semgrep audit rule
[`dangerous-subprocess-use-audit`](https://github.com/semgrep/semgrep-rules/blob/develop/python/lang/security/audit/dangerous-subprocess-use-audit.yaml).
The rule flags the shared runner's variable argv and asks for a caller audit.
All production callers supply repository-owned argument lists: fixed metadata
and installer commands, the current interpreter with fixed private workers/gate
scripts, validated build paths, or fixed profiler wrappers. The manual dispatch
input selects only a mode; it never supplies commands, source refs or flags.
Original package/source validation and literal profiler arguments remain intact.

`literal_argv()` now rejects command strings, empty lists/executables and
non-string/path elements before child launch. Actual execution retains
`shell=False`, timeout handling, stdout/stderr and exact return-code recording.
The existing real-child test proves shell metacharacters remain literal; the
new regression verifies invalid shapes cannot invoke `subprocess.run`. The
dynamic-call audit finding is therefore handled as a verified false positive
with **one line's rule-specific** `# nosemgrep: dangerous-subprocess-use-audit`.
No configuration, file exclusion, analyzer-wide suppression or threshold changes.

Semgrep 1.180.0 with that upstream rule reproduces one original finding. The
corrected scripts produce zero findings with the precise exception; rerunning
with `--disable-nosem` exposes exactly that same audited call, exit 1 under
`--error`. The rule source and its SHA-256, all three JSON outputs and caller
evidence are included in the handoff archive; this does not assert equivalence
to every remote Codacy rule.

Targeted RED/GREEN and verification:

```sh
python3 -m unittest discover -s .github/scripts/tests \
  -p test_dal469_diagnostics.py -k invalid_command_shapes -v
PYTHONPATH=.github/scripts/tests:.github/scripts python3 -W error::ResourceWarning -m unittest -v \
  test_dal469_diagnostics test_dal469_setup
python3 -m py_compile .github/scripts/diagnose_dal469.py \
  .github/scripts/setup_dal469_toolchain.py .github/scripts/tests/test_dal469_diagnostics.py
ruff check --isolated .github/scripts/diagnose_dal469.py \
  .github/scripts/setup_dal469_toolchain.py .github/scripts/tests/test_dal469_diagnostics.py
semgrep --metrics=off --disable-version-check --error \
  --config ../dal-469-dangerous-subprocess-use-audit.yaml --json \
  .github/scripts/diagnose_dal469.py .github/scripts/setup_dal469_toolchain.py
git diff --check
```

The invalid-argv test failed before validation and passed afterward. A separate
immutable-baseline replay of the final test against `485faeb7` confirms all four
invalid forms fail because the expected rejection is absent. The final scoped
suite passes **34 tests**, including the three prior evidence-loss fixtures,
all ten installer branches, original-hash/probe ordering, literal subprocess
arguments, environment differences/original-evidence copying, and available/
unavailable/failing profiler branches. Python compilation, Ruff, Semgrep and
whitespace checks pass. Lizard's original five violations become zero without
raising the limit. Prior actionlint remains applicable because the workflow is
unchanged. No product matrix, core-build or DAL timing was repeated.

The completed [run 40455954661](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/runs/40455954661)
at `485faeb7` was inspected for script defects only. Downloaded artifact
`12713533699` matches GitHub SHA-256
`0b08c4ab27057b5c95776a9208c6fa04b956db7187c5d29cb3eb5fe3664470f2`.
Setup/build/preflights/profile/upload completed; measurement status is tape 0,
Python-prefix 1, original-CI 1. Before/after identities are equal, all 40 Python
raw reports exist, and no stage error file exists. Re-evaluating those retained
reports with the unchanged evaluator reproduces the exact serialized comparison
records and failure: `mc.vanilla.double.default`, rounds +8.24%/+10.93%, paired
support 9/10 and 10/10. This is a performance-gate failure, not a discovered
controller error. Perf permission failures (255) remain recorded separately.
No new measurement was run; performance attribution remains with performancer.

Original failures and hosted CPU/ISA/toolchain/profile limitations remain.
Final-head CI/Codacy can still be pending in the one requested snapshot; do not
claim all PR checks clear until that exact head reports them. Reviewer validation
and the user's requested benchmark rerun remain orchestrator-controlled.

## Remote setup repair

Performer evidence for [run 40442839473](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/runs/40442839473)
at `15260e2f7c61477cb517bb21259d340f432d932a` establishes a setup failure, not a
performance result. `linux-tools-generic_6.8.0-146.146` returned HTTP 404 and
apt exited 100. The apt policy and install logs show GCC/G++ 14.2 already
installed. The workflow combined those compilers with optional profilers and
misreported this failure as compiler unavailability, then repeated the failing
combined install. No compatibility inspection, build or measurement ran.

Reused source: performer's `dal-469-remote-results-evidence.zip`, attachment
`01a17bc9-fd6b-76c4-ae97-57b2fc44adeb`, including both apt logs and the complete
job log. `tests/fixtures/dal469-profiler-404.txt` is the exact original
`artifact/apt-install.txt`, without edits. The accompanying performer report
is attachment `01a17bc9-f96d-7ef3-af8a-bc2a4a2a49e1`. No remote run was started
by this repair.

Only the diagnostic workflow setup command, private installer, related tests,
the log fixture and this active record change:

- Refresh the apt index first with strict update-error handling; retain its
  output and return code before policy inspection or installation.
- Probe GCC and G++ versions. Reuse installed 14.2.0; otherwise query the
  refreshed index for the exact original compiler packages. Use generic GCC 14
  only when that original version is unavailable. Failed compiler installation
  is fatal and never triggers a generic retry.
- Verify both compiler versions are GCC 14 and agree; record original-version
  differences. Install and verify required make/time separately. Any required
  install failure retains its actual exit (including apt 100) and fails setup.
- Install optional GDB and perf independently afterward. Record each install
  and version-probe status, including missing/kernel-incompatible tools. Such
  capability gaps warn and allow the subsequent build/gate steps to run.
- Reuse the reviewed literal-argument command runner. All setup stdout/stderr,
  command identities, return codes and summary JSON live in
  `diagnostic-results/setup`, covered by the unchanged `always()` upload.
  Original CI exit status 1 remains separate from current setup status.

The first regression test executes the actual workflow setup block with
isolated command fixtures, an installed GCC 14.2 pair, and the original apt
404 output. It fails before the repair: exit 100 plus the inaccurate original
GCC warning. The identical test passes after the repair: setup exit 0, perf
install exit 100/version exit 1 retained, no compiler install or fallback.

RED and GREEN command:

```sh
python3 -m unittest discover -s .github/scripts/tests -p test_dal469_setup.py -v
```

Then the smallest affected checks run:

```sh
PYTHONPATH=.github/scripts/tests:.github/scripts python3 -W error::ResourceWarning -m unittest -v \
  test_dal469_setup.DiagnosticSetupTest \
  test_dal469_diagnostics.DiagnosticWorkflowTest \
  test_dal469_diagnostics.DiagnosticCommandTest.test_command_arguments_are_literal_and_status_and_output_are_retained
ruff check --isolated .github/scripts/setup_dal469_toolchain.py .github/scripts/tests/test_dal469_setup.py
../dal-469-tools/actionlint -shellcheck= -pyflakes= .github/workflows/benchmarks.yml
git diff --check
```

Results: all 14 targeted tests pass (10 setup cases plus four existing workflow
and command-safety contracts); Ruff, actionlint v1.7.12 and whitespace checks
exit 0. Shellcheck remains unavailable. Setup cases cover strict index ordering
and refresh failure, installed compilers plus profiler 404, required-tool
failure, missing compilers/exact install, unavailable-original fallback and
version differences, compiler-install failure without retry, invalid actual
versions, optional GDB failure, and manual-mode/argument isolation. The fixture
commands replace apt/sudo/compiler/profiler executables; they do not install
packages or execute DAL. Persistent per-scenario result trees, command traces,
RED/GREEN logs and a SHA-256 inventory accompany the handoff.

Case selection follows changed setup paths only. No sampler, numerical check,
threshold, measured source, build configuration or profiling implementation
changes; the prior applicable checks remain evidence for those paths. Full
product tests, core-build and local performance comparisons were not repeated.
Scheduled/default manual job definitions are untouched. No workflow dispatch
or merge is authorized in this turn.

Remaining risks: a fresh runner must still verify CPU/ISA, exact Python/CMake,
dependencies/binaries and profiler permissions. The latest remote attempt has
zero samples for both original failing targets; neither verdict is cleared.
The previous PR head's Codacy `ACTION_REQUIRED` is retained as outstanding;
maintenance findings are not suppressed or expanded into this repair. Report
the new commit's policy snapshot separately without inferring success from
local tests. Reviewer revalidation and another remote run remain orchestrator
decisions; DAL-469 stays `in_progress`.

## Reviewer evidence-retention blocker correction

Review of `1d0c17384169c181ff45360bdd114de60601c20d` found that the replay copy
ran after numerical preflights and the final identity check. The reviewer
attachment `dal-469-benchmark-diagnostic.md` supplied three fixture scenarios:
head tape preflight failure, head prefix preflight failure, and post-sampling
identity drift. All had lost both replayable executables/packages; the first
two also lacked the base configuration.

The correction changes only the diagnostic controller, its related tests and
this record. `preserve_replay_evidence()` now saves both CMake caches, available
compile commands, tape executables and complete selected Python packages before
the measurement stage's first identity validation or numerical preflight. Python archives are saved
for both original-package and rebuild selection. Later checks do not overwrite
these original copies. Existing before/after identity reports, numerical logs,
command return codes, sampling-status file and exception-status handling remain
unchanged; the workflow's existing unconditional evidence upload is reused.

The three regression fixtures call the actual manual `main(['measure'])` path
with mocked source identities and child commands, matching the review method.
They verify both sides' binary bytes, Python native module and wrapper bytes,
caches and compile commands; each stage exits 1. Numerical failures retain
preflight exit 23 and never start sampling. The drift fixture modifies the live
head executable, retains different before/after hashes and sampler exit 7,
while the saved executable remains the original. Original CI exit status 1 is
retained throughout. These are synthetic fixtures, not new DAL measurements.

RED command (before the controller correction):

```sh
python3 -m unittest discover -s .github/scripts/tests \
  -p test_dal469_diagnostics.py -k DiagnosticFailureEvidenceTest -v
```

Result: exit 1, all three fixtures fail with missing saved replay files.
The same command after correction exits 0: three tests pass. Then only these
fixtures and four directly affected contracts were checked:

```sh
PYTHONPATH=.github/scripts/tests:.github/scripts python3 -W error::ResourceWarning -m unittest -v \
  test_dal469_diagnostics.DiagnosticFailureEvidenceTest \
  test_dal469_diagnostics.PrefixDiagnosticTest.test_measurement_stage_retains_gate_failures_and_binary_evidence \
  test_dal469_diagnostics.DiagnosticWorkflowTest.test_task_ref_controller_fixed_sources_and_always_upload_are_retained \
  test_dal469_diagnostics.DiagnosticCommandTest.test_sampling_failure_is_retained_while_the_other_target_still_runs \
  test_dal469_diagnostics.DiagnosticCommandTest.test_profiles_do_not_replace_failed_measurement_status
```

Result: exit 0, seven tests pass. Ruff on the two changed Python files and
`git diff --check` also pass. Full script tests, core-build, actionlint and local
performance comparisons were not repeated; the prior exact-head evidence is
retained with its original provenance. No workflow was started or merged.

Persistent fixture evidence under the task-owned `dal-469-blocker-evidence/`
directory contains each scenario's exact uploaded-result tree and a SHA-256
inventory, alongside RED/GREEN and affected-check logs in the delivery archive.
The failure and drift paths can therefore be inspected without a live runner.

Codacy remains a separate outstanding policy result. The inspected original
head has `ACTION_REQUIRED`, five annotations: dynamic subprocess plus four
complexity notices. This correction does not suppress them or undertake the
deferred maintenance refactor. The new head's checks must be reported from its
own snapshot; no policy clearance is inferred from these local tests. Remote
execution and focused reviewer revalidation remain orchestrator decisions.

## Remote diagnostic patch

The current handoff authorizes workflow/private diagnostic helpers, effective
tests and usage instructions only, with reviewer and remote execution scheduled
by the orchestrator. It supersedes the previous missing-execution-entry blocker,
not the original performance verdict. No additional local Intel comparisons
were run in this implementation turn.

Files changed:

- `.github/workflows/benchmarks.yml`: manual choice `mode=dal-469` selects an
  isolated Ubuntu 24.04 job. Schedule/default manual mode preserves both
  existing jobs and their concurrency group. The controller comes from the
  requested task ref; measured sources are the immutable original pair.
- `.github/scripts/diagnose_dal469.py`: fixed source/dependency verification,
  environment/ISA inspection, safe original-package selection or paired rebuild
  fallback, existing native gate and Python evaluator reuse, retained failures,
  binary/configuration identity checks, and profiles after all gate sampling.
- `.github/scripts/dal469_prefix_worker.py`: the prior eight-case prefix,
  preserving original case order, full-scale numerical checks, two warmups and
  one measured invocation per fresh process; explicit native-package validation.
- `.github/scripts/tests/test_dal469_diagnostics.py`: mode/parameter isolation,
  literal command arguments, archive safety, identity-before-execution, paired
  ISA fallback, timeout/failure retention, real fixture sampling and upload
  contract coverage.
- `.codex/references/benchmark-workflow.md`: invocation, selected coverage,
  fallback, evidence and profiler limitations. This active record retains the
  preceding investigation rather than replacing its evidence.

The entry tries the original GCC 14.2 Ubuntu package and pins CMake 3.31.6 and
Python 3.13.17. It records actual versions and CPU differences. Both original
module hashes must match before either is executed. Missing original artifacts,
missing AVX-512, interpreter mismatch, or SIGILL/import incompatibility selects
same-machine rebuilds for both Python sides. Correctness failures do not qualify
for fallback. Tape always rebuilds both sides because its original binaries were
not retained. File-prefix maps normalize both isolated sources to the controller
workspace; source/binary paths and actual hashes remain recorded.

Selected coverage remains one `tape_perf` executable (all existing cases because
its CLI has no case selector) and the eight-case Python prefix. Sampling remains
two rounds of ten alternating pairs, strict 4%, existing minima reduction,
Python's 90% paired support, and the existing unchanged-binary retry rule. Target
exit codes and original CI exit status 1 are retained separately; success of the
other target or profiling cannot clear a failed measurement step. GNU time,
optional perf/libmemusage and SIGILL GDB evidence are separate from gate timing.

RED/GREEN evidence:

- First focused run failed because `workflow_dispatch` had no diagnostic mode.
  Workflow changes made that contract pass.
- New private-helper tests failed while command/fallback/evidence operations
  were absent, then passed with the minimum implementation.
- Archive `.` rejection exposed an `IndexError`; validating nonempty path parts
  made the error-case test green. A head-hash mismatch exposed premature base
  execution; verifying both hashes before probes made that test green.
- `python3 -W error::ResourceWarning -m unittest discover -s .github/scripts/tests -p test_dal469_diagnostics.py -v`:
  16 focused tests pass, including synthetic child processes using unchanged
  samplers/evaluators. These fixtures do not execute DAL or compare Intel timings.
- `python3 -m unittest discover -s .github/scripts/tests -v`: 227 tests pass,
  six skipped. Existing scheduled-workflow command tests remain green.
- `actionlint -shellcheck= -pyflakes= .github/workflows/benchmarks.yml`:
  exit 0 with checksum-verified actionlint v1.7.12. Shellcheck is unavailable;
  the diagnostic build shell block is executed against a literal-argument
  CMake fixture by the focused tests.
- `ruff check --isolated` on the three new Python files: exit 0.
- `git diff --check`: exit 0. No source/dependency changes require additional
  library correctness tests or algorithm timing; previous evidence is retained.

Standalone component verification (build only, no benchmark execution):

```sh
cmake -S dal-cpp -B ../dal-469-standalone-core \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DDAL_BUILD_PUBLIC=OFF \
  -DDAL_CPP_BUILD_TESTS=OFF -DDAL_CPP_BUILD_EXAMPLES=OFF \
  -DDAL_CPP_BUILD_BENCHMARKS=ON -DCMAKE_CXX_COMPILER=g++-14
cmake --build ../dal-469-standalone-core --target tape_perf --parallel 6
```

Both commands exited 0 and `tape_perf` built. `CMAKE_HOME_DIRECTORY` points to
`dal-cpp`; the public component is absent from this standalone project.
`DAL_BUILD_PUBLIC=OFF` is retained in the cache as an unused top-level-workspace
option, which CMake warns about; no public target or facade dependency is added.
No library/CMake target source was modified.
The generated target inventory contains `tape_perf` and no `dal_public` target.

Remaining gaps: remote dispatch and profiling have deliberately not run pending
orchestrator review. Ubuntu 24.04 does not guarantee EPYC/AVX-512, exact GCC
package availability, original artifact retention, or perf access. Every such
difference/unavailable tool is retained; a rebuild or heterogeneous pass cannot
clear the original CI failure. The previous implementer and performancer
attachments remain the source for original sample and ISA findings.

## Original investigation

## Scope and source identity

- Assigned clean baseline: `d7eaefb7808ba32aecc870b7babc7cc9e7fc650a`.
- Dedicated task checkout: `agent/dal-implementer/ae51bbf718d9`, created by
  `multica repo checkout`; the orchestrator's local branch is not a remote ref.
- Failing scheduled comparison: head `d7eaefb7808ba32aecc870b7babc7cc9e7fc650a`
  versus first parent `72b3d15e04a9a42859b3f3b264b0f4ea635ddb20`.
- CI run: https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/runs/40411137529
- Linux evidence artifact: `benchmark-linux-40411137529-1`, artifact ID
  `12700672224`. Downloaded with authenticated `gh run download`.

The issue description contains only the benchmark repair request; the triggering
handoff and `.codex/references/benchmark-workflow.md` control this investigation.
No task-specific spec, API note, or critique exists. No public API decision has
been made.

## Confirmed CI failures

The CI build, both revisions' required Python workloads, head native smoke runs,
third-party comparisons, and Windows job passed. Two Linux performance steps failed:

| Case                   | Base minimum | Head minimum | Round deltas         | Paired support |
|------------------------|--------------|--------------|----------------------|----------------|
| Python vanilla default | 3.951634 ms  | 4.362873 ms  | +10.4068%, +10.5002% | 9/10, 9/10     |
| Tape width 16          | 0.652853 ms  | 0.702838 ms  | +7.6564%, +4.9634%   | —              |

The full case names are `mc.vanilla.double.default` and `tape_perf` /
`PropagateToStart multi-mode (50K steps, 16 results)`. The native gate uses round
minima without Python's additional paired-support criterion.

Both used two rounds of ten alternating process pairs, best-of-ten minima, and
the unchanged strict +4% threshold. Re-evaluating all retained raw samples with
the current gate code reproduced exactly these two failures; there is no evidence
of a parser or verdict-calculation defect.

The native tape executables have different SHA-256 hashes:

- Base: `985c793b57ab2e8faa1738809c3519f82f78689505127fe2970064115fc0dded`.
- Head: `52e4b08263e3814de6f52c01c1bf099825b2aa5669f5f30da6cfed3088997f33`.

The existing identical-binary retry correctly did not apply. Seven other gated
native targets were byte-identical; `rate_risk_perf` also changed and passed.
Neither retry eligibility nor acceptance thresholds should be relaxed on this
evidence.

Both Python A/A controls passed, but some individual round minima moved about
10%. For example, the baseline-module default vanilla case moved +3.4407% and
+9.7169% in its two rounds. This demonstrates variability, not clearance of the
original base/head failure.

The only native library source changed by the original comparison is
`dal-cpp/dal/curve/yclogdf.cpp`. The AAD, script, model, public valuation, and Python
binding sources used by the two failing algorithms did not change. Link-layout
or allocation/scheduling effects remain hypotheses, not established root causes.

## Reproduction environment and selection

CI: AMD EPYC 9V74, Ubuntu 24.04 hosted runner, GCC 14.2.0, CMake 3.31.6,
CPython 3.13, native AAD, static Release, native architecture enabled, four DAL
threads. The preserved CI Python module exited with `SIGILL` (132) on the local
machine before import completed, so it cannot be replayed directly here.

Local: Intel Core i9-13900HX under a shared WSL VM, GCC 14.3.0, CMake 4.2.3,
CPython 3.13.9, native AAD, static Release, native architecture enabled, four DAL
threads. Other compilation activity was observed during the builds. This is
not the original runner/toolchain and cannot clear its failure.

Selected cases:

- `tape_perf`: all cases in the affected executable, including result-width
  boundaries 1/4/10/16/64. Its numerical assertions remain enabled.
- Python `mc.vanilla.double.default`, `.tree`, `.compiled`: same 200,000-path
  full-scale workload, including numerical checks, one measured invocation and
  two warmups per process. Explicit evaluator modes distinguish the default
  route from its two implementations.
- Other native targets and the remaining Python cases are excluded from new
  timing: they did not fail, and no changed code justifies a full matrix.

Expansion reason recorded after the initial scoped runs: selecting only the
three vanilla cases omits process allocation history from preceding workloads.
An additional diagnostic therefore preserves the first eight full-scale cases
in their original order: four RNG cases, script construction/parsing, and the
three vanilla-double cases. It uses the same sampling, validation, and verdict
calculation. Workloads after the failing case cannot affect its timing and
remain excluded. Neither diagnostic claims a full-suite pass.

Local builds use separate detached `base-source` and `head-source` worktrees
under `../dal-469-investigation/`, with independent `base-build` and `head-build`
directories and identical pinned dependencies. Both receive the same
`-ffile-prefix-map=<base-source>=<head-source>` argument. They build only
`tape_perf` and `_dal`; no installed binaries are used.

## Verification and pending work

- `python3 -m unittest discover -s .github/scripts/tests -q`: 211 tests,
  passed, six skipped.
- Both isolated Release builds completed successfully.
- Shared head `dal-python/tests/test_benchmarks.py` against each build: base
  121 passed in 14.08s; head 121 passed in 11.56s.
- Each build's `tape_perf --diagnostics`: exit 0, numerical assertions passed.
  These runs overlapped compilation and their timings are excluded.
- Both isolated `dal/math/aad/tape.cpp.o` files have identical SHA-256
  `7db3dcf710db51e284fef7a7465a357eb69814286fef8909121a87e531fc280b`.
  The complete tape executables differ. This narrows investigation without
  proving the performance difference harmless.
- Native targeted gate: exit 0, all 27 cases passed, no retry. The original
  width-16 case measured 0.603625 ms base versus 0.602650 ms head; round changes
  were -0.51% and +0.68%.
- Vanilla-only diagnostic: exit 0, all three cases passed. Its default-route
  timings differed substantially from the original-prefix diagnostic, reinforcing
  the process-history coverage limitation rather than demonstrating improvement.
- Original-prefix diagnostic: exit 0, all eight cases passed. Default vanilla
  measured 1.830984 ms base versus 1.800980 ms head; round changes were -1.69%
  and +3.62%, paired support 4/10 in both rounds.
- At the initial investigation handoff, production changes, new regression tests, RED/GREEN implementation cycle,
  commits, PR, and reviewer handoff: none. There is no defensible failing
  implementation test yet. The only repository file added is this active record;
  helper scripts and raw evidence remain outside the source checkout.

Raw CI evidence, replay results, local build logs, and the scoped reproduction
drivers are retained in sibling task-owned evidence directories and a compact
issue attachment. Overall repair verdict: **inconclusive**; the original CI
failures remain valid. No public interface, numerical tolerance, workload size,
sampling count, confirmation rule, retry policy, or threshold was changed.
The task-owned isolated sources and builds are retained for the ongoing
investigation rather than removed after measurement; all foreground processes
have completed. No source changes exist in either detached worktree.

## Commands and next reproducibility gap

Both isolated sources use the same configure options:

```sh
cmake -S <source> -B <build> -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 \
  -DBUILD_SHARED_LIBS=OFF -DDAL_BUILD_PUBLIC=ON \
  -DDAL_PUBLIC_BUILD_TESTS=OFF -DDAL_CPP_BUILD_TESTS=OFF \
  -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_CPP_BUILD_BENCHMARKS=ON \
  -DDAL_BUILD_PYTHON=ON -DPython3_EXECUTABLE=<same Python 3.13> \
  -DCMAKE_CXX_FLAGS=-ffile-prefix-map=<base-source>=<head-source> \
  -DDAL_ENABLE_NATIVE_ARCH=ON
cmake --build <build> --target tape_perf _dal --parallel 6
```

After both builds complete, from the task source checkout:

```sh
DAL_NUM_THREADS=4 python3 .github/scripts/check_benchmark_regressions.py \
  --base-root ../dal-469-investigation/base-build \
  --head-root ../dal-469-investigation/head-build \
  --output-dir ../dal-469-investigation/tape-paired \
  --benchmarks tape_perf --samples 10 --confirmation-rounds 2 \
  --threshold-percent 4 --retry-identical-binaries
python3 ../dal-469-investigation/reproduce_vanilla.py
python3 ../dal-469-investigation/reproduce_vanilla.py --prefix
```

The Python helpers call the existing full-scale workloads and existing evaluator
without modifying either. They alternate process order for 20 pairs, retain all
raw reports, and preserve the 90% paired-support requirement. The prefix helper
executes exactly the original first eight workloads. The scheduled full Python
gate itself has no case filter and was not changed or claimed to pass.

The missing evidence is a compatible reproduction of the original EPYC/GCC
14.2 native binaries and their two failing workloads. A scoped run there can
retain the existing sampling policy while adding hotspot/allocation diagnostics
or additional samples if variability persists. Local Intel/GCC 14.3 results,
source equality, and passing A/A controls do not justify changing production
code or dismissing the CI failure. No additional specialists or full-suite CI
runs were triggered.

## Local per-case measurements

These are diagnostics on the shared local WSL2 host. A local pass does not
clear the CI failure. No production implementation changed between these runs.

### Native tape (27 cases)

| Case                                                 | Base min ms | Head min ms | Change | Round changes  | Local gate |
|------------------------------------------------------|-------------|-------------|--------|----------------|------------|
| Clear + re-record (100K nodes)                       | 0.865706    | 0.866157    | 0.05%  | +0.05%, +0.28% | pass       |
| PropagateToStart (100K nodes)                        | 0.386430    | 0.387136    | 0.18%  | +0.18%, -0.00% | pass       |
| PropagateToStart multi-mode (100K nodes, 10 results) | 0.456939    | 0.461323    | 0.96%  | +1.01%, +0.77% | pass       |
| PropagateToStart multi-mode (50K steps, 1 result)    | 0.364568    | 0.365519    | 0.26%  | +0.33%, -0.38% | pass       |
| PropagateToStart multi-mode (50K steps, 16 results)  | 0.603625    | 0.602650    | -0.16% | -0.51%, +0.68% | pass       |
| PropagateToStart multi-mode (50K steps, 4 results)   | 0.380770    | 0.381389    | 0.16%  | -0.59%, +0.80% | pass       |
| PropagateToStart multi-mode (50K steps, 64 results)  | 4.614000    | 4.726000    | 2.43%  | +3.42%, +0.98% | pass       |
| PropagateToStart passive constants (50K steps)       | 0.149009    | 0.148809    | -0.13% | +0.03%, -0.13% | pass       |
| Rewind + passive-constant recording (50K steps)      | 0.245673    | 0.245030    | -0.26% | +0.36%, -0.26% | pass       |
| Rewind + re-record (100K nodes)                      | 0.578419    | 0.584339    | 1.02%  | +4.05%, -0.86% | pass       |
| ZeroAdjoints sweep (100K nodes)                      | 0.111913    | 0.112116    | 0.18%  | +0.26%, +0.17% | pass       |
| cashflows full                                       | 0.801713    | 0.819037    | 2.16%  | -3.47%, +2.16% | pass       |
| cashflows full-warm                                  | 0.666280    | 0.684229    | 2.69%  | +8.06%, +1.24% | pass       |
| cashflows segmented                                  | 0.959016    | 0.957724    | -0.13% | +1.13%, -0.13% | pass       |
| cashflows segmented-warm                             | 0.916321    | 0.939447    | 2.52%  | +6.88%, +2.50% | pass       |
| long256 full                                         | 0.663740    | 0.655367    | -1.26% | +1.57%, -1.26% | pass       |
| long256 full-warm                                    | 0.532415    | 0.525392    | -1.32% | -0.96%, -1.32% | pass       |
| long256 segmented                                    | 0.823016    | 0.810167    | -1.56% | +1.58%, -1.56% | pass       |
| long256 segmented-warm                               | 0.795912    | 0.767849    | -3.53% | -2.07%, -3.53% | pass       |
| long64 full                                          | 0.637488    | 0.636005    | -0.23% | -2.04%, +0.22% | pass       |
| long64 full-warm                                     | 0.503638    | 0.499876    | -0.75% | -1.36%, -0.75% | pass       |
| long64 segmented                                     | 0.815155    | 0.818436    | 0.40%  | +1.90%, -0.00% | pass       |
| long64 segmented-warm                                | 0.780890    | 0.786759    | 0.75%  | +0.62%, +0.75% | pass       |
| short full                                           | 0.035832    | 0.036088    | 0.71%  | +0.71%, +0.57% | pass       |
| short full-warm                                      | 0.001196    | 0.001186    | -0.84% | -1.00%, -0.08% | pass       |
| short segmented                                      | 0.036815    | 0.037615    | 2.17%  | +1.21%, +2.55% | pass       |
| short segmented-warm                                 | 0.002406    | 0.002390    | -0.67% | -0.67%, -0.21% | pass       |

### Vanilla-only diagnostic (3 cases)

| Case                       | Base min ms | Head min ms | Change  | Round changes    | Local gate |
|----------------------------|-------------|-------------|---------|------------------|------------|
| mc.vanilla.double.compiled | 1.848212    | 1.866270    | 0.98%   | +3.50%, +0.98%   | pass       |
| mc.vanilla.double.default  | 3.878193    | 1.800960    | -53.56% | -46.10%, -53.56% | pass       |
| mc.vanilla.double.tree     | 2.057611    | 2.026663    | -1.50%  | -2.75%, -1.50%   | pass       |

The large apparent default-route improvement disappears when preserving the
original workload prefix below; it is not accepted as an algorithm improvement.

### Original Python workload prefix (8 cases)

| Case                       | Base min ms | Head min ms | Change | Round changes  | Local gate |
|----------------------------|-------------|-------------|--------|----------------|------------|
| mc.vanilla.double.compiled | 1.818480    | 1.779525    | -2.14% | -2.14%, -1.12% | pass       |
| mc.vanilla.double.default  | 1.830984    | 1.800980    | -1.64% | -1.69%, +3.62% | pass       |
| mc.vanilla.double.tree     | 2.020009    | 2.024790    | 0.24%  | +3.03%, -1.89% | pass       |
| rng.mrg32_normal           | 42.610194   | 43.158767   | 1.29%  | +1.75%, -0.94% | pass       |
| rng.sobol_normal_fast      | 4.849033    | 4.891727    | 0.88%  | -0.07%, +1.04% | pass       |
| rng.sobol_normal_precise   | 37.003447   | 36.556514   | -1.21% | -1.21%, +0.07% | pass       |
| rng.sobol_uniform          | 1.404963    | 1.451060    | 3.28%  | +3.28%, -1.56% | pass       |
| script.construct_and_parse | 1.020233    | 0.990132    | -2.95% | -2.95%, -2.77% | pass       |
