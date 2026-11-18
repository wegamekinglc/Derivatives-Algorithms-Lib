# Quote-risk state coverage profile

The optional `--state-profile` entry retains three informational rows in the
existing `quote_risk_perf` target:

```bash
cmake --build build/core-bench --target quote_risk_perf --parallel 4
DAL_NUM_THREADS=1 build/core-bench/dal-cpp/benchmarks/quote_risk_perf/quote_risk_perf --state-profile --preflight
DAL_NUM_THREADS=1 build/core-bench/dal-cpp/benchmarks/quote_risk_perf/quote_risk_perf --state-profile
```

Use a Release configuration with `DAL_CPP_BUILD_BENCHMARKS=ON`. This target uses
core APIs and supports `DAL_BUILD_PUBLIC=OFF`. CTest registers the command as
`quote_risk_state_profile` under the existing `benchmark` label. Default target
invocation retains its existing workload list, output and timing bodies. The
nine-target regression allowlist is unchanged.

| Row | Fixture |
|---|---|
| Generic joint build | JointQuoteRiskFixtures N5: three layered PWC curves with 2/2/1 quotes; exact analytic calibration, inverse requested |
| Generic joint component probe | The same N5 provenance's three components, using its explicit state `/4` |
| Staged basis component probe | MakeStagedXccyProvenanceMaterials N16, one basis component, using its explicit state `/3` |

Calibration, market construction and probe provenance are prepared before the
timers. Preflight recomputes every component against the held fingerprint,
checks live Kind/state admission and finite inverse entries, and verifies a
retain=true copy's axis/state/inverse and canonical-record SHA. It reports the
record byte size outside timing; measured creation retains the default false.

The original Bench harness supplies three warmups and twenty measured calls.
Creation includes its original availability check, sink and provenance
destruction. Probes include explicit-scheme recomputation, full component-hash
comparison, sink and temporary-state destruction. Calibration counters must not
change inside those timings. Bench retains median/min/max, not each inner raw
duration; keep complete process outputs when collecting paired samples.

These fixtures match the original DAL-547 external coverage probes. Earlier
staged timing checked nonempty fingerprints; this retained profile also compares
the expected full fingerprint. Preserve original source/binary ownership when
reusing evidence. New-head timings are informational until the relevant driver,
library, inputs and boundaries have been paired under the repository's sampling
rules. In particular, an old `/2` generic probe or an implicit base `/3` probe
cannot substitute for current `/4` component verification.
