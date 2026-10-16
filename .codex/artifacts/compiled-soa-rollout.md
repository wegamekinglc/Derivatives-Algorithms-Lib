# Restricted compiled double SoA (DAL-357)

The production opt-in is `MCDoubleSimulationSoa<L_>` in
`dal-cpp/dal/script/compiledsoa.hpp`. The existing simulation overloads and
binding settings retain their current behavior. This batch implements the
approved restricted deployment; the earlier prototype remains reproducible in
`dal-cpp/experimental/compiledsoa` and its historical study note.

## Admission and execution

The entry accepts a preprocessed `ScriptProduct_` or a sealed `PreparedScript_`
and a real `AAD::Model_<double>*`. Live requests require the exact
`AAD::BlackScholes_<double>` dynamic type and a pool configured for one worker.
Derived generators are rejected. Prepared requests must have compiled double
execution and matching RNG/bridge settings and model type. Preparation remains
responsible for fixing resolution, valuation date, observations and history.
The entry does not prepare or reinterpret a request on the caller's behalf.

The whole request is checked before the first random draw. Model allocation and
initialization retain existing timeline, parameter and sample-output validation.
The plan decodes all events, branches, operands, constant/variable indices and
stack layouts before evaluation. Prepared observation IDs lower to frozen
constants or model sample rows after checking their model output slots. Event
samples and observation samples may differ. Ordinary immediate PAYS uses its
event sample's deterministic numeraire. Actual model parameters and the sealed
grid determine all BS coefficients, with the existing scalar arithmetic order.

The supported instruction set is the prototype's ordinary hard scalar set plus
prepared observation loads and discard. Powers, fuzzy opcodes, vectors, delayed
payments, rates/forwards and LSMC are rejected. There is no exception-swallowing
fallback. An unsupported live request raises `ScriptError_`; the caller may
choose the existing driver before invoking this opt-in. Empty/expired products
retain the existing zero-result behavior after executable/RNG/path-count checks.
Fewer than sixteen paths use the original compiled scalar driver after basic
model/configuration/sample admission; that scalar bypass does not require the
SoA opcode subset. There is no partial execution followed by request replay.

Widths 8, 16, 32 and 64 are explicit template choices, with 32 the default.
Each opcode executes over contiguous variable/stack rows. IF partitions parent
masks, including nested and mixed branches; inactive lanes never execute branch
arithmetic. The prototype's quarter-minority scalar transfers are absent.
Tail rows are bounded by the actual lane count; a full 64-lane mask avoids a
shift by 64. Extrema retain left/right operand order, including NaN and signed
zero. Each block resets variable seeds. Workspace and RNG belong to the request;
exceptions publish no partial result and leave no persistent evaluator state.

The random method, normal precision, bridge, path order and chunk-start offsets
are those of the existing driver. Gaussian filling remains path-major. Chunk
reduction also matches the scalar driver's 64..2048-path chunks, rather than
changing summation order to lane reductions. There are no new AAD, Python,
Excel, weighted-objective or parallel execution modes.

## Verification and measurement scope

Core regression cases cover 8/16/32/64 lanes, vanilla/weekly/frequent divergence,
1/7/8/9/15/16/17/65/129 paths, prepared observation-only dates and history, all three
RNG methods, both explicit normal precisions and bridges, nested inactive
invalid arithmetic, extrema operands, admission, overflow, payoff errors and
repeated requests. Existing scalar/tree/AAD callers provide isolation evidence.
The existing nine-target performance gate and full cross-platform regressions
remain CI responsibilities.

`script_mc_perf --compiled-soa` measures whole production requests. `steady` reuses a parsed
and preprocessed product but includes compilation, plan construction, model
initialization, RNG, workspaces and execution on every invocation. `cold` also
includes product parsing and preprocessing. Thus neither result omits plan or
allocation costs, and neither is a sum of separately timed phases. The same
driver builds against immutable master with `DAL_SOA_PROFILE_BASELINE`, which
selects its unchanged compiled scalar entry point. Both builds use identical
dependencies, toolchain and flags. Formal pairs cover vanilla L16/16387,
weekly L32/16387, frequent divergence L32/16387 and weekly L32/7, /9, /65,
with two rounds of ten alternating pairs in each cost mode. A 4% two-round
threshold classifies changes; 2x is not an adoption condition.

Production code owns the new plan implementation and has no experimental
dependency. The frozen study retains its historical optional fallback for
reproducibility; it is not another production backend. Maintenance cost is a
bounded opcode decoder and mask executor plus an exact-BS batch generator,
using existing step math and driver RNG/chunk contracts. New opcodes require
explicit admission and parity evidence before this subset is extended.

## Measured adoption boundary

Baseline is immutable master `3be8dab9`; final measured production source is
`e43237f4`. Two rounds of ten alternating process pairs in each cost mode
produce these best-of-ten speedups:

| Case                | Steady R1/R2 | Cold R1/R2  |
|---------------------|--------------|-------------|
| vanilla L16/16387   | 1.801/1.816  | 1.814/1.891 |
| weekly L32/16387    | 1.372/1.434  | 1.384/1.358 |
| divergent L32/16387 | 1.479/1.517  | 1.511/1.509 |
| weekly L32/7        | 0.983/0.980  | 1.016/1.016 |
| weekly L32/9        | 0.964/0.965  | 0.990/1.007 |
| weekly L32/65       | 1.180/1.175  | 1.068/1.029 |

All 240 final formal pairs have identical payoff sums, exceeding the requested
1e-12 agreement. Adopt the measured large vanilla, weekly and frequent-divergence
shapes. Seven- and nine-path costs remain in the 4% no-change band. The 65-path
cold benefit is inconclusive under the two-round rule, so there is no blanket
all-size speed guarantee. Prepared named observations have correctness coverage
but were not separately timed. The shared WSL2 machine had unrelated background
load; raw minima and both rounds are retained.

Initial `fca6198a` sampling found a 12.08%/14.32% nine-path steady loss. The
`f73c84fe` repair moved the scalar bypass below sixteen paths, with 15/16/17-path
boundary coverage. Its intermediate executable differed only at four compare
bytes and retained identical read-only constants; that proof and 40 repair pairs
remain traceable history.

The first PR snapshot at `7e478f90` found two complexity warnings and a benchmark
inventory failure. Admission was split into prepared/sample/observation checks
without removing conditions or changing their order, discard decoding moved
out of the stack decoder, and the profile joined the existing `script_mc_perf`
target. Because setup code and the harness changed, all six scenarios and both
cost modes were refreshed for final acceptance. The evidence bundle includes
final and earlier source/dependency/build identities, executable hashes, commands
and every raw output; earlier timings are not substituted for final evidence.

All seven new regressions pass normally and under ASan/UBSan on the new
header/test translation units. Three unchanged tree/scalar/AAD caller checks
retain their valid passing evidence. The integrated profile smoke and local
documentation integrity check pass. Full regressions and platform gates remain
CI work; production review is independent of the old prototype review.
