# Saved-output Gaussian error certificate

This private checkpoint certifies deterministic errors of 36 already observed
Price/par report outputs: EXACT/APPROXIMATE, x1/x2/x4, and D30/55/80. Cheng Li
selected this scope in comment `01a17228-8906-71f8-b18f-a2e9ef1cb683`.
The executable is `dal-cpp/tests/script/xccy_acceptance_fixed_output.py`;
the complete exact certificate is
`.codex/artifacts/validation/dal425-fixed-output-certificate.json`.
Every report row is **strictly RED** against its original MC quarter budget.
The paired 18 direct capture Price payloads are also strictly RED.

## Method, timing and cost

The method/input/cost plan froze at **2026-10-25 08:46:22.223783 UTC**,
after the earlier 08:28:35 point-discrepancy report. The lineage clarification
preceded the first certificate calculation. Final helper/test source froze
at **08:53:07.969505 UTC**; certificate generation began at 08:53:08.061036
and ended at 08:53:08.171326 UTC, exit 0, 0.109523 CPU seconds.
These are post-observation controls, without a heldout or blind claim.

The finite plan permits six coefficient-reference pairs, two measures,
36 report rows, no subdivision or transcendental reevaluation, a total
1800 CPU-second ceiling and 120-second individual command ceilings.
It uses exact rational arithmetic, with bounded imported dyadic exponents.
No scalar/libm/normal, D1/D2, long MC, old gradient/statistical, native,
install, platform or performance matrix is executed. New helper paths have
no production or performance callers; production benchmark selection is empty.

## Inherited strict Gaussian reference

The only mathematical reference input is the accepted PR643 certificate
`acceptance-bias/certificate-final.json`, SHA256
`2f6666deed5b704edfc37014f99a8ac06e7bb537edd4bbc12754f480cc59ef49`.
Its independent cashflow derivation and directed interval Gaussian centers
were covered by the original D1/D2/D3 review and exact replay. The dedicated
frozen fixture retains SHA256
`5c2f3d48861e173185219bd32fad7cbf49a6c9e92918586b9a7bc0bd8108837a`.
All five relevant inherited Python source hashes are checked before use.
The new certificate records all 22 input/source hashes.

For each dated independent cashflow term c exp(k+ell.Z), the Gaussian model
target has independent standard normal coordinates, so its expectation is
c exp(k+sum(ell_j^2)/2). The accepted Brownian/dual-Cholesky construction,
shared coordinates, signs, spot coefficient, coupon/principal/MTM flows,
domestic and foreign discount/projection/basis curves, calendar grids and
model parameters supply these c,k,ell. Captured production prices or terms
do not supply the expected value. Summing the signed terms gave the strict
236-bit outward intervals for a=E[A] and b=E[B] reused here.

The Gaussian expectation part does not depend on the conditional IID-grid
theorem about approximate normals. That theorem's error boxes, finite-state
law, scalar accuracy bound, probability experiment and sampling SEs are not
used here. Gaussian model coordinate independence defines the target; no
independence claim is made about the actual fixed MRG sequence.

The inherited trusted basis is the independently reviewed dated-cashflow and
Brownian algebra plus mpmath 1.3.0/libmp directed arithmetic. This checkpoint
checks identities and consumes its Gaussian endpoints; it does not reprove
that old mathematics or present an ordinary high-precision point as a bound.
Neither the 65/85-digit numerical oracle nor its refinement indicator enters
the new reference interval. The old numeric oracle is read only to check
capture/context identity.

## Exact propagation and comparison

Each finite libmp endpoint [sign,mantissa,exponent,bitcount] is decoded as the
exact Fraction (-1)^sign mantissa 2^exponent. Endpoint order, finite canonical
encoding and outward decimal display are checked. Binary64 model inputs and
the coupon enter under the accepted exact-dyadic convention; specifically
K=Fraction.from_float(.001), with the original passive binary64 100/110
already retained by the inherited coefficient derivation.

For interval a=[aL,aU] and b=[bL,bU], form

```text
Price = b + K a
D = -a, with D.lower > 0
par = b / D = -E[B]/E[A].
```

Addition and multiplication use exact rational endpoint/corner operations;
positive division multiplies by [1/D.upper,1/D.lower]. Repeated-coordinate
dependencies can widen these intervals but cannot make them inward. All six
Gaussian annuity lower bounds exceed 54.7014/108.6859/162.7988 for D30/55/80.
The widest resulting Price/par reference interval is less than 3.34e-66.
These are ratios of coefficient expectations, never E[pathwise par].

For an exact saved binary64 dyadic y and reference enclosure [L,U],

```text
signed error = [y-U, y-L]
absolute lower = 0 if L <= y <= U, else min(|y-U|,|y-L|)
absolute upper = max(|y-U|,|y-L|).
```

