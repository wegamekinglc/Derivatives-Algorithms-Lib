# DAL-426: prepared XCCY basket scalar allocation

## Decision, audience and authority

Select **prepared disjoint scalar-coordinate allocation**, with one immutable
member list and a fixed block containing all three phase reservations for each
member. Retain each member's existing dated grid and five-slot mapping. This
supports a vector of same-model marginal estimates and their sampling-error
covariance. It does not simulate a portfolio on common economic Brownian paths.
There is no caller-supplied offset, range, union-grid switch or stream switch.

This is an active design for the core implementer, facade/binding implementer,
independent tester and focused reviewer. All new names below are **proposals**;
this PR implements no allocator, product behavior or numerical acceptance.
The coordinator must obtain review and actual design merge before dispatching
implementation. No downstream work is dispatched by this note.

The product baseline is accepted master
`4dbdff636bcd92d2a236abe644a3e0c73b9c208a` (M1--M4 merged). Authority remains
[DAL-410 sections 7--8](dal-410-xccy-mc-model-decision.md), SHA256
`c2ff68e1a696b620682b896ca9b292ec6d577bf92cadb3597991b427267733d5`,
[DAL-398](dal-398-script-contract.md), and the
[script calibration methodology](../../../docs/methodology/script_calibration_contract.md).
No model, estimator, ratio, gradient scale, tolerance or budget is reopened.

DAL-425 R2, PR #631 at
`d3765eedee88a8c6a92c9ec396d3937b9c26c12d`, is diagnostic evidence only.
Its attachment `01a169d5-ddee-7dea-90f4-be49b408576b` was downloaded through
Multica and its ZIP SHA256 verified as
`af4b4e6919e2803fd23d40e5c4bd5ae11b0439c79e87afcaddc5e6c60ac153b2`.
The archive's geometry and staged captures show actual pre-draw rejection,
`CalibrationContextMismatch /sampling/mapping`, for D=30/55/80. This is a
correct safety boundary. The original unprepared refusal must remain a negative
control after adding the new prepared path.

M5 / PR #632 at `4bda6772cc9f7b79e8996d3492324e15f8179774` was inspected
read-only for projection compatibility. Neither unmerged tree is imported into
this branch. M5 is not a prerequisite for core allocator implementation; actual
M5 merge/acceptance is required before extending its facade/bindings.

## Current surfaces and source basis

Locations in this table refer to the product baseline unless a head is named.
They are implementation seams, not evidence that the proposed basket exists.

| Source | Existing responsibility and consequence |
|---|---|
| `dal-cpp/dal/script/xccymc.hpp:20` | `XccyMcSettings_`, `/1` reservation/result, and `PreparedXccyMc_` are single-instrument types. No public allocator capability exists. |
| `dal-cpp/dal/script/xccymcranges.cpp:91` | Derives frozen phase prefixes and scalar endpoints `p*D`; at line 115 independence requires identical mapping/D before examining overlap. Keep this old entry point. |
| `dal-cpp/dal/script/xccymc.cpp:56` | Builds a complete interval/innovation mapping from the actual M2 provider; reuse its identity construction. |
| `dal-cpp/dal/script/xccymc.cpp:127` | Source-derived two-coupon coefficient execution, AAD checkpoint and full native chain. Reuse internally, without changing the single runner's seek schedule. |
| `dal-cpp/dal/script/xccymc.cpp:238` | Current runner creates an RNG with NDim=D and seeks by normal-vector path. A basket must use its own checked scalar-seek adapter. |
| `dal-cpp/dal/script/xccyobservations.cpp:158` | M2 derives dates from actual payments and unresolved observations, then binds observation uses to dated model slots. The disjoint route needs no union-slot remapping here. |
| `dal-cpp/dal/model/xccygaussian.hpp:203` | Exact transition `/1`, `xd,xf,Id,If,Z`, D=5 times advancing intervals. Three economic Brownian factors are distinct from five scalar innovations. |
| `dal-cpp/dal/math/random/pseudorandom.cpp:33` | `SkipNormalTo(p)` resets/seeks by `p*NDim()` uniform draws. MRG32's override at line 208 transforms exactly the supplied vector's entries, with one uniform per Precise normal; no antithetic normal cache. |
| `dal-cpp/dal/model/xccygaussiandata.cpp:302` | Numeric refresh checks V/pair/routes, curve/native-map geometry and dynamics buckets/names. Reuse and add full basket mapping checks before draws. |
| `dal-cpp/dal/curve/xccygaussianmoments.cpp:206` | Complete snapshot-to-native Jacobian; basis affects foreign collateral discount/projection/quanto inputs. Do not differentiate snapshots as if they were independent native axes. |
| `dal-cpp/dal/script/xccymcstatistics.cpp:102` | Paired coefficient batches drive Price/par and raw-gradient statistics. Its covariance is covariance of batch means, before division by batch count for mean uncertainty. |
| `dal-cpp/dal/script/xccymcvalidation.cpp:111` | Existing validator admits actual Validation and exact `/1` results/ledgers; never disguise a basket member as a legacy result or relabel HeldOut. |
| `dal-cpp/dal/calibration/protocol.hpp:120` | `CalibrationReport_` is final with fixed `/1` data; `CalibrationFailure_` owns only diagnostic/report/candidate. A basket partial needs its own additive carrier, not an extension hidden in these fields. |
| `dal-public/src/calibrationinstrument.hpp:30` | Shared typed sampling/simulation and owning scalar result; no offset or phase extension to these shared types. |
| `dal-public/src/xccycalibration.hpp:18` | Existing stochastic staged result ownership remains distinct from the new MC basket; the basket consumes its immutable fitted model. |

Read-only M5 seams are `dal-public/src/xccymontecarlo.hpp/.cpp`,
`dal-python/src/bindings/xccymontecarlo.cpp`, and
`dal-excel/src/__xccymontecarlo.hpp/.cpp` plus `__xccyinput.hpp` at the head
above. They supply typed model/request/result, required phase, copied inputs
before GIL release, owning outcome/report/candidate, and lossless raw Excel
table validation. The new projection should follow those patterns only after
they are accepted. Existing generic dispatch and old archives remain unchanged.

Analogous examples inspected: the core MTM calibration program in
`dal-cpp/examples/curves/xccy_mtm_calibration/`, the current facade program in
`dal-public/examples/xccy_script_calibration/`, and M5's proposed separate MC
program. Existing core programs retain location, purpose and core-only links.

## Alternatives and statistical meaning

| Route | Feasibility, cost and decision |
|---|---|
| One union grid and five-slot mapping | Valid common-path solution: prepare every member's payment/fixing uses on the union, generate one exact state path and project states to each source's slots. Requires new M2 plan/provider support because current providers own their grid and validate the complete states against it. Splitting intervals changes Cholesky innovations, mappings and fixed-seed samples. Gives economic cross-member path covariance, useful for a joint payoff. Defer: not needed for the frozen marginal calibration acceptance. |
| Prepared disjoint scalar blocks | Reuses each admitted source/provider and exact transition. Separate members receive disjoint pieces of the same versioned MRG32 scalar stream. Gives marginal expectations, raw partials and the full covariance of these Monte Carlo estimators under this independent-member sampling design. Select as the smallest implementation. |
| Same path ordinal under D=30/55/80 | Invalid: scalar coordinates can overlap across phases/members and do not define shared Brownian intervals. The existing refusal is correct. |
| Round offsets to each D, change seeds, or add a free offset | Unnecessary second allocation rule, hidden stream change, or caller-controlled overlap. Reject. A scalar seek does not need divisibility by D. |
| Strip the guard or reuse GSR/three-factor dimensions | Loses ownership of coordinate geometry and the five-innovation contract. Reject. |

