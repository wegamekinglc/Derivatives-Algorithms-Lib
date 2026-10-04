# Native AAD contract audit

Status: D00/D01/D02/D03 requirements are verified at publication source
`b42f9eb9c12a5987e8a81a869f59b38d8096a1de`; all 28 exact-head CI checks succeed.
This audit also identifies the evidence boundary for D01/D02 and remaining P01.
The full development goal, including Stages B/C/D, remains active.

The preceding goal turn made concrete progress: it completed fixed performance
confirmation, committed its full evidence, published the native-only increment
and corrected an exact-head static-analysis finding. It was not a status-only wait.

## Source and evidence identity

The [native-only report](../perf/aad-native-only.md) supplies frozen implementation
`8816951662fc6788f3e9876c45ac72f7e48c65f7`, original baseline, configurations,
dependency and binary identities, every performance case, initial failures,
controls/confirmation and limits. Publication `59d81b0` changes only active
evidence/contracts; `b42f9eb` changes only the CI matrix helper invocation.
The library, benchmark workloads, build/export configuration and generated
source diff from the measured implementation is empty. The 22 measured binary
digests are verified after measurement and the evidence-only publication.

Final local logs under `/tmp/dal-aad-evidence` establish 2,330 OFF and 2,359 ON
CTest passes, including 793 Python tests each and 34 examples. Each final
installed prefix passes 2/2 consumers; ten configuration/header cases, 60 focused
ASan/UBSan cases, generation/drift, 172 CI helper tests and 21 serial benchmark
smokes pass. These are precisely scoped evidence, not a completely sanitized
local library or a substitute for Windows/wheel/exact-head CI.

The initial publication's Codacy annotation flags the matrix test's dynamic
interpreter command. `b42f9eb` uses a literal `python` argv with
`executable=sys.executable`, preserving the exact interpreter, compiler matrix
execution and all assertions. Twenty focused and 172 full helper tests pass;
the corrected exact-head static analysis succeeds. No suppression or gate-policy
change is introduced. Retain the initial check/annotation and local logs.

## D00: native-only implementation

The controlling [D00 specification](../specs/aad-native-only.md) has eleven
requirements and ten acceptance criteria. Local conclusions below apply to the
actual implementation, including fresh checkout and exact-head CI.

- R01: `dal-cpp/dal/math/aad/aad.hpp`, `expr.hpp`, `node.hpp`, `tape.hpp` and
  `tape.cpp` contain the direct native implementation. External implementations,
  aliases/includes/guards and the empty external-definition translation unit
  are removed. Number/expression/node original copyright banners remain.
- R02: `.gitmodules` and the index have five retained gitlinks and no three
  external AAD gitlinks. Actual checkout pins match the baseline. All three
  external directories are physically absent during complete fresh builds;
  completed CI fresh checkouts independently confirm this state.
- R03: `dal-cpp/CMakeLists.txt` and its package template have no backend
  selection/link/install/export branches. Package identity is fixed `Native`.
  Both final installed prefixes contain no external export or old backend header.
- R04: one CMake boundary rejects each old ON variable, while unspecified and
  old OFF configurations succeed. The native tape header rejects defined old
  compile macros. `tests/native-aad/run.cmake` exercises all ten cases, including
  the package metadata assertion and independent header compilation.
- R05: `dal-cpp/dal/math/aad/native.hpp` replaces the old backend header.
  Seed replacement/addition, channel reads and active roots have concrete
  native callers/tests; no external specialization, adapter inheritance,
  backend-name dispatch, virtual interface or registry remains.
- R06: example and benchmark CMake files have no external includes/links.
  `dal-cpp/examples/aad/aad.cpp` validates native checkpoint accumulation against
  an independent passive/analytic price and five partials. All 34 examples run
  locally; the existing Windows example compile policy remains explicit.
- R07: Linux retains six native compiler PR legs and the coverage compiler;
  Windows retains native/diagnostic MSVC legs. Both diagnostic extended builds,
  sanitizer variants, warning-clean, generation/docs, bindings, consumers and
  stable gates remain and pass. Removed jobs have no `needs`/environment/gate references.
  Executed matrix and gate-shell helper tests verify actual policy behavior.
