# DAL-201 isolated hosted diagnostic package

## Cancellation repair awaiting independent review

The DAL-227 P2 cancellation finding is repaired locally. This supersedes the
rejected diagnostic tree `4aea255d32293d53bc1155440845692d6a52d70a`, originally
commit `dfc4ca434927c5f565a5cd504242557625b9f01e`. The supplied portable patch
was SHA256-verified and reconstructed as local commit
`2d9be658aa166dde3552f862da3a885c8425e18e` with exactly that rejected tree before
editing. The attached publication record supplies the new exact commit/tree,
complete patch from the published product head, minimal repair delta and hashes.
The previous candidate was unavailable in this fresh workspace; no unpublished
content was discarded. Its full 11-file tree was preserved.

Only `diagnostics.py`, new `test_cancellation.py` and these two active reports
change relative to that candidate. All script paths in this section are under
`.github/scripts/dal201_diagnostics/`. The workflow, fixed identities, original
14 tests, workload/measurement code, product and existing CI remain byte-identical.

SIGINT/SIGTERM handlers now retain the first cancellation request, with nested
main/process scopes restoring prior handlers afterward. Cancellation is raised
at controlled checkpoints rather than asynchronously during spawn, group
collection or evidence writes. Waiting checks every 50 ms. `waitid(WNOWAIT)`
retains the exited leader until group termination, preventing its group ID from
being reused before signaling. Every exit after successful spawn enters group
cleanup, including timeout, KeyboardInterrupt, SystemExit, other errors and a
normal leader exit with descendants. Cleanup kills only the new worker group,
reaps the direct child, then checks Linux `/proc` until no live group member
remains. Exited zombies cannot write output; their owning parent/init reaps them.

`process.json` is written in `finally`, after closing raw stdout/stderr. Signal
cancellation records `interrupted` and the signal; timeout remains `timeout`;
other exceptions record failure. Main records an interrupted/failed summary and
the final manifest after process collection, preserving partial output. A
cancellation arriving while hashing the manifest also updates the summary and
recomputes its hashes. SIGINT/SIGTERM return 130/143. Repeated cancellation does
not interrupt cleanup or replace the first signal.

The original execution deadline remains 30 minutes and the job 40 minutes,
with 80 controls, four phase workers and at most two perf workers. Cleanup has
a bounded tail of up to five seconds for direct-child reaping and two seconds
for descendant stop checks; failure is reported rather than silently called
successful cleanup. Final file hashing follows. This Linux harness assumes
workers retain their inherited process group. SIGKILL, runner loss, storage
failure and uninterruptible kernel tasks cannot promise complete final evidence;
upload remains subject to runner and Actions availability.

### Fresh RED/GREEN evidence

All new test executions used **CPython 3.13.15**, with PyYAML 6.0.3. The attached
evidence retains initial failures, focused passes, final commands and raw process
directories. Tests run real child processes through `main` and `run_process`,
substituting only artifact preparation and the workload for small deterministic
workers. The worker spawns a descendant that ignores SIGINT/SIGTERM. Each test
adopts, kills as necessary and reaps its own descendants, even on assertion
failure, and verifies an unrelated sentinel survives.

- Initial RED: `python -m unittest discover -s .github/scripts/dal201_diagnostics
  -p test_cancellation.py -v` failed all three initial cases: SIGINT and SIGTERM
  left both worker and descendant live with `starting` records; normal exit
  left a live descendant with a passed record. Reconfirmed against the original
  `diagnostics.py` in an isolated evidence copy: exit 1, three failures.
- Initial GREEN: the same three tests passed after group cleanup. Seven process
  cases then passed, adding repeated cancellation during cleanup, direct
  KeyboardInterrupt, SystemExit and descendant timeout.
- Finalization RED: `python -m unittest discover -s
  .github/scripts/dal201_diagnostics -p test_cancellation.py -k during_manifest
  -v` failed with `0 != 143`. The identical test passed after final status/hash
  reconciliation. No assertion was weakened.
- Full GREEN: `python -m unittest discover -s
  .github/scripts/dal201_diagnostics -v`: **22/22 pass**, including all original
  14 identity, ISA/ABI, child SIGILL/timeout, measurement and workflow tests.
  The eight added regressions cover all cancellation/descendant edges above.
- In the final run, both parent signals exited in approximately 0.032 seconds;
  all seven real process cases had zero live descendants and consistent hashes.
  Timeout exited in approximately 0.315 seconds; normal exit approximately
  0.032 seconds. These are local lifecycle checks, not hosted performance data.
- `actionlint 1.7.12 .github/workflows/dal-201-hosted-diagnostics.yml` passes.
  The existing YAML test checks all shell blocks with `bash -n`; documentation
  checks pass for 56 Markdown files, and `git diff --check` passes.

