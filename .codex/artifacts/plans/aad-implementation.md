# DAL AAD implementation ledger

Status: active implementation under the native-only AAD scope.
No stage is complete until its correctness, compatibility,
performance, and applicable CI evidence has been inspected.

Current boundary (2026-10-10): F02, P02, P03 and F03 are complete.
Financial PDE acceptance is merged in
[#506](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/506),
at `bef4d87a`, after two complete 36/36 exact-head audits, zero Codacy
annotations and unresolved threads, and a clear full current-head review body.
All six financial cases execute in each of fourteen sanitizer/extended/MSVC
profiles. Their checked-out merge preview and final squash merge have identical
tree `a42cd250ae0c7e781d5a0369266578144d167c49`.
The two new caller cost rows complete forty samples in 5.59 seconds; all 176
existing objects and seven accepted callers retain their hashes, with zero old
timing rows repeated. Completed financial controls remain in immutable
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/8493c3c11e75d68dfc8f12eaf4c30a981a3014ce/.codex/artifacts/specs/aad-pde-financial-acceptance.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/8493c3c11e75d68dfc8f12eaf4c30a981a3014ce/.codex/artifacts/reviews/aad-pde-financial-acceptance.md)
and [cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/8493c3c11e75d68dfc8f12eaf4c30a981a3014ce/.codex/artifacts/performance/aad-pde-financial-acceptance.md).

The first P04 numeric plan is accepted in merged
[#507](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/507),
at `c11ebd81`, after two complete 37/37 exact-head audits, zero Codacy annotations
and unresolved threads, and an inspected current-head review body. The date
concern is resolved with the workspace's Asia/Shanghai timezone and re-review.
All ten cases execute in fourteen profiles; tested and merged tree are identical
at `8cabc791b300c25ec9852b5b5f502c25c6595427`. Local acceptance includes 512
small support patterns, six strict checks and installed consumption. All 176 old
objects/seven fresh accepted callers retain bytes; two new numeric costs complete
forty observations in 0.3613 seconds with zero old timing rows repeated.
Completed numeric controls remain in immutable
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c34b5950ebf3eb166cf3f7dd8966cfb3825084ba/.codex/artifacts/specs/aad-structural-jacobian-plan.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c34b5950ebf3eb166cf3f7dd8966cfb3825084ba/.codex/artifacts/reviews/aad-structural-jacobian-plan.md)
and [cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c34b5950ebf3eb166cf3f7dd8966cfb3825084ba/.codex/artifacts/performance/aad-structural-jacobian-plan.md).

Native execution is accepted in merged
[#508](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/508),
at `5f41297f`, after two complete 36/36 exact-head audits, zero Codacy annotations
and unresolved threads, and complete current-head review-body inspection. The
Markdown alignment finding is fixed without repeating C++ tests or timing.
All thirteen cases execute in fourteen profiles; tested and merged tree both
equal `a5ea0c6911286b71a12542b26c940ea327c9f62d`. Six strict checks and an
installed consumer pass. All 177 old objects/seven fresh accepted callers retain
bytes. Four new complete-request costs finish eighty observations in 1.55 seconds
with zero old timing repeats. Completed controls remain in immutable
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/b7459586d877d3a8735b74d92d2d1865dc35acc1/.codex/artifacts/specs/aad-native-structural-jacobian.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/b7459586d877d3a8735b74d92d2d1865dc35acc1/.codex/artifacts/api-notes/aad-native-structural-jacobian.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/b7459586d877d3a8735b74d92d2d1865dc35acc1/.codex/artifacts/critiques/aad-native-structural-jacobian.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/b7459586d877d3a8735b74d92d2d1865dc35acc1/.codex/artifacts/reviews/aad-native-structural-jacobian.md)
and [cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/b7459586d877d3a8735b74d92d2d1865dc35acc1/.codex/artifacts/performance/aad-native-structural-jacobian.md).

Financial dependency capture/identity is accepted in merged
[#509](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/509),
at `282adff0`, after two complete 36/36 exact-head audits, zero Codacy annotations
and unresolved threads, and complete current-head review-body inspection.
All sixteen cases execute in fourteen profiles; tested and merged tree both
equal `040ea87b6709407d7eaf7b8730664fc2a1ec9fa9`. Local acceptance includes
22 targeted cases, eight strict checks, installed consumption and new-function
complexity at most eight. Four scoped cost rows finish 120 observations in
0.2087 seconds; affected old passive/joint paths stay within calibrated noise.
Completed controls remain in immutable
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/90ec2da3c48e50bd85d76dac54acabe42c7070ab/.codex/artifacts/specs/aad-rate-structural-jacobian.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/90ec2da3c48e50bd85d76dac54acabe42c7070ab/.codex/artifacts/api-notes/aad-rate-structural-jacobian.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/90ec2da3c48e50bd85d76dac54acabe42c7070ab/.codex/artifacts/critiques/aad-rate-structural-jacobian.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/90ec2da3c48e50bd85d76dac54acabe42c7070ab/.codex/artifacts/reviews/aad-rate-structural-jacobian.md)
and [cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/90ec2da3c48e50bd85d76dac54acabe42c7070ab/.codex/artifacts/performance/aad-rate-structural-jacobian.md).

