# TSan exception runtime checks

The `tsan-profiling` Linux job builds a private, TSan-instrumented `libstdc++`
from the source package matching its GCC compiler. It links only that job's test
binaries to the private runtime. The ordinary toolchain and production builds
retain their existing configuration.

An uninstrumented library can hide synchronization from TSan. In particular,
exception reference-count decrements must order an active handler's reads before
the final exception destruction. See the [TSan manual's library instrumentation
requirements](https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual#non-instrumented-code).

`exception_lifetime_repro.cpp` exercises `packaged_task`, `future`,
`current_exception`, rethrow, and a two-level exception's `what()` message.
Its relaxed gate adds no happens-before edge; the synchronized mode supplies a
release/acquire control. `real_race.cpp` deliberately races on an integer so the
check also proves the detector remains active.

`probe_runtime.sh <compiler> <runtime-directory> <output-directory> <stock|instrumented>`
records loaded library paths, Build IDs, hashes and compiler commands. Stock
mode requires an exception race report (exit 66) and a clean synchronized control.
Instrumented mode requires both exception modes to pass. Both require the real
race to be detected. Fatal diagnostics fail every mode, including expected race
reports with exit 66. Startup failures, timeouts and unexpected exit codes fail
the job; no warnings are suppressed.

The output gate's controlled-process regression runs before the runtime build:
`python3 -m unittest discover -s .github/scripts/tests -p test_tsan_probe.py -v`.

`build_runtime.sh <matching-GCC-source-directory> <work-directory>` builds the
library, verifies POSIX thread support and the TSan atomic call in exception
release, and places the selected compiler's `libtsan` beside it. The workflow
verifies the DAL binary actually loads these libraries before running the
focused recovery test and the existing sanitizer filters. Runtime evidence is
uploaded even when a check fails.
