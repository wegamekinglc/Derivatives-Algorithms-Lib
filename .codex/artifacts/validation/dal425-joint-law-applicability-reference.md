# DAL425 joint-law applicability and mixed-moment obligations

This private checkpoint audits the applicability of accepted D2 to the original
requests. It supplies an exact synthetic admission control and a complete
machine-readable obligation ledger. It does **not** certify actual generator
moments. The controlling base is accepted master `26fe00e3`, PR644's squash.
The original D1/D2 certificates and all original numerical failures are retained.

## J1: which law is actually specified?

The model contract specifies a continuous Gaussian/Brownian probability model.
Its independent innovations justify the Gaussian cashflow oracle. That target
model is different from a probability distribution over the implemented MRG
state. The execution contract specifies MRG32, Precise, seed1024, stream `/1`,
and deterministic reserved scalar positions. It describes independent sampling
and disjoint members as the intended sampling design, but does not define a
randomized seed, initial-state measure, random shift or random stream selector.
The inspected retained requests contain none of those either. This audit does
not reinterpret the design's word “independent” as an actual joint-moment proof.

| Evidence at the accepted source                                                                                  | Established premise                                                                                                                                  | Premise not established                                                                       |
|------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------|
| `.codex/artifacts/api-notes/dal-410-xccy-mc-model-decision.md:812`, `:835`, `:850`                               | Fixed seed/stream, phase and refinement prefixes; batch-based SE and acceptance formulas                                                             | A randomized experiment realizing IID-grid outputs or independent batches                     |
| `.codex/artifacts/api-notes/dal-xccy-mixed-mapping-basket.md:82`, `:135`                                         | Disjoint scalar-block design with mapping and allocation identities                                                                                  | Moment factorization from nonoverlap alone                                                    |
| `dal-cpp/dal/math/random/pseudorandom.cpp:172`, `:177`, `:182`, `:299`                                           | Factory maps seed to `(a,b)=(unsigned(seed),unsigned(seed)+1)`; reset state is `(a,a,a,b,b,b)`; integer recurrences and output map are deterministic | A law over those initial states or numerators                                                 |
| `dal-cpp/dal/math/random/pseudorandom.cpp:160`, `:199`                                                           | `m1=4294967087`, `m2=4294944443`; numerator `x-y` if positive, else `x-y+m1`; double division by `4294967088`                                        | Uniformity or joint independence of these numerators under an admitted measure                |
| `dal-cpp/dal/math/random/pseudorandom.cpp:33`, `:60`, `:208`, `:216`                                             | Absolute seek by `NDim*path`; reset and matrix powering; Precise consumes one scalar uniform per supplied entry and calls scalar inverse             | A randomized seek or independent stream split; the unused `Branch` method proves nothing here |
| `dal-cpp/dal/script/xccymcbasketscalars.cpp:22`, `:33`                                                           | New MRG32 with factory dimension1 and Precise; seek to the absolute scalar start; fill a D-vector and advance D scalars                              | A new independent RNG for each batch/member; all use the same seed and recurrence             |
| `dal-cpp/dal/script/xccymcranges.cpp:12`, `:115`; `dal-cpp/dal/script/xccymcbasketranges.cpp:57`, `:149`, `:203` | Capacity, mapping and nonoverlap checks, stable geometry-dependent offsets                                                                           | A distributional independence theorem; these functions inspect deterministic metadata         |
| `dal-cpp/test-support/xccyacceptancebasketcapture.cpp:177`, `:191`, `:220`                                       | Original fit, HeldOut, B16, n16384 times x1/x2/x4; imported native inputs frozen                                                                     | A seed-randomization stage omitted from the retained command                                  |

