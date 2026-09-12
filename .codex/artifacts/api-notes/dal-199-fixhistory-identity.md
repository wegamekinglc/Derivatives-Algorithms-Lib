# DAL-199 prerequisite: distinct fixing-history C++ identities

Status: approved API direction for the inherited ODR repair. This note controls
implementation, testing, documentation, and review on
`fix/dal-199-fixing-history-definition`, based on `65c6b87a`.

## Decision and audience

Rename the standalone map wrapper in `dal-cpp/dal/indice/fixings.hpp` from
`Dal::FixHistory_` to `Dal::IndexFixHistory_`. Preserve the active vector
aggregate `Dal::FixHistory_` in `dal-cpp/dal/storage/globals.hpp` exactly.
Each qualified type name then has one definition across translation units.
Do not provide an alias, compatibility macro, conditional definition, or old
map-wrapper class under the qualified name `Dal::FixHistory_`.

This is a core C++ source/ABI repair for direct `DAL::cpp` consumers. The
public-facade, Python, and Excel calling surfaces need no changes.

## Verified current surfaces

- `indice/fixings.hpp:25` currently declares the map-owning class, with public
  `vals_t = std::map<DateTime_, double>`, an explicit copying constructor,
  private `vals_`, and `Find(time, quiet = false)`.
- `storage/globals.hpp:21` independently declares the vector-owning aggregate,
  with public `Vector_<pair<DateTime_, double>> vals_`. `Global::Fixings_::History`
  returns it; `XGLOBAL::StoreFixings` accepts it with `append = true`.
- Repository-wide symbol search finds the map wrapper used only in its own
  declaration, `Find` definition, and `FixHistory::Empty()` implementation.
  Global capture in `indice/fixingsnapshot.cpp`, curve code, and existing
  snapshot/curve tests use the vector aggregate.
- `indice/index.hpp` exposes the separate storable `Dal::Fixings_` through
  `FixingsAccess_`; `Index::PastFixing` calls `LookupFixing`. Neither is the
  standalone map wrapper.
- `dal-public/src/global.hpp` exposes initialization and evaluation dates.
  `dal-public/src/curveprotocol.hpp:85` constructs `MarketFixingSnapshot_` from
  nested maps. Python's snapshot class/builder in `bindings/curve.cpp:521` and
  Excel's `MarketFixingSnapshot_New` in `src/__curveprotocol.cpp:181` use that
  separate snapshot type. Neither legacy history wrapper is directly bound.

The independent review in `dal-199-inherited-triage.md` supplies the inherited
baseline comparison, same-TU redefinition failure, and separate-TU ASan
heap-buffer-overflow evidence. Those tests were performed by the reviewer;
this API pass does not claim to have rerun them. The active F2 implementation
handoff in `.codex/artifacts/reviews/dal-199/implementation.md` on the feature
branch confirms that F2 composes index fixing access with global snapshot
capture and changes no facade/Python/Excel entry point.

## Proposed surface

```cpp
namespace Dal {
    class IndexFixHistory_ {
    public:
        using vals_t = std::map<DateTime_, double>;

    private:
        vals_t vals_;

    public:
        explicit IndexFixHistory_(const vals_t& vals) : vals_(vals) {}
        double Find(const DateTime_& fix_time, bool quiet = false) const;
    };

    namespace FixHistory {
        const IndexFixHistory_& Empty();
    }
}
```

Update the constructor, out-of-line `Find` definition, and return/local/nested
type names in `FixHistory::Empty()`. Keep the `FixHistory` namespace and `Empty`
function name; renaming these would add unrelated migration. `Empty()` remains
a reference to one process-lifetime const empty map wrapper.

Keep `Fixings_`, `FixingsAccess_`, `LookupFixing`, global storage signatures and
representation, snapshot behavior, generated storable records, and archive
format unchanged. No new adapter or implicit conversion is required.

## Lookup and error contract

`IndexFixHistory_` copies its supplied map and performs exact `DateTime_` lookup.
It does not apply name parsing, FX inversion, nearest-date search, or snapshot
validation. A missing time with `quiet = false` retains the existing exception
and message `no fixings for that time`; `quiet = true` retains `-INF`. An empty
history follows the same rule. Existing finite/nonfinite/value acceptance is
unchanged; richer input diagnostics are outside this identity repair.

