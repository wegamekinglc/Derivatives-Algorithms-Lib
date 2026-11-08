# DAL-502 bounded BBS initialization repair

This batch diagnoses and repairs the three saved BBS initialization failures.
It does not restart the stopped pilot or qualify BBS as mandatory TREE.
N3/N6/N7 remain INCOMPLETE and S4 remains paused.

Baseline: `e0e4c275764ff698da68ea14736338eee8ae53a9`. The bounded follow-up
decision `01a1b92b-3d68-7e7e-bfd4-db090418b92d` was committed in
`61f92f52c955d9cb57f5c70a5ce574bddb4f1391` before any new numerical execution.
The implementation/tests commit is `8607092c8ccd6c9c1c2df1a08387577112e9570b`.
Subsequent evidence-only commits do not change the tested sources.

## Recovered failure and cause

The old source was copied before editing and compiled with the same Release
compiler/flags. Only the original initialization prefixes were replayed;
the historical deadline was disabled for these bounded prefix diagnostics.
No rollback or full pilot price was computed. Each replay stopped at exactly
the saved visited-node/BS count and retained the original error text.

[Diagnosis](dal502-bbs-repair/diagnosis.json) contains the three original
requests/IDs, n, zero-based j, root and actual spot, dt, u/d/p, discounted up/
down probabilities, d1/d2, both erfc/CDF values, rate/yield discounts, both
price terms, the rejected value and binary64 hex encodings. All inputs to
the diagnosed scalar calculation are finite. The guards rejected negative
values; these were not nonfinite failures or invalid tree probabilities.

- PUT n=8192, root S=99, j=4117: actual S=108.86812897227774,
  dt=.0001220703125, d1=38.45560418367248, d2=38.453394474981266.
  Both rounded CDFs are 2 minimum subnormals; the unequal discounted
  coefficients produce rejected value `-8.8931816251424378e-323`.
- PUT n=8193, root S=100.5, j=4114: actual S=108.82078834416117,
  dt=.00012205541315757354, d1=38.26110814397549,
  d2=38.25889857014185. The two price terms are approximately
  1.479603e-318 and 1.479223e-318, giving
  `-3.8043054729775984e-322` after the put sign.
- CALL n=16385, root S=99, j=8176: actual S=94.17185671373306,
  dt=.000061031431187061336, d1=-38.43280185194914,
  d2=-38.4343643042676. The rejected value is
  `-1.1363509854348671e-322`.

The cause is quantization of subnormal CDFs before multiplication and
subtraction. Their relative precision is insufficient to retain the small
positive difference. The rejection itself correctly prevents a negative
European value from entering the terminal condition.

Independent mpmath scalar BS uses a d2-first formula, high-precision erfc and
exact binary64 inputs. The 100- and 160-decimal-digit evaluations agree to
better than 1e-85 relative. The three positive true values are respectively
approximately `4.861381128219865e-326`, `8.531288657731370e-323`, and
`7.152064486069598e-326`. Half the smallest binary64 subnormal is
`2^-1075`, approximately `2.470328229206233e-324`. Thus the first and third
correctly round to zero; the second must retain **17 minimum subnormal
units**, approximately `8.399115979301191e-323`. Replacing all three negative
results with zero would destroy a representable value.

## Stable expression and diagnostics

Only private scalar arithmetic and error details in `bbs.hpp` changed.
The original `Obstacle`, `Initialize`, `Layer` and `Evaluate` source suffix
is byte-identical. CRR u/d/p, discounts, terminal max, American exercise and
the BBS last-step mathematical definition remain unchanged. There is no clamp,
Richardson, new reference level or acceptance-threshold change.

Write the out-of-the-money scalar value as

$$
V=A\phi(x)hQ(x,h),\qquad
Q(x,h)=\frac{M(x)-M(x+h)}{h},\qquad
M(x)=\frac{\Phi(-x)}{\phi(x)}.
$$

For a call, x=-d1 and A is discounted spot; for a put, x=d2 and A is
discounted strike. In both cases h=sigma*sqrt(dt). For x>=8, a 64-level
positive continued fraction evaluates M. Its divided difference is propagated
directly: if a=k/(x+a_next), b=k/(x+h+b_next), then
`(a-b)/h = k*(1-(a_next-b_next)/h)/((x+a_next)*(x+h+b_next))`.
This retains the small difference without subtracting two rounded CDFs or
two close Mills ratios. The final value uses
`exp(log(A)-x*x/2-log(sqrt(2*pi))+log(h)+log(Q))`, combining the scale before
the final exponential. The ordinary expression remains in use outside this
tail region. The x>=8 switch selects arithmetic, not qualification criteria.

