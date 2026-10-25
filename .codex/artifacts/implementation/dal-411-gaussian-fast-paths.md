# DAL-411: Gaussian Caplet/Swaption script fast paths

Code delivered in original PR #616, branch `agent/dal-implementer/3c688a76ade6`,
base `master`. Focused code review approved head
`cd78a8b39bf18dc171df4983f2d916fead997495`; shared documentation is now integrated.
Necessary documentation acceptance and current-head CI remain coordinator gates.
The final handoff and attached source manifest identify the exact PR head.
This bounded template extension does not deliver DAL-412's MC calibration.

## Authority, baseline and ownership

Input head `f4e00a8f9e77339fb3895892ecc62b5bbf4b8e83` contained the prerequisite
record on merged DAL-404 `1f6eaa32424d0c71efb148cfb953f1634ff209cf`.
Research source `0f1feabee193d0976ab7a5336c9e9e29cb2aa28d` is historical.
Controlling inputs read: research attachment `01a15c20-3723-7ddd-8ef1-c0ac706cdc16`
§§3.2–3.4, 5, 7.3, 8, 10–11 and critique;
[DAL-398 API](../api-notes/dal-398-script-contract.md);
[methodology](../../../docs/methodology/script_calibration_contract.md);
DAL-399–403 implementation records;
[P0 acceptance](../validation/dal-404-p0-same-source-validation.md);
implementer/code/test/component/publication contracts; relevant source/tests.

Coordinator comment `01a16117-a0f2-7340-8e50-8e50b24a9e0b` resumes full delivery
and grants the serial writes in RateStrike decision
`01a16115-f296-74de-a024-969bad9885f2`. Changed shared files are limited to the
role markup/three generated outputs, instrument validator and dedicated tests.
Preparation extension, new model files/tests and this record are original
DAL-411 scope. No DAL-405 curve/provider/optimizer, build/registration, example,
public facade, shared compiler or MC/statistics implementation is changed.
Shared documentation/index writes below use the later serial release.

## Shipped behavior and bounded semantics

- RateStrike is appended after Accrual, preserving old ordinals. K/Real/Rate
  is bindable with finite signed defaults/rebinds, zero/negative rates, existing
  bounds and provenance. Exact archive token `RateStrike`, Java `RATESTRIKE`;
  portable `/1` and DAL `_v1` stay unchanged. Strike stays positive BondPrice;
  old Strike/Rate failure assertions are untouched.
- `dal/model/gsrrateoption.hpp` adds `NewCapletScriptInstrument`,
  `NewSwaptionScriptInstrument`, `PriceGaussianRateOption` and an import-settings
  alias reusing BondOption settings (explicit positive N). Native input is
  consumed once to emit authoritative source, never retained beside it.
- Complete matching in `dal/script/calibrationrateoption_internal.*` checks the
  exact N/K declarations, full hard-MAX expression, positive literal accruals,
  every index/date and entire cashflow. Descriptors are derived only from AST
  and effective binding. Extra arithmetic, payments, parameters, conditions,
  EXERCISE and ON settlement do not acquire a fast capability from a subterm.
- FastGaussian calls unchanged native double Black/quadrature kernels from
  reconstructed descriptors. Its derivative capability is FiniteDifference,
  not fast AAD. New Evaluate numerical failures retain cause, instrument,
  measure and owned report; the native legacy exception protocol is unchanged.
- Existing generic Price MC executes the original bound program with opt-in.
  Native effective-rank admission is reused during preparation (one native
  price call, included in preparation cost). Hard AAD conservatively requires
  finite nonzero rate exposure and positive native expectation; zero volatility,
  zero exposure and identically zero payoffs are refused.
- New rules `dal.gaussian-caplet/1`, `dal.gaussian-swaption/1` use compiler
  components `rate-option-cashflows/1` and `rate-option-ast/1`. P0 tokens stay
  byte-identical. Actual Libor day basis enters only the new context identity.
  Convention, binding, source, curve/fixings, model and smoothing changes
  invalidate plans, including MC plans via existing RequireCompatible.

