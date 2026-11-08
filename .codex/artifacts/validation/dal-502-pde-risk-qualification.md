# DAL-502 PDE risk qualification

**S3 is not qualified: FAIL in N5/N7 and INCOMPLETE in N3/N6/N8.**
Do not release S4 from this evidence. No production algorithms, defaults,
thresholds, reference levels or budgets were changed. The two failing core
regressions must remain visible pending an implementer repair on this branch.
The frozen CRR reference needs an explicit follow-up decision; it cannot be
silently extended or replaced.

## Scope and identity

Product source is merged S2 `1b8c92e8f23320b2723628fd37b399455c918e23`
(PR #684, head `4ec4925bf26751b4b71ede7a9925ed47f7dc21bb`). Its accepted
review/36-check evidence is reused for unchanged implementation scope. This
record supplies new numerical evidence, not a rerun of those gates.

All twelve groups use the source, dependency, configuration and executable
mapping in [identity.json](dal502/identity.json). The production diff is empty.
The configuration is Linux x86-64, GCC 15.2, CMake 4.2.3, Release, C++17,
native AADET, Eigen at its pinned submodule SHA, native CPU tuning off.
Public/Python/Excel/portable are OFF; core examples are configured ON.
Production flags include `-O3 -DNDEBUG -fPIC -ffp-contract=fast`.

| Executed artifact                | SHA-256                                                            |
|----------------------------------|--------------------------------------------------------------------|
| Primary capture/reference driver | `06884f2f2f8b54de9988bd5f51b7fe1bc566401d8b76b085e9e4437789a1a2fc` |
| Supplement/replay driver         | `2d7515ae5fd546878296f0058f368b1826e9bc27de711ac7c19af8beb134036e` |
| Unchanged core static library    | `c57d4d11c125717991ca0faf2b58b368ce42332fdbe65a97a8f6804b804334b1` |

The supplement driver adds observation/replay operations. Four fixed ordinary
result controls (N1, N4, failing N5b, N9a) are identical between the two drivers.
Primary captures retain their own executable identity. The observed N9a binary
and final Google Test executable have separate hashes in the identity record.

Q/C/L caps are exactly the frozen explicit qualification caps, including
valuation work 1e9 and risk work 1e10. The default-upper control retains D
budgets. No adaptive fixture search, tolerance relaxation or cap escalation was
used. No reference value is derived from the product evaluator's differences.

## Group verdicts

Every row binds the common identity above and the exact original inputs in
[fixtures.json](dal502/fixtures.json). Risk/scenario identities, raw residuals,
both stencil scales and work/capacity records remain in the attached raw
archives. [Numerical results](dal502/numerical-results.json) contain per-fixture
normalized errors and failures; [separated checks](dal502/directional-and-failure-checks.json)
contain all refinement vectors. Contract-only cases use their accompanying
audits rather than a numerical-reference comparison.

| Group | Verdict    | Evidence and remaining qualification                                                                                                                                                                                                                            |
|-------|------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| N1    | PASS       | All 12 Q PUT/CALL × spot × yield cases COMPLETE; every analytic risk passes. D default-upper control resolves 480 with one common plan.                                                                                                                         |
| N2    | PASS       | Three spots and the two-segment variant COMPLETE against analytic segment partials. Base and all eligible nonpositive-yield scenario nonterminal regions independently inspected and empty.                                                                     |
| N3    | INCOMPLETE | All four products COMPLETE, positive exercise premium and CONTINUATION. Frozen CRR fails Gamma mesh, parity and bump self-qualification; no product accuracy verdict from that oracle.                                                                          |
| N4    | PASS       | Eight fixtures COMPLETE against qualified LOG-LCP, including four carry partials. Exact C/E anchors, single coincident node, projection order and segment indices pass.                                                                                         |
| N5    | FAIL       | Six deterministic order/style controls and two deep-exercise controls pass. All four required two-day node fixtures fail; four half-cell diagnostics also fail.                                                                                                 |
| N6    | INCOMPLETE | All eight separated direction sequences have available intermediate/final risks and pass last-step limits. European and Bermudan final-reference checks pass; two American fixtures inherit the N3 reference gap.                                               |
| N7    | FAIL       | Three upper=800 fixed-dx cases fail the solver. Other domain, all penalty, fixed-grid spot-bump and parameter-bump checks pass their directional limits. American hard-reference qualification still inherits N3.                                               |
| N8    | INCOMPLETE | Both explicit CN products COMPLETE, 5002 actual steps, admitted original residuals and common arrays; both agree with Q implicit. European analytic check passes; American TREE accuracy remains unqualified.                                                   |
| N9    | PASS       | 40 observed L-profile layers versus 32 active sets each; independent final iteration/LCP/linear residuals. Typed penalty/iteration/real linear failures and exact cumulative work controls pass; private scenario propagation covered by the 32 existing tests. |
| N10   | PASS       | Prescribed boundary midpoint and ±4h controls pass independent bracket/membership checks. Zero/near-zero diffusion, both spot-domain edges, ten analytic stress cases and upwind diagnostic pass.                                                               |
| N11   | PASS       | Sub-cell bumps rejected at node/half-cell before any rollback. Valid h=.5 has finite-scale tags and nonzero qualified Gamma. Separate node/half-cell spatial Gamma differences pass.                                                                            |
| N12   | PASS       | Both knots, both bump sizes, canonical metadata and independent segment comparisons pass. Parallel partial checks/self-refinement and all 15 selector combinations pass; malformed lengths/all-off rejected.                                                    |

The base matrix has 162 owning result captures. Supplements add four N2 region
audits, three N10a boundary cases and four N12 parallel audits. All 13 supplement
checks pass. Final audits contain 2774 checks; their three failed entries are
the N7 domain sequences. These audit counts do not replace the N5b failures
or N3 reference verdicts in the table.

## Product failures requiring repair

All failures below retain unavailable price/risk values and the original
diagnostics. They are mandatory smooth cases, so BASE_FAILED cannot be used
as an accuracy exemption.

| Reproducer        | Observation                                                                      | Frozen limit |
|-------------------|----------------------------------------------------------------------------------|--------------|
| `N5b-2028-0-100`  | First solve, time index 7999, backward error `1.0397496282895078e-5`, work 15999 | `1e-12`      |
| `N5b-2028-1-100`  | Fourth solve, time index 7996, error `1.7368390335271222e-11`, work 63996        | `1e-12`      |
| `N7-0-domain-800` | First solve, error `0.0019801980198019802`, work 3199                            | `1e-12`      |
| `N7-1-domain-800` | First solve, error `0.009345794392523364`, work 3199                             | `1e-12`      |
| `N7-3-domain-800` | First solve, error `0.014492753623188406`, work 3199                             | `1e-12`      |

N5b 2027 dates reproduce the same failure class without a leap-year
denominator. Both node and half-cell outputs are retained. These observations
identify the failure path, not a proven root cause. Production repair belongs
to the coordinator's implementer; this PR only adds reproductions/evidence.

Narrow Google Test reproductions:

```bash
DAL_NUM_THREADS=4 build/dal502-make/dal-cpp/dal_cpp_tests \
  --gtest_filter=EarlyExercisePdeRiskQualificationTest.TestFrozenN5bShortExpiryRequiresAvailableGamma
DAL_NUM_THREADS=4 build/dal502-make/dal-cpp/dal_cpp_tests \
  --gtest_filter=EarlyExercisePdeRiskQualificationTest.TestFrozenN7FixedSpacingDomainRefinementRetainsAllRisks
```

## Reference self-qualification

[references.json](dal502/references.json) records all 56 reference qualifications:
44 analytic formula checks PASS, eight LOG-LCP PASS and four CRR INCOMPLETE.
Analytic formulas are independently checked against scalar Python price
differences. Core tests also verify independent CRR/log European price controls.

CRR uses both raw parities at 4096/4097, 8192/8193 and 16384/16385. Final
derivatives use the extra half bump, without selecting a favorable parity.
The normalized Gamma self-limit is strict `<.005`:

| N3 fixture       | Last mesh difference | Extra-half bump difference | Final extra-half odd/even difference |
|------------------|----------------------|----------------------------|--------------------------------------|
| PUT S=100        | .010415989           | .085806282                 | .347950061                           |
| PUT S=120        | .031723363           | .022522435                 | .211708502                           |
| CALL S=100 q=.08 | .009703318           | .098035654                 | .395915245                           |
| CALL S=120 q=.08 | .013246428           | .008841481                 | .123470688                           |

All raw member values, pair means and three bump scales are retained. No tree
level was added after failure. Consequently N3 and its dependent American
checks remain INCOMPLETE even though their product stability screens pass.

LOG-LCP uses the frozen three log domains/node counts and 16/32/64 substeps per
calendar day, independent matrix assembly/Thomas solve, exact dated projections,
independent endpoint envelopes and quadratic spot interpolation. Its prescribed
reference mesh/domain changes are labeled together; they are never used as a
product separated-refinement sequence. Every N4 metric passes strict reference
refinement and bump-halving limits before its product comparison.

## Additional audit details

[N9a audit](dal502/n9a-audit.json) contains all independently assembled matrices,
right-hand sides, feasible enumerated solutions, observed layers and recomputed
residuals. The observation is inserted only in a build-directory source copy;
removing it restores the original source byte for byte. The entire ordinary and
observed owning result is equal. No public observer API or production edit was
introduced. Bump layers are checked independently; base layer residuals are
also compared with retained step diagnostics.

N9d caps 1326/1325/390/78/77 consume respectively 1326/1287/390/78/39 work.
Their aggregate states are PARTIAL/WORK_LIMIT/WORK_LIMIT/WORK_LIMIT/BASE_FAILED.
The coarse unlimited control is PARTIAL from stability screening; this work
bookkeeping case does not claim coarse numerical qualification.

[Supplement audit](dal502/supplement.json) includes every observed contact
bracket and membership, all parallel price pairs, and parallel-reference
refinement/bump tables. At the smaller carry step, normalized segment-sum
differences are at most `4.614e-10`, below `.003`. Independent parallel reference
bump differences are at most `3.604e-8`, below strict `.00075`.

## Commands and test separation

The [tool instructions](../../../dal-cpp/test-support/pde-risk-qualification/README.md)
give complete configure/build/capture/reference/observer/supplement/report
commands and fixed selector syntax. The executed reference phases selected
`N1,N2,N5b,N10d,N11`, then `N3`, then `N4`; only independent fixtures ran in
parallel. Exact invocations and captured output are in the raw `logs/` folder.

Existing tests, unchanged production source:

```bash
build/dal502-make/dal-cpp/dal_cpp_tests \
  --gtest_filter='EarlyExercisePdeRiskTest.*:EarlyExercisePlanTest.*' \
  --gtest_output=xml:build/dal502-evidence/existing-tests.xml
```

Fresh result: **32/32 PASS**. Full Linux/Windows suites were not rerun; unchanged
S2 gates are reused. No old `test_output.txt` is cited.

New tests:

```bash
DAL_NUM_THREADS=4 build/dal502-make/dal-cpp/dal_cpp_tests \
  --gtest_filter='EarlyExercisePdeRiskQualificationTest.*' \
  --gtest_output=xml:build/dal502-evidence/new-tests.xml
python3 -m py_compile dal-cpp/test-support/pde-risk-qualification/*.py
```

Fresh result: **5 PASS, 2 FAIL**, both required product regressions named above.
Python syntax checks pass. No production repair was attempted. The PR must stay
draft until the coordinator arranges repair and the reference gap is resolved.

Performance is excluded: changed files are independent test/reference tools and
qualification records; production numerical callers are unchanged. S5 owns the
frozen performance/installed-consumer/example gates. No build/example source
changed here; the fresh isolated core build nevertheless establishes that this
test code has no facade dependency. S4 bindings and Windows XLL evidence remain
with their assigned stages and are not claimed by this record.

Raw archive names, contents and SHA-256 fingerprints are in
[raw-archives.json](dal502/raw-archives.json). They are attached to the DAL-502
result comment. Runtime paths are execution instructions, not delivered files.
