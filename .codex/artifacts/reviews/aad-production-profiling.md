# Native production profiling review

Verdict: Comment Only pending final whole-PR checks. The
[complete current-head confirmation](../performance/aad-production-final-confirmation.md)
passes all 44 MC cases and resolves the prior performance finding retained below.
Exact `8886c083` passes all 35 applicable CI checks, including all four MSVC
configurations and the full sanitizer/TSan matrix. Resource/scaling/overhead
measurement is complete. Default-OFF production performance is locally accepted;
the final PR head needs its own complete checks. The overall goal remains active.

## Findings

The original-baseline full two-by-thirty Monte Carlo confirmation passes 43/44:
GSR 1F bond option fails at +5.0028%/+8.0347%. The complete same-binary control
passes 44/44 and CPU-4 European isolation passes 7/7. A predeclared fresh matched-
prefix comparison has a byte-identical GSR European object but passes 42/44:
vanilla native AAD tree is +6.3848%/+7.7373%, four-node GSR AAD calibration is
+4.2605%/+5.0575%. The failing identities and timing minima remain unstable
on this shared WSL2 host. Verdict is inconclusive, and every failure is retained.
Those earlier results remain inconclusive. The new full current-head confirmation
uses the original workload and two-round/thirty-process policy and passes 44/44;
it does not replace full acceptance with controls or CI.

The complete [measurement report](../performance/aad-production-profiling.md)
contains each formal/supplemental case and the 24-profile, 2,496-process
scaling/resource/overhead sweep. Every price/selected risk passes rel/abs 1e-10;
all LSM samples remain bitwise equal. The curve control and full confirmation
pass all 25 cases. Numeric resources and diagnostic cost are no longer missing.

### Corrected historical CI failures

The b37b49f MSVC profiling and combined configurations fail compilation: the
private windows.h include defines VOID before DAL's generated StackInfoType_
enum is parsed. The local correction undefines that macro after the Windows
include, without altering public headers, algorithms or non-Windows branches.
Both raw failing job logs are retained under the evidence root. Corrected-head
Windows compilation and runtime pass at `8886c083`.

The default MSVC leg independently fails its second CMake configure after 2,311
functional cases pass. Vendored pybind11 retains a cached FOUND flag across
processes, while pybind11_add_module must load again. Independent installed-
Python configuration reproduces the failure. Guarding discovery with command
availability restores two consecutive configure passes, retaining lookup order
and the existing CI reconfiguration check. Vendored Python 3.12 standalone
binding configures twice, builds and passes 792 tests with one absent-monorepo-
native-module skip. Refreshed OFF/ON/combined core builds, ON/combined 20 focused
tests and OFF/ON six CLI contracts each pass. Corrected-head Windows validation
passes at `8886c083`.

The corrected MSVC lifetime job configures successfully, then the new CLI fails
to compile a noncopyable product conditional expression. The local change uses
direct prvalue branch returns in a lambda and keeps copyability/compiler flags
unchanged. Fresh MSVC compilation succeeds at `8886c083`.
OFF/ON CLI builds and six contracts each pass, as do all 38 representative
diagnostic cases. Formatting and docs checks pass; library algorithms are
unchanged by this benchmark-only portability correction.

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
  archive is not. Exact-head CI subsequently passes full sanitizer and MSVC validation.
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
new cases remain informational. Those executable/configuration/gate hashes
match at `8886c083`. Completed resource/scaling and current-head CI do not
establish acceptance for later implementation by themselves. The final complete
confirmation resolves MC performance with 600 processes and 1,240 unchanged
inputs. Every LSM PV/risk remains bitwise equal. The user has authorized whole-PR
fixes and merging after F01/final gates; this profiling review performs no merge.