For disjoint members, equal `(phase,batch,path-within-batch)` is an estimator
bookkeeping tuple, **not** a common market scenario. Correlation R among the
three Brownian drivers still operates inside each member's five-slot exact
transition. Cross-member economic payoff covariance, exposure/netting,
nonlinear portfolio payoffs and common-path hedging errors are unavailable.
There is no automatic aggregate price or aggregate par API. A caller may form a
linear combination of marginal estimates using the reported full estimator
covariance and explicit units, without interpreting it as pathwise portfolio
risk. No joint model, variance reduction or MC calibration optimizer is added.

## Immutable preparation and identities

The prepared basket owns copied settings, an ordered nonempty member list, one
immutable model, and one **explicit nonnull fixing snapshot** in its common
XCCY context. An explicitly empty snapshot is valid; missing required history
still fails. Null/omitted snapshot is rejected by this new basket API, before
member preparation. This deliberate required input avoids adding cross-member
global capture or changing M2's distinction between omitted and supplied
history. Single-instrument defaults and omitted/empty semantics are unchanged.
V, pair, collateral and route come from the model at the facade; the core checks
the explicit context against it. Future snapshot values stay ignored and
at-V payment SPOT stays distinct from reset FIX under the frozen policy.

Each member has a required byte-exact unique `memberId`, instrument handle and
measure ID. Member ID is a row key, not a second trade description. Preserve
input order for results and allocation. Do not sort by hash, deduplicate or use
global registration order. A reordered input list is a new allocation, while
evaluating one fixed list in a different internal traversal/call order must
give identical results. Repeated instrument handles/identities with different
member IDs are allowed as explicitly separate sampled rows, each with its own
block. Repeated member IDs fail, even if instruments differ. Repeated economic
rows are neither merged nor counted as independent market observations; users
can identify them through reported source/instrument identities.

Preparation completes for **all** members before any random draw. It admits
the full bound AST/measure, freezes history/policy/conventions, derives legal
low/high coupon bindings as today, prepares both source-derived M2 views, and
derives each complete dated mapping. It then allocates blocks and all phase
endpoints, checks the selected prefix, and freezes the ledger. Internal
coupon views are not additional economic members and consume no extra draws.
An analytic fit reserves Fit but consumes no coordinates; this allocator is
not wired into the existing deterministic staged solver.

Keep these identities separate and report exact version tokens:

| Identity | Required content |
|---|---|
| Member source/preparation | Member ID, instrument/source/definition/binding, measure, compiler/rule, complete observation uses with source locations, dates, supplied snapshot marker/content, policy/conventions, preparation context and low/high binding identities. No pointer addresses. |
| Member mapping | Existing complete `CalibrationMcMapping_` identity, D, ordered dated intervals/slots, clock, exact transition layout, numeraire, generator/seed/stream/precision/bridge. Keep the original mapping identity; allocation does not masquerade as a new transition. |
| Basket allocation | New `dal.xccy-gaussian-basket-coordinate-ranges/1`, ordered member IDs and source/binding/measure/policy/geometry identities, mapping hashes/D, full-phase capacity T, member blocks and every reserved phase scalar interval, seed/stream/normal/seek convention. Independent of selected phase/counts, compiled/tree, derivative method and candidate numeric market content. |
| Execution reservation | Allocation identity plus selected explicit phase, batches, paths per batch, ordered actual path/scalar intervals and expected consumption. Different prefixes have different reservation identities. |
| Prepared/evaluation context | Prepared identity adds original numeric model identity and execution settings, labels and selected reservation. Each evaluation adds actual candidate model/content and native-map identity. Numeric refresh retains allocation only when all geometry and mappings remain identical. |

Use existing canonical fingerprints and exact ordinal text, never formatted
doubles or case-insensitive token comparison. A phase-independent allocation
identity lets Fit/Validation/HeldOut and x1/x2/x4 be checked together without
stateful registration. Changing any source/member date, history/policy, member
order, membership, D, mapping or native geometry requires preparation again.
Candidate spot/curve/sigma numbers may refresh through existing
`RequireSameGeometry`, with every member's full dated mapping rechecked. Refresh
does not mutate the plan. Changing source K/N remains immutable rebind and new
preparation, not an unchecked numeric-model refresh.

## Coordinate law and checked scalar seeking

All intervals below are half-open. Let `T=2359296`, the unchanged total
reserved paths per member across all phases. Member i has dimension D_i and
block start O_i:

```text
O_0 = 0
O_(i+1) = O_i + T * D_i
member block i = [O_i, O_(i+1))

phase       F          E          initial batches * n
Fit         0          262144     16 * 4096
Validation  262144     1310720    16 * 16384
HeldOut     1310720    2359296    16 * 16384

reserved phase coordinates i = [O_i + F*D_i, O_i + E*D_i)
batch b: p_begin = F + b*n; p_end = p_begin + n
actual batch coordinates = [O_i + p_begin*D_i, O_i + p_end*D_i)
path p, interval j, slot s: c = O_i + p*D_i + 5*j + s
j is zero based; s=0..4 denotes xd,xf,Id,If,Z
```

The phase path ordinals remain member-local. For D_i>0 the inverse for a
coordinate c in its block is `p=(c-O_i)/D_i`, remainder r=`(c-O_i)%D_i`,
`j=r/5`, `s=r%5`; check membership before subtracting. No division by D=0.
O_i is derived from **full** reservations, never actual consumed prefixes, so
changing selected phase/counts cannot move any other member. There is no union
mapping, union D or implied common-date observation alias.

Implement a private checked adapter using an ordinary fresh
`New(RNGType_("MRG32"), seed, 1, true)`. At the start of each batch call
`SkipNormalTo(firstCoordinate)` on that dimension-one engine, then fill the
member's D_i-element normal vector with `FillNormal`. This is scalar seeking:
one engine path equals one scalar coordinate. MRG32's existing FillNormal
iterates the supplied vector and does not rely on its NDim cache. The adapter
must check vector length equals D_i and its expected end cursor; it must never
call FillUniform, SkipTo, Branch or change normal precision. For D_i=0 do not
construct, seek or draw an RNG. Do not call `SkipNormalTo(firstCoordinate/D_i)`:
O_i need not be divisible by D_i. No public or underlying RNG API change is
needed. A deterministic adapter test is required before relying on this seam.

Checked arithmetic precedes allocating vectors/matrices, casting or seeking:

1. Validate phase, signed integer counts and seed/version before conversion;
   at least two batches of at least two paths, nonnegative seed, exact stream.
2. Check batches*n and capacity `batches*n <= E-F`. Check all products/additions
   for T*D_i, cumulative block ends, both reserved endpoints, each batch's
   path/scalar endpoints, slot coordinates and cumulative consumption. Check
   **full** reservation ends, even for a tiny selected prefix.
3. The executable scalar seek parameter is size_t; MRG32 skip matrices/state
   use uint64_t and int64_t internally. The supported exclusive endpoint must
   fit `min(SIZE_MAX, UINT64_MAX)` without narrowing, and actual matrix/vector
   dimensions must fit their real int/size_t indices. Do not assume a 64-bit
   build because the generator's state is 64-bit. No binary64 ordinal cast.
   T*sum(D_i) fitting is necessary but cannot replace individual checked ops.
4. Verify recomputed mappings, frozen header/identity, contiguous equal-size
   batch prefixes, range containment, phase/block separation, ordered axes and
   exact expected consumption. Reject malformed ledgers before draw; they are
   read-only outputs, not accepted as caller configuration.

The unchanged x2/x4 refinement doubles/quadruples n with 16 batches. The total
phase sample set is a prefix, and x1/x2 are overlapping refinements, not new
independent experiments. Batch boundaries regroup when n changes: batch b>0
does not keep the same start. Compare shared normals by absolute member/path/
scalar coordinate, not by batch ordinal. Do not pool refinements for SE.
For any run with N=batches*n, expected consumed coordinates are
`N*sum(D_i)`, once per member path regardless of A/B, AAD, or tree/compiled.
Seeks and reservations are not consumption. Record separately the consumed
count of each explicitly repeated CRN evaluation; do not call repeats fresh
independent samples.

### Concrete D=30/55/80 allocation