Caplet pays at P and explicitly observes fixing E, concrete start S, nominal
3M/6M maturity T and literal coupon accrual. Amount is
`N*(alpha*MAX(L(E;S,T)-K,0))`, reversed for floor. Import requires exact native
end/index accrual agreement with GSR's nominal-maturity/Libor day-basis mapping.
E<=S<T<=P; delayed payment P>T uses the native payment-measure kernel.

Swaption at E pays `N*MAX(floatingPV-K*annuity,0)`, reversed for receiver.
Floating terms are literal coupon accrual * exercise-date Libor(concrete start)
* DF(end); fixed terms are literal accrual * DF(pay). Supported native inputs
have fixing>=E, fixing<=start, floating payment=end, canonical 3M/6M LCH indices,
physical settlement and one discount/forecast curve. Payment=end makes native
future-fixing convexity exactly zero, proving this conditional forward PV
mapping. Original future fix dates are not retained as redundant authority.
Floating payment lag, projection curves, cash settlement and nonmatching
conventions are refused. SLV, smoothing, conditional/early exercise and >3
effective Swaption directions are refused; rank-one four-factor pricing passes.

## Frozen acceptance and independent oracles

V=2026-10-21, E=2027-10-21, flat continuous r=.03, N=100, K=.03, H=1.
Caplet S=E, T=2028-01-21, ACT/360 index/coupon alpha=92/360, P=2028-02-04.
Swaption floating 6M endpoints: 2028-04-21, 2028-10-21, 2029-04-21, 2029-10-21,
payment=end, ACT/360. Fixed dates: 2028-10-21, 2029-10-21; ACT/365 alpha=366/365,1.
Both directions use g=.012,.016,.02,.0201,.0199,.02005,.01995 (XML ordinals 0–6).

Before statistical execution, the import guard rejected the initially proposed
weekend-adjusted fixture: GSR's Date::NominalMaturity adds nominal months without
business-day adjustment. The fixture was corrected to the supported convention;
mismatched end/accrual refusals remain. No threshold or scale changed.

Independent Caplet mathematics uses payment-measure forward
`F=exp(r*tau-g*g*tau*lag)`, sigma=g*tau, bond strike=1+indexAlpha*K and normal
CDF truncated moments, multiplied by N*DF(P)*couponAlpha/indexAlpha. Analytic g
partial includes dF/dg and dSigma/dg; it calls no matcher/native Black routine.
Independent Swaption mathematics telescopes original floating coupons to
1-DF(last), forms exponential E-forward bonds and annuity, and integrates
truncated normal moments after monotone-root bisection. Boundary differentiation
cancels at zero payoff, leaving analytic g partials. A separate two-factor
Sobol cashflow integration checks quadrature without calling the lowerer.

Native/fast/oracle Price tolerance=1e-10, fine FD/oracle gradient tolerance=2e-4.
FD h=1e-4/5e-5, pathwise CRN FD h=1e-5/5e-6. MRG32 seed=1024, Precise normals,
fit=8*8192, validation=16*16384, total maximum=327680 paths/view, disjoint ranges.
Frozen sigma=3, nu=nuG=.25, Price scale=.1, parameter scale=.01,
epsilonV=1e-10, epsilonG=1e-8. No post-observation budget enlargement.

All 28 oracle–fast, fast–MC and oracle–MC value/gradient rows pass shared
ValidateCalibrationMc. XML retains definition/binding, per-row instrument,
context/plan/mapping identities and all 448 validation batch values, gradients
and ranges. Selected reference-g rows:

| Template/direction | Fast Price  | MC mean     | MC SE      | Fine FD g    | MC g         | MC g SE    |
|--------------------|-------------|-------------|------------|--------------|--------------|------------|
| Caplet/call        | .191113088  | .190274160  | .000560919 | 9.727682133  | 9.686467826  | .028008538 |
| Caplet/put         | .198834117  | .199505238  | .000636912 | 9.765167336  | 9.798640007  | .031840825 |
| Swaption/payer     | 1.570004045 | 1.563347747 | .003180410 | 76.338310647 | 76.021881984 | .153182561 |
| Swaption/receiver  | 1.485427264 | 1.486243366 | .005255050 | 76.338310647 | 76.378944812 | .276963961 |

Reference-g FD refinements are about 2.1e-8 (Caplet), 4.45e-7 (Swaption), separate
from MC gradient SE. Two-factor order16/32 prices=1.558110315578993/1.558110315578999,
refinement indicator=6.06e-15. Independent 131072-point Sobol=1.558050043485312,
within frozen 2e-4; it is not an MC SE estimate.

Native fast fits from g=.012 recover reference g=.02; same-binding independent
MC validation passes. An extra MC-fit-versus-validation comparison correctly
rejects Swaption's frozen fit budget: fit SE=.0107029603, validation
SE=.00318041035, hence 3*hypot(SEFit,SEVal)>.025. Price/gradient parity passes;
precision/error fail. The initial extra test wrongly assumed this budget must
pass; the corrected regression explicitly retains refusal, unchanged scales
and counts. It is not an accepted MC calibration or a passed validation pair.
Tiny-budget regressions separately return budgetExhausted.

## RED, GREEN and verification

- Corrected RateStrike test against isolated input protocol/validator plus old
  role headers fails unknown RateStrike; current generated objects pass. The
  first test setup used private Vector resize; that compile error was corrected
  and is not behavioral RED.
- Initial Caplet source fails original preparation with UnsupportedFastEvaluator,
  then passes new FastGaussian admission. Isolated input-head preparation
  replays both Caplet/Swaption witnesses and rejects both; current code accepts
  both. The replay uses the current extended variant layout, not a falsely
  claimed untouched old executable.
- Identically zero rate-MAX regression initially fails because hard AAD admits
  it despite nonzero earlier bond exposure. Requiring positive expectation
  closes that real gap; the unchanged regression passes.
- Final focused executable: 150/150 pass, including 23 new tests (16 rate-option,
  6 RateStrike, 1 BondPrice isolation). It retains 23 BondOption script, 17 native
  Gaussian/calibration and 20 MC protocol tests plus original boundary assertions.
  P0's unrelated statistical matrix is reused from accepted DAL-404.
- Input/current P0 probes compare definition, binding, instrument, compiler,
  context, fast plan, MC plan/mapping and quote fingerprints: byte-identical
  Deposit and BondOption rows. JSON/SHA quote-risk implementation is unchanged.
- Scoped GCC15 Wall/Wextra/Wpedantic/Werror syntax checks pass for affected
  sources with existing CI exclusions. Changed handwritten functions have
  CCN<=8. Machinist regeneration changes no further content; three output
  hashes remain stable. Actual Windows status belongs to PR CI.

Core build: `cmake --build build/Release-linux --target dal_cpp -j 10`.
Focused link: test_main; rateoption/bondoption/european model tests;
instrument, calibration_preparation and calibration_mc script tests. Filter is
their nine suites, excluding GSRBondOptionNumericalTest's accepted old matrix.
Full commands and RED/GREEN/XML outputs are attached.

Configuration: GCC15.2, Release/O3/C++17, static core, Eigen ON,
`-fPIC -ffp-contract=fast`, native architecture OFF, native AAD, lifetime/profiling
OFF. Public and benchmarks configured ON, core/public examples OFF, Python OFF,
portable Excel tests configured ON but not run. This differs from the earlier
prerequisite worker's public-disabled build; its binary is not this executable.
Dependency pointers are unchanged. No full matrix, example, standalone,
public-disabled or frontend product result is claimed; those components did
not change. Source/binary manifests, public compatibility, scoped performance
and static checks are attached; pushed-head CI is reported in the handoff.

## Compatibility and measured costs

