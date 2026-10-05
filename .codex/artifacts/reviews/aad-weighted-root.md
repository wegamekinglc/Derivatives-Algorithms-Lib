# F02 native weighted-root increment

Verdict: Ready for draft publication; complete weighted valuation remains open.

The increment adds `AAD::WeightedPayoffRoot` in a separate native header. It
records the ordered scalar weighted expression and validates nonempty matching
dimensions, finite weights/components and the resulting sum. It never assigns
component adjoints, so aliases accumulate correctly. Prefix-only outputs also
produce a path-local root without an extra independent zero parameter.

The existing scalar root/driver, tape representation and binding entry points
are unchanged. No legacy numerical/performance gate is rerun for this isolated
helper; actual prepared integration will require affected cost acceptance.

## Tests and evidence

Evidence is under `/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- RED: `aad-weighted-root-red-01.log` rejects the missing header. The first
  standalone compile then catches the missing default Vector template argument;
  the header explicitly uses `Vector_<double>` and compiles independently.
- Initial GREEN: `aad-weighted-root-green-02.log` passes the independent analytic
  objective `(9.5, 5, 3)` for value and two derivatives.
- RED: `aad-weighted-root-red-03.log` has four passing numerical/lifecycle cases
  and three failures for absent nonfinite-weight/component/overflow validation.
- GREEN: `aad-weighted-root-green-04.log` passes all seven weighted tests.
- Final OFF: `aad-weighted-root-off-final-01.log` passes seven weighted cases and
  the existing terminal scalar-root case. The alias test runs 257 suffix reverses
  and one prefix reverse for each of two requests with distinct signed weights.
- Combined diagnostics/profiling + ASan/UBSan: `aad-weighted-root-combined-sanitized-01.log`
  passes the same eight cases with leak detection and halt-on-UB. Compile the
  current tests, recording, tape and profiling sources with instrumentation;
  reuse unchanged cached library support and Google Test. This is focused AAD
  instrumentation, not a full instrumented library/build claim.
- Formatting passes for both new C++ files. The helper has CCN four. The
  documentation checker passes all 139 Markdown files.

No generated enums/worksheet files or submodule pointers change. Publication
checks must use this new PR's own head; merged #480 acceptance cannot substitute.

## Remaining controlling requirements

The [specification](../specs/aad-weighted-script-risk.md),
[API note](../api-notes/aad-weighted-script-risk.md) and
[critique](../critiques/aad-weighted-script-risk.md) keep output identity/preflight,
payload budgets, prepared worker batches, passive owning results/provenance,
independent common-path oracles, Python/Excel boundaries and affected scalar
cost checks open. The active helper return stays inside its live recording.
Blocked Jacobians and portfolio timelines remain separate F02 requirements.
