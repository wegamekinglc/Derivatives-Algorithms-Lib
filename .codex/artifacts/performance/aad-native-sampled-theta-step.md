# Recorded sampled PDE cost acceptance

Status: local scoped acceptance passes; exact-head publication remains pending.

## Frozen scope

Only `dal-cpp/dal/math/aad/sampledthetastep.cpp` adds production behavior. The
existing generic checked event, numeric PDE cache, solve/root implementations,
Number/node/tape layouts and existing headers are unchanged. Baseline is accepted
#504 head `3b4aad18e3a51a8888ae3af732959f416de43fe8`, merged as
`94799f9e3f349bf89b3a2c58a16bde322d90c9f6`, tree
`c5e0f586c25da5e63ecd4eadb214d41153b09dbe`.

Native informational rows use the numeric PDE sizes: n3/one layer, n65/two layers,
n513/two layers, theta0 and theta0.5, cached scalar reverse with owning reports
and complete input registration/capture/reverse/close. These twelve rows measure
new costs, with numerical checksum agreement against the accepted numeric cache.
They do not claim a native speedup or impose a cross-interface regression bound.
Widths1/4/8 and caller/tape capacity boundaries are covered by functional tests;
this measurement selects scalar channels only. Complete rows include active
input registration, unlike numeric cache construction alone.

The exact existing-object and fresh-call dependency proofs permit reuse of
accepted existing timing. No existing row is remeasured. No Cartesian portfolio,
curve, solver or full nine-target gate is added. A changed existing executable
would require its affected paired comparisons with two best-of-ten rounds and
the established +4% threshold.

## Existing-path identity

The accepted archive has 175 members. The current archive contains those exact
name/hash multiplicities and one added native sampled-step object. Numeric and
native translation units share the basename `sampledthetastep.cpp.o`; occurrence
indices change when the native object is inserted. Matching the multiset proves
identity without mistaking ordinal changes for altered numeric machine code.

Fresh links match all seven accepted executable hashes: checked dense solve,
diagnosed dense solve, checked coordinate solve, numeric/native implicit roots,
numeric sampled PDE and the passive PDE benchmark. This retains their accepted
raw evidence without relabeling old rows as newly timed passes.

Evidence root is `/home/wegamekinglc/.cache/dal-aad-evidence-20261008`:

- `native-pde-performance/baseline-provenance.json` freezes accepted source,
  configuration, archive and existing caller hashes.
- `native-pde-performance/identity-proof.json` contains the multiset proof,
  exact fresh-link commands, output hashes and new production source hashes.
- `pde-sampled-step-performance/provenance.json` and its copy-assignment repair
  evidence retain accepted existing numerical/performance observations.

## Native informational results

Twenty process samples produce 240 observations in 2.46 seconds. Both round
minima below are microseconds. Every process validates primal/risk checksums
against the accepted numeric cache and physical errors against the declared
policy. Resources and checksums are identical across samples. The initial
numerical pilot is retained separately and excluded from these minima.

| Grid/layers | Theta | Complete round 1 (us) | Complete round 2 (us) | Cached round 1 (us) | Cached round 2 (us) |
|-------------|-------|-----------------------|-----------------------|---------------------|---------------------|
| 3/1         | 0     | 38.131                | 38.318                | 0.514               | 0.515               |
| 3/1         | 0.5   | 38.646                | 38.747                | 0.569               | 0.572               |
| 65/2        | 0     | 58.173                | 58.985                | 13.731              | 13.735              |
| 65/2        | 0.5   | 72.362                | 73.569                | 17.939              | 17.899              |
| 513/2       | 0     | 202.922               | 206.803               | 105.119             | 107.451             |
| 513/2       | 0.5   | 322.340               | 319.549               | 143.229             | 144.304             |

| Grid/layers | Theta | Event bytes | Scratch bytes | Caller capture peak bytes | Caller capture retained bytes |
|-------------|-------|-------------|---------------|---------------------------|-------------------------------|
| 3/1         | 0     | 1160        | 120           | 344                       | 56                            |
| 3/1         | 0.5   | 1232        | 120           | 440                       | 56                            |
| 65/2        | 0     | 14736       | 4680          | 14904                     | 2096                          |
| 65/2        | 0.5   | 17040       | 4680          | 18248                     | 2096                          |
| 513/2       | 0     | 111504      | 36936         | 118840                    | 16432                         |
| 513/2       | 0.5   | 129936      | 36936         | 145480                    | 16432                         |

Event bytes include the owning cache/bindings and descriptor table. Caller
capture columns count staged construction and returned outputs/diagnostics after
active input registration; they exclude input buffers and reverse reports.
Functional resource tests separately assert complete overlapping caller/tape
peaks including reports. These are tracked buffer capacities, not RSS.

Small complete requests cost substantially more than cached sweeps: input
registration, scope lifecycle and publication remain part of native execution.
This supports explicit cost disclosure, not a universal speedup, vector-width
recommendation or cross-interface performance guarantee.

The host is WSL2 Linux on an Intel i9-13900HX. Set DAL_NUM_THREADS=4 and pin the
single caller to CPU0; these scalar kernels submit no worker tasks. All compilation
finished before timing. Production source hashes remain unchanged. Absolute
timings depend on this virtualized environment.

Raw evidence is native-pde-performance/native-raw/; native-results.json retains
environment, commands, source/workload/executable hashes, observations and minima.

Overall local verdict: **no existing-path regression**, using accepted immutable
evidence and fresh object/executable identity. New native costs pass numerical
and resource checks and remain informational.


## Configuration and remaining acceptance

Local configuration is GCC15.2, C++17, Release static/PIC, `-O3 -DNDEBUG`,
`-ffp-contract=fast`, native AAD, built-in SIMD without Eigen, lifetime/profiling
OFF. Twenty process outputs are separated into two rounds of ten and reduced
to each row's minimum. Hardware/thread settings and every raw output are retained.

Exact-head platform/CI/review acceptance remains open. Production source hashes
must remain applicable to the final head.