Rebuilt public compatibility filter `*Calibration*:*Script*`: **114/114 pass**.
Documentation static check: **85 Markdown files pass**. Formatting dry-run,
CCN<=8, patch whitespace and stable regeneration checks pass. These are scoped
checks, not a full frontend/example or platform matrix.

After local builds finished, measurements ran serially on one allowed CPU
(affinity recorded in evidence). New APIs use one warmup and ten rounds;
unchanged native APIs use three alternating input/current runs, each with
one warmup and ten rounds, 1000 calls/case. The input native object is compiled
from exact input-head gsreuropean.cpp using the current CMake compile command.
Core native sources and price results are unchanged. Median current/input
ratios: BondOption 1.023, Caplet 1.047, Swaption .988. These small local timing
samples do not establish a portable performance bound.

Informational new API medians (microseconds/call): Caplet native=1.274,
prepare=98.036, cached Price=46.441; Swaption native=8.202, prepare=131.766,
cached Price=57.453. Cached Price includes immutable context compatibility
hashing; its overhead versus direct native pricing is visible and not called
a speedup. Preparation includes parsing/matching plus native rank admission.
Representative compiled MC/AAD validation costs 165.103ms/268.698ms for
Caplet/Swaption, each 262144 paths and 16 engine runs. Peak tape nodes=155/204,
reported allocated tape bytes=1966080 each. Timing is separate from precision;
path counts and frozen numerical scales were not reduced for performance.

Final SHA-256 identities:

- Focused executable: `b820ce6faa72a87684c82efa8008b658aab9abc128999f4caa774752d191c60e`.
- Core archive: `fc67c81a67b2a4673d7b94c012c36e55a544547e682d86bcef9a701719e80ac8`.
- Performance executable: `1563bacc75b68a85f8142c61025780f730f46dba0fcfbece2fd3158b6381af74`.
- Public executable: `64746bbd0fa90cfd67ad404727535208e68938ee7f60b96f5a25d8a55653f4c6`.

## Serial documentation integration

Coordinator release `01a16155-7e5c-79bf-9988-a5b2e37a32f8` transfers the shared
writing window after DAL-405's batch, without waiting for its CI or merge.
DAL-405 completed at `e60a166b3fe6d749fc4209c00ef7edfdc0b781b5` in PR #617;
none of its unmerged product commits is imported here. Merged master remains
`1f6eaa32424d0c71efb148cfb953f1634ff209cf`; the shared guide, CHANGELOG and
artifact index matched that baseline before this integration.