No product rebuild, full benchmark panel, hosted execution or push occurred.
DAL-201 must route the new exact tree to DAL-227 for independent review before
publishing this diagnostic branch. The original hosted paired **87/90** remains
failed; A/A **90/90** does not waive it. PayoffRoot's missing exact boundary is
still explicit and, per DAL-227, does not block one bounded diagnostic after
cancellation review. CPU equivalence, ISA/import compatibility on the actual
runner, perf permissions and successful upload remain unverified hosted risks.

## Preserved diagnostic design

This is a **local, unpushed review candidate**, not a product repair or an
acceptance gate. The original hosted paired result remains **87/90 failed**;
both original hosted A/A groups remain 90/90. Earlier local 87/90 and 88/90
failures remain valid evidence. No new Actions run was requested or started.

The diagnostic branch is `fix/dal-201-hosted-phase-diagnostics`, starting at
`12353bb773ad16de37405f6e37a312867c918acd`, tree
`466d55a4ed7e215ca79877679173022aceee47e8`. The F4 product branch and
https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/371 are unchanged.
The attached publication record identifies the final local commit/tree and
portable patch hash. The previous detailed hosted attribution report remains
DAL-224 attachment `01a0a071-83ec-715b-9d86-71413bd2fd92`; its full evidence
archive is `01a0a073-e882-7dde-9413-71447418d219`.

## Design and fixed identities

The additional workflow triggers only on a push to the exact diagnostic branch.
It has one `ubuntu-latest` job, a 40-minute timeout, read-only `contents` and
`actions` permissions, no user secrets, no PR trigger, and no dispatch
dependency. Checkout, setup-uv and upload-artifact use the repository's existing
SHA pins. Checkout does not persist credentials. The automatic read-only job
token is scoped to the download step. Evidence uploads with `always()` after
success, preflight failure, tool failure, validator failure or timeout, subject
to Actions' own runner/upload availability.

The original artifact is fixed by ID **10352292456**, run **34851796577**, name
`benchmark-linux-34851796577-1`. The downloader checks API identity, expiry and
digest before downloading that exact artifact ID. Its downloaded ZIP SHA256 is
`dbc1af632c094fcc406c65e2dc33c6ca6c13195604a6763e9b090f179078adec`.
This was verified locally with an actual authenticated download.

The GitHub API names the PR head `12353bb773ad16de37405f6e37a312867c918acd`.
The benchmark reports name tested merge source
`88ecd58bb33529a8f27387b47645624fd43b0863`; its tree equals the published tree.
Baseline source is `8ee1b09dcaf531add9695d466941026da87a4942`. These distinct
identities are checked separately, never equated by assumption.

`identity.json` pins all 127 original input files: environment, three aggregate
reports, all 120 original process reports, and both package archives. It also
pins all 24 suite files and relevant native hook headers. The compile source
must be a clean checkout of the exact published product tree. The workflow
checks it out separately as `product-source`; no DAL binary is rebuilt.

Archived native extension SHA256:

- Base: `67a0a4650238df0287b919d1bf3388188f86833167c18204c64040ffb5ab49fa`.
- Head: `63a2fc6130f0cc0682ed6de9055b81369872541ebd468e48e975de88a96937b8`.

Extraction rejects escaping paths, links, duplicate names and oversized archives.
Each side gets a separate package directory and a byte-for-byte copied package
control. Binary hashes are checked before import and in every worker report.

## Execution preflight and budget

CPython **3.13.15** is requested, and SOABI must be
`cpython-313-x86_64-linux-gnu`. Diagnostic Python dependencies are pinned to
NumPy 2.4.6 and PyYAML 6.0.3; these are recorded dependencies, not a claim that
every original hosted Python dependency was captured. The full original build
environment is retained. The new runner image, CPU flags, Python version and
diagnostic compiler version are captured separately.

The CPU precheck requires AVX, AVX2, AVX512F/VL/DQ/BW on every reported CPU.
This is a conservative necessary check, **not proof of every instruction's
compatibility**. It is followed by an actual isolated import of each unchanged
extension, with a 30-second timeout. Subsequent workload SIGILL also fails
explicitly. No ISA failure causes a replacement build, emulation, affinity
change, runner polling or retry. `ubuntu-latest` does not guarantee the original
Xeon 6973P-C; a different compatible CPU is recorded as a limitation.

The harness has a fixed 30-minute execution deadline and bounded process
timeouts. Workloads execute serially, with `DAL_NUM_THREADS=4`, always in the
same evidence directory. The native libraries and original suite are unchanged.

- 80 uninstrumented workload processes: 20 each for base, head, base-copy and
  head-copy. Arm order reverses after every outer sample. Results retain two
  predeclared groups of ten, all samples and their minima; no new gate verdict.
