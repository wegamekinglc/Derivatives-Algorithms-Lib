# DAL-202 / F5: execution boundary investigation

An isolated intervention now explains the model-recording regression: GCC expands native three-input tape allocation into the model time-step loop and repeats TLS resolution. Outlining that allocation restores model-phase cost close to master. Compiled evaluation has a separate placement effect, with a material residual. The combined diagnostic remains slower than master. **The original complete Python gate remains RED, 83/90.**

This publication changes only `.codex/artifacts/reviews/dal-202/implementation.md`. No diagnostic code is shipped. The supported allocation intervention touches the native AAD backend, outside approved F5 product scope, and is returned to parent DAL-202 for an expert scope decision. PR #372 remains open against master in the owner's ready-for-review state; no semantic acceptance, merge or F6 advancement is claimed.

## Revisions and financial contract

- Baseline: merged F4 master `b4e8b56135b5cfcbbe2ddd8d753921dd40d6caa2`, tree `f531d1858b881d3cf352c05c4e461e34f4263502`.
- Starting published revision: `b1b3c719333d6bb53004a06f1663a277895ed65e`, tree `8ea892af2b3a9b101a369750d367714069cfd8a0`.
- Unchanged product and retained performance build: `dad5e6ae417a47bade877230d8f0383287d68e1d`, tree `a7097403da513f35389ad248c8761bb9c6222466`. All 1,150 source hashes match this checkout; both retained binary inventories were verified before reuse.
- The new report-only publication SHA/tree is in `investigation/published-identity.json` and the delivery comment. A report cannot contain its own commit hash.

All fixed F4/F5 semantics remain unchanged: conservative folding guards, exact adjacent-float comparisons and tiny fuzzy divisors, historical hard replay/PAYS discard, typed seeds/terminal roots, fractional future weights, eager operands, syntax-wide prefetch, zero-worker preparation errors, exception drain, LocalCheckedPaths, allocation and lifetime contracts. No binding/API/model/backend product change, benchmark, CI, threshold, skip, curve, RNG or threadpool change is published.

## Concrete instruction evidence

The selected binding-emitted worker calls `BlackScholes_<Number_>::GeneratePathAndValidate`. Its valid exact-model route records the existing three-input expression `logSpot += drift + std * gauss` and an exponential per time step. Financial expressions and tape node counts are unchanged.

The baseline's unchanged unstripped relink calls `Tape_::RecordNode<3>()` at `0x37f61f`; this allocator receives an already resolved tape pointer. In inherited F5, allocation is inlined into the loop. Extra TLS calls at `0x381492`, `0x3814bd`, `0x381512` and `0x381584` reload the tape while allocating the node, derivatives and adjoint-pointer slots. There are four extra TLS resolutions (eight versus four) on the ordinary validated time-step path. The fresh current relink retains this pattern at relocated addresses.

`base-model.asm`, `candidate-model.asm`, per-overlay `selected-model.asm`, link maps and module hashes retain these exact bodies. Whole-function static TLS site counts are baseline 15 / current 20 / record3-outline 15; those totals include fallback and initialization branches and are not dynamic counts. `hot-loop-tls.json` retains the four/eight call-site ledger. The validated-loop comparison, together with the controlled intervention below, is the evidence for the per-step cost. It does not assign an exact nanosecond cost to each lookup or exclude associated spills/layout effects.

Removing only the compiled AAD entry-point specialization returns its existing generic implementation to callers while leaving the tree specialization in core. The selected binding/LTO interpreter routes arithmetic recording through outlined `Number_::FromExpr` helpers. Direct TLS sites disappear from the dispatcher itself, moving into helpers; TLS use is not eliminated. The interpreter's static instruction count changes from 5,850 to 1,653, partly because exception/recording code moves out. Size alone is not the causal claim: actual financial workloads and phase timings establish the effect.

## Interventions and results

All variants use current F5 sources and preserve the financial work:

- `control`: unchanged relink, also compared with the original `stock` module.
- `caller`: remove only the compiled AAD specialization declaration/body from `event.hpp`/`evaluation.cpp`. All prepared bytecode and generic execution remain.
- `model`: add a core `GenerateCheckedPath<Number_>` helper at the script path-generator boundary, delegating to the unchanged model. Rejected as slower.
- `record3`: diagnostic native `Tape_::RecordNode<3>()` explicit specialization with `noinline` and the existing allocation body; no layout/graph change.
- `caller-record3`: combine the relevant boundaries to test interactions.