- R08: native mathematical, state/checkpoint, non-finite/channel, exception,
  concurrency and lifetime tests remain unconditional or diagnostic-specific.
  Only external-specific CoDiPack thread, XAD batch-memory and Adept stack tests
  are removed. `dal-cpp/tests/math/aad/test_native.cpp` uses independent VJP,
  alias/constant/identity, width/storage and prefix oracles.
- R09: diagnostic OFF remains the default and conditionally excludes lifetime
  fields/instrumentation. OFF number/node/tape/scope sizes remain 16/40/368/72
  bytes on the measured host. ON ABI definitions propagate PUBLIC and through
  installed targets; scalar production loops have no capability lookup.
- R10: current-state README/setup/architecture/methodology and CHANGELOG reflect
  native-only behavior. All active contracts use the amended plan. Protected
  `CLAUDE.md:35` still lists obsolete options and is reported, as required by
  AGENTS.md; no unauthorized Claude original is edited. Historical reports and
  explicit migration rejection retain the old names intentionally.
- R11: unchanged nine-target pairing passes 65 comparable cases; 25 curves pass.
  Initial affected production pairing has two failures, both retained. Complete
  published-head control and predefined original-baseline two-by-thirty
  confirmation pass all 44 cases. Four-node AAD fit remains +3.52%/+5.24%:
  passing the existing two-round rule is not an identical-runtime claim.

Acceptance mapping:

- A01/A02: inspected index/modules, physical directories, production/build/export
  search, both installed prefixes and fresh CI prove removal.
  Byte escapes ending in `xad` are Unicode test data, not library references.
- A03: the ten migration/header cases and installed native identity pass.
- A04: complete local OFF/ON core/public/Python/portable Excel/examples pass;
  Windows and all exact-head CI now have successful terminal results.
- A05/A06: both final installed consumers and analytic native test suite pass.
- A07: focused sanitizers, thread/epoch/generation tests, layouts and full
  regeneration and complete sanitizer-library CI pass.
- A08: fresh builds with absent external directories and both analytically
  checked AAD examples pass; no installed/stale external artifact is used.
- A09: local immutable formal/affected pairing satisfies existing acceptance,
  with all initial failures and positive movements retained.
- A10: current-state removal/migration/changelog and gate-reference checks pass;
  all 28 checks at the captured exact publication head succeed.

## D03: thin native operations

The controlling [D03 contract](../specs/aad-backend-adapter.md) describes native
operations, not an external-backend extension interface.

- R01: D00 removes the external code and selection; `native.hpp` is the only
  operation header, and scoped recording invokes native functions directly.
- R02: constexpr capabilities report actual scalar/vector/repeated/interval/
  prefix/scoped support, real maximum width and compiled diagnostic status.
  Independent nesting, reverse events and higher order remain false.
- R03: `SetSeed`, `AddSeed` and passive `ReadAdjoint` use actual indexed vector
  storage, including channel zero. Reference-alias seeds accumulate; repeated
  weighted materialized graphs give the independent analytic gradients.
- R04: width/mode/channel and missing vector storage reject before access or
  mutation. Invalid requests preserve the existing valid seed/graph, and a
  subsequent reverse still gives its independent result.
- R05: `ActiveRoot` uses existing `PayoffRoot`; constant and direct-input outputs
  preserve their zero/identity derivatives without resetting the recording.
- R06: ON validates owner/epoch/live interval/generation before unsafe node
  access; discarded/reset/live-foreign/exited-thread tests cover these rules.
  OFF has no lifetime registry or general stale-handle safety claim.
- R07: registration, new-recording no-op, full gradient clearing and suffix
  restore remain distinct. Full clearing covers scalar fields and every vector
  slot; three restored suffixes accumulate into one independent prefix reverse.
- R08: retained fixed-graph weighted sweeps pass after explicit clear/reseed.
  Closed/discarded activity cannot be revived; passive results are extracted
  before cleanup. Scalar and vector cases exercise consumed intermediates.
