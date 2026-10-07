# Selected portfolio extraction performance acceptance

Verdict: **no regression** under the paired acceptance rule. All 37 default
and 50 selected/empty/width cases pass; six overlaps are measured once, giving
81 unique cases. Legacy executable identity preserves 162 accepted comparisons.
This is local performance acceptance; publication still requires exact-head CI,
Codacy, review audits and a guarded merge.

## Sources and environment

Baseline: merged P02 #488, `b7e69342d618b539ffc5d3964ed0d6ac2fd15934`,
whose tree equals accepted `4a3229996fcafddd58fafc84f751d2e24bd1e166`.
Candidate: `e425e52dbcb7a84f71dd192154868146c9511f12` plus the compact-source
fallback. The frozen patch, source/archive hashes and link commands are in
`aad-selected-extraction-window-link-proof-10/provenance.json`. Publication
must match these production hashes independently of subsequent documentation.

| Production file                            | SHA-256                                                          |
|--------------------------------------------|------------------------------------------------------------------|
| dal-cpp/dal/script/portfoliobatch.hpp      | a80c11b6a2abf56dcc70083f9fbc49eb86b919d1587bacea6f21c5cb7623a66b |
| dal-cpp/dal/script/portfoliobatch.cpp      | 37f75b249196a935b904843421a2f715c72d2b694b90a3f49cc7bb9c6dc2322b |
| dal-cpp/dal/script/portfolioadmission.cpp  | f5e6817bf32a6ff66d47d9235e4ebd070b34845a56b6be7b277f43fd277d30a0 |
| dal-public/src/portfolioreplayinternal.cpp | 60d2832ee66e70a09cd458fabc0b0332b90fc785f5febfe26d0540c95fd272c0 |

Evidence root: `/tmp/dal-aad-evidence`, persistently backed by
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.
Candidate build: `aad-portfolio-dev-build-01`. Separate merged-baseline archives
and source/object reconstruction proof: `aad-selected-extraction-resource-baseline-03`.
Changed translation units/headers are restored to the exact baseline; unchanged
objects are reused only with the recorded identity proof.

Release C++17, GCC 15.2, native AAD, Eigen ON, native architecture OFF,
profiling/lifetime diagnostics OFF. Production flags:
`-O3 -DNDEBUG -std=c++17 -fPIC -ffp-contract=fast -O3`; benchmarks enabled.
Both executables link one identical helper object,
`aad-selected-extraction-entry-cost-05.o`, to separate production archives.
CPU: Intel i9-13900HX, 32 logical processors, WSL Microsoft hypervisor;
affinity 0/2/4/6, one/four DAL workers by case.
No local builds or CPU tests run during measurement. The host is noisy;
the predefined calibration and retained initial failures below limit interpretation.

## Sampling and calibration

Two rounds of 30 alternating process pairs, minimum per side per round;
failure means **above 4% in both rounds**. The threshold remains unchanged.
Each process warms one complete public request, then separately times 256 complete
requests for compact workloads with at most 1,024 paths, or one for wide/larger
workloads. The process observation is average time per complete request;
minimum still reduces observations across processes. Every constituent duration
is retained. Repeated shapes, values/risks, groups, work and actual widths are
checked against the warm result outside timed regions.

Every request still prepares, admits capacity, initializes history, replays and
constructs an owning result. Only the immutable sealed input is shared; no
cross-request work cache is introduced. Sealing is recorded separately outside
request timing, matching the immutable-input contract.

The original one-request thirty-pair current-binary A/A control fails at
6.30%/9.05%; the 32-request baseline control has a 5.07% round.
These retained controls do not authorize A/B acceptance. The 256-request window
is fixed before sampling, and both baseline/head A/A must have both absolute
round deltas within 4%. Their accepted controls are:

| Control | Round 1 | Round 2 |
|---------|---------|---------|
| base    | 0.28%   | 1.29%   |
| head    | 0.95%   | 0.21%   |

