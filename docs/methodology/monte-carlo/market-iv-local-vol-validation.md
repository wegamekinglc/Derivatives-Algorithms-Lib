# Market-grid local-volatility qualification

The [owning market model](../../models/local-volatility.md#owning-market-model-in-core-c)
uses a successful market-IV snapshot and its analytic Dupire grid for passive
Monte Carlo pricing. Its numerical qualification separates fit error, continuous
local variance, grid interpolation, time stepping and sampling. A fit certificate
alone does not qualify a discrete grid or a Monte Carlo price.

The independent harness in `tests/market-iv-local-vol/` checks Black and two SSVI
smiles under deterministic flat or nonflat F/D. Qualification is limited to the
frozen inputs, payoffs and sampling protocol below. Asian benchmark workloads
measure calling cost; they do not extend this qualification to Asian, barrier,
exercise, stochastic-rate, multi-asset or risk products. The production BS PDE
entry points do not acquire local-volatility admission from this test solver.

## Fixtures and acceptance gates

The fixture spot is `S0=100`. Nonflat carry has days `{91,182,273,365,730}`,
forwards `{101,99,102,100.5,104}` and discounts `{.995,1.005,.998,1.01,.97}`;
the flat case uses `r=.03,q=.01`. Maturities are `{91/365,1,2,2.25}` and strikes
are forward times `{.7,.85,1,1.15,1.3}`. The Black fixture has volatility `.20`.
The smile generators use `rho=-.35,eta=.60`, with either `theta(t)=.04*t` or
PCHIP through `(0,0),(91/365,.009),(182/365,.018),(1,.04),(2,.09)`.
Oracles use the actual successful fitted snapshot, rather than assuming that
the fitter recovers generating parameters exactly.

Independent log-F/log-D calculations and `erfc` Black prices check carry and
the Black limit. The 90-digit SSVI/PCHIP reference checks analytic derivatives;
the independent backward log-spot PDE checks continuous and interpolated-grid
coefficients separately. These references do not call the production pricing,
interpolation or derivative kernels.

Let `epsilon=SE/S0` and `epsilon_pair=paired_SE/S0`. Prices and mean differences
are normalized by S0; deterministic payment errors use
`abs(value-oracle)/max(1,abs(oracle))`.

| Gate                        | Required bound                                                                                                                                                                      |
|-----------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| N1 carry/date               | log-F, log-D, R and Q absolute error, and normalized F/S0, D and N*D error: at most `2e-12`; mismatched dates fail before sampling                                                  |
| N2 deterministic/flat limit | Zero-volatility S/S0 and N error at most `2e-12`; compare flat paths with the same grid and normals                                                                                 |
| N3 Black                    | Fit price error/S0 at most `1e-6`, local-volatility error at most `1e-7`; price/mean error at most `5e-5+4.5*epsilon`, with `epsilon<=1e-4`; deterministic payments at most `2e-12` |
| N4 local variance/reference | Nonpathological local-variance error at most `2e-10*max(1,abs(reference))`; independent Black, mass, normalized forward, space/time/domain checks at most `2.5e-5`                  |
| N5 smile grid               | Certified grid-PDE versus continuous-PDE price error/S0 at most `2.5e-4`                                                                                                            |
| N5 smile time pair          | Final adjacent-level difference/S0 at most `2.5e-4+4.5*epsilon_pair`, with `epsilon_pair<=5e-5`                                                                                     |
| N5 smile total price        | Finest MC versus continuous-PDE error/S0 at most `6e-4+4.5*epsilon`, with `epsilon<=1e-4`                                                                                           |

N6 separately protects owning inputs, detached getters, Clone and concurrent
requests at one/four workers. N7 covers exact/one-byte-short/zero budgets,
overflow, numeric range, failed grids, horizon and unsupported capabilities.
These behavior checks remain necessary alongside price qualification. Warnings
about fit ambiguity or extrapolation do not waive any price gate.

## Canonical grids

`a1_axes.py` evaluates axis expressions at 90 digits and rounds each final
coordinate once to binary64. It sorts numerically and removes only exactly
equal values. Carry times and maturities use the same rational sources; JSON
round trips must retain those coordinates. No epsilon merging, deleted columns
or price-dependent extra points are permitted.

With both endpoints included, the axes are:

```text
spots = {100*exp(log(a)+(log(b)-log(a))*j/(B-1)), j=0..B-1}
      union {100*exp(-.02+.04*j/(C-1)), j=0..C-1}
times = {tau+(H-tau)*j/(U-1), j=0..U-1}
      union {tau*((1/32)/tau)^(j/J), j=0..J}
      union {day/365: day in {91,182,273,365,730}, day/365<=H}
```

| Grid | B   | C  | U   | J  | tau      | a,b  | H    | Spots | Times | Points |
|------|-----|----|-----|----|----------|------|------|-------|-------|--------|
| P0   | 161 | 41 | 49  | 12 | 1/36500  | .2,4 | 2.25 | 202   | 66    | 13332  |
| P1   | 241 | 61 | 49  | 12 | 1/36500  | .2,4 | 2.25 | 302   | 66    | 19932  |
| P2   | 320 | 81 | 49  | 12 | 1/36500  | .2,4 | 2.25 | 401   | 66    | 26466  |
| P3   | 320 | 81 | 97  | 24 | 1/36500  | .2,4 | 2.25 | 401   | 126   | 50526  |
| P4   | 320 | 81 | 193 | 48 | 1/36500  | .2,4 | 2.25 | 401   | 246   | 98646  |
| P5   | 320 | 81 | 193 | 48 | 1/73000  | .2,4 | 2.25 | 401   | 246   | 98646  |
| P6   | 320 | 81 | 193 | 48 | 1/146000 | .2,4 | 2.25 | 401   | 246   | 98646  |
| P7   | 320 | 81 | 193 | 48 | 1/146000 | .1,6 | 2.25 | 401   | 246   | 98646  |
| P8   | 320 | 81 | 193 | 48 | 1/146000 | .1,6 | 2    | 401   | 245   | 98245  |

P0-P6 are diagnostics. Qualification uses both P7 and P8, including P8's flat
time tail at T=2.25. The 100000-point ceiling remains in force. Moving tau also
moves its geometric columns. Every axis interval must support three strict
binary64 subdivisions; an unrepresentable or nonnested timeline stops the run.

## Independent PDE reference

`a1_pde.py` uses Crank-Nicolson midpoint coefficients, with two half implicit
Euler steps in the initial payoff interval. It interpolates sigma in log spot
and time, applies flat tails, then squares sigma. The operator retains drift
`L'-.5*sigma^2`, discount `-r` and independent carry boundary values.

The central log-spot domain is `[-3,3]`. Mandatory nodes include all grid
log-spot knots, five strike locations, zero and endpoints. Remaining nodes are
allocated proportionally to anchor-interval lengths using floor and largest
remainders, breaking ties by the left endpoint. This gives exactly 601 nodes;
bisection gives 1201/2401/4801. The expanded `[-4,4]` domain keeps every central
node and adds 100 intervals per outer side, bisected at the same levels, giving
801/1601/3201/6401 nodes.

For adjacent spacings hm and hp, the independent nonuniform stencils are:

```text
D1 = [-hp/(hm*(hm+hp)), (hp-hm)/(hm*hp), hm/(hp*(hm+hp))]
D2 = [2/(hm*(hm+hp)), -2/(hm*hp), 2/(hp*(hm+hp))]
```

Each study starts with six solves: central 601/1201/2401 at 2048 requested
steps, central 2401 at 512/1024, and expanded 3201 at 2048. If admission fails,
there is one four-solve fallback: central 4801/4096, 2401/4096, 4801/2048 and
expanded 6401/4096. Space, time and domain differences are checked separately
at matched resolutions. The fallback preserves the actual discrete sigma.
Instability or nonrepresentable coefficients produce REFERENCE_UNAVAILABLE;
there is no smoothing, upwind substitution or assumed global convergence order.

Time anchors include zero, maturity and all in-range carry/grid times; each
interval has `ceil(requested_steps*interval_length/maturity)` substeps. The caps
are 6401 nodes and 4096 requested steps, with actual steps conservatively bounded
by 4342. Two half Euler steps add one linear solve. Requested/actual counts,
all layers, residuals and actual solve counts remain separate evidence.

Reference admission proceeds through eight Black self-check studies, eight
old-grid diagnostics, P0-P8 capture and high-precision N4/probe checks, sixteen
P7/P8 grid studies, then 56 P0-P6 single-layer diagnostics. Default-stream
alignment and formal MC follow only after the required reference/grid gates
pass. The old-grid and single-layer diagnostics retain their unavailable or
INCOMPLETE statuses; they cannot qualify an oracle or explain a certified price
error. Input, identity, implementation or resource failures still stop execution.

## Brownian coordinates and statistical stopping

The two F2 families each drive P7/P8 at
`h={1/384,1/768,1/1536,1/3072}`: eight models on one fine Brownian field.
Each maturity has five calls, five puts, DF, discounted forward, five parities,
mean, fixed payment and delayed payment. The master timeline has 438 anchors
and 437 intervals. All endpoints are retained exactly.

`a3_protocol.py` assigns `ceil(384*dt)` coarse steps except at zero-based segment
60. That segment must match both hex endpoints
`0x1.0006f5248e2e8p-4` and `0x1.2006f5248e2e8p-4`; its exact length is 1/128
and its coarse count is four. Four levels use n/2n/4n/8n, giving dimensions
1092/2184/4368/8736 and a checked upper bound of 10408. Actual Allocate/Init,
strict nesting and weighted fine-to-coarse Brownian aggregation must agree.

Sampling uses seed 1024, MRG32 Precise, no bridge and no payoff smoothing.
Sixteen blocks reserve Pmax=2^20 paths each, with Dmax=8736 scalar coordinates.
For family f, block b, path p and coordinate j, checked uint64 addressing is:

```text
address = (((f*16+b)*2^20+p)*8736+j)
f=0..1, b=0..15, p=0..2^20-1, j=0..8735
slots per family = 146565758976
total slots = 293131517952; final address = 293131517951
```

Default-production-stream coordinates, paths and prices are checked on 64 paths
before formal sampling. Coarser normals use the same fine Brownian increments
with sqrt(dt_fine/dt_coarse) weights. Different SimDim values do not reinterpret
the same RNG ordinal. Repeated default-seed Price calls or Branch calls do not
supply independent blocks. Raw fields using Dmax1584/8728 cannot fill this run.

Per-block tiers are `{2^16,2^17,2^18,2^19,2^20}` with the same path prefix.
Only insufficient preregistered price or adjacent-pair SE increases the whole
family's tier. Sixteen block means estimate SE with 15 degrees of freedom;
4.5*SE is the fixed acceptance band, without a universal coverage claim.
The final comparison uses h=1/3072 and the 1/1536 versus 1/3072 pair. Sobol is
auxiliary evidence. Sufficient SE does not excuse price failure, and insufficient
SE at the cap is INCOMPLETE.

## Resource limits and evidence reuse

The qualification's cumulative limits are 376 reference solves, six hours of
PDE, six hours per MC family and eighteen total numerical hours. PDE concurrency
is at most four; MC uses one fixture process with sixteen concurrent blocks and
runs families sequentially, separately from PDE. Limits also include 6 GiB
process RSS, 1 GiB retained evidence, 16777216 fine fields per family and
33554432 in total. Each field drives eight models; it is counted once.
Production payload/workspace/step/dimension admission remains independent.

The completed study budget is sealed. Its source, binary, configuration,
requests, ledger and hash-chained journal retain their original identities.
Reproduction commands in the [harness guide](../../../tests/market-iv-local-vol/README.md)
do not reopen that budget. Resource exhaustion is INCOMPLETE; real failures and
unavailable references stay recorded. Restarts do not reset costs or authorize
new samples. An approved recovery appends an explicit identity transition and
any one-to-one retirement of proven unexecuted reservations, preserving the
entire prior chain, failures and consumed budget. Partial alignment output
cannot become formal statistics. The sealed archives retain the exact recovery
rules and frozen control bytes used by their producers.

The qualified view is 160 F1 rows plus 320 F2 rows. Historical rows, failed
grids and diagnostics are separate; they are not added to that view or relabeled
as new execution. Each result retains fixture/grid/T/K/output, estimate/SE,
oracle, normalized errors, actual steps, source/dependencies/configuration and
binary identity. Status is PASS, FAIL, INCOMPLETE, REFERENCE_UNAVAILABLE or
NOT_RUN. Missing reference, admission or SE evidence prevents qualification.

Reuse requires a field-level mapping of inputs and dependencies, evidence hashes
and original producer identities. A changed total request hash alone does not
invalidate unchanged grid/reference mathematics; it also does not establish
new-head validation. Rebuild affected executables and recheck only the affected
gates. Preserve source/model/request schema domains, including the
[request `/2` compatibility rules](../../models/local-volatility.md#request-identity-and-execution-reports).

Fast unit/interface checks, numerical qualification, performance samples and
Windows native/Office evidence have distinct scopes. For timing, use the
[focused benchmark](../../../dal-cpp/benchmarks/script_mc_perf/README.md#market-grid-local-volatility)
and [performance workflow](../../../.codex/references/benchmark-workflow.md):
construction, preparation, Allocate, Init, inspection hashing and whole valuation
have separate boundaries. Keep two rounds of ten alternating process samples,
the existing 4% comparison threshold and the supplemental 10% flat-path budget.
Neither a numerical PASS nor portable Excel tests supply a performance verdict
or live XLL/OPER/Office acceptance.