- R09: scoped phase/mode/thread/token/ownership checks and controlled reverse/
  cleanup failure/recovery remain. No external activation/disposal distinction
  is carried into native lifecycle operations.
- R10: NativeOperations_ is stateless; no default node/number/tape field or
  adapter allocation is added. Existing propagation specialization remains;
  successful scalar paths avoid error strings and dynamic dispatch.
- R11: recording-state Machinist markup and generated outputs preserve the six
  states/transitions. Full generation/drift and installed headers pass.
- R12: errors identify the native operation/constraint; precondition rejection
  preserves usable graphs. Business/reverse/cleanup/extraction exceptions retain
  their required identity and next-request recovery, without a successful
  partial-risk result.
- R13: both exported OFF/ON packages and consumers share diagnostic definitions
  and execute independent scalar/vector oracles. Bindings retain passive values;
  no active tape slot or external dependency is exposed.
- R14: local full suites, consumers, bindings, focused sanitizers, layout,
  review, immutable formal/production/resource evidence and all exact-head CI
  pass; earlier external results are historical only.

Acceptance mapping:

- A01: D00's inspected removal and fresh native builds prove local implementation.
- A02/A03: weighted/repeated/reference-alias/constant/identity and vector-channel
  tests, including widths 1/3/5/7/10/16 and a zero channel, pass independent oracles.
- A04: invalid mode/channel/unbound/missing-storage preservation tests pass.
- A05: ON stale/foreign focused ASan/UBSan, OFF layouts and complete
  sanitizer-library CI pass.
- A06: prefix/lifecycle/recovery tests and ordinary/LSM task-drain suites pass.
- A07: full generated-source drift and both installed consumers pass.
- A08: capability values match exercised operations; nesting/mode requests reject
  explicitly. Reverse-event/higher-order request APIs are not provided or claimed.
- A09: fresh local OFF/ON functionality/consumers/bindings and exact-head
  required CI pass.
- A10: fresh unchanged formal and all affected production pairing passes under
  policy, with initial failures, confirmations and resource limits retained.

## D01/D02 and remaining Stage A evidence

The current [D01 lifecycle contract](../specs/aad-recording-lifecycle.md) is checked
against the actual scope implementation, its two state/ownership test suites,
analytic curve envelope, simulation workers and LSM replay. In particular,
`dal-cpp/dal/curve/calibration_internal.hpp` already calls StartRecording and
FinishRecording around residual construction. Its raw Jacobian harvester and
the rate/GSR compatibility envelopes are not proof of a missing analytic-curve
migration. No new C++ change is justified merely by finding a low-level call.

R01/R02: non-copyable/non-movable default-tape ownership and all six phases are
implemented. R03/R04/R05: analytic scalar/vector/prefix/repeated/checkpoint/mode
tests cover accumulation, opaque token rejection and full storage clearing.
R06/R07: explicit close, noexcept cleanup, retained poisoned context, rebuild,
foreign-thread rejection and independent thread tests cover recovery. R08: curve
start/finish/close, per-worker MC and separate LSM training/replay lifetimes,
checkpoint windows and task-drain tests cover the required migration.

Acceptance A01–A10 has direct mathematical/state/failure tests, including actual
post-reverse result-extraction failure and the next analytic request. A11 has
full applicable local suites and successful exact CI. A12 has full formal and
affected performance, complete LSM numeric comparisons, resources/capacity and
independent mathematical tests. The initial evidence gap is now filled by the
[production numeric comparison](../perf/aad-recording-numeric-validation.md):
ten ordinary MC and all 25 curve cases, 34,028 finite numeric cells, full risk
labels/vectors, residuals and Jacobian/inverse matrices agree at rel/abs 1e-10.
The largest absolute difference is 1.42e-14. Actual existing workload fixtures,
archives and settings are preserved. It is untimed validation; no timing policy
or benchmark is changed. Other unchanged kernels are not claimed as a complete
numeric matrix, and no later source change is accepted by this snapshot.

