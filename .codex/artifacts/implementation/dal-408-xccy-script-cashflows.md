# DAL-408: native XCCY script cashflows and timestamp admission

Product checkpoint: `09ecf4849afc8e9e2475bd05f21de39a7a959fac`.
Base: master `f50db2e93eb16823190ed18b5d73a115f0e9c48f`, containing accepted
DAL-406 / PR #619. The research baseline `0f1feabee193d0976ab7a5336c9e9e29cb2aa28d`
is historical evidence, rather than the implementation base.

This record controls the focused code review and the subsequent DAL-409 adapter.
Shared methodology, CHANGELOG and the artifact index now include the bounded
XCCY capability. The documentation-only integration follows the accepted code
repair; its handoff is recorded below. No public, Python, Excel, compiler,
model, MC, wire or build file changes.

## Design and authority

`NewXccyScriptInstrument` imports a native `CrossCurrencySwap_` once. Resolved
coupon dates/accruals, domestic N, the foreign/domestic notional ratio, signed
K spread, payment directions and principal/reset exchanges become ordinary
source. The returned instrument retains its immutable definition and effective
binding, with no native product stored beside them.

Shared `PrepareCalibrationInstrument` keeps the existing receiver/history guard.
Its XCCY branch lowers every payment, reconstructs the existing
`XccyCashflowPlan_`, and compares the complete regenerated bound AST **and event
dates** with the original payments. Extra, missing, moved or changed principal,
conversion and spread expressions cannot inherit a capability from a name or flag.
Schedules are not rebuilt during candidate evaluation.

`EvaluateXccyScript<double/AAD::Number_>` refreshes the existing native market-view
kernel. Price is B+K*A; BasisSpread is -B/A with the declared annuity sign/floor.
Candidate market values remain explicit caller inputs. This preparation layer
does not implement the later staged solver or freeze a caller's curve objects.
It owns and freezes the fixing snapshot for all evaluations and bumps.

`XccyFxForward` exposes the shared native primitive, including the reverse quote.
EUR/USD remains USD per EUR, with `F=spot*foreignDF/(domesticDF*basisDF)`.
The native reset path and the new primitive share the same arithmetic and
reciprocal order. Extra standalone-provider domain checks do not change legacy
native pricing admission.

Payment conversion is a model-sourced `SPOT()` in the complete two-currency
template. The matched currency legs and domestic header bind its FX direction.
It remains active model FX at the payment date, including valuation-date
payments. Explicit reset `FIX(FX[EUR/USD],date)` has the timestamp policy below.
Consequently an optional same-time reset fixing does not replace payment-date
model spot. This is a new bounded deterministic capability; the existing generic
SPOT/FIX collector and MC semantics are unchanged.

## Supported and rejected contracts

| Input or request                                               | Admission                                                                                                                         |
|----------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------|
| Fixed, resettable and MTM domestic notionals                   | Supported with the existing native signs and exact mappings below                                                                 |
| Both spread legs and receive/pay orientations                  | Derived from every coupon and principal; signed decimal BasisSpread                                                               |
| Different leg frequencies and payment dates                    | Supported when each rate's canonical geometry is exact; tested 3M/6M with foreign payment lag                                     |
| Period-varying principals                                      | Existing FX reset notionals, final exchanges and MTM deltas; no new arbitrary amortization convention                             |
| Rate observations                                              | Exact canonical `IR:CCY,LIBOR_3M_LCH` or `IR:CCY,LIBOR_6M_LCH`, without start override                                            |
| Native rate mapping                                            | Explicit projection tenor, OIS selector, canonical fixing-to-start/end and accrual must all match                                 |
| Collateral and curves                                          | Domestic-currency collateral, explicit OIS discount entries and matching live tenor/currency projections; domestic basis currency |
| Historical-only coupons                                        | Unused projection routes are not required; there is no live forecast fallback                                                     |
| Valuation and fixing times                                     | Exact midnight only; native intraday rates/resets and intraday valuation are rejected                                             |
| Anonymous/custom fixing identity or unmatched schedule/accrual | Rejected by the new importer/preparation; legacy native entries retain their behavior                                             |
| AutoFast / RequireFast                                         | Complete XCCY rule `dal.xccy-cashflows/1`, FastDeterministic                                                                      |
| Derivatives                                                    | Native templated AAD, caller finite differences, or None; unsupported explicit methods fail                                       |
| GenericDeterministic                                           | Explicit rejection: no generic XCCY script executor is claimed                                                                    |
| ScriptMC, including the MC entry point                         | `MCCrossCurrencyUnsupported` before model execution; no stochastic FX/quanto model is added                                       |

