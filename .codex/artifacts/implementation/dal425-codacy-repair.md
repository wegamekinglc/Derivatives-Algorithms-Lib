# DAL-451 controlled dispatch repair

The ten security annotations on PR #648 head `5c53230b` are addressed in source.
No rule, threshold, analyzed path or repository gate is disabled. Local Bandit
reproduces the five import/process warnings on that head and reports zero on
the revised four production modules. Semgrep's public Python rules report zero
on the revised modules; that rule bundle also reports zero on the old source,
so it is additional checking, not reproduction of Codacy's five critical rules.
The new remote Codacy result remains a separate exact-head check after push.

Worker and profile-helper creation share a fixed dispatch module. Both factories
require literal `/usr/bin/python3`, isolated startup flags, the fixed worker
source or profile entry, permitted modes, a private root and sealed descriptors.
Worker source bytes must match the fixed source file; helper tickets bind the
parent PID, mode, root and source hash before any spawn. No interpreter path is
selected from `sys.executable`, PATH or an input command.

Dispatch requires an isolated single-thread parent using the fixed interpreter,
with mappings below the child AS bound. It forks that verified runtime and loads
an immutable bootstrap, JSON payload, fixed source and helper ticket from sealed
descriptor slots. Staging duplicates above those slots prevents remapping
overlap. The bootstrap verifies seals, source digest, parent and cwd before
loading the selected source. Receipts identify the inherited interpreter/fork
contract and logical intent, binding bootstrap/payload hashes. Missing or
changed bindings reject recovery. Each new child profile describes this fork
context and needs independent acceptance.

The child applies explicit descriptor actions: null stdin, a bounded pipe
for stdout/stderr, only declared sealed inputs retained, and other open
descriptors closed, a fixed environment and standard source finders. The worker applies AS, CPU, file-size and core limits before
its ordinary imports. There is no Python `preexec_fn`. The original parent owns
the child PID and its final wait4; process creation adds no observer layer. If
closing parent descriptors fails after spawn, the dispatcher closes the pipe,
stops and reaps that owned child, retaining the raw wait result in the exception.

The isolated launcher loads its sealed source with the standard source-loader
protocol in the same limited interpreter. It does not build an exec command or
start an additional supervisor process. The existing single-thread, mapping,
identity and two-process limits still apply. It requires `-I -S -B` and the fixed
environment explicitly.

Code identity uses a shared recursive JSON fingerprint of bytecode, constants,
names, argument/local/free/cell variables, flags, filename/line metadata and
exception tables. Frozensets have canonical ordering. No marshal/dill/pickle
serialization or deserialization remains. Profile and execution use the same
fingerprint implementation; their support source is loaded without cached code.

Focused RED witnesses cover the inherited interpreter path, missing canonical
fingerprint, a forged sealed helper ticket and lost child ownership after a
parent close failure. GREEN includes the existing execution/profile/caller
controls and eleven new dispatch controls. The latter run the fixed zero/nonzero
workers and sealed launcher init in fresh synthetic test fixtures, retaining
their complete final wait4 values. These are regression tests, not a real shared
allocation, profile acceptance or financial qualification.

The revised source and runtime identities require fresh independent acceptance;
old compiled/live/profile evidence does not qualify this head. Permanent old
markers, G2 history, allocations, FAIL and UNKNOWN are preserved. Facility,
H_exact/H_exclude and all financial flags remain false. No public API, DAL build,
numerical experiment or performance algorithm changes, so native/MC/benchmark
matrices are outside this private Python repair.