- Four instrumented processes: base/head, then head/base. These have separate
  output directories and are never pooled into uninstrumented timings.
- At most two perf workload processes: one base and one head, only if a single
  `cpu-clock:u` DWARF-recording permission probe succeeds. No retries. Missing
  perf or insufficient permissions retain raw errors and use native hooks.
- Two import processes and one perf capability process are preflight, not
  benchmark samples. Compiler/binutils commands have separate timeouts.

Every workload process runs the full original **36-case prefix** through
`xccy.joint.ANALYTIC.solve`. This preserves the original nine-case MC prefix
and every intervening calibration case, in order. Each case retains original
full workload, one validation invocation, two warmups, one measured invocation,
and its unchanged validator. It is not the complete 90-case acceptance suite.

## Measurement boundaries and their limits

Three targets are reported independently: `mc.vanilla.aad.compiled`,
`xccy.joint.ANALYTIC.diagnostics`, and `xccy.joint.ANALYTIC.solve`.
The raw original failures are +4.09%/+5.46%, +8.95%/+5.93%, and +6.01%/+5.22%
respectively. No criterion, threshold, tolerance or required check is changed.

The ELF resolver follows the exact private class RTTI string to its typeinfo
relocation and vtable. It requires unique matches, exact unwind FDE starts and
executable load-segment containment. The observed original Gradient ranges are
base `0x447b00..0x448bc3` and head `0x454880..0x455943`, both 4,291 bytes.
It retains raw dynamic symbols, relocations, FDEs and disassembly. Addresses
are resolved afresh from each hash-verified ELF, not guessed from proximity.

Only the diagnostic shared object is compiled. A version script exports exactly
three marker/install functions and three named DAL boundary interposers. It
uses the existing RegisterCurveParameters, NewRecording and HarvestCurveJacobian
functions; all calculations still delegate to the original loaded library.
An instrumented process replaces only the verified Gradient and double-F
vtable slots in its private memory mapping, checking runtime load bias and both
original pointers, then restores the page to read-only. No on-disk native
binary or product source is modified. Missing hooks or changed counts fail.

Boundaries are:

- **Target inclusive:** native target call plus Python/native conversion.
  Validation and workload preparation are outside the marker.
- **Gradient inclusive:** verified virtual Gradient entry through return.
- **Register:** original RegisterCurveParameters call through return.
- **Recording:** original NewRecording return through Harvest entry; includes
  active Residuals evaluation and boundary-call overhead.
- **Harvest:** original HarvestCurveJacobian entry through return. Input,
  residual and Jacobian fingerprinting is outside this timer.
- **Double residual:** verified virtual F entry through return. Local tests
  observe seven intercepted F calls per calibration while public solver
  diagnostics report eight evaluations; the unobserved/devirtualized work is
  deliberately left in the remainder, not mislabeled as absent work.
- **Unattributed remainder:** target minus inclusive Gradient and intercepted
  double-F time. This includes solver/diagnostic assembly, unobserved residual
  work, allocation, conversion and instrumentation overhead. It is not a pure
  solver microkernel measurement. Negative or overlapping phase totals fail.

Counters and fingerprints verify 3/1 Gradient/Register/recording/harvest events
per diagnostics/solve invocation, each 25 by 25 with 2,511 nodes. All 16 harvest
fingerprints must agree across the four instrumented processes. Solver reports
must retain eight evaluations and convergence. Original validators remain the
authority for numerical results. No finite-difference or candidate optimization
is substituted.

**Root attribution remains an explicit gap.** The hosted dynamic symbol tables
have no separately named PayoffRoot entry. This package collects compiled-MC
target cost and optional raw perf instruction/stack samples, and preserves the
full linked disassembly. It does not label a guessed address or the entire
MC call as root cost. The next evidence needed for root nanoseconds is an
independently reviewed instruction-ownership/boundary map for these exact
stripped binaries; source-level instrumentation would require a separately
identified diagnostic rebuild and could not replace archived-binary timing.
Zero root samples or unavailable perf cannot close this gap. Final status is
`collected-with-root-attribution-gap`, never acceptance passed.

The native hooks change linkage, two vtable targets, call overhead and cache
layout; fingerprinting lengthens inclusive Gradient/target durations. Timers
add overhead and can cross scheduling boundaries. Those effects are visible in
separate plain versus instrumented records. The package supports phase
localization, not an unbiased estimate of the uninstrumented overhead. The
original gate is not rerun or repaired by this workflow.

## Output and retention contract

`dal201-evidence` contains the original ZIP and extracted original evidence,
download/API status, toolchain/setup logs, harness test log and `replay/`.
The replay directory contains:

- `summary.json` (`dal201.diagnostics/1`): explicit failure or partial-collection
  status, fixed budget and `acceptance_gate: false`.
