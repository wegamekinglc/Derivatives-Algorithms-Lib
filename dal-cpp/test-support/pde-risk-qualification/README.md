# PDE risk qualification tools

The fixed DAL-497 fixtures and thresholds remain unchanged. Python generates
jobs, reads results and audits data. Compilation and execution use fixed native
targets and explicit foreground commands; Python never launches an executable.
Process success means the evidence was captured, not that numerical qualification
passed.

From the repository root, configure/build core independently, then the tools:

```bash
cmake --preset Release-linux -S . -B build/dal502-make \
  -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_PYTHON=OFF -DDAL_BUILD_EXCEL=OFF \
  -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DDAL_CPP_BUILD_BENCHMARKS=OFF
cmake --build build/dal502-make --target dal_cpp -j4
mkdir -p build/dal502-tools build/dal502-followup/products
python3 dal-cpp/test-support/pde-risk-qualification/observe.py prepare \
  --build build/dal502-tools --evidence build/dal502-followup/products
cmake -S dal-cpp/test-support/pde-risk-qualification -B build/dal502-tools \
  -DCMAKE_BUILD_TYPE=Release -DDAL_CORE_BUILD="$PWD/build/dal502-make"
cmake --build build/dal502-tools -j4
```

The standalone CMake project links only the independently built core library.
Its observed target uses a build-directory source copy. Removing its observation
statement must exactly restore the original source; these guards use explicit
exceptions and remain active under Python optimization.

The approved bounded TREE continuation is generated and collected in phases.
Use a new evidence directory; phases are append-only and the ledger cannot be
reset when an executable/source/configuration identity changes.

```bash
python3 dal-cpp/test-support/pde-risk-qualification/campaign.py equivalence \
  --output build/dal502-followup/reference --driver build/dal502-tools/dal502_driver
xargs -n1 -P4 prlimit --as=134217728 -- \
  ./build/dal502-tools/dal502_driver --batchfile \
  < build/dal502-followup/reference/workers.txt
python3 dal-cpp/test-support/pde-risk-qualification/campaign.py accept \
  --output build/dal502-followup/reference
python3 dal-cpp/test-support/pde-risk-qualification/campaign.py initial \
  --output build/dal502-followup/reference
xargs -n1 -P4 prlimit --as=134217728 -- \
  ./build/dal502-tools/dal502_driver --batchfile \
  < build/dal502-followup/reference/workers.txt
```

The equivalence phase compares 449 unique outputs with saved raw references,
covering a complete three-level TREE and LOG-LCP control plus all analytic
formula cases and other TREE cases at the original first paired level. It then
recomputes every original TREE/LOG-LCP self-qualification vector. Only after
exact equality may explicit identity bridges reuse the other original price
nodes. The old origin identity and raw file hashes are retained.

The initial phase fills missing coarse/fine/extra-half scales at the original
levels. Every cache miss, including TREE equivalence witnesses, is charged before
execution. Price identities include actual physical inputs, axis, absolute
offset, n/parity, source, configuration and executable. Reference bindings also
include each actual product bump triple. A shared Q label cannot substitute for
a different triple.

Append level 32768, execute its four worker files with the same foreground
xargs command, then collect/self-qualify. Repeat for 65536:

```bash
python3 dal-cpp/test-support/pde-risk-qualification/campaign.py level \
  --output build/dal502-followup/reference --level 32768
# Execute the worker list with the fixed command above, then:
python3 dal-cpp/test-support/pde-risk-qualification/campaign.py qualify \
  --output build/dal502-followup/reference --level 32768
```

Both paired levels are mandatory. Only if the latest full vector still fails
may 131072 be appended. The tool rejects extensions before the preceding pair
is complete, any unapproved level, and an unnecessary extension after success.
The cumulative TREE node cap is 3e12; all original quarter limits are strict.
Four processes run under a 128 MiB address-space bound each; actual RSS is
recorded per result. The ledger stores the 90-minute deadline, output cap,
charged misses and completed results. Missing, failed or nonfinite prices never
supply an oracle. Keep INCOMPLETE results at a resource/level stop.

The pure reference header preserves erfc/exp analytic BS, paired CRR and the
original daily log-price discretization, Thomas/PSOR arithmetic, endpoint
envelopes and local quadratic spot interpolation. Refactoring splits their
steps into small functions without changing their mathematical order.

The native batch entry accepts generated JSONL jobs and writes adjacent
`.results.jsonl` files. Each result records job ID, owning output, elapsed time,
node charge and peak RSS. Product captures use a separate directory and retain
original/resolved settings, all plan arrays, risks, stencils, scenarios,
original residuals and work/capacity counters. Observed N9a replay must equal its
ordinary owning output before independently enumerating all 32 active sets at
each of 40 layers.

```bash
python3 -m unittest discover -s dal-cpp/test-support/pde-risk-qualification \
  -p test_referenceplan.py -v
python3 dal-cpp/test-support/pde-risk-qualification/qualify.py EVIDENCE_DIRECTORY
python3 dal-cpp/test-support/pde-risk-qualification/audit.py EVIDENCE_DIRECTORY
```

Read numerical results and contract/directional audits together. An estimate
can meet a hard product error while its reference or refinement is incomplete.
A changed production library needs new applicable product captures; old
captures never acquire a new executable identity.