These are **derived planned endpoints**, not product MC execution evidence.
The order is the original three instruments, seed 1024. Full blocks are
`[0,70778880)`, `[70778880,200540160)`, `[200540160,389283840)`.

| D | Phase | Reserved scalar interval | Initial actual scalar prefix |
|---|---|---|---|
| 30 | Fit | [0,7864320) | [0,1966080) |
| 30 | Validation | [7864320,39321600) | [7864320,15728640) |
| 30 | HeldOut | [39321600,70778880) | [39321600,47185920) |
| 55 | Fit | [70778880,85196800) | [70778880,74383360) |
| 55 | Validation | [85196800,142868480) | [85196800,99614720) |
| 55 | HeldOut | [142868480,200540160) | [142868480,157286400) |
| 80 | Fit | [200540160,221511680) | [200540160,205783040) |
| 80 | Validation | [221511680,305397760) | [221511680,242483200) |
| 80 | HeldOut | [305397760,389283840) | [305397760,326369280) |

x4 fills each reserved phase exactly. Initial HeldOut consumes 43,253,760
scalars across members; full three-phase reservations total 389,283,840
scalars, with at most 2,359,296 paths **per member**, not per economic basket.
The first member happens to retain the old `/1` coordinates; later members
deliberately differ. None of the prior captured estimates is overwritten.

The original R2 mappings remain, as independently captured in both staged
solve modes. Grids below have V as element 0; each consecutive pair gets the
five slots above. Dates are not rebuilt from a second trade:

```text
D30: 2026-10-21, 2027-04-30, 2027-05-04, 2027-08-02,
     2027-08-04, 2027-11-04, 2027-11-08
D55: 2026-10-21, 2027-04-30, 2027-05-04, 2027-08-02,
     2027-08-04, 2027-11-02, 2027-11-04, 2027-11-08,
     2028-02-02, 2028-02-04, 2028-05-04, 2028-05-08
D80: 2026-10-21, 2027-04-30, 2027-05-04, 2027-08-02,
     2027-08-04, 2027-11-02, 2027-11-04, 2027-11-08,
     2028-02-02, 2028-02-04, 2028-05-02, 2028-05-04,
     2028-05-08, 2028-08-02, 2028-08-04, 2028-11-04,
     2028-11-07
```

At seed 1024 their existing mapping hashes are respectively
`47d8cbb515b7160dac446e6810a52cad3a0a64c0e464b21759604b8088628ece`,
`d005a08bc0a3adcea595bf0add2ac49b10b021c025fb9d8bcefb34ee023c42de`,
`dd9895f992c4abfc5047d51a65f90ddf0b1871027c2a1c167b3612d9805122ae`
(each with `sha256:` prefix). The source periods retain canonical USD 3M and
EUR 6M index identities, fixing dates, accruals and actual payments from the
R2 geometry. Final EUR payments are 2027-11-08/2028-05-08/2028-11-07;
the free basis knots remain 2027-11-08/2028-05-08/2028-11-08. The refused
2028-11-06 maturity is not reinstated. A complete execution trace must join
member ID, source event/statement/observation use, canonical index/time, local
sample/slot and interval/innovation to `(p,c)` and the checked batch interval.
The grids/hashes alone do not certify that trace.

## Independence, replay and degenerate inputs

A basket-specific checker validates two **owned prepared executions**, with
default purpose Independent and explicit purpose CommonRandomNumbers for
comparisons. Do not overload the old `/1` checker or expose mutable range
inputs. Independent mode requires the same allocation family/identity and
stream definition and checks every nonempty scalar interval in one execution
against every interval in the other, across all members. Different phases of
one allocation are disjoint. Same-phase repeats and x1/x2/x4 overlap and fail
Independent. Changing seed is not a shortcut to independent certification.
Different basket membership/order/mapping allocations are incompatible for
this checker; no automatic cross-basket or old-single/basket independence claim.

CommonRandomNumbers requires identical allocation, selected phase, counts,
actual coordinates, source/binding/policy geometry and stream. Numeric model
contexts may differ only through the checked same-geometry refresh; different
numeric model identities remain in both results. Compiled/tree and AAD/value
comparisons may reuse identical ranges deliberately, with their execution
method/labels reported. The choice is an explicit comparison declaration,
not a mutable global RNG cursor or an input offset. Repeated Evaluate on one
plan is deterministic replay by contract, never automatic independent evidence.

D=0 members have empty `[O_i,O_i)` blocks and empty coordinate ranges. Preserve
their member/phase/path reservation metadata, mark deterministic, and consume
zero scalars. Price and its qualified raw derivatives are deterministic; SE=0
does not imply random replication. Skip empty intervals in overlap arithmetic
and report that independence applies only to stochastic members. An all-D=0
basket may return deterministic Price estimates, but Independent comparison of
two all-deterministic executions is rejected. An empty basket fails rather
than returning a fabricated zero or a dimensionless aggregate. One-leg-settled
members use the existing admitted source/provider; do not assume D=0. All-
expired par, or any failed annuity floor/sign/confidence condition, remains
`InvalidCalibrationAnnuity`; legacy refusal outside its domain remains tested.

## Errors, stages and owning failures

Use the basket-specific `XccyMcBasketError_` below, derived from
`CalibrationError_`, with the existing owning diagnostic/report/candidate. Include
member ID and index, instrument/measure, stage, actual/required values and
source event/statement/index/time when applicable. Basket structural errors
have no invented instrument. Member failures retain their existing field path
and use diagnostic row/member context to identify ownership; do not replace
`/model/...` with a vague basket error. Basket structural paths below are new;
existing single paths/codes do not change.

| Condition | Owning code / field | Stage and behavior |
|---|---|---|
| Empty members, null instrument, empty/duplicate member ID | `InvalidCalibrationPlan /members`, `/members/<i>/instrument`, or `/members/<i>/member_id` | Prepare; name offending index/key. |
| Missing explicit snapshot for the new basket, null model | `InvalidCalibrationPlan /context/fixings` or `/model` | Prepare; empty typed snapshot is accepted. |
| Unset/unknown phase, including malformed version/token | `InvalidCalibrationPlan /phase`; unknown layout at `/sampling/layout`, stream at `/sampling` | Input/Prepare, before providers/workers; show actual and legal token. Layout is fixed output, not an option. |
| Noninteger/negative/too-small counts, bad seed; any count/coordinate/byte arithmetic or index-capacity overflow | `InvalidCalibrationPlan /sampling` | Input/Prepare; actual operand, limit, member and endpoint. Validate signed inputs before casting. |
| Valid count exceeds selected fixed phase capacity | `CalibrationBudgetExceeded /sampling` | Prepare; requested paths and phase capacity; no clamping or extension. |
| Incompatible context/model/candidate geometry | `CalibrationContextMismatch` at existing `/model/market`, `/model/market/refresh`, `/model/market/refresh/curves`, `/model/market/refresh/projections`, `/model/market/refresh/dynamics`, or precise existing coverage field | Prepare or Evaluate preflight; preserve the original narrower field. |
| Stale member hash/D/grid/slot/date/native-map or wrong allocation family | `CalibrationContextMismatch /sampling/mapping` | Prepare/Evaluate preflight or Validate checker; even equal D with changed dates fails. |
| Changed range/header/identity/count or invalid CRN declaration | `CalibrationContextMismatch /sampling/ranges` | Prepare/Evaluate preflight or Validate checker; expected owned ledger required. |
| Overlap declared Independent; all-D=0 claimed independent | `InvalidCalibrationPlan /sampling/ranges` | Prepare/Evaluate preflight or Validate checker; show both intervals/member IDs, or absence of random coordinates. |
| Unsupported product/collateral/domain; missing history/intraday/override | Existing `MCCrossCurrencyUnsupported` or admission `UnsupportedCalibrationProduct`, `MissingFixing`, `UnsupportedCalibrationFixingTime`, `InvalidCalibrationFixingPolicy`, with existing precise fields | Prepare, for every member before any draw. Do not loosen admission to create a basket. |
| Unsupported/duplicate/unknown raw label, R/a, rank/boundary, derivative-method mismatch or tape mode | `DerivativeUnavailable /derivatives`, `/mc/parameter_labels`, `/derivatives/sigma`, `/derivatives/tape`, or existing `/derivative_method` | Prepare/Evaluate preflight; preserve narrower existing diagnostic. No fake zero or partial AAD success. |
| Finite but unqualified par denominator | `InvalidCalibrationAnnuity` at existing `/annuity` for direct evaluation, `/mc/annuity` for MC, `/validation/annuity` for validation confidence | Prepare when structurally known, otherwise Evaluate/Validate after coefficients; retain measured A/sign/floor/SE. |
| Extra/missing draw or provider vector dimension/cursor mismatch | `CalibrationContextMismatch /sampling/mapping` for layout; `/sampling/ranges` for actual consumption | Preflight if knowable; otherwise Evaluate at the deterministic cursor assertion, not a statistical parity test. |
| Nonfinite path/coefficient/gradient/covariance or unresolved affine subtraction | Existing `CalibrationNumericalFailure /execution/...` or `/derivatives/...` | Evaluate when observed; retain cause and last complete results, never pad missing batches with zeros. |
| Post-draw failure, including late-member, late-batch or summary/annuity failure | Preserve the cause's exact code/field above, with `XccyMcBasketError_::Partial()` | Evaluate; owned progress/rows/ranges survive unwind. This is a failed basket, even if some members completed. |
| Basket outcome getter requests an absent prepared handle, result, partial or validated model | `CalibrationCandidateUnavailable /outcome/prepared`, `/outcome/result`, `/outcome/partial`, or `/outcome/model` | Input, at the new getter; name the unavailable object. Never substitute a zero, original model or empty successful result. |
| Request to validate basket as legacy `/1`, or relabel HeldOut/ Fit as Validation | Existing `InvalidCalibrationPlan /validation/identity` or `/validation` | Validate; new ledger must be checked before applying unchanged statistical arithmetic. |
| Frozen precision/combined budget failure | Existing numerical-validation failure flags/report, completed estimates retained | Validate; no additional paths, seed/VR/scale changes, or successful-estimate-to-acceptance conversion. |

