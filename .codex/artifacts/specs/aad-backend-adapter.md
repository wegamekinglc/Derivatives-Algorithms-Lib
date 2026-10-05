# DAL native AAD operation contract

Status: D03 accepted at native-only publication `b42f9eb`; this contract still
controls the remaining full-goal request/operation work.
External-backend removal and native simplification are implemented locally;
fresh complete local verification, paired performance, all 28 exact publication
CI checks and the final requirement audit pass.

Source: the user's full AAD implementation request, its no-performance-regression
and no-CI-failure condition, and the subsequent requirement to remove XAD,
CoDiPack and Adept. The [native-only removal contract](aad-native-only.md)
controls D00 and replaces earlier four-backend obligations in this specification.

## Problem and goals

The pre-removal D03 implementation supplied compiled capabilities, weighted
seed/read gateways and scoped recording integration alongside external adapters.
Native AAD is now the sole local implementation. Keep useful operation contracts
while eliminating external adapters and selection branches.

Retain the operation boundaries that clarify native scalar/vector storage,
seed replacement versus addition, gradient clearing, prefix accumulation and
recording recovery. Keep the layer thin and used by callers. Avoid a pluggable
framework, virtual dispatch, per-node capability queries or compatibility fields
whose only purpose was describing external backend differences.

The shared dependency direction remains `dal-cpp <- dal-public <- bindings`.
No new portfolio/result API is delivered by this core contract; F02 and D04 remain
separate requirements. Native mixed-mode Hessians, custom reverse events and
independent nested recordings remain unsupported until their own implementation
and acceptance.

## Requirements

R01. D00 removes external AAD implementation, dependencies, export paths and
selection first. Replace `backend.hpp` with `native.hpp` and make callers use native operations
directly; remove `ScalarBackendAdapter_`, external specializations and external
capability branches. Preserve useful existing native service symbols where doing
so keeps callers simple, but do not keep empty selection scaffolding.

R02. Report only capabilities needed by actual request planning or diagnostics:
native vector width, lifetime diagnostics available/enabled, fixed-graph repeated
sweeps, interval/prefix propagation and scoped ownership. Unsupported nesting,
reverse events and higher-order modes must not be advertised as implemented.
Remove external names and suffix-disposal metadata; constant native package
identity may remain for installed consumers.

R03. Native SetSeed replaces and AddSeed accumulates a passive seed in the
requested scalar/vector channel. ReadAdjoint returns a passive value. In vector
mode, channel zero is the first vector slot, not the legacy scalar node field.
Aliases of one output root accumulate their weights before reverse.

R04. Validate positive width, scalar width one, native maximum width and channel
range before mutation/access. A node without suitable vector storage must be
rejected before accessing it. An invalid request preserves the existing graph,
primal values and valid seed, allowing a subsequent valid reverse.

R05. ActiveRoot retains existing constant-output and direct-input behavior.
The caller's registered active zero may keep a constant output in the requested
graph; the operation must not silently clear a recording or register extra inputs.

R06. Diagnostic ON seed/read validates owner, epoch, retained prefix and generation
before unsafe node access. OFF introduces no lifetime registry, token fields or
per-node checks and makes no general stale-handle safety claim. Common public
mode/channel preconditions remain checked in OFF builds.

R07. Registering inputs, starting recording, clearing the graph, clearing adjoints
and restoring a suffix are distinct operations. Native NewRecording retains its
established no-op. Full gradient clearing covers actual scalar/vector storage.
Suffix reverse and restore preserve prefix accumulation; consumed intermediates
and input accumulation keep their documented behavior.

R08. A valid retained fixed graph supports repeated VJPs after caller-controlled
clearing/reseeding. Results do not include previous seeds or gradients. A discarded
suffix or closed recording cannot be revived by capability metadata; extract
passive outputs before discarding their activity.

R09. Integrate native services with scoped recording and existing failure seams.
Remove external activation/disposal distinctions rather than propagating them
into every lifecycle call. Preserve phase validation, nested-use rejection,
thread affinity, cleanup error retention and next-request recovery.

R10. No new fields in default-OFF Number_, TapNode_ or Tape_. No virtual call,
heap-backed adapter instance, successful-path error strings or per-scalar
capability allocation. Existing scalar/vector propagation specialization remains.

R11. The private six-state lifecycle enum follows Machinist generation. Preserve
states, transitions, messages and test seams; retain generated outputs and verify
full regeneration/drift without manually editing generated files.

R12. Invalid operation/mode/channel errors identify the operation and native
context. Precondition failures leave a valid recording usable. Business failures
retain their original exception; failed requests do not publish partial risk.

R13. Library, installed headers and consumers share exported diagnostic ABI
definitions. Native OFF/ON consumers compile/link through DAL exported targets
without XAD, CoDiPack or Adept. Bindings do not expose active nodes or tape slots.

R14. Acceptance uses fresh native OFF/ON complete suites, consumers, applicable
bindings/sanitizers, local review and exact new-head CI. Freeze final source and
binaries for the existing nine-target policy plus affected MC/LSM/calibration
and resources. Previous four-backend passes or benchmark smoke are not D00/D03
performance or publication acceptance.

## Independent mathematical and lifecycle oracles

