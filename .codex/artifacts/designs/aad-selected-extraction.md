# Selected extraction ownership and admission

Status: active implementation design for [P03](../specs/aad-selected-extraction-block-policy.md).

The resource RED uses two live trades with 1,024 referenced private constants each,
a shared four-input BS owner and 1,048,577 paths in 129 original batches on one worker.
The full-input request supplies the reference capacity. The selected request asks
for three reordered columns and receives a quota smaller by half of the retained
batches' discarded numeric columns. Against reconstructed merged #488 production
it rejects in preflight: required 1,984,448 bytes, quota 1,981,104 bytes. The quota
never depends on the selected request's own peak. Full production sources, object
and executable hashes, commands and the failing log are retained under
`aad-selected-extraction-resource-baseline-03` in the session evidence directory.

The earlier five-batch fixture also rejected after extraction changed because
complete-axis metadata construction exceeded its quota. The revised fixture makes
retained numeric slots dominate that independent metadata floor. Every constant
participates in the historical prefix, because indexing removes unused declarations.
This also keeps forward path work small enough for focused tests. Both earlier
failures remain in the evidence directory.

`PortfolioInputColumns_` records the full source extent and either compact identity
selection or strictly increasing original ordinals. Explicit empty ordinals select
no numeric columns. `PortfolioGradientSelection_` owns model selections by owner
and private selections by original trade position. Complete global selection uses
the full-gradient route without allocating extra mappings, including reordered
requests for the complete axis. `ReplaySelection_` owns subset mappings
for the entire replay, including task draining after failures. Batches
retain only numeric arrays and original trade positions; they do not copy mappings.

Derive the full model prefix from sealed complete-axis families. The existing
prepared-axis validation checks those axes against actual models; preflight also
checks their extents against planned models. Keep full source extents for private-axis
offsets and validate core selections against actual model/private extents before
historical initialization. Scatter packed positions via original ordinals and the
complete model prefix into existing ordered public columns.

Private batch settings group scratch/tape budgets and the borrowed selection
pointer. A missing pointer preserves the full-gradient core reference route.
Private worker-admission settings group quotas, width, owner and live original
trade positions with the same borrowed mapping. This avoids extending already
long positional argument lists. Public request signatures and defaults are unchanged.

Preflight charges owning mappings, selected batch slots and worker replacement
arrays. Model/evaluator inputs, recording registration and tape estimates remain
complete. A block with no selected private columns still executes its live trades;
a selected risk of a trade with no selected output remains zero without additional
valuation or history. Native empty selection retains native work and matrix rows.

Local acceptance passes 41 core and 46 public affected cases. The resource fixtures
passes tree/compiled without altering its bound. New packed-array shape and extent
rejection/recovery tests preserve native zero-column rows; all six native families
match independent scalar risks across original batches, 1/4 workers and widths
1/2/3. The three-row Jacobian resource fixture retains width two, including its
padded tail and historical prefix alias, while returning only three columns.
Full P03 platform and measured-width acceptance remains.

Four early default-route performance cases retained one failure: the 257-path,
single-trade compiled weighted request regressed by 9.41% and 5.23% in two
confirmation rounds. Raw evidence remains in `aad-selected-extraction-initial-pairs-01`.
The corrective change bypasses subset-map and original-trade admission allocations
for complete selection. Fresh verification uses a new candidate executable;
no unchanged binary is rerun to replace failed evidence.
The revised candidate passes all four initial cases under the unchanged policy.
Round deltas are 4.73%/1.14% (small single trade), 15.29%/3.69% (small shared
portfolio), 4.76%/-2.25% (large width-one Jacobian) and -0.72%/-2.84% (large
passive weighted). None exceeds 4% in both rounds; variability remains visible.
This is partial evidence, not complete performance acceptance or a uniform speedup.
The frozen logs and binaries remain in `aad-selected-extraction-identity-pairs-01`.
