# Signed finite-stream transform bounds

This private reference extends accepted D1 only. It neither changes nor
recomputes accepted D2, the original 288-row total-error overlay, or actual
financial qualification. The executable is `xccy_acceptance_finite_bias.py`
with the mathematical enclosure kernel `xccy_acceptance_finite_math.py`, both
under `dal-cpp/tests/script/`. The pre-execution method and finite cost limit
are retained in the evidence package's `PLAN.md`.

## Frozen domain and trust

The base is accepted master `32e9a68869ac5ecb8ff677f0e147d862b783ce85`.
The fixture retains SHA256
`5c2f3d48861e173185219bd32fad7cbf49a6c9e92918586b9a7bc0bd8108837a`;
its original capture retains SHA256
`c5707a03f041e50d8d24e7584ff01fe74b4d0f825f22c45282361073b7c06846`.
Only EXACT/APPROXIMATE x D30/55/80, the original N110, binary64 100/110,
K=.001, spot1.1, sigma, bucket, dates, curves and native Jacobian are admitted.
All eleven native axes retain their original identities and units.

For rounded-grid ideal normals z, accepted Mills/support arithmetic gives
| z_j | <=M=25/4. Subject to the accepted scalar's caller assumptions, |
the corresponding scalar normals are z+e with |e_j|<=E=9.006864425e-9.
E is the same rational upper bound, never a sampled error or a replacement
5e-13 target. The entire homotopy z(t)=z+t e, 0<=t<=1, lies in
[-M-E,M+E]^D. All cashflows on a path share z(t) and e. Each sample has a
fixed nonnegative weight w_i, sum_i w_i=1, independent of the native
parameter. Weights and normal inputs are identical in A, B and every native
column, before and after the perturbation. No IID assumption is made.

Unchanged accepted independent `IndependentTerms` derives signed cashflows
and dual Brownian/Cholesky coefficients. Write S=sum c exp(k+ell.z).
For native theta, its terms are exp(k+ell.z)(d+v.z), with
d=c_theta+c k_theta and v=c ell_theta. Native derivatives of coefficients,
deterministic exponents and loadings are all retained. Spatial derivatives
of exp(x)(p0+p.z) are exp(x)[p_j+ell_j(p0+p.z)]. The program differentiates
the original numerator before applying any denominator anchor.

Arithmetic is mpmath 1.3.0 interval/libmp at 236 bits. Inherited binary64
inputs enter as exact integer ratios. exp, expm1, log and all arithmetic
use interval operations. Exact binary endpoints are authoritative; decimal
endpoints are rational floors/ceilings at 35 places. This trusts Python
integer/rational arithmetic, libmp's directed elementary functions and the
unchanged accepted independent cashflow derivation. It is not a formally
verified implementation or a bound on the floating cashflow pipeline.

## Signed pair enclosure

At a fixed parameter, represent every term by an affine polynomial P(z)
and an affine exponent x(z). For any pair, the exact identities

```text
exp(x)P + exp(y)Q = exp(y)[P+Q+P expm1(x-y)]
                  = exp(x)[P+Q+Q expm1(y-x)]
```

retain the common coordinate and the signed polynomial sum before taking
an interval enclosure. Polynomial coefficient sums and exponent coefficient
differences are formed before evaluating their complete box ranges.
The unpaired expression is another enclosure; intersecting these three
outward enclosures is valid. The code requires a nonempty intersection.
Consecutive B terms pair coupon growth-minus-one, initial/final principal
and MTM exchanges. The identity is valid for any pairing, even when
coefficients do not cancel. No equality is inferred from approximate
coefficients or close endpoints. Different pairs still use the same domain;
interval evaluation can lose dependency but cannot assume independence.

This bound also applies to every spatial derivative and mixed native/spatial
derivative. In particular, the polynomial derivative p_j must remain when
differentiating a native column. Pairing is performed afresh on those exact
polynomial terms, not by differentiating an absolute error majorant.

