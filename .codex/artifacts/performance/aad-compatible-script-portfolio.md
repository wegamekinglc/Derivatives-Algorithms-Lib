# Compatible portfolio acceptance and entry cost

Status: implementation, installed consumers and comparative performance are
locally accepted. The final publication commit must pass its own CI, Codacy and
paginated review audits before merging PR
[#487](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/487).
New-entry costs identify measured targets for subsequent P02/P03 work.

## Sources and environment

Measurements use `9cc9fe0aab274764448f294496ebf4442bd33061` plus the recorded
two-helper preparation-inlining patch. Private helpers `Complete` and
`PlanBeforeHistory` retain the former preparation entry's inlining behavior
with `FORCE_INLINE`; APIs, fixtures and numerical programs are unchanged.
Task and isolated preparation sources match byte-for-byte. The patch fingerprint
is in `aad-portfolio-prepare-inline-candidate-patch-02.json`.

Merge-base is `1c9273c952aec10d5adc439cdfa04b87b18465d7`. The cached baseline
was built at `1785162cc6476c6bed26910a7da7138f2756b591`; its entire Git tree
equals the merge-base tree. Both builds use GCC 15.2, CMake 4.2.3, C++17
Release/static native AAD, portable Eigen and native-architecture OFF.
Lifetime/profiling are OFF for timing. The shared WSL2 host reports an Intel
i9-13900HX. Both sides use affinity `0,2,4,6` and four DAL workers, except
the two explicitly serial confirmations, which use caller CPU 0.

Evidence root is `/tmp/dal-aad-evidence`, backed by
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.
`aad-portfolio-inline-candidate-environment-03.json` records current sources,
libraries, binaries and settings. Fresh relinking leaves all nine gate
executables and the curve executable SHA-256 identical to their accepted
versions; `aad-portfolio-inline-candidate-binary-reuse-01.json` proves reuse
of those 100 measurements. The supporting archive changed and is not claimed
identical. Failed measurements and their historical fingerprints remain.

## Existing functionality

The nine-target gate and original supplemental workloads remain unchanged.
Acceptance uses two rounds, ten alternating process pairs per side and round,
minimum reduction, and failure only above 4% in both rounds. Individual samples
and verdicts remain in each group's `results.json` and raw process logs.

| Coverage                           | Cases | Accepted evidence                                                     |
|------------------------------------|------:|-----------------------------------------------------------------------|
| Nine standard targets              | 75    | `aad-portfolio-isolated-nine-01`, identical relinked binaries         |
| Curve calibration and queries      | 25    | `aad-portfolio-isolated-curve-01`, identical relinked binary          |
| Existing scalar risk               | 8     | `aad-portfolio-inline-candidate-scalar-pairs-01`                      |
| Existing weighted risk             | 8     | `aad-portfolio-inline-candidate-weighted-pairs-01`                    |
| MC, GSR market risk and LSM replay | 42    | Accepted cases in `aad-portfolio-inline-candidate-mc-pairs-01`        |
| Two serial GSR/LSM calculations    | 2     | `aad-portfolio-inline-candidate-serial-mc-pairs-01`, fixed caller CPU |
| Total                              | 160   | All accepted under the stated workload and affinity scopes            |

The Sobol precise-opt-in/fast ratio is 9.15x, within the unchanged 10x ceiling.

### Repair and retained failures

Initial complete runs accept 157/160 and 159/160 cases. Same-binary controls
show substantial short-timer noise. A 64-request diagnostic establishes
steady-state timing only; it does not replace original single-request acceptance.
Its contaminated run and unrelated host workload observations are retained.
The original single-request control passes, while the unrepaired comparison
fails one-component compiled weighted risk at +10.99%/+4.86% and four-component
compiled risk at +4.05%/+4.20%. These failures motivate the inlining repair.

After the repair, the original single-request driver passes all eight weighted
cases. It uses the original timer and frozen ten-pair helper, without extra
requests or warm-up changes:

| Components | Evaluator | Round 1 | Round 2 | Result |
|-----------:|-----------|--------:|--------:|--------|
| 1          | tree      | +9.25%  | -1.79%  | pass   |
| 1          | compiled  | -8.43%  | -4.37%  | pass   |
| 4          | tree      | -1.02%  | +1.04%  | pass   |
| 4          | compiled  | -5.02%  | -1.73%  | pass   |
| 16         | tree      | +1.00%  | -6.11%  | pass   |
| 16         | compiled  | -2.25%  | +2.44%  | pass   |
| 64         | tree      | +1.45%  | -1.30%  | pass   |
| 64         | compiled  | +3.14%  | -4.17%  | pass   |

All eight original scalar cases pass. The unchanged MC executables pass 42/44
cases at four-core affinity. The two failures are serial GSR path generation
(+4.93%/+8.42%) and degree-three LSM regression (+5.45%/+6.59%); neither calls
script preparation. Fixed caller CPU 0, with unchanged executables, timers,
warm-ups, paths, samples and threshold, gives +2.919%/+5.716% and
+3.251%/+0.691%, respectively, passing the original two-round rule.
Only those two serial metrics are accepted from that run; parallel MC
acceptance uses the original four-core run. Its failed result is preserved.
These host/configuration results do not promise identical timings elsewhere.

## New portfolio entry cost

`aad-portfolio-inline-entry-pairs-01` contains 130 matched-work cases, three
alternating process pairs each. Values, objectives and selected global risks
match independent existing calls within 1e-10; shapes, work counts and finite
recording/scratch peaks pass. Coverage includes five shapes, tree/compiled,
one/four workers, native/passive, weighted/blocked attribution, widths 1/2/3/8
and all six supported model families.

Both sides start from the same sealed product/model snapshots. Initial sealing
is measured separately. Timing includes preparation, cloning/history,
simulation, reverse, reduction and owning projection. Portfolio calls enforce
64 MiB recording and 16 MiB scratch per worker. These additional-entry costs
are informational and separate from old-entry acceptance.

Representative BS cases use 257 paths, compiled evaluation, four workers,
one trade for `one` and eight otherwise. Attribution uses width three.

| Shape           | Result      | Independent μs | Portfolio μs | Ratio | Groups | Scenarios | Evaluations |
|-----------------|-------------|---------------:|-------------:|------:|-------:|----------:|------------:|
| one             | weighted    | 108.330        | 268.770      | 2.481 | 1      | 257       | 257         |
| one             | attribution | 165.502        | 655.513      | 3.961 | 1      | 257       | 257         |
| compatible      | weighted    | 732.113        | 697.099      | 0.952 | 1      | 257       | 2056        |
| compatible      | attribution | 1113.150       | 915.332      | 0.822 | 1      | 771       | 2056        |
| mixed meshes    | weighted    | 618.870        | 797.649      | 1.289 | 2      | 514       | 2056        |
| mixed meshes    | attribution | 1151.113       | 1066.197     | 0.926 | 2      | 1028      | 2056        |
| distinct owners | weighted    | 665.168        | 1501.415     | 2.257 | 8      | 2056      | 2056        |
| distinct owners | attribution | 1087.880       | 1569.647     | 1.443 | 8      | 2056      | 2056        |
| private history | weighted    | 862.718        | 842.567      | 0.977 | 1      | 257       | 2056        |
| private history | attribution | 1337.598       | 1126.336     | 0.842 | 1      | 771       | 2056        |

These rows retain at most 7,864,320 recording bytes and 107,300 scratch bytes.
All 130 ratios range from 0.317 to 5.333. One-trade and distinct-owner requests
can cost substantially more; sharing does not establish a general speedup.
P02/P03 should measure setup/capacity reuse and width choices while preserving
ownership, original meshes and aggregate admission. Full-entry timing does not
isolate a particular phase. Earlier pre-repair costs remain retained.

## Runtime and publication boundary

The complete OFF suite passes 2,698 tests, including 21 benchmark smokes,
34 examples and all 1,166 Python cases. After the inlining repair, 875 affected
C++/Excel cases and 126 Python cases pass. Refreshed standard installation passes
the documented C++ consumer and all 126 affected Python cases. Approved strict
OFF/combined ON syntax and formatting checks pass.

At published head `9cc9fe0a`, all 35 checks succeed, with zero Codacy annotations
and unresolved review threads. Actual extended runtime passes all four modes.
Actual MSVC runtime passes all seven new typed/raw/registration cases in each
of four modes and installed Excel suites 3/3 each. All six ASan/UBSan/TSan jobs
execute and pass all 37 core plus 41 public portfolio cases. The TSan oracle
repair retains independent scalar risks, exact private risks, work/history and
finite-budget assertions; no race report appears.

The final source commit must pass its own complete checks and actual platform
runtime. Earlier green results establish coverage, not acceptance of a different
commit. Repeat paginated exact-head audits immediately before the guarded merge;
begin P02/P03 in a new PR only after the merge is verified.