Native configuration remains broader than this shared template. A native trade
whose custom/anonymous identity, stub, adjusted endpoint or index/coupon accrual
cannot be mapped exactly is not silently migrated. Existing constructors,
defaults, aliases and bare-product archives remain intact. Save/load of the new
instrument uses the existing definition/binding format; prepared market state is
not claimed to be serializable.

## NativeXccyTimestamp and freezing

The dedicated `XccyScriptContext_` fixes this policy. Generic today-policy
overrides fail `InvalidCalibrationFixingPolicy`.

| Fixing timestamp t versus valuation V | Resolution                                                                            |
|---------------------------------------|---------------------------------------------------------------------------------------|
| t < V                                 | Exact canonical identity/timestamp in the frozen snapshot; missing history fails      |
| t = V                                 | Supplied value, including inverse canonical FX lookup, wins; otherwise active forward |
| t > V                                 | Active forward, even if the snapshot contains a future entry                          |
| Snapshot supplied, including empty    | Authoritative; no global supplementation                                              |
| Snapshot omitted                      | Capture only the strict historical unsettled dependency closure once                  |

Tests cover same-time supplied and empty rate/FX observations, omitted snapshot
with an unused global same-time FX value, future entries, missing history,
same-day-earlier/intraday rejection, custom identity and non-midnight fixing
rejection. An 11:00 value is never copied to midnight. A started three-reset MTM
trade succeeds with only the remaining coupon/latest-principal dependencies.
Changing global history after Prepare leaves the prepared value unchanged.
Snapshot source, actual captured contents, currencies/routes and canonical
day-basis/spot-lag/holiday conventions contribute to plan compatibility.

## Independent numerical evidence

The fixture has N_d=110, N_f=100, valuation 2026-10-21, start 2027-05-04,
domestic coupons ending 2027-08-04/2027-11-04, and a foreign coupon paid
2027-11-08. Initial/final principal exchanges occur on the contractual dates.
Flat continuously compounded discount/projection rates are .04/.02, basis .003,
and spot 1.1. Coupon accruals are ACT/360. This gives unequal leg payment dates
and changing domestic period notionals in reset/MTM modes.

An independent explicit cashflow oracle, native plan, lowered fast plan and the
original bound script tree agree across twelve mode/spread-leg/orientation rows.
For the foreign spread, receive-nonspread/pay-spread cases:

| Mode           | Oracle Price at K=.001 | Oracle decimal par spread |
|----------------|------------------------|---------------------------|
| FIXED          | -0.22000103820950934   | -0.002983490837363863     |
| RESETTABLE     | 1.7252833930615246     | 0.032239173433225896      |
| MARK_TO_MARKET | 3.670567824332557      | 0.06746183770381563       |

Price tolerance is 1e-10 and par tolerance 1e-12. Non-root raw AAD columns in
domestic rate, foreign rate, basis rate and spot match independent h=1e-5 and
h/2=5e-6 cashflow bumps within 1e-8 for both Price and par spread. A separate
Python audit uses independent date arithmetic, cashflow sums and complex-step
derivatives (1e-20) to verify all twelve rows and four raw columns. Full-precision
values, refined FD rows and instrument/plan identities are in the attachment.
These are deterministic tests, not XCCY MC consistency evidence or quote risk.

## RED, GREEN and final verification