- The existing [contract guide](../../../docs/methodology/script_calibration_contract.md#gaussian-caplet-and-swaption-price)
  now describes all three core entry points, signed RateStrike versus positive
  BondPrice Strike, nominal maturity/day basis, Caplet payment measure,
  physical Swaption payment=end, effective rank and qualified MC/AAD admission.
  The BondOption fitting and Deposit sections retain their original scope.
- CHANGELOG links the bounded Price capability; the artifact index links this
  active evidence record. No new guide is introduced. Existing docs/README,
  methodology/README, script-engine and CLAUDE guide links remain valid and their
  data-contract descriptions remain accurate; no extra index entry is needed.
- All changes in this batch are Markdown. Static documentation/link and patch
  checks pass. All 2740 tracked non-Markdown entries and the four product trees
  remain identical to the approved code head. The attached before/after manifest
  and exact Markdown patch are the applicability evidence.

The reviewer accepted all 28 numerical states and independently recomputed 448
validation batches in parent comment `01a16152-84e3-70ab-8821-baa3d69970c1`.
That code Approve and the 150 core/114 public, frozen numerical and scoped
performance evidence remain applicable. No product or performance test is rerun
for this Markdown-only batch. The extra Swaption MC-fit precision refusal above
remains a refusal. Current-head CI and necessary documentation review are still
pending acceptance; no downstream delivery or merge is claimed.

## Master integration after DAL-405 acceptance

Coordinator trigger `01a1619e-b19f-7c36-9189-41ae46cced0b` requests the serial
integration after DAL-405 / PR #617 merged, retaining shared documentation
ownership on DAL-411. The accepted incoming PR #616 head is
`b12b0a8fc6dd57fb9468c912a04700f2e350186c`; its 36 successful effective checks,
code Approve and documentation acceptance are prior-head evidence, not checks
for this integration.

The new integrated head is `7fc6192137755d8c4e339558bae266521f7d4554`, with
ordered parents `b12b0a8fc6dd57fb9468c912a04700f2e350186c` and merged master
`072c7da1b3071e71dac4f901451c2345957bd42a`. Their merge base is
`1f6eaa32424d0c71efb148cfb953f1634ff209cf`. This appended record is delivered
as a subsequent Markdown-only commit; its exact published head is recorded in
the issue handoff and attached final manifest. Both commits stay on the same
branch and PR. No DAL-406 unmerged commit is imported.

Before editing, `git merge-tree --write-tree --name-only` reproduced exactly
the two reported conflicts. Its expected automatic tree is
`543e4b1a5b90ac45227fe7bba5aefc9caf7b206f`; the resolved merge tree is
`b24d0de9591d847d724d07a4dc97295482495b05`. The only differences between those
trees are `CHANGELOG.md` and `.codex/artifacts/README.md`: conflict markers are
replaced by the complete 411 entry followed by the complete 405 entry, with a
blank line between them. A line-preservation assertion verifies that both
parents' entire versions of each document remain in order, without wording
changes. The existing date/section and all other entries are retained.

`docs/methodology/script_calibration_contract.md` is exactly Git's automatic
merge blob `aa202cc158276222c62ed69257ea5d6efec66814`. It retains every line
from accepted 411 and the complete new Native curve instrument calibration
section from merged 405. No manual guide edit is made. The remaining 405
activity record and underdetermined-search documentation match master.

All **2744 tracked non-Markdown entries**, including modes and object IDs,
match the automatic merge. The two parents' changed non-Markdown paths are
disjoint, and each changed path retains the exact object from its owning
parent. The four resolved product tree IDs also match the automatic result:

- `dal-cpp`: `89f86737cef6bc3dea1deea672e981bbdc375205`.
- `dal-public`: `9f30dbe5825eb2a8202cbf4480c8695a72a56481`.
- `dal-python`: `39bc8427f0e4a7b566c3dc164ecc6d40694e89f3`.
- `dal-excel`: `7fb7da8118e1da7ee44a067786af4b035a433bc9`.

The core tree combines accepted 411 `af0320b90027a91cbdf30525a62b340fa1b6d083`
with accepted master `23b55b675fa67d40e976065967b98c2acb6768b6`; it is not
claimed identical to either parent. Master adds the accepted native curve
provider/driver and additive Underdetermined::Fit code. Gaussian fitting still
uses the unchanged BoundedGaussNewton implementation. GCC15 `-MM` preprocessing
of eight relevant source/test roots verifies that none of the master's changed
non-Markdown files enters those Gaussian/RateStrike/preparation dependencies.
The attachment retains those full dependency lists and the preservation audit.
No extra product edit, dependency pin, build setting, example or generated
output change is introduced by the conflict resolution.

Static verification passes for **86 Markdown files**, including local links
and anchors, and `git diff --cached --check` passes with no unresolved paths.
The manual-resolution patch and this record-only follow-up are separately
audited. The original code Approve, documentation acceptance, 150 core/114
public checks, 28 numerical states/448 batches and scoped performance evidence
remain applicable to their unchanged paths; the merged 405 code retains its
accepted master provenance. Product and performance matrices are not rerun for
this mechanical integration. No fresh integrated product binary is built or
claimed: the earlier executable hashes identify only those earlier builds.
Exact new-head CI is reported separately in the final issue handoff and is a
coordinator merge gate. The author does not merge, dispatch reviewers or start
DAL-412; no acceptance threshold or prior precision refusal changes.