Every pair verifies finite values/risks within absolute `1e-10`, exact shapes,
paths, groups, scenarios, evaluator/reversal counts and actual widths.
Production requests enforce finite scratch/recording quotas.
Raw process outputs, all request durations and parsed samples:
`aad-selected-extraction-window-pairs-30-10`.
No unchanged candidate is rerun until it happens to pass.

## Complete paired matrix

Case names give family/shape/evaluator/workers/mode/result/requested width/paths/
selection. Times are minimum process per-request milliseconds for each round.
A single round above 4% remains visible and does not fail the two-round rule.

| Case: family/shape/evaluator/workers/mode/kind/width/paths/selection | Base 1 ms | Head 1 ms | Delta 1 | Base 2 ms | Head 2 ms | Delta 2 | Verdict |
|----------------------------------------------------------------------|-----------|-----------|---------|-----------|-----------|---------|---------|
| BS/compatible/compiled/4/native/weighted/1/257/all                   | 0.616     | 0.615     | -0.17%  | 0.614     | 0.616     | 0.30%   | Pass    |
| BS/compatible/compiled/4/native/weighted/1/131072/sparse             | 25.450    | 25.876    | 1.68%   | 25.525    | 26.077    | 2.16%   | Pass    |
| BS/compatible/compiled/4/native/jacobian/8/131072/sparse             | 42.208    | 42.599    | 0.93%   | 42.538    | 42.783    | 0.57%   | Pass    |
| BS/one/compiled/1/native/weighted/1/257/all                          | 0.094     | 0.094     | 0.27%   | 0.094     | 0.094     | 0.63%   | Pass    |
| BS/one/compiled/4/native/weighted/1/257/all                          | 0.208     | 0.209     | 0.44%   | 0.210     | 0.205     | -2.50%  | Pass    |
| BS/compatible/compiled/1/native/weighted/1/257/all                   | 0.476     | 0.473     | -0.69%  | 0.474     | 0.471     | -0.78%  | Pass    |
| BS/mixed/compiled/1/native/weighted/1/257/all                        | 0.508     | 0.503     | -1.02%  | 0.506     | 0.501     | -0.91%  | Pass    |
| BS/mixed/compiled/4/native/weighted/1/257/all                        | 0.782     | 0.765     | -2.13%  | 0.783     | 0.777     | -0.67%  | Pass    |
| BS/owners/compiled/1/native/weighted/1/257/all                       | 0.711     | 0.705     | -0.89%  | 0.709     | 0.703     | -0.80%  | Pass    |
| BS/owners/compiled/4/native/weighted/1/257/all                       | 1.429     | 1.415     | -0.98%  | 1.434     | 1.431     | -0.22%  | Pass    |
| BS/history/compiled/1/native/weighted/1/257/all                      | 0.664     | 0.656     | -1.29%  | 0.655     | 0.650     | -0.70%  | Pass    |
| BS/history/compiled/4/native/weighted/1/257/all                      | 0.788     | 0.776     | -1.57%  | 0.779     | 0.772     | -0.92%  | Pass    |
| BS/compatible/compiled/4/native/jacobian/1/257/all                   | 1.372     | 1.376     | 0.28%   | 1.390     | 1.365     | -1.80%  | Pass    |
| BS/compatible/compiled/4/native/jacobian/3/257/all                   | 0.939     | 0.946     | 0.76%   | 0.935     | 0.933     | -0.25%  | Pass    |
| BS/compatible/compiled/4/native/jacobian/8/257/all                   | 0.674     | 0.672     | -0.33%  | 0.674     | 0.670     | -0.49%  | Pass    |
| BS/compatible/compiled/4/passive/weighted/3/257/all                  | 0.523     | 0.521     | -0.48%  | 0.525     | 0.510     | -2.95%  | Pass    |
| BS/compatible/compiled/4/passive/jacobian/3/257/all                  | 0.518     | 0.512     | -1.18%  | 0.523     | 0.512     | -2.22%  | Pass    |
| BS/one/compiled/1/native/weighted/1/131072/all                       | 20.047    | 20.157    | 0.55%   | 20.052    | 20.176    | 0.62%   | Pass    |
| BS/one/compiled/4/native/weighted/1/131072/all                       | 5.475     | 5.550     | 1.36%   | 5.401     | 5.483     | 1.53%   | Pass    |
| BS/compatible/compiled/1/native/weighted/1/131072/all                | 96.049    | 96.306    | 0.27%   | 95.890    | 96.271    | 0.40%   | Pass    |
| BS/compatible/compiled/4/native/weighted/1/131072/all                | 27.533    | 27.931    | 1.45%   | 28.325    | 28.190    | -0.48%  | Pass    |
| BS/mixed/compiled/1/native/weighted/1/131072/all                     | 107.256   | 106.396   | -0.80%  | 106.340   | 106.295   | -0.04%  | Pass    |
| BS/mixed/compiled/4/native/weighted/1/131072/all                     | 29.205    | 28.986    | -0.75%  | 28.964    | 28.992    | 0.10%   | Pass    |
| BS/owners/compiled/1/native/weighted/1/131072/all                    | 161.048   | 161.753   | 0.44%   | 161.088   | 160.762   | -0.20%  | Pass    |
| BS/owners/compiled/4/native/weighted/1/131072/all                    | 48.312    | 49.469    | 2.40%   | 48.173    | 48.096    | -0.16%  | Pass    |
| BS/history/compiled/1/native/weighted/1/131072/all                   | 113.617   | 114.242   | 0.55%   | 114.222   | 113.920   | -0.26%  | Pass    |
| BS/history/compiled/4/native/weighted/1/131072/all                   | 30.560    | 30.624    | 0.21%   | 30.742    | 30.736    | -0.02%  | Pass    |
| BS/compatible/compiled/4/native/jacobian/1/131072/all                | 44.905    | 46.899    | 4.44%   | 45.437    | 45.130    | -0.68%  | Pass    |
| BS/compatible/compiled/4/native/jacobian/3/131072/all                | 46.088    | 45.936    | -0.33%  | 46.427    | 46.276    | -0.33%  | Pass    |
| BS/compatible/compiled/4/native/jacobian/8/131072/all                | 42.611    | 42.739    | 0.30%   | 42.947    | 43.035    | 0.20%   | Pass    |
| BS/compatible/compiled/4/passive/weighted/3/131072/all               | 8.377     | 8.464     | 1.05%   | 8.351     | 8.177     | -2.08%  | Pass    |
| BS/compatible/compiled/4/passive/jacobian/3/131072/all               | 8.677     | 8.497     | -2.07%  | 8.505     | 8.707     | 2.38%   | Pass    |
| CorrelatedBS/compatible/compiled/1/native/weighted/1/16384/all       | 12.437    | 12.550    | 0.91%   | 12.654    | 12.598    | -0.44%  | Pass    |
| Hybrid/compatible/compiled/1/native/weighted/1/16384/all             | 13.245    | 13.214    | -0.24%  | 13.066    | 13.068    | 0.01%   | Pass    |
| GSR/compatible/compiled/1/native/weighted/1/16384/all                | 14.523    | 14.533    | 0.07%   | 14.521    | 14.579    | 0.40%   | Pass    |
| MultiGSR/compatible/compiled/1/native/weighted/1/16384/all           | 14.453    | 14.554    | 0.70%   | 14.486    | 14.527    | 0.29%   | Pass    |
| GSRSLV/compatible/compiled/1/native/weighted/1/16384/all             | 39.136    | 39.206    | 0.18%   | 39.121    | 39.112    | -0.02%  | Pass    |
| BS/compatible/tree/1/native/weighted/1/32785/all                     | 26.684    | 26.547    | -0.52%  | 26.771    | 26.606    | -0.62%  | Pass    |
| BS/compatible/tree/4/native/weighted/1/32785/all                     | 7.415     | 7.508     | 1.26%   | 7.399     | 7.457     | 0.78%   | Pass    |
| BS/compatible/tree/1/native/weighted/1/257/sparse                    | 0.495     | 0.491     | -0.76%  | 0.495     | 0.489     | -1.27%  | Pass    |
| BS/compatible/compiled/1/native/weighted/1/257/sparse                | 0.475     | 0.471     | -0.84%  | 0.475     | 0.469     | -1.15%  | Pass    |
| BS/compatible/tree/4/native/weighted/1/257/sparse                    | 0.632     | 0.636     | 0.70%   | 0.631     | 0.625     | -0.95%  | Pass    |
| BS/compatible/compiled/4/native/weighted/1/257/sparse                | 0.626     | 0.637     | 1.70%   | 0.627     | 0.622     | -0.77%  | Pass    |
| BS/compatible/tree/1/native/weighted/1/8193/sparse                   | 6.902     | 6.913     | 0.17%   | 6.899     | 6.902     | 0.05%   | Pass    |
| BS/compatible/compiled/1/native/weighted/1/8193/sparse               | 6.228     | 6.222     | -0.09%  | 6.210     | 6.232     | 0.35%   | Pass    |
| BS/compatible/tree/4/native/weighted/1/8193/sparse                   | 2.242     | 2.271     | 1.33%   | 2.264     | 2.301     | 1.62%   | Pass    |
| BS/compatible/compiled/4/native/weighted/1/8193/sparse               | 2.215     | 2.255     | 1.82%   | 2.176     | 2.256     | 3.68%   | Pass    |
| BS/compatible/tree/1/native/weighted/1/32785/sparse                  | 26.732    | 26.708    | -0.09%  | 26.684    | 26.639    | -0.17%  | Pass    |
| BS/compatible/compiled/1/native/weighted/1/32785/sparse              | 24.118    | 24.178    | 0.25%   | 24.155    | 24.113    | -0.17%  | Pass    |
| BS/compatible/tree/4/native/weighted/1/32785/sparse                  | 7.562     | 7.501     | -0.81%  | 7.637     | 7.529     | -1.41%  | Pass    |
| BS/compatible/compiled/4/native/weighted/1/32785/sparse              | 7.167     | 7.311     | 2.02%   | 6.979     | 7.177     | 2.84%   | Pass    |
| BS/compatible/tree/1/native/weighted/1/131072/sparse                 | 105.794   | 105.578   | -0.20%  | 105.595   | 105.339   | -0.24%  | Pass    |
| BS/compatible/compiled/1/native/weighted/1/131072/sparse             | 95.333    | 95.456    | 0.13%   | 95.414    | 95.663    | 0.26%   | Pass    |
| BS/compatible/tree/4/native/weighted/1/131072/sparse                 | 28.432    | 28.770    | 1.19%   | 28.545    | 28.588    | 0.15%   | Pass    |
| BS/one/compiled/1/native/weighted/1/257/sparse                       | 0.094     | 0.093     | -0.66%  | 0.093     | 0.094     | 0.27%   | Pass    |
| BS/compatible/compiled/1/native/weighted/1/257/empty                 | 0.480     | 0.470     | -2.04%  | 0.476     | 0.467     | -1.84%  | Pass    |
| BS/compatible/compiled/4/native/weighted/1/257/empty                 | 0.626     | 0.614     | -1.78%  | 0.625     | 0.607     | -2.82%  | Pass    |
| BS/compatible/compiled/4/native/jacobian/1/257/sparse                | 1.424     | 1.405     | -1.39%  | 1.417     | 1.397     | -1.39%  | Pass    |
| BS/compatible/compiled/4/native/jacobian/2/257/sparse                | 1.045     | 1.037     | -0.82%  | 1.051     | 1.036     | -1.43%  | Pass    |
| BS/compatible/compiled/4/native/jacobian/3/257/sparse                | 0.929     | 0.944     | 1.61%   | 0.937     | 0.933     | -0.42%  | Pass    |
| BS/compatible/compiled/4/native/jacobian/8/257/sparse                | 0.687     | 0.680     | -0.97%  | 0.688     | 0.680     | -1.08%  | Pass    |
| BS/wide/compiled/1/native/jacobian/1/257/sparse                      | 55.751    | 55.181    | -1.02%  | 55.422    | 54.434    | -1.78%  | Pass    |
| BS/wide/compiled/1/native/jacobian/2/257/sparse                      | 55.939    | 54.545    | -2.49%  | 55.341    | 54.298    | -1.88%  | Pass    |
| BS/wide/compiled/1/native/jacobian/3/257/sparse                      | 55.059    | 54.617    | -0.80%  | 55.481    | 55.062    | -0.75%  | Pass    |
| BS/wide/compiled/1/native/jacobian/8/257/sparse                      | 55.590    | 54.444    | -2.06%  | 56.830    | 56.201    | -1.11%  | Pass    |
| BS/compatible/compiled/4/native/jacobian/2/257/all                   | 1.035     | 1.024     | -1.03%  | 1.032     | 1.025     | -0.70%  | Pass    |
| BS/one/compiled/1/native/weighted/1/131072/sparse                    | 19.978    | 20.170    | 0.96%   | 19.989    | 20.124    | 0.68%   | Pass    |
| BS/compatible/compiled/1/native/weighted/1/131072/empty              | 95.914    | 95.491    | -0.44%  | 95.492    | 95.690    | 0.21%   | Pass    |
| BS/compatible/compiled/4/native/weighted/1/131072/empty              | 27.728    | 27.516    | -0.76%  | 26.697    | 27.445    | 2.80%   | Pass    |
| BS/compatible/compiled/4/native/jacobian/1/131072/sparse             | 45.407    | 47.076    | 3.68%   | 45.053    | 47.777    | 6.05%   | Pass    |
| BS/compatible/compiled/4/native/jacobian/2/131072/sparse             | 48.673    | 48.243    | -0.88%  | 47.279    | 47.401    | 0.26%   | Pass    |
| BS/compatible/compiled/4/native/jacobian/3/131072/sparse             | 47.141    | 46.349    | -1.68%  | 46.260    | 47.664    | 3.04%   | Pass    |
| BS/wide/compiled/1/native/jacobian/1/131072/sparse                   | 235.269   | 231.978   | -1.40%  | 235.181   | 232.630   | -1.08%  | Pass    |
| BS/wide/compiled/1/native/jacobian/2/131072/sparse                   | 250.634   | 250.466   | -0.07%  | 249.709   | 249.893   | 0.07%   | Pass    |
| BS/wide/compiled/1/native/jacobian/3/131072/sparse                   | 246.887   | 245.727   | -0.47%  | 246.715   | 246.938   | 0.09%   | Pass    |
| BS/wide/compiled/1/native/jacobian/8/131072/sparse                   | 242.322   | 238.745   | -1.48%  | 241.028   | 239.528   | -0.62%  | Pass    |
| BS/compatible/compiled/4/native/jacobian/2/131072/all                | 46.350    | 47.112    | 1.64%   | 46.032    | 45.974    | -0.13%  | Pass    |
| BS/wide/compiled/1/native/weighted/1/8193/sparse                     | 62.750    | 62.736    | -0.02%  | 62.978    | 62.350    | -1.00%  | Pass    |
| BS/wide/compiled/1/native/weighted/1/32785/sparse                    | 84.978    | 83.739    | -1.46%  | 86.239    | 84.448    | -2.08%  | Pass    |
| BS/wide/compiled/1/native/weighted/1/131072/sparse                   | 173.058   | 173.097   | 0.02%   | 173.760   | 172.989   | -0.44%  | Pass    |
| BS/wide/compiled/1/native/weighted/1/131072/all                      | 171.147   | 170.795   | -0.21%  | 171.874   | 171.928   | 0.03%   | Pass    |