Finite/nonnegative value admission remains enforced; nonpositive or nonfinite
sd is also rejected. Errors retain the `BBS invalid European value` prefix and
now include call/put, actual spot, strike, volatility, rate, yield, dt, sd,
d1/d2 and rejected value with max_digits10 precision. The saved final log
contains reproducible zero-time, NaN-input and nonfinite-value witnesses.

## Focused RED/GREEN and independent checks

Before the arithmetic edit, the new focused test
`BbsScalarTest.TestRejectedPutTailRetainsRepresentableSubnormalValue` failed
with the original `BBS invalid European value` exception. It passed after the
minimum change to the private reference arithmetic. The exact
17-unit assertion was retained. The old header, rejected values and RED log
are preserved in the raw archive.

The final standalone Google Test executable passes **4/4** tests:

- Exact representable-subnormal regression for the rejected PUT node.
- 41 fixed scalar inputs: all three rejected nodes, adjacent floating-point
  spots and adjacent tree spots, ordinary PUT/CALL controls, and tails around
  x=8/12/25/37/39/50/100. Inputs and high-precision values were fixed before
  the repair. JSON is read with full-precision parsing.
- Invalid arithmetic still throws and includes every required replay field.
- All three original initialization prefixes complete through the formerly
  rejected positions, with finite nonnegative values and no rollback.

[Scalar comparison](dal502-bbs-repair/scalar-comparison.json) retains all
observed values. All representable subnormal results match the independently
rounded result exactly (zero ULP difference). All 17 zero-valued witnesses
have an independently positive value below half-minimum-subnormal. Maximum
relative difference for normal results is `1.5524600616980947e-11`; normal
price controls also meet the absolute binary64 roundoff check. These are
private arithmetic checks, not a BBS convergence qualification.

Actual final build/test commands:

```bash
cmake -S dal-cpp/test-support/pde-risk-qualification \
  -B build/dal502-bbs-repair-tools -DCMAKE_BUILD_TYPE=Release \
  -DDAL_CORE_BUILD="$PWD/build/dal502-make"
cmake --build build/dal502-bbs-repair-tools \
  --target dal502_bbs_tests dal502_bbs -j2
python3 build/dal502-bbs-repair/budget.py final-green 16410 16455 \
  build/dal502-bbs-repair-tools/dal502_bbs_tests \
  --gtest_output=xml:build/dal502-bbs-repair/tests-final.xml
```

The standalone CMake target registers `dal502_bbs_scalar` with CTest; `ctest -N`
verified registration without another numerical run. New reference/test targets
link no core or public library. Only the affected opt-in binaries were built.
Local lizard CCN<=8 and whitespace checks pass. Documentation verification is
recorded with delivery. There is no full-suite, performance-matrix or pilot run.

## Resource account and identities

[Resources](dal502-bbs-repair/resources.json) append every attempt, including
the expected RED failure, to the immutable prior costs. One worker was used,
with prlimit AS/RSS headroom 128 MiB and a remaining-time subprocess timeout.
Counts include old-prefix replay, three diagnostic scalar reconstructions,
82 independent BS evaluations at the two precisions, and every native test
attempt. Build/text/integrity operations perform no BS/tree pricing.

New batch: **49,230 nodes / 49,407 BS calls / 0.131164 seconds cumulative
numerical subprocess wall time / 17,576 KiB maximum worker RSS**. These are
within 1e6 / 1e5 / 60 seconds / 128 MiB and one of two allowed workers.
Raw output plus prior pilot raw output remains below 64 MiB; the exact archive
and per-file sizes/hashes accompany delivery in the
[raw manifest](dal502-bbs-repair/raw-archive.json).

Cumulative BBS charge is **3,230,750,980 nodes**, visited nodes
**3,028,838,550**, and **594,267 BS calls**. Old CRR work remains
2,997,881,833,992; combined charge is **3,001,112,584,972**, below
3,003,881,833,992. Original reservations are retained, with this batch added
separately; no reservation is reset or used to fill an unexecuted pilot job.

[Identity](dal502-bbs-repair/identity.json) binds the implementation commit,
old/new executable hashes, source/configuration/compile-command hashes,
dependencies, compiler, independent mpmath version and final XML. The new
native BBS executable has a distinct producer identity despite retaining the
same BBS mathematical method name. No identity bridge is created.

[Preserved origins](dal502-bbs-repair/preserved-origins.json) retain all 49
old successful prices and payload hashes under their original producer. The
original ledger hash is unchanged. Production, ANALYTIC/CRR/LOG-LCP, original
pilot fixtures/results, P2 view repair and prior qualification records remain
untouched. This batch creates **zero full pilot prices**.

The coordinator retains the next decisions: affected tester verification,
focused review of the new BBS method/repair, and whether to resume the pilot
under a separately specified quota. PR #685 stays draft without auto merge;
DAL-502 stays in progress and S4 stays paused. Required pushed-head CI is
reported separately and is not replaced by earlier green checks.
