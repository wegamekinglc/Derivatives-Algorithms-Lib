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

`compiled_soa_perf` measures whole production requests. `steady` reuses a parsed
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