## Width decision and resources

All rows below request three reordered input columns and eight output rows.
Compatible uses 12 complete inputs and four workers; wide uses 8,204 inputs and
one worker, including 1,024 live historical constants per trade.
Times are the two candidate round minima. Work is per complete request, including
the actual original worker/batch plan. Capacity columns are maxima of reported
last-request peaks across 60 candidate process outputs. They measure tracked
capacity, not RSS or a maximum over every repeated request.

| Shape      | Paths  | Width | Head ms 1/2     | Scenarios/suffix | Prefix | Scratch bytes | Tape bytes |
|------------|--------|-------|-----------------|------------------|--------|---------------|------------|
| compatible | 257    | 1     | 1.405/1.397     | 2056             | 32     | 33432         | 7864320    |
| compatible | 257    | 2     | 1.037/1.036     | 1028             | 16     | 44792         | 7864320    |
| compatible | 257    | 3     | 0.944/0.933     | 771              | 12     | 56136         | 7864320    |
| compatible | 257    | 8     | 0.680/0.680     | 257              | 4      | 113800        | 7864320    |
| compatible | 131072 | 1     | 47.076/47.777   | 1048576          | 128    | 36376         | 7864320    |
| compatible | 131072 | 2     | 48.243/47.401   | 524288           | 64     | 49120         | 7864320    |
| compatible | 131072 | 3     | 46.349/47.664   | 393216           | 48     | 62512         | 7864320    |
| compatible | 131072 | 8     | 42.599/42.783   | 131072           | 16     | 128392        | 7864320    |
| wide       | 257    | 1     | 55.181/54.434   | 2056             | 8      | 6369184       | 1966080    |
| wide       | 257    | 2     | 54.545/54.298   | 1028             | 4      | 6369184       | 1966080    |
| wide       | 257    | 3     | 54.617/55.062   | 771              | 3      | 6369184       | 1966080    |
| wide       | 257    | 8     | 54.444/56.201   | 257              | 1      | 6369184       | 3670016    |
| wide       | 131072 | 1     | 231.978/232.630 | 1048576          | 128    | 6369184       | 1966080    |
| wide       | 131072 | 2     | 250.466/249.893 | 524288           | 64     | 6369184       | 1966080    |
| wide       | 131072 | 3     | 245.727/246.938 | 393216           | 48     | 6369184       | 1966080    |
| wide       | 131072 | 8     | 238.745/239.528 | 131072           | 16     | 6369184       | 3670016    |

