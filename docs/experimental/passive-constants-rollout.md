# Native passive constants production rollout

DAL-353 implements an explicit core C++ memory option for prepared compiled
ordinary scalar AAD using the native three-factor GSR model. The default stays
OFF. Adopt the option for workloads whose tape capacity matters, after checking
their event schedule; this study does not justify a general latency improvement
or automatic enablement. See the [API and migration contract](../methodology/passive-constants.md).

## Source and validation identity

The baseline is `ff57ec9972d110a579e46edcbaf5d53a6003ddbb`, including the
coordinator's merge of master `abda015d`. The production native source commit is
`e3e58114dac8dacb3c5f744fbed08f980fc3481e`. The [machine-readable report](data/passive-constants-rollout-summary.json)
records the measured source file hashes and executable SHA-256 identities. The
separate isolated overlay studies are frozen selection evidence; none of their
runtime checks or measurements is relabeled as production validation.

Release GCC 15.2, static C++17 builds, `-O3 -DNDEBUG`, native architecture OFF,
lifetime diagnostics OFF and profiling OFF are used for measurement. A separate
correctness build enables both lifetime diagnostics and profiling. Declared
dependency gitlinks and complete CMake caches are in the evidence bundle.

- Release focused native/MC checks: 77/77 passed on this native source.
- Lifetime diagnostics and profiling focused checks: 106/106 passed.
- Production-profile CLI regression checks: 9/9 per baseline and candidate.
- Default native tape gate: 27 cases passed,
  two independent rounds of ten paired processes, unchanged 4% threshold.
- Production cohort: 250 successful commands, including 240 timed requests
  and ten separate numeric-admission/structure requests. Every PV and all named
  risk values match bit for bit, including risk ordering and signed zero.

Checks cover explicit input separation, passive read/seed diagnostics, scalar
and vector channels, weighted and block roots, prefix/suffix checkpoint reuse,
rewind/clear, owner thread and stale bindings, close/unwind/recovery, retained
block reclamation and budgets. GSR comparisons include a nonlinear payoff and
an explicitly registered script constant risk. Unsupported models, factors,
RNG/precision/bridge settings, objectives, caller vector mode and EXERCISE fail
closed. Structured reverse operators are rejected before callbacks.

## Default-mode cost

These controls compare the baseline executable with the new executable with
the option OFF. Each row uses two rounds of ten interleaved process pairs.
Request latency uses each round's minimum. A regression signal requires both
rounds to exceed +4%; no assertion is based on the pooled minimum alone.

| Control       | Base min ms | Default min ms | Round 1 | Round 2 | Signal    |
|---------------|-------------|----------------|---------|---------|-----------|
| default-short | 8.098       | 8.292          | +8.97%  | -0.35%  | no-change |
| default-gsr   | 416.992     | 408.833        | +1.93%  | -3.32%  | no-change |

The first production implementation failed the native tape `short full` case
at +9.68% and +4.01%. The final implementation moves opt-in constant allocation
out of the default inline constructor expansion and preserves input assignment
ordering. The initial failure, patch and raw outputs remain in the evidence
bundle. The final gate uses unchanged sample counts and thresholds.

## GSR memory benefit

The same production executable is measured with the option OFF and ON. The
fixture has 17 events and 16 named model risks. A separate `cold` request for
each worker/path/option case first runs the numeric and alternate-engine
oracles. Timed RSS samples then use the GSR-only `memory` mode, with
`oracle_paths: 0`, to keep unrelated oracle allocation from setting the process
high-water mark. Each memory request still performs a matching reference
valuation, and all its PV/risk bits are compared across OFF/ON externally.

| Metric per worker   | OFF     | ON      |
|---------------------|---------|---------|
| nodes               | 19588   | 15775   |
| edges               | 48324   | 48324   |
| tape_occupied_bytes | 1556704 | 1404184 |
| tape_capacity_bytes | 2621440 | 1966080 |
| tape_blocks         | 5       | 4       |

The occupied tape payload drops 152,520 bytes per worker. Allocated tape
capacity drops 655,360 bytes (640 KiB), or 25%, by crossing one 16,384-node
block boundary. Edges and unused sink writes remain. Enabling the owned policy
also reclaims previously retained excess blocks before input registration;
the retained-block test confirms OFF/ON/OFF behavior. Path count does not
multiply per-worker tape memory. Other event schedules may not cross a block
boundary, and factor counts other than three are not admitted by this option.

Process peak RSS is a separate observation. Each cell is a median over process
samples; both per-round medians and the combined median are retained, with
full ranges in the machine-readable data.

| Workers | Paths | OFF RSS KiB | ON RSS KiB | Round 1 | Round 2 | Combined |
|---------|-------|-------------|------------|---------|---------|----------|
| 1       | 8192  | 12646       | 11686      | -8.26%  | -7.59%  | -7.59%   |
| 4       | 8192  | 20770       | 17966      | -13.42% | -13.50% | -13.50%  |
| 8       | 8192  | 31276       | 26404      | -15.63% | -15.64% | -15.58%  |
| 4       | 32768 | 20722       | 17966      | -13.43% | -13.31% | -13.30%  |

## Timing and limits

These timings compare OFF/ON in the same binary, using the same two rounds of
ten paired processes. Negative percentages mean lower elapsed request time.

| Workers | Paths | OFF min ms | ON min ms | Round 1 | Round 2 | Signal    |
|---------|-------|------------|-----------|---------|---------|-----------|
| 1       | 8192  | 414.446    | 422.415   | +0.30%  | +1.92%  | no-change |
| 4       | 8192  | 131.738    | 129.804   | -0.31%  | -1.47%  | no-change |
| 8       | 8192  | 76.985     | 76.002    | -1.28%  | -3.51%  | no-change |
| 4       | 32768 | 512.643    | 510.978   | +0.01%  | -0.65%  | no-change |

All heavy DAL work holds the shared measurement lock, and each request pins
the first N allowed CPUs for N workers. External Rust compilation remained on
the shared WSL2 host. Affinity and the DAL lock do not isolate shared caches,
memory bandwidth or scheduler pressure. These timing signals are informational;
they do not support a general acceleration claim. RSS savings also remain
allocator/process observations rather than guaranteed service-wide savings.

## Reproduction and decision

Use `script_mc_perf --production-profile gsr-swap-3f 8192 aad compiled cold
1 0 0 1 --passive-constants` for numeric admission. Run the same request with
`memory` only after admission to measure process RSS, and omit the final flag
for the OFF comparison. Set `DAL_NUM_THREADS` and CPU affinity explicitly.
The attached evidence contains the collector, raw command ledger, outputs,
resource records, source patch/manifests, focused logs and static build caches.
The [CSV](data/passive-constants-rollout-results.csv) separates baseline/default
controls from same-binary OFF/ON comparisons.

The delivered production boundary is a supported, default-off C++ option with
explicit admission and lifecycle ownership. Its measurable value is tape memory
reduction for the selected GSR workload. Keep general constructor/input behavior,
Python and Excel bindings, and unadmitted estimators on their existing policy.
PR integration still requires current-head CI, Codacy and focused independent
code review; local validation alone is not acceptance or a merge claim.