Complete financial execution is accepted in merged
[#510](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/510),
at `d18ebe4e`. Two complete final audits accept 36/36 exact-head checks,
zero Codacy annotations and unresolved review threads. Actual logs verify all
ten new cases in fourteen sanitizer, extended and MSVC profiles; the tested
and merged trees both equal `2a847eced1b483f20ef711433a83264aca2ed7e8`.
Ten execution cases and twenty-two affected existing cases pass, including
complete analytic/native matrices, seven pricing and four curve families,
two-step passive differences with rebuilt bases, fresh numeric reuse, invalid
requests, mode restoration and concurrency. Eight strict checks pass; twelve
new production functions have complexity at most seven. Installed consumption
passes 1/1; eight final scoped cost rows finish 200 observations in 0.5103 seconds.
The two affected old paths pass the sustained 4% gate; all 178 other objects
retain accepted bytes. Small-portfolio compressed costs exceed dense costs,
including with reuse; no AUTO speedup is claimed. External review exposed currency
label and dense admission ordering defects; both have RED/GREEN repair evidence,
including a memory-capped early-rejection probe. Completed controls remain in
immutable [specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/7bbeb259062b5338f5ff5db0812e7abb2ad28189/.codex/artifacts/specs/aad-rate-parameter-jacobian.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/7bbeb259062b5338f5ff5db0812e7abb2ad28189/.codex/artifacts/api-notes/aad-rate-parameter-jacobian.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/7bbeb259062b5338f5ff5db0812e7abb2ad28189/.codex/artifacts/critiques/aad-rate-parameter-jacobian.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/7bbeb259062b5338f5ff5db0812e7abb2ad28189/.codex/artifacts/reviews/aad-rate-parameter-jacobian.md)
and [cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/7bbeb259062b5338f5ff5db0812e7abb2ad28189/.codex/artifacts/performance/aad-rate-parameter-jacobian.md).
Merge proof is retained in `rate-parameter-merged-tree.json`.

Measured strategy work is accepted in merged
[#511](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/511),
at `ddea35911`. Two complete final audits accept 36/36 exact-head checks,
zero unresolved review threads and zero Codacy annotations. Actual logs verify
all seven new cases in fourteen sanitizer, extended and MSVC profiles. The
tested and merged trees equal `5b3feec8a99a339a35712f26dcc1559a0aaa66a6`.
Completed controls remain in immutable
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c9d06fa8ecde507e7ba0deea0c2b9c1a3cd22d30/.codex/artifacts/specs/aad-rate-jacobian-strategy.md),
[API note](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c9d06fa8ecde507e7ba0deea0c2b9c1a3cd22d30/.codex/artifacts/api-notes/aad-rate-jacobian-strategy.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c9d06fa8ecde507e7ba0deea0c2b9c1a3cd22d30/.codex/artifacts/critiques/aad-rate-jacobian-strategy.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c9d06fa8ecde507e7ba0deea0c2b9c1a3cd22d30/.codex/artifacts/reviews/aad-rate-jacobian-strategy.md)
and [cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c9d06fa8ecde507e7ba0deea0c2b9c1a3cd22d30/.codex/artifacts/performance/aad-rate-jacobian-strategy.md).
Direct dense planning removes generic conflict construction. The explicit cached
plan request checks current identity once and falls back to the requested dense
matrix on mismatch. Local acceptance covers 40 distinct cases, eight strict
checks and installed-only consumption. The final scoped cost study completes
760 observations in 9.86 seconds without a sustained regression; 177 unchanged
archive objects reuse accepted evidence. The 64-by-8 dense financial request
improves approximately 28–32% across scalar and width-four cases. The external
operation-label finding has focused RED/GREEN repair evidence; structural error
labels and dense rate request context are preserved.

All declared lightweight shapes lose with compression, including cached reuse.
Keep the dense default and explicit caller selection, with no universal AUTO
cutoff or hidden trial pricing. Native forward is unavailable for this closed
financial path. P04's implementation and publication requirements are closed.
The operation-context and Markdown alignment findings were repaired before merge.

P05's generic core is accepted in merged
[#512](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/512),
at `235a5c11`. Two complete final audits accept 36/36 exact-head checks,
zero unresolved threads and zero Codacy annotations. Actual logs verify all
twelve cases in fourteen sanitizer, extended and MSVC profiles. The tested and
merged trees equal `f49459b03ef36a2e2c658d9ab7f26c3ee3e4dece`. Eight strict
OFF/combined checks and installed-only consumption pass. Eight scoped
regime/strategy pairs finish 320 observations in 1.592 seconds, with approximately
60% smaller long-path tape plus checkpoints and 40–52% greater reusable-tape time
cost. Keep segmentation explicit; short paths have no capacity benefit.
Completed core controls remain in immutable
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a5d0022e54c5e07c5751b75fc9c8f19e2ce3ecea/.codex/artifacts/specs/aad-path-segmentation.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a5d0022e54c5e07c5751b75fc9c8f19e2ce3ecea/.codex/artifacts/api-notes/aad-path-segmentation.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a5d0022e54c5e07c5751b75fc9c8f19e2ce3ecea/.codex/artifacts/critiques/aad-path-segmentation.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a5d0022e54c5e07c5751b75fc9c8f19e2ce3ecea/.codex/artifacts/reviews/aad-path-segmentation.md)
and [cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a5d0022e54c5e07c5751b75fc9c8f19e2ce3ecea/.codex/artifacts/performance/aad-path-segmentation.md).

P05's financial fixed-path adapter is accepted in merged
[#513](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/513),
at `201339fd`. Two final audits accept 36/36 current-head checks, zero Codacy
annotations and zero unresolved threads; complete final Copilot/Codex bodies
contain no open actionable findings. Actual logs verify all 29 new cases in each
of fourteen sanitizer, extended and MSVC profiles. The tested and merged trees
equal `3a792482c0cc0634a6a76d6f7dbc695387532d1f`.
The trusted preparation, full state/trace replay, independent mathematical checks,
budgets, recovery, concurrency, ten strict checks and installed consumption pass.
Scoped financial timing accepts 400 samples in 6.883 seconds; affected ordinary
callers and the two affected compiled LSMC caller gates pass. Complete long-path
heap totals are about 6.2% smaller at about 2.6–2.7 times warm latency. Keep this
explicit memory/latency tradeoff. The final provenance repair preserves every
timed request body and all 179 archive members; twelve repaired heap rows match,
so no latency sampling repeats for that admission-only change.
Completed controls remain in immutable
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/19c23ad620788b9bbbab0bdedd89f44abdd5153f/.codex/artifacts/specs/aad-black-scholes-segmentation.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/19c23ad620788b9bbbab0bdedd89f44abdd5153f/.codex/artifacts/api-notes/aad-black-scholes-segmentation.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/19c23ad620788b9bbbab0bdedd89f44abdd5153f/.codex/artifacts/critiques/aad-black-scholes-segmentation.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/19c23ad620788b9bbbab0bdedd89f44abdd5153f/.codex/artifacts/reviews/aad-black-scholes-segmentation.md)
and [cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/19c23ad620788b9bbbab0bdedd89f44abdd5153f/.codex/artifacts/performance/aad-black-scholes-segmentation.md).

P05's Monte Carlo adapter is accepted in merged
[#515](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/515),
at `faccea0b`. Two final audits accept 36/36 checks on `08769ee0`, zero Codacy
annotations and zero unresolved threads. All eleven new tests actually execute
and pass in each of fourteen sanitizer, extended and MSVC profiles. The tested
and merged trees equal `1401f9100a22449a3814e7a2d015b56fb4ef1896`.
All four inline findings are repaired, including the final representable
zero-driver path and the plan-table alignment. Scoped correctness/strict probes,
installed consumption, bounded lanes, ordered reduction and failure recovery pass.
Four selected ordinary-caller gates pass. Long MC requests save about 9.5%/21.5%
warm payload at about 8.1x/7.3x latency; short cold scheduling can retain more tape
memory. Segmentation remains explicit. Review repairs use only one short-request
canary each, retaining the matrix/resource evidence. Upstream #514 error-path
integration preserves every unrelated accepted archive member. Whole P05 is closed.
Completed controls remain in immutable
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/08769ee02a46315c402ddcac9f6928c7b5191864/.codex/artifacts/specs/aad-segmented-monte-carlo.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/08769ee02a46315c402ddcac9f6928c7b5191864/.codex/artifacts/api-notes/aad-segmented-monte-carlo.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/08769ee02a46315c402ddcac9f6928c7b5191864/.codex/artifacts/critiques/aad-segmented-monte-carlo.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/08769ee02a46315c402ddcac9f6928c7b5191864/.codex/artifacts/reviews/aad-segmented-monte-carlo.md)
and [cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/08769ee02a46315c402ddcac9f6928c7b5191864/.codex/artifacts/performance/aad-segmented-monte-carlo.md).

The F04 native bump-over-AAD driver is accepted in merged
[#517](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/517),
at `6d5fde3e06d09d51924a74dc7c19f8f8f84c0c85`, with the identical tested
merge-preview tree `0310f352f9d422fdfde8b901e1fbea03a5f3cac9`.
Two complete paginated audits pass 36/36 checks; Codacy has zero annotations
and all inline and review-body findings are closed. Actual logs confirm all
fourteen new cases in each of fourteen runtime profiles. Scoped costs finish
160 observations in 2.447 seconds; both old-caller +4% rounds pass. Allocation
failure and callback checkpoint/restore repairs have isolated RED/GREEN and
related boundary acceptance. Completed controls remain in immutable
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/074b8345abfcc56e521260a936b004b5ca425db3/.codex/artifacts/specs/aad-bump-over-aad.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/074b8345abfcc56e521260a936b004b5ca425db3/.codex/artifacts/api-notes/aad-bump-over-aad.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/074b8345abfcc56e521260a936b004b5ca425db3/.codex/artifacts/critiques/aad-bump-over-aad.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/074b8345abfcc56e521260a936b004b5ca425db3/.codex/artifacts/reviews/aad-bump-over-aad.md)
and [cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/074b8345abfcc56e521260a936b004b5ca425db3/.codex/artifacts/performance/aad-bump-over-aad.md).

The F04 common-path Monte Carlo increment is accepted in merged
[#519](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/519),
at `d2bd2ad22142929c30eb21ea25870ebbd2123b57`. The tested merge-preview,
publication and merged trees equal `9cf0e27d3f02fd283e6130351e87c010e5c2d631`.
Two final exact-head audits accept 36/36 checks, zero Codacy annotations and zero
unresolved threads; every full review body is dispositioned. Actual logs show
all thirteen new cases passing in each of fourteen runtime profiles. After
upstream #518, thirty-three selected local cases, installed consumption and
affected strict probes pass. Only two affected financial cost rows repeat:
eighty observations in 1.193 seconds. Unchanged generic evidence is retained.
Completed controls remain in immutable
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/ae418029b5ee933bde0194d76b35104821540cac/.codex/artifacts/specs/aad-monte-carlo-curvature.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/ae418029b5ee933bde0194d76b35104821540cac/.codex/artifacts/api-notes/aad-monte-carlo-curvature.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/ae418029b5ee933bde0194d76b35104821540cac/.codex/artifacts/critiques/aad-monte-carlo-curvature.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/ae418029b5ee933bde0194d76b35104821540cac/.codex/artifacts/reviews/aad-monte-carlo-curvature.md),
[cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/ae418029b5ee933bde0194d76b35104821540cac/.codex/artifacts/performance/aad-monte-carlo-curvature.md).

The native Dupire quote increment is accepted in merged
[#520](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/520),
at `262492d79d701a3b81acc9e12e4354649aa8054c`. Two final audits accept 35/35
checks, zero Codacy annotations and unresolved threads. All eleven new cases
pass in fourteen runtime profiles. Tested and merged trees both equal
`617760c1319102dcbd3c2e430bd5165cc60a1654`. The current-head Copilot rerun
hit its account quota; prior complete Copilot findings are dispositioned and
current-head Codex review is clear. Copilot is not a branch-required check.
Twenty-three scoped local cases, seventeen strict probes, installed consumption
and three cost comparisons pass; 120 samples take 0.2875 seconds. Completed
controls remain in immutable
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3ec38b69f0f29c086c6a62c349c1ba58458d63b7/.codex/artifacts/specs/aad-dupire-quote-curvature.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3ec38b69f0f29c086c6a62c349c1ba58458d63b7/.codex/artifacts/api-notes/aad-dupire-quote-curvature.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3ec38b69f0f29c086c6a62c349c1ba58458d63b7/.codex/artifacts/critiques/aad-dupire-quote-curvature.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3ec38b69f0f29c086c6a62c349c1ba58458d63b7/.codex/artifacts/reviews/aad-dupire-quote-curvature.md) and
[cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3ec38b69f0f29c086c6a62c349c1ba58458d63b7/.codex/artifacts/performance/aad-dupire-quote-curvature.md).

The financial MC quote adapter is accepted in merged
[#522](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/522),
at `710899adcec70fb185cb4d52d24df6f2a0e77c62`, with tested and merged tree
`5b2a87ada1092111dcbadfb86d7c3bc443a49889`. Both complete publication audits
pass 35/35 checks, with zero Codacy annotations, unresolved threads or actionable
review findings. Fourteen actual runtime profiles execute all eleven new cases.
Copilot's review quota was exhausted; the complete current-head Codex review is
clear. Fifty distinct local affected cases, ten strict probes and installed
consumption pass. Three scoped cost comparisons retain 120 samples containing
0.7812 seconds of timed work. Completed controls are immutable:
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/41ae9478a819880144c7b2373f1b1ab2efc4f7b7/.codex/artifacts/specs/aad-mc-quote-curvature.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/41ae9478a819880144c7b2373f1b1ab2efc4f7b7/.codex/artifacts/api-notes/aad-mc-quote-curvature.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/41ae9478a819880144c7b2373f1b1ab2efc4f7b7/.codex/artifacts/critiques/aad-mc-quote-curvature.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/41ae9478a819880144c7b2373f1b1ab2efc4f7b7/.codex/artifacts/reviews/aad-mc-quote-curvature.md),
[cost report and harness](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/41ae9478a819880144c7b2373f1b1ab2efc4f7b7/.codex/artifacts/performance/aad-mc-quote-curvature.md).

Single/same-currency joint rate replay is accepted in merged
[#523](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/523),
at `de810076b7e3748e17c22a6ee621f7f0190ea551`, with tested and merged tree
`835cb3b32da8fc237d69d2752e331e3013d87c1c`. Both complete publication audits
pass 35/35 checks, with zero Codacy annotations, unresolved threads or actionable
review findings. All twelve new cases execute in fourteen runtime profiles.
Copilot's quota was exhausted; current-head Codex review is clear. Twenty-four
distinct local affected cases, six strict probes and installed consumption pass.
Three scoped cost comparisons retain 120 samples and 1.3698 seconds of timed
work; the Codacy repair repeats only the 80 affected new-path samples, retaining
40 old control samples with exact executable identity. Completed controls are
immutable:
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c81367ae13dd63629171cde3cb8ba4db2e829eb7/.codex/artifacts/specs/aad-rate-quote-curvature.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c81367ae13dd63629171cde3cb8ba4db2e829eb7/.codex/artifacts/api-notes/aad-rate-quote-curvature.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c81367ae13dd63629171cde3cb8ba4db2e829eb7/.codex/artifacts/critiques/aad-rate-quote-curvature.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c81367ae13dd63629171cde3cb8ba4db2e829eb7/.codex/artifacts/reviews/aad-rate-quote-curvature.md),
[cost report and harness](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c81367ae13dd63629171cde3cb8ba4db2e829eb7/.codex/artifacts/performance/aad-rate-quote-curvature.md).

Rate-provider cross-currency acceptance is merged in
[#524](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/524),
at `b5b17a0f362daa444deb27406245583d6523cf10`. Both complete publication
audits pass 35/35 checks with zero Codacy annotations, unresolved threads or open
actionable review findings. All 24 affected rate cases actually pass in each of
fourteen runtime profiles. Tested merge preview `fb575fb95aae841fd410cb88201f567194ce7fc0`
and final squash merge have identical tree `5ffca22fc5c0b76f561e59bb6772f8ac6734d30c`.
Current-head Codex review is clear; Copilot could not execute because its account
quota was exhausted and it is not a branch-required check.
Local acceptance includes 90 independent financial curvature references, 30
gradient coordinates, eight strict probes and installed consumption. Three scoped
cost comparisons pass with 120 samples and 0.8805 seconds of measured work;
existing single-curve round deltas are +2.83%/+0.69%, below the sustained 4% gate.
The repeated-declaration-name review repair has RED/GREEN evidence, eight affected
cases rerun and sixteen unchanged cases retained. Completed controls are immutable:
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/04e9dd0cc109b78cb3e17fb0779387e54eecf29d/.codex/artifacts/specs/aad-xccy-quote-curvature.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/04e9dd0cc109b78cb3e17fb0779387e54eecf29d/.codex/artifacts/api-notes/aad-xccy-quote-curvature.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/04e9dd0cc109b78cb3e17fb0779387e54eecf29d/.codex/artifacts/critiques/aad-xccy-quote-curvature.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/04e9dd0cc109b78cb3e17fb0779387e54eecf29d/.codex/artifacts/reviews/aad-xccy-quote-curvature.md),
[cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/04e9dd0cc109b78cb3e17fb0779387e54eecf29d/.codex/artifacts/performance/aad-xccy-quote-curvature.md)
and [harness](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/04e9dd0cc109b78cb3e17fb0779387e54eecf29d/.codex/artifacts/performance/aad-xccy-quote-curvature-cost.cpp).

Financial trading quote curvature is accepted in merged
[#525](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/525),
at `f30c7bf4`, after two complete 35/35 audits, zero Codacy annotations and
unresolved review threads, and clear current-head Codex review. All 58 selected
cases execute in fourteen runtime profiles. The tested merge preview and final
squash merge share tree `3acc1ced75eed4b980b50d2e6e2785eaf3571bb3`.
Local acceptance includes ten strict probes, installed consumption and five
cost comparisons (200 observations; 1.0866 seconds of measured work).
Completed controls remain immutable:
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c26e198d882cbaae91fe1a5683b9cb5ea2742c86/.codex/artifacts/specs/aad-rate-trade-quote-curvature.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c26e198d882cbaae91fe1a5683b9cb5ea2742c86/.codex/artifacts/api-notes/aad-rate-trade-quote-curvature.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c26e198d882cbaae91fe1a5683b9cb5ea2742c86/.codex/artifacts/critiques/aad-rate-trade-quote-curvature.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c26e198d882cbaae91fe1a5683b9cb5ea2742c86/.codex/artifacts/reviews/aad-rate-trade-quote-curvature.md),
[cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c26e198d882cbaae91fe1a5683b9cb5ea2742c86/.codex/artifacts/performance/aad-rate-trade-quote-curvature.md)
and [harness](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/c26e198d882cbaae91fe1a5683b9cb5ea2742c86/.codex/artifacts/performance/aad-rate-trade-quote-curvature-cost.cpp).

Policy curvature is accepted in merged
[#527](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/527),
at `19a34172`, after two complete 36/36 audits, zero Codacy annotations and
unresolved threads, and clear current-head Codex/Copilot reviews. All 17 new
cases actually execute in fourteen runtime profiles. The tested merge preview
and squash share tree `793da2e70c5b92f47431f9221e38b05eb1548949`.
Local acceptance includes 26 selected tests, eight strict probes, installed
consumption and four paired cost cases (160 observations; 1.831268117 measured
seconds). Model provenance prevents incompatible Hybrid preparations from
entering the Black-Scholes adapter. Immutable controls remain available in the
[accepted specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/138846cf8d8960548af8da96cf321a1a1badeccd/.codex/artifacts/specs/aad-lsmc-curvature.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/138846cf8d8960548af8da96cf321a1a1badeccd/.codex/artifacts/reviews/aad-lsmc-curvature.md)
and [complete cost evidence](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/138846cf8d8960548af8da96cf321a1a1badeccd/.codex/artifacts/performance/aad-lsmc-curvature-results.json).

Native smooth mixed mode is the active increment, under its
[specification](../specs/aad-smooth-mixed-mode.md),
[API](../api-notes/aad-smooth-mixed-mode.md) and
[critique](../critiques/aad-smooth-mixed-mode.md).
It composes native forward directional arithmetic with scalar reverse, without
an outer finite-difference step. Ordinary native higher-order and independent
nesting remain disabled. Rectangular/approximate calibration semantics remain
outside the accepted smooth square-system quote-curvature scope.
The prototype now passes 21 new and three selected legacy tests, six strict
OFF/combined probes, six capability compile probes and installed C++ consumption.
Two scoped paired cost cases retain 80 refreshed observations and 2.970270252 measured
seconds in the [complete cost report](../performance/aad-smooth-mixed-mode.md).
Publication remains; estimated effort is 6–12 developer-hours excluding CI wait.

Native PDE recording is accepted in merged
[#505](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/505),
at `72cac784`, after two complete 36/36 exact-head audits, zero Codacy
annotations and unresolved review threads, and a clear full current-head review
body. All 31 new cases execute in each of fourteen sanitizer/extended/MSVC
profiles. Their checked-out merge preview and final squash merge have identical
tree `9d1586a0498f06c815660c91b9a45d54385756e3`. The ARM64 runner-capacity
cancellation is repaired by one job rerun; successful wheel jobs are retained.
All 175 existing objects and seven freshly linked existing callers retain their
accepted bytes. Twelve new cost rows complete in 2.46 seconds; zero old timing
rows repeat. Completed native controls are retained in immutable
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/17e67ea4536a537eb8e47f96a2ba72209a274602/.codex/artifacts/specs/aad-native-sampled-theta-step.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/17e67ea4536a537eb8e47f96a2ba72209a274602/.codex/artifacts/reviews/aad-native-sampled-theta-step.md)
and [cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/17e67ea4536a537eb8e47f96a2ba72209a274602/.codex/artifacts/performance/aad-native-sampled-theta-step.md).
Complete financial price/Greek and grid-convergence acceptance follows in #506
under its immutable
[caller API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/8493c3c11e75d68dfc8f12eaf4c30a981a3014ce/.codex/artifacts/api-notes/aad-pde-financial-acceptance.md)
and [critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/8493c3c11e75d68dfc8f12eaf4c30a981a3014ce/.codex/artifacts/critiques/aad-pde-financial-acceptance.md).

The first F03
numeric operator is accepted in merged
[#490](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/490),
at `a08d20e0`, with the exact tree of final `11a6a20b`. Both complete
paginated publication audits verify 35/35 checks, zero Codacy annotations and
zero unresolved review threads. Actual logs confirm all ten new cases in four
Windows, six sanitizer and four extended configurations. All Windows installed
consumers pass 3/3; every extended Python run passes 1,166 cases.

The accepted numeric operator reuses an owning normalized LU for forward and
transpose solves. Independent analytic, scalar-AAD, finite-difference and
directional oracles cover dense coordinates, multiple RHS, permutations, seed
linearity, ownership, recovery and concurrent const reverse. The review finding
for early RHS scaling is reproduced and repaired with per-column late scaling;
direct-division references retain representable subnormal solutions/adjoints.
The corrected sixteen-case cost study accepts 640 processes and 81,920 results.
Fourteen fresh legacy/portfolio links retain accepted hashes; the Eigen-free
translation unit passes 10/10. Earlier failed/superseded evidence remains retained.

Completed numeric artifacts are retired; immutable history retains their
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a08d20e02396ef89629df3edb869f81bca0033f9/.codex/artifacts/specs/aad-linear-solve-pullback.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a08d20e02396ef89629df3edb869f81bca0033f9/.codex/artifacts/critiques/aad-linear-solve-pullback.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a08d20e02396ef89629df3edb869f81bca0033f9/.codex/artifacts/reviews/aad-linear-solve-pullback.md)
and [cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a08d20e02396ef89629df3edb869f81bca0033f9/.codex/artifacts/performance/aad-linear-solve-pullback.md).
Merge and actual-log verification are retained in session evidence
`aad-linear-solve-pullback-merged-01.json` and
`aad-linear-solve-final-runtime-acceptance-01.json`.

The recorded F03 increment is accepted in merged
[#491](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/491),
at `05420e36`. Final exact-head audits accept 35/35 checks, zero Codacy
annotations and zero unresolved review threads. Actual logs confirm all 48 new
cases in each of 14 sanitizer, extended and MSVC configurations. Scoped
performance acceptance covers the affected callers and boundaries in 9.6
minutes; the unstarted 81-case portfolio matrix was replaced by six cases.

Opt-in numeric diagnostics are accepted in merged
[#493](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/493), at
`c3f93930`: 35/35 exact-head checks, zero Codacy annotations and zero unresolved
threads. Actual logs confirm all 17 diagnostic cases in each of 14 platform,
extended and sanitizer configurations. Independent rational residual references
pass 32/32; ordinary runtime objects match the accepted baseline byte for byte.
Four informational size/RHS cases complete the scoped diagnostic cost study.

Owning native diagnostic results are accepted in merged
[#494](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/494), at
`d08b9f63`, under the completed
[recorded diagnostic specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/d08b9f63392138f54981c8d8c679901f958ba1ec/.codex/artifacts/specs/aad-linear-solve-diagnostics.md).
Ordinary recording includes, numeric caches and native layouts remain unchanged.
Thirteen new cases cover analytic composition, three activity combinations in
scalar and width-1/4/8 modes, independent Cramer/difference references,
checkpoints, detached ownership, exact budgets, range failure and concurrency.
The affected local batch passes 72/72; the installed CMake consumer passes.
Six scoped ordinary comparisons pass the two-round 4% rule and all 166 unchanged
objects match baseline bytes. Two complete exact-head publication audits verify
35/35 checks, zero Codacy annotations and zero unresolved threads; actual logs
confirm all 13 new cases in each of 14 MSVC, extended and sanitizer configurations.
The Codacy oracle-complexity finding is repaired without dropping scenarios.
Immutable evidence is retained in `pr-494-merged.json` and
`native-runtime-01/verification.json`; completed diagnostic artifacts are retired.

Numeric symmetric/banded coordinate pullbacks are accepted in merged
[#495](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/495), at
`0ed5a908`, under the completed
[coordinate specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/0ed5a908d81ca02fcb8a1d5b80885868237fa6b6/.codex/artifacts/specs/aad-solve-coordinates.md).
Sixteen new tests pass locally,
including independent differences/directions, indefinite systems, exhaustive
small layouts, wide metadata, snapshot ownership, const concurrency and exact
construction/reverse budgets. The affected batch passes 27/27 and the installed
consumer passes. A scoped cost finding is repaired with hoisted row ends and
inline contraction; the final 16 affected cases pass without repeating old
suites. All 167 existing library members and a fresh ordinary executable match
accepted bytes. Six informational cost rows retain small/full-band overhead
and a narrow-band benefit; see the accepted
[report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/0ed5a908d81ca02fcb8a1d5b80885868237fa6b6/.codex/artifacts/performance/aad-solve-coordinates.md).
Both complete exact-head audits accept 35/35 checks, zero Codacy annotations
and zero unresolved review threads. Actual logs verify all 16 new cases in each
of 14 MSVC, extended and sanitizer configurations. Merge verification is retained
in `pr-495-merged.json`; completed numeric artifacts are retired.

Native coordinate recording is accepted in merged
[#496](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/496), at
`3d597db8`, with the exact tree of accepted head `40f8fb7f`. Twenty-eight new cases
cover independent Cramer/native-expression and multi-step difference references,
three activity combinations, scalar/width-1/4/8 sweeps, aliases, repeated seeds,
checkpoints, ownership, exact resources, failure recovery and concurrency.
All 72 distinct affected cases pass under combined original and failed-only
repair evidence; fixture failures and the initial missing-header RED are retained.
The private event wrapper is shared between dense and coordinate payloads;
ordinary payload fields and Number/node/tape layouts stay unchanged. Native
coordinate recording passes installed CMake consumer and strict OFF/combined ON
warning checks. Eight existing-caller comparisons pass the scoped two-round 4%
policy, including diagnosed-cache borderline confirmation. All 167 other archive
members match accepted bytes; three new-interface cost boundaries retain both
benefits and full-band metadata overhead. See the accepted
[performance report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/40f8fb7fb3bc2168cb24dd5194ca86576aff0d66/.codex/artifacts/performance/aad-native-solve-coordinates.md).
Both complete paginated audits accept 36/36 exact-head checks, zero Codacy
annotations and zero unresolved review threads. Actual logs verify all 28 new
cases in each of 14 MSVC, extended and sanitizer configurations. The overview's
date-only review observation is assessed against the supplied Asia/Shanghai
date. Merge verification is retained in `pr-496-merged.json`; completed native
coordinate artifacts are retired into Git history.

Numeric forward/transpose accuracy checks are accepted in merged
[#497](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/497), at
`0c5b5918`, with the exact tree of accepted head `a0144f51`.
All 36 exact-head checks pass, with zero Codacy annotations and unresolved
review threads. Actual logs verify all twelve new cases in each of fourteen
sanitizer, extended and MSVC configurations. Installed consumption passes.
All 168 previous archive members and both fresh legacy callers retain accepted
bytes after checking their 31 dependency headers; accepted timing is reused.
The archived
[performance report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a0144f51ab873975b600911084ecf38652c97b5a/.codex/artifacts/performance/aad-solve-accuracy.md)
discloses the three optional-checking cost boundaries. Merge verification is
retained in `pr-497-merged.json` and `pr-497-tree-verification.json`.
Completed numeric accuracy artifacts are retired into Git history.

Native per-invocation accuracy reports are accepted in merged
[#498](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/498), at
`e30858c8`, with the exact tree of accepted head `45ba16db`.
Local acceptance covers
25 new and 78 related existing cases, actual reverse-window/channel provenance,
independent rational/Cramer references, owning copies, failure cleanup and exact
tape/caller resources. A focused empty-width RED is repaired through native
mode validation; eight affected cases pass without repeating old suites.
Installed consumption, strict OFF/combined ON and complexity checks pass.
The accepted
[performance report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/45ba16db3bfbcf2170005fe3cdfe20fc783aa222/.codex/artifacts/performance/aad-native-solve-accuracy.md) accepts
twelve existing caller rows and discloses six new optional-cost rows. Only the
optional accuracy object changes in the review repair; fresh existing caller
binaries retain their measured hashes. Repeated complete paginated audits
accept 36/36 exact-head checks, zero unresolved review threads and zero Codacy
annotations. Actual logs verify all 25 new cases in each of fourteen sanitizer,
extended and MSVC configurations. Guarded merge and tree verification are
retained in `native-accuracy-after-merge.json` and
`native-accuracy-merged-tree.json`; completed artifacts are retired into history.

Checked numeric coordinate accuracy is accepted in merged
[#499](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/499),
at head `991ca6a45043509f7596f082e046d457b3d30faa`. Its
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/991ca6a45043509f7596f082e046d457b3d30faa/.codex/artifacts/specs/aad-coordinate-solve-accuracy.md)
defines one checked
physical cache and contracts packed parameters directly without a dense matrix
gradient. First-case RED confirms the missing public interface. Its frozen
performance scope selects four actual existing symmetric/banded native caller
rows, with optional new costs reported separately. Local acceptance now covers
13 new and 44 affected existing cases, strict OFF/combined ON compilation and
fresh installed usage. All four existing caller rows pass the declared
two-round sustained-regression rule; four optional cost rows expose added
condition/residual work and exact storage. See the
[local review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/991ca6a45043509f7596f082e046d457b3d30faa/.codex/artifacts/reviews/aad-coordinate-solve-accuracy.md) and
[performance report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/991ca6a45043509f7596f082e046d457b3d30faa/.codex/artifacts/performance/aad-coordinate-solve-accuracy.md).
All 36 exact-head checks pass; actual logs confirm all 13 new cases in fourteen
sanitizer, extended and MSVC configurations. Final review has zero unresolved
threads and Codacy zero new findings. Guarded merge
`4bf66b4fa7b356cb34a6594626c33826940085cc` has the accepted tree. Evidence is
retained in `coordinate-accuracy-runtime/verification.json`, final paginated
audits and `coordinate-accuracy-merged-tree.json`; completed artifacts are
retired into history.

Checked native coordinate accuracy is accepted in merged
[#500](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/500),
at head `2dc6d6eb06da060cdc07d11b61a5d82795465cd3` and merge
`f98ef99d6037a1922ef7223d8d23f005292ab53c`; accepted and merged trees match.
Its [specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/2dc6d6eb06da060cdc07d11b61a5d82795465cd3/.codex/artifacts/specs/aad-native-coordinate-solve-accuracy.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/2dc6d6eb06da060cdc07d11b61a5d82795465cd3/.codex/artifacts/api-notes/aad-native-coordinate-solve-accuracy.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/2dc6d6eb06da060cdc07d11b61a5d82795465cd3/.codex/artifacts/critiques/aad-native-coordinate-solve-accuracy.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/2dc6d6eb06da060cdc07d11b61a5d82795465cd3/.codex/artifacts/reviews/aad-native-coordinate-solve-accuracy.md) and
[performance report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/2dc6d6eb06da060cdc07d11b61a5d82795465cd3/.codex/artifacts/performance/aad-native-coordinate-solve-accuracy.md)
retain the shared dense collector, one checked physical cache and O(p) bindings.
Fifty-one distinct local solve boundaries pass. The sustained
+4.86%/+6.08% first dense cached gate is preserved; the private inline repair
restores it to -0.65%/-1.36%, with all eighteen actual caller rows passing the
unchanged two-round +4% rule. Four optional cost rows disclose added physical
checks/report work. Strict OFF/combined ON and installed usage pass.
Repeated complete paginated audits accept all 36 exact-head checks, zero
unresolved threads and zero Codacy annotations/new issues. Actual logs verify
all eighteen new cases in fourteen sanitizer, extended and MSVC profiles.
Completed artifacts are retired into immutable history. Final evidence includes
`native-coordinate-accuracy-audit-final`, `native-coordinate-accuracy-runtime`
and `native-coordinate-accuracy-merged-tree.json`.

Numeric implicit-root linearization #501 is accepted and merged at
299cc8d02caa408409e66ded93fe4f4a0c8e2a51. Accepted head
a4a43f19a8c76bd908b55536a0328bb3ab25f7e7 and merge trees both equal
532010598dbd8e63dd486ae3b58a0934c856c48f. All 36 exact-head checks pass;
all 22 root cases actually execute in each of fourteen profiles. Copilot's
current-head re-review has zero open findings; the contraction-underflow
finding is fixed and resolved. Codacy has zero new issues/annotations and all
threads are resolved. Repeated paginated final audits and guarded merge pass.

One equation evaluation and owning J/K/point/policy serve independent transpose
seed columns. Independent complete root/stationarity references, branch and
approximate-point identity, exact accuracy/resource limits, source destruction,
concurrent readers and failure recovery pass. Nonzero contraction products that
round to zero reject; exact minimum subnormal risks remain supported. All 171
legacy archive objects remain identical. Only four affected root cost rows are
measured for the repair; their risks/resources match and the unchanged sustained
+4% gate passes. Strict compilation and installed consumer also pass.

Accepted controls and evidence are preserved as immutable
[SPEC](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a4a43f19a8c76bd908b55536a0328bb3ab25f7e7/.codex/artifacts/specs/aad-implicit-root-linearization.md), [API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a4a43f19a8c76bd908b55536a0328bb3ab25f7e7/.codex/artifacts/api-notes/aad-implicit-root-linearization.md), [critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a4a43f19a8c76bd908b55536a0328bb3ab25f7e7/.codex/artifacts/critiques/aad-implicit-root-linearization.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a4a43f19a8c76bd908b55536a0328bb3ab25f7e7/.codex/artifacts/reviews/aad-implicit-root-linearization.md) and [performance](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/a4a43f19a8c76bd908b55536a0328bb3ab25f7e7/.codex/artifacts/performance/aad-implicit-root-linearization.md).
Completed artifacts are retired into Git history. Final session evidence includes
implicit-root-repair-audit-final, implicit-root-runtime and
implicit-root-merged-tree.json.

Native implicit-root recording is accepted in merged
[#502](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/502), at
7ded331fa48a390a8efb2197ee3544c5da233333. Final head
3434a0731c737230dd889837bf0d4ff41b22d2d6 and merge share tree
d4038d76aa0c8cebc0265a95242b4ba6c8c62937. All 36 exact-head checks pass,
with zero Codacy issues/annotations and unresolved review threads. Actual logs
confirm all 26 new cases in fourteen platform configurations. The macOS x86
Python 3.14 lock-window scheduling failure is reproduced and repaired without
changing native code; its final wheel run passes 1,164 tests with two skips.

All 115 affected existing native solve cases, strict OFF/combined checks and
installed consumption pass. Caller-owned equation buffers, post-callback phase
rejection and exact publication-overlap resources have focused RED/GREEN
evidence. Only the 22 actual shared-object legacy caller rows require fresh
performance acceptance; four optional native-root costs are reported separately.
Final callback changes retain unchanged caller binaries and their accepted
timing evidence. The Python-only CI repair requires no new native timing.

Accepted controls remain in immutable
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3434a0731c737230dd889837bf0d4ff41b22d2d6/.codex/artifacts/specs/aad-native-implicit-root.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3434a0731c737230dd889837bf0d4ff41b22d2d6/.codex/artifacts/api-notes/aad-native-implicit-root.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3434a0731c737230dd889837bf0d4ff41b22d2d6/.codex/artifacts/critiques/aad-native-implicit-root.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3434a0731c737230dd889837bf0d4ff41b22d2d6/.codex/artifacts/reviews/aad-native-implicit-root.md)
and [performance report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3434a0731c737230dd889837bf0d4ff41b22d2d6/.codex/artifacts/performance/aad-native-implicit-root.md).
Completed controls are retired. Session evidence includes
native-implicit-root-audit-final, native-implicit-root-runtime and
native-implicit-root-merged-tree.json.

The owning numeric sampled theta-step is accepted in merged
[#504](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/504),
at head `3b4aad18e3a51a8888ae3af732959f416de43fe8` and merge
`94799f9e3f349bf89b3a2c58a16bde322d90c9f6`. Both complete paginated
audits accept 36/36 checks, zero Codacy annotations and zero unresolved threads.
Actual logs verify all 28 cases in each of fourteen sanitizer, extended and
MSVC profiles. Guarded squash merge matches the tested preview tree
`c5e0f586c25da5e63ecd4eadb214d41153b09dbe`.
Seven independent step references cover 121 coordinates/363 differences;
complete three-step rollback covers 33 coordinates/99 differences. Forty-two
existing dense accuracy cases, installed consumption and strict checks pass.
The subnormal assertion loophole and failed copy-assignment corruption are
reproduced and repaired. Transactional assignment preserves the original cache
under zero/one-byte-short failure. Twenty affected legacy caller rows pass
scoped two-round +4% acceptance in 38 seconds. After assignment repair, all
legacy/passive caller executables remain byte-identical; zero legacy rows are
remeasured and twelve optional PDE rows take another 1.91 seconds.

Completed controls are retired into immutable history:
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3b4aad18e3a51a8888ae3af732959f416de43fe8/.codex/artifacts/specs/aad-sampled-theta-step.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3b4aad18e3a51a8888ae3af732959f416de43fe8/.codex/artifacts/api-notes/aad-sampled-theta-step.md),
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3b4aad18e3a51a8888ae3af732959f416de43fe8/.codex/artifacts/critiques/aad-sampled-theta-step.md),
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3b4aad18e3a51a8888ae3af732959f416de43fe8/.codex/artifacts/reviews/aad-sampled-theta-step.md)
and [performance report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/3b4aad18e3a51a8888ae3af732959f416de43fe8/.codex/artifacts/performance/aad-sampled-theta-step.md).
Session evidence includes pde-sampled-step-audit-final,
pde-sampled-step-runtime and pde-sampled-step-merged-tree.json.

The accepted native sampled PDE increment is retained under its immutable
[specification](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/17e67ea4536a537eb8e47f96a2ba72209a274602/.codex/artifacts/specs/aad-native-sampled-theta-step.md),
[API](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/17e67ea4536a537eb8e47f96a2ba72209a274602/.codex/artifacts/api-notes/aad-native-sampled-theta-step.md) and
[critique](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/17e67ea4536a537eb8e47f96a2ba72209a274602/.codex/artifacts/critiques/aad-native-sampled-theta-step.md).
The missing-header RED and first independent scalar-rate GREEN pass.
One owning event reuses the accepted numeric cache and generic checked event
wrapper. Seven selected activity fields and full activity pass in scalar and
width-1/4/8 modes against fifteen exact-symbolic risks with direct objective
terms: 32 sub-scenarios within one named test. Active fields override their
primal values and shape; unused passive fields need no populated values.
Layer and channel axes, independent output slots and exact unused/zero-lane
risks are verified. Thirty-one new cases pass in focused batches: exact-symbolic
aliases, repeated seeds, serial rollback and upstream volatility; seven frozen
high-precision fixtures including consecutive adjacent pivots; ownership,
independent workers, wrong/stale/foreign inputs, checkpoints and raw windows;
inclusive accuracy, nonfinite/range/late-channel failures and recovery; exact
caller/tape limits with one-byte-short refunds and 32 capacity sub-scenarios.
The seven numeric reference fixtures share unchanged golden data between numeric
and native tests; both affected existing numeric cases still pass.
Strict compilation repairs add braces around test assertion macros without
changing their numerical assertions. Installed native CMake consumption passes.
All 175 accepted existing archive members match as a name/hash multiset, including
the reindexed numeric sampled-step basename. Seven fresh existing caller links
retain accepted hashes; zero existing timing rows are repeated. The three new
native size boundaries pass twenty process samples/240 observations in 2.46s,
with all numerical/resource checks; see the accepted
[cost report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/17e67ea4536a537eb8e47f96a2ba72209a274602/.codex/artifacts/performance/aad-native-sampled-theta-step.md).
Twenty-four strict OFF/combined ON source/header checks pass. Exact-head
publication acceptance completes with two 36/36 audits at `17e67ea4` and
guarded merge #505. All 31 cases execute in each of fourteen profiles.
No shared helper or existing production source has changed.

The initial native PR #505 head 0293c04a reports one Codacy test finding: a local
scalar projection functor named `read` is mistaken for a buffer-reading API.
The focused repair names it `valueOf` while preserving bounds, reference data and
assertions. Only its two native reference cases and strict modes need new local
evidence; the unchanged production/performance proof remains applicable.
The finding and subsequent review-body documentation findings are repaired and
accepted in the merged final head; no native-step finding remains open.

The following recording notes are
historical repair evidence, not outstanding #491 gates.

Recording progress: the analytic/interface, passive/RHS-only, reset-mutation,
resource and capability RED logs remain in session evidence. Forty-eight new
cases now pass across three overloads, independent scalar/finite-difference
oracles, vector widths 1/2/3/4/8, aliases, shared/serial solves, repeated reverse,
checkpoints, raw windows, block boundaries, ownership and failure recovery.
The resource implementation admits exact owned descriptor/owner/table/cache
capacity and numeric scratch before allocation, refunds failures and releases
before scalar reuse. Parent buffer ceilings overlap reverse scratch while
retained cache construction/destruction suspends parent accounting. Exact peak,
one-byte-short, allocation/copy failures, suffix release, detached readmission,
foreign live inputs, wrong-thread use and table growth have focused evidence.
The production resource batch passes 113 affected cases; subsequent edge and
integration batches pass 8/8, 4/4 and 3/3 without repeating broad suites.

The resource implementation `25f347c7` passes 35/35 exact-head checks, with zero
Codacy annotations and unresolved threads; its six sustained tape/Jacobian
regressions are repaired in published `64cdbfa6`. That head passes 75 legacy
comparisons using fresh affected measurements and verified identical binaries,
but fails the GSR AAD price Jacobian. The current local repair uses a thread-stable
validation-flag address and a cold validation helper while retaining failure/mode
checks. Final affected functionality passes 132/132. Same-binary noise controls
and six GSR rows pass, with the formerly failing case at +3.13%/+3.95% after
borderline confirmation. See the [performance report](../performance/aad-linear-solve-recording.md).
Under the project-wide performance scope amendment, the unstarted 81-case matrix
is cancelled. Final affected caller/boundary measurements, installed consumers
and exact-head OFF/ON runtime CI remain open. The 48 new cases comprise 45
recording/resource cases, one RHS-only numeric pullback case, one native
seed-clear case and one live-block range case; the existing capability-contract
test is separate.
Scoped acceptance finds a sustained large width-8 portfolio regression. Bulk
root seed clearing preserves validation and alias semantics; 36 focused cases
pass and the repaired single-case confirmation is -12.79%/-13.43%. Final
affected callers/boundaries and exact-head CI remain open.
Review identifies repeated live-block scans during input capture. A temporary
budgeted sorted index removes that repetition; 63 directly affected tests and
four combined diagnostic syntax checks pass before final publication. The
original rate-risk executable's failing row passes shape-matched A/A controls
and 30-pair confirmation at +0.58%/+0.10%; the earlier failure remains recorded.
The macOS Python scheduling assertion is repaired, with deterministic RED/GREEN
and three focused Python tests. Final platform and remaining scope gates stay open.
Accepted #490 baseline libraries, headers and tests are frozen in
`aad-recorded-solve-baseline-01/provenance.json`; the published prototype is
frozen separately in `aad-recorded-solve-increment-01/provenance.json`.

Scope amendment (2026-10-04): the user requires removing XAD, CoDiPack and Adept
support and keeping only DAL's built-in native AAD. This replaces the earlier
four-backend compatibility goal. D00 is now a Stage A requirement; D03 is limited
to useful native operations and capability contracts. See the controlling
[native-only specification](../specs/aad-native-only.md).
Removal is implemented locally; fresh native functionality and paired performance pass.
All 28 exact `b42f9eb` CI checks and the D00/D01/D02/D03 audit pass.
Earlier external-backend results below remain
historical evidence, not continuing support obligations or removal acceptance.

Controlling design: [detailed AAD plan](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/de5dd8b20089223e6938dfd8100d463aa6d4a169/.codex/artifacts/plans/aad-improvement-plan.md).
Its appendix expansions preserve the original scope and add worked examples,
task cards, resource models, complete request execution, operator pullbacks,
cache/failure boundaries and explicit no-regression acceptance.
The further method expansion specifies solver precision, nonsmooth estimators,
sparse reconstruction, segmented state seeds, backend effects and result failures;
The native-only amendment adds external support removal and supersedes prior
external-backend obligations while preserving the remaining feature scope.
Initial implementation baseline: `b5e3caca85bb1a5b1aa3acf7898b387832d443c8`.

The user authorized full implementation on 2026-10-04 and requires existing functionality
to retain performance and CI compatibility. This ledger preserves the full scope across
incremental implementation turns and PRs; a green first stage does not complete the goal.

## Delivery rules

- Delivery amendment (2026-10-05): finish all F01 request, budget, market-coordinate
  and binding acceptance in PR #480. Then review and fix the entire PR, reconcile
  its base with master and unresolved production performance, and merge only after
  current-head checks and review gates pass. The user explicitly authorized this
  merge. F02, P02/P03, F03, P04/P05 and F04 continue in new PRs after that merge;
  the full implementation goal remains active across these delivery boundaries.
- Work in isolated writable sources; preserve the original workspace and unrelated changes.
- F02 delivery boundary (2026-10-06): weighted C++/Python/Excel PR #483 is
  merged at `d1600a15`. Accepted head `9e24cc77` passes all 35 checks, zero
  Codacy issues/annotations and both paginated review audits. All four Windows
  modes run thirteen weighted typed/raw cases. Eight repaired scalar-entry
  comparisons pass; identical MC/GSR/LSM executables retain the other 44 cases.
  Continue budgeted blocked Jacobians in a separate PR under the
  [new contract](../specs/aad-blocked-script-risk.md),
  [API decisions](../api-notes/aad-blocked-script-risk.md) and
  [critique](../critiques/aad-blocked-script-risk.md). Portfolio integration
  remains later F02 work.
- F02 delivery boundary (2026-10-06): blocked Jacobian PR #484 is merged at
  `1c9273c9`. Accepted head `1785162c` passes all 35 CI checks, zero Codacy
  annotations and two complete paginated review audits with no unresolved
  threads. All 160 existing performance comparisons pass the unchanged paired
  policy; 78 informational block-width processes pass independent row oracles.
  Both actual Windows raw-export cases pass in all four configurations.
  Compatible portfolio integration is accepted in PR #487, merged at `bf52e386`.
  Final head `47c33a12` passes 35/35 checks, both complete paginated publication
  audits, four actual Windows modes, six sanitizer jobs and all 160 comparative
  cases. Completed portfolio artifacts remain in Git history. P02 worker reuse
  is accepted in merged #488; P03 selected extraction continues under its new
  specification. The full plan remains incomplete.
- Establish a failing independent test before each behavioral change.
- Validation amendment (2026-10-05): select tests from the changed behavior and
  dependency paths. A binding-only increment runs its focused tests and affected
  legacy contracts; reuse accepted native mathematical/configuration evidence
  when those sources and contracts are unchanged. Do not repeat complete Python
  or C++ suites after small parser/documentation fixes. Consolidate full applicable
  configuration/platform acceptance at the stable F01/merge boundary; rerun
  broader checks earlier only for a new failure or an identified coverage gap.
  Keep the existing mathematical assertions, paired performance policy and
  required final-head CI gates. Inspect CI asynchronously during independent
  work; avoid publishing small follow-up commits that cancel an active matrix.
- Remove external AAD implementation, dependency, export, example and CI paths;
  validate native diagnostic OFF/ON and their actual operation contracts.
- Preserve legacy single-output APIs, units, paths, normalization, and LSM policy semantics.
- Compare isolated Release baseline/head binaries with matching dependency SHAs and configuration.
- Performance scope amendment (2026-10-07): for all project development, select
  the smallest set covering changed paths, callers and meaningful boundaries.
  Do not run all nine targets or a full parameter matrix at each edit/PR/merge.
  Repeat affected cases during repair; expand only for a documented failure,
  dependency change or coverage gap. Reuse evidence with immutable provenance
  and verified executable identity. See `AGENTS.md` and the benchmark reference.
- Use the existing paired regression policy for selected cases: ten interleaved
  process samples per side in each of two confirmation rounds, best-of-N minima,
  and a 4% threshold. Borderline results may require additional samples.
- Cover affected production workloads while retaining required exact-head CI.
- Performance failures require investigation and correction. Noisy evidence is inconclusive.
- Keep each change reviewable. Publish current-state documentation only after behavior exists.
- Do not mark the overall goal complete until every applicable requirement below is verified.
- Report the whole-plan status as a table and update remaining development-time
  estimates at the end of every active development turn, at milestones, and when
  findings change the estimate. Distinguish implementation, local verification,
  exact-head CI acceptance and publication rather than combining them into one status.
- Reporting amendment (2026-10-07): during active development, provide a complete
  progress table every 15 minutes, including next development tasks and remaining
  effort. Report material failures promptly. Separate development effort from
  CI queues and shared-environment waits; do not count overlapping work twice.

## Stage A: trustworthy differentiation and measurement

- [x] C01: propagate every nonzero scalar adjoint; exact zero is the only default skip.
- [x] C02: vector channels have independent propagation semantics and match scalar requests.
- [x] C03: non-finite derivatives/seeds remain observable; public results diagnose invalid risk.
- [x] C04: preserve consumed intermediate clearing, leaf accumulation, repeated sweeps, and checkpoints.
- [x] C05: no implicit approximate gradient truncation.
- [x] D00: remove XAD, CoDiPack and Adept code, gitlinks, configuration, exports,
  examples, scripts and CI; verify fresh native-only builds and migration errors.
- [x] P01: correct production-oriented tape/Jacobian/MC benchmarks and explanatory resource metrics.
- [x] D01: explicit recording lifecycle, checkpoints, nested-use rejection, and exception recovery.
- [x] D02: optional owner/slot-lifetime diagnostics without release per-node overhead.
- [x] D03: thin native operation/capability contracts; remove unpublished external
  adapters and selection metadata rather than introducing a pluggable framework.

## Stage B: market and portfolio risk

- [x] D04: scalar model/script requests/results with stable axes, methods, units,
  provenance and numeric payload budgets; future quote/output methods remain with F01/F02.
- [x] D04: compatibility projections for existing `PV` and `d_...` outputs.
- [x] F01: deterministic-rate Dupire spread-quote pullback connected to Hybrid valuation gradients.
- [x] F01: direct quote dependencies and snapshot/axis mismatch validation.
- [x] F01: calibration-only and full bump/recalibrate/common-path oracles.
- [x] F01: common calibration pullback integration with existing curve quote-risk semantics.
- [x] F02: fixed-weight VJP for multiple prepared-script outputs, including aliases/constants.
- [x] F02: budgeted native blocked Jacobian with explicit rerecording behavior.
- [x] F02: compatible portfolio observation/timeline integration.
- [x] P02: per-worker capacity reuse and safe re-registration/reinitialization.
- [x] P03: measured block-width policy and demand-driven result extraction; default maximum one retained.
- [ ] Bindings: F01/F02 C++/Python/Excel is accepted; extend all three surfaces
  for the remaining structured-operator and second-order requests.

## Stage C: structured reverse operators

- [x] F03: independent linear-solve pullback with decomposition reuse and directional adjoint oracle.
- [x] F03: recording integration, aliases, multiple seeds, repeated reverse, cache ownership, and failures.
- [x] F03: optional numeric condition and forward componentwise residual diagnostics, including range/resource acceptance.
- [x] F03: owning native diagnostic results with unchanged ordinary-path behavior and resource accounting.
- [x] F03: numeric symmetric/banded independent coordinates with singular/ill-conditioned rejection and exact resources.
- [x] F03: native coordinate recording with aliases, modes, checkpoints and exact binding/scratch resources.
- [x] F03: owning transpose residual reports and declared forward/reverse accuracy policy.
- [x] F03: implicit calibration and PDE pullbacks derived and verified separately.
- [x] P04: proven structural sparsity, safe invalidation, compressed seeds, and mode selection evidence.
- [x] P05: measured long-path checkpoint with complete state/RNG restoration and recomputation.

## Stage D: second-order risk

- [x] F04: specified Gamma, cross-Gamma, and Hessian-vector requests using bump-over-AAD.
- [x] F04: quote-risk second order includes calibration curvature through full recalibration.
- [x] F04: common-path, smoothing, Frozen/RetrainedBump, and nested-step semantics.
- [ ] F04: native mixed-mode prototype on smooth kernels and actual capability validation.
- [ ] F04: estimator validation for applicable simulation/calibration cases before general promotion.

F04 uses separate focused PRs in the following order. The table gives the rolling
remaining estimates for second-order requests; they do not add to the mixed-mode or
final-integration estimates. Each PR requires independent mathematical RED/GREEN,
affected-boundary tests, scoped costs and complete current-head CI/review acceptance.

| Increment                   | Required result                                                                           | Remaining hours |
|-----------------------------|-------------------------------------------------------------------------------------------|-----------------|
| Native driver               | #517 merged; explicit requests, mathematical/lifecycle and exact-head acceptance complete | 0               |
| Financial MC                | #519 merged; common-path requests and exact-head acceptance complete                      | 0               |
| Quote curvature             | #520/#522/#523/#524/#525 merged; financial trading integration and publication accepted   | 0               |
| Policy/estimator validation | #527 merged; native policy semantics, numerical checks and publication accepted           | 0               |

## Completion evidence

- [ ] Focused red/green evidence and independent mathematical references for each feature.
- [ ] Fresh full native/core/public/portable-binding verification.
- [ ] Fresh native-only OFF/ON, no external dependency/export/selection paths,
  explicit old-config migration errors and exact implementation-head CI checks.
- [ ] Python and Excel parity, generated-source integrity, and documentation integrity.
- [ ] Change-scoped performance acceptance with caller/boundary coverage and retained raw evidence.
- [ ] Current-state method docs, examples, and necessary changelog entries.
- [ ] Local read-first review with no unresolved correctness or compatibility findings.
- [ ] Requirement-by-requirement audit of the actual final state.

## Current evidence and next action

Prior profiling state: exact `8886c083` passes all 35 CI checks, including the
four MSVC native/profiling/lifetime combinations and sanitizer/TSan jobs.
P01's 24-profile resource/scaling/diagnostic-cost sweep completes 2,496 processes;
all prices and risks satisfy rel/abs 1e-10, and every LSM sample is bitwise equal.
The [complete production report](../performance/aad-production-profiling.md)
records all 65 formal, 44 MC, 25 curve and 44 matched-prefix cases, plus all
scaling/resource/overhead rows. The nine-target gate passes unchanged. Curve
confirmation passes 25/25. Earlier MC confirmations are 43/44 and 42/44 with
different failed cases; all remain retained. The final current-head complete
confirmation now passes 44/44, with 600 processes, 1,240 unchanged inputs and
bitwise LSM PV/risk parity. See the
[final production report](../performance/aad-production-final-confirmation.md).
P01 local acceptance is complete; final whole-PR checks still remain.

Evidence now persists under the home cache; exact source/baseline checkouts and
the `/tmp` alias were restored after cleanup. All 1,681 frozen source/helper/binary
hashes match. Measured executables were retained unchanged.

Independent Stage B work proceeds under the [D04 specification](../specs/aad-risk-results.md),
[API boundary](../api-notes/aad-risk-results.md) and [critique](../critiques/aad-risk-results.md).
The scalar converter has RED missing-interface evidence followed by nine focused
GREEN tests. Its first standard OFF build passes all 2,341 non-benchmark CTest
cases. The public planner and actual execution/product/model/history provenance
are implemented, with seven focused C++ tests, sixteen Python cases, three
portable Excel cases and the independent date-capture consumer passing.
Callback-mutation and malformed Excel factor tests first fail, then pass after
fixing the actual snapshot/error-context defects. Native empty-column requests
retain smoothing; LSM/RQMC results remain bitwise equal to legacy results and
retrained policy risks carry a mixed-method label. Twelve Excel function stubs
are regenerated with their markup.

The complete OFF build passes 2,351 non-benchmark CTest cases (809 Python tests
and all 34 examples); combined profiling/lifetime passes 2,364 cases. Final
review fixes add snapshot geometry/history validation, correct generated optional
boolean defaults and share Excel path-error context. Their actual failing tests
and compile probe are retained; final OFF focused checks pass 25 cases, combined
checks pass 89, and legacy Excel contracts pass 28 per configuration. The final
fully instrumented ASan/UBSan shared libraries pass 77 core and 10 public cases.
Both installed consumers and the independent date-capture consumer pass after
the fixes. Production complexity passes the unchanged CCN-eight threshold;
regeneration has no drift and changes only two optional-boolean inc/HTML pairs.

The [new-entry cost report](../performance/aad-risk-entry-cost.md) retains 640
alternating process samples over 16 cases, all with bitwise single-worker parity
and unchanged source/helper/binary hashes. Structured metadata adds about four
microseconds to short requests; longer native cases differ by -0.27% to +1.70%.
These are additional-capability costs, not an old-entry performance verdict.
All nine final OFF gate executables remain identical to the retained measured
binaries. The simulation/LSM/tape hot loops are unchanged by D04.

Exact `d2916e9c` finishes 35 checks with 29 successes, five failures and one skipped
wheel-matrix verification. Two MSVC configurations and Windows/ARM-macOS wheels
fail a new Python exact-PV assertion; the Windows gate fails consequently.
The four-worker local RED reproduces it, and legacy/self controls show the same
ordinary parallel reduction rounding. Preserve every original exact assertion
and all 257 paths in an isolated one-worker process, then separately check four
parallel tree/compiled 257/2057-path cases at the already specified rel/abs 1e-10.
Focused GREEN passes 20 cases and the complete four-worker Python suite passes
813. Production code and the nine gate binaries are unchanged by this correction.
Exact corrective `e3720f9f` passes all 35 checks, including both previously failing
MSVC combinations and the Windows/ARM-macOS wheels. The scalar D04 boundary is
accepted; those results do not cover the later unpublished Dupire source.
P01 stays inconclusive and the complete Stage B/C/D scope remains required.
F01 has its [frozen-calibration specification](../specs/aad-dupire-pullback.md),
[API decision](../api-notes/aad-dupire-pullback.md) and [critique](../critiques/aad-dupire-pullback.md)
controlling active core implementation, followed by Hybrid and binding oracles.
The core frozen snapshot and native scalar VJP are now implemented locally.
Ten new Dupire tests plus five interpolation contracts pass OFF: every flat and
Merton quote bucket/direction passes all three predeclared difference steps,
with all 42 rows retained. They cover alias-additive/negative/zero seeds, separate
direct terms, content identity, callback mutation, original-IVS destruction,
domain failures, nested rejection and success/failure/success recovery. An
unrepresentable grid first times out, then rejects before sampling after the
checked-entry precision guard. Final OFF/combined non-benchmark CTest passes
2,364/2,377 cases, fully instrumented ASan/UBSan passes 75 core and 10 public
cases, and two installed consumers pass each configuration. All nine old gate
executables remain SHA-256 identical. The [core cost report](../performance/aad-dupire-entry-cost.md)
retains 240 processes, complete hashes and numeric identity: warm frozen-snapshot
costs are about 18–1,020 microseconds and independent VJPs span 18.216–179.557 microseconds
over its four informational cases. These are extra capability costs, not a
production no-regression verdict. The [core review](../reviews/aad-dupire-pullback.md)
has no unresolved local correctness findings; exact-head CI and full
Hybrid/binding/curve acceptance remain required. None of the F01 completion
boxes is closed by this core increment.

Core publication `d740a4c6` receives one Codacy warning: a test's CCN is 12 rather
than the existing maximum eight. The corrective bucket/direction helpers retain
all cases, fixed steps, tolerances and independent old-calibrator references.
Local complexity now passes without changing the limit; OFF/combined focused
checks each pass 15, with all 42 oracle rows identical to their prior traces.
The default core archive remains SHA-256 identical. The instrumented refactor
rerun passes 15 cases with all 42 oracle rows identical. Exact corrective
`e1ff0dd6` passes all 35 CI checks. This accepts the published core increment;
those checks do not cover the subsequent public Hybrid source.

The public Hybrid adapter now derives surface seeds from typed component order
and model ordinals, checks the retained model and complete axis, and adds a
passive result wrapper without rerunning Monte Carlo. The initial OFF complete
chain passes all 300 oracle rows: 216 independent surface-node rows and 84 full
quote bucket/direction rows over flat/Merton bases and tree/compiled evaluation.
The extended suite passes 18 public checks and 321 oracle rows, including
21 distinct-trade full portfolio recalibration rows. All original 300 rows
remain bitwise identical. The [aggregation oracle review](../critiques/aad-dupire-aggregation.md)
retains the failed strict distinct-seed comparison and its bitwise reproduction
against the unchanged published core; exact power-of-two controls and the
predeclared independent portfolio oracle pass without changing production math.
The direct input is a PV adjoint, including the analytic cashflow discount; an
earlier test supplied an undiscounted payment adjoint and its failed trace is
retained. Steps, tolerances, paths and the old-calibrator/valuation reference
remain unchanged. Complete combined-diagnostic CTest passes 2,395 cases; fully
instrumented ASan/UBSan passes 28 public cases, with all 321 oracle rows identical
to OFF. Both installed consumers pass in OFF and combined. The complete OFF
CTest passes all 2,382 cases, including its existing billion-path European MC
example. The [public entry-cost report](../performance/aad-dupire-hybrid-entry-cost.md)
retains 1,280 processes over 16 cases/four modes, with all 1,040 input hashes and
numeric references unchanged. Public pullbacks cost about 40–42/270–275
microseconds on 9×2/21×8 surfaces; these are additional-capability costs. All
nine old gate binaries and the default core archive remain identical. The
[public review](../reviews/aad-dupire-hybrid-pullback.md) has no unresolved local
correctness findings. Binding, curve integration and exact new-publication CI
acceptance remain separate requirements.

Exact public Hybrid publication `c5c922eff4ddffb52aacf841cf7ca2e351422f58`
passes all 35 CI checks. This accepts that increment, including its retained
321 oracle rows, without closing F01's binding or common-curve requirements.
The [binding API decision](../api-notes/aad-dupire-bindings.md) controls active
Python implementation. Its initial C++ missing-overload compile and 31 Python
missing-interface failures are retained before production implementation.

The Python projection and flat-BS convenience now pass complete local
verification: 42 new language checks, 84 fixed-step quote rows, OFF CTest
2,383 (including 855 Python checks and 33 regular examples), combined CTest
2,397 and fully instrumented ASan/UBSan 30 public checks. Both installed C++
consumers pass OFF/combined; standalone Python against the installed public
package passes 62 checks. An independent consumer linked to the unchanged
accepted `c5c922ef` native libraries matches all 248 Python surface/price/model/
quote numeric cells bitwise. The existing 321 native and initial 84 Python
oracle rows remain identical. Nine old gate binaries and the default native
archive remain identical; CCN-eight, format, documentation and generation pass.
All 497 generated files have zero content drift.

The [Python review](../reviews/aad-dupire-python-bindings.md) retains all failed
evidence, including the initial architecture-guard include failure, an incorrect
new shape-error regex and an existing exact parallel risk assertion. The latter
is reproduced as ordinary legacy-self/legacy-to-structured roundoff against
the unchanged accepted package. Keep its exact 257-path assertion under an
explicit scoped single worker and add eight four-worker Hybrid/GSR cases at
the existing rel/abs 1e-10 protocol. No production math or CI limit changes.
New binding publication/CI, Excel and common curve adaptation remain required.

Exact Python `12dcd09a` completes 35 checks with 33 successes, one macOS ARM
wheel failure and one dependent wheel-matrix skip. Seven Dupire cases fail the
strict primal replay check. A local contracted-arithmetic RED reproduces
call-level rounding amplified by the strike second difference. The
[replay correction](../api-notes/aad-dupire-replay-rounding.md) retains the old
agreeing graph and uses bounded scalar-call primals with the original expression
derivatives only on mismatch. The final relative/absolute 1e-12 surface check,
all fixed quote steps/tolerances, boundary aliases and recording lifecycle remain.

The first unconditional replay passes correctness but regresses four VJP costs
by 72–98%; retain and reject that implementation. The final conditional version
passes all twelve paired entry combinations over 480 processes under the
unchanged two-by-ten/4% comparison. Original nine gate binaries remain identical;
the native archive changes. Final OFF/combined CTest passes 2,384/2,398, relevant
GCC ASan/UBSan passes 95 and Clang FMA passes 11. Both installed consumers pass
in each configuration. Rebuilt standalone Python passes 62; joint/standalone
installed modules each match all 248 accepted native cells bitwise. Previous
42 core, 321 Hybrid and 84 Python oracle traces remain identical. Generation
has zero drift in 497 files. Fully instrumented Clang FMA with lifetime/profiling
also passes all 11 cases. Corrective head `95f0706d` passes all 35 CI checks,
including ARM wheels (`aad-dupire-replay-ci-05.jsonl`); see the
[review](../reviews/aad-dupire-replay-rounding.md) and
[cost report](../performance/aad-dupire-replay-rounding.md). Excel and common
curve integration remain F01 implementation tasks; every full F01 box
and the Stage B/C/D requirements remain open.

The [Excel boundary](../api-notes/aad-dupire-excel.md) is now implemented and
locally verified: immutable grid/quote/calibration/seed/result handles, checked
Merton inputs, a detached model from frozen carry, complete Hybrid quote mapping
and copied getters. C++ and Python share the checked factories; the existing
BS-local-vol factory shares an internal builder without changing its alias or
constructor convention. Nine Excel cases and all 84 independent legacy quote
oracle rows pass. Refactoring preserves every oracle row bitwise. Final
OFF/combined pass 2,394/2,408 cases, including 857 Python checks OFF and 33 regular
examples. Fully instrumented ASan/UBSan passes 71 relevant cases. Both final
installed prefixes pass two C++ consumers; installed joint and freshly built
standalone Python each pass 64 cases. All 23 new registration/HTML pairs have
matching markup. Production CCN-eight, formatting and documentation checks pass.

Existing nine gate binaries and native archive remain bitwise identical to the
accepted correction. The changed old BS model factory's separately declared
two-by-ten/4% cost comparison passes both rounds (+0.29%/+0.49%) with all 40
numeric checks and unchanged input hashes. Preserve the initial test/tool
failures; the [Excel review](../reviews/aad-dupire-excel.md) records the exact
evidence and limits. Excel publication-head CI and common curve adaptation
remain required before full F01 acceptance.

Excel publication `ad9681d2` reports a Codacy test-helper complexity failure:
`CheckDirection` has CCN 12 against the unchanged limit of eight. The previous
local scan covered production functions. A test-only helper extraction passes
the complete file's CCN check (29 functions, no warnings), all nine Excel cases
in OFF and combined, and 71 fully instrumented ASan/UBSan cases. All 84 oracle
rows in each configuration remain bitwise equal to the published trace; no
step, tolerance, assertion, path count or CI policy changes. Preserve the
original annotation and local RED. Exact test correction `50d286af` passes
Codacy, but both published Excel heads fail all four MSVC jobs.

The Windows SDK's `REGISTERING` macro collides with the native recording enum
when the new Excel input helper precedes native declarations. Actual local
MSVC 14.51.36231/SDK 10.0.26100.0 reproduces the production and test compile
failures. Loading native declarations first fixes both; OFF and combined
each pass both syntax checks, including the generated worksheet entries and
Windows-only input test. Full DLL linking/runtime validation remains a CI
requirement. Rebuilt OFF/combined each pass nine Excel cases with all 84
published oracle rows bitwise unchanged; rebuilt ASan/UBSan passes 71 relevant
cases. No enum, SDK macro, generated content, runtime math, tolerance or policy
changes. Preserve all failed CI/local evidence. Corrective publication
`533b6602` passes all 35 exact-head checks, including four complete MSVC
build/link/runtime configurations and wheel builds. The capture
`aad-dupire-excel-windows-fix-ci-06.jsonl` accepts the reviewed Excel increment.
The [Excel review](../reviews/aad-dupire-excel.md) now approves that increment;
common curve adaptation and full F01 acceptance remain open.

The common boundary now has a controlling
[specification](../specs/aad-calibration-pullback.md),
[API decision](../api-notes/aad-calibration-pullback.md) and
[critique](../critiques/aad-calibration-pullback.md). It requires a shared
immutable result, complete content identity and explicit curve record capture.
The initial native capture implementation establishes actual missing-interface
and three-provider RED before GREEN. Six focused tests pass across all four
provenance kinds, ANALYTIC/BUMPED and generic joint layered bases. Thirteen
canonical records independently verify against their published SHA-256 state
fingerprints. Ownership, case-preserving bytes and an actual inverse-disabled
calibration pass; original ten record traces remain bitwise unchanged.
Keep the failed fast JSON test-decoder evidence: full-precision parsing fixes
its one-ULP read discrepancy without changing production arithmetic or assertions.
Final OFF/combined full suites pass 2,400/2,414, including 857 Python checks and
33 regular examples OFF. Fully instrumented ASan/UBSan passes 105 relevant
cases; all six capture cases also pass with leak detection ON. Actual default
and captured portfolio aggregation match every price, quote/DV01 bucket and
metadata field exactly across all kinds, modes, layered graphs and mixed PV
currencies. Four installed consumer runs pass eight workloads with identical
state/axis fingerprints, inverse checksums and tolerance. The unchanged
two-by-ten/4% nine-target gate passes all 75 comparable cases; eight separate
default factory cost comparisons also pass. Retained opt-in records occupy
9,115–368,141 bytes in these workloads; all 80 supplementary process numeric
checks and input hashes pass. The
[record review](../reviews/aad-calibration-record.md) and
[performance report](../performance/aad-calibration-record.md) retain the
protocol, failures and bounds. Exact corrective publication is accepted below.
This foundation does not close F01 or P01 production acceptance.

Initial capture publication `28b4e180` fails acceptance: 12 of 35 checks
succeed, 22 builds fail and Codacy requests const-reference JSON input.
The new native tests omit their private bundled RapidJSON include path;
the local `/usr/local` header masks it. The correction declares that path
only for the native test target and reads the JSON tree by const reference.
Actual MSVC reproduces both missing-header failures, then passes all six
production/test OFF/combined syntax checks. Rebuilt full OFF/combined pass
2,400/2,414; rebuilt ASan/UBSan passes 105 plus six leak-enabled capture cases.
All thirteen traces remain bitwise unchanged with the bundled parser, and
fresh installed consumers retain numeric parity. Preserve every failed log.
Fresh corrected performance passes all 75 gate cases and eight default factory
cost comparisons under the unchanged two-by-ten/4% policy. All eighty process
numeric checks and measured input hashes pass. Exact corrective
`a3833be93e64a2e427debbcf787fd99a7ff37c22` passes all 35 checks, including
four MSVC configurations, all wheels and sanitizer jobs. The complete capture
is `aad-calibration-record-correction-ci-11.jsonl`; this accepts native capture.

The shared C++ operation/result is now implemented locally under the common
contract. Missing-header RED precedes GREEN. Twelve focused cases pass:
bitwise typed Dupire parity, independent owned seeds, quote-only direct identity,
all four curve providers, sixteen native mode/representation/layer combinations,
mixed curve representations, actual USD/EUR PV currencies, signed portfolios,
complete source identity, unavailable/missing records, invalid inputs, direct
terms, overflow recovery and preserved/nested native recordings. All twelve
predeclared smooth square directional recalibration rows pass; no step or
tolerance changes. Retain failed test-setup evidence for the initially missing
FlatIVS test class, incorrectly reused curve direct rule, overwritten XCCY
fixture route and legitimate nondependent gradient cells. Reference assembly
now admits only the original structural-zero reason; other failures reject.
Full OFF/combined suites pass 2,412/2,426, including 857 existing Python checks
and 33 regular examples OFF. Fully instrumented ASan/UBSan passes 186 relevant
cases; all twelve common cases also pass with leak detection ON. Two fresh
installed consumers link only DAL::public and preserve sixteen numeric and
three metadata rows exactly across OFF/combined. Six actual MSVC syntax checks
pass for the common, test and legacy units in both configurations; Windows
runtime publication checks remain required.

The unchanged two-by-ten/4% nine-target gate passes all 75 comparable cases.
All 677 measured inputs remain unchanged; eight gate executables and the
provenance-constructor object are byte-identical to accepted native capture.
Twenty separate informational common-cost processes preserve all twelve numeric
checks and 408 input hashes. Prepared curve calls cost approximately 0.12–1.42
microseconds; the two Dupire sizes cost approximately 18/175 microseconds.
These new-entry costs exclude calibration, record capture and seed construction.
The [shared C++ review](../reviews/aad-calibration-pullback.md) and
[performance report](../performance/aad-calibration-pullback.md) retain full
case rows, independent references, original failures and bounded conclusions.
Exact common C++ publication `12b3d7d1a9e43ac008c257c24b6365e424b458e5`
passes all 35 CI checks in `aad-calibration-pullback-ci-05.jsonl`, including
Windows runtime and ARM wheels. Its review now approves that increment.

The Python capture/common increment passes complete local verification. Retain
the missing-common, missing-capture and raw-matrix RED logs before their GREEN
runs. Sixty-two new cases cover all four curve providers in both inverse modes,
plain/layered generic coordinates, canonical record bytes, strict Boolean capture,
owning/detached values, source identity, numeric coercion/shape/domain errors,
zero/direct/overflow recovery, callback collection and concurrent native mappings.
Actual single/plain-generic trade gradients match old quote risk exactly. A real
larger Dupire operation proves GIL release without enabling a test barrier.
Workspace OFF Python passes 919; fresh standalone OFF/combined each pass 918,
with one existing workspace-only opaque fixture skipped. All three modules
match 48 numeric cells and three metadata rows from the independent installed
C++ consumer exactly. The Python 3.9 syntax floor, CCN-eight, formats and
documentation checks pass. Native archives and all nine old C++ gate executables
remain byte-identical to accepted common C++.

The unchanged two-round/ten-process/4% protocol passes all 35 changed-binding
old-entry workloads: config construction, all four provenance kinds at N=8/16
and ANALYTIC/BUMPED, ten-trade aggregation and typed Dupire. All forty legacy
and twenty separate new-cost processes preserve their numeric digests; all
640 measured input hashes remain unchanged. Prepared common Python calls cost
approximately 0.61–0.66 microseconds for these small curve sources and 26
microseconds for the 9-by-2 Dupire source. This excludes calibration/capture,
seed construction and numeric getters. Keep the first measurement script's
path-alias hash-lookup failure; the correction changes only canonical lookup.
The [Python review](../reviews/aad-calibration-python.md) and
[performance report](../performance/aad-calibration-python.md) retain every
old-entry row and the bounded conclusion. Exact Python publication
`fb1a116bc8672e2b1698467e2d60b19de44babc3` passes all 35 checks in
`aad-calibration-python-ci-05.jsonl`, accepting this increment. Excel
capture/common wrappers and request/budget/market integration remain required.
P01 production MC acceptance remains inconclusive.

The Excel capture/common increment passes local acceptance. Four immutable
common handle types expose 13 worksheet constructors/getters, with detached
matrices and owning typed sources. Five provenance registrations add a strict
optional final Boolean/blank capture flag while preserving legacy export prefixes
and generic dispatcher exclusions. Thirteen portable common cases plus three
Windows raw-cell cases verify ownership, signed priced joint portfolios across
both modes/all representations/plain or layered bases, separate actual PV
currency groups, identity/shape/domain errors, finite overflow, NUL input,
archive rejection and failed-operation recovery. Final portable suites pass
99 cases in each OFF/combined configuration. Full regular CTest passes
2,428/2,442 before the final test-helper fix; final leak-enabled ASan/UBSan
passes 41 related cases. Actual MSVC compilation passes 12/12 production,
registration, import and test-support units. All 36 generated files have no drift.

Read-first review identifies distinct XLL/test-executable tape state, confirmed
by an actual two-module MSVC probe. The repaired lifecycle tests create the
recording inside the XLL utility and retain node-count, nested-use, output
preservation and derivative-recovery checks. Keep the failing missing-helper
build and first probe/fixture/SDK failures. At that local stage Windows runtime
still required the increment's own CI; syntax and portable tests did not replace it.

Initial Excel publication `a522b16b` fails Windows test compilation on three
`numeric_limits::max()` calls colliding with the SDK macro. Keep the first
actual CI log and the reproduced MSVC failure without NOMINMAX. Parenthesized
calls preserve all extreme-value assertions; all 12 syntax units then pass
without the extra local define. Production objects and the paired measurement
remain unchanged. The repaired publication must pass its own complete CI.

Correction `39b2802a` compiles and runs the new common/XLL lifecycle tests on
Windows. Inspected diagnostic runs fail only an existing registration assertion
that still expects five arguments. Update it to verify all original prefixes,
the appended optional capture marker and complete type metadata, and add all
13 common registration entries to the help/long-name checks. Preserve every
failed job log; the next head still requires complete own CI acceptance.

The unchanged two-round/ten-process/4% Excel wrapper protocol passes all 48
old/default comparisons. Forty processes preserve 640 numeric checks and
1,477 frozen inputs. All 32 new capture/prepared-common costs are retained
separately. Both measured production objects are identical after the final
Windows/test-only fix; both native archives and all nine accepted C++ gate
executables remain identical. The [Excel review](../reviews/aad-calibration-excel.md)
and [performance report](../performance/aad-calibration-excel.md) retain all
evidence and bounded conclusions. Excel publication
`a0801deceff9e89ac2aa8a121d985f2b2146c223` passes all 35 exact-head checks in
`aad-calibration-excel-registration-ci-10.jsonl`, including four actual Windows
runtime configurations. Its first GCC-13 job fails only at Eigen checkout with
an early EOF, before compilation. Retain the raw failure and initial rerun
rejection while the parent workflow is active; same-head failed-job rerun after
completion succeeds without changing any input, CI flag or threshold.
Full F01 request/budget/market-coordinate integration remains required.

The first request increment follows the [concrete contract](../specs/aad-calibration-risk-request.md)
and [API decision](../api-notes/aad-calibration-risk-request.md), locally critiqued
before implementation. A passive common quote plan owns canonical source-scoped
IDs and complete/selected native axes, preserves caller ordering, and checks a
budget of all three full retained quote matrices before mapping. Raw calibration,
direct and total derivatives stay in their native shapes; detached scalar-row
projections apply report factors once and reject finite scaling overflow before
publication. No PV currency/value is inferred from external parameter seeds.
Native Dupire parameter identity and its quote-only direct identity are preserved.
Automatic Hybrid script planning, direct constant bindings and request bindings
remain required; this increment does not close F01.

Ten new request cases pass, and both OFF/combined public suites pass 229.
Regular CTest passes 2,438/2,452; leak-enabled fully instrumented combined
ASan/UBSan passes 50 related cases. Both installed public consumers pass all
24 native/direct/report oracle cells. Actual MSVC OFF/combined production/test
syntax passes 4/4 without an extra NOMINMAX override. Keep the initial fixture
API/iterator failures, shell-wildcard zero-test run and the two incorrect native
contract expectations, corrected without modifying native behavior or thresholds.
The [request review](../reviews/aad-calibration-risk-request.md) approves this
bounded increment: exact `ea8973b1174d10814cee9bd510016258e3d8e824` passes all 35
checks in `aad-calibration-request-ci-06.jsonl`. Keep the empty failed TLS capture
`ci-04.jsonl`; it is not a valid zero-check inventory. The [cost report](../performance/aad-calibration-risk-request.md)
records all six new rows across 20 processes, 480 numeric cells and 1,061
unchanged inputs. Nine old gate executables, the core archive and all 18 old
public object members remain byte-identical to accepted common C++ after rebuild;
the public archive adds only the new request object. This is bounded identity
evidence, not a fresh whole-PR/master timing gate.

The [automatic Dupire request contract](../specs/aad-dupire-risk-request.md),
[API](../api-notes/aad-dupire-risk-request.md) and
[critique](../critiques/aad-dupire-risk-request.md) control the accepted C++ increment.
Its owning plan seals exact native flat-rate Hybrid/product/settings data,
including nested surface/correlation values, and shares actual native axis/layout
checks. All required surface inputs precede explicit direct constant bindings;
quote selection changes only quote projections. External direct inputs retain
native quote-only identity and exclude bindings. Preflight counts one scalar
value, all mandatory valuation derivatives and three full quote matrices.
Execution preserves native mean normalization, estimator and fixed-calibration
provenance, with explicit expired/mixed method labels. Results do not retain
the execution plan's extra direct seed arrays; getters remain detached/passive.

Seventeen new request cases and two automatic flat/Merton oracle cases pass
with the original 50 related cases. The new oracle contributes 84 independent
full bump/recalibration/common-path price rows with unchanged original steps,
tolerances and adjacent-step rules. Fresh regular CTest passes 2,457 OFF /
2,471 combined; leak-enabled fully instrumented ASan/UBSan passes 69 related
cases; actual MSVC syntax passes 12 OFF/combined units without an extra
NOMINMAX override. Installed public consumers pass 128 processes across both
configurations. Preserve wrong-path/fixture/custom-archive/reduction failures
and corrections in the [review](../reviews/aad-dupire-risk-request.md).

All 64 changed legacy installed-entry rows pass the unchanged paired 4% policy:
two rounds/ten alternating samples, with 64 additional new-cost rows, 3,840
processes and 1,578 unchanged source/install/build inputs. Nine accepted gate
binaries and 17 unaffected public objects remain byte-identical. The core
archive and two existing public objects change because passive helpers are
added; their affected entries are measured rather than accepted by analogy.
The [performance report](../performance/aad-dupire-risk-request.md) retains every
row and the corrected member inventory. This does not close whole-PR P01.
All 35 exact `e42c483551cd252fc95a7574b83b562ee6abb3ee` checks pass in
`aad-dupire-request-ci-07.jsonl`, including Windows runtime. Preserve the failed
empty TLS captures and their errors. The remote user's dataset-only merge of
master `2fc748ef` was retained when rebasing the two unpublished local commits;
all eighteen staged implementation/document files stayed identical, and the
result was pushed without force. Strict Python/Excel request parity remains
required. Full F01 boxes and the authorized PR-to-merge gates remain open.

The [Python request contract](../specs/aad-risk-request-python.md),
[API](../api-notes/aad-risk-request-python.md) and
[critique](../critiques/aad-risk-request-python.md) control common then automatic
bindings. Common quote requests are implemented locally with shared strict
scalar parsing, owned readonly plans/results, detached native metadata and
callback-free GIL release. The first missing-interface test fails, then passes.
All 52 new cases and 82 related accepted cases pass; workspace Python passes
971, and both fresh installed OFF/combined modules pass 970 with the one existing
opaque-curve test-helper absence skipped. Each of three modules matches fifteen
full/subset/empty installed C++ request cases, every coordinate and all seven
matrices exactly. All four curve providers and both inverse modes are exercised
by the Python tests. Thirty-six old scalar parser cases preserve their stable
diagnostics and validation timing; shared-parser CCN is at most eight. Preserve
the first formal performance failure: a shared-parser context build makes one
old selected request +4.56%/+5.97%. Fixed field/identifier labels keep one shared
parser and all checks/messages. All functional/install/parity checks are repeated;
the complete second paired run passes all 63 old rows and thirteen separate new
cost rows across 100 processes and 747 unchanged input hashes. The formerly
failing row is -0.42%/-1.19%; thresholds, samples, work and assertions stay.
The [review](../reviews/aad-risk-request-python.md) and
[performance report](../performance/aad-risk-request-python.md) retain evidence.
Own-head publication checks remain pending.
Automatic Python requests are now implemented and locally verified. They expose
owned strict typed configuration, required axes and detached native plans/results;
planning/execution releases the GIL after input copies. The new suite passes 69
workspace tests, and affected old scalar/common suites pass 72. Installed
OFF/combined modules cover all new cases through focused plus corrected-case
runs; all three modules match 24 independent installed C++ cases exactly,
including every coordinate and seven matrices. Preserve the initial fixture
errors and concurrent quote failures: the pre-binding module reproduces quote
roundoff up to 8.38e-9 from surface-seed differences of 1.78e-15. Strict quote
parity stays at rel/abs 1e-10 with two calling threads and one native worker;
four-worker PV/surface parity also stays at rel/abs 1e-10. Native mathematics,
normalization and original independent oracle steps/tolerances do not change.
All nine affected existing constructor costs pass the unchanged paired policy
with 704 unchanged input hashes and eight separate new cost rows. See the linked
review/performance report for complete evidence. The next publication is
consolidated with Excel request parity; no incremental full suites are repeated.
Common `d51df72` now passes all 35 own-head checks, captured in
`aad-risk-request-excel-pr-state-02.json`. Excel adds 24 request/plan/result
functions reusing native plans, immutable storage and passive projections.
The final affected OFF/combined suites each pass 15 tests, including a 24-case
flat/Merton/tree/compiled/selection/direct parity matrix and all four captured
curve providers. Twelve initial MSVC units and six final affected production
units pass syntax checks; real Windows runtime remains a publication gate.
All ten changed existing Excel cost rows pass the original paired policy after
correcting inlined numeric formatting and temporary history-header strings.
The [Excel review](../reviews/aad-risk-request-excel.md) and
[complete cost report](../performance/aad-risk-request-excel.md) retain failures,
scope and final evidence. Python and Excel publish together; full F01 remains
open until own-head CI and the requirement audit pass.

### Whole-plan status and remaining effort

PR #480 is merged at `079c9d520db17dde7017b329aff0ddc4c6a22afb` on
2026-10-06 (Asia/Shanghai). Final head `38eaceac` passes all 35 checks, including
Linux/Windows gates, wheels, sanitizers and Codacy (zero issues/annotations).
Both paginated review threads are resolved; the additional overview file-header
nit is fixed. Repeated completion audits verify master/base, all check/status
pages, branch policy and review state. The guarded squash merge is confirmed
closed/merged, and its tree equals the accepted head tree. GitHub Actions runner
allocation failures and the lost Windows host remain retained; failed-only
retries reused successful same-head jobs. No accepted full local suites were
repeated for these failures.

F02 starts from that merged tree on `feature/aad-weighted-script-risk` under the
[weighted specification](../specs/aad-weighted-script-risk.md). Its initial native
root increment does not close weighted valuation, blocked Jacobians or portfolio
integration. Evidence: `aad-pr-480-completion-head38-{initial,final}-01/validated.json`,
`aad-pr-480-after-merge-01.json` and `aad-pr-480-merged-tree-01.json`.

F02's additive native preflight now owns selected/full scalar output identities,
normalized passive weights, input choices, date/method and the exact
`sizeof(double) * (1+n+2*k)` payload. It reuses scalar input/report checks and
rechecks prepared axes without changing scalar implementation or tape layout.
Twenty focused tests (ten new plan and ten existing scalar-result cases) pass
OFF and combined diagnostic/profiling + ASan/UBSan. The CI's existing GCC 14
warning policy passes; all four ASan/UBSan filters now select the seventeen new
weighted/plan tests and the related old scalar-root test. See the
[preflight review](../reviews/aad-weighted-preflight.md).

Prepared weighted C++ execution and owning results are now implemented in draft
PR #483. Compile-time objective policies share double/AAD drivers while keeping
scalar entries free of weighted buffers; one suffix reverse runs per path and
one prefix reverse per batch. Eight weighted and eight existing risk tests pass
OFF; seventeen pass with diagnostics/profiling plus ASan/UBSan, including an
independent sweep-count fixture. Historical aliases, passive result ownership,
callback mutation, task-drain recovery and common-path component references are
covered. See the [execution review](../reviews/aad-weighted-execution.md).
Python now supplies keyword-only weighted requests, an output-axis query,
read-only owning results and GIL release after copying native inputs. The frozen
spot difference passes at step 0.1 and `1e-10` tolerances in its first run.
179 related Python cases pass OFF and combined diagnostics/profiling, covering
the new boundary and old scalar/calibration/Dupire request parsers. Standalone
installed-prefix configuration passes in both modes. See the
[Python review](../reviews/aad-weighted-python.md), including retained support
import failures and the explicit initialization repair for isolated CI tests.
Excel now supplies immutable weighted handles, ordered component tables,
gradients/shapes and retained snapshot getters. Ten weighted cases and the old
scalar/calibration/Dupire consumers pass: 25 relevant tests in each OFF/combined
mode, with focused ASan/UBSan in combined. Twelve registrations generate 24
files without drift; numeric/boolean/range guards precede corresponding
generated conversions. See the [Excel review](../reviews/aad-weighted-excel.md).
At `eb2be051`, all four Windows/raw configurations and 35 CI/Codacy checks
pass, with zero Codacy issues/annotations. All 52 affected scalar performance
cases pass the unchanged paired policy; 1/4/16/64-output costs and diagnostic
census are retained separately. Copilot then identifies weighted projection
identity and integer Excel conversion repairs. Corrective head `9e24cc77`
passes all 35 CI/Codacy checks, all four Windows integer-export regressions,
both repeated paginated review audits and the affected eight-case scalar gate.
Relinked production MC/GSR/LSM executable equality preserves its 44-case result.
All four Copilot threads are fixed/resolved; #483 is merged at `d1600a15`, with
the accepted tree verified against master. The new blocked-Jacobian branch
starts from that merge, with passive block planning as its first increment.
The next native increment adds aggregate cached-tape admission and block
allocation tickets, transactional clear with overlap accounting, and bounded
vector roots with explicit tail seed clearing. Fifty focused cases pass per
OFF/combined ASan/UBSan configuration; nine new capacity/root cases also pass
combined-diagnostic TSan. The full producer and its performance/consumer gates
remain open; see the [capacity/root review](../reviews/aad-blocked-capacity-roots.md).
Scratch allocation now admits actual vector/matrix/nested/packed-boolean capacity
and heap model/component payload before allocation, including old/new overlap.
Ninety-four focused cases pass in OFF, combined ASan/UBSan and combined TSan.
The fresh OFF core/public shared build and all three installed consumers pass;
the buffer consumer checks caller-scope/library-origin allocation enforcement.
These are local increment checks, not full producer or performance acceptance;
see the [scratch review](../reviews/aad-blocked-scratch-capacity.md).
The native batch collector now executes ordered output lanes through the shared
prepared-script loop. Four new cases cover 1/4/16/64 common-path scalar row
oracles, prefix aliases/direct inputs/constants, padding, scratch scope sharing
and early unsupported-product rejection. A fresh matching OFF core/public build
passes 20 blocked/scalar/weighted integration cases. Request-level scheduling,
owning result/consumers and full performance/CI acceptance remain open; see the
[batch review](../reviews/aad-blocked-batch-execution.md).
Cold worker tape construction now admits each initial block before allocation
and refunds partial construction. Budgeted batches reserve cleanup headroom
throughout execution, including simultaneous worker replacements. Twenty-three
capacity/recording cases pass OFF, combined ASan/UBSan and combined TSan;
11 storage cases pass OFF/combined. The internal request driver now copies
selections/settings, replays complete common paths, drains failed submissions,
reduces owning raw matrices and reports widths/work/capacity peaks. Five OFF
cases verify 1/4/16/64 ordered outputs against scalar rows and weighted `J^T w`
in one/four-worker tree/compiled execution, budget failures/recovery and native
empty columns. Public axes/provenance, full snapshot/minimum preflight, consumer
and performance/platform acceptance remain open; see the
[request replay review](../reviews/aad-blocked-request-replay.md).
The preceding paragraphs record intermediate F02 acceptance. F02 is now complete:
blocked Jacobians and recording budgets merged in #484, and portfolios merged
in #487. Their final acceptance appears in the current status table below.

This snapshot distinguishes accepted increments from locally implemented work.
Estimates are remaining single-developer effort, not promises of calendar time;
overlapping acceptance work is included once in the integration allowance.

| Work item               | Implementation/local verification                                     | Publication/CI                           | Remaining person-days |
|-------------------------|-----------------------------------------------------------------------|------------------------------------------|-----------------------|
| C01–C05, D00–D03        | Accepted native correctness/lifecycle/removal                         | Accepted exact-head checks               | 0                     |
| P01                     | Tooling/resources/scaling and final 44/44 MC confirmation pass        | Merged; final 35/35 checks accepted      | 0                     |
| F01                     | C++/Python/Excel and complete requirement audit accepted              | Merged; final 35/35 checks accepted      | 0                     |
| F02 weighted/blocked    | C++/Python/Excel and independent mathematical oracles accepted        | #483/#484 merged; exact-head gates pass  | 0                     |
| F02 portfolio           | C++/Python/Excel, installed consumers and timing repair accepted      | #487 merged; final 35/35 checks accepted | 0                     |
| P02                     | Worker reuse: 82 tests and 37 full-request cost cases accepted        | #488 merged; final 35/35 checks accepted | 0                     |
| P03                     | 89 tests; 81 unique complete-request timing comparisons accepted      | #489 merged; final 35/35 checks accepted | 0                     |
| F03                     | Solve/root/PDE operators and financial/grid acceptance complete       | #506 merged; final 36/36 gates accepted  | 0                     |
| P04                     | Structural compression, safe fallback and selection accepted          | #511 merged; final 36/36 gates accepted  | 0                     |
| P05                     | Core, fixed-path and Monte Carlo adapters accepted                    | #512/#513/#515 merged; 36/36 gates       | 0                     |
| F04                     | Finite-step accepted; smooth prototype passes local correctness/costs | Publication remains                      | 0.75–1.5              |
| Final integration/audit | Cross-platform/binding/docs/performance acceptance remains            | Open                                     | 7–11                  |

Remaining total during F04 publication: 62–100 hours, approximately
8–13 eight-hour person-days, excluding CI queue time. Delivery contingency
is included in those same hour ranges; there is no extra undisclosed allowance. F01 is merged;
native algorithms and independent mathematical acceptance already exist. This is a rough
effort estimate, not a guaranteed completion date. Re-estimate when review
findings change the scope.

### Next delivery tasks and effort

One person-day means eight hours. The following detail subdivides the total
above. The accepted #490/#491/#493/#494/#495/#496/#497/#498/#499/#500 deliveries contribute zero remaining
hours. Structured coordinates are split into numeric and native PRs; completed
numeric/native diagnostics are not counted again.

| Order | Delivery task                        | Required result                                                                        | Remaining hours |
|-------|--------------------------------------|----------------------------------------------------------------------------------------|-----------------|
| 1     | Accepted solve/root PRs through #501 | Merged; mathematical, resource, platform and scoped performance gates accepted         | 0               |
| 2     | F03 numeric coordinates              | Accepted in merged #495; immutable accepted evidence retained                          | 0               |
| 3     | F03 native coordinates               | Accepted in merged #496; immutable accepted evidence retained                          | 0               |
| 4     | F03 native coordinate accuracy       | Accepted in merged #500; exact-head CI/review and guarded merge pass                   | 0               |
| 4a    | F03 implicit calibration             | Numeric #501 and native #502 merged; all final gates accepted                          | 0               |
| 5     | F03 PDE operators                    | #504/#505/#506 merged; financial/grid and all publication gates accepted               | 0               |
| 6     | P04 structural sparsity              | #507–#511 merged; implementation, scoped cost and publication gates accepted           | 0               |
| 7a    | P05 segmentation core                | #512 merged; actual runtime, exact-head CI/review and guarded merge accepted           | 0               |
| 7b    | P05 financial adapter                | #513 merged; exact-head CI, review, costs, actual runtime and merge accepted           | 0               |
| 7c    | P05 Monte Carlo acceptance           | #515 merged; 36/36 gates, fourteen actual runtime profiles and merge proof             | 0               |
| 8     | F04 second-order risk                | #517–#527 increments merged; finite-step mathematical and publication gates pass       | 0               |
| 9     | F04 native mixed-mode prototype      | Local implementation/analytic/capability/cost acceptance complete; publication remains | 6–12            |
| 10    | Bindings and complete acceptance     | C++/Python/Excel, documentation, platform and performance gates                        | 56–88           |
| Total | Full remaining implementation        | All remaining plan requirements                                                        | 62–100          |


The first delivery boundary is complete. Whole-PR review repaired the Windows
registration-help defect, both include-order findings and the overview file
header. See the [whole-PR review](../reviews/aad-pr-480-final.md). Subsequent
stages use new PRs and retain their own publication and acceptance gates.

### Earlier snapshots retained for acceptance context

P01's controlling [production measurement contract](../specs/aad-production-measurement.md)
now specifies actual phase/resource windows, default instrumentation exclusion,
fixed-path thread scaling, long-path/surface/real-output coverage and complete
numeric/no-regression acceptance. In local unpublished work, profiling package
configuration first fails, then all three OFF/ON/combined settings pass. Native
scoped timing, explicit tape high-water samples, actual successful block-allocation
events and task-owned collectors are implemented. Focused tests first expose absent
interfaces and an incorrectly complete unexecuted task, then pass after correction.
Ordinary passive/AAD and LSM training/regression/replay call sites are instrumented;
the earlier 15 focused tests pass, including passive/failure handling and strict
LSM price/every-risk bit comparisons. Fresh full OFF/ON builds pass 2,332/2,346
CTest cases, each with 793 passing Python tests, and both installed consumers pass
in both configurations. Regeneration has zero drift after staging the three new
generated outputs with their markup; the initial untracked-output rejection is
retained. Default Release has no profiling allocation/tape/span/clock/collector hook symbols
or references. Number/node/tape/recording sizes remain 16/40/368/72 bytes; the
task-group size remains 48 bytes OFF and is 56 bytes ON. This supports layout and
instrumentation exclusion, not a throughput claim. The combined lifetime/profiling
full build passes 2,375 CTest cases, including 793 Python tests, and both installed
consumers pass. The 1,179 compiled/test input hashes still match the final build
manifest for that earlier snapshot. The 28 published-head checks at `6161e5a` all pass and do not cover
these unpublished changes. P01 remains open until workload/resource/scaling,
diagnostic-cost, default-OFF performance, complete feature and new CI acceptance.

The latest unpublished increment implements setup-prefix samples, selected
array payload/capacity, same-request self thread CPU, and lazy memory callbacks
that skip scans outside an explicit measurement scope. Missing interfaces and
prefix samples first fail; the fully rebuilt ON library passes all 20 focused
tests. The production CLI supports five scenarios, cold/warm/phases, expanded
surfaces, and 1/4/16/64 distinct outputs with explicit scalar-output execution.
Independent short-BS analytic price/four-risk checks preserve the 64-output
kink failure evidence rather than weakening tolerance. The permanent CLI
contract and 38 representative functional cases pass. Current ON passes all
2,351 non-benchmark tests, including 793 Python tests; the command also ran
21 benchmarks under simultaneous builds and returned failure for the existing
rate-risk overhead timing assertion. Keep that failure and perform quiet serial
revalidation followed by the formal paired gate. Latest OFF/combined rebuilds,
consumers and erasure proofs have since passed: OFF 2,332 complete functionality
checks, two installed consumers in each of all three configurations, refreshed
Release symbols/layouts, and an O0 Debug call-erasure probe. Combined passes
20 focused cases and all 2,380 full-suite projects, including 793 Python tests
and the slow European MC example. Quiet serial ON benchmark smoke passes all
21 targets, including the initial parallel timing failure; both logs remain.
Sixteen focused profiler ASan/UBSan cases pass;
the supporting Release archive is not fully instrumented, so full sanitizer
coverage remains the new CI's responsibility. Resource/scaling/overhead,
formal default-OFF performance and new-head CI are pending. Full details and
raw log paths are in the production contract.

Published profiling source `5a6382b` passes the frozen original-baseline formal
nine-target gate: all 65 comparable cases pass the unchanged two-by-ten/4%
policy, ten head-only cases remain informational, and Sobol precise/fast is
9.38x below 10x. Raw results are `production-profiling-paired-01/` under the
evidence root. This does not complete affected production, scaling, resources
or diagnostic-cost acceptance. Its first Codacy check reports 26 new checker/
consumer issues. A local test-tool refactor keeps every numeric case/tolerance,
uses always-on unittest checks and validated shell-free build-tree execution,
passes ON/OFF-optimized contracts and all installed consumers, and measures
maximum complexity 8/4. No library/benchmark source changes in this corrective
increment. Merge b37b49f integrates master's 3fe44ecd documentation and resolves
the Copilot conflict while retaining source/docs-only classification and the
native-only matrix. Production/build/benchmark inputs are unchanged by that
documentation merge. Its Codacy check succeeds and Actions actually run; two
MSVC profiling-ON legs fail because windows.h's VOID macro replaces the generated
StackInfoType_ enum value. Clear the macro locally after the private Windows
include. Preserve both failing job logs; exact corrected-head MSVC compilation,
runtime and all applicable CI checks remain required.

The same head's default Windows configuration passes 2,311 functional tests,
then fails the CLI step's second CMake configure: vendored pybind11 caches FOUND
without reloading its commands. An independent installed-Python consumer
reproduces first-configure success and second-configure failure. Change the
three discovery guards to actual command availability; two corrected configure
passes succeed without changing lookup order or skipping the existing Windows
CLI reconfiguration. Python 3.12 vendored standalone binding configures twice,
builds and passes 792 tests with one skip for its absent monorepo-only native
test module. Refreshed default/profiling/combined core builds pass; profiling/
combined each pass 20 focused cases and default/profiling six CLI contracts each.
Exact corrected-head Windows CI remains open. Both CI and local RED logs are
retained.

The corrected Windows lifetime leg loads pybind11 successfully, then exposes a
second CLI compile defect: MSVC tries to copy a noncopyable ScriptProductData_
through the conditional expression. Use direct prvalue returns in a local
lambda; keep product ownership and compiler flags unchanged. Preserve that RED
job log and require fresh MSVC compilation.
The corrected CLI rebuilds in OFF/ON, passes six contracts in each and all 38
representative diagnostics again. Formatting and docs checks pass. No core
algorithm changes require repeating otherwise-unaffected library tests locally;
the new exact-head Windows compile/run coverage remains mandatory.

The frozen 7ead7ecd default-OFF nine gated binaries, configuration rows and gate
script match the passing 5a6382b inputs exactly. Its 44-case supplemental MC
pairing passes all cases at two-by-ten; all paired LSM prices/risks agree. The
GSR 1F bond's +5.5307%/+3.9994% movement remains visible. The curve supplement
passes 24/25, with a 24-node PWL DF query failure at +6.58%/+4.29%. Preserve this
initial failure. Before further sampling, freeze a complete 25-case prior-native
8816951 control at two-by-ten and complete original-baseline confirmation at
two-by-thirty, retaining the same 4%/min/interleaving rule. Persistent movement
requires diagnosis; this increment is not accepted from a partial performance
pass or an older green CI snapshot.

The native-only development source is `/tmp/dal-aad-backend-adapter`. External
implementations, three gitlinks, build/export paths and CI jobs are removed;
native operation services use `native.hpp` without adapter inheritance or selection.
Fresh OFF/ON full builds succeed with external dependency directories absent.
Final full CTest passes 2,330 OFF and 2,359 ON cases, including 793 Python tests
each and all 34 examples. Both final prefixes pass the two installed consumers.
Configuration/header migration checks, generation/drift and 60 focused ASan/UBSan
tests pass. The AAD example matches an independent price/five-partial oracle;
the corrected vanilla example matches price and six partials. Preserve its
initial diagnostic failure and include-order build failure logs.

Frozen native-only head `8816951662fc6788f3e9876c45ac72f7e48c65f7` passes
the unchanged nine-target gate (65 comparable cases; ten new informational rows),
25 curve cases and all 44 production cases in the predefined two-by-thirty
confirmation. The initial two-by-ten production run fails GSR 1F swaption
(+5.75%/+6.84%) and four-node AAD market fit (+4.46%/+5.13%); retain both.
The published-head native control passes all 44 cases. Original-baseline fixed
confirmation gives -1.30%/-3.63% and +3.52%/+5.24%, respectively. The latter
remains a borderline positive cost; passing the two-round rule is not a claim
of zero runtime overhead. Three resource pairs have overlapping RSS ranges,
zero major faults and equal daily-LSM results. Source/configuration/five pins,
all 22 binaries and helpers are unchanged through measurement.
The [native-only report](../perf/aad-native-only.md) records every case, initial
failures, control provenance/annotation correction, fixed confirmation and limits.
At publication `b42f9eb9c12a5987e8a81a869f59b38d8096a1de`, all 28 exact-head
checks pass, including complete diagnostic sanitizers, native compilers, both
stable gates, MSVC/consumers and all wheel platforms. The
[native contract audit](../reviews/aad-native-contract-audit.md) verifies D00,
D01, D02 and D03 requirements. The library/build/benchmark diff from measured
`8816951` is empty and all 22 binary digests still match.
D01's initial numeric-coverage gap is filled by the
[production comparison](../perf/aad-recording-numeric-validation.md): all ten
ordinary MC and 25 curve cases, 34,028 numeric cells, pass rel/abs 1e-10, including
complete risk vectors, residuals and Jacobian/inverse matrices. The existing
three LSM profiles also agree in every paired sample. Remaining P01
phase/resource/scaling measurement and all later
stages remain required.

Protected guidance sync item: `CLAUDE.md:35` still lists legacy AAD selection
options. AGENTS.md forbids editing Claude originals without explicit user
authorization. Editable current-state docs and Codex contracts are updated;
this protected line remains an identified follow-up.

Isolated sources: `/tmp/dal-aad-baseline` and `/tmp/dal-aad-implementation`.
Evidence/build root: `/tmp/dal-aad-evidence`; dependencies are checked out at baseline gitlink SHAs.
Full native baseline/head Release builds use matching benchmark-enabled configuration.

The locally committed lifecycle increment `8f4f09c` passes 2,336 native and 2,255
CoDiPack CTest cases and all 20 native scoped-recording cases under ASan/UBSan.
Its fresh nine-target pairing passes. Supplemental ordinary MC detects genuine
short-path regressions: vanilla AAD tree +15.00%/+10.74%, compiled +11.25%/+15.08%.
Long weekly AAD and BS/local-vol LSM profiles pass; all supplemental LSM PV/risk
entries agree with baseline at relative/absolute tolerance 1e-10. Raw evidence is
retained in `lifecycle-paired/` and `lifecycle-mc-paired/`.

Corrected C++ head `71f41a81e4d939b192a986199104019bbbe9230c` inlines existing
checked operations and builds errors out of line. It passes 2,315 native functional/
example cases, all 21 quiet serial benchmark smoke checks, 2,255 CoDiPack cases,
and twenty native scope cases under ASan/UBSan. Its fresh nine-target gate, all
35 ordinary MC cases, three LSM profiles, and 25 calibration cases pass. A second
complete MC/LSM pairing confirms the short vanilla results; all four rounds remain.
Matching four-thread RSS observations show no material increase. The
[lifecycle acceptance report](../perf/aad-recording-lifecycle.md) contains full
tables, conditions, initial failures, and remaining coverage limits. Publication-
head four-backend/Windows/binding CI remains required; the prior ownership head's
green CI does not validate this increment.

At first publication head `ca977e2`, Codacy reports complexity 10 (limit 8) in
the combined scalar/vector full-block test. Split the two cases with shared
graph setup and preserve every assertion. All 21 native/fifteen CoDiPack cases
and all 21 native ASan/UBSan cases pass locally after this test-only correction;
production source and all measured nine-target binaries remain unchanged.
Corrected publication head `0ee84e1d06aed29e93c21639953dd8ff65314828` now passes
all 46 exact-head CI checks, including static analysis, sanitizers, the four-
backend compiler matrix and Windows/Python/Excel jobs. The audit is retained in
`recording-lifecycle-0ee84e1-ci.jsonl`. This validates the published D01 increment,
not subsequent local changes; the final D01 requirement audit still precedes its
ledger check-off.

Current D02 work adds the default-OFF native lifetime option, PUBLIC/exported
ABI definition and incompatible-backend rejection. An ignored-option oracle
first fails; the native export and all three external-backend rejection checks
now pass. Three native stale-handle/assignment regressions first fail; all 23
initial diagnostic cases pass ASan/UBSan, including full reset, suffix/generation ABA,
foreign/exited threads, saved expressions, scalar/vector modes, count/epoch/
identity exhaustion and recovery after partial allocation. The added model-copy
case also passes: 24 diagnostic cases now pass ASan/UBSan, and 45 lifetime/
scoped cases pass the full ON library. Native ON core/public/portable Excel CTest
passes 2,307 cases, followed by the additional model-copy check; Python ON passes
792 tests with one skip. ON and OFF installed consumers verify ABI propagation,
analytic gradients and supported lifetime behavior. Default native CTest passes
2,318 functional/example cases; CoDiPack passes 2,257. OFF number/node/tape size
and alignment match the original baseline (16/40/368 bytes on this host), and
diagnostic functions are absent from its tape object. All 21 quiet serial OFF
benchmark smoke checks pass. Final include-order rebuild passes 71 ON and 47 OFF
focused cases. At immutable implementation head `c83bcc9`, the fresh nine-target
gate passes all 65 comparable cases; 35 ordinary MC cases, three LSM profiles,
and 25 calibration cases also pass. Every LSM paired PV/risk agrees at relative/
absolute tolerance 1e-10. Three four-thread RSS pairs have overlapping ranges
and no major faults. The [D02 acceptance report](../perf/aad-native-lifetime-diagnostics.md)
records all rows and limits. Current publication-head CI and the final requirement
audit remain open; smoke timing is not paired acceptance.
Post-pairing review reproduces two independent-rebinding failures and one
expression-assignment failure under counter exhaustion. Diagnostic assignment
now commits primal/handle only after successful allocation/materialization;
default OFF assignment bodies are unchanged. All 28 diagnostic cases now pass
ASan/UBSan, including actual scoped-registration failure/recovery. Corrected
complete ON/OFF builds now pass: ON CTest has 2,313 cases including Python,
and OFF has 2,318 functional/example cases. Evidence is retained in
`lifetime-assignment-{on,off}-{build,ctest}.log`. Fresh default OFF pairing is
recorded below; current publication CI remains required before accepting the
corrected increment. Keep the earlier `c83bcc9` data.

At frozen corrected head `8e1ef0941a949059858aa78cab46af4cb50b41e6`, fresh
nine-target pairing passes all 65 comparable cases; ten new rows remain
informational. All 35 ordinary MC cases and three LSM profiles pass, and all
LSM PV/risk entries match. Initial supplemental calibration fails its 8-node
query (+8.73%/+5.61%); retain the failure. With unchanged sources and binaries,
independent two-by-ten confirmation and fixed two-by-thirty stability checks
pass all 25 cases. The query becomes -0.75%/-2.39% and +1.54%/-1.32%,
respectively. A same-binary control is retained. This supports measurement
variation rather than a sustained regression, without establishing a specific
hardware cause. RSS ranges overlap and major faults are zero. Every measured
source/configuration/dependency/helper/binary identity is unchanged afterward.
The [corrected D02 report](../perf/aad-native-lifetime-diagnostics-corrected.md)
records every row, initial failure, confirmation and limitation. Exact-head CI,
the D02 requirement audit and D03 capability reporting remain required.

Direct D01 result-extraction failure coverage now completes a scoped reverse,
reads its analytic price/gradient, throws during extraction, preserves that
exception and proves the next independent scoped graph gives its analytic
result. Native ON/OFF focused runs pass 76/48 cases. The new test and include
ordering change no production or benchmark source. The complete CoDiPack
rebuild/CTest passes 2,258 cases, including this recovery test.
Publication head `9b5febcca79643e817999e76e8f5dda44922d618` now passes all
49 exact-head CI checks, including the new complete native diagnostics ON/OFF,
diagnostic sanitizer and Windows/consumer legs. Checked PR head and every
paginated check are retained in `lifetime-9b5febc-{pr-state,ci}` evidence.
The final D01/D02 requirement audit remains open.
The independent full-final-block `BlockList::Size` repair has its
bounded timeout RED and eleven-case ASan/UBSan GREEN evidence; it also awaits
final paired/CI acceptance; its full native/CoDiPack functional checks pass.

D03's pre-removal work is in `/tmp/dal-aad-backend-adapter`, based on the
unchanged successful publication head, with local commits `2b4bf89` and `5689ff9`.
Full native OFF/ON CTest passes 2,328/2,324 cases; XAD, CoDiPack and Adept each
pass 2,264. All five installed consumers pass; diagnostic ON passes 60 focused
ASan/UBSan cases with leak detection, and native OFF passes 21 serial benchmark
smoke cases. These are terminal local results, not publication or paired
performance acceptance. The compile/UBSan RED failures remain recorded.
The amended [native operation contract](../specs/aad-backend-adapter.md) preserves
useful seed/channel/lifecycle behavior while removing external adapters under
D00. This pre-removal evidence did not establish native-only acceptance; the
fresh removal pairing is now recorded above and publication CI remains open.

At implementation commit `247c7aefab0f54af788b8a44f9dd71e606562c8a`, the fresh full native/core/public/
portable-Excel and non-slow example run passed 2,286 cases. The initial nine-target paired gate
found approximately 50% regressions in vector propagation and the small Jacobian harvest;
these failures are retained in `p0-paired-initial/` and are not waived.

The corrective kernel uses exact IEEE magnitude-bit classification (portable comparison fallback),
continuous vector arithmetic for finite derivatives, and an out-of-line non-finite path that skips
zero channels. Mode and common widths are selected once per sweep; other widths use the same
semantics through the dynamic loop. No per-node data members or approximate thresholds are added.

Nine focused native propagation tests pass, including new scalar/vector subnormal regressions,
signed-zero handling, alias accumulation, repeated sweeps, and specialized/fallback widths.
The subnormal cases fail against the original baseline. The corrected kernel's isolated diagnostic
pairing passed both rounds for tape and Jacobian targets; this is not the final nine-target verdict.
Raw evidence is in `width-specialized-paired-diagnostic/` (same CPU affinity for both sides).

The original CoDiPack CI failures exposed its explicit `sqrt(0)` zero-gradient convention rather
than a missing numeric-result check. Retain that boundary test for native, and validate all backends
with a smooth positive-domain payoff whose derivative overflows while its primal remains finite;
the new public test covers interpreted/compiled paths and subsequent recovery.

At `83399b5a31389563a26b7e69d82bfc2b5bd4b68e`, fresh full native and CoDiPack runs passed
2,290 and 2,238 cases respectively. All 46 exact-head CI checks subsequently passed.
Later increments still require their own exact-head CI audit.

The first P01 measurement increment adds explicit native tape snapshots for logical storage,
cursor storage including block padding, and retained block capacity. No recording/propagation
counters or node fields are added. The snapshot's full-block boundary regression first crashed,
then passed after bounding its scan by the known node count; ASan/UBSan passes all 11 focused
statistics/block-list tests. A growth/rewind test distinguishes occupied storage from capacity.

`tape_perf` now retains fresh handles after its Clear/Rewind timings, checks analytic values and
gradients, and covers passive constants and vector widths 1/4/10/16/64. Historical case names
and workloads remain. A vector fixture seeds multiple channels of one output; it does not claim
to exercise distinct portfolio outputs. `jacobian_perf` additionally calls the actual
`HarvestCurveJacobian` for 23-by-24 and 95-by-96 matrices and checks every entry analytically.
Diagnostic scans are enabled explicitly with `--diagnostics`; default timing runs omit them.

With the measurement increment, full native and CoDiPack runs pass 2,293 and 2,239 cases.
Both changed benchmark executables pass their result checks. Native statistics are not an RSS
budget, cumulative allocation counter, or automatically sampled high-water mark. MC phase
attribution, worker scaling, and the production multi-output cases remain P01 work.

The first full gate with P01 coverage (`e64a2690e7d4ee2ce6cc857aad5929325ea7a409`)
passed every existing case except the small dense reference Jacobian (+6.52%/+6.80%).
Raw evidence remains in `p0-paired-final/`. The same current core library with the original
benchmark fixture passed both rows, localizing the difference to the expanded fixture.
Separating the established timed fixture from command parsing/additional cases passed both
Jacobian rows (-4.83%/-4.41% dense and -4.30%/-4.49% prefix). Result checks, work, case names,
sample counts, and thresholds remain. Graph construction is separately factored, and production
prefix widths are selected before timing rather than copied in each measured harvest.
This also corrects Codacy's two new complexity findings without relaxing its limit.

The fresh nine-target gate at `13b0964b870af232ff4bed745835ac076249ded8` passes every
performance acceptance check. Raw evidence is in `p0-paired-verified/`, including immutable
source SHAs, compiler/cache/dependency metadata, CPU affinity, and binary SHA-256 digests.
Both small Jacobian rows improve in both rounds; tape costs remain within the unchanged policy.
The [performance report](../perf/aad-native-stage-a.md) preserves every case and the coverage
limits. C01-C05 are verified by their mathematical tests, the full core-repair CI audit, and
this performance gate; new increments still require their own current-head checks.

The [recording lifecycle contract](../specs/aad-recording-lifecycle.md) specifies D01 ownership,
states, checkpoint validation, and cleanup/recovery acceptance. The independent ownership
increment implements thread-affine `RecordingScope_`, scoped nesting rejection, explicit
close, noexcept fallback, cleanup-failure retention, and recovery before the next recording.
`TapeGuard_` delegates ownership to it; curve Jacobian, node-risk, and GSR/SLV Jacobian callers
close explicitly after passive extraction. Raw graph operations remain unchanged.

Nine native and eight CoDiPack ownership tests pass, including nesting during entry/exit
reset callbacks, exception recovery, foreign-thread rejection, idempotent closure, and
native vector-capacity retention. The nested-guard and entry-reset tests first failed.
ASan/UBSan passes all nine native cases. Fresh full native CTest passes 2,324 cases
(core/public/portable Excel, 33 regular examples, one slow example, and 21 benchmark smoke
tests); full CoDiPack CTest passes 2,247. Logs are retained under `recording-*-ctest.log`
and `recording-owner-sanitized.log`. Benchmark smoke tests are not paired performance proof.

All 46 exact-head CI checks passed at `aec6689ae6d029f9a7eb4f810e3e074fd4ed6030`,
before the ownership increment. Its fresh nine-target paired gate passes at
`080d16e0047798766116d1dba34b03ff8d860815`; supplemental end-to-end calibration also
passes all 25 common cases under the same sampling/confirmation criterion. The
[ownership acceptance report](../perf/aad-recording-ownership.md) records conditions,
case movements, raw evidence, and coverage limits. All 46 exact-head checks pass
at `bf89c6b66cf0dd2f0253543cf5fc3fcbe02af5d7` for this ownership increment.
This ownership increment covers ownership only; scoped phases, tokens, clearing,
mode boundaries, and MC/LSM migration belong to the following lifecycle increment.

The next local increment implements scoped states, unique validated checkpoint tokens,
native scalar/vector clearing, mode-selection boundaries, reverse-failure recovery,
and ordinary/LSM worker-batch ownership. Twenty native and fifteen CoDiPack focused
recording cases pass. The new nested-MC-batch test first failed, then passed with
ownership before reset; its existing simulation error/recovery test still passes.
Fresh full native and CoDiPack CTest pass 2,336 and 2,255 cases respectively;
twenty native ownership/state cases also pass ASan/UBSan with leak detection.
Logs are `lifecycle-*-ctest.log` and `lifecycle-sanitized.log` under the evidence root.
The first paired run then reveals the short-path failures recorded above. The
corrected head passes fresh correctness and all changed-workload comparisons;
all 46 exact publication-head CI checks subsequently pass at `0ee84e1`.
Do not reuse the ownership snapshot's acceptance as proof for these new changes.

PR #484 is merged after full acceptance at `1785162c`: all 35 exact-head checks,
zero Codacy annotations, zero unresolved review threads, all 160 existing paired
performance cases, and both actual Windows raw-export cases in all four modes.
Its 78 informational width processes independently verify every row; they do not
change the default width or constitute a comparative speedup claim.
The corrected budget fixture isolates its required single worker, and exact
double curve-knot queries avoid transient weights without changing AAD or
non-knot behavior. Existing failed measurements remain retained in delivery
evidence; no thresholds, work counts or numeric oracles were relaxed.

Open #487 now implements sealed C++ ownership, a passive global coordinate
catalog and the internal deterministic compatibility planner. Original handles
establish owners before exact snapshots; same-named constants remain private.
Twenty-one new tests and two existing scalar-risk contract tests pass locally,
including six model families and private historical scalar/vector seeds over
repeated paths in tree/compiled evaluators. The planner retains incompatible
groups and compares complete sample/observation/settings contracts. Foundation
repair head `0fffbf74` passes all 35 CI checks, zero Codacy annotations and zero
unresolved review threads.

Whole-request preparation is now locally implemented. Move-only plans own
private products/models; all trades plan before admission and before one union
fixing snapshot is captured. The producer retains original explicit/global
source metadata and date, completes private trade state and establishes the
sealed-owner/path-count grouping boundary. Fourteen new cases verify caller and
global mutation isolation, late-trade rejection before reads, private historical
state, failure recovery and original meshes. The admission callback is a policy
boundary; actual aggregate capacity policy is still pending. After refactoring
the shared completion epilogue, 61 targeted core cases and six public cases pass,
including old historical replay, startup budgets and LSM prune/reinitialize
branches. Both production units pass strict OFF/combined ON syntax checks.

Shared weighted group batches are now locally implemented. Nine focused cases
pass in tree/compiled evaluation, including independent nonzero-volatility
absolute-path comparisons for Sobol/MRG32 and bridge OFF/ON, private historical
vectors/direct constant aliases, original-trade error context and failure recovery.
The model leaves are registered once, private constants remain separate, actual
counters prove one scenario/suffix reverse per path and one prefix reverse per
batch, and unselected trades are not evaluated. Strict warning categories pass in
OFF/combined ON syntax modes; linked/runtime diagnostic acceptance is pending.
Raw passive batch sums do not constitute whole-portfolio valuation acceptance.
The user moved #487 out of draft; its open review state is preserved.

Preparation repair head `e9931d3a` passes Codacy with zero annotations and has
zero unresolved review threads. Windows CI passes. Linux/Mac checkout failures
remain upstream Eigen download rejection after a failed-only retry; a further
failed-only retry is running. No passing jobs/full local suites were repeated.

Internal weighted replay now accumulates shared model inputs across incompatible
groups and distinct owners, preserving requested output/input order and original
meshes. Seven public-layer internal tests verify every risk against independent
existing calls for all six model families in tree/compiled mode and one/four
workers. Malformed selections submit no tasks; selected derivative overflow,
submission and worker failures retain context/results and permit valid follow-up.
Empty inputs retain native pricing and unselected groups generate no scenarios.
The replay production unit passes strict OFF/combined ON syntax checks.

Persistent upstream Eigen checkout failures now have a CI download fallback at
the unchanged pinned commit. Seven helper cases, 20 workflow-classification and
33 release regressions pass. An isolated real fallback fetch matches the exact
existing commit/tree and normal submodule status. All build/wheel/release/benchmark
workflows retain their existing gates and initialize other dependencies recursively.
New publication-head acceptance must verify the helper on actual CI runners.

At `070bf9d1`, every build and wheel platform completes the new dependency
checkout; strict warnings and three extended diagnostic/profiling configurations
pass. The only Codacy annotation is mixed-mesh test complexity 10/limit 8.
Extracted the independent scalar-reference helper while retaining every execution
setting, coordinate assertion, count and tolerance. Targeted repair verification
and fresh publication-head checks remain separate from the earlier snapshot.

Internal weighted request planning now owns the sealed handle, selected global
coordinates, weights and report factors, defaulting to one payoff per trade.
It enforces the exact checked weighted numeric payload before history/tasks.
Five new cases cover ownership, distinct model ordinals, aliases/zero weights,
native-empty/passive shape and invalid selection/budget rejection. An additive
schema-independent request validator reuses old constraints while preserving
every scalar ordinal-ID check and old function body. Ten existing scalar result
cases also pass; strict OFF/combined ON syntax checks pass for both production units.

Native weighted startup/runtime budgets and sparse-vector holes are implemented
and locally verified. Publication `e90fd8f` passes all 35 checks with zero Codacy
annotations and unresolved review threads. This confirms the capacity increment
without accepting later unpublished changes.

Public C++ weighted requests/results now own component/objective means, raw and
reported gradients, selected/complete global axes and original trade/group
metadata. Report projection rejects overflow before publishing a result. Passive
execution shares original paths with private sharp evaluator/history state,
ignores recording limits and retains zero risk columns. Native-empty inputs keep
fuzzy pricing. Seven public-result cases and one additional passive admission case
extend the existing independent six-family/mixed-mesh oracles to both modes.
Known private vector capacities reject before history. No old driver or scalar
provenance body changes; a small internal helper reuses snapshot capture.
Local review fixes per-trade passive engine labels, report failure context and
aggregate weighted overflow context with retained RED tests. Forty-eight public
increment/legacy and nine core batch cases pass; final context repair receives
targeted verification: all 16 result/batch cases pass after the final repair.
Across this increment, 58 distinct affected cases pass. Strict OFF/combined ON
syntax checks, independent headers, actual documented C++ consumer and all 157
Markdown checks pass. Complete linked/platform/performance acceptance is pending.

The public weighted/passive increment is published at `2338fc7d`. Its first
audit reports one Codacy replay-complexity finding (9/limit 8), with no unresolved
review threads and other platform jobs passing or running. Mode validation is
now extracted with identical guards and execution order; 20 affected cases and
strict OFF/combined ON warnings pass. Fresh repair-head gates remain required.

The Codacy repair head `d2de68d` passes Codacy with no unresolved review threads;
31 of 34 registered checks have passed and three are still running in the latest
exact-head audit. Native blocked group batches now pass 12 targeted cases,
including shared-model/private-constant rows, historical prefix aliases,
absolute path/RNG/bridge comparisons, zero tail lanes, capacity failures and
recording-mode recovery. The production unit passes strict OFF/combined ON
warnings. Public attribution coordination/admission and bindings remain pending.

Owning native portfolio Jacobians now use shared request selection/result metadata,
independent matrix payloads, original-path group blocks and global owner/private
scatter. Known root/matrix/private history shapes admit before history, with
capacity-only width narrowing and aggregate runtime guards. The zero-budget RED
and wrong-report-row RED are repaired. Forty-three distinct affected cases pass,
including all six native families, original mixed meshes/owners and widths
one/two/three across tree/compiled and one/four workers. Strict OFF/combined ON
warnings and independent headers pass. Passive attribution, dedicated narrowing
and failure acceptance, bindings and final delivery gates remain pending.

Passive attribution now retains independent sharp-price rows with zero risk
columns, no native widths/reversals and zero tape peaks, including finite rows
whose unused sum overflows. All 26 affected public cases pass; independent
six-family/mixed-mesh tests cover both attribution modes, and private historical
shapes reject before reads with valid recovery. Strict warning checks pass.
Exact head `4194be4b` exposes a real finite-capacity CI fixture failure caused by
using a previous scheduling-dependent peak as the following quota. The fixture
now uses declared finite per-worker allowances and retains its exact risks and
peak-within-limit assertions. Fresh repair-head checks are required.

Dedicated attribution narrowing now compares requested width two with explicit
width one under identical finite scratch/tape budgets and an independent price
oracle. Work, requested/actual widths and actual peaks agree. Submission/worker
and selected-derivative failures recover; unselected derivatives do not reject.
A retained late-width RED shows earlier tasks were submitted before a later
invalid width; all selected group widths now validate before any task. All 17
affected replay/admission cases and strict OFF/combined ON warnings pass.

Python sealed construction and both typed risk surfaces now pass 49 portfolio
cases, including shared/distinct owners, detached matrices/metadata, strict
requests/path counts, capacity recovery and independent nonzero-volatility
original-mesh native/passive oracles. All 126 affected portfolio/weighted/Jacobian
cases pass through the installed extension; strict OFF/combined ON syntax checks
pass. The documented Python surface reflects implemented behavior.

The same finite-budget narrowing case now also proves every selected shared-model
and private-constant risk against explicit width one and independent scalar risk.
No production capacity or numerical guard changed.

Excel now seals strict physical trade tables, exposes typed immutable weighted
and attribution requests/results and shares checked passive getters. Four new
and all 23 affected portable risk cases pass, including no-work getters,
detached data and failed-request recovery. Repository lookup errors retain the
original trade/physical cell context after a focused RED and repair. Fourteen
MSVC source/configuration checks and ten strict source/header checks pass.
Nineteen generated exports await actual Windows runtime acceptance.
Exact code head `4bea5026` passes all four extended lifetime/profiling runtime
configurations: 2,643/2,672/2,663/2,692 tests, each including 1,166 Python cases
and the new portable Excel contracts. Codacy passes with zero annotations.
The standard complete OFF build and installation also finish successfully.
The sanitizer selectors now include all 37 core and 41 public portfolio cases
in the six existing ASan/UBSan/TSan matrices; fresh selected runtime is required.

The final standard OFF suite passes all 2,698 tests; installed C++ and all 126
affected installed Python cases pass. The expanded TSan jobs find a 257-path
price/derivative comparison whose four-ULP assertion is invalid for separate
reductions; repair adds a path-count-scaled machine epsilon bound and an
independent scalar-risk oracle while retaining exact private risks and all
capacity/work/history assertions. Fresh sanitizer acceptance remains required.
The first complete performance run passes 157/160 cases; fixed affinity accepts
159/160. Original failures and contaminated measurements are retained. A
same-binary weighted calibration demonstrates unreliable short-request timing.
Predeclared quiet confirmation times 64 identical full requests per process,
preserving the workload, ten alternating pairs, two rounds and 4% threshold.
The same-binary control and all eight repeated weighted comparisons pass.
The final original single-request control passes, but two compiled weighted
cases fail. Repeated timing alone cannot close this original gate. A preparation
inlining repair is now applied: both extracted private helpers retain the old
entry's inlining behavior. All eight original weighted and eight scalar cases
pass; 100 fresh-relinked gate/curve executables are byte-identical to their
accepted versions. MC accepts 42 cases at original four-core affinity; two
unchanged serial calculations pass with fixed caller CPU 0. All 160 cases are
accepted within those explicit scopes. After the repair, 875 affected C++/Excel
and 126 Python cases pass, as do strict syntax and refreshed installed consumers.
At `9cc9fe0a`, all 35 checks pass, with zero Codacy annotations and unresolved
review threads. All four actual Windows modes pass seven new typed/raw and
registration cases each; all six sanitizer jobs pass all 78 portfolio cases.
The archived [portfolio performance report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/47c33a12b142af8aec9532b0c7c1084fd9f064a6/.codex/artifacts/performance/aad-compatible-script-portfolio.md)
retains limitations and all 130 matched new-entry cost cases, including expensive
one-trade and distinct-owner shapes. Final `47c33a12` accepts all 35 CI checks,
zero Codacy annotations and unresolved threads in both complete audits. Actual
Windows logs verify all seven new cases in each of four modes and installed
consumers; all six sanitizer logs verify all 78 portfolio cases. PR #487 is
confirmed MERGED at `bf52e386`, whose tree equals the accepted source tree.

P02 is accepted in merged #488 at `b7e69342`, with a tree identical to final
`4a322999`. Both publication audits verify all 35 checks, zero Codacy annotations
and zero unresolved threads. Actual logs verify 82 affected cases in each of six
sanitizer jobs, 11 key cases and installed consumers (3/3) in each of four Windows
modes, and 1,166 Python cases in each of four extended modes. Complete-request
timing accepts all 37 portfolio cases; thirteen freshly linked legacy executables
retain their measured hashes. The archived
[review](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/4a3229996fcafddd58fafc84f751d2e24bd1e166/.codex/artifacts/reviews/aad-worker-reuse.md)
and [performance acceptance](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/4a3229996fcafddd58fafc84f751d2e24bd1e166/.codex/artifacts/performance/aad-worker-reuse.md)
retain the accepted source, limitations and all failed/intermediate evidence.

P03 is accepted in merged #489 at `97567d6e`, with the exact tree of final
`027a15b5`. The source includes original-ordinal scatter, matched admission/replay
budgets and a deterministic compact fallback. Both paginated publication audits
verify 35/35 checks, zero Codacy annotations and zero unresolved review threads.
Final actual logs verify 89 affected cases in each of six sanitizer configurations,
96 relevant cases and installed consumers (3/3) in each of four Windows modes,
and 93 relevant C++ plus 1,166 Python cases in each of four extended modes.
Copilot's packed six-family coverage finding is repaired with distinct
packed/compact fixtures and independent scalar references. The final test-only
correction preserves production/archive/measured-binary hashes.

The archived
[performance report](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/97567d6e7fc395fb3c7b383050edcd7416f68a4e/.codex/artifacts/performance/aad-selected-extraction.md)
accepts 37 default and 50 selected/empty/width cases (81 unique) under the frozen
calibrated sampling contract, plus thirteen unchanged fresh-linked legacy
executables retaining 162 accepted comparisons. It records every round minimum,
width/capacity measurement and source hash. All failed/intermediate experiments
remain in the archived
[ownership design](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/97567d6e7fc395fb3c7b383050edcd7416f68a4e/.codex/artifacts/designs/aad-selected-extraction.md).
The default maximum remains one; measured width choices are explicit upper bounds
with capacity-only narrowing, not a universal widening or speedup claim.
Merge verification and both audits are retained in session evidence
`aad-selected-extraction-merged-01.json`; the merge used the accepted-head SHA guard.
P03 remaining effort is zero. The owning normalized-LU numeric pullback is
accepted in merged #490. Recording/event integration is the next F03 increment;
the independent numeric acceptance does not prove that integration.
Overall remaining effort is approximately 40–66 person-days.
Stage A is accepted; the full Stage B/C/D goal remains incomplete.
