# Excel FIX Settings and Diagnostics

The Windows XLL values scripts containing `FIX(EQ[AAPL])` with explicit
valuation dates, immutable fixing snapshots, and tree or
compiled price/AAD execution. Three settings constructors return repository
handles for contract, valuation, and simulation choices.

## Worksheet Functions

These are the registered worksheet names; use them without an added `DA.`
prefix. Square brackets below mark optional arguments and are not formula
syntax. Settings are worksheet ranges; the other settings inputs
are handles returned by constructors.

| Function                       | Inputs in order                                          | Result                     |
|--------------------------------|----------------------------------------------------------|----------------------------|
| `SCRIPTPRODUCTSETTINGS.NEW`    | `name, [settings]`                                       | Product settings handle    |
| `PRODUCT.NEWWITHSETTINGS`      | `name, dates, events, settings`                          | Script product handle      |
| `SCRIPTVALUATIONSETTINGS.NEW`  | `name, [settings], [fixings]`                            | Valuation settings handle  |
| `MONTECARLOSETTINGS.NEW`       | `name, [settings]`                                       | Simulation settings handle |
| `MONTECARLO.VALUEWITHSETTINGS` | `product, modelData, n_paths, [valuation], [simulation]` | N×2 price/risk table       |
| `PRODUCT.DESCRIBE`             | `product`                                                | N×1 JSON text chunks       |
| `SCRIPTVALUATION.EXPLAIN`      | `product, modelData, [valuation]`                        | N×1 JSON text chunks       |
| `SCRIPTSIMULATION.EXPLAIN`     | `product, modelData, n_paths, [valuation], [simulation]` | N×1 JSON text chunks       |

`PRODUCT.NEWWITHSETTINGS` requires a product settings handle, even for defaults.
Use `SCRIPTPRODUCTSETTINGS.NEW("defaults")` to create one, or use the original
`PRODUCT.NEW(name, dates, events)`. Both product constructors retain the same
event-date conversion: numeric dates must be exact integers, while definition
labels and schedule cells keep their existing meaning. Event descriptions,
including constant definitions such as `2.0`, must be text cells.

The original valuation keeps all seven inputs:

```text
MONTECARLO.VALUE(product, modelData, n_paths, rsg, use_bb, enable_aad, smooth)
```

It uses default valuation settings and has no compiled-selection argument.
Both Value functions require a finite integer `n_paths` in `1..2147483647`
and use the same native preparation. Model data must be Black-Scholes or
Dupire. Both return a headerless two-column table: column 1 contains `PV` and
optional `d_<parameter>` keys; column 2 contains their numeric values. Look up
keys instead of assuming PV is the first row. AAD adds model and script-constant
risks; risks are already normalized, and there are no fixing-risk or diagnostic
rows. With AAD disabled the result is a 1×2 PV table.

## Settings Ranges

Each `settings` range has exactly two columns, key then value, with no header.
Model-sourced FIX observations are bound by index name, so the valuation
constructor takes no binding range: the engine binds the model's spot output
to the script's future FIX index.

| Settings handle | Key                     | Default                                         | Accepted value                                |
|-----------------|-------------------------|-------------------------------------------------|-----------------------------------------------|
| Product         | `default_index`         | No default                                      | Nonempty index-name text                      |
| Valuation       | `evaluation_date`       | Capture global date at each Value/Explain entry | Valid integral Excel date serial              |
| Valuation       | `today_fixing`          | `Model`                                         | Exact text `Model` or `RequireHistorical`     |
| Simulation      | `method`                | `sobol`                                         | Text `sobol`, `mrg32`, or `irn`               |
| Simulation      | `use_bb`                | `FALSE`                                         | Excel boolean or numeric 0/1                  |
| Simulation      | `enable_aad`            | `FALSE`                                         | Excel boolean or numeric 0/1                  |
| Simulation      | `smooth`                | `0.01`                                          | Finite, strictly positive number; not boolean |
| Simulation      | `compiled`              | Unset, selecting tree execution                 | Excel boolean or numeric 0/1                  |
| Simulation      | `lsmc_basis_degree`     | `3`                                             | Integral number 1..8; not boolean             |
| Simulation      | `lsmc_training_paths`   | Same as the pricing count                       | Integral number 1..2147483647; not boolean    |
| Simulation      | `lsmc_validation_paths` | Omitted (fixed degree)                          | Integral number 1..2147483647; not boolean    |
| Simulation      | `lsmc_rqmc_replicates`  | Omitted (deterministic Sobol)                    | Integral number 2..2147483647; not boolean    |
| Simulation      | `lsmc_training_seed`    | Omitted (effective 0 in RQMC mode)              | Integral number 0..2147483647; not boolean    |
| Simulation      | `lsmc_pricing_seed`     | Omitted (effective 0 in RQMC mode)              | Integral number 0..2147483647; not boolean    |