For an affine anchor j(z), dividing an exponential-polynomial term by exp(j)
means replacing its exponent by x-j. Signed pair differences x-y remain
shared. This is the only normalization operation in the enclosure kernel.

## Positive annuity and coefficient means

Let D=-A=sum_h a_h exp(k_h+ell_h.z), with every a_h>0; all admitted annuity
terms satisfy this sign. With n annuity terms, AM-GM gives a common anchor

```text
J(z) = exp(log(n) + (1/n)sum_h(log(a_h)+k_h)
                   + ((1/n)sum_h ell_h).z) <= D(z).
```

Interval division by n encloses the exact rational weight 1/n; no rounded
weights are silently treated as summing exactly to one. J is a positive
pointwise lower function. Its parameter derivative is never substituted
for D_theta. Separately, the sum of each term's minimum over the homotopy
box proves a strictly positive absolute lower bound m for D and every
coefficient mean throughout the homotopy.

For any signed S, |S(z)|<=C D(z) implies
| sum_i w_i S(z_i) | <=C sum_i w_i D(z_i). It is enough to enclose S/J, since |
J<=D. This establishes the same normalized bound for every legal finite
weighted mean without dividing a separate global numerator maximum by a
separate global denominator minimum. It does not replace the ratio of
coefficient means by the mean of pathwise ratios.

Write bars for weighted coefficient means. Over the full box, compute

```text
R  >= sup |B/D|;              Rt >= sup |B_theta/D|
V  >= sum_j sup |B_zj/D|;     Vt >= sum_j sup |B_theta,zj/D|
L  >= sum_j sup |D_zj/D|
U  >= sup |D_theta/D|;        W  >= sum_j sup |D_theta,zj/D|.
```

R, Rt, V and Vt use signed-pair enclosures divided by J. Positive annuity
weights give sharper denominator controls. Put
eta_h=a_h,theta/a_h+k_h,theta+ell_h,theta.z. Then

```text
L = sum_j max_h |ell_hj|
U = max_h sup |eta_h|
W = sum_j max_h sup |ell_hj eta_h + ell_h,theta,j|.
```

These follow because D_theta/D and D_theta,zj/D are annuity-weighted means
of eta_h and ell_hj eta_h+ell_h,theta,j. All bounds are uniform over the
same homotopy box. The program records m, J, L, U, W and R, Rt, V, Vt for
each geometry and native axis. An anchor is not an independently moving
cashflow or a fixed denominator assumption.

## Homotopy quotient and all native movements

Let r=bar(B)/bar(D)=-bar(B)/bar(A). For a dot denoting t differentiation,
the mean-value bound and |e_ij|<=E imply
| dot(bar(B)) | /bar(D)<=E V and |
|-------------|------------------|
derivative on each path before summing weights; path perturbations may
differ, and are not factored out of a signed mean. Similarly,
| dot(bar(D)) | /bar(D)<=E L and |
|-------------|------------------|

The exact mean-quotient identities are

```text
dot(r) = dot(Bbar)/Dbar - (Bbar/Dbar) dot(Dbar)/Dbar
r_theta = Bbar_theta/Dbar - Bbar Dbar_theta/Dbar^2
dot(r_theta) = dot(Bbar_theta)/Dbar
 - (Bbar_theta/Dbar) dot(Dbar)/Dbar
 - (dot(Bbar)/Dbar) (Dbar_theta/Dbar)
 - (Bbar/Dbar) dot(Dbar_theta)/Dbar
 + 2 (Bbar/Dbar) (Dbar_theta/Dbar) dot(Dbar)/Dbar.
```

Integrating from 0 to 1 gives uniform component majorants

```text
delta r       <= E (V+R L)
delta r_theta <= E (Vt+Rt L+V U+R W+2 R U L).
```

Every native term and both denominator movements remain. The certificate
stores the five native contributions separately. Absolute A/B and their
natives use E times the corresponding full-box spatial derivative sum.
Price=B+K A uses the original imported binary64 K and the triangle bound
for its two components. Each new majorant is compared with the inherited
accepted D1 bound; their minimum is another valid majorant. Raw new values
remain visible when the old bound is tighter. No old theorem is replaced.

