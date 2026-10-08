# Native solve accuracy performance acceptance

Status: local scoped acceptance complete; publication gates remain open.
Overall verdict: no regression in the twelve selected existing caller rows.
The new optional checks have additional work and the costs below are informational.

## Scope and provenance

Baseline is merged #497, `0c5b5918b614a2856df6394b84d7cccbe4726060`.
The predeclared scope is retained in session evidence
`native-accuracy-performance-scope.md`. Six ordinary dense, two diagnosed and
four packed-coordinate rows cover actual changed helpers/recording callers.
No full benchmark target or parameter matrix is built or measured. This is
standalone native caller acceptance, not a claimed scheduled nine-target run.

167 previous archive members remain byte-identical; only native linear-solve
and recording objects change, with one additional optional accuracy object.
Hash multisets distinguish duplicate archive basenames and prove that the
accepted numeric accuracy object is preserved. Number/node/tape layouts,
ordinary payload member order and ordinary reverse loops stay unchanged.
The recording edit adds cold identity access; existing recording method bodies
are unchanged. MC/portfolio, PDE, RNG, interpolation, Krylov and Cholesky kernels
are outside the change and retain object identity; their timing is reused.

The review repair changes only the optional native accuracy object. All three
freshly relinked existing-caller executables retain the exact measured hashes,
so the twelve-row acceptance is reused without repeating timing.

Release GCC 15.2, C++17, O3, `-ffp-contract=fast`, portable ISA, native AAD;
Eigen, lifetime diagnostics and profiling are OFF. Both sides use identical
workloads/includes/flags, CPU 0 affinity and DAL_NUM_THREADS=4. Two rounds of ten
alternating process pairs use best-of-ten minima and a sustained +4% threshold.
Existing acceptance retains 120 process outputs and 480 row observations;
new-cost acceptance retains 40 outputs and 240 observations. Compilation and
sampling are sequential. This is a shared host; minima do not certify performance
on other compilers, builds, CPUs or unmeasured shapes.

## Existing callers

| Existing caller                              | Round 1 change | Round 2 change | Verdict |
|----------------------------------------------|----------------|----------------|---------|
| coordinate/band-n64-rhs4-width4-cached       | -1.84%         | +5.43%         | Pass    |
| coordinate/band-n64-rhs4-width4-complete     | +0.96%         | -0.69%         | Pass    |
| coordinate/symmetric-n2-rhs1-scalar-cached   | -0.09%         | +0.95%         | Pass    |
| coordinate/symmetric-n2-rhs1-scalar-complete | -0.01%         | -0.41%         | Pass    |
| diagnosed/n2-rhs1-width0-activity0-cached    | -0.14%         | -0.11%         | Pass    |
| diagnosed/n2-rhs1-width0-activity0-complete  | -0.17%         | +0.74%         | Pass    |
| ordinary/n2-rhs1-width0-activity0-cached     | +2.21%         | +0.44%         | Pass    |
| ordinary/n2-rhs1-width0-activity0-complete   | +0.77%         | -0.20%         | Pass    |
| ordinary/n2-rhs4-width8-activity2-cached     | +0.01%         | -1.11%         | Pass    |
| ordinary/n2-rhs4-width8-activity2-complete   | +0.92%         | +0.61%         | Pass    |
| ordinary/n32-rhs4-width4-activity1-cached    | -0.60%         | +1.83%         | Pass    |
| ordinary/n32-rhs4-width4-activity1-complete  | -0.83%         | +0.22%         | Pass    |

The narrow-band cached row is +5.43% in round two and -1.84% in round one.
It does not meet the declared sustained-regression rule. All raw results are
retained; neither an improvement nor a universal speedup is claimed.

## New optional checking costs

Both variants compose native inputs and reverse actual seeds. The diagnosed
variant performs less work: it does not check physical transpose residuals or
return invocation/event/channel reports. The same-responsibility regression
threshold does not apply to this added responsibility.