Settings keys and RNG names use DAL's case-insensitive
comparison, with no whitespace trimming. Today-policy values are case-sensitive:
`MODEL`, `model`, and `UseIfAvailable` are invalid. Boolean fields reject text
`"TRUE"`, `"FALSE"`, and `"1"`, as well as numbers other than 0 and 1.
Smoothing must be valid even with AAD disabled.
`lsmc_training_paths` sets the regression count for exercise products;
`n_paths` still sets the pricing count. Training and pricing use disjoint Sobol blocks.
Omit the training key to use the pricing count. Products without `EXERCISE`
ignore the setting after validation.
`lsmc_validation_paths` reserves a disjoint block between training and pricing
for selecting a polynomial degree up to `lsmc_basis_degree`. Omitting it keeps
the fixed-degree fast path. When present, all three Sobol blocks are disjoint.
`lsmc_rqmc_replicates` enables independently digitally shifted Sobol pricing
replicates for one frozen policy. `n_paths` then counts paths per replicate;
the price averages replicate means and the `uncertainty` diagnostic reports
their standard error. Training and pricing seeds make streams reproducible.
Seeds without a replicate count, or RQMC with a non-Sobol method, are errors.
The error estimate is conditional on the fitted policy; it excludes policy
bias and retraining uncertainty.

Omitting a matrix, passing `""`, or referencing one blank cell selects defaults.
An entirely blank two-column range also selects defaults. Within a two-column
range, completely empty rows are skipped and later rows are still read. An
empty cell or empty string is blank; a space, zero, or `FALSE` is not blank.
Any other column count is rejected, even for an entirely blank range.

A half-empty row is an error. Omit the `default_index` or `compiled` row to
leave it unset; do not pair the key with a blank value. Unknown or repeated
keys are errors, including differently cased duplicates. Complete
index parsing and model-capability checks occur in description/preparation,
so constructing a settings handle does not establish that a product can price.

Matrix errors include `InvalidSetting`, the constructor and argument name,
and one-based `row`/`column` relative to the supplied range. Skipped blank rows
still count. Duplicate errors also identify the first occurrence. For example,
the fixture's two rows `today_fixing / Model` and
`TODAY_FIXING / RequireHistorical` report `duplicate key`,
`settings row=2 column=1`, and `first row=1`. A missing value points to column 2;
a missing key points to column 1. Shape errors use row 1 and the first missing
or extra column. Invalid Excel cell types also retain physical coordinates.

## Dates, Snapshots, and Recalculation

An `evaluation_date` cell must hold a valid integer serial, such as
`=DATE(2026,9,12)` (46277 in the 1900 date system). The native binding also
accepts a valid DAL `Date_` cell. Its supported serial range is 25569..91103;
text dates, booleans, nonfinite values, out-of-range dates, and fractional
serials are rejected. A DAL `DateTime_` is not a `Date_`, even at midnight.
Cell formatting does not remove a fractional time. Use the 1900 date system
for the supplied workbook.

`MARKETFIXINGSNAPSHOT.NEW([indexNames], [fixingTimes], [values])` takes three
parallel, equal-length ranges. Timestamps preserve the fractional part of a
numeric Excel serial: `=DATE(2026,9,11)+TIME(11,0,0)` is 11:00, not midnight.
Do not insert blank rows in these arrays: each vector stops at its first blank
cell. This differs from the settings-matrix blank-row rule. Duplicate
index/timestamp observations are rejected; values must be finite, with positive
FX observations. Ordinary equity fixings may be zero or negative.

The optional `valuation`, `simulation`, and `fixings` handles accept omitted
arguments, `""`, or a single blank-cell reference. Zero, `FALSE`, an invalid
tag, or a handle of the wrong type is an error. The following states differ:

| Input                                                      | Meaning                                                                          |
|------------------------------------------------------------|----------------------------------------------------------------------------------|
| Omitted valuation                                          | Default policy, global date and required history captured for this call          |
| Valuation constructed with blank settings/fixings          | Same defaults; construction captures neither date nor history                    |
| Explicit valuation date, omitted fixings                   | Fixed date, current required global history at each call                         |
| Snapshot handle, including an empty snapshot               | Use only that snapshot; missing required history fails                           |

