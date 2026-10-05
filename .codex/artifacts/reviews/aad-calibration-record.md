# F01 native curve record foundation review

Verdict: Comment Only. No unresolved local correctness, methodology or API
findings. Exact new-publication CI remains required. This review covers opt-in
native record capture, not the unimplemented common pullback or its bindings.

## Findings

No unresolved local findings. The config flag is appended and defaults false;
old aggregate-prefix initialization and factory signatures remain valid.
One helper canonicalizes the existing state exactly once, computes its original
SHA-256 and moves the same bytes into immutable optional ownership. It neither
changes the state schema nor adds the storage preference to its fingerprint.
All four factories use it after their original validation and inverse retention.
The generic joint builder retains v2 routing, selected-solution mapping and
actual-mode result fields; the other builders retain their original v1 bytes.

The record getter returns a const byte-preserving std::string reference or a
stable empty string. Data remains shared and immutable; owned record bytes do
not borrow calibration inputs or market curves. Default construction retains
one ownership slot and no record text. Captured text reuses the canonical string
already required for hashing. There is no second JSON tree or canonicalization.
Inverse matrices, tolerance, curve solve, AAD operations and aggregation are
unchanged. Native config binary layout and structured-binding arity are not
promised; consumers rebuild as for other appended calibration config members.

Read the native builders/getters, changed tests, shared test checks, public
current-state guide and controlling common spec/API/critique. The published
guide advertises only implemented native capture. Python/Excel capture and the
shared result remain explicit future requirements. Record retention alone is
a preparatory capability, so no separate significant-methodology changelog
entry is added; the common public operation will need its own decision.

## Open questions and limits

No user decision is needed. Exact new-head cross-platform CI remains required,
including MSVC, ARM wheels and sanitizer configurations. The accepted 35 checks
at `533b6602` validate the preceding Excel correction, not this native change.
The common operation must still compare complete source content, preserve
direct-quote identity, reuse mapping without hot-path overhead, and complete
all three language/request/budget requirements. Every full F01 box remains open.

## Tests

Evidence persists under
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- `aad-calibration-record-off-red-01.log` proves missing flag/getter RED.
  Minimum single-curve GREEN precedes the other three provider REDs in
  `aad-calibration-record-extended-red-01.log`; retain all failed logs.
- A fast JSON test decoder initially changes one parsed quote by one ULP.
  Use RapidJSON's existing full-precision policy in the test helper only.
  Keep exact equality and original production arithmetic; all 13 records
  independently match their published SHA-256 digest and decode as JSON.
- Six focused cases cover all four kinds, ANALYTIC/BUMPED, generic joint
  layered/plain sources, default-false initialization, source destruction,
  input mutation, case-preserving bytes and actual inverse-disabled calibration.
  Default/captured axes, all inverse entries, state, bindings and tolerance
  match. Original canonical traces remain bitwise equal after test refactoring.
- Actual default/captured portfolio aggregation matches every price, decimal
  quote sensitivity, DV01, coordinate and metadata field exactly, including
  mixed actual PV currencies and signed generic joint portfolios. OFF/combined
  focused cases pass; ASan/UBSan also passes the six with leak detection ON.
- Final full OFF/combined CTest passes 2,400/2,414, including 857 Python
  cases and 33 regular examples OFF. Fully instrumented lifetime/profiling
  ASan/UBSan passes 105 relevant cases with leak detection OFF for Excel.
  Final runs after the extra portfolio assertions pass again:
  `aad-calibration-record-{off,combined}-full-final-02.log` and
  `aad-calibration-record-sanitized-final-02.log`. Keep the initial runs too.
- Old installed capture consumer fails only on the new flag/getter after its
  missing public include is fixed. Fresh OFF/combined installed consumers pass
  all eight workloads; old/default/captured fingerprints, inverse checksums and
  tolerance match. All failure evidence remains available.
- Clang-format checks pass; new or complexity-changed functions remain within
  CCN eight, and the shared helper's complete-file scan has no warnings.
  Documentation links/tables and patch whitespace receive fresh final checks.

## Performance and summary

The [performance report](../performance/aad-calibration-record.md) retains all
75 nine-target rows, eight default factory comparisons and opt-in record costs.
The nine-target gate and separate default cost acceptance pass the original
two-round/ten-process/4% policy. Eight old gate executables are byte-identical;
the changed rate-risk executable is measured. All eighty supplementary processes
preserve numeric checks and input hashes. Retained record sizes are 9,115–368,141
bytes in these cases. This is a bounded incremental no-regression verdict;
P01's production MC acceptance remains inconclusive.

The native foundation requires corrected publication and exact-head CI.
Full shared calibration/F01/AAD acceptance remains open.

## Publication correction

Initial publication `28b4e180` completes 35 checks with 12 successes, 22 build
failures and one Codacy action-required result. It is not accepted. Linux and
MSVC fail to find `rapidjson/document.h` in the new test helper: the dependency
was private to the core library but not declared for the native test target.
The local `/usr/local/include` copy masked that omission. Preserve the complete
check capture and representative GCC/MSVC failed build logs. Codacy separately
flags the read-only Json_ argument being passed by value.

Declare bundled RapidJSON privately on `dal_cpp_tests`; keep the installed
public interface unchanged. Read JSON through a const reference and move only
the canonical string into retained ownership. The generic joint caller no
longer moves its JSON tree. Actual local MSVC reproduces both test failures
under the old include flags; production/single/joint units each pass OFF and
combined with the declared path. Preserve raw bytes and decoded output because
this compiler emits localized messages. The first UTF-8-only runner fails to
decode them; the corrected capture retains raw output and detects its encoding.

Rebuilt OFF/combined full suites pass 2,400/2,414. Rebuilt ASan/UBSan passes
105 relevant cases; six native capture cases pass with leak detection ON.
The dependency file confirms tests now use the bundled header. All thirteen
canonical traces remain bitwise identical, and fresh installed prefixes each
pass the existing eight-workload consumer. Fresh corrected performance runs
pass all 75 nine-target cases, eight default factory costs and eighty numeric
process checks under the original policy. The updated performance report pins
the corrected native patch and fresh input hashes while retaining initial
evidence. Exact corrective publication CI remains required; retain every
original threshold and failed log.
