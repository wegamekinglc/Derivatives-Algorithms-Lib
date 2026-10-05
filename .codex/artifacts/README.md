# Active Codex Artifacts

This directory is the durable output surface for active DAL specifications, designs, API notes,
critiques, reviews, plans, and performance reports.

Add work under the appropriate subdirectory only while it controls ongoing work, then retire it
after documenting the current-state outcome.

## Active Implementation

- [DAL AAD implementation ledger](plans/aad-implementation.md): full-scope
  delivery and correctness, performance, compatibility, and CI evidence.
- [Weighted script risk specification](specs/aad-weighted-script-risk.md),
  [API decisions](api-notes/aad-weighted-script-risk.md) and
  [critique](critiques/aad-weighted-script-risk.md): active F02 weighted-root,
  output preflight, batch integration and owning three-language delivery.
  [Preflight review](reviews/aad-weighted-preflight.md) records the implemented
  metadata/budget boundary and its remaining execution requirements.
- [Native lifetime diagnostic contract](specs/aad-native-lifetime-diagnostics.md)
  and [initial acceptance evidence](perf/aad-native-lifetime-diagnostics.md): optional
  active-number checks, ABI propagation, failure recovery and default performance.
- [Corrected lifetime diagnostic evidence](perf/aad-native-lifetime-diagnostics-corrected.md):
  assignment safety, complete builds/consumers, fresh paired performance, calibration
  failure/confirmation and resource observations; the final requirement audit remains open.
- [Native-only AAD removal contract](specs/aad-native-only.md): remove XAD,
  CoDiPack and Adept implementation/dependencies; preserve native checks and performance.
- [Native operation contract](specs/aad-backend-adapter.md): scalar/vector seed/read,
  recording integration and D03 acceptance after external backend removal.
