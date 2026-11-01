# Autocallable workbooks

The ten numbered `.xlsx` files match the [C++ contract catalog](../../../dal-cpp/examples/products/autocallables/README.md)
and [Python examples](../../../dal-python/examples/autocallables/README.md).
Each workbook contains visible event scripts, model inputs and double/AAD
valuation formulas rather than precomputed prices.

Load the DAL XLL in Windows Excel, open a workbook and force a full
recalculation with Ctrl+Alt+F9. `Inputs!B2` initializes DAL. Valuation uses an
explicit date from the workbook, so it does not depend on the global evaluation
date set by another workbook.

- `Read me` explains units and conventions.
- `Inputs` contains editable model inputs, path count and immutable setting tables.
- `Events` contains the dates and editable payoff scripts; the initialization
  row is followed by twelve monthly observations.
- `Pricing` builds product/model/settings handles and displays two-column output
  arrays for the double and smoothed AAD valuations. Retain the reserved array
  ranges when editing the sheet.

Contract terms such as coupon rates and barriers are literal values in `Events`.
Edit that script text to change the contract. Model spots can be changed in
`Inputs` while initial reference levels stay fixed. Principal is included in
the PV; the purchase price is excluded. AAD differentiates the smoothed payoff
with contract terms fixed and can produce a different price from hard barriers.

To regenerate all workbooks from the shared Python event builder, install
`openpyxl>=3.1` and run from the repository root:

```bash
python dal-excel/examples/autocallables/generate_workbooks.py
```

`--output-dir <directory>` writes a separate copy. Workbook generation and
portable contract checks do not recalculate Excel formulas; native
recalculation requires Windows Excel and a matching DAL XLL.

Check the committed workbooks against the shared scripts, dates and output
array contracts with:

```bash
python -m pytest dal-excel/tests/test_autocallable_workbooks.py -q
```
