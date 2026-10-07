# Native solve-coordinate scoped performance acceptance

Status: local functional and scoped performance acceptance complete; exact-head
platform, sanitizer, Codacy and review acceptance remain publication gates.

## Changed paths and selected callers

The native `dal-cpp/dal/math/aad/linearsolve.cpp` shares private publication,
seed/channel reversal and owned-resource helpers between dense and coordinate
payloads. Only that archive member changes: all 167 other members match the
accepted baseline byte for byte. Ordinary dense payload field order and
Number/node/tape layouts remain unchanged. Numeric coordinate, LU and diagnostic
objects reuse their immutable accepted evidence.

The predeclared ordinary caller scope has three boundaries, each measured for
complete requests and cached reverse: n=2/m=1 scalar with both inputs active;
n=32/m=4 width four with only RHS active; n=2/m=4 width eight with only A active.
Scope review adds the diagnosed n=2/m=1 scalar/both-active caller because it
shares the changed helpers. This yields eight comparisons, rather than timing
the diagnosed parameter matrix or unrelated portfolio/MC/PDE/curve/matrix paths.

Matched GCC 15.2 Release/O3, C++17, fast FP contraction, static-library builds
disable Eigen, lifetime diagnostics, profiling and native-architecture tuning.
CPU 0 affinity and DAL_NUM_THREADS=4 match both sides. No build, test or other
local validation runs concurrently with timing. Two rounds use ten alternating
process pairs per round and best-of-ten minima under the existing sustained 4%
policy. Exact numeric checksums agree in every existing-interface comparison.

## Existing-interface results

Positive percentages mean increased time relative to the accepted baseline.
The diagnosed cached row uses its separately retained borderline confirmation.

| Caller / boundary                     | Phase    | Round 1 change | Round 2 change | Verdict  |
|---------------------------------------|----------|----------------|----------------|----------|
| Ordinary n=2/m=1 scalar, both active  | Complete | +0.425%        | +3.114%        | Accepted |
| Ordinary n=2/m=1 scalar, both active  | Cached   | +2.646%        | +2.486%        | Accepted |
| Ordinary n=32/m=4 width 4, RHS active | Complete | +2.104%        | +1.120%        | Accepted |
| Ordinary n=32/m=4 width 4, RHS active | Cached   | +1.840%        | +2.994%        | Accepted |
| Ordinary n=2/m=4 width 8, A active    | Complete | +2.280%        | +0.956%        | Accepted |
| Ordinary n=2/m=4 width 8, A active    | Cached   | +2.147%        | +1.931%        | Accepted |
| Diagnosed n=2/m=1 scalar, both active | Complete | +1.008%        | +2.779%        | Accepted |
| Diagnosed n=2/m=1 scalar, both active | Cached   | +3.831%        | +3.816%        | Accepted |

The initial diagnosed cached result was +3.831%/+4.269%. Only that boundary
received additional confirmation: ten further pairs in each round, combining
twenty samples per side per round under the existing borderline rule. The
diagnosed complete phase remains in process preparation and its raw output
is retained; its accepted comparison was not reopened. Eight initial comparisons
comprise 80 processes/320 observations; confirmation adds 40 processes. Original
samples and the borderline result remain available.

This accepts the selected callers under the declared threshold. It does not
claim zero overhead, improvement to existing dense solves, or acceptance for
unmeasured future callers.

## New-interface informational costs

Packed native recording is compared with a parameter-to-dense native adapter on
the same final archive. Both activate all parameters and RHS entries. Symmetric
mirrors alias one parameter binding; the narrow-band adapter shares one fixed-zero
Number for every out-of-band entry. Symmetric/full-band adapters need no fixed
zero. The reference avoids artificially recording n-squared separate parameters
or zero nodes.

Complete requests include registration, adapter mapping/allocation or packed
capture, forward solve, reverse, one gradient read and close/destruction. Cached
reverse excludes construction and mapping, and includes seeding, reverse and one
gradient read. Cache/resource scanning and close occur outside that timer.
Each process checks every output and requested gradient against the accepted
numeric coordinate cache, including signed/zero channels. Independent Cramer/
native-expression and difference oracles are covered by tests.

Two rounds of ten alternating pairs produce 40 processes/240 observations.
Three boundaries are n=2 symmetric/m=1/scalar (10,000 iterations), n=64 band
below=1/above=2/p=252/m=4/width=4 (200 iterations), and n=16 full band/p=256/m=2/
width=8 (500 iterations). Checksums agree within the predeclared relative 1e-10
bound; aliases can change floating-point accumulation order.