Keep default maximum width one, the explicit caller maximum and capacity-only
narrowing. Larger widths reduce scenario/reverse passes, but total request time
also includes lane work, metadata and history. Wide-input cases do not justify
a universal automatic width increase. This increment adds no public policy/API.

Packing reduces retained numeric slots and harvesting reads; it retains full
model/evaluator registration and tape lanes. The scalar-payload proxy preserves
compact nonempty full extraction. Metadata can dominate a measured peak even
when numeric slots shrink. Independent resource contracts use 2,052 inputs and
129 original batches: weighted selection fits below full-extraction capacity;
selected Jacobians keep width two, padding and history aliases.
The compact contract derives its quota only from known-fit full requests:
published production is RED (43,692 required / 43,684 allowed), fallback GREEN.
All 41 core and 47 public affected cases pass.

## Legacy executable identity and coverage

Thirteen fresh links from candidate archives are byte-identical to accepted P02
executables. The window link proof checks unchanged production/archive identity
against `aad-selected-extraction-subset-only-link-proof-08/provenance.json`.
This reuses 162 accepted legacy/scalar/weighted/MC comparisons.
The closed nine-target gate is unchanged; `script_mc_perf` and
`curve_calibration_perf` remain informational.

| Executable             | Accepted/fresh SHA-256                                           | Proof     |
|------------------------|------------------------------------------------------------------|-----------|
| scalar entry           | b7d25e1a70e36b16d52878ba26673b2397bd66a7b4ab45466ee9f2018cc9ab1a | Identical |
| weighted entry         | f9237fdd8be443426fffda8ebd8b0e9e00fab4bcc5bfa3a0554d41addd86f9e5 | Identical |
| tape_perf              | 71a236e5a25779d832fc51ada267e51bc9b82b99a81a04842050bd242605fc48 | Identical |
| jacobian_perf          | 9c2cb2a0c142df755aa4dc5082cb7f5ca91356a0a65a8df1a8ce8b97a9359ce9 | Identical |
| pde_perf               | dc6a349a9261d8634b6a072d959afda5ddea3992988812101bed59d4a8835b9d | Identical |
| rng_perf               | 6ef4ed04dac439860a61b90e642209a08f39641a985e473eb7b1481b24db554d | Identical |
| interp_perf            | b4515d8bc512b8a9d0450b844927e2ee0e84aeee9a91272ef1005f1eb28280a6 | Identical |
| krylov_perf            | ff531ca400edb2dda44237d9068e78c5441cd92f47de4f0522683d389074bb45 | Identical |
| banded_perf            | eb0fe90252bc8ea680f1bc885377ab4fa2bafe38fe43383432f4bda87331efd5 | Identical |
| cholesky_perf          | 5144d98825d2984bcb4c9bbdf8554c9bf959ae4ed488d8168eb3d3d0d0df81f4 | Identical |
| rate_risk_perf         | d9a5720215490da4cfe87c5c4152a405357ac4962b63017bfd2739475dfadd27 | Identical |
| script_mc_perf         | 5a95c0d3e7d1f226d495f400872513ba13acaf4eb10ce5096f1762c53ed8e11f | Identical |
| curve_calibration_perf | 0b68a126877f45964f706516e0e29b270c389a13dd010ab3a78934fb682700c6 | Identical |

