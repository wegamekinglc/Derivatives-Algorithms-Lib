# Compounded OIS contract fixtures

`compounded_ois_observation_schedule.csv` is a hand-written `/2` schedule oracle.
It describes one coupon `[2024-03-05,2024-03-08)`, coupon ID `coupon`, an explicit
2024-03-04 holiday, ordinary closed weekends, ACT/360 and midnight fixings.
All day-count columns are calendar-day numerators over 360. Row/reuse ordinals
are zero-based within a mode; a blank reuse field means no reuse. Source keys
will be `observation_schedule/<row>`. The four mode groups are alternative
contracts, not rows to combine into one product. These are expected data for
future expansion tests; stage 0 does not execute `/2`.

For N=100, the independently multiplied floating amounts, before payment DF or
fixed-leg subtraction, are:

- None: `0.00972239351527777778`.
- Lookback(1): `0.00749993054583333333`.
- ObservationShift(1): `-0.00250115281666666667`.
- Lockout(1): `0.01666759029479166667`.

Shift uses six shifted weight days against three actual days, with no 3/6
amount multiplier. Lockout keeps the anchor and repeated factor separately.
At valuation Mar-05 under Model, Lookback/Shift classify rows Historical,
Projection, Projection; RequireHistorical changes the second row to Historical.
Lockout's Mar-06 anchor is still projected at that valuation. Payment status
and fixing availability must be supplied by each eventual execution test.

The three `compounded_ois_v1_*.json` files freeze the unchanged production
implementation at master `e669ced750972b65011693b11ed828888f1d7248`, using
`CompoundedOisTest::Contract()` and `Valuation()` from the adjacent fixture header:

- `prepared`: canonical preparation/1 JSON with explicit tables and full snapshot.
- `instrument`: canonical bound instrument archive including exact source rows,
  ON literals, declarations and effective bindings.
- `identities`: calendar, contract, preparation, source, definition and binding
  fingerprints, compared byte-for-byte as well as after loading old archives.

Files have one trailing newline for repository text conventions; comparisons
remove that newline, preserving all canonical JSON bytes. Do not regenerate
goldens to accept an identity or source drift. The contract uses a negative past
fixing and deliberately different today/future snapshot values; default Model
must ignore the latter two. The first four-day ON interval crosses a weekend and
the explicit holiday, and the legs use distinct ACT/360 and ACT/365F bases.

`CompoundedOisTest::Calibration(role)` provides independent scalar ParRate quotes
for two log-discount coordinates and two independent quote IDs. `Overnight`
uses passive discount, `Shared` uses the target for both roles, and `Discount`
uses passive ON with a short Deposit anchor and a two-payment OIS quote. A
single-payment OIS ParRate cancels its discount factor, so it cannot anchor a
discount coordinate. Every role also preregisters a separate fixed-K off-market
PV. Changing a quote must preserve all these contracts and passive inputs.

The baseline test checks existing solved-fit snapshot rejection at
`/fit/observations` and OIS per-PV rejection against an available Deposit snapshot
at `/instrument`. These are phase 0 compatibility assertions to replace when
the corresponding capability is implemented; they do not authorize bypassing
table, identity, original solve-evidence or numerical admission.
