# Index Names and Parsing

This note describes how `dal-cpp/dal/indice/` turns index name strings into
`Index_` objects and how indices resolve fixings.

## The `Index_` interface

`Index_` (`dal-cpp/dal/indice/index.hpp`) is the minimal market-observable
interface: `Name()` returns the canonical name, and
`Fixing(_ENV, fixingTime)` returns the value at a fixing time. The base
implementation of `Fixing` resolves the index by name through the
environment: `Index::PastFixing` fetches the `FixingsAccess_` environment
entry, retrieves the `Fixings_` record held for the name, and looks up the
exact `DateTime_` in its map. A missing record or time throws unless the
caller passes the `quiet` flag, which returns $-\infty$ instead. `IndexKey_`
wraps a handle with its name so scenario containers can order indices.

## Parse dispatch

`Index::Parse(const String_&)` (`dal-cpp/dal/indice/indexparse.cpp`) first
tries composite parsing, then single-index parsing. Single parsing splits the
name at the first `:` or `[`: the prefix before the separator selects a
parser from a process-wide registry, and the selected parser interprets the
remainder of the string. A name with no separator currently matches nothing,
and an unregistered prefix throws.

Parsers self-register through `Index::RegisterParser(name, func)` under a
mutex. The built-in registrations are installed once by
`IndexParsers_::Init()` (`dal-cpp/dal/indice/parser/init.cpp`), which runs as
part of `RegisterAll_::Init` at library initialization:

| Prefix | Parser                                                         | Produces         |
|--------|----------------------------------------------------------------|------------------|
| `EQ`   | `Index::EquityParser` (`dal-cpp/dal/indice/parser/equity.cpp`) | `Index::Equity_` |
| `FX`   | `Index::FxParser` (`dal-cpp/dal/indice/parser/fx.cpp`)         | `Index::Fx_`     |

`Index::Clone` re-parses `src.Name()`, so round-tripping an index through its
name works only for families with a registered parser.

## Equity names

The equity grammar is `EQ[stock]` with an optional delivery suffix
(`dal-cpp/dal/indice/parser/equity.cpp`):

- `EQ[stock]` — spot equity; the delivery date falls back to
  `Date::Maximum()`;
- `EQ[stock]@2027-06-18` — forward with an explicit delivery date parsed by
  `Date::FromString`;
- `EQ[stock]>3M` — forward whose delivery is the fixing date stepped by a
  date increment (see [dates, calendars, and schedules](dates.md)).

## FX names

The FX grammar is `FX[fgn/dom]` (`dal-cpp/dal/indice/parser/fx.cpp`) — for
example `FX[USD/JPY]` is one USD priced in JPY. `Index::Fx_::Fixing` first
looks up `FX[fgn/dom]` in the environment's fixings and falls back to the
reciprocal of `FX[dom/fgn]`.

## IR indices are constructed, not parsed

Interest-rate indices (`dal-cpp/dal/indice/index/ir.hpp`) have canonical name
formats but no registered string parser: `Libor_`, `Swap_`, and `DF_` are
built directly in C++:

- `Libor_(ccy, tenor)` names itself `IR:<ccy>,<tenor>` (for example,
  `Libor_(Ccy_("USD"), TradedRate_("LIBOR3MLCH"))` produces
  `IR:USD,LIBOR_3M_LCH`);
- `Swap_(ccy, tenor)` names itself `IR:<ccy>,<tenor>` with a numeric-leading
  tenor (for example `IR:USD,5Y`);
- `DF_(ccy, maturity)` names itself `IR[DF]:<ccy>,<maturity>`.

Start and maturity offsets are `Cell_` values resolved against the fixing
date: empty means the fixing date itself, an integer is a day offset, a date
or datetime is absolute, and a string is applied as a date increment.
`Libor_` and `Swap_` start dates then roll by the currency's spot-lag
convention (`Libor::StartFromFix` in `dal-cpp/dal/protocol/conventions.cpp`).

## Composites and historical paths

`Index::Composite_` (`dal-cpp/dal/indice/indexcomposite.hpp`) declares a
weighted component list; the composite parsing hook is a placeholder that
currently matches nothing. `IndexPathHistorical_`
(`dal-cpp/dal/indice/indexpath.hpp`) adapts a fixing time series to the
`IndexPath_` interface used where models need path-level expectations and
range probabilities.

## Examples

No dedicated example program exercises index parsing in isolation; the
snippet below is drawn from the public headers in `dal-cpp/dal/indice/` and
shows the parse-then-construct surface. Every symbol matches the current
signatures in `index.hpp`, `indexparse.hpp`, `index/equity.hpp`,
`index/fx.hpp`, and `index/ir.hpp`.

```cpp
// Inline snippet drawn from the public headers in dal-cpp/dal/indice/.
#include <dal/currency/currency.hpp>
#include <dal/indice/index/equity.hpp>
#include <dal/indice/index/fx.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/indice/index.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/math/cell.hpp>
#include <dal/protocol/couponrate.hpp>
#include <dal/time/date.hpp>

using namespace Dal;

// Registered parsers: 'EQ[...]' -> Index::Equity_, 'FX[...]' -> Index::Fx_.
// Index::Parse returns std::unique_ptr<Index_>; the prefix before ':' or '['
// selects the parser, and an unregistered prefix throws.
std::unique_ptr<Index_> spotEq = Index::Parse("EQ[SPX]");
std::unique_ptr<Index_> fwdEq  = Index::Parse("EQ[SPX]>3M");
std::unique_ptr<Index_> usdJpy = Index::Parse("FX[USD/JPY]");
const String_ fxCanonical      = usdJpy->Name();     // "FX[USD/JPY]"

// IR indices have canonical names but no registered parser; they are built
// directly. Libor_ takes a TradedRate_ tenor; Swap_ takes a swap-tenor string.
const Index::Libor_ libor(Ccy_("USD"), TradedRate_("LIBOR3MLCH"));
const Index::Swap_  swap(Ccy_("USD"), "5Y");
const Index::DF_    df(Ccy_("USD"), Cell_(Date_(2027, 6, 18)));
const String_ liborName = libor.Name();              // "IR:USD,LIBOR_3M_LCH"
const String_ swapName  = swap.Name();               // "IR:USD,5Y"

// FxIndexName(domestic, foreign) produces the canonical 'FX[fgn/dom]' name used
// by fixing snapshots and XCCY reset conventions; it round-trips with Index::Parse.
const String_ usdGbp = FxIndexName(Ccy_("GBP"), Ccy_("USD"));  // "FX[USD/GBP]" (domestic=GBP, foreign=USD)
```

The build-tree test binary covers the parse dispatch table and canonical name
formats: `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter=IndexParseTest.*`
exercises the registered parsers, and `--gtest_filter=IndexTest.*` covers
`Libor_`, `Swap_`, and `DF_` name generation and fixing lookup.