Input, capacity, mapping, geometry, history and derivative-domain refusals
must happen before **any** basket draw, including when the last member fails.
Preflight all candidate providers/mappings/native axes and tape availability
before executing the first member. Numerical path, actual-consumption and
annuity-confidence failures necessarily can occur after drawing; the contract
does not promise pre-draw detection of observed numerical failures. A thrown
failure owns its report and only a fully geometry-validated candidate.
Preserve complete member/batch coefficients and explicit completion counts in
the separate owned payload below. AAD scopes must restore the caller tape on
every failure, before control reaches any facade/binding catch.

### Owned partial payload and failure carrier

Choose a **derived basket error with a separate immutable payload**. Do not
put a wrapper in `CalibrationFailure_::candidate_`: that slot continues to mean
the actual validated model, and existing translators already recognize model
candidates. Do not change `CalibrationReport_`, its schema, the generic error
protocol, or the old single runner. Core types belong in `xccymcbasket.hpp`:

```cpp
struct XccyMcBasketPartialSnapshot_;
class XccyMcBasketPartial_ final {
    struct Impl_;
    std::shared_ptr<const Impl_> impl_;
    explicit XccyMcBasketPartial_(std::shared_ptr<const Impl_> impl);
    friend class PreparedXccyMcBasket_;
    // Private core producer freezes the owned snapshot/report; no default instance.
public:
    const XccyMcBasketPartialSnapshot_& Snapshot() const;
    const Handle_<CalibrationReport_>& Report() const;
};
class BASE_EXPORT XccyMcBasketError_ final : public CalibrationError_ {
    Handle_<XccyMcBasketPartial_> partial_;
public:
    XccyMcBasketError_(const std::string& file, long line,
                      const std::string& function,
                      std::shared_ptr<const CalibrationFailure_> failure,
                      Handle_<XccyMcBasketPartial_> partial);
    const Handle_<XccyMcBasketPartial_>& Partial() const noexcept;
    // Failure() remains the inherited diagnostic/report/validated candidate.
};
```

`Handle_` shares immutable ownership. The producer copies the base
`CalibrationFailure_` into its existing shared ownership and freezes the
partial snapshot; no references point into plan, runner, tape or caught error.
Copying the derived error preserves both owners. Catch it by const reference
before `CalibrationError_`; rethrow with `throw;`, never reconstruct/slice it as
the base error. Legacy C++ catches still see the original code, field, report
and candidate. The prepared constructor and Evaluate use this basket error
for their calibration failures. A failure before a complete prepared allocation
exists has `Partial()==nullptr`, plus its normal owning diagnostic/report.
Evaluate preflight already has an allocation: its partial has zero draws and
no completed rows, including for a rejected numeric candidate.

`XccyMcBasketPartialSnapshot_` is a dedicated owned data record, with const-only
inspection through the payload. Its schema is
`dal.xccy-gaussian-mc-basket-partial/1`, distinct from any complete result.
All referenced record names below are new core types; their contents are fixed
by this table, rather than an untyped report extension:

| Record / accessor data | Required owned content and availability |
|---|---|
| `header_ : XccyMcBasketExecutionHeader_` | Plan/allocation/layout/execution/stream versions and identities; phase, seed, precision, requested batches/paths/derivative method; native labels/order/units; frozen context, policy, fixing and convention identities; prepared model identity and attempted evaluation model/content/native-map identities, with `candidateGeometryValidated_`. Missing/unreadable attempted identities are explicitly absent, never replaced by the prepared identities. |
| `allocation_ : XccyMcBasketAllocation_` | Copy of the entire immutable member order, member IDs, instrument/measure/source/definition/binding/preparation/mapping identities, dated grids/slots/D/native-map identities, all three reserved phase blocks and selected planned batch ranges. These are planned ranges, never labeled consumed. |
| `members_ : Vector_<XccyMcBasketMemberProgress_>` | One row for every original member, including NotStarted members. State is `NotStarted`, `InProgress`, `CoefficientsComplete`, or `Complete`; keep member identity and any observed failure location. `Complete` requires every requested batch, derivative and member summary check to succeed. A member whose complete coefficients fail annuity confidence is only `CoefficientsComplete`. |
| Each member's `batches_ : Vector_<XccyMcBasketCompletedBatch_>` | Only committed, finite complete batch rows. Each owns original batch ordinal, path/scalar endpoints, path count, first normal vector and ordered `(B,A,dB_1,dA_1,...,dB_q,dA_q)` coefficient means/units. For None, q=0; for AAD, all requested derivative columns must finish before row commit. Never append a short row or publish an unfinished batch mean. |
| Each member's `actualRanges_`, `consumedCoordinates_`, `completedPaths_`, `completedBatches_` | Exact generated scalar spans/cursor and count, independently of successfully evaluated full coefficient paths and committed batch rows. A generated path vector counts as consumed even if its pricing/AAD fails. Completed paths may include valid paths from an unfinished batch; those paths do not create a committed batch row. Consumed spans never include unexecuted reservations or seeks themselves. D=0 keeps exact zero consumption and the deterministic marker. |
| Each member's `completedEstimate_` | Optional owned member estimate, coefficient means/covariance and requested raw gradients/SE, present only for a `Complete` member. Earlier completed members retain these data. Incomplete members have no finalized Price/par, gradient, covariance or SE, even if their raw coefficient rows are available. None always has absent gradient fields. |
| Optional retained paths | When requested in the original settings, only fully evaluated finite coefficient tuples with original member/path coordinates and all requested derivative columns. No partially evaluated path or failed batch aggregate is substituted. With retention off, only the rows/counts above are retained. |
| Snapshot completion/failure fields | `completedMembers_`, per-member completed counts, total `consumedCoordinates_`, and optional failed member/batch/path plus observed cursor. Missing location is absent for summary-stage failures. `basketComplete=false`; basket estimate, joint C_batch/cross-member projections, basket SE and acceptance flags are unavailable on every failure, even if all coefficient rows exist. No implicit resume, pooling or certification from partial rows. |