Focused RED evidence records the missing FX API and missing shared XCCY context,
then real counterexamples: moved MTM payment accepted, BondPrice K smuggled into
a Price-only spread contract, unchecked Price overflow/annuity direction,
generic today-policy acceptance and unowned custom/global-fixing failures,
unnecessary historical projection routing, stale canonical convention identity,
and same-currency/nonfinite standalone FX results. The corresponding unchanged
assertions pass after the minimum fixes and subsequent refactoring.

The common execution form is:

```sh
cmake --preset Release-linux -B build/dal408 -DCMAKE_CXX_COMPILER=g++-14 -DDAL_BUILD_PUBLIC=OFF -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_PUBLIC_BUILD_TESTS=OFF -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/dal408 --target dal_cpp gtest -j 10
build/dal408/dal408_tests --gtest_filter='XccyScriptTest.*'
```

The evidence helpers retain exact real CMake compile/link argv, selected sources,
RED/GREEN filters, XML and logs. After enabling public with the same core flags,
the core and public libraries and affected clients are freshly linked.

- **31/31 new XCCY tests** pass; the final selected core client passes
  **313/313**, comprising these 31 and 282 existing checks. Native XCCY
  pricing/markets/basis/joint Jacobians, shared admission/persistence, Deposit,
  curve families/adapters and Gaussian/MC compatibility are included.
- **37/37 public** native XCCY, curve instrument/protocol and script archive
  checks pass. Public code is unchanged.
- All six affected translation units pass the repository's GCC 14 warning
  policy. All changed production/test functions have CCN <= 8. Clang-format and
  diff checks pass. No enum markup or generated source changes are needed.
- Final selection excludes frozen statistical numerical suites. The earlier
  313-test selection included three existing Gaussian rate-option numerical
  checks; that separate initial XML is retained, not counted as new XCCY MC
  evidence. Full unrelated suites, frontend/workbook runs and example topology
  matrices were not run; no example or build configuration is changed.

Draft compile/setup failures are retained separately from behavioral RED:
header ordering, test API/constness, an invalid MC-rejection test budget, and a
performance probe missing the benchmark fixture's valuation-date setup. No
acceptance tolerance, sampling protocol, existing test or product assertion was
weakened to resolve them.

## Caller performance and provenance

Changed native FX arithmetic maps to the existing `xccy_perf` MTM 10Y Price and
10-instrument basket. Only those existing workloads run, preserving batch sizes,
three warmups and ten internal repeats. Baseline pricing is compiled from an
isolated `f50db2e9` worktree using the same flags/dependencies and linked ahead
of the current archive; all other selected native dependencies are unchanged.
Two rounds of ten interleaved process samples per side use DAL_NUM_THREADS=4
and best-of-ten minima. Final round deltas are +.873%/-.513% for single Price
and -.381%/+.562% for basket passes. Raw samples and executable hashes are kept.

The new Prepare/Evaluate caller is informational at one and sixteen repetitions
of the small shared template: minima approximately 267.5 us/4.530 ms for Prepare
and 363 ns/5.557 us for Evaluate. Value and plan identity are recorded. XCCY is
outside the closed performance-gate allowlist, so this is not a regression-gate
verdict. The machine is shared; no solver/MC/full benchmark matrix or AAD timing
claim is made.

Build identity is GCC 14.3.0, CMake 4.2.3, C++17 Release/O3/NDEBUG,
ffp-contract=fast, static native AAD, Eigen ON and native architecture OFF.
Examples, benchmarks, Python and Excel targets are disabled in the product
configuration; the narrow performance callers are separately compiled. The
bundle contains exact dependency pins, CMake caches/commands, source and Git blob
identities, archives/client SHA-256 identities, environment and code diff.

No DAL-412 unmerged implementation was imported. The only shared preparation
changes are the dedicated XCCY context/descriptor/admission dispatch and its
metadata; existing compiler identity values and generic paths are retained.
Coordinator review and exact-head CI remain outstanding delivery gates.

## Focused review repair: affine representability and DSL literals