The vector aggregate stays default/aggregate constructible with directly
mutable `vals_`. Existing storage semantics remain: `StoreFixings` merges when
`append` is true, replaces when false, lets later supplied values overwrite the
same timestamp, stores chronological rows, and returns the stored row count.
No mutation-synchronization guarantee is added.

Typical migrated core use, with both header families intentionally visible:

```cpp
#include <dal/platform/platform.hpp>
#include <dal/indice/fixings.hpp>
#include <dal/storage/globals.hpp>

const Dal::DateTime_ time(Dal::Date_(2026, 9, 11), 0, 0);
const Dal::IndexFixHistory_ history({{time, 123.0}});
const double observed = history.Find(time);
const Dal::IndexFixHistory_& empty = Dal::FixHistory::Empty();

Dal::FixHistory_ stored;
stored.vals_.push_back({time, observed});
```

## Compatibility and alternatives

External C++ map-wrapper callers must replace the old type name and its nested
`vals_t` references with `IndexFixHistory_`. Explicit declarations receiving
`FixHistory::Empty()` must use the new type; `auto` callers need no source edit.
Vector/global callers retain their names, signatures, layout, and source use.

Require a clean rebuild of DAL and dependent C++ objects, including bindings
and executables. Do not mix objects compiled against the old conflicting
definitions with repaired objects: old weak special-member symbols can retain
the defect. This is not a binary-compatible map-wrapper rename; method symbols
and C++ type identity change. An unchanged `Empty` function symbol on an ABI
that omits return types is not evidence of binary compatibility. The published
`docs/public-api.md` contract describes core as a source-level API whose
consumers track core changes and gives no facade ABI isolation guarantee.

No Python function/class/keyword, Excel registration/argument, facade builder,
default, error text, serialized storable name, or example API changes are needed.
Rebuilding those binaries against repaired core code remains necessary.

A unified type could retain a map constructor and `Find` while exposing a vector
`vals_`, but that requires new lookup rules for arbitrary mutable vector order
and duplicate timestamps, changes map-wrapper layout, and risks aggregate
initialization compatibility. A dual-container design additionally requires a
coherency rule for public vector mutation. Neither is a minimal compatible fix.
Renaming the heavily used vector surface creates needless migrations; removing
the map wrapper loses its API entirely. Separate names preserve both behaviors.

## Documentation handoff

The doc writer should record a breaking core C++ rename and rebuild requirement
in `CHANGELOG.md`; this meets its explicit breaking-public-API criterion despite
having no in-repository map-wrapper clients. Add a short current-state note in
`docs/methodology/index_parsing.md` distinguishing `IndexFixHistory_` exact map
lookup, the `FixHistory::Empty()` result, the global vector `FixHistory_`, and
the separate environment storable `Fixings_`. Keep migration history in the
changelog. No new methodology page, Python/Excel documentation, or example
program is warranted. Final wording and placement belong to the doc writer.

## Acceptance evidence for implementer, tester, and reviewer

1. A same-TU regression includes `indice/fixings.hpp` and `storage/globals.hpp`
   with their usual platform prerequisites and constructs both histories.
   Demonstrate that this fails on the baseline and compiles after repair.
   Exercise present/missing/quiet map lookup, the empty singleton, and a
   nonempty vector history; exact hits and the legacy missing behavior must pass.
2. Adapt the existing independent three-file reproducer by renaming only its
   map-wrapper references, compile with `-O0 -fsanitize=address`, and run with
   both map-first and vector-first object link order. Both must terminate without
   sanitizer failures. Confirm distinct special-member identities with `nm -C`
   where emitted; do not encode compiler-specific 48/24-byte sizes in tests.
3. Existing snapshot/global fixing and affected curve regressions pass on the
   baseline repair branch. After integrating the prerequisite, F2 fixing
   environment, preparation, snapshot, and script regressions also pass. The
   tester/reviewer retain their normal native/public suite requirements.
4. The final diff preserves the vector definition, lookup bodies, defaults,
   storable metadata, and facade/binding signatures. Search confirms there is
   one `Dal::FixHistory_` definition and no alias or map wrapper with that name.
   Documentation states the source migration and clean-rebuild requirement.

## Open questions

There is no blocking business/API decision. External use of the map wrapper
cannot be established from the repository; the explicit migration notice covers
that uncertainty. Broader fixing error handling and synchronization changes are
outside this authorized minimum repair. This note adds no code, tests, or build
changes and should be retired once the repair's current-state docs are accepted.
