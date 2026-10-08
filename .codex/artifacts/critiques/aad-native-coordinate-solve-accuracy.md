# Native coordinate accuracy critique

Status: local controls verified after #499 merge. Exact-head remote acceptance
remains open; declaration/high-precision preparation alone is not acceptance.

| Risk                                                     | Required control                                                                                  |
|----------------------------------------------------------|---------------------------------------------------------------------------------------------------|
| Symmetric risk halved or zeros dropped                   | Exact independent off-diagonal and zero-parameter cases                                           |
| Native packing hides a dense gradient/cache              | O(p) binding differences, exact reverse scratch and one-byte-short budgets                        |
| Diagnostics mistaken for accuracy of actual reverse      | Reports collected from actual event invocations and seed axes                                     |
| Shared template changes optional dense behavior          | Preserve dense field order; all 25 relevant dense tests and six scoped old-caller rows            |
| Compiler changes point contribution inlining             | Retain failed samples; verify shared-helper boundaries and eighteen actual caller rows            |
| Mixed event collectors diverge                           | One private TLS collector and shared event publication; mixed serial/window/restore tests         |
| Report charged to tape or scratch escapes caller ceiling | Allocate report before event scratch; exact caller/tape peaks and refunds                         |
| Failure leaks partial results or usable partial adjoints | Unpublished collection remains empty; native graph rejects reads/reverse; later healthy recording |
| Finite ill-conditioned risk clipped                      | Explicit legal pivot case with independently known large finite gradient                          |
| Optional cost confused with regression                   | Matched old-call gates; separate new-coordinate costs, dense-complexity disclosure                |
| Next increment starts on unaccepted code                 | #499 exact-head gates and guarded merge before production edits                                   |

Keep the collector in the small optional native accuracy TU. A cache/binding
payload template plus typed validation/common publication avoids another
private mutation header. Numeric coordinate contraction is already accepted;
do not change it to accommodate native binding storage. Add no runtime field or
branch to unchecked Number/node/tape paths. Check source/archive/executable
identity rather than rerunning unrelated benchmark matrices.