Register x=2,y=3; record u=x*y and v=x*x+y, with materialized shared intermediates.
Values are (6,7), and Jacobian rows are (3,2) and (4,1).

1. Seeds (2,-1) produce input adjoints (2,3).
2. Clear gradients; seeds (0,3) on the retained graph produce (12,3).
3. Two references to the same u root with weights (2,-1) add to one seed,
   producing (3,2). A copy that creates a new root is not a reference-alias oracle.
4. A supported constant output contributes zero parameter gradient; a direct
   input output contributes its identity derivative.
5. Native vector channels reproduce these independent scalar results, including
   a zero channel. The legacy scalar field cannot substitute for vector channel zero.

Cover widths 1, 3, 5, 10, 16 and a non-specialized supported width, respecting
the actual maximum. Invalid channels/modes and missing vector storage leave the
previously valid seeded graph usable. Native diagnostic ON also rejects
discarded/reset/foreign-thread numbers before unsafe access.

Prefix oracle: p=x*x at x=2, rebuilt suffix weights 1,2,3, total prefix seed six;
one prefix reverse gives dx=24. Exercise native services through scoped ownership
and validated checkpoints. Test exception after successful extraction, failed
reverse/cleanup and the next independent analytic graph.

## Acceptance

| ID  | Required evidence                                                                                          |
|-----|------------------------------------------------------------------------------------------------------------|
| A01 | D00 has removed external adapters, dependencies, exports and selection; fresh native builds require none.  |
| A02 | Native scalar VJPs, reference aliases, constant/direct-input roots and repeated intermediate sweeps pass.  |
| A03 | Indexed vector channels match analytic/scalar results; zero channels and scalar storage remain distinct.   |
| A04 | Invalid width/channel/missing-storage requests preserve valid graph and seed.                              |
| A05 | ON stale/foreign validation passes ASan/UBSan; OFF layout and cost guarantees remain.                      |
| A06 | Prefix accumulation, lifecycle recovery, MC/LSM task draining and legacy calls retain behavior.            |
| A07 | Generated lifecycle enum has no drift and installed headers remain self-contained.                         |
| A08 | Native advertised support matches executed tests; unsupported requests fail explicitly.                    |
| A09 | Fresh complete OFF/ON builds, consumers, bindings and exact-head required CI pass.                         |
| A10 | Fresh unchanged formal and affected production pairing passes; failures and confirmations remain recorded. |

## Current evidence and next step

Pre-removal local commits 2b4bf89 and 5689ff9 pass full native OFF/ON CTest
with 2,328/2,324 cases. XAD, CoDiPack and Adept each passed 2,264 cases; these
external results are retained history. All five installed consumers passed.
Native ON passes 60 focused ASan/UBSan cases with leak detection; OFF passes
21 serial benchmark smoke cases. Full generation/drift checks passed.

The missing adapter first failed compilation. Scalar-node/vector-mode access
first failed UBSan with null-reference binding; the native channel gateway now
rejects absent vector storage before access. Both failures and corrected runs
remain in the evidence root. The local generated enum preserves transitions
and default-OFF scope size/alignment on the measured host.

Earlier PR #480 publication 9b5febc passed 49 checks while containing external
backends; it did not validate the then-unpublished removal. Fresh D00/D03
performance and exact native-only publication CI are recorded below.

Current local native-only source has no external dependency directories. Fresh
OFF/ON complete builds, final incremental rebuilds and full tests pass.
All ten configuration/header migration cases, 60 focused native/lifecycle/lifetime
ASan/UBSan cases, and both analytically checked AAD examples pass. These results
do not establish full performance or publication acceptance.

OFF full CTest passes 2,330 cases. The first ON full pass exposes a pre-existing
`vanilla` example lifetime error: registered inputs are discarded by full rewind.
The narrow CTest reproduction fails with the same epoch diagnostic. The local
repair uses scoped registration/reverse and verifies price and all six partials
against independent analytic formulas. Its narrow OFF/ON reruns pass; the
corrected complete suites pass 2,330 OFF and 2,359 ON cases, including 793 Python
tests each. Both final installed prefixes pass their two consumer tests; complete
generation/drift verification passes. Default OFF layout probes confirm
Number_/node/tape/scope sizes of 16/40/368/72 bytes on the measured host.

Frozen native-only head `8816951` passes all 65 formal comparable cases and
25 curve cases. The initial production pair fails two GSR cases; the full
published-head control and predefined original-baseline two-by-thirty confirmation
pass all 44 cases. Four-node AAD fit remains borderline at +3.52%/+5.24%.
All LSM numeric results agree and before/after source/configuration/pin/binary
identities match. See the [native-only evidence](../perf/aad-native-only.md) for
every result, retained failures and limits.

All 28 checks at `b42f9eb9c12a5987e8a81a869f59b38d8096a1de` succeed, including
complete diagnostic sanitizer libraries, Windows/consumers, wheels and both
stable gates. Source equivalence to measured `8816951` and all 22 binary digests
are rechecked. The [contract audit](../reviews/aad-native-contract-audit.md)
verifies every D03 requirement/acceptance item within its stated evidence limits.
D00/D01/D02/D03 can be checked off after the additional D01 numeric comparison;
P01 remains open, and no Stage A or full-goal completion is claimed.