Repair input is PR #620 head `d64cc1ed62c15af3678f93219d00224b8fdc721b`,
with the unchanged merged base `f50db2e93eb16823190ed18b5d73a115f0e9c48f`.
The controlling issue's two P2 findings and reviewer attachment
`01a1632e-d976-7d04-aa8f-ea1b084ad4a8` authorize only the XCCY exporter/provider,
dedicated test file and this record. Shared preparation, protocol/report,
curve adapter, public/bindings and public documentation windows are untouched.
No unmerged DAL-407/412 content is imported.

The original reviewer probe was rebuilt and run before changing production.
It reproduced both finite individual annuities summing to infinity and the
incorrect par zero, and the `1e-7` import failure. The independent scaled par
is `0.0053559072433565372`; the small-ratio native par remains
`-0.0029834908373643772`.

The new wrapper checks representability of both affine coefficients before
forming Price or par. Nonfinite A/B fail `CalibrationNumericalFailure` at
`/affine/a` or `/affine/b`, with Evaluate stage, actual instrument/measure IDs,
the Infinity cause, and an owning report. Tests copy the exception out of a
scope containing the native trade, instrument, plan, market and AAD recording;
all diagnostics remain readable afterward, with no candidate. Double/AAD,
Price/par, A/B overflow, normal-scale positive controls and recording cleanup
are covered. The legacy native overflow result remains zero, as explicitly
required; native implementation and annuity floors are unchanged. String
formatting of a nonfinite coefficient occurs only on the failure path.

The exporter expands a scientific `NumberText` token into its equivalent plain
decimal spelling when embedding a number in an expression. It moves the decimal
point without changing the significant digits. Ordinary tokens and typed N/K
definitions retain their existing spelling. This applies to foreign coupons,
principal exchanges and reset/MTM notional ratios, using the unchanged complete
AST/date matcher. The lexer, compiler, canonical JSON formatter and archives are
unchanged. Regression cases cover the original `1e-7`, `1e-8`, `1.28e-7`,
`1e22`, and a full scientific mantissa, plus native Price/par, immutable binding,
save/load and all three notional modes. Frozen tolerances stay at Price 1e-10,
par 1e-12 and raw derivative 1e-8.

Actual RED/GREEN commands are retained in the repair attachment. The runner
reuses the unchanged main object, recompiles the dedicated test TU from its
actual CMake command and links the current core archive:

```sh
python3 evidence/dal408-r1/run_focused.py annuity-red
cmake --build build/dal408 --target dal_cpp -j 4
python3 evidence/dal408-r1/run_focused.py annuity-green
python3 evidence/dal408-r1/run_focused.py literal-red-corrected2
cmake --build build/dal408 --target dal_cpp -j 4
python3 evidence/dal408-r1/run_focused.py literal-green
python3 evidence/dal408-r1/run_focused.py final-green-corrected 'XccyScriptTest.*:XccyScriptRepairTest.*'
```

The helper paths above are relative to the task workdir; CMake runs in the
repository. The evidence stores absolute argv and working directories for
reproduction. Final result is **37/37**: the original 31 tests plus six repair
tests. Original test/helpers are a byte-identical prefix; all thirteen other
previously selected test sources and the build configuration/dependency pins
are unchanged. Three affected TUs pass the same GCC14 warning policy, all
functions have CCN <= 8, and format/diff checks pass. The original reviewer
probe, unchanged, now reports `/affine/a` and successfully imports the small
ratio. The previous 313 core/37 public and independent twelve-case/raw AAD/FD
evidence is reused only for unchanged paths and numerical conditions; it is
not described as a fresh execution of those larger selections.

Early new-test compilation used nonexistent tape size/iterator methods; the
test now checks the actual node occupancy API. A strict warning also required
const-reference String iteration. These harness corrections are preserved.
An extra full-mantissa MTM comparison at enormous par magnitude exposed the
existing normalized-ratio representation limit: for Nd=1e9 and
Nf=123.45678901234567, `(Nf/Nd)*Nd` is 123.45678901234565. The finite raw par
difference was 3.7252902984619141e-9 at magnitude 1.421230769e7. Neither the DSL
spelling repair nor the native implementation changes that normalization.
That exploratory failure is retained, rather than counted as a passed frozen
precision case. Cross-mode magnitude controls use ratios with exactly
recoverable native notionals (including 1.28e-7); a separate fixed-notional
full-mantissa test retains the original inputs, exact ratio comparison and
unrelaxed Price/par tolerances at ordinary par scale. No claim of uniform raw
precision for arbitrarily ill-conditioned native-to-ratio normalization is made.