The nine-target module map covers tape, sweeps, calibration, PDE/RNG,
interpolation and linear algebra. Private portfolio selection, admission and
scatter need complete-request coverage, supplied by this matrix and independent
unit oracles. The existing `script_mc_perf` is the suitable future home for
repeatable portfolio workloads; this increment does not add another gated target.

## Retained failed evidence and limits

The [ownership design](../designs/aad-selected-extraction.md) records failures
and disposition. Outlining, vector reservation and a shared-path functor failed
timing and were rolled back. The unconditional compact classifier passes 50
selected cases but fails two small default cases. The subset-only classifier
restores the complete/passive outer branch; its short single-request result
remains inconclusive under failing same-binary controls.

Retained runs include `aad-selected-extraction-initial-pairs-01`,
`aad-selected-extraction-selected-pairs-02`,
`aad-selected-extraction-outlined-selected-pairs-03`,
`aad-selected-extraction-reserved-selected-pairs-04`,
`aad-selected-extraction-reserved-selected-pairs-30-05`,
`aad-selected-extraction-path-loop-selected-pairs-30-06`,
`aad-selected-extraction-compact-default-pairs-30-06`,
`aad-selected-extraction-compact-selected-pairs-30-07`,
`aad-selected-extraction-subset-only-default-pairs-30-07`,
`aad-selected-extraction-small-aa-30-03` and
`aad-selected-extraction-window-pairs-30-09`. They are not overwritten.
Final acceptance follows the amended observation-window contract and passing
predefined controls; it does not claim the earlier one-request runs passed.

Histograms in `aad-selected-extraction-profile-hotpath-01` are diagnostic.
This verdict covers the measured configuration and workloads, not a universal
speedup or minimum possible scratch capacity for every selection.
Exact-head platform acceptance remains a separate publication gate.