Progress counters use checked integer types as in the allocation law. A batch
commit is atomic across value and all requested raw derivatives. Already
committed rows and completed member summaries are preserved on a later failure;
uncommitted accumulators are discarded. Member/batch IDs retain their original
order, including under reversed execution traversal. If actual cursor checks
fail, publish the observed consumed spans and mismatch diagnostic, not the
expected full range. Report completion counts separately from planned capacity
and random replication; no total over members becomes a larger sample count.

`Report()` shares the same owning `/1` report as `Failure().report_`; it contains
the diagnostic and applicable ordinary report data, not the partial extension.
The payload owns copied identity strings, mappings, rows, counters and optional
completed summaries. It needs no caller input handles to remain readable.
The failure's candidate is the **actual evaluation model** only after geometry
and native-map checks have succeeded for every member. A rejected refresh or
last-member geometry mismatch leaves candidate null; do not publish the
attempted model, or silently fall back to the original prepared model. After
full geometry validation, a later numerical/annuity failure can retain that
model without claiming a successful basket or calibrated candidate.

The core payload is a non-Storable immutable object. No archive reader, report
extension slot, diagnostic-text encoding, double-valued coordinate table or
external last-error/shadow store is introduced. The facade supplies explicit
JSON export and bindings hold normal shared owners, as specified below.

## Coefficients, native partials and full covariance

For each member preserve current source-derived `B+K*A`, legal coupon views,
gross-cashflow rounding checks and raw-native AAD. Generate one member normal
vector per path and reuse it for both coupon views. Reuse scalar recording and
checkpoint discipline; do not keep multiple members' active tapes concurrently.
Member-major execution can retain B batch coefficient rows per member and
concatenate after all members complete. Execution order cannot affect draws.

Common ordered native labels are explicit for AAD; each member uses the same
model/native-map axes, though true sensitivities can be zero. Apply the entire
`XccyGaussianMomentNativeMap` chain through discount, projection and basis
including foreign collateral/quanto dependence. New numeric candidates rebuild
the map and coefficients; never reuse stale numeric moments. All supported
interior labels must be qualified on all members before drawing. A requested
unavailable derivative fails the requested evaluation; the caller can prepare
an explicit value-only request. None returns absent gradient fields, not zero
gradient placeholders. Raw partials hold fitted curves fixed and are not
calibrated quote risk.

CRN finite differences are diagnostic caller evaluations using the same
prepared allocation and same-geometry bumped models at h/h2/h4. Never refit
on HeldOut paths, seek by bumped D, or draw a new seed per sign/member.
Paired coefficient/gradient differences must be formed batchwise before
their SE. The production API still accepts None/AAD; no new FD engine or
one-sided-bound policy is added. Zero vol, rank changes and real bounds retain
qualified one-sided FD or DerivativeUnavailable, as prescribed in DAL-410.

For m members and q requested labels, build exactly B equal-size paired batch
rows in this fixed order:

```text
z_b = concat_i [B_i,b, A_i,b, dB_i,b,1, dA_i,b,1, ..., dB_i,b,q, dA_i,b,q]
z_bar = sum_b(z_b)/B
C_batch = sum_b (z_b-z_bar)(z_b-z_bar)^T / (B-1)
C_mean = C_batch / B
```

Publish all axes, units, B/n and the complete square C_batch, including measured
cross-member blocks. Do not zero them by an independence assumption, truncate
to per-member matrices, zip mismatched axes, or call covariance of batch means
path covariance. Rank deficiency with 16 batches and many axes is expected;
no inversion/rank-based risk claim is needed. Check dimensions/storage
capacity before allocation and finiteness/symmetry on completion. Per-member
diagonal blocks must agree with the existing coefficient statistic. Sample
cross-block values need not be exactly zero for disjoint random inputs.

Price uses weights `(1,K)` on `(B,A)`, and its raw partial uses `(1,K)` on
the corresponding `(dB,dA)` columns. Par is `-B_bar/A_bar`; its derivative is
`-dB_bar/A_bar + B_bar*dA_bar/A_bar^2`. Apply the full delta-method Jacobian
to C_mean, including cross-member entries if comparing multiple estimates.
Within each member, keep the complete `(B,A,dB,dA)` covariance for par-gradient
SE and the signed annuity gate `abs(A_bar)-3*SE_A>A_min`. Never average path
or batch par ratios and never substitute Price dispersion for gradient SE.

Do not divide by member count or treat the m entries of one z_b as m extra
replicates. There are B batch vectors. This layout estimates uncertainty of
independently sampled marginals; the full cross-block matrix describes that
estimation design only. Shared-source Price/par diagnostics can be derived
from one member's same coefficients without adding duplicate sampled rows.
The existing ordinary-MRG32 SE assumptions, validation formulas, per-measure
scales and separate parity/precision/combined flags stay fixed.

## Smallest developer-facing surface

Core owns a new dedicated basket header/implementation, with no facade types
or callbacks. Required arguments precede optional defaults. Proposed shape:

```cpp
struct XccyMcBasketMember_ {
    String_ memberId_;
    Handle_<CalibrationInstrument_> instrument_;
    String_ measureId_;
};
class PreparedXccyMcBasket_ {
public:
    PreparedXccyMcBasket_(const Vector_<XccyMcBasketMember_>& members,
                         const CalibrationPreparationContext_& context,
                         const Handle_<XccyGaussianModelData_>& model,
                         const XccyMcSettings_& settings);
    const XccyMcBasketAllocation_& Allocation() const;
    const String_& PlanIdentity() const;
    XccyMcBasketResult_ Evaluate(
        const Handle_<XccyGaussianModelData_>& candidate = {}) const;
};
```

These are owning pimpl/value types. `Allocation()` is a const inspection view
valid for the lifetime of the prepared object; Evaluate returns a self-owned
result surviving destruction of plan, inputs, model caller handles and bindings.
There is no public constructor from a mutable ledger and no member Evaluate
overload accepting ranges. A typed execution-purpose enum is sufficient for
the basket-specific independence/CRN checker; no new platform API is needed.

`XccyMcBasketResult_` owns the actual model handle, ordered member results,
source/preparation/mapping identities, immutable allocation and actual ranges,
phase/counts/versions, B coefficient rows, coefficient means, covariance axes
and C_batch, per-member measure/SE/raw gradients, first normal vectors and
actual consumption, deterministic/completion markers and an owning report.
Use a new result family `dal.xccy-gaussian-mc-basket-result/1` and execution
rule `dal.xccy-gaussian-script-mc-basket/1`. Member estimates may reuse internal
`CalibrationMcResult_` arithmetic, but are **not** `XccyMcResult_` `/1` with
patched reservations. Full successful results require all members complete.
Optional retained paths contain per-member coefficients, never a common
economic path or pathwise par ratio.

Expose complete-result details through a separate versioned basket JSON,
`dal.xccy-gaussian-public-basket-result/1`, with exact integer strings for all
ordinals and counts. Keep the existing CalibrationReport `/1` columns and
single-result JSON untouched. The existing report can carry the ordered member
observations/plans/model and diagnostics; the new result is authoritative
for member scalar ledgers, cross-covariance axes and completion. A thrown error
instead owns `XccyMcBasketPartial_`; it does not require a complete result to
export partial details. Do not encode
offsets in `firstPaths_` doubles or infer basket coordinates from old tables.

After M5 actually merges, the additive facade can mirror the core:

```cpp
struct XccyMonteCarloBasketRequest_ {
    XccyMonteCarloPhase_ phase_;                  // required
    CalibrationMonteCarloSampling_ sampling_;    // required
    Handle_<MarketFixingSnapshot_> fixings_;      // required, may be empty
    CalibrationSimulationSettings_ simulation_{false, {}, 0., "Precise"};
    CalibrationDerivative_ derivativeMethod_ = CalibrationDerivative_::Value_::None;
    Vector_<String_> parameterLabels_;
};
// Public member type can alias the core typed member; no duplicate trade fields.
PreparedXccyMonteCarloBasket_ PrepareXccyBasketMonteCarlo(
    const Vector_<XccyMcBasketMember_>& members,
    const Handle_<XccyGaussianModelData_>& model,
    const XccyMonteCarloBasketRequest_& request);
String_ DumpXccyMonteCarloBasketPartialJson(
    const Handle_<XccyMcBasketPartial_>& partial);
```

The facade prepares its context from the explicit model and snapshot, validates
existing shared sampling/simulation and None/AAD restrictions, and delegates
to the core. The prepared facade exposes Evaluate(candidate={}), allocation
inspection and a distinct owning basket result/JSON. It passes
`XccyMcBasketError_` through unchanged with `throw;`, preserving dynamic type,
the base report/candidate and the partial's identical report owner. Function/
argument input context is captured before delegation; no post-catch report
rewrite can disconnect it from the payload. Facade input failure before
preparation has no partial. The partial dump function exports
the snapshot schema above with `status="Failed"`, `basket_complete=false`,
all member states/rows/identities/ranges/counts and explicit unavailable fields
as JSON null. Every ordinal/count is an exact integer string; absent rows are
omitted, not zero-filled. A null payload gives
`CalibrationCandidateUnavailable /outcome/partial`. No generic report dumper
is changed. Do not add a generic
Price wrapper, new market defaults, archive fields or alternate stochastic
staged solver. A bare basket result has numerical acceptance unavailable.
The existing single validator remains unchanged; the independent V1 helper
must verify the new ledger/source/model identities before applying the frozen
per-member statistical formulas. A future product basket validator, if needed,
requires a separately checked new-family wrapper; it cannot pass fabricated
single `/1` results into the old admission check. No new validator is necessary
to deliver this allocator and its independent HeldOut acceptance.

Python maps to `XccyMonteCarloBasketRequest_(phase, sampling, fixings, *, ...)`,
typed `XccyMcBasketMember_(member_id, instrument, measure_id)`,
`XccyBasket_PrepareMonteCarlo(members, model, request)` and
`prepared.evaluate(candidate=None)`. Read/copy/validate all Python-owned input
before GIL release; no worker Python callbacks. Expose read-only owned result
members, covariance axes/matrix and details JSON; returning a member must keep
the parent result alive or return an owning copy. Python omission of a required
argument is TypeError naming it; typed null/invalid input uses owning
CalibrationError with the core code/path.

Choose one local Python path: register `XccyBasketCalibrationError` in the new
basket binding as a subclass of the existing Python `CalibrationError`, with
the same message/code/diagnostic/report/candidate properties and a read-only
`partial` property. Register an immutable `XccyMcBasketPartial_` binding with
read-only `header`, `allocation`, `members`, `completed_members`,
`consumed_coordinates`, `failure_location`, `report` and `details_json`.
Nested progress/batch properties follow the snapshot table; return owning
copies or shared immutable owners, never borrowed plan/error references.

Both new Prepare and prepared.evaluate wrappers catch
`XccyMcBasketError_` **locally first**, after the GIL-release scope has unwound,
and raise this Python subclass with copied base fields and the shared partial.
A following local catch of plain `CalibrationError_` raises the same subclass
with `partial=None` for early facade/input failures. Create Python objects
only with the GIL held; the worker executes no Python code. `candidate` is the
existing typed actual model or None, never a payload wrapper. Existing
`except CalibrationError` handlers therefore keep working with identical
code/path/report/candidate. The old global translator, Candidate helper,
exception constructor and all old single bindings stay unchanged; do not rely
on that translator to discover the new payload. No new module-wide exception
translator is registered. Missing required Python arguments still produce
TypeError before the call, rather than a fabricated partial.

Excel uses typed member-list, basket-request, prepared-basket and outcome
handles. Proposed member factory accepts one complete three-column table
`member_id,instrument,measure_id` with typed instrument handles; request factory
has required phase, typed sampling and typed fixing snapshot, followed by
optional settings. Preparation and evaluation are separate functions with at
most four required inputs each. Reuse M5's lossless full-dimension raw cell
validation and duplicate-key/row diagnostics, including Excel error cells
before coercion. Phase is not an optional settings row. No offsets or models
are hidden in text settings. Preserve raw input row numbers, core code/path
and function/argument/row/column context; wrong native types fail at input.
The new Excel-only `StorableXccyMonteCarloBasketOutcome_` owns a basket outcome
value with `status` (`Prepared`, `Estimate` or `Failed`), owning diagnostic/report,
optional prepared handle, optional geometry-validated model, optional complete
result and `Handle_<XccyMcBasketPartial_> partial`. Prepared has only the fully
prepared handle/report/model; Estimate requires a complete result and has no
partial; Failed has neither prepared handle nor complete result and may have
a partial. New Prepare/Evaluate entrypoints catch the derived error before the base error;
the derived catch copies diagnostic/report/candidate/partial, leaving result
and prepared absent. The base catch records a null partial for early failures.
`XccyBasket_PrepareMonteCarlo(members,model,request)` returns this outcome;
successful Prepare has status Prepared without claiming an estimate.
`XccyBasket_EvaluateMonteCarlo(prepared,candidate={})` requires the prepared
handle and returns Estimate or Failed. No generic outcome or single entrypoint
changes.

Required new getters are `XccyMonteCarloBasketOutcome_Get_Status`,
`_Get_Report`, `_Get_Prepared`, `_Get_Model`, `_Get_Result`, `_Get_Partial`
and `_Get_Json`.
`_Get_Partial` returns a new typed
`StorableXccyMonteCarloBasketPartial_` holding the same shared core payload;
it remains readable after both the outcome and prepared handle are released.
`XccyMonteCarloBasketPartial_Get_Json(partial)` calls the facade partial dump
directly, without reading a complete result. `_Get_Json(outcome)` exports a
new `dal.xccy-gaussian-public-basket-outcome/1` envelope with status,
diagnostic/report JSON, prepared-plan identity, validated-model identity,
`result` and `partial`; the absent alternative is null.
The partial subtree is exactly the partial
dump above. Use existing Excel diagnostic chunking for long JSON strings,
preserving exact integer strings and all unavailable markers.

Getters for absent prepared/result/model/partial use the owning input errors in the
table above. Incomplete member gradients/SE are unavailable; finalized fields
of an earlier Complete member remain explicitly member-scoped in the partial.
No failed outcome has a basket Mean/SE getter returning zero. The Excel handle
repository owns these normal typed values until released; each returned partial
wrapper independently shares ownership. No hidden per-call lookup is needed.
These are transient session handles: their Write follows existing
`CalibrationPersistenceUnsupported` behavior, not a new archive/import/resume
protocol. JSON is an explicit export. Existing single `/1` tables/getters,
failure outcomes and M5 raw input entrypoints stay untouched.

Happy path, assuming the original three imported immutable instruments, the
staged fitted model, explicit frozen snapshot and sampling already exist
(proposed C++, not compiled or runnable on the baseline):

```cpp
XccyMonteCarloBasketRequest_ request;
request.phase_ = XccyMonteCarloPhase_::Value_::HeldOut;
request.sampling_ = frozenHeldOutSampling; // 16 x 16384, seed 1024, stream /1
request.fixings_ = frozenFixings;          // nonnull snapshot; zero entries is valid
const auto prepared = PrepareXccyBasketMonteCarlo(
    {{"short", instruments[0], "Price"},
     {"middle", instruments[1], "Price"},
     {"long", instruments[2], "Price"}}, fittedModel, request);
const auto basket = prepared.Evaluate();
// Inspect each member estimate/SE and the owned scalar ledger/covariance.
```

Late-failure access uses the distinct carrier, without a complete result:

```cpp
Handle_<XccyMcBasketPartial_> partial;
try {
    const auto basket = prepared.Evaluate();
} catch (const XccyMcBasketError_& error) {
    partial = error.Partial(); // shared owner, independent of the error and plan
    // error.Failure() keeps the original diagnostic/report/validated model.
}
if (partial) {
    const auto json = DumpXccyMonteCarloBasketPartialJson(partial);
}
```

```python
try:
    basket = prepared.evaluate()
except XccyBasketCalibrationError as error:
    partial = error.partial
    # partial.details_json and partial.members survive the exception/plan.
    diagnostic, report, validated_model = error.diagnostic, error.report, error.candidate
```

Excel's same case is `outcome = XccyBasket_EvaluateMonteCarlo(prepared)`,
then `XccyMonteCarloBasketOutcome_Get_Partial(outcome)` and
`XccyMonteCarloBasketPartial_Get_Json(partial)`. The outcome remains Failed;
its result getter refuses. A preflight candidate geometry mismatch exposes a
zero-draw partial and null candidate, not the rejected model.

Rejection examples: passing the three old PreparedXccyMc_ reservations to
`RequireXccyMcIndependent` still fails `/sampling/mapping` before draws;
passing the same basket twice as Independent fails `/sampling/ranges`;
preparing with the last member's changed dates and an old allocation cannot
reuse the old ledger; omitted basket fixings fails `/context/fixings`.
No successful example claims numerical acceptance or a new untouched holdout.

## Executable RED-to-GREEN acceptance handoff

The rows below are required later execution, **not tests run in this design
PR**. Freeze exact source/dependency/configuration/executable and new layout
identities, independent geometry/oracles, unchanged counts/scales/thresholds
before observing new samples. Keep R2 files and failures byte-identical.

| Check | RED, then required GREEN / retained boundary |
|---|---|
| Real original staged basket | Preserve R2 exact D30/55/80 pre-draw refusal and old unprepared test. Through the new prepared basket actually evaluate the same three instruments and both fitted EXACT/APPROXIMATE immutable models, HeldOut 16x16384 and predeclared x2/x4 prefixes. Require finite complete coefficient rows and valid traces; report statistical gates separately, never require false flags to become true. |
| Scalar adapter | Against existing MRG32 stream, compare dimension-one seek/fill to direct serial NextUniform plus Precise inverse-normal for small non-D-aligned starts and endpoint algebra. At real large batch starts cross-check an independently reconstructed MRG32 skip/normal trace. First-member/singleton coordinates must match existing single `/1` exactly; later members must match their derived scalar positions. No RNG change. |
| Full trace | Assert every member source use/date/model slot, interval, five innovations, path ordinal, scalar coordinate and end cursor. Match all batch first-normal vectors and total consumption. An extra normal, missing fixing date, swapped slot or changed D must fail deterministic trace/identity checks even if prices agree statistically. |
| Fixed reservations/prefix/call order | Assert the nine planned reserved intervals above; selected Fit/Validation/HeldOut starts differ. Run phases and internal members in reversed order and repeat, with identical traces/results for a fixed input list. Verify x1 set is contained in x2/x4 and compare normals by absolute coordinate; changed batching is not another independent sample. |
| Range failures before draws | Instrument the draw count as zero for unknown/unset phase/stream, signed counts, capacity+1, size_t and uint64 product/addition limits, matrix index/storage limits, stale ledger/hash/D/equal-D date change, forged overlap/CRN and last-member preflight failures. On 32-bit builds apply the smaller real limit. Use synthetic checked arithmetic fixtures without allocating huge vectors. |
| Input/domain/degenerate | Empty list, duplicate ID, null model/instrument/snapshot, missing strict-past history, bad currency/route/coverage, intraday/unsupported product, invalid labels/rank/tape all refuse at the owning code/path/stage. Explicit empty snapshot with future instruments succeeds. Duplicate economic instruments with distinct IDs get separate blocks. D=0 and mixed deterministic/stochastic membership consume exactly zero for deterministic rows; all-D=0 independent claim and par refuse as specified. |
| Same-source economics | Independently recheck the R2 canonical cashflow/index/date table and original three basis knots. Same source supplies fast/MC A,B,Price,eligible par and selected raw basis partials through the full native chain. Preserve the third-node recovery limitation; allocator does not alter analytic fitting or canonical admission. |
| AAD/CRN/ownership | Interior raw basis/spot/sigma AAD and h/h2/h4 CRN use identical member scalar coordinates and full map, with batchwise differences. Unsupported methods/labels/rank stay unavailable. Mutate globals after preparation and between bumps; explicit snapshot and prepared results stay fixed. Drop input/plan handles and verify owning success and caller tape restoration. The explicit partial-failure cases below add no paths to the frozen numerical acceptance runs. |
| Late-member partial failure | Inject a controlled failure in member 1 after member 0 has completed all B batches, raw partials and summary checks. Catch the derived error, retain its payload/base report/candidate, unwind and destroy prepared plan, instrument/binding/context/model caller handles and caught temporary. Assert member 0's rows/qualified summary remain readable, every original identity/reservation remains exact, later states are incomplete/NotStarted, actual consumption matches draws, and the caller AAD tape is restored. No complete basket result or joint covariance/SE/acceptance is available. |
| Late-batch partial failure | In member 1, commit k batches (0<k<B), then fail a path/AAD operation after its vector was drawn in batch k. Assert exactly k full coefficient rows with every requested label, no row k or padded statistics, correct completed-path versus consumed-scalar counts including the failing vector, and optional retained-path commit behavior. Repeat a member-summary annuity failure after all its rows: state CoefficientsComplete, no qualified member estimate/gradient/SE. Hold payload through the same unwind/destruction check. Synthetic fault cases are ownership controls, not new numerical samples. |
| Partial carrier projections | Public catch/rethrow preserves dynamic type, original code/path/report and payload owner. Last-member geometry rejection has zero draws and null candidate; late numerical failure after full geometry retains only the actual validated model. Python catches the new subclass via both its name and CalibrationError, inspects `error.partial.details_json`, saves payload, drops exception/plan/inputs and runs GC; all rows remain readable with the GIL correctly restored. Excel obtains typed partial then releases outcome/prepared/input handles; partial JSON still reads, while complete-result getter refuses. Early Prepare failure has null partial and owning report; getter rejects it. Old single translator/outcome regression remains unchanged. |
| Cross-member statistics | Independently concatenate all B coefficient rows; reconstruct every element of full C_batch and all per-member Price/par/gradient SE. Prequalified wrong-SE and omitted cross-block/empty/ragged/truncated covariance mutants must fail; do not require measured independent cross-block entries to be exactly zero. Use B, never B*m, as batch count. |
| Numerical verdict and phase provenance | Original and new parity, precision, combined and gradient gates remain distinct under DAL-410. Preserve current nine original `numerics=false`, extended profile REDs and five binary64 FD RED rows. New results cannot convert those to passes; lack of precision retains complete estimates and fails acceptance. HeldOut remains disjoint from this allocation's Fit/Validation, never relabeled to pass the Validation-only product validator. |
| Facade/bindings after real M5 merge | Direct C++ core/public agreement, Python exact types/GIL/ownership/errors, Excel raw tables/typed handles/owning failure and range JSON. Native Windows/XLL/workbook claims require actual native execution; Linux portable checks are scoped as such. |

The old HeldOut has **already been observed**, including the earlier base long
runs and R2 refusal/diagnostics. The new disjoint layout is a newly identified
execution plan, not a way to make old data unobserved. Its first member even
reuses old coordinates. Do not advertise any of these runs as an untouched
terminal holdout or use them to tune then certify a final model. Any future
fresh holdout or changed budget needs explicit preregistration/authorization;
this design authorizes neither. No hidden paths, VR, fixture scaling, epsilon
or tolerance changes are part of the GREEN conditions.

## Implementation ownership, prerequisites and risks

These are disjoint task proposals for coordinator scheduling, not assignments.