- `identity.json`, `preflight.json` (`dal201.preflight/1`) and `cpuinfo.txt`.
- Per-process `process.json`, raw stdout/stderr and original benchmark
  `results.json`/`summary.md`. Timeouts retain output before killing and reaping
  that process group. No successful result file is reused from an earlier run.
- `targets.json` (`dal201.targets/1`): all 20 raw values and two minima for
  each target and each of the four uninstrumented arms.
- `elf-base/`, `elf-head/`, hook source/binary hashes and build command.
- Separate instrumented `phases.jsonl`, `phase-summary.json`, runtime maps and
  solver audit. JSONL records include phase, tag, invocation, Gradient index,
  nanoseconds, rows/cols/nodes and decimal-string 64-bit work fingerprints.
  Tags 1/2/3 correspond to the three named targets in order.
- `phase-coverage.json`, perf availability/probe logs, and available raw
  `perf-*.data` plus `perf script` output. No-symbol regions remain unresolved.
- `manifest.json` (`dal201.manifest/1`): SHA256 of every retained replay file
  except the manifest itself. The original download has its separately pinned
  ZIP digest. Upload retention is 30 days; failed samples are never deleted.

## Local verification and review handoff

RED/GREEN command: `python3 -m unittest discover -s
.github/scripts/dal201_diagnostics -v`. Initial identity and boundary tests
failed on the absent modules; the nested-boundary regression failed on the
absent summarizer. The first summarizer exposed a KeyError on a target row;
filtering by child phase before reading its Gradient index corrected that
without changing the test. Original package result: **14 tests pass**; the
cancellation repair above adds eight regressions and now passes **22 tests**.

Coverage includes wrong binary hash, missing ISA, wrong ABI, SIGILL, timeout
with retained partial stdout, archive traversal, GitHub head versus merge
identity, ambiguous/missing function boundaries, missed harvests, impossible
nested timing, unchanged validators and fixed schedule/prefix. Workflow tests
parse YAML, validate exact trigger/read-only permissions/existing action pins
and failure upload, and syntax-check every shell block. No actionlint executable
was available for the original delivery; the cancellation repair now passes
actionlint 1.7.12. Actual Actions execution remains deferred to review.

Actual download and ZIP hash validation pass. All 127 original input hashes and
24 suite hashes pass. Local original-binary preflight fails explicitly on
missing AVX512F/VL/DQ/BW and retains a failure summary and manifest. Both original
ELF Gradient ranges resolve to the prior verified addresses without execution.

The final probe compiles with GCC 14 and repository formatting. Four explicitly
**LOCAL functional-validation** processes cover compatible baseline/head, each
plain and instrumented: all 36-case validators pass, all expected hook counts
and nested boundaries pass, and corresponding 16 harvest fingerprints match.
Earlier development attempts are retained in the attached validation evidence.
None is a hosted performance sample, a full gate or independent F4 acceptance.
No unchanged product correctness/backend suite was rebuilt or rerun.

Review the portable patch before execution. After DAL-227 approval, the parent
can apply it to the exact starting head and push this branch once:

```bash
git switch -c fix/dal-201-hosted-phase-diagnostics 12353bb773ad16de37405f6e37a312867c918acd
git am dal-224-hosted-diagnostics.patch
git rev-parse HEAD 'HEAD^{tree}'
git push origin HEAD:refs/heads/fix/dal-201-hosted-phase-diagnostics
gh run list --workflow dal-201-hosted-diagnostics.yml --branch fix/dal-201-hosted-phase-diagnostics --limit 1 --json databaseId,headSha,url,status
```

After applying the patch, compare its tree with the attached reviewed tree;
`git am` may change commit metadata. Match the returned run's head SHA to the
actual diagnostic commit pushed, and record its real ID/URL. Do not watch,
repeatedly poll, rerun failures, or create a PR solely
to make this workflow execute. If the one immediate read has not discovered a
run yet, report that rather than inventing a run ID. The present delivery has
**no hosted run ID**, because it has not been pushed.

For manual preflight on an already provisioned compatible host, use the pinned
product checkout, Python ABI and original artifact:

```bash
python .github/scripts/dal201_diagnostics/fetch_artifact.py --output dal201-evidence
python .github/scripts/dal201_diagnostics/diagnostics.py --artifact dal201-evidence/original --source product-source --output dal201-evidence/replay --preflight-only
```

Omit `--preflight-only` for the single fixed-budget diagnostic execution after
review. DAL-224 returns to `in_review` for this package; DAL-201 arranges
DAL-227 read-only review and any subsequent execution. Future supported product
repairs still require independent testing, documentation/CHANGELOG decision and
final independent review. No F4 merge, closure, F5 or DAL-223 work is included.