The [D02 lifetime contract](../specs/aad-native-lifetime-diagnostics.md) is
verified against native tape/expression source and
`dal-cpp/tests/math/aad/test_lifetime.cpp`:

- R01: CMake default-OFF option, PUBLIC definition and installed metadata/targets
  propagate the same layout to public/bindings/consumers; both consumer builds pass.
- R02: conditional number/node/tape fields and validation bodies are absent in
  OFF; layout probes match the baseline and OFF tape symbols exclude diagnostics.
- R03: ON adds explicit opt-in storage (64/56/424-byte number/node/tape), with
  matching ON tests. All release pairing compares OFF with OFF.
- R04: owner identity is nonzero and independent of reused addresses/thread IDs;
  live-foreign and exited-thread tests reject before old context/node access.
- R05: full reset increments epoch before releasing/reusing storage; full-rewind
  and Clear/fresh-allocation tests reject stale activity under sanitizers.
- R06: live/marked logical counts distinguish retained prefix from discarded
  suffix; rejection before slot reuse and three-prefix-accumulation tests pass.
- R07: fresh allocation generation distinguishes same-address ABA; copied old
  numbers reject while the new node gives its independent derivative.
- R08: binding captures mode/width; raw scalar/vector and width changes reject
  before accessing incompatible storage. Supported vector oracles pass.
- R09: identity/epoch/count/generation exhaustion rejects before destructive
  mutation or stale revalidation. Rebinding/registration/assignment preserve
  prior values; partial allocation requires reset and next-scope recovery.
- R10: expression operands validate before allocation/assignment commits;
  saved-tree, stale-destination and exhaustion tests prove the old graph survives.
- R11: both operands/nested unary trees, compound assignment and model/container
  copies obey validation. This does not validate destroyed C++ references.
- R12: cached Value stays passive; explicit double/independent/PutOnTape binding
  starts a fresh chain. Ordinary copies preserve the old identity and remain stale.
- R13: missing-node active access rejects, cached primal remains readable, and
  no hidden leaf is created during reverse.
- R14: rejected operations identify context/epoch/slot/generation/mode safely;
  messages are built after failure without dereferencing another thread's TLS.

Acceptance mapping: A01 smooth scalar and A09 supported vector oracles pass;
A02/A03 reset/free, A04 prefix/suffix, A05 same-address ABA, A06 live/exited thread,
A07 fresh chain, A08 saved/compound/model-copy/destination, A10 exhaustion and
A11 full-final-block/capacity cases all pass in the 28-case lifetime suite and
focused sanitizers. A12 has OFF layout/symbol/conditional-source proof and active
required scope checks. A13 has full OFF/ON local and exact-head native package/
binding/Windows verification. A14 has fresh default formal/affected pairing and
all 28 exact-head checks. Complete sanitizer-library CI now succeeds.
The option does not protect dangling C++ references or arbitrary internal
block-list mutation, and this audit makes no such claim.

P01 still requires completing the plan's production-phase/resource/scaling
measurement obligations. The present process-wide RSS observations do not
implement a request memory budget, identify tape-only peak bytes, or choose a
block width. D04/F01/F02/P02/P03, F03/P04/P05 and F04 remain undelivered full-goal
requirements; no Stage A or overall completion follows from this audit.

## Verdict and next action

At `b42f9eb`, D00/D01/D02/D03 acceptance is verified within the explicit evidence
limits. Terminal paginated check runs and matching PR head are retained in
`native-only-b42f9eb-ci-final.jsonl` and `native-only-b42f9eb-pr-final.json`.
Every check is completed/success, including both stable gates, all six native
compilers, complete OFF/ON/diagnostic sanitizer, TSan, warning-clean, MSVC/Windows
consumers and wheel verification. Binary identity and source-equivalence proof
are rechecked; no later C++ source is silently covered by this acceptance.

Verdict: Comment Only for the full unfinished Stage A. Preserve the draft PR and
full goal; proceed to remaining
Stage A and market/portfolio development.