The new evidence `source-audit.json` stores exact hashes and numbered excerpts.
`moment-ledger.json` binds the original six capture/command files, allocation,
execution, model, mapping, source, instrument and selected native identities.
The accepted input certificate SHA256 is
`2f6666deed5b704edfc37014f99a8ac06e7bb537edd4bbc12754f480cc59ef49`;
the frozen fixture SHA256 is
`5c2f3d48861e173185219bd32fad7cbf49a6c9e92918586b9a7bc0bd8108837a`.
The executed historical caller identities are inherited separately; these
source reads do not reestablish historical runtime binding.

### Exact coordinate sharing and weights

Let T=2359296 and H=1310720. The member blocks are:

| Member               | D  | Scalar block          | Fit / Validation / HeldOut local path reservations |
|----------------------|----|-----------------------|----------------------------------------------------|
| independent-staged-0 | 30 | [0,70778880)          | [0,262144), [262144,1310720), [1310720,2359296)    |
| independent-staged-1 | 55 | [70778880,200540160)  | same three local path intervals                    |
| independent-staged-2 | 80 | [200540160,389283840) | same three local path intervals                    |

For offset O, phase path p and slot j, the scalar address is `O+D*p+j`,
0<=j<D. Ordered j are five innovations `(xd,xf,Id,If,Z)` per dated interval,
exactly as listed for each of the six cases in the ledger. Both fits use the
same addresses and mappings; their fitted financial coefficients differ.
Their estimates therefore do not provide independent statistical replications.

At factor f, batch b uses paths `[H+b*n,H+(b+1)*n)`, n=16384*f, b=0..15.
The complete union is `[H,H+16*n)`. The x1 union is a prefix of x2, and x2
of x4; equal-numbered batches are generally **not** nested because their
starts move with n. Batch0 has the prefix relation. No paths are pooled into
new independent observations. All three phases and different member blocks
are disjoint scalar intervals; this asserts address separation only.

For a given member/path, A, B, coupon differences, native columns, tree/compiled
and declared CRN finite differences share the coordinate vector. Cholesky
loadings deliberately share earlier innovations within a path. The original
single-member D30 stream and basket member0 reuse the same seed and scalar
addresses (accepted record at `dal-xccy-gaussian-numerical-validation.md:725`).
Old HeldOut is already observed. Reuse supplies no new statistical evidence.

Ideal coefficient means use identical fixed nonnegative weights
`w_p=1/(16*n)` for values and native derivatives. Equal weighting follows from
`dal-cpp/dal/script/xccymcbasket.cpp:230` and batch accumulation/means; it is
also explicit in the retained request metadata. Binary accumulation errors
remain part of the unqualified pipeline. Native perturbations keep the scalar
inputs, law (if one is admitted) and weights fixed, and differentiate c,k,ell.
The ledger records both the seven sampled basket labels and all eleven D2
native labels; it does not claim new sampling of the other four columns.

There are three distinct legitimate expectation statements. The target
Gaussian expectation is specified by the model. The accepted D2 theorem is
conditional on independent uniform numerators 1..4294967087 followed by RN
division and the qualified scalar transform. A fixed vector of executed paths
also has a meaningful finite weighted average, equivalently integration against
its empirical atomic measure. The last is not the IID-grid experiment and
contains realized sampling discrepancy when compared with the Gaussian target.
A fixed-seed trace is meaningful deterministic data; it does not make every
expectation meaningless. No distribution over seeds/states is invented here.

## J2: the weaker moments actually needed

Each accepted independent cashflow term has the form
`c_t exp(k_t + ell_t.Z_p)` in signed group A or B. The accepted construction
is `xccy_acceptance_bias_cashflows.py:89`, `:100`, `:106`, `:117`, `:127`;
the accepted D2 implementation is `xccy_acceptance_bias_qualification.py:119`.
For native label j, write `dc_tj`, `dk_tj`, `g_tj=dell_t/dtheta_j`.
Assume an admitted measure mu, fixed with respect to these native inputs,
and the same fixed normalized weights. The required mixed moments are only

