# DAL-418 publication after accepted test complexity repair

PR #620 continues from approved `ad604e82955dbf7df4732c4796b54421e588a856` on
`agent/dal-implementer/e2075e8549aa`. All requested local gates have now passed
on fresh exports of that exact source, including accepted DAL-414. The publication
commit adds only this record, its index update and the preserved limited-review
report. Product, formal-test, build, dependency and quality inputs remain identical
to the approved source. Exact-head CI follows publication; the author does not merge.

## Review, source and history

Normal merge `c590d15398a5f5c54933f14371628a95f8661eef` retains parents
`1d42e54c1240a8114bcb895d6895870008160b8c` and accepted master
`2baceaf083258a657dc954642e7c279507fb5721`. The accepted limited review verifies
the two exact-union Markdown resolutions, automatic guide merge, all 30 independent
upstream blobs and the test-only complexity repair. No new semantic difference is
introduced by publication.

The original product review remains byte-identical, SHA-256
`cfb1e9ffa503c79a7f7ac256d0e0ee3bb86be3545cbe7e465e1bf6d00789e556`.
The initially untracked [limited-review report](../reviews/dal-418-test-complexity.md)
is preserved verbatim, compared with its authenticated attachment, SHA-256
`995b53049b9a8802ab1ddc4b1d12d49d3c423f27987b1e90dacb1e6b3cab4cd8`.
All five earlier implementation/validation/review/documentation reports retain
their recorded hashes. Their historical stage-status statements remain historical.

The approved test remains CCN 7, with its independent expected-value and scenario
helpers at 5/4; all 50 functions satisfy the unchanged limit 8. The accepted
18-input/36-instrument/72-observation evidence, fatal assertions, exact-zero and
relative-error controls remain applicable through exact test-source identity.
The earlier actual-parent/master RED, test-first commits and independent oracle,
subnormal and identity evidence are retained without inventing another functional RED.

Main source archive SHA-256:
`f094841855ea4545e79caf140d375b8a1d262350edba39157529594ee501b311`.
Both OS exports include all five exact dependency pins. Every one of 5,686 actual
source/dependency files is checked: Linux is byte-identical; Windows differs only
in 16 generated files' CRLF/LF representation. There is no semantic mismatch.
Actual Python selections are Linux 3.13.9 and native Windows 3.12.13. Both use
pybind11 3.1.0, and all 55 selected pybind headers match across OSes.

## Complete local gates on the approved integration

Every command below exits 0. Counts come from this run's raw logs rather than
the previous head's results.

- Independent Linux core with public/Python/Excel disabled: 3,299/3,299 tests and
  36/36 fast core examples. Target and compile-command closure contains no public
  dependency. Core example locations, purposes and target names are preserved.
- Original `build_linux.sh`, GCC 14.3 / CMake 4.2.3, Python enabled:
  3,917/3,917; separate fast examples 39/39 and original installed consumer 3/3.
  The complete workspace Python result retained in CTest's original log is
  1,494 passed / 2 skipped.
- Original native `build_windows.bat`, MSVC 19.44.35228.0 / actual CMake 3.27.6:
  4,003/4,003, including 39 fast examples and the existing slow `european_mc`.
  That slow example passes in 756.25 seconds with its original budget/timeout;
  this is a smoke-test observation, not a paired benchmark claim.
- Fresh standalone installed-package Python: Linux 1,493 passed / 3 skipped;
  native Windows 1,492 passed / 4 skipped. Each complete Python CTest entry passes.
- Fresh native workspace Python: 1,493 passed / 3 skipped. Enabling Python leaves
  all seven existing library/test/XLL outputs and all 11 pre-existing installed
  CMake/library contract files byte-identical. The earlier complete native and
  default-install evidence therefore retains those exact inputs.
- Native default installed consumer and Excel, separately under actual CMake
  3.27.6 and 4.2.3: each 3/3 and 269/269. Each driver proves default CP936 and
  empty `CL`/`_CL_`; all 74 selected compile commands use the exported static CRT
  without `/utf-8`, source-charset or execution-charset overrides.
- Rebuilt C++/actual Excel joint caller controls: 3/3 on each OS, including
  18 plain/tiny/full-mantissa CALL/PUT cases and owning-error/settings boundaries.
  Complete JSON and seven identities match across OSes. Maximum observed price
  difference is `1.3478801408339791e-15`, not an arbitrary-scale precision promise.
- All four freshly built workspace/installed Python modules independently match
  those 18 records and preserve three owning errors. Module paths are checked
  to exclude an earlier editable installation.

The two optional independent market-IV oracles, installed-only opaque-helper and
Windows huge-allocation resource-limit cases retain their actual skips, never
passes. Existing unrelated nodiscard/compiler warnings remain in the raw logs;
these builds are not described as warning-free. No live Excel workbook/registration
smoke or local reproduction of every CI compiler/wheel/diagnostic leg is claimed.

The exact source, cache, compile/link commands, discovery, Python environments,
188 artifact identities, exits and original CTest/pytest logs are retained in the
recoverable issue attachment. Canonical logs are saved before later scoped CTest
invocations overwrite the normal LastTest log. Original sampling, acceptance
tolerances, timeouts and benchmark thresholds remain unchanged. The formatter is
still import-only; the accepted helper extraction changes no product hot loop.
No unrelated benchmark matrix or repeated product/design/documentation chain is added.

## Delivery and remaining protected gates

The [earlier publication record](dal-418-publication.md) and
[test complexity record](dal-418-test-complexity.md) retain their original facts.
Old head `1d42e54c` completed with 35 SUCCESS / 1 Codacy ACTION_REQUIRED, not an
accepted CI result. Its GraphQL EOF and same-head foreground transport recovery
are retained. The repaired source's local CCN and matrix evidence do not relabel
that old CI failure or promise the future provider's result.

Runtime tools, probes and raw evidence remain outside Git. The accepted complexity
bundle/archive and original role reports stay recoverable; the final evidence
attachment includes inventory/hashes and a verified restoration. Publication's
Git-tree audit proves every entry outside the three necessary Markdown paths
equals the approved head. Documentation and patch-integrity checks cover those
new files; the complete numerical gates apply through unchanged tested inputs.

The existing ready PR preserves `Closes DAL-408` and `Closes DAL-418`. After the
local gates and scope audit, the same branch is pushed normally. One foreground
`gh pr checks 620 --watch` collects the complete final-head CI result in this
dispatch. Exact SHA, CLI exit and provider terminal states are delivered in the
final issue evidence and PR checks without creating another code head just to
record CI. This record makes no advance claim that CI or protected merge gates pass.

The coordinator checks final protected merge requirements. No author review,
merge, duplicate PR/branch/issue or expert dispatch occurs; DAL-418/DAL-408 and
their parent remain in progress.