For an explicit empty snapshot, use `=MARKETFIXINGSNAPSHOT.NEW(,,)` and pass
its nonempty handle as the valuation constructor's third argument. It selects
`ExplicitSnapshot`; it never fills missing values from global history. A blank
handle selects `GlobalSnapshot`. Global history capture copies sequences in
turn, not as an atomic market-wide snapshot; exclude concurrent fixing writes
during capture.

Settings handles contain immutable native values. Construction copies the
input ranges; product construction copies its contract settings, and Value and
Explain read settings copies. Snapshot handles share immutable data. Editing
input cells requires recalculating the constructors and their dependent calls
to create updated objects. Existing products are not mutated by a later
settings construction.

These worksheet functions are nonvolatile. Each actual Value or Explain call
prepares afresh, but changing process-wide history or the global date does not
create an Excel cell dependency. Explicitly recalculate the relevant valuation
or diagnostic formulas after such a change. Reusing a default valuation handle
then captures the current global date/history; an explicit date/snapshot keeps
those inputs fixed. No prepared plan is cached between calls. Settings handles
are local calculation objects, not persistent settings archives. The separate
[product v2 archive](methodology/script_engine.md#contract-archive) preserves
contract text/default identity and excludes runtime plans and market data.

## FIX Source and Model Rules

Write `FIX(EQ[AAPL])` or `FIX(EQ[AAPL], 2026-09-11)` in event text, with no
quotes around the index. An omitted fixing date means the event date. An
explicit date must be a valid `YYYY-MM-DD` literal; runtime date expressions
and intraday script timestamps are unsupported.

For evaluation date D, fixing date F, and event date E:

- F < D requires history at exactly 00:00 on F; an 11:00 observation cannot
  satisfy it. Missing history is an error, without model fallback.
- F = D uses the model under `Model`, or exact-midnight history under
  `RequireHistorical`, regardless of which quotes happen to exist.
- F > D uses the model. F > E always fails with `LookAheadObservation`, even
  in a dead branch.

`default_index` only gives the legacy zero-argument `SPOT()` an identity.
It does not change `FIX` literals or bind a model. For model-sourced
named observations, the engine binds the model's `spot` output to one ordinary
EQ, taken from the script's own future FIX index by name; several distinct
future indices fail with `MultipleModelIndices`. BS and Dupire support that
single equity; future FX, IR, composite,
delivery indices, and multiple assets are unsupported. Historical EQ/FX observations
need no model index and may contain multiple identities. A product default
does not supply market data: the model inputs must describe
the intended equity.

`SPOT()` is the retained zero-argument compatibility form; write `FIX(index)`
for named observations in new worksheets. Future-only unbound `SPOT()` remains
compatible. Historical unbound SPOT fails
with `UnboundHistoricalSpot`; mixing a future-only unbound SPOT with FIX
fails with `MissingDefaultIndex`. Matching default-bound SPOT and FIX
share one request. `FIX()` and named/argument-taking SPOT are invalid. See the
[script-engine rules](methodology/script_engine.md#the-script-model-index-and-legacy-spot)
for retained observations, separate payment numeraires, hard historical replay,
fuzzy AAD future conditions, and wholly expired products.

## Describe and Explain JSON

`PRODUCT.DESCRIBE` returns schema `dal.script-product/2`, including original
input rows, default/original/canonical identities, all dated events, AST nodes,
fixing dates/times, and source locations. It reads no history or global date,
constructs no model, and starts no workers. It has no valuation phase and does
not establish pricing readiness; even empty or definitions-only products can
be described.

`SCRIPTVALUATION.EXPLAIN` returns `dal.script-valuation/1` from one fresh
default price preparation: Sobol, no bridge/AAD, smoothing 0.01, tree execution.
It accepts no path count or simulation settings. It can initialize a model,
read history, and replay past state, but generates no paths, submits no workers,
and creates no active AAD recording. It describes its own preparation, including
date/policy/source kind, requests and uses, resolved historical values, model
slots, sample dates, event mappings, and payment-numeraire requests. It neither
reuses a preceding Value nor caches a subsequent one. Use the same explicit
date/snapshot and product/model inputs when comparing them.

`SCRIPTSIMULATION.EXPLAIN(product, modelData, n_paths, [valuation],
[simulation])` returns `dal.script-simulation/1`: unlike the valuation Explain
it explicitly runs the full double valuation with `n_paths` pricing paths and
`lsmc_training_paths` training paths (defaulting to `n_paths`) and optional
`lsmc_validation_paths` held-out paths on exercise
products — path generation plus workers plus the exercise regressions — and
reports the
simulation echo (including `lsmc_basis_degree`, `lsmc_training_paths`,
`lsmc_validation_paths`, `lsmc_rqmc_replicates`, `lsmc_training_seed`, and
`lsmc_pricing_seed`, null when unset), the explicit per-replicate `n_paths`,
an `uncertainty` object with budgets, seeds, replicate means and error, and
one `exercise_events` entry per exercise date with the selected degree, basis,
solver, effective rank, fallback reason, validation MSE, regressor index,
in-the-money condition-true path count, frozen coefficients,
degenerate flag and reason, and the exercise rate. Products without `EXERCISE`
skip the simulation run and return an empty `exercise_events` array. `enable_aad` settings are rejected with
`UnsupportedExecutionMode`; `compiled` selects the engine for the run.

Both functions return one column of text, with no header or row-number column.
Concatenate every row in order **without separators or added newlines**, then
parse the complete JSON. Each chunk has at most 30,000 ASCII characters and
may split a JSON token or escape sequence. Non-ASCII UTF-8 is represented with
equivalent JSON Unicode escapes, including surrogate pairs for supplementary
characters. Numeric text and decoded field values are preserved. Invalid UTF-8
raises `DiagnosticEncoding`; output exceeding 1,048,576 chunks raises
`DiagnosticOutputTooLarge` rather than being truncated. Leave room for the
entire spill below the formula cell.

Read the whole spill's `Value2` in the host, as the PowerShell runner does;
do not combine a large document into one worksheet cell with `TEXTJOIN`.
This output encoding does not extend the existing byte-oriented worksheet
input converter's general Unicode support. Diagnostic JSON is not a loadable
product or prepared-plan archive. The full
[schema contracts](methodology/script_engine.md#product-archive-and-diagnostics)
apply unchanged. `PRODUCT.DEBUG` remains the legacy text output.

## Runnable Workbook

The committed [fixture manifest](../dal-excel/examples/010.script_fix_settings.json)
owns the exact cells and formulas. The
[PowerShell runner](../dal-excel/tests/windows/run-script-fix-settings.ps1)
creates a workbook from it, writes invariant English formulas through
`Formula2`, registers the supplied XLL, calculates in dependency order, checks
results, and saves the executed workbook. It requires Windows, a matching XLL
and Excel architecture, and Excel supporting `Formula2` and dynamic spills.
See [Windows build prerequisites](installation.md#windows-c-and-excel).

From a Visual Studio 2022 x64 developer PowerShell at the repository root,
build the source being recorded, then execute the runner. Set the three Office
type-library paths to your installed files; this example uses a Click-to-Run
Office16 layout:

```powershell
cmake --preset Release-windows `
    '-DOFFICE_EXCEL_EXE=C:/Program Files/Microsoft Office/root/Office16/EXCEL.EXE' `
    '-DOFFICE_MSO_DLL=C:/Program Files/Microsoft Office/root/vfs/ProgramFilesCommonX86/Microsoft Shared/OFFICE16/MSO.DLL' `
    '-DOFFICE_VBE_OLB=C:/Program Files/Microsoft Office/root/vfs/ProgramFilesCommonX86/Microsoft Shared/VBA/VBA6/VBE6EXT.OLB'
cmake --build build/Release-windows --target dal_excel dal_excel_tests --parallel 8
& .\build\Release-windows\dal-excel\dal_excel_tests.exe
& .\dal-excel\tests\windows\run-script-fix-settings.ps1 `
    -Xll .\build\Release-windows\dal-excel\dal_excel.xll `
    -OutputDirectory .\build\excel-fix-results `
    -SourceSha (git rev-parse HEAD) -SourceTree (git rev-parse 'HEAD^{tree}')
```

The runner uses a new Excel instance and the 1900 date system. It calculates
`INIT.GLOBALDATA` and `EVALUATIONDATE.SET` once and replaces their control
formulas with values before further recalculation. It checks registration,
handles, the complete price/risk tables, expected DAL error text, today policy,
explicit-date repricing, and complete long JSON spills. It saves
`010.script_fix_settings.executed.xlsx`, `results.json`, and four
`diagnostic-*.json` files in the output directory, then closes its workbook and
cleans up its own process. Verify `registered`, `passed`, and
`cleanedOwnProcess` are true and no `saveError` is present. Source metadata
identifies the checkout; retain the corresponding build log and XLL hash to
establish binary identity.

To generate a static workbook for inspection with Python and `openpyxl`:

```bash
python3 dal-excel/tests/windows/generate-script-fix-workbook.py --output build/010.script_fix_settings.xlsx
```

The output directory must exist. Static generation writes the manifest's cells;
it does not load the XLL, compute cached prices, or add the runner's long-output
cases. Use the COM runner for executed results. DAL failures are `#Error:` text;
checking only Excel's `ISERROR` is insufficient. A registration or spill failure
such as `#NAME?` or `#SPILL!` does not satisfy an expected `MissingFixing` case.

### Fixture Values and Independent Oracles

The manifest sets D=2026-09-12, H=2026-09-11 midnight, F=2026-09-15, and
P=2026-09-22, with AAPL history 80, spot 100, SCALE 2, and 257 paths.
`Handles!B4` supplies D, `Model`, and the snapshot; the model's `spot` output
is bound to the script's `EQ[AAPL]` FIX index by name.
`Handles!B5` selects Sobol, compiled AAD, no bridge, and smoothing 0.01.
`Handles!B6` is zero-volatility/zero-rate BS; `B20` has zero volatility,
rate 0.05 and dividend yield 0.02.

`Inputs!A28:B30` defines text `SCALE / 2.0`, an H event
`x = SCALE * FIX(EQ[AAPL])`, and a P event `pay PAYS x`. Its product is
`Handles!B18`; the actual zero-rate formula is:

```text
=MONTECARLO.VALUEWITHSETTINGS(Handles!B18,Handles!B6,257,Handles!B4,Handles!B5)
```

This occupies `Values!M20` in the fixture. With `tF=3/365` and `tP=10/365`,
the independent expectations are:

| Values anchor      | Scenario                                                | Expected PV                                    | Selected AAD risks                                         |
|--------------------|---------------------------------------------------------|------------------------------------------------|------------------------------------------------------------|
| `M20`              | Historical SCALE payment, zero rate                     | `160`                                          | `d_SCALE=80`, `d_rate=-tP*160`; spot/vol/dividend risks 0  |
| `G20`              | Same payment, rate 0.05                                 | `160*exp(-0.05*tP)` = 159.78097197125695       | `d_SCALE=PV/2`, `d_rate=-tP*PV`; spot/vol/dividend risks 0 |
| `J20`              | F fixing paid at P, rate 0.05/dividend 0.02             | `100*exp(0.03*tF-0.05*tP)` = 99.88773429802069 | `d_spot=PV/100`, `d_rate=(tF-tP)*PV`, `d_div=-tF*PV`       |
| `A2`               | Historical SCALE plus F fixing, zero rates              | `260`                                          | `d_SCALE=80`, `d_spot=1`                                   |
| `A20` / `D20`      | Today Model / RequireHistorical                         | `100` / `80`                                   | AAD disabled                                               |
| `D2` / `G2` / `J2` | Legacy seven-input / omitted settings / default handles | `100`                                          | AAD disabled                                               |

The retained-F case deliberately observes before payment: substituting P-day
spot gives a different price. Zero volatility makes these prices and selected
risks deterministic; finite-path future/mixed `d_vol` need not be zero.
PV tolerance is `1e-12*max(1,abs(expected))`; analytic AAD tolerance is `1e-10`.
The cross-language comparison uses `1e-8` with fixed paths and checks result
keys and finiteness as well as prices.

The [installed C++ consumer](../dal-excel/tests/windows/native-consumer/) and
[Python verifier](../dal-excel/tests/windows/verify-script-fix-consumer.py)
construct equivalent snapshots independently and check those formulas. After
`bash ./build_linux.sh --python 3.13` provides the
[Linux build with Python and staged install](installation.md#linux-workspace-build),
copy the Windows output directory to `build/excel-fix-results` on Linux and run:

```bash
cmake -S dal-excel/tests/windows/native-consumer -B build/excel-fixture-consumer -DCMAKE_PREFIX_PATH="$PWD/build/stage/Release-linux"
cmake --build build/excel-fixture-consumer --parallel 8
build/excel-fixture-consumer/dal_excel_fixture_consumer build/excel-native-diagnostics
PYTHONPATH=build/Release-linux/dal-python dal-python/.venv/bin/python dal-excel/tests/windows/verify-script-fix-consumer.py --output build/excel-fix-comparison.json --excel-results build/excel-fix-results/results.json --native-diagnostics build/excel-native-diagnostics
```

Use matching source revisions for the installed library, Python module, and
XLL. The verifier checks all four native/Excel diagnostic documents in full.
Linux portable binding tests, Windows XLL conversion/registration tests, and
actual Excel workbook execution are separate validation scopes; the portable
binary does not compile the Windows generated wrappers or run Excel.
