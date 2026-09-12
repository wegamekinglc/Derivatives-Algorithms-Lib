# DAL-199 prerequisite implementation

Base: `65c6b87a088ba12cddfa57dcc111250c9bfae16a`, branch
`fix/dal-199-fixing-history-definition`. The API direction is controlled by
`.codex/artifacts/api-notes/dal-199-fixhistory-identity.md`.

## Change and compatibility

The only production changes rename the map wrapper in
`dal-cpp/dal/indice/fixings.hpp/.cpp` to `Dal::IndexFixHistory_`, including its
constructor, `Find`, and the return/local type of `FixHistory::Empty()`.
The global vector aggregate `Dal::FixHistory_` remains unchanged. No alias
reintroduces the conflicting qualified identity. Lookup bodies, defaults,
missing error text, storable metadata, global storage, and bindings are unchanged.

Direct C++ map-wrapper users must migrate the type name and rebuild DAL plus
dependent objects. The API note records this source/ABI migration; the doc
writer owns published migration wording and the changelog. No F2 implementation,
public pricing behavior, `.claude`, or public documentation changed here.

Permanent regressions in `tests/indice/test_fixhistory.cpp` and
`tests/indice/test_fixhistory_reverse_headers.cpp` deliberately exercise both
header orders. They assert distinct C++ identities, aggregate vector
construction, exact map lookup, copied map ownership, finite zero/negative
values, unchanged missing/quiet behavior, empty singleton identity, and global
vector storage/append round trips. Existing CMake test globs register both files.

## RED

The independent triage reproducer was copied to `build/fixhistory-repro/`, using
the actual headers from this repair checkout. No DAL implementation is linked
into the three-file layout reproducer.

```bash
c++ -std=c++17 -I dal-cpp -fsyntax-only build/fixhistory-repro/headers.cpp
c++ -std=c++17 -I dal-cpp -I dal-cpp/externals/googletest/googletest/include -fsyntax-only dal-cpp/tests/indice/test_fixhistory.cpp dal-cpp/tests/indice/test_fixhistory_reverse_headers.cpp
c++ -std=c++17 -O0 -g -fsanitize=address -fno-omit-frame-pointer -I dal-cpp build/fixhistory-repro/map.cpp build/fixhistory-repro/vector.cpp build/fixhistory-repro/main.cpp -o build/fixhistory-repro/red
./build/fixhistory-repro/red
```

Header compilation exits 1 with `Dal::FixHistory_` redefinition in both orders.
The ASan compilation succeeds, but execution exits 1 with heap-buffer-overflow:
destroying the vector history invokes the conflicting map destructor. Logs:
`build/fixhistory-repro/{headers-red,tests-red,asan-red}.log`.

## GREEN

The same syntax-only commands now exit 0. Logs are
`build/fixhistory-repro/{headers-green,tests-green}.log`.
Only map-wrapper type references in the reproducer's `map.cpp` changed;
`vector.cpp` and `main.cpp` compare byte-for-byte equal to the independent
originals (`cmp` exit 0).

From `build/fixhistory-repro/`:

```bash
c++ -std=c++17 -O0 -g -fsanitize=address -fno-omit-frame-pointer -I ../../dal-cpp -c map.cpp vector.cpp main.cpp
c++ -fsanitize=address map.o vector.o main.o -o green-map-first
./green-map-first
c++ -fsanitize=address vector.o map.o main.o -o green-vector-first
./green-vector-first
nm -C green-map-first
```

Both link orders exit 0 without sanitizer diagnostics. The symbol dump shows
separate weak destructors for `Dal::IndexFixHistory_` and `Dal::FixHistory_` at
different addresses. No compiler-specific object sizes are asserted. Logs:
`build/fixhistory-repro/asan-green-{map-first,vector-first}.log` and
`build/fixhistory-repro/symbols-green.log`.

Core configuration/build:

```bash
cmake --preset=Release-linux -S . -B build/Release-linux -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_PUBLIC=ON
cmake --build build/Release-linux --target dal_cpp_tests -j12
```

Both commands exit 0 (`build/fixhistory-{configure,green-build}.log`). Focused
runtime verification:

```bash
./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='FixHistoryTest.*:FixingSnapshotTest.*:XccyPricingTest.*:XccyJointCalibrationTest.TestOmittedFixingsCaptureAllHistoricalRequestsOnceAndExplicitSnapshotRemainsAuthoritative:RateCashflowPricingTest.TestPreparedTradesRefreshFixingsAndValuationTime'
```

Result: 44/44 tests from five suites passed, including all three new permanent
regressions (`build/fixhistory-green-tests.log`). Source search finds only the
global `struct FixHistory_` definition. The diff for `storage/globals.hpp/.cpp`
is empty. Changed production lines and new tests are clang-formatted, preserving
the intentionally opposite include orders; working/cached whitespace checks pass.

Required gtest/rapidjson/machinist submodules were initialized at pinned SHAs;
no gitlink changed. Full native/public suites, documentation, independent review,
separate repair publication, and integration/revalidation with F2 remain owned
by the subsequent specialists. No new broad history redesign was introduced.
