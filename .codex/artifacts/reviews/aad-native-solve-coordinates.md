# Native solve-coordinate local review

Verdict: Approve for publication; merge requires the current-head remote gates.

## Findings

No unresolved correctness, resource, compatibility or style findings.
Documentation review corrected the obsolete statement that native structured
coordinates require separate support, and aligned the active acceptance scope
with the diagnosed caller's shared publication/reverse helpers.

## Reviewed scope

Read the complete changed production header/source, four new test files and
active specification/API/critique/performance contracts. Checked the controlling
plan's remaining F03 requirements against the implementation ledger. Reviewed
the related matrix/AAD methodology and qualifying changelog entry.

The event owns one numeric coordinate cache, p parameter bindings, active RHS
bindings and solution slots. Symmetric pairing, active-zero parameters, alias
accumulation, exact-zero channel skipping and passive-parameter overflow
avoidance follow the accepted numeric/native contracts. Shared publication
preserves caller-owned outputs and diagnostic copies before commit. Begin
validation remains outside failure invalidation for null/wrong-thread calls.
Owned construction/destruction and scratch scopes retain existing accounting.
No Number/node/tape layouts, existing dense headers or binding surfaces change.

## Open questions

None blocking this C++ increment. Owning transpose residual reports and declared
accuracy policy, implicit calibration, PDE and later bindings remain open plan
requirements. Dense LU complexity and the absence of coordinate diagnostic
results are explicit public limitations.

## Tests

- Combined evidence accepts all 72 distinct affected cases, including 28 new
  analytic/independent-reference, mode/alias/checkpoint, resource and lifecycle
  cases. Only the two tolerance/invalid-input cases were rerun after repairing
  a fixture that incorrectly requested zero pivot tolerance; legal 1e-18 and
  explicit zero rejection preserve the numeric contract.
- GCC strict-warning syntax checks pass with lifetime/profiling OFF and both ON.
- Changed C++ formatting passes; all 101 examined functions meet complexity 8.
- Installed CMake DAL::cpp consumer runs the documented symmetric gradient,
  including the expected -1.4 derivative and -1 objective.
- Eight selected existing-interface performance comparisons pass the declared
  two-round 4% policy; the diagnosed cached borderline confirmation and all
  initial results remain retained. Three new-interface cost boundaries report
  packed/dense-adapter work and memory with their limitations.
- All 167 unchanged archive members match accepted bytes. The measured runtime
  source/archive hashes are recorded in the performance report.

## Summary and residual risk

The additive API composes packed parameters with native scalar/vector recording
without dense active expansion. Local acceptance is complete. Actual execution
of all 28 new cases in each of 14 MSVC, sanitizer and extended configurations,
current-head CI/Codacy and paginated review-thread audits remain remote gates;
local GCC evidence alone does not establish those platform results.
