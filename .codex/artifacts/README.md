# Active Codex Artifacts

This directory is the durable output surface for active DAL specifications, designs, API notes,
critiques, reviews, plans, and performance reports.

Add work under the appropriate subdirectory only while it controls ongoing work, then retire it
after documenting the current-state outcome.

## Active Implementation

- [DAL AAD implementation ledger](plans/aad-implementation.md): full-scope
  delivery and correctness, performance, compatibility, and CI evidence.
- [Native lifetime diagnostic contract](specs/aad-native-lifetime-diagnostics.md)
  and [initial acceptance evidence](perf/aad-native-lifetime-diagnostics.md): optional
  active-number checks, ABI propagation, failure recovery and default performance.
- [Corrected lifetime diagnostic evidence](perf/aad-native-lifetime-diagnostics-corrected.md):
  assignment safety, complete builds/consumers, fresh paired performance, calibration
  failure/confirmation and resource observations; current CI acceptance remains open.
