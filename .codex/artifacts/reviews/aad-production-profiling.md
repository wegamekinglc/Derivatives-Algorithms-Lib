# Native production profiling review

Verdict: Request Changes pending corrected-head Windows validation.
P01 acceptance remains open for affected production performance, scaling/resources, overhead,
and exact publication-head CI. The overall implementation goal remains active.

## Findings

The b37b49f MSVC profiling and combined configurations fail compilation: the
private windows.h include defines VOID before DAL's generated StackInfoType_
enum is parsed. The local correction undefines that macro after the Windows
include, without altering public headers, algorithms or non-Windows branches.
Both raw failing job logs are retained under the evidence root. Successful
corrected-head Windows compilation and runtime remain required.

The default MSVC leg independently fails its second CMake configure after 2,311
functional cases pass. Vendored pybind11 retains a cached FOUND flag across
processes, while pybind11_add_module must load again. Independent installed-
Python configuration reproduces the failure. Guarding discovery with command
availability restores two consecutive configure passes, retaining lookup order
and the existing CI reconfiguration check. Vendored Python 3.12 standalone
binding configures twice, builds and passes 792 tests with one absent-monorepo-
native-module skip. Refreshed OFF/ON/combined core builds, ON/combined 20 focused
tests and OFF/ON six CLI contracts each pass. Corrected-head Windows validation
remains open.

No additional blocking correctness finding remains in the inspected profiling collector,
task ownership, native storage hooks, ordinary/LSM call-site changes, benchmark
CLI, configuration/export, tests and documentation. Default-off instrumentation
is removed at compile time; diagnostic reports remain request-owned and are
read after task draining. No RNG, batching, payoff, policy, normalization or
reduction algorithm changes are intended.

Scope CPU inclusion, same-thread nested-task subtraction, phase overlap and
componentwise storage maxima have explicit contracts. Selected array payloads
exclude private evaluator/model/solver/diagnostic storage. Process RSS and sums
of per-thread maxima require separate interpretation. Real outputs use scalar
single-output execution; this increment does not implement portfolio VJP.

## Tests

- Latest default/profiling/combined builds succeed. Non-benchmark functionality
  passes 2,332/2,351/2,380 cases, with 793 Python tests in each configuration.
- Profiling and combined each pass 20 focused tests; LSM price/every risk remain
  bitwise identical. Ordinary parallel risk uses rel/abs 1e-10.
- Both installed consumers pass in all three configurations. CLI contracts
  pass OFF/ON; all 38 representative diagnostic scenarios pass.
- Sixteen profiler ASan/UBSan cases pass with leak detection. Only profiler/test
  TUs and inline boundaries are instrumented locally; the supporting Release
  archive is not. Full sanitizer and MSVC runtime validation remain new CI work.
- Generated output has zero drift; 172 CI helper tests pass with six existing
  skips; documentation integrity checks pass for 79 Markdown files.
- Refreshed OFF symbols, Debug no-op probe and OFF/ON type layouts pass.
- Initial parallel benchmark smoke triggers the existing rate-risk timing
  threshold during simultaneous builds. Quiet serial reproduction passes all
  21 targets. Preserve both; no formal performance verdict is inferred.

The [active measurement contract](../specs/aad-production-measurement.md) retains
RED/GREEN evidence, raw log paths, numerical-oracle corrections and limitations.

## Open acceptance

The unchanged nine-target two-by-ten/4% gate passes for the frozen 5a6382b
default-OFF binaries: 65 comparable cases and the Sobol ratio rule pass; ten
new cases remain informational. Corrected-head source/binary identity must be
reconciled before extending this result. Affected production comparisons,
new CLI fixed-path thread scaling, process resources,
enabled-but-unsampled overhead, explicit diagnostic overhead and current-head
CI must be inspected before P01 closes. Prior 6161e5a CI success covers that
published source only. No merge is requested or performed.
