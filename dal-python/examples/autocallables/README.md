# Autocallable examples

The ten numbered scripts match the [C++ contract catalog](../../../dal-cpp/examples/products/autocallables/README.md),
including observation schedules, coupons, redemption rules, model inputs and
AAD smoothing. `common.py` builds the event scripts and shares the pricing and
output code. The [Excel workbook generator](../../../dal-excel/examples/autocallables/generate_workbooks.py)
uses this same event builder without importing DAL or requiring a compiled extension.

With `dal` installed, run from the repository root:

```bash
python dal-python/examples/autocallables/001.classic_snowball.py
python dal-python/examples/autocallables/005.memory_phoenix.py --paths 32768
python dal-python/examples/autocallables/010.worst_of_phoenix.py --paths 32768
```

Every script accepts `--paths` (default 16384) and prints the double price,
smoothed AAD price and named derivatives, per notional 1 including redemption.
Contract terms stay fixed when differentiating model inputs. AAD prices can
differ from hard-barrier prices because of smoothing.

Focused tests exercise prescribed paths for KO, KI and recovery, lockout,
step-down, conditional and memory coupons, airbag redemption, principal
protection, upside participation and both worst-of assets:

```bash
python -m pytest dal-python/tests/test_autocallable_examples.py -q
```
