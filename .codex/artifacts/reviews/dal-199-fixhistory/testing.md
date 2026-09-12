DAL-199 prerequisite independent tester handoff, 2026-09-13. Tested production
head: `b15e5340853d6d074c4fd9be969ac16141d36080`, based on
`65c6b87a088ba12cddfa57dcc111250c9bfae16a`, in the isolated
`fix/dal-199-fixing-history-definition` checkout. The containing commit adds
only this active report. The original F2 checkout and its builds were untouched.

All requested local repair checks pass. The inherited same-name map/vector
destructor collision no longer occurs in the independently rebuilt actual-header
reproducer in either object link order. Documentation, independent final review,
publication, and separate integration/revalidation with F2 remain outstanding.

## Running existing tests

Read AGENTS.md, the tester contract and test references, the approved
`.codex/artifacts/api-notes/dal-199-fixhistory-identity.md`, implementation
evidence, production diff, and both permanent regression translation units.

Verified the production delta only changes the map wrapper's class/constructor,
Find definition, and Empty result/local/nested type names. The map is now
IndexFixHistory_; the vector remains FixHistory_. The diff against the base is
empty for storage/globals.hpp/.cpp, indice/fixingsnapshot.cpp, dal-public,
dal-python and dal-excel. Lookup bodies, defaults, errors, and storage/binding
behavior are unchanged.

The full Linux workflow used a new build directory and new install prefix,
verified absent before configuration. It reused neither the implementer's
existing objects nor any F2 build output. Initialized only the missing pinned
XAD submodule needed by examples; no gitlink changed. Removed any prior root
test_output.txt and ran:

```bash
NUM_CORES=12 DAL_BUILD_DIR=build/dal-199-fixhistory-tester-clean DAL_INSTALL_DIR=build/stage/dal-199-fixhistory-tester-clean ADDITIONAL_CMAKE_FLAGS='-DDAL_BUILD_EXCEL_PORTABLE_TESTS=ON' bash ./build_linux.sh > test_output.txt 2>&1
```

Exit 0, including configure, all-target compilation, installation and CTest:
`100% tests passed, 0 tests failed out of 1595`, 8.39 seconds.
Fresh logs: root `test_output.txt` and `build/fixhistory-tester-native-full.log`.

Configuration: Linux Release, GCC 15.2.0, native AADET; core/public/portable
Excel tests and examples enabled. Python and benchmarks disabled, with the
standard script excluding the benchmark label. Discovery comprises 1,457 main
core tests, four allocation/boundary tests, 105 public API tests, and 29 portable
Excel contracts (`build/fixhistory-tester-discovery.log`).

The three new FixHistoryTest cases pass in this fresh full run. They cover
both header orders, distinct C++ identities, vector aggregate construction,
map copy ownership, exact midnight/intraday lookup, zero/negative values,
missing/quiet results, unchanged missing text, the Empty singleton, and global
storage replacement/append with chronological round-trip and overwritten quote.
Existing FixingSnapshotTest and affected curve suites also pass, including raw
requested timestamps, reverse FX, invalid quotes, frozen snapshots, refreshing
historical requests and explicit snapshot authority.

## Independent header and sanitizer verification

Created fresh scratch files under `build/fixhistory-tester-repro/`. Both
header-order files include the actual repaired platform, indice/fixings and
storage/globals headers and construct both history types. This command exits 0:

```bash
c++ -std=c++17 -I dal-cpp -fsyntax-only build/fixhistory-tester-repro/headers-map-first.cpp build/fixhistory-tester-repro/headers-vector-first.cpp
```

Captured output: `build/fixhistory-tester-repro/headers.log` (empty, no errors).
For the separate-TU ASan check, copied the independent triage's vector.cpp and
main.cpp unchanged; cmp against `../dal-199-triage-scratch/` exits 0 for both.
The map.cpp only changes the map-wrapper type references to IndexFixHistory_.
Using actual headers, freshly compiled all three objects from that scratch
directory, then linked and ran both orders:

```bash
c++ -std=c++17 -O0 -g -fsanitize=address -fno-omit-frame-pointer -I ../../dal-cpp -c map.cpp vector.cpp main.cpp
c++ -fsanitize=address map.o vector.o main.o -o map-first
./map-first
c++ -fsanitize=address vector.o map.o main.o -o vector-first
./vector-first
```

Compilation, both links, and both executions exit 0. Runtime logs contain only
the reproducer's size diagnostics and no ASan error; no object size is asserted.
Logs: `build/fixhistory-tester-repro/compile.log`, `asan-map-first.log`, and
`asan-vector-first.log`. This tester reran repaired GREEN, not the baseline RED;
the latter is independently captured in the earlier triage/implementation record.

`nm -C` on the fresh objects verifies map.o emits only
IndexFixHistory_::~IndexFixHistory_ and vector.o only FixHistory_::~FixHistory_.
The linked executable has distinct destructor addresses. Logs:
`build/fixhistory-tester-repro/object-symbols.log` and `linked-symbols.log`.
The fresh native core archive exports IndexFixHistory_::Find and has no old
FixHistory_::Find symbol (`build/fixhistory-tester-library-symbols.log`).
Source search finds exactly the global struct FixHistory_ definition and no
alias, compatibility macro, conditional map definition, or map class under
that qualified name. The remaining FixHistory_ destructor belongs to the
preserved global vector, as required.

## Authoring tests and repairing failures

No additional permanent tests were necessary: the implementer's three new
regressions and existing snapshot/curve suites cover this narrowly scoped
identity repair. No test, production, generated, configuration or documentation
files were edited by the tester. No test or build failures occurred during this
independent repair pass. Scratch reproducers/logs are untracked build evidence;
only this report is committed. Working and exact scoped staged whitespace
checks pass.

## Remaining delivery requirements and limits

The approved API note requires direct C++ map-wrapper callers to migrate the
type and vals_t references to IndexFixHistory_, update explicitly typed Empty
results, and cleanly rebuild DAL plus every dependent C++ object/binary. The
unchanged global vector name and unchanged Empty function spelling do not make
old conflicting objects safe to mix with repaired objects. The doc writer must
publish that source/ABI migration and rebuild requirement before delivery.

No local Windows/XLL or Python runtime is claimed. Alternate AAD repetition is
unnecessary for this pure C++ type-identity change; sanitizer coverage is the
specific actual-header cross-TU reproducer, not the entire native suite. F2 is
not present on this prerequisite branch and must be integrated and independently
revalidated afterward. No local repair-testing blocker remains.
