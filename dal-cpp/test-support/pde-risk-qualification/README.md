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
Admission checks the full vector at each coarse/fine/extra-half mesh and each
raw parity, plus fine/extra-half agreement. The original extra-half mesh field
is retained alongside the three explicit mesh differences.
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

Scoped product continuation is independent of reference scheduling:

```bash
python3 dal-cpp/test-support/pde-risk-qualification/products.py prepare \
  --output build/dal502-followup/products
xargs -n1 -P2 ./build/dal502-tools/dal502_driver --batchfile \
  < build/dal502-followup/products/workers.txt
python3 dal-cpp/test-support/pde-risk-qualification/products.py collect \
  --output build/dal502-followup/products
```

The `complete`, `supplement` and `observed` phases prepare separate worker
lists. Completion fills N1/N2; supplements capture N2 contacts, N10a brackets
and N12 parallel differences. Prepare `observed` with `--driver` pointing to
`dal502_observed`, then execute its sole job with `DAL502_LAYER_FILE` pointing
to a new N9a trace. Collect before running `observe.py collect`. Keep at most
four numerical processes across concurrent product and reference work.

`integrate.py prepare --directory PRODUCTS` explicitly imports the original
independent N12 parallel references with their hashes; it does not import old
product results as current. After supplements, run `supplement.py`, then
`integrate.py analyze --directory PRODUCTS --references REFERENCES`. Integration
replays every audit in a separate view, records its input dependencies, and
marks any dependency on an old product capture INCOMPLETE. N9 failure/work
controls cite the accepted repair's direct tests and matching library, rather
than relabeling historical captures. American directions have a separately
recorded capture phase; numerical admission still needs the qualified finest
reference.

Native batches refuse to overwrite existing result files. The reference TREE
exception path emits a valid error object before any price key is published.
These wrapper checks do not change successful reference arithmetic. When a
wrapper changes after a run, retain that run's actual source and executable
identity; a later build is not its producer.

The separately approved BBS diagnostic uses the opt-in `dal502_bbs` target,
which has no core/public library link. `bbs.hpp` leaves `reference.hpp` and
pure CRR untouched. Pilot inputs, witnesses, caps and stop rules are frozen
in the active acceptance contract. `bbsplan.py` uses a new method/source/
configuration/executable identity and never imports a CRR price or bridge.

```bash
cmake --build build/dal502-bbs-tools --target dal502_bbs -j4
python dal-cpp/test-support/pde-risk-qualification/bbsplan.py \
  --output build/dal502-bbs/data --driver build/dal502-bbs-tools/dal502_bbs
xargs -n1 -P4 prlimit --as=134217728 -- \
  ./build/dal502-bbs-tools/dal502_bbs --batchfile \
  < build/dal502-bbs/data/witness.workers.txt
python dal-cpp/test-support/pde-risk-qualification/bbsverify.py witness \
  --output build/dal502-bbs/data
# Only after all witnesses pass, execute pilot.workers.txt in the same way.
python dal-cpp/test-support/pde-risk-qualification/bbsverify.py finish \
  --output build/dal502-bbs/data
```

Unexpected numerical errors stop the worker and retain its partial counters.
After a stopped batch, `bbs_stopped.py --output ...` derives a partial report
from saved results without new BS/tree evaluations. No rerun or expansion is
implicit. The first bounded pilot stopped on three negative European-value
guards; its missing rows and all six final scale qualifications remain
INCOMPLETE. European/formula controls passing does not establish American
pilot feasibility or replace S3 mandatory TREE evidence.