Price and par retain .0025 USD and .000025 decimal-spread budgets, divided
by the unchanged native parameter scale (.01 for spot/FX sigma, .001 for
logDF/rate sigma). A and B have units USD and no invented budget. Component
comparisons are sufficient-upper-bound diagnostics only. They are neither
total numerical passes nor actual-error lower bounds.

## Targeted historical caller applicability

This is a new bounded read-only audit of caller premises, extending the
accepted source/object/libm match. The original executable SHA256 remains
`b4151ec3502b0e3426a77b74e3c2540094fb17430f95b53022cba22b0ade6aed`.
No product, MC executable, new libm probe or rebuilt capture is run.

| Premise                       | Evidence and conclusion                                                                     | Status                                      |
|-------------------------------|---------------------------------------------------------------------------------------------|---------------------------------------------|
| Original source/config route  | Retained basket capture, six original commands, scalar adapter New(MRG32,seed,1,true)       | Established static intended route           |
| Linked adapter arguments      | Retained ELF 0x482478 sets dimension 1; 0x482480 sets precise=true before factory call      | Established static instructions             |
| Linked scalar branch          | MRG FillNormal 0x57f170 bypasses block when precise; 0x57f283 calls linked inverse 0x5d3120 | Established static branch and direct call   |
| Scalar polish / NCDF route    | Both booleans come from precise; linked inverse calls NCDF at 0x5d3338                      | Established static route under object flag  |
| Mean/cashflow coordinates     | RunBatch passes one normals vector to coefficient execution; Accumulate uses common n       | Established source semantics                |
| Historical per-call fenv      | No historical rounding-control or MXCSR trace exists in retained provenance                 | Assumed; not historically reconstructible   |
| Historical FTZ / DAZ          | Static object match does not record caller or worker MXCSR during each call                 | Assumed; not historically reconstructible   |
| Historical dynamic libm GOT   | ELF log/exp GLIBC_2.29 relocations exist; no per-call resolved target capture               | Assumed; not historically reconstructible   |
| Whole linked caller execution | Virtual call 0x482d20 and execution history are not a captured per-call proof               | Partial static evidence; runtime unproved   |
| Actual joint draw law         | Same frozen trace/support; no law certificate introduced                                    | Unqualified, independent of this D1 theorem |

The ELF inverse calls log@plt and exp@plt; relocations name GLIBC_2.29 at
GOT offsets 0xa87cf0 and 0xa87f50. Library-file identity, a static relocation
or today's resolved symbol does not establish the historical selected
implementation. No absence of a mode-setting instruction in a narrow
routine proves its incoming mode. Source/config evidence and retained linked
instructions improve the static route evidence; they do not establish
all historical invocations or complete pipeline arithmetic applicability.

The minimal additional caller evidence is a contemporaneous record bound to
the original executable/runtime: incoming rounding and MXCSR for each
relevant worker and invariance over the scalar calls, selected loader/GOT
targets and accepted libm implementations, and the linked execution route.
These historical facts cannot be created by a new probe. If absent, any
future applicability validation must be explicitly distinguished from the
old captures. No such new experiment is selected or run here.

## Executed component results

The first six-case calculation completed in 584.03 seconds, below the
pre-frozen 900-second cap. Every geometry contains 48 comparisons: A/B/Price/
par values and their eleven native derivatives, for 288 comparisons total.
230 raw new majorants are strictly tighter; the other 58 preserve the
accepted old majorant through the explicit minimum. All 144 raw new Price/
par value/native component bounds fit their unchanged individual budgets.
The old sufficient majorants exceeded those budgets in 22 comparisons.
There are no remaining over-budget transform-component bounds in this
domain. This conclusion is subject to the stated scalar and mathematical
cashflow premises, including the unqualified historical caller binding.

The following displays round upward to six significant decimal digits.
The package's canonical certificate contains every strict binary endpoint,
every native column, old/raw/selected bound and original budget; its
`comparisons.csv` is a readable 288-row projection of those endpoints.