| Optional checking boundary            | Round 1 us: diagnosed -> checked | Round 2 us: diagnosed -> checked | Ratios      |
|---------------------------------------|----------------------------------|----------------------------------|-------------|
| medium-n32-rhs4-width4-full-cached    | 56.0190 -> 76.4750               | 56.1203 -> 76.8071               | 1.365/1.369 |
| medium-n32-rhs4-width4-full-complete  | 110.2764 -> 131.6905             | 110.5411 -> 133.3949             | 1.194/1.207 |
| small-n2-rhs1-scalar-full-cached      | 0.1937 -> 0.2865                 | 0.1933 -> 0.2857                 | 1.479/1.478 |
| small-n2-rhs1-scalar-full-complete    | 0.7947 -> 0.9264                 | 0.7881 -> 0.9175                 | 1.166/1.164 |
| tiny-n1-rhs1-scalar-rhs-only-cached   | 0.1073 -> 0.1788                 | 0.1079 -> 0.1794                 | 1.667/1.662 |
| tiny-n1-rhs1-scalar-rhs-only-complete | 0.5506 -> 0.6567                 | 0.5489 -> 0.6532                 | 1.193/1.190 |

The medium boundary uses n=32, four RHS and width four, including a zero
channel. The tiny boundary keeps A=1e-150, B=1e150 and finite RHS-only risks;
unrequested overflowing matrix risk is omitted. Complete requests include
recording/capture/reverse/close. Cached rows include seed setup, reverse,
collection publication and destruction. Independent closed-form value/RHS-risk
checks run outside timing; all paired checksums agree and are finite.

| Boundary                     | Tape retained bytes | Reverse scratch bytes | Caller retained bytes | Caller peak bytes |
|------------------------------|---------------------|-----------------------|-----------------------|-------------------|
| medium-n32-rhs4-width4-full  | 30320 -> 38584      | 10240 -> 10272        | 2080 -> 2280          | 12320 -> 12552    |
| small-n2-rhs1-scalar-full    | 656 -> 760          | 64 -> 72              | 40 -> 120             | 104 -> 192        |
| tiny-n1-rhs1-scalar-rhs-only | 524 -> 604          | 16 -> 24              | 24 -> 104             | 40 -> 128         |

Resource observation runs outside timed loops. Retained caller values include
active solution containers, forward diagnostics and optional reports. Tape
retained bytes include descriptors/table/cache/bindings. The checked event adds
n*n doubles for the physical transpose plus 72 descriptor/cache/token bytes in
this build. A collected entry adds its metadata and m*channels doubles; numeric
reverse scratch adds m doubles for errors. Caller/tape scratch overlap and must
not be added as independent RSS measurements. Fixture and allocator bookkeeping
are excluded from these payload capacities.

## Retained evidence

Session root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261008`.

- `native-accuracy-performance/{provenance,results,samples}.json` and individual
  round/process outputs retain the original accepted twelve-row timing.
- `native-accuracy-performance/final-repair-identity.json` retains the final
  archive/object/source identities and fresh legacy relink commands.
- `native-accuracy-cost/{provenance,results,samples}.json` retains final optional
  cost binaries, exact commands, workload hash and raw process outputs.
- `native-accuracy-local-acceptance.json` retains the 25 new and 78 existing
  distinct case union; original RED/failed fixtures and repair evidence remain.

- Final archive SHA-256: `b5df1f4d628e53e75c9d53ead64c9e86d6e68f31151c38b0f1be2632983de622`.
- Existing results SHA-256: `00ccdccea12c7952874b0a429d0b2d22a2b738a030f3263d822252c95af039cf`.
- New cost results SHA-256: `cb990d7f74f57409a1034074a668d473973951884ddd2b437b22835447625f42`.
- New cost workload SHA-256: `f2bfb177472807ce7aa9b7a90ffa087be3954019db3e8991d906a7225a6ab462`.

Final production source hashes are retained in the identity manifest and match
the installed archive. No production source changes are accepted after sampling
without identity analysis or affected-case remeasurement.
