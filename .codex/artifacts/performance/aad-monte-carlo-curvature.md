# Scoped cost selection: Monte Carlo curvature

Status: local scoped costs accepted. Selection was frozen before measurement;
no unrelated benchmark matrix was run.

| Case                       | Compared requests                                                   | Purpose                                                                    |
|----------------------------|---------------------------------------------------------------------|----------------------------------------------------------------------------|
| Spot Gamma                 | 65 common one-step paths, one spot direction, one worker            | Small financial-entry overhead against equivalent manual segmented secants |
| Mixed HVP                  | 65 common 16-step paths, three model/script directions, two workers | Streamed financial request/resource behavior without a large matrix        |
| Existing generic curvature | Accepted smooth polynomial driver and identical request             | Regression control for moving its shared numerical loop                    |

Both financial implementations must use the same preparation, RNG settings,
path interval, point, steps, mode guard and output consumption. Use accepted
master source and immutable archive-member provenance. Record source/object/
binary hashes, environment, raw interleaved observations and two rounds.
Existing caller threshold is +4% in both rounds under the established protocol;
financial-entry overhead is descriptive. Expand only a failed/noisy affected
case, never the unrelated full benchmark collection. Timing remains read-only.


## Evidence and observations

Final financial source: `e114b6d8917f4ffcfae1dfc3be12ec483ba70c01`;
original generic control source: `726bf2f8d9c4fdb008b8ce4d4d26b5a1895515c4`;
accepted master baseline: `6d5fde3e06d09d51924a74dc7c19f8f8f84c0c85`.
Separate detached sources/builds use GCC 15.2.0, C++17, O3/NDEBUG/PIC,
fast contraction, portable architecture and native AAD. Benchmarks are explicitly
ON in both isolated CMake caches. `DAL_NUM_THREADS=4` is common; the financial
cases explicitly select one/two workers respectively.

The manual baseline overlay adds only the new benchmark/API declarations and
const prepared-ownership accessor. It composes accepted first-order segmented
MC gradients with the same kernel/request snapshots, stack mode guard, FMA
bumps, output ownership and resource capture. Its ordinary central quotient
has moderate representable operands. The production entry additionally performs
complete bump-domain admission, contextual error handling and robust scaled
division. Baseline source/body changes and all source/object/archive/executable
hashes are retained in `cost-null-environment.json` and the first study snapshot.

The kernel accessor rebuild changes only two exception-path line-number
immediates (144 to 145 and 153 to 154); all other MC object bytes are identical
after source-path canonicalization. The current archive uses that rebuilt
object, replaces the generic driver and adds the new entry. It retains 179
original member occurrences exactly (182 members total). Accepted unrelated
caller costs remain applicable; no unrelated timing rows are repeated.

| Case             | Round 1 base/head, ns | Round 1 change | Round 2 base/head, ns | Round 2 change | Interpretation                             |
|------------------|-----------------------|----------------|-----------------------|----------------|--------------------------------------------|
| Spot Gamma       | 797705 / 812415       | +1.84%         | 822097 / 806649       | -1.88%         | Informational entry overhead               |
| Mixed HVP        | 3387924 / 3855169     | +13.79%        | 6565221 / 5214599     | -20.57%        | Relative overhead inconclusive under noise |
| Existing generic | 1308 / 1324           | +1.22%         | 1623 / 1651           | +1.73%         | Both +4% regression rounds pass            |

The original three-case study collects 120 observations in 2.110 seconds. Review
repair adds a public-constructor invariant, so only the two affected financial
cases repeat: 80 observations in 3.949 seconds. No generic-control timing repeats;
its driver and benchmark object bytes are unchanged. Its original head executable
is retained under `first-accepted-costs/tape_perf`, alongside accepted metadata,
and the accepted native baseline executable remains immutable. The table uses
current financial observations and the reused original generic row.

Both studies use two rounds of ten interleaved process samples per side. Each
process reports the minimum of three warm measurements; the gate uses each
round's best-of-ten process minimum. Total sampling is 200 observations in
6.060 seconds. Original and repair raw outputs remain in `cost-raw/` and
`cost-null-raw/`, with separate environment, sample and result JSON files.
Prices, Gamma, gradients and all products are consumed and checked against
manual secants; independent mathematical validation is in the correctness suite.

This is a shared, visibly busy host: other workspaces compile and benchmark
concurrently. The existing generic caller meets its gate. Financial mixed-case relative
overhead is inconclusive: the sign changes across rounds and baseline minima
differ by almost a factor of two. Those differences are not treated as speedups
or a stable overhead estimate. No total-memory, strong-scaling or broad
application-speed claim is made.

Overall verdict: no regression in the selected affected existing caller;
financial-entry costs are informational, with mixed-case overhead inconclusive
under shared-host noise. Required remote CI is separate.
