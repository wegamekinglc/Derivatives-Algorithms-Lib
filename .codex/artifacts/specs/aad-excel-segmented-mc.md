# Excel segmented native Monte Carlo boundary

Active increment after merged [#537](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/537).
The accepted public `montecarlocurvature.hpp` and native segmented algorithms
control numerical behavior. This increment adds worksheet ownership and admission,
not another path simulator. LSMC follows in a separate PR.

## Requirements

1. Provide immutable named segmented settings, a sealed Black–Scholes script plan,
   an owning mean/gradient result and an owning finite-step curvature result.
   Plans retain copied contracts, valuation/history and full parameter order:
   spot, vol, rate, div, script constants. Reuse existing product/valuation handles.
2. Settings retain RNG, Brownian bridge, absolute first path, optional uint64
   scramble key, normal precision, positive segment steps and independent
   checkpoint/recording caps. Blank caps are unset; zero is a real cap. Numeric
   counts/caps are exact nonnegative size_t integers through `2^53-1`.
   A present scramble key also accepts canonical decimal text through UINT64_MAX;
   queries always return it as text. Reject bool/date/fraction/overflow, unknown
   or duplicate keys and embedded NUL. RNG/domain combinations remain subject to
   native evaluation admission.
3. Mean and curvature factories take a complete finite numeric point through a
   row/column matrix and positive exact path count. Reject every blank gap and
   invalid physical kind; preserve integer normalization in actual Windows calls.
   Do not use ordinary MC's INT_MAX path converter for the native size_t entry.
4. Curvature uses the accepted common bump request and exactly one public native
   evaluation. Preserve common absolute paths, directions, positive actual steps,
   complete raw gradients/products and method `BumpOverSegmentedNativeAAD`.
   Zero directions retain logical `0 x N`, spill empty text and evaluate one base.
5. Expose all native mean and curvature execution fields. Gradient evaluations
   are `1+2M`; path tape/checkpoint/cleanup maxima are per path. Requested settings
   retain their path cap; curvature execution reports the effective minimum with
   the bump cap. Native numeric payload excludes worksheet/preparation ownership.
6. Every passive getter returns detached data without estimator/history work.
   A curvature base getter creates the same owning mean-result type from stored
   data, retaining its plan, point and settings without a second valuation.
   Expose complete observation presence/value records, contract, labels/constants,
   valuation and smoothing. New risk handles explicitly reject serialization.
7. Native nested-recording rejection, resource failures and invalid input must
   leave previous output handles and caller graph/mode/seeds intact. Construct
   complete values before replacing output handles. Valid calls recover after
   failures. Generic active graph callbacks and general higher order stay outside
   this worksheet API.

## Acceptance

- RED first on the missing worksheet interface; GREEN on the independent
  time-zero `SCALE * FIX(EQ)^2` value, gradient and signed mixed products.
  At spot 10 and SCALE 2, direction `(spot=1,SCALE=.5)` gives products 14 and 20.
- Small typed cases cover deterministic future value, 1/33-path batch boundary,
  sealed history without rereads, detached projections, complete settings/key
  boundaries, zero directions, exact native payload and separate caps/recovery,
  invalid handles/kinds/rows/NUL/archives and active caller preservation.
- Strict OFF/combined checks, actual CMake binding boundary and official generated
  wrapper/help drift checks pass. Actual Windows raw tests call every new export
  and verify registration, numeric kinds, gap rejection and logical empty spills.
- Use the [scoped cost selection](../performance/aad-excel-segmented-mc.md).
  Accepted native mathematical evidence is reused only with verified provenance.
- Inspect all applicable exact-head CI, actual runtime logs, Codacy and complete
  review bodies/threads. Audit twice, merge by SHA and verify the merged tree.

No open user decision is required; native/public algorithm behavior is unchanged.
