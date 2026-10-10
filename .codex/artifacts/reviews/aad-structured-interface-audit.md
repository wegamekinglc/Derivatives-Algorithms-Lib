# Structured-interface audit review

## Findings

One material delivery gap: #506 accepts a C++ financial PDE example, but no
closed owning public/Python PDE price/Greek request exists. Record the missing
boundary and independent acceptance in the next implementation increment;
do not mark Python/Excel parity complete from native operator tests.

Generic solve/root/PDE recording remains C++-owned. Current calibration
pullbacks retain their own method and boundary metadata; an effective inverse
does not imply the exact residual-Hessian stationary-fit derivative. Existing
passive Python IVS overrides remain supported. These limits are explicit in
the current-state methodology and active API handoff.

## Open questions

None blocking publication of the audit. Detailed financial PDE defaults,
recording preservation and capacity formulas require the next specification
and critique before production changes.

## Verification

- Read native solve/root/PDE headers, their installed C++ example/support,
  public financial headers, Python/Excel registrations and original H.3 scope.
- No production, test, generated interface, build or CI configuration changes.
  Preserve #532 source/dependency/module identities and accepted numerical/cost
  evidence; do not rerun unchanged full suites or claim new dynamic acceptance.
- Documentation links/format pass for 164 Markdown files. All changed paths
  classify as documentation; exact-head publication checks remain required.
- Retire #532 controls to immutable accepted-head references; preserve the
  actual 35/35 checks, platform counts, two audits and identical merged tree.

## Summary

Verdict: Approve the bounded audit and handoff. PDE implementation, Excel and
final requirement acceptance remain open; this PR makes no new algorithm claim.