Allocator variants rebuild all 64 affected core/public/binding translation units; caller-only rebuilds all 9 and model-only all 3. Tracing likewise rebuilds every affected TU, including the binding value TU that emits the selected worker templates. Dependency inventories, commands, link maps and selected bodies are retained. No incompatible baseline object is transplanted into a current module. The GNU attribute and duplicated specialization body are diagnostic devices, not a portable production design.

Four-thread uninstrumented minima in milliseconds, ten alternating process samples per variant, allocation matrix:

| Workload                        | Master  | Control | Caller  | Record3 | Combined |
|---------------------------------|---------|---------|---------|---------|----------|
| Barrier AAD compiled            | 8.8006  | 11.1710 | 10.8960 | 11.1014 | 9.6999   |
| Barrier AAD tree                | 10.7172 | 11.3740 | 11.5631 | 10.3616 | 10.5311  |
| Vanilla AAD compiled            | 0.7904  | 0.9487  | 0.9138  | 0.8883  | 0.8275   |
| Comparison vanilla greeks 65536 | 2.3733  | 2.8279  | 2.7752  | 2.5928  | 2.4096   |

The first separate matrix rejects the core model helper: barrier compiled 12.3591 ms versus control 11.3714 ms. Original stock 11.3747 ms shows that an unchanged relink does not explain the failure. Caller-only is 10.4647 ms in that first matrix and 10.8960 ms in the second; both sets remain. Cross-matrix differences are not acceptance results. The combined allocation matrix still exceeds master by 10.2% for compiled barrier and 4.7% for native compiled vanilla.

One-thread every-path phase measurements, nanoseconds/path, taken from each variant's minimum end-to-end sample (not independently minimized phases):

| Variant      | Barrier compiled model | Barrier compiled evaluation | Barrier tree model | Barrier tree evaluation |
|--------------|------------------------|-----------------------------|--------------------|-------------------------|
| Master       | 1433.7                 | 1272.4                      | 1412.3             | 1910.3                  |
| Control      | 1753.0                 | 1905.7                      | 1751.4             | 1927.6                  |
| Caller       | 1807.9                 | 1541.5                      | 1810.3             | 1919.2                  |
| Model helper | 2157.5                 | 1895.7                      | 2179.0             | 1897.8                  |
| Record3      | 1414.6                 | 2025.4                      | 1416.3             | 1912.4                  |
| Combined     | 1401.7                 | 1553.2                      | 1415.4             | 1877.0                  |

Reverse propagation remains roughly 277–291 ns/path. Allocation outlining restores the model phase; caller placement reduces compiled evaluation but leaves about 281 ns/path versus master in the combined variant. Allocation alone also changes evaluation adversely, demonstrating non-additive compiler/linkage interactions. The matching model TLS patterns persist in traced and uninstrumented modules. Equal financial work and earlier equal node counts do not prove all graph layouts identical.

The separate one-thread overhead control records current compiled barrier at 41.2616 ms uninstrumented, 41.3292 ms with batch tracing but no per-path clocks, and 42.5344 ms with every-path clocks. Compiled vanilla is 3.1117 / 3.2034 / 5.3154 ms respectively. Thus clocks materially perturb small workloads; instrumented totals are not acceptance timings. All 4,160 sampled case results across the four matrices have matching PV/all-risk maps and native arguments. Full 16-case inventories, every raw sample and overhead variants are retained in each matrix's summary and raw records.

A final untimed actual-module audit compares the opcode streams emitted after the real public preparation call. All 100 compiled valuations produce identical streams on master/current, and canonical validators pass. `bytecode.py`, its two isolated public-TU overlays and `bytecode-comparison.json` retain the records. This rules out a changed opcode sequence in these workloads; constant-pool contents were not inspected. The extra diagnostic compilation is not part of any timing sample.

All 16 canonical script MC cases retain original arguments, validators, two warmups, PVs and every risk map. Native barrier/vanilla AAD use 10,000/20,000 paths; comparison cases use 16,384/65,536. Comparison barrier greeks are seven bumped double valuations, while comparison vanilla greeks use AAD. Every sampled case result and native call is compared, including identical risk keys, at relative 1e-10 / absolute 1e-9. No public numeric result is replaced with diagnostic data.

