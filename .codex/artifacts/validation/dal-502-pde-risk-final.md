# DAL-502 final S3 numerical qualification

**N1–N12: 12 PASS / 0 FAIL / 0 INCOMPLETE.** Coordinator adoption decision
`01a1ba61-8784-74fe-89bf-29c603550a07` accepts the reviewed bounded
`BBS-last-step-v1` reference for N3/N6/N7. The last code blocker is closed at
`179f50c6f3f7a6ca9f7dfcfdac808ba3d505f3f7`. This is the current S3 entry for
[PR #685](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/685);
all linked checkpoint verdicts and original evidence retain their identities.
It does not establish S4/S5 or full-feature release acceptance.

## Adoption and limits

The [active adoption contract](../specs/dal-497-pde-risk-acceptance.md#adopted-bounded-bbs-tree-reference-dal-502)
releases the earlier candidate-only restriction for exactly the twelve frozen
constant-carry [actual bump bindings](dal502-bbs-candidate/binding-summary.json).
BBS preserves CRR u/d/p and discount, uses independent scalar European BS at
the penultimate layer with the American obstacle, and retains the obstacle
through rollback. The private stable-tail arithmetic is from `8607092c`.
It is an independent reference, not a production pricing mode or public API.

The corrected [full-vector proof](dal502-bbs-candidate/result-qualified.json)
at `8c3e272e2b2e94140642dfc054fe54a4701ab599` qualifies all six components
at each actual coarse/fine/extra-half scale, retaining both raw members at
4096/4097, 8192/8193 and 16384/16385. Original strict quarter mesh, parity and
fine-to-extra-half limits are unchanged; maximum quarter ratio is
**0.8661478662 < 1**. No favorable intermediate layer or parity was selected.

The [required-layer repair and replay](dal-502-candidate-p2.md) at `179f50c6`
retains **42/42** affected case comparisons, **756/756** original residual /
capacity checks, four premium/continuation controls and **14/14** American
direction / finest comparisons as PASS. Maximum product hard-limit ratio is
**0.3100466313**. All required layers' existing smooth, residual and capacity
predicates propagate to directions and N6/N7; reference accuracy remains a
finest-product-layer requirement. The original attachment and sixteen
counterfactual failures are distinct from the unchanged real captures.

Product hard limits, fixtures and Q/C/L caps remain frozen. The adoption
qualifies only the reviewed fixtures, scales and levels; empirical refinement
is not a rigorous continuous-model error bound or arbitrary-input guarantee.
The [original product FAIL / CRR INCOMPLETE checkpoint](dal-502-pde-risk-qualification.md),
[bounded pure-CRR stop](dal-502-pde-risk-followup.md#independent-reference-qualification),
[failed old-producer BBS pilot](dal-502-bbs-pilot.md), and superseded
[first analysis](dal502-bbs-candidate/first-analysis.json) are preserved.
None is relabeled PASS or assigned the repaired producer's identity.

## Final N1–N12 evidence map

Every row uses the frozen [acceptance fixtures](../specs/dal-497-pde-risk-acceptance.md).
The [follow-up groups](dal502-followup/groups.json) map unchanged groups to
individual captures, reference bindings and direct tests. Their
[risk errors](dal502-followup/risk-errors.json),
[scenario residuals](dal502-followup/scenario-residuals.json) and
[direction / contract checks](dal502-followup/directional-and-contract-checks.json)
remain the detailed data, rather than duplicated tables here.

| Exit | Final | Applicable evidence                                                                                                                                                                                                 |
|------|-------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| N1   | PASS  | [Follow-up](dal-502-pde-risk-followup.md#integrated-n1n12-exits): twelve analytic Q cases and separate default-upper control.                                                                                       |
| N2   | PASS  | [Follow-up](dal-502-pde-risk-followup.md#integrated-n1n12-exits): analytic/contact and two-segment partials.                                                                                                        |
| N3   | PASS  | Adopted twelve-binding proof and [replayed products](dal502-candidate-p2/products-replayed.json): four mandatory full-risk comparisons with positive premium and continuation.                                      |
| N4   | PASS  | [Follow-up](dal-502-pde-risk-followup.md#integrated-n1n12-exits): eight LOG-LCP / exact-event cases with individual carry partials.                                                                                 |
| N5   | PASS  | [Follow-up](dal-502-pde-risk-followup.md#integrated-n1n12-exits): deterministic ordering, repaired short expiry and deep intrinsic exercise.                                                                        |
| N6   | PASS  | European/Bermudan follow-up plus [American direction captures](dal-502-american-directions.md#separated-direction-results) and adopted finest-reference [replay](dal502-candidate-p2/products-replayed.json).       |
| N7   | PASS  | European/Bermudan follow-up plus all ten American domain/penalty/spot/parameter directions and adopted finest-reference [replay](dal502-candidate-p2/products-replayed.json).                                       |
| N8   | PASS  | [Follow-up](dal-502-pde-risk-followup.md#integrated-n1n12-exits): explicit C-profile CN, residual/capacity, Q comparison and its independently qualified 4/2/1 TREE binding.                                        |
| N9   | PASS  | [Repair](dal-502-pde-tail-repair.md#red-and-green) current-library direct tests, [follow-up](dal-502-pde-risk-followup.md#identity-and-applicability) 40-layer / 32-active-set audit and real strict-limit failure. |
| N10  | PASS  | [Follow-up](dal-502-pde-risk-followup.md#integrated-n1n12-exits): boundary brackets, degeneracy/domain, upwind and analytic stress cases.                                                                           |
| N11  | PASS  | [Follow-up](dal-502-pde-risk-followup.md#integrated-n1n12-exits): sub-cell rejection, finite-scale Gamma, independent spatial checks and narrow test.                                                               |
| N12  | PASS  | [Follow-up](dal-502-pde-risk-followup.md#integrated-n1n12-exits): LOG-LCP, segment metadata, parallel partials and selectors.                                                                                       |

N8 retains its original qualified reference; BBS adoption is required to close
N3/N6/N7, not to rewrite previously admitted groups. Coverage is 154/162 current
base captures plus eight N9 controls covered by accepted current-library direct
tests. The [view-source map](dal502-american-directions/view-sources.json)
preserves those distinctions; eight historical raw N9 rows are not promoted.

## Source, configuration and executable applicability

The reviewed code head is `179f50c6`, not a new numerical producer. Markdown
closeout adds no pricing, BS, tree, product-test or performance execution.
Reuse binds:

- Product source `015885814288b8f884594430b03a557c4bdae865`; core library
  SHA256 `7317377c982fd132c20bc8f74634feeb0ad422aa8d1d9dd424deb41077daa125`.
  [Repair identity](dal502-repair/identity.json),
  [follow-up identity](dal502-followup/identity.json) and
  [American identity](dal502-american-directions/identity.json) retain all
  capture/input hashes, dependencies and producer distinctions. Most captures
  use `6f27eb2d` numerical sources / executable
  `473d7b87a630e7e7a664e09e8c13b7d23c7dc681f5f189d05f1f7013e6bb2a2d`;
  reused repair captures keep their original executable identity.
- Product core configuration: Release, GCC 15.2, C++17, AADET, pinned Eigen,
  native tuning OFF; public/Python/Excel/portable and benchmarks OFF, core
  examples configured ON. Existing standalone core/tool build evidence applies
  to these unchanged sources; it does not replace S5 example/consumer exits.
- BBS source `8607092c8ccd6c9c1c2df1a08387577112e9570b`; native SHA256
  `f6a72ce4dc17ac5bd62562ff33e73a21db4cac94aae0e36d5ff26a4bf5b8b518`;
  configuration hash
  `4eea84d21d1e98de6afafec079ba56eeafcfc6bbbe2ad92106b3efdfa3bad9f9`.
  The [restart identity](dal502-bbs-restart/identity.json) and
  [full-risk identity](dal502-bbs-candidate/identity.json) bind compile commands,
  exact dependency commits, source snapshots and standard-runtime-only link.
  Exactly 84 same-producer prices were reused; 708 new prices completed once.
  No old-producer or pure-CRR identity bridge supplies a BBS value.
- Audit control `09640b0a` preceded computation; actual-scale correction
  `55071227` and full proof `8c3e272e` retain the superseded first receipt.
  Propagation fix `8a7df04d5a702bb694bd823e6dd30bbffd3ffc11` has product-audit
  SHA256 `d179400adadeb6bf595180463a96b1158a8768f4f9ae0a9872e31f55168872f9`.
  [Repair identity](dal502-candidate-p2/identity.json) and
  [146 unchanged file hashes](dal502-candidate-p2/unchanged-inputs.json)
  bind the final same-raw replay. Final audit source cannot impersonate an
  earlier executable; original ANALYTIC / LOG-LCP bridges retain their scope.

Commands, logs, RED/GREEN and archive member hashes remain in the linked
records. Raw transport manifests are available for the
[production repair](dal502-repair/raw-archive.json),
[follow-up](dal502-followup/raw-archives.json),
[American captures](dal502-american-directions/raw-archives.json),
[initialization repair](dal502-bbs-repair/raw-archive.json),
[restart](dal502-bbs-restart/raw-archives.json),
[full-risk stage](dal502-bbs-candidate/raw-archives.json) and
[propagation repair](dal502-candidate-p2/raw-archives.json).
Archives were delivered as DAL-502 attachments; recorded native execution
commands are evidence, not instructions to restart numerical work.

## Retained review decisions

These are dal-reviewer decisions on the DAL-502 triggering thread, identified
by exact comment ID and fixed head. They are not GitHub approval submissions.
The final adoption explicitly retains the earlier applicable review scopes.

| Scope                               | Fixed head | Review comment / disposition                                                                                            |
|-------------------------------------|------------|-------------------------------------------------------------------------------------------------------------------------|
| Production / initial tools          | `ac78bfb1` | `01a1b8eb-8a20-74a6-a155-609591d90e91`: production accepted; view P2 required repair.                                   |
| View repair / American captures     | `d96a89e8` | `01a1b907-464f-7a50-9726-5ba9cc9679e5`: Approve; original P2 closed.                                                    |
| BBS method / private tail           | `8c90d53b` | `01a1b94d-d29c-7480-97e6-8d6b4e64ab41`: Approve; six BS calls imported once.                                            |
| Repaired-producer pilot             | `9af285a8` | `01a1b95e-d573-76e5-a35b-f6d3f8726f7a`: Approve within spot-only scope.                                                 |
| Full-risk proof / product audit     | `8c3e272e` | `01a1b97a-b1af-7e94-88ac-66854603cc06`: proof accepted; required-layer P2 remained.                                     |
| Required-layer P2 / same-raw replay | `179f50c6` | `01a1ba5e-e528-78af-84d9-418441d1b104`: Approve; last P2 closed, no remaining code/numerical blocker in reviewed scope. |

## Final resource account

The [full-risk ledger](dal502-bbs-candidate/resources.json) and
[restart reconciliation](dal502-bbs-restart/resource-account.json) separate
actual consumption from reservations, failed attempts and imported review
work. All old stages remain charged; the six review BS calls are already in
prior actual consumption and are not charged again.

| Account                                           | Charged nodes  | BS evaluations |
|---------------------------------------------------|----------------|----------------|
| Prior BBS actual, including repair/restart/review | 8,869,730,816  | 1,511,876      |
| Full-risk stage reservation and matching actual   | 41,587,475,848 | 6,766,946      |
| Final BBS actual                                  | 50,457,206,664 | 8,278,822      |
| BBS cumulative hard cap                           | 51,000,000,000 | 9,000,000      |

Immutable pure-CRR cost is **2,997,881,833,992 nodes**. Combined actual is
**3,048,339,040,656 < 3,048,881,833,992**. New-stage actual is below its
42e9-node / 7e6-BS caps. Its native window is 23.183981657 s, peak worker
RSS 26,908 KiB with at most four workers (aggregate bound 107,632 KiB), and
final execution raw 1,775,800 bytes; original 600 s / 128 MiB per worker /
512 MiB aggregate / 64 MiB limits hold. The propagation repair, its focused
review, adoption and Markdown closeout add **0 nodes / 0 BS / 0 product
evaluations**. No unused allocation authorizes another attempt.

## Documentation decision and remaining exits

The production significand/exponent solve and per-row residual measurement
are a significant numerical-method change affecting the existing price API.
The published [early-exercise guide](../../../docs/methodology/pde/early-exercise.md#original-lcp-residuals)
therefore includes a small current-state solver explanation, with one tail-repair
[CHANGELOG entry](../../../CHANGELOG.md#2026-11-08). Baseline integration
preserves DAL-506's core risk calling guide, catalog entries and separate
core-capability CHANGELOG entry, with numerical status reconciled to this
bounded qualification. The private BBS reference,
adoption history and qualification tables stay in active artifacts. No new
methodology file is added or renamed, so the docs and CLAUDE methodology
catalogs retain DAL-506's core-risk coverage. Public/Python/Excel risk API
recipes and complete release examples remain S4/S5 deliverables.

The accepted production RED/GREEN, 51/51 direct tests, four independent
80-digit row audits and scoped old-price performance remain applicable.
Method 4/4 tests, 41 high-precision witnesses, the 113-input pilot, seven
candidate tests and eight propagation tests are reused with their recorded
identities. Markdown closeout checks content, local links/anchors, tables,
whitespace and final newlines with the repository documentation checker;
it does not repeat numerical, build, product or performance tests. The checker
passes for 135 Markdown files; `git diff --check` passes. Sixteen final replay
source hashes and the production source hash match their accepted identities.

The reviewed code head `179f50c6` had **36/36 checks passed**; that snapshot
does not certify a later documentation head. Final-head gate inspection and
actual merge belong to the coordinator. PR #685 remains draft / no auto merge;
DAL-502 and parent DAL-497 remain in progress for this handoff. Baseline
`cd90d13e6f43c716a06b676bf394d97ef256071f` (DAL-506 / PR #686) is integrated
by an ordinary merge retaining reviewed commit identities. Four Markdown
conflicts are reconciled alongside automatically merged status prose;
DAL-506's documentation-record rename and core calling guide are retained.
All non-Markdown tracked entries, including code/tests/build/dependencies/CI,
remain identical to `716a98c23ade677a5d51470b28a9c5314a042709`. Final-head gates
remain separate from prior green snapshots.

S4 / DAL-503 still waits for actual S3 merge and must implement and validate
public C++ / Python / Excel projections, strict inputs, copy/lifetime, GIL,
status/table equivalence and relevant B1–B7/B10 checks. S5 / DAL-504 still owns
selected risk/projection performance, independent core and installed
`DAL::cpp` / `DAL::public` consumers, separate component-owned examples and
complete release docs. Real Windows/MSVC XLL build, registration/help and
worksheet COMPLETE/PARTIAL evidence remain required; Linux portable tests or
S3 CI do not supply that evidence. No S4 start, merge, full-feature release or
parent completion is claimed by this numerical qualification.