| Work | Allowed paths/seams and real prerequisite |
|---|---|
| Core allocator/runner | New dedicated basket files under `dal-cpp/dal/script/` and new focused core tests, including immutable partial snapshots/derived error and batch commit/unwind paths. Starts after this design actually merges, from accepted master containing M1--M4. Narrow serial extraction from `xccymc.cpp`/`xccymc_internal.hpp` can share member preparation, coefficient execution and summary arithmetic; preserve old runner, public single types/versions and behavior. A separate basket range checker must not weaken `xccymcranges.cpp`'s old mixed guard. No protocol.hpp, M1/M2 grid/model/RNG change is required. |
| Facade and binding projection | After core allocator and M5 both actually merge/accept: new basket facade/binding files and direct tests, including partial dump, local Python exception/catch and Excel typed partial/failure wrappers/getters; only necessary registration/CMake/generated wrappers in a coordinator-approved window. Do not write PR632's current files concurrently or depend on its unmerged objects. Single request/result/dump/validator/error/outcome behavior remains frozen. |
| Independent V1 continuation | Original tester retains exclusive tests/helpers and validation record, after required product merges. Its existing P2 repairs and R2 focused review continue independently. Implementation tests use new basket-specific filenames; do not edit the tester's active helper/record. Coordinator arranges integration of the independent basket acceptance against the merged allocator. |
| Public docs/example/build | Only after the callable surface is accepted, and in an agreed window with M5/docs owner. A new facade example is a separate program under dal-public/examples. Old core examples never move or depend on public. No public guide/index/CHANGELOG edit in this design PR. |

Concretely, reserve proposed core files `xccymcbasket.hpp/.cpp`,
`xccymcbasketranges.cpp`, `xccymcbasketstatistics.cpp` and, if needed, a private
`xccymcbasket_internal.hpp` in the script directory. The direct call chain is
PreparedXccyMcBasket_ -> shared admitted-member/coefficient helpers from
xccymc.cpp -> existing PreparedXccyScript_/XccyScriptPathProvider_ -> existing
exact XccyGaussian_. Only the private scalar adapter calls New/SkipNormalTo/
FillNormal. The old PreparedXccyMc_ keeps calling its old range/runner path.
New focused implementation tests can be `test_xccy_mc_basket.cpp` and
`test_xccy_mc_basket_ranges.cpp` in the core script tests directory, separate
from the tester's acceptance files. Core sources/tests use existing CMake
CONFIGURE_DEPENDS globs; inspect reconfigure discovery before adding build edits.

Reserve proposed public files `dal-public/src/xccymontecarlobasket.hpp/.cpp`,
Python `dal-python/src/bindings/xccymontecarlobasket.cpp`, and Excel
`dal-excel/src/__xccymontecarlobasket.hpp/.cpp`. Public Prepare delegates only
to the new core; Python and Excel delegate only through that facade. Python
needs its explicit source list in dal-python/CMakeLists.txt and a narrow
registration call in `dal-python/src/bindings/calibrationinstrument.cpp`
after M5's init_xccy_monte_carlo; it needs no new exception translator or
module-wide dispatch. Register the new Python subclass/partial types and the
local Prepare/Evaluate catches in the **new basket binding file**; the shared
registration call only invokes that initializer, after CalibrationError and
M5's model types exist. The facade partial dump is in the new basket .cpp.
Core snapshot/error definitions and producers stay in the new basket files,
with no protocol.hpp or single OwnFailure edits.

Excel outcome/partial Storable wrappers, derived-first catch, all new getter
declarations/definitions and Machinist function blocks belong solely in
`__xccymontecarlobasket.hpp/.cpp`; no edit to `__xccymontecarlo.cpp`,
`__xccyinput.hpp`, the shared converter or M5 factories is required.
Public/Excel source globs already discover new files,
but Excel Machinist outputs in dal-excel/auto need generation/drift checks.
These named registration/build seams are coordinator-owned serial windows,
not permission to edit M5's ongoing tree. Direct tests belong in each owning
component; the independent tester's validation tools are consumers, not core
or public dependencies.

Minimum core implementation checks: new checked-layout/adapter/negative tests,
existing single XCCY range/MC/AAD boundary tests and their direct observation/
model callers, plus an exact fixed-seed single regression to protect unchanged
`/1`. Reuse qualified M1--M4 mathematics instead of rerunning unrelated model
matrices. Inspect includes and target closure for new files. If build/example
wiring is actually changed, require public-disabled workspace, standalone
core configure/build/install and affected core caller, separate public build/
example/CTest and installed consumer, then direct Python/Excel callers as
applicable. No-tests configurations are not passing targets. New shared-header
effects require direct caller analysis, not an automatic whole-repository run.

Measure only the new caller's prepare/evaluate and scalar-seek overhead at the
needed boundaries; keep any comparison matched by source/dependencies/config/
executable. There is no measured speedup or performance acceptance in this
design. The dimension-one adapter avoids extra RNG draws but needs explicit
trace protection if the RNG's vector-fill implementation ever changes.
Full covariance storage grows with members and labels, and 16-batch covariance
can be singular; checked storage limits and no inversion keep the scope bounded.
Disjoint inputs forfeit common-path cross-member covariance reduction; do not
compensate by increasing the frozen budgets.

## Compatibility and open questions

The new layout/result/partial/outcome families start at `/1` under **new names**
because their coordinate meaning and member ownership are new. Existing
`dal.xccy-gaussian-phase-ranges/1`, `dal.xccy-gaussian-mc-result/1`, stream
`dal.mrg32-normal-paths/1`, exact transition `/1`, seeds, Precise normals,
single-instrument results/defaults, report columns, archive and generic
dispatch are unchanged. Unknown families fail closed; no implicit upgrade or
cross-family coercion. Any later common-path union mode needs its own explicit
layout/compatibility decision, not reinterpretation of this disjoint layout.

No unresolved financial/model choice is needed to implement the selected
allocator. Two execution questions remain for coordinator/reviewer closure:

- Confirm the serial extraction and facade registration windows against the
  actual accepted M5 head when scheduling; this note cannot grant ownership of
  another active checkout. This affects wiring, not the allocation law.
- Frozen precision/combined and production binary64 FD qualification remain
  genuinely unresolved V1 failures. The allocator permits the missing basket
  execution but does not predict their closure. Any budget/estimator/fresh-
  holdout proposal must be decided separately under the existing authority.

Design validation is limited to source/caller inspection, attachment hash and
R2 capture checks, exact-integer coordinate/overlap/inverse/prefix reasoning
and repository documentation/static checks. No product build, MC matrix,
Windows/XLL run or numerical GREEN is claimed here.

Actual design checks on the baseline plus this note: `python3
.github/scripts/check_docs.py` passed for 111 Markdown files; `git diff
--check` passed. An integer-only audit checked all nine table intervals,
inverse interval/slot coordinates, disjoint full reservations, x1/x2/x4
prefix/capacity laws, total consumption and synthetic 32/64-bit arithmetic
limits. The audit also verified the control SHA and both R2 staged modes'
full grids/slot strings/mapping hashes and actual no-draw refusal. This is
static design/captured-evidence validation, not execution of the proposed
adapter or basket. No product executable was built or run.

The partial-carrier revision adds only the owned snapshot/error, local
three-end access paths, failure examples and directly affected handoff/scope
above. The coordinate law, fixed reservations and statistical formulas are
unchanged from `2905af5d4239e9b90bac6bf7d59c9132938c985a`. Reviewer report
`01a169f8-b1ff-78e9-834c-b3089f8702cd` supplies the source/API limitation;
its 2,565 existing-RNG comparisons are reused within their stated scope, not
rerun or counted as basket execution. Critic conclusion
`01a169fb-5269-7260-a16c-74bea04a7cce` found no coordinate/statistics blocker
at that original head; it does not approve the new carrier or replace its
necessary limited review. Revision checks are documentation, exact diff/scope
and static ownership/API consistency only. Product implementation, lifetime
fault controls and native Python/Excel execution remain required future GREEN
evidence; this design supplies none of those runtime results.