```text
M_t   = sum_p w_p E_mu[exp(ell_t.Z_p)]
F_tj  = sum_p w_p E_mu[(g_tj.Z_p) exp(ell_t.Z_p)]
G_t   = exp(sum_i ell_ti^2/2)
H_tj  = (sum_i g_tji ell_ti) G_t
```

For the target independent standard Gaussian Z, G and H follow from its
moment-generating function and directional derivative. Under the accepted
IID-grid premise, scalar r0/r1, tilted tails, rounded uniforms and transform
error imply sufficient bounds on these moments. Multiplying scalar moments
is the step that needs the joint premise. The scalar transform certificate
and D1's uniform same-input perturbation theorem do not establish it.

No full-distribution identity is necessary. Even termwise error boxes are
stronger than needed. It suffices, for each group Q=A,B and native j, to prove

```text
abs(sum_(t in Q) c_t exp(k_t) (M_t-G_t)) <= e_Q
abs(sum_(t in Q) exp(k_t) [
    (dc_tj+c_t dk_tj)(M_t-G_t) + c_t(F_tj-H_tj)
]) <= e_dQj
```

Here e_Q and e_dQj are the exact outward upper endpoints of the accepted
D2 A/B/dA/dB component enclosures copied into the ledger. Their independent
Gaussian signed-center enclosures are copied too. Certification must enclose
the error against the exact Gaussian centers, not merely overlap a center
interval or compare decimal displays. The old coefficient endpoint arrays
enclose the original real coefficients from the independent construction;
a proof may cover these boxes uniformly, or bind to the exact underlying
construction. It may not treat an arbitrary midpoint as the financial input.
Sparse indices 0,1,2+i denote dc,dk,g_i; omitted entries are exactly zero.
The value coefficients are indexed 0,1,2+i as c,k,ell_i. This preserves
coefficient, drift, loading, sign, coordinate order and native-map identity.

The ledger contains **156 nonconstant term-indexed zeroth entries, 438 active
term/native directional entries and 144 aggregate signed envelopes** across
six profiles. Repeated functions may be deduplicated by a later proof; these
are coverage counts, not a claim of 594 distinct moment functions.
Each term and every native direction is included, with exact binary endpoints.
F need not be certified separately in every coordinate if its directional
combination is enclosed directly. A/B/value and native coefficient errors
then feed the unchanged Price formula and accepted expectation-quotient
inequality, including its positive denominator margin and all native terms.
This concerns ratios of coefficient expectations, not expected pathwise ratios.

Linearity of expectation means these first-moment obligations do not require
cross-path, cross-batch, cross-phase or cross-member independence. The same-path
mixed moments are essential. No `E[exp(ell.Z_p) exp(v.Z_q)]` is used for D2.
Such pair moments and further statistical premises would matter to covariance,
SE or confidence claims, which this checkpoint leaves unchanged. Exact normal
marginals and even covariance agreement do not determine M or F.

### Actual status and the concrete cut-point

All six geometries' actual weighted M/F conditions remain unproved. The
inspected deterministic implementation plus marginal scalar bounds do not
provide a probability measure mu bound to these requests, or bounds for these
specific signed combinations under one. This is a missing premise, not a
numerical proof that the combinations exceed the budgets, nor a claim that
the technical frontier is exhausted. No support/cardinality/rank/period
argument is used to claim an over-budget error. No actual D2 certificate is
produced by selecting an unrequested uniform state/seed distribution.

The minimum executable successor is a bounded premise reconciliation: identify
the authoritative, already-authorized experiment (sample space, measure,
seed/state/position mapping, parameter-independent weights, and binding to
the six requests), or explicitly confirm that D2 is only a conditional theorem.
Only after that can a proof tool consume the six-profile ledger and establish
the 144 signed envelopes under that measure. A certificate for weighted M/F
is sufficient; a general IID theorem is optional. If an actual experiment
requires changing the probability contract, the coordinator must obtain that
decision before execution. This checkpoint supplies no such change.

