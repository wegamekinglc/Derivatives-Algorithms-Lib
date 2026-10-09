# Complete rate Jacobian scoped cost acceptance

Active execution increment against accepted #509, merge `282adff0`. This report
controls local acceptance; exact-head CI and review gates remain open.

## Scope and reproducibility

Only `ratecashflowpricing.cpp.o` changes among 179 library objects. The other
178 retain accepted bytes; complete MC/PDE/solve and unrelated portfolio evidence
is reused. Existing function bodies in the changed pricing object remain exact.
Repeat only its complete passive and joint-native requests.

Two portfolios return equivalent complete 3-by-5 matrices. One has disjoint
supports admitting two colors; three repeated IRS rows need three colors.
Compare explicit dense execution, cold capture/plan/execution and retained-plan
execution at a fresh numerical point. The reused plan is captured at independent
reference point 2 and executes point 3; the zero IRS contract rate stays fixed.
Selected base and dependent curves are both rebuilt with current values.

Every request checks all PVs and fifteen derivatives against the independent
analytic reference frozen before provider implementation. Timing includes current
validation, preparation, identity verification, input binding, recording, every
reverse sweep, recovery, result storage, output checks and release. Cold requests
also include capture and plan construction. Market/trade fixtures and the reused
initial plan are outside timing. Scalar width one is the affected cost setting;
vector correctness is covered separately rather than expanding the timing matrix.

GCC Release uses C++17, `-O3 -DNDEBUG -ffp-contract=fast`, diagnostics OFF, CPU 0,
`DAL_NUM_THREADS=4` and zero pricing workers. Two interleaved rounds each take ten
samples, with eight warmups and 128 complete requests per observation. Acceptance
uses each round's best-of-ten and the calibrated sustained 4% regression gate.

## Results

The final 200 observations finish in 0.5103 seconds. Existing passive head/base ratios
are 1.0117 and 1.0084; joint-native ratios are 0.9821 and 1.0085. Neither old path
has a sustained regression. New request costs are informational:

| Complete request                 | Round 1 µs | Round 2 µs | Directions |
|----------------------------------|------------|------------|------------|
| Compressible dense               | 5.7760     | 5.7820     | 3          |
| Compressible compressed cold     | 29.5030    | 29.5927    | 2          |
| Compressible compressed reused   | 17.2313    | 17.0565    | 2          |
| Uncompressible dense             | 7.2211     | 7.2126     | 3          |
| Uncompressible compressed cold   | 40.6204    | 40.7429    | 3          |
| Uncompressible compressed reused | 23.0422    | 22.7993    | 3          |

For these small requests, complete structural validation costs more than the
saved reverse direction. This execution increment keeps both operations explicit.
The following measured selector must account for cold/reuse costs and prefer
dense when compression offers no complete-request advantage. This fixture proves
neither a universal sparse speedup nor a crossover for larger portfolios.

## Frozen evidence

Session evidence `rate-parameter-performance-repair/` retains `selection.json`, raw
process output, `results.json`, archive identity and binary/source/reference
hashes. The accepted base archive SHA256 is
`2f6a787fb180b3a2de554184a9ce7dc337f4310945badd471fe50dddf70d9d9c`;
the execution archive is
`300f22e9af384b28feafad5994bd47aba2a22ad98a0d3b96818b04ca5c677597`.
Production sources, fixture and binaries retain their final measured hashes.
The initial submission's 0.5058-second study remains in
`rate-parameter-performance/`. Currency metadata and preflight repairs change
the production object, so only these same eight affected rows are repeated.
The accepted baseline binary is reused byte for byte. The harness permission
failure before its first timed observation is retained separately.
