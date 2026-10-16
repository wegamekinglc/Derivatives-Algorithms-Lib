# Native passive constant sink study

This isolated experiment measures a native per-tape constant sink against the
unchanged DAL implementation. The overlay patch is applied only to a separate
source worktree. It is not a public library option and is not production adoption.

Double construction and assignment share one lazily allocated zero-argument node
per tape recording. Explicit `RegisterIndependent`, `PutOnTape` and scoped input
registration create distinct independent nodes. Direct passive adjoint reads and
seeds reject. Expression operand edges and sink writes remain; computed expression
results still own ordinary nodes. `Number_` retains its size. Tape storage gains
two pointers for the current and marked sink.

Mark captures the current sink. Suffix restore resets its unused scalar/vector
seeds or drops a suffix-created sink. Full rewind/clear drops both cached pointers.
Existing optional lifetime bindings validate owner, epoch, slot generation and
layout. A passive value bound to a retained prefix sink remains valid across
suffix restore; a value bound only to a discarded suffix sink does not.

This changes the implicit-construction/adjoint contract within the experimental
build. All code linked into that build must use its matching headers and flags.
Generic recorded operators, structural Jacobian constant outputs, public bindings,
LSMC and calibration are outside the admitted prototype domain. The experiment
does not supply a migration policy or approve those callers.

## Reproduce on Linux

Initialize the four declared dependencies without changing their gitlinks:

```bash
git submodule update --init dal-cpp/externals/eigen dal-cpp/externals/googletest \
  dal-cpp/externals/rapidjson dal-cpp/externals/machinist
python3 dal-cpp/experimental/passivesink/prepare.py --repo . \
  --work-root ../sink-study-new --commit 3be8dab9
```

The work root must not exist. Preparation validates clean dependency identities
before creating detached source worktrees. It shares dependency directories through
symlinks and does not modify them. Resolve the baseline to a full immutable SHA
in the retained manifest before sampling.

Run every build, test and complete paired sampling cohort under the workspace's
shared measurement `flock`; inspect external load before entering it. From the
directory containing the work root:

```bash
cmake -S sink-study-new/base-source/dal-cpp/experimental/passivesink \
  -B sink-study-new/base-build -DCMAKE_BUILD_TYPE=Release -DDAL_ENABLE_NATIVE_ARCH=OFF
cmake --build sink-study-new/base-build --target passive_sink_tests passive_sink_study script_mc_perf -j8
cmake -S sink-study-new/head-source/dal-cpp/experimental/passivesink \
  -B sink-study-new/head-build -DCMAKE_BUILD_TYPE=Release -DDAL_ENABLE_NATIVE_ARCH=OFF \
  -DCMAKE_CXX_FLAGS=-DDAL_EXPERIMENTAL_PASSIVE_SINK=1
cmake --build sink-study-new/head-build --target passive_sink_tests passive_sink_study script_mc_perf -j8
```

For lifetime checks, configure a separate `head-diagnostics` build with the same
sink flag and `-DDAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS=ON`; build and run
`passive_sink_tests`. The native tests cover distinct registration, passive
diagnostics, scalar/vector weighted seeds, fresh recordings, prefix/suffix sink
restore, passive/aliased block outputs, owner rejection and exception recovery.
Diagnostic builds additionally check discarded-suffix, cleared and exited-thread
bindings, followed by explicit re-registration.

The paired collector runs correctness/CLI admission first and stops on any
failure. It compares every PV and named risk by double bits, preserving signed
zero. Production cases are short/long BS, LV, GSR, GSR-SLV, long BS tree and four
workers; chains cover Number coefficients at scalar/vector width four and double
coefficients as a control. Chain recording includes scope setup/cleanup; reverse
includes clear and seed. Each chain process reports the minimum of twenty timed
iterations after three warmups. Each outer case uses two rounds of ten processes
per side, alternates base/head first and reduces each round by minimum. Single
workers use one fixed CPU and four workers use the first four allowed CPUs.

```bash
python3 Derivatives-Algorithms-Lib/dal-cpp/experimental/passivesink/runstudy.py \
  --work-root sink-study-new --output sink-study-new/results
python3 Derivatives-Algorithms-Lib/dal-cpp/experimental/passivesink/test_runstudy.py
python3 -O Derivatives-Algorithms-Lib/dal-cpp/experimental/passivesink/test_runstudy.py
```

The output directory must be new. Raw commands, stdout/stderr, process RSS,
fingerprints, executable hashes and before/after identity checks are retained.
Negative deltas mean faster; both rounds must exceed four percent in the same
direction for improvement/regression. These are explicitly requested experimental
measurements, outside the repository's nine-target production gate. Production
adoption also requires the original compatibility, numerical and affected-boundary
acceptance criteria. Tape payload and process RSS remain separate measurements.
