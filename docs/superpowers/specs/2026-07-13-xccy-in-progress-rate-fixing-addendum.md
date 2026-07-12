# XCCY In-Progress Trades — Rate Fixing Addendum

This addendum is part of
`docs/superpowers/specs/2026-07-13-xccy-resettable-mtm-design.md` and is normative
for the implementation plan.

## Generalized Snapshot

Use one immutable `MarketFixingSnapshot_`, not an FX-only snapshot. Its canonical
mapping is `index name -> DateTime_ -> positive finite value` and it contains:

- domestic floating-rate fixings;
- foreign floating-rate fixings;
- FX reset fixings.

The XCCY swap configuration carries explicit canonical domestic and foreign rate-index
names. A resettable or mark-to-market configuration also identifies the canonical FX
index through its currency pair. No canonical name may be inferred from tenor alone.

## Coupon Resolution

For a coupon whose payment date is on or after the valuation date:

- `rateFixingTime < valuationTime`: a historical rate fixing is mandatory;
- `rateFixingTime == valuationTime`: use a supplied fixing when present, otherwise use
  the active forecast curve;
- `rateFixingTime > valuationTime`: use the active forecast curve.

Historical rate fixings are passive constants in AAD evaluation. Future or today-unfixed
rates retain the active scalar type and curve sensitivities. Coupons whose payment date is
before the valuation date are removed before fixing resolution and therefore do not
require historical fixing data.

## Validation and Tests

- Reject a missing canonical rate-index name when an unpaid coupon has a historical
  fixing time.
- Reject missing, nonpositive, NaN, or infinite historical rate fixings with instrument,
  leg, index name, and fixing timestamp in the message.
- Test a started trade with a past-fixed/future-paid coupon on each leg.
- Verify historical rate-fixing rows have zero curve sensitivity while future coupon rows
  retain domestic or foreign forecast-curve sensitivity.
- The in-progress `xccy_perf` workload must include both historical rate fixings and
  historical FX reset fixings in one snapshot.
