# F01 Python Dupire binding review

Verdict: Comment Only. No unresolved local correctness or API findings;
exact new-head CI, Excel and common curve integration remain open.

## Findings

Read the complete changed public adapter, Python registration/tests, native risk
test repair, installed consumer, specification/API/critique, methodology and
Python guide. The public BS convenience copies deterministic carry through a
private IVS and delegates to the accepted core without changing that kernel.
The Python factories project the core/Hybrid snapshot and seed identities,
retain the GIL while sampling custom IVS, and release it only after copying
passive inputs for native pullbacks. Matrices, axes, configuration and surfaces
are detached on read; callbacks are not retained. Explicit type checks reject
required None, bool/enum numeric coercion and invalid names.

The initial public compile fails on the absent BS overload; all 31 initial
Python interface checks fail before implementation. A first configure failure
catches direct core includes in the binding. Routing IVS through the public
header and BS types through public models resolves the existing architecture
guard without relaxing it. The first numerical run passes all 84 quote rows,
but one new test incorrectly expects the word `adjoints` in an existing core
shape error. Correct its regex to the complete existing `seed dimensions
disagree; field=parameters` diagnostic. Preserve the failed log and unchanged
production error, steps, tolerances and numerical rows.

An expanded sanitizer run at the host's default 32-thread initialization finds
one strict map comparison failure in the pre-existing Hybrid/GSR risk test,
at ordinary parallel reduction roundoff. Its fresh isolated test initializes
one thread and passes; explicit one/four-thread controls also pass. A separate
consumer linked only against the unchanged accepted `c5c922ef` installed
libraries shows both legacy-to-structured and legacy-to-legacy differences of
`7.105427357601002e-15` for the interpreted Hybrid sample. This is not a new
binding or calibration gradient failure.

The test's original exact comparison and all 257 paths remain, with a scoped
single-worker configuration that restores prior pool state. A separate scoped
four-worker test compares every old/structured payload field for Hybrid/GSR,
tree/compiled and 257/2057 paths (eight cases) using the existing rel/abs 1e-10
numeric protocol. All original identity assertions remain. No production
simulation, reduction, calibration or derivative code changes in this repair.
The retained default-host failing log remains evidence; no assertion or CI
threshold is waived.

## Tests

Evidence root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- Public missing-overload RED: `aad-dupire-bindings-public-red-01.log`;
  focused GREEN: `aad-dupire-bindings-public-green-01.log`.
- Python missing-interface RED: `aad-dupire-bindings-python-red-02.log`;
  42-check GREEN: `aad-dupire-bindings-python-green-02.log`.
- Complete final OFF CTest: 2,383 passes, including 855 Python tests and 33
  regular examples; `aad-dupire-bindings-off-full-final-02.log`.
  The unchanged billion-path European example passed in the accepted Hybrid
  increment and is not rerun for this binding/test-only extension.
- Complete combined lifetime/profiling CTest: 2,397 passes;
  `aad-dupire-bindings-combined-full-final-02.log`.
- Fully instrumented ASan/UBSan: 30 public passes both at default host thread
  initialization and the CI four-thread environment;
  `aad-dupire-bindings-sanitized-final-{default,ci}-01.log`.
- Two installed C++ consumers pass in OFF and combined;
  `aad-dupire-bindings-{off,combined}-consumer-01.log`.
- Standalone Python compiles against the installed public package; 62 Dupire
  and existing structured-risk checks pass, with all 84 quote rows unchanged;
  `aad-dupire-bindings-installed-python-tests-01.log`.
- Independently installed Python versus unchanged accepted native C++ package:
  four flat/Merton/tree/compiled cases, 248 cells bitwise identical (surface,
  price, full model gradients, separate/total quote matrices);
  `aad-dupire-bindings-installed-parity-01.json`. Native helper, CMake, comparison
  source and raw output remain in `aad-dupire-bindings-native-control*`.
- All previous 321 native oracle rows and initial 84 Python rows remain
  bitwise identical: `aad-dupire-bindings-oracle-identity-01.json`.
- CCN-eight unchanged: 114 functions, zero warnings. Format and generated
  checks pass; 497 generated files have zero content drift. Only 16 identical
  output timestamps are restored after generation.

## Performance and remaining scope

All nine existing OFF performance-gate executables and the native core archive
remain SHA-256 identical to the accepted Hybrid measurement inputs, retained in
`aad-dupire-bindings-gate-identity-01.json`. This increment adds Python startup
registration and opt-in calibration factories; existing valuation entry bodies
and native hot loops are unchanged. The existing core/Hybrid additional-entry
cost reports remain applicable to their unchanged native operations. No new
Python timing result or production no-regression verdict is inferred from
numeric equality. P01 remains inconclusive on the shared WSL2 host.

The public Hybrid `c5c922ef` passes all 35 exact-head checks, which cannot certify
this later binding increment. Exact Python `12dcd09a` finishes with 33 successes,
one macOS ARM wheel failure and one dependent wheel-matrix skip. Seven new
Dupire cases fail the strict primal replay check on contracted arithmetic. The
[replay correction](aad-dupire-replay-rounding.md) retains its failing FMA
control, unchanged numerical limits and failed first cost implementation.
Final corrective cross-platform/wheel CI remains required. Excel handles/getters,
common curve adaptation and the remaining Stage B/C/D requirements remain active;
no full F01 checkbox closes.