A strict lower bound greater than the original budget proves RED. An upper
bound at most the budget proves only this fixed-payload comparison within
budget. Otherwise the result is inconclusive, including a lower bound equal
to budget with an upper bound above it. No loose upper bound proves RED.
Numerator/denominator pairs are authoritative; all decimal interval displays
are exact rational floors/ceilings to 35 decimal places.

Price uses original s_measure=1/100 USD and quarter budget=1/400 USD;
par uses 1/10000 and 1/40000 decimal spread. All 36 strict error lower bounds
exceed even the corresponding full s_measure. No new acceptance tolerance or
replacement combined-error formula is introduced.

## Saved payload lineage

The original report retains SHA256
`cca7aac898607d6b2eef7f5d477e259cbff7fd1f167399cb55da35bddbc51327`;
the old numerical oracle retains SHA256
`ef53e98e1dae547439f0bfc983974256f8b35b665be730e834ef0b763897ba5c`.
All six raw captures and six successful capture-command records have fixed
SHA256 admissions. Each member binds source/instrument/mapping/model plus
binding/context, complete dated coordinates, D, seed1024/Precise/HeldOut,
B=16 and n=16384*factor per batch. Price/par report means are recomposed
only from their original paired batch B/A columns and must match exactly as
binary64. This is a narrow value-lineage check, not an old-matrix rerun.

Each certificate row gives the raw capture file/member pointer, saved report
JSON pointer, original numeric token, float.hex representation and exact
dyadic, Gaussian A/B source pointer, positive annuity, reference interval,
signed/absolute errors, units, budgets, original flags, SE and eFast tokens.
The separate CSV presents all 36 rows for inspection.

Ten of 18 directly captured Price values differ from the recomposed report
Price by a few ulps, at most 2^-49 USD. They are not called identical. Each
Price row separately retains and encloses both exact payloads. All 18 direct
Price payloads have strict lower bounds above budget as well. The raw capture
does not contain a separate par output field: the certified par is the saved
report's binary64 ratio of paired coefficient means, with raw A/B tokens
retained as lineage. No pathwise-par average is substituted.

For EXACT x4 the following decimal numbers are conservative strict lower
bounds, rounded downward to 17 decimal places:

| D  | Measure | Absolute error lower | Quarter budget |
|----|---------|----------------------|----------------|
| 30 | Price   | 0.03243897443431575  | 0.0025         |
| 55 | Price   | 0.01193358847320429  | 0.0025         |
| 80 | Price   | 0.01872984519526048  | 0.0025         |
| 30 | par     | 0.00058579712171146  | 0.000025       |
| 55 | par     | 0.00010979155290944  | 0.000025       |
| 80 | par     | 0.00011187232168672  | 0.000025       |

The certificate contains every mode/factor/member, including signed errors;
these six examples do not stand in for the full 36-row result.

## New controls and reproducibility

Thirteen new unit tests cover independent exact constant-Gaussian positive
examples, unequal-annuity mean quotients, uncertain coefficient corners,
strict lower-bound RED versus inconclusive upper bounds, budget equality,
signed/zero-crossing errors, exact binary64 conversion, outward display,
invalid/nonpositive denominators, point/encoding refusals, wrong units,
source digest admission and missing/duplicate/extra rows. A focused initial
behavior RED was an assertion failure in strict RED classification; adding
the strict lower-bound branch closes it. The earlier absent-module loader
error is retained separately and is not counted as that behavior RED.

Ten direct controls on copies of the actual inputs reject wrong reference
source/model, wrong capture binding, missing member/batch, changed saved
payload, nonpositive Gaussian annuity, inward decimal endpoints, an ordinary
point in place of an enclosure, and wrong measure. Each reaches its intended
ValueError; there are no harness errors. All original files remain untouched.
Ruff/format and Lizard CCN<=8 cover only the two new Python files.

```text
python -m unittest discover -s dal-cpp/tests/script -p test_xccy_acceptance_fixed_output.py -v
python dal-cpp/tests/script/xccy_acceptance_fixed_output.py .codex/artifacts/validation/dal425-fixed-output-certificate.json
python dal-cpp/tests/script/xccy_acceptance_fixed_output.py .codex/artifacts/validation/dal425-fixed-output-certificate.json --verify
python acceptance-fixed-output/controls.py
```

The evidence package retains actual commands/exits, freeze records, complete
input certificate/captures, source copies, exact CSV and isolated replay.
New runtime identities include Python executable, Fraction/standard-library
dependencies and loaded libraries. Inherited financial execution has its own
source/config/dependency/binary manifests. The retained original capture
executable still matches SHA256
`b4151ec3502b0e3426a77b74e3c2540094fb17430f95b53022cba22b0ade6aed`.
The current driver and CMake cache differ from the original frozen versions
after intervening accepted work; they are not passed off as the old build
or rerun. The original accepted build provenance remains the input identity.