## Verification and acceptance boundary

This is a diagnostic/report delivery with no production behavior change. The controlling RED is inherited Python 83/90; diagnostic gains are not GREEN acceptance. No artificial failing unit test was introduced and unchanged full gates were not rerun to seek green.

Inherited product correctness is native 1744/1744, focused native/Adept/CoDiPack/XAD 454/453/453/452, Python 402/402 on correctness and both performance builds, exact 4 / tiny 6 / allocation 2, including all 27 lifetime combinations and 33 legacy/parity/fuzz cases. These source-matched results are retained evidence, not fresh full tests of the backend overlay. The original C++ gate 63/63, Python 83/90 and candidate A/A 90/90 retain every raw sample, inventory, two rounds of ten interleaved samples, minimum reduction and strict 4% rule. The seven Python failures remain barrier greeks 16384; vanilla greeks 16384/65536; vanilla price 16384; barrier AAD compiled/tree; vanilla AAD compiled.

Main reconstruction commands, using the recorded retained source/build identities:

```sh
python3 investigation/build_matrix.py control caller model
python3 investigation/collect.py matrix4-valid base,stock,control,caller,model 10 4 0
python3 investigation/build_matrix.py control-trace caller-trace model-trace record3 caller-record3
python3 investigation/build_matrix.py base-control-trace record3-trace caller-record3-trace
python3 investigation/collect.py matrix4-recording base,control,caller,record3,caller-record3 10 4 0
python3 investigation/collect.py matrix1-phases base-control-trace,control-trace,caller-trace,model-trace,record3-trace,caller-record3-trace 10 1 1
python3 investigation/collect.py matrix1-overhead base,control,caller,record3,caller-record3,base-control-trace,control-trace,caller-trace,record3-trace,caller-record3-trace 10 1 0
python3 investigation/summarize_matrix.py matrix4-valid matrix4-recording matrix1-phases matrix1-overhead
python3 investigation/bytecode.py
```

Two diagnostic setup failures were corrected before sampling: `inspect.py` shadowed Python's standard library and aborted the first matrix before workloads (renamed `inspect_assembly.py`); a specialization insertion anchor matched four backend declarations (now restricted to native). Failed first-matrix output is retained. Neither is a product failure or completed timing sample. Measurements ran sequentially after own builds ended. GCC 14 / CMake 4.2 Release, native-architecture-ON performance flags and original binding LTO/visibility settings remain. External WSL2 host load, clock instrumentation and unavailable hardware perf counters limit attribution. Windows, sanitizer and full alternative-backend overlay validation are not claimed.

## Parent decision and remaining work

The supported next repair boundary is **`dal-cpp/dal/math/aad/tape.hpp`, native `Tape_::RecordNode<3>()` allocation/inlining**. No binding API or model-formula change is needed for this experiment. Parent should authorize an AAD-backend expert to select a portable minimal boundary without duplicated production logic and validate single/multiple adjoints, block rollover and backend compatibility. The diagnostic GNU specialization is not ready for publication as product code.

Caller-side compiled placement is supported partial attribution, not a complete standalone repair. Preserve the model allocation control while isolating the remaining actual-module compiled arithmetic/dispatch cost. Do not require the seven failures to share one mechanism. Earlier root outlining, tape-pointer caching, extern instantiation, LTO/no-semantic-interposition failures and full gate/control samples remain inherited; they were not rerun as new hypotheses.

After an approved product repair, fresh native/supported-backend/Python correctness and both complete original gates remain required, followed by serial DAL-229 testing, DAL-230 documentation/CHANGELOG decision and mandatory DAL-231 review on exact revisions. Older done reports do not approve new code.

The archive retains current commands, overlays, selected disassembly, link maps, raw financial/timing/phase data, hashes and this matching report. It nests the previous archive unchanged, SHA256 `643f12959c87308ff24a8d8e54eff47b9af615f7c89eeb7ac6c099e697cce45d`; all 1,560 manifest entries and report equality were verified. Generated binaries/objects/full-module disassemblies are excluded with hashes and reconstruction commands. Absolute build paths are provenance, not deliverable links. Publication records one CI snapshot; no watch/poll or acceptance is claimed.