| Fit         | D  | Old Price USD | New Price USD | Old par       | New par    | New par FXsigma /sigma |
|-------------|----|---------------|---------------|---------------|------------|------------------------|
| EXACT       | 30 | 0.0000121490  | 0.00000244992 | 0.00000169177 | 4.23040E-8 | 0.00000195572          |
| EXACT       | 55 | 0.0000978632  | 0.0000145729  | 0.0000536820  | 8.32036E-8 | 0.00000656402          |
| EXACT       | 80 | 0.000582327   | 0.0000853170  | 0.00128870    | 1.77205E-7 | 0.0000199262           |
| APPROXIMATE | 30 | 0.0000121490  | 0.00000244992 | 0.00000169177 | 4.23040E-8 | 0.00000195572          |
| APPROXIMATE | 55 | 0.0000978632  | 0.0000145729  | 0.0000536820  | 8.32036E-8 | 0.00000656402          |
| APPROXIMATE | 80 | 0.000582327   | 0.0000853170  | 0.00128870    | 1.77205E-7 | 0.0000199262           |

The strict positive absolute annuity minima exceed 32.3210431, 37.1028799
and 38.3098836 for D30/55/80, on the entire homotopy box for both fits.
The positive-denominator proof and the relative anchor have separate jobs;
no near-zero denominator or fixed-denominator approximation drives this result.

Fourteen new deterministic tests cover analytic cancellation, annuity count/
sign, all native terms against the independent direct cashflow evaluator,
spatial polynomial derivatives, exact mean weights, ratio of coefficient
means, full quotient movement, units, scalar E/support, trusted arithmetic,
endpoint encoding, input identity and cache domain/precision. The independent
rational control D=1+z+theta(1-z), B=1-z+theta(-1+z), z in [0,1e-6],
needs all five quotient-native contributions: its native change exceeds
5.99e-6 while the bound is 6e-6. It is an analytic algebra control, not an
additional financial fixture or scalar-error support assumption.

Fourteen isolated omissions are assertion-rejected, with no harness errors:
unpaired cancellation; missing coefficient, deterministic or loading native;
missing spatial polynomial derivative; annuity count; changed or negative
mean weights; incorrect par units; each of five quotient-native movements.
Finite-point direct-cashflow controls validate implementation only. The
uniform conclusion follows from the interval box theorem above.

Existing product tests, old 18-report/six-profile MC, accepted 18 D1/D2/D3
tests and certificates, 408 FD controls, scalar/libm cells, Windows/native/
install and performance matrices are inherited only; none is rerun here.
No product failure is repaired. New helper work and its initial failures are
separate: the first cancellation test was initially too trivial and passed;
its corrected nonzero shared residual exponent fails before the signed
factorization and passes after it. An initial mutation substitution failed
its harness precondition after formatting; the corrected isolated mutations
all produce assertion failures. Initial style/CCN and a caller-audit filename
error are retained separately from mathematical or product results.

## Qualification limits

Every conclusion is a mathematical transform component on the stated box,
conditional on scalar applicability. The actual joint law, historical full
caller binding and floating cashflow arithmetic remain separate. Even a
small component bound would not change `financial_bias_bound_qualified`,
`actual_joint_law_qualified`, `caller_binding_qualified` or
`original_numerical_pass`; all four remain false. Original 18+six overall
FAILs, 288/216/214 counts, five FD hard-floor REDs, full eFD, 25-axis
sampling gaps, Precise ideal RED, D80-x4 exhaustion, observed HeldOut,
identities, sampling errors, budgets and thresholds remain untouched.

This finite method performs no domain subdivision and has no exponential
search. The remaining uniform overestimate includes the AM-GM denominator
gap, interval loss between different signed pairs, sums of coordinatewise
suprema and the triangle inequality between quotient contributions. A large
upper bound cannot prove a lower bound on actual Precise error, historical
stream error or a business-goal conflict. Any next refinement must target
one of these explicit dependencies without shrinking E/support, changing
law/weights, or substituting conditional D2 for the finite theorem.
