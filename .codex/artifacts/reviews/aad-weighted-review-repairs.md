# F02 weighted publication review repairs

Status: four Copilot threads at `eb2be051` are addressed locally. New-head
Windows, CI/Codacy and repeated final review gates remain required. Earlier
acceptance is complete at its identified head; these repairs do not complete F02.

## Findings and changes

- `PRRT_kwDOBtahP86pSNpN`: the weighted projector inherits scalar payoff
  identifiers in objective/raw/report nonfinite errors. A shared objective
  projector accepts diagnostic identity; weighted calls supply ordered component
  IDs and full-precision signed weights. Scalar signatures, result layout and
  message bodies remain unchanged, with a null context and no identity-string
  allocation on successful scalar requests.
- `PRRT_kwDOBtahP86pSNpl`: shared Excel `ToDouble` reads the double union member
  for integer inputs. Correct the shared primitive conversion to read `val.w`;
  this also fixes scalar/single-cell-range callers beyond weighted valuation.
  Generated markup is unchanged and must retain zero drift. The actual generated
  weighted valuation export now has an integer/single-cell-range regression
  checking objective value and the retained path count.
- `PRRT_kwDOBtahP86pSNqC` and `PRRT_kwDOBtahP86pSNqc`: reconcile the active
  ledger/spec/API note with completed predecessor acceptance. Record the new
  repair-head gate separately rather than counting accepted Windows/cost/CI
  work as still missing.

## Evidence and limits

Evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- `aad-weighted-review-projection-red-01.log`: the new independent test fails
  on the former `output=payoff` error. The same assertions cover objective-sum,
  raw-risk and reported-risk failures with selected component IDs, weights and
  the requested input identity.
- `aad-weighted-review-green-{off,combined}-01.log`: all 21 weighted preflight,
  projection and existing scalar-result cases pass. Combined lifetime/profiling
  and ASan/UBSan instrument the two affected producers and test units; supporting
  archive/Google Test code is cached. This is focused instrumentation.
- `aad-weighted-review-public-green-01.log`: all 17 weighted/scalar public-entry
  cases pass with the repaired producers and combined sanitizer instrumentation.
- GCC 14 OFF/combined warning checks retain the original flags and pass for both
  affected producers. All four affected core/test units meet complexity eight;
  formatting, patch integrity and 144 Markdown files pass. Excel regeneration
  leaves every generated file unchanged.
- `aad-weighted-excel-integer-body-red-01.log` reproduces `17` becoming
  `8.39912e-323`; `green-01.log` reads `17`. The harness extracts the actual
  `ToDouble` body and supplies portable SDK-type shims. This proves the union
  member correction, not the Windows/XLL ABI; own-head Windows CI must run the
  tracked actual-export regression.
- At `eb2be051`, all 35 CI/Codacy checks pass and all four Windows configurations
  pass twelve weighted typed/raw cases each. Copilot's subsequent review
  succeeds but produces the four threads above. Its sole check annotation is
  a runner-image migration notice, not a source finding.
- All 52 affected scalar cases pass before these repairs. The new cold
  projection entry requires another eight-case scalar comparison. Complete
  MC/GSR/LSM benchmark source is unchanged; relinking against the repaired
  archive produces the identical executable SHA-256
  `4b082f64ad373d5a4e2b26416be41c0df8bf573df5c7e07a05da57051e695689`
  (`aad-weighted-review-mc-equivalence-02.json`). Its original 44-case result is
  therefore retained without repeating an unchanged workload. Weighted cost measurements
  remain informational and must identify the exact measured source.

No tolerance, workload, threshold, old test or diagnostic/profiling policy is
relaxed. Keep the final gate pending until the current repair head is accepted.
