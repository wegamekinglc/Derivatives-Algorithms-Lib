# Native implicit-root design critique

Local design/correctness acceptance passes with no blocking findings. Numeric
#501 is merged and tree-verified; native publication remains pending.

| Risk                                              | Implemented control and evidence                                                                                       |
| ------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------- |
| Callback redirects captured input                 | Own Number bindings before snapshot/evaluation; source mutation leaves original risk 1/4 and replacement risk zero     |
| Residual is confused with zero-RHS convergence    | Separate owning equation residual/policy/physical-condition diagnostics                                                |
| Report rows equal parameter count                 | One transpose RHS per channel; scalar and width 1/4/8 require 1-by-width errors                                        |
| Input aliases overwrite contributions             | Existing checked accumulation sums declared columns; alias/direct risk is 4; finite-column alias overflow rejects      |
| Stationarity uses approximate Jacobian            | Complete nonzero-residual stationarity risks 2/7 and 1/7; independent of Gauss–Newton's 2/5 and 1/5                    |
| New collector loses older events                  | Existing identities/TLS collector; mixed windows, restore and serial root execution pass                               |
| Failure exposes stale adjoints or partial reports | Ordinary/collected transpose failure, underflow and range failure invalidate recording and preserve historical reports |
| Method data outlives borrowed sources             | One owning numeric cache, detached diagnostics/reports, destroyed callback and source-mutation checks                  |
| Root duplicates event accounting                  | Existing event RAII with deduced output/diagnostics types and three narrow internal collector bridges                  |
| Resource peaks omit overlap                       | Actual tape/caller census, exact/one-byte-short/refund tests; caller scratch overlaps report/output storage            |
| Changed helper prompts full performance matrix    | Object multiplicities select only 22 old caller rows; paired gate passes; unchanged numeric bytes reuse evidence       |

All 22 runtime cases pass locally, including independent elementary/Cramer,
negative/approximate branches, complete stationarity, k=0, serial roots,
actual inclusive errors, stale slots, numerical/alias failures and exact
resource boundaries. All 115 affected old solve cases pass. Twenty strict
OFF/combined source/direct-header checks pass; maximum function CCN is six.
The installed native consumer passes owning observations and composition.

Initial resource fixture failures are retained: caller peak must include
reverse scratch, while tape cleanup reservation is headroom excluded from
measured peak. Tests were corrected to established implementation semantics;
production budgets and acceptance thresholds were unchanged.

The four optional native/numeric cost rows retain independent primal/risk/
report/resource checks. Full native requests include tape cleanup and are
about 37–39 microseconds; cached reverse is about 262–266 ns scalar and
989–993 ns width4. The numeric request does less work. Initial print precision
and per-side resource parser failures are retained; only the four optional
rows were resampled for the precision correction, and final reduction reused
all raw legacy measurements. No universal speedup is claimed.

See the active [review](../reviews/aad-native-implicit-root.md) and
[performance report](../performance/aad-native-implicit-root.md). Remaining
publication gates are actual 22-case execution in fourteen platform profiles,
exact-head CI/Codacy/review, repeated paginated audits and guarded merge.