Premise reconciliation requires source/contract reading, with a proposed
15-minute bound, zero product execution and zero state enumeration. Its result
determines a separate proof method and cost freeze. No credible cost for a
full-state proof is claimed before that input exists. Enumerating a hypothetical
six-word state space would neither answer the current question nor respect the
method budget. More MC paths cannot discharge the missing premise.

## J3: exact positive and rejection controls

The private helper `xccy_acceptance_joint_law.py` admits only explicit tiny
synthetic laws. It sums positive Fraction weights over integer atoms, checks
normalization and ordered coordinates, and replays each required mixed moment.
For positive rational bases q_i, integer powers evaluate
`exp(sum_i log(q_i) z_i)` exactly as `product_i q_i^z_i`; no approximate log or
exp is used. Directional first moments multiply by the exact rational `g.z`.
Signed directions and negative integer atoms are covered. Request and law
digests bind complete ordered coverage, weights, tilts and required envelopes.
Digests bind content; exact replay supplies the proof. They are not signatures
or an oracle for an upstream request's authority/completeness.

On independent Bernoulli(1/2) x,y, q=(2,2): M=9/4 and F_x=3/2. The correlated
law `(0,0),(1,1)` with weights 1/2 has identical marginals but M=5/2 and F_x=2.
Copying the independent moments fails exact replay even with a fresh matching
law/request identity. With q=(2,1), both laws have M=3/2, but F_y is3/4 and1:
zeroth moments and marginals cannot replace the required first moment.
The dependent law is positively admitted for its own correct M/F obligations.
Thus the checker neither requires independence unnecessarily nor always refuses.

Missing/extra moments, stale identities, swapped/missing coordinates, altered
weights/tilts/directions/envelopes, unnormalized/negative weights, duplicate or
wrong-dimensional atoms, inexact inputs and a boolean “qualified” claim are
rejected. The authoritative complete request is a trust input to local admission;
an arbitrary caller choosing fewer obligations obtains no global claim. No
adapter promotes this synthetic result to the actual financial ledger.

The domain cap is four dimensions, 64 atoms, 64 conditions, |z_i|<=8, and
law/request rational numerator/denominator bit lengths<=128. Cost is linear in the atom,
condition and dimension counts, with bounded integer exponentiation. The
trusted arithmetic is Python Fraction/integer arithmetic and the inspected
checker. The proof runtime's identity is recorded separately from the inherited
historical financial executable/runtime. This is not a new MC or normal sample.

## Verification and preserved limits

The focused incomplete-admission test first failed with “ValueError not raised”
when the new skeleton trusted claimed moments. Exact replay closes that failure.
Thirteen new tests pass. No existing product tests are rerun or repaired.
The ledger extraction checks all six frozen profile identities, all eleven
native labels and original request coordinate ranges, without reexecuting D2,
old scalar proofs or financial captures. Ruff/format/CCN checks cover only the
new helper and tests. Command outputs, exits, source/runtime identities,
negative controls, the complete ledger and SHA manifest accompany delivery.

The accepted record's 118965-byte prefix is preserved. D1 remains a qualified
finite-input transform component within its accepted assumptions. D2 remains
an explicit IID-grid conditional expectation theorem. Historical caller/runtime
binding and the binary pipeline remain unqualified. The four global flags
`financial_bias_bound_qualified`, `actual_joint_law_qualified`,
`caller_binding_qualified`, `original_numerical_pass` remain false.
Original eighteen/six-profile FAIL, all precision/combined failures, five FD
hard-floor REDs, full eFD, 25 unsampled statistical axes, observed HeldOut and
the ideal-Gaussian scalar RED are retained with original budgets, counts,
reservations, scales, seeds and thresholds. No numeric V1, quote-risk or P4
acceptance is implied. The issue remains in progress for its complete chain.