## Accepted documentation integration and exact source admission

PR647 merges accepted documentation master
`1a295ead5b53810cdb89e665e5fdf1f2be0d7b76` into original proof head
`bcdf21eddb86b40ebd8677f2de73c0bbeb41b19c`. Merge commit
`4c7a7162d5a8f9f7a84887ab596fcd0867d4b9ac` has those two parents,
in that order. Its 11 documentation paths are inherited upstream changes;
the proof does not restore retired records or modify those documents.

The original certificate used C410's frozen 67,450 bytes, SHA256
`c2ff68e1a696b620682b896ca9b292ec6d577bf92cadb3597991b427267733d5`.
Accepted master's C410 has 67,796 bytes, SHA256
`6f1231c60cd1667601a84c1bf1d5c00b855a3bae81b9a88e0d115fc637e6e1f4`.
Its status header now records the accepted decision and current capability;
the complete body from `## 1. Audience` onward is byte-identical. The model,
formulas, parameters, numerical budgets and acceptance requirements remain
the same. C398's inherited header update is outside this helper's input set.

The helper registers exactly these two full-file C410 hashes at its exact
relative path. No arbitrary header, body change or basename is accepted;
all other 21 frozen input/source admissions retain their original hashes.
`input_sha256` continues to identify the original frozen dependencies.
The added `model_decision_admission` object records the actual root's
`admitted_sha256`, the original `frozen_sha256`, the exact compatible status
hash and its accepted master commit. It does not claim a historical execution
used the new document. A different admitted document produces different
identity metadata, so a certificate for one root cannot silently verify
against the other. No mathematical certificate field changes.

Integration source froze at **2026-10-25 09:32:00.372462 UTC**, after the
original post-observation proof. New generation at 09:32:00.404430 UTC
finished at 09:32:00.567766 UTC, exit 0, 0.162484 CPU seconds. This is a new
source/metadata replay, without a new timing, holdout or mathematical-review
claim. The integration package retains original bcdf source/certificate,
both C410 files, the original execution evidence, and new command/runtime
identities separately. The old evidence package remains unchanged.

From the designated safe checkout, with its retained input directories:

```text
python -B dal-cpp/tests/script/xccy_acceptance_fixed_output.py .codex/artifacts/validation/dal425-fixed-output-certificate.json --verify
python -B acceptance-fixed-output-integration/check_admission.py
python -B dal-cpp/tests/script/xccy_acceptance_fixed_output.py acceptance-fixed-output-integration/frozen-root-certificate.json --root acceptance-fixed-output/isolated --verify
```

The first command verifies the new accepted-header root and new certificate.
The second checks the actual old/new input roots, all original certificate
fields, and six real-input refusals, then writes a new metadata certificate
for the untouched frozen input root. The third verifies that frozen-root
certificate using the new helper. For the unchanged original replay, use the
original helper, certificate and input root together in
`acceptance-fixed-output/isolated`; do not mix its certificate with new
source metadata. The small integration ZIP includes all current admitted
inputs, original bcdf source/certificate and the frozen C410 for independent
replay. A Git-only checkout lacks the historical captures and accepted
Gaussian endpoint certificate; `--root` must name an extracted evidence root.

Two additional focused tests exercise accepted-header admission and rejection
of unregistered header/body edits; all 15 tests pass. The focused initial
test fails through the original source-identity ValueError before the fix.
Six complete-loader controls admit neither altered header/body/empty model,
the registered model bytes at another source path, another changed source,
nor a changed frozen fixture. All 36 complete row structures and every other
original certificate field are exactly equal to bcdf; only the new admission
object is added. Existing 36/36 report and 18/18 direct Price REDs remain.
The new verification interpreter/modules/libraries are captured separately
from the five unchanged inherited financial identity manifests. No old
financial certificate, product, MC, build, scalar/libm or native matrix is run.

## Scope of the conclusion

This proves errors of the identified saved payloads relative to the existing
Gaussian target. Their sampling discrepancy is part of the deterministic
difference; it is not relabelled bias-only. The result requires no new
seed/state probability experiment, SE interpretation or historical fenv
reconstruction. It does not prove errors of the exact real-arithmetic
algorithm, historical floating pipeline or future requests. Original per-call
fenv/MXCSR/FTZDAZ/GOT, caller binding, full eFD, five FD hard-floor REDs,
25 unsampled axes and all other missing global qualifications remain open.

The original 18+six overall failures and 288/216/214 statistical counts remain;
3SE, eFast/eFD, precision/combined and their original formulas remain intact.
All four global flags are false. This limited RED certificate supplies concrete
reviewable facts for the coordinator's next decision. It does not declare all
technical routes exhausted, authorize more paths/VR/pooling or altered budgets,
change RNG/model/fixtures, or qualify numerical V1/quote-risk/P4.