Ratios are packed time divided by dense-adapter time; they compare different
APIs and are informational, rather than additional regression gates.

| Layout / boundary          | Complete round 1 | Complete round 2 | Cached round 1 | Cached round 2 |
|----------------------------|------------------|------------------|----------------|----------------|
| Symmetric n=2/m=1 scalar   | 0.9662           | 0.9882           | 0.9568         | 0.9586         |
| Band n=64/m=4 width 4      | 0.4423           | 0.4516           | 0.3176         | 0.3195         |
| Full band n=16/m=2 width 8 | 0.8625           | 0.8497           | 0.9895         | 0.9925         |

Cached measurements expose retained operator bytes and peak reverse scratch;
they exclude caller containers, tape block capacity and process RSS. Width does
not multiply this per-channel numeric scratch.

| Layout         | Dense / packed nodes | Dense / packed retained bytes | Dense / packed scratch bytes |
|----------------|----------------------|-------------------------------|------------------------------|
| Symmetric n=2  | 7 / 7                | 616 / 608                     | 64 / 56                      |
| Band n=64      | 765 / 764            | 109232 / 47736                | 36864 / 6112                 |
| Full band n=16 | 320 / 320            | 7920 / 7928                   | 2560 / 2560                  |

The narrow-band gain comes primarily from packed bindings and avoiding dense
matrix-adjoint work; the shared-zero adapter adds only one node. The full-band
endpoint retains eight more bytes of layout metadata. Complete-phase output
uses zero placeholders for memory fields, not measured zero memory usage.
Factors remain dense: this study establishes neither sparse LU nor an
asymptotic factorization improvement. Results do not replace the separately
accepted numeric-coordinate cost report, including its small/full-band overhead.

## Immutable evidence and reproducibility

Session root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261008/`.

- `native-coordinates-baseline/provenance.json`: accepted
  `0ed5a908d81ca02fcb8a1d5b80885868237fa6b6`-equivalent sources/archive.
- `native-coordinates-performance/provenance.json`, `samples.json`,
  `results.json` and all raw stdout: eight existing-interface comparisons.
- `native-coordinates-performance/diagnosed-confirmation/`: additional raw
  samples and combined per-round minima; original observations are retained.
- `native-coordinate-cost.cpp`, `sample-native-coordinate-cost.py` and
  `native-coordinate-cost-results/`: matched new-interface costs and memory.
- `native-coordinates-affected-tests.log` and
  `native-coordinates-affected-repair-tests.log`: 72 distinct affected cases
  accepted by combined original and failed-only repair evidence, including 28
  new cases. Earlier fixture failures remain retained.

SHA-256 provenance:

| Artifact                        | SHA-256                                                          |
|---------------------------------|------------------------------------------------------------------|
| Accepted baseline archive       | 11d9624ce748facd2b2e6ae852eed4668283dd8ae4cf19f7d2cd1e8a91c1b06a |
| Final head archive              | e28d400e5d7e101f98281331c9c00e6f22f29eb9483429db5a326aa9487402de |
| Final native runtime source     | 9e5dea48620f9c0ea17d193b9fabdeedcd013f0c41ad0060d1adcd0c6c61798f |
| Accepted ordinary executable    | 42f239b7d96d5bc6d34d9c22d74d6b471d1ac437665233b33b3c6ad2189d22f2 |
| Fresh head ordinary executable  | 4acda986a85d2e081f56fc757012a2e2928b2791e2a019dc65e930b796d4d6de |
| Baseline diagnosed executable   | f04c47f4889144c4348ccbf27ab16b8e2c421ac79a4cf936f54f5c90a12b9376 |
| Fresh head diagnosed executable | 7d0a13c8cb7602ba206ef7c0b5fbeefd41d6ad9856fb24a33146a79a4cd86028 |
| New-interface cost source       | b8da7ddbb53a0770960100408dbd51a9c3a574d87dafd4e15249c366ae23cba8 |
| New-interface cost executable   | 3c85b563bd85a631cb105913f8cd6e98478bcc92950cfe123d28070b415c5aeb |

Complete compile commands, compiler/configuration, member hashes and numeric
values accompany raw evidence. Publication must preserve this measured production
source; a behavioral repair requires affected comparisons only.