Changed-path caller selection is Import for exporter spelling, Prepare for its
full-matcher caller, and Evaluate for both coefficient guards, at 1/16 repeated
MTM contracts. Same GCC14/O3 flags, three warmups, ten samples, 100 inner
Evaluate repetitions, DAL_NUM_THREADS=4 and two rounds of ten interleaved
process samples per side are retained. CPU affinity and all 240 rows are in
the repair bundle. Baseline uses the exact original exporter/provider objects
ahead of the current archive for unchanged dependencies; this is not an
untouched original full executable. Value, checksum and instrument/plan IDs
match exactly. Second-round minima before/after are approximately 123.939 /
126.746 us for Import x1, 312.665 / 308.556 us for Prepare x1 and 395 / 406 ns
for Evaluate x1. XCCY remains outside the closed performance-gate allowlist;
these measurements are informational. Unchanged native, stochastic and full
performance matrices are not repeated.

The 180 historical evidence hashes were checked without modifying that bundle.
Current sources, baseline objects, current archive/clients, commands, RED/GREEN,
configuration, code patch and fresh caller identities are separately recorded.
The final commit/head and exact-head CI snapshot are in the handoff manifest.
Public guide, CHANGELOG and index integration remains pending in the coordinated
window; the earlier accurate draft in this record is preserved. Code repair can
be handed off for limited review independently of that documentation gate.

## Serial public-documentation integration

The code baseline is the reviewed/approved
`bcd0d8ca834c59fdfe5cf7d4bc42f39e327eb24a`. The original review and limited
two-P2 approval at parent comment `01a16349-641b-7c7a-993c-32cd8a21c1e6`
remain applicable; the large-MTM normalization exploration remains a disclosed
failure and is not reclassified as a precision pass.

The current issue and parent comment `01a1634c-25e2-73f1-9197-db37746ca782`
authorize the narrow Markdown window after DAL-407's CI repair. That repair
was safely delivered at `79e773213a2b00ded1a201988d60b3f5528b85a8`, with only
its own record modified among documentation. Before editing, the original
DAL-412 checkout was verified at
`38b0427830659950540a311d5a4997fcfea23295`, with no staged or unstaged paths;
its previously committed documentation window had ended. This establishes
the serial transfer to DAL-408 without waiting for sibling CI or merge.
Only the DAL-408 checkout is written for this integration.

Four Markdown paths change: the existing methodology guide, CHANGELOG, active
artifact index and this record. The new guide section covers one-time source
authority, complete AST/date matching, signed affine measures, explicit routes
and canonical geometry, future/payment/reset FX distinctions, exact timestamp
and frozen-snapshot semantics, numeric failures and the bounded deterministic
capability. Existing P0/405/406/411 prose is preserved. No unmerged DAL-407
interface or DAL-412 generic MC content, new financial family, downstream
adapter or nonexistent artifact link is copied. The guide already has stable
entries in docs/README, methodology README and CLAUDE; those indexes need no
additional entry for a section in the same guide.

Verification is limited to Markdown/link/anchor/diff checks and a full Git-tree
comparison against the reviewed code head. Every non-Markdown mode/type/blob
is identical, and the previous guide text is intact after removing just the
new section. Exact counts, patch, source and final-head identity plus a single
CI snapshot are in the documentation handoff. No product, statistical or
performance test is run for this phase. The 37 repair checks, original
313 core/37 public, independent numerical/compatibility evidence and caller
information retain their recorded identities and applicability. Code Approve
is reused; necessary documentation review and exact-head gates remain for
the coordinator. DAL-408's Markdown writing is complete before the author
returns to the separately authorized DAL-412 Coupon dispatch repair.
