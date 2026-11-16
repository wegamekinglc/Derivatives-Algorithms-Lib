# Public projection benchmarks

Configure with `DAL_PUBLIC_BUILD_BENCHMARKS=ON` and build
`compounded_ois_audit_perf`. It links `DAL::public`; core benchmarks remain
under `dal-cpp/benchmarks` and build without the facade.

From the repository root:

```bash
cmake --preset core-dev -DDAL_BUILD_PUBLIC=ON -DDAL_PUBLIC_BUILD_BENCHMARKS=ON
cmake --build build/core-dev --target compounded_ois_audit_perf --parallel
ctest --test-dir build/core-dev -R '^compounded_ois_audit_perf$' --output-on-failure
```

The audit workload uses D=30/365/1825, all four `/2` modes, one native
coordinate, a January holiday and an explicit working Saturday. It measures
22-column prepared and 26-column evaluated table conversion from already
prepared/evaluated owning inputs. A separate owning evaluation starts with
frozen tables and an existing instrument. It includes native plan preparation
and validation, pricing, details and report construction; initial schedule
expansion and script source construction are outside its timer. It is not a
measurement of the prepared compounding kernel. Both table projections
include allocation and destruction. They retain identical 22-column prefixes
and the documented zero-based row/statement and one-based source row.
These Cell conversions do not time Python-list or Excel-OPER marshaling,
native XLL registration or Office COM/spill.

Each process keeps three warmups and ten samples per case. The executable
prints per-process median/min/max; comparisons require the repository's
two rounds of ten alternating process pairs, best-of-ten reduction and 4%
policy. New projections have no pre-feature historical baseline: report
absolute cost, not a comparison with the old 15-column table or PeriodForward.

Resource lines report output cell count and `sizeof(Cell_)` payload as a lower
bound, excluding indirect string storage. Tracked vector capacity is not total
heap usage. Peak RSS is process-wide on Linux, including setup and earlier
cases; zero on other platforms means unavailable. This target is informational,
outside the scheduled nine-target performance verdict.

Installed OIS consumers are in `tests/installed-consumer`. Configure against a
fresh core-only SDK with `DAL_CONSUMER_CORE_ONLY=ON`, then build/run
`dal_installed_core_ois`. With both matching SDKs installed, the default
configuration additionally provides `dal_installed_public_ois`. The consumers
exercise all modes, `/2` archive identity, EXACT calibration, full/PV-only risk,
two-step fixed-contract recalibration, signed DV01, rejection and owning audit
after input destruction. The core target links only `DAL::cpp`.
See the [installation recipe](../../docs/installation.md#installed-ois-consumers)
for separate fresh core-only and matching core/public configurations. All shared
headers, libraries and bindings must be rebuilt with matching layouts/options.
