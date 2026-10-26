# DAL-451 controlled support: Codacy repair candidate

The private caller, execution and profile tools separate allocation validation,
ledger transitions, process observation and cleanup into focused operations.
The complexity limit remains 8. No checks, analyzed paths or scanner rules are
disabled. The caller's refusal order, permanent pending markers, held allocations
and one-shot output slots remain in force.

Profile imports use a `SourceFileLoader` subclass that returns code compiled from
bounded captured source bytes. Standard `exec_module` performs module loading;
there are no direct `exec` calls in the profile tool. The fixed execution module
uses the same captured bytes for its loaded source and identity. Imported source
bypasses bytecode caches, and a valid stale cache control checks this behavior.
Initial startup modules use the explicit standard CPython loader to check cache
code against source, with the existing fixed-host, faithful-loader and no
concurrent mutation assumptions. That standard loader still interprets trusted
local bytecode internally; this does not make untrusted caches safe.

The execution identity includes methods of its local lifecycle class as well as
module functions. New sources require new identity/profile evidence; older
worker results cannot qualify this implementation. Product and compiler entry
points remain disabled. This repair executes no synthetic worker, DAL product,
RNG, MC, normal or probe, and does not consume the historical S0L grant.

The worker dispatch boundary now rejects nonfixed actions itself. The profile
helper validates its exact generated argv before `Popen`. Failed ticket
persistence closes its owned descriptor before any dispatch. A reaped profile
helper is not waited for again after logging fails; when the partial slot remains
writable its known wait4 values are included in that partial record. Persistent
I/O failure can still prevent a receipt from reaching disk and remains UNKNOWN.

The original 29 annotations are mapped individually in
`dal425-codacy-annotation-map.json`. Eleven complexity findings have local
same-tool, same-threshold RED and GREEN evidence. Eight annotations refer to the
removed profile `exec`/direct `marshal.loads` calls. Ten annotations remain for
review: subprocess imports, two fixed `Popen` sites, the fixed launcher `execve`
and `marshal.dumps` of locally created code objects. Serialization does not read
or deserialize an input and does not use dill. The launcher's unchanged literal
bootstrap executes the sealed fixed source; it still contains an explicit exec.
No waiver, security-rule exemption or current-head Codacy success is asserted.

The remaining process calls serve the existing finite lifecycle and limit
protocol. They use explicit argv, absolute interpreter/fixed source paths,
explicit environment and cwd, no shell, bounded output, owned-child reaping and
sealed source or ticket descriptors. The caller assumes a trusted private root,
fixed host and single writer. This is not a general untrusted-code sandbox.

Verification selects all three affected data/mock suites: caller 8, execution 23,
profile 15. The unchanged caller fixture retains both separate 0700 root checks
and the 0755 permission rejection, with its markers preserved. Tests inject
process and wait4 results; those values are fixtures, not observed worker usage.
The outer timeout, GNU time receipts, initial RED output, intermediate failures,
final commands and source/tool identities are delivered with the issue report.

No public API or financial behavior changed, so no public documentation, native
build, numerical or benchmark matrix is selected. The changed process lifecycle
needs independent review to select any permitted minimal real-worker control;
mock GREEN does not confer runtime qualification. Complete resource closure,
current allocation integration, history exclusion and product experiments remain
separate obligations. H_exact/H_exclude, facility qualification and all four
financial qualification flags remain false. All historical holds and UNKNOWNs
remain without refund. Current-head CI and gate disposition belong to the
coordinator after independent review.
