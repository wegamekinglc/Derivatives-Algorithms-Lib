# Segmented MC boundary critique

Verdict: Proceed with caveats.

No blocking numerical change: the proposal delegates to accepted owning public
entries and exposes their existing native contract. Preserve these constraints
during implementation and review:

- Raw `number[]` and handle-vector readers can truncate blank ranges. Read the
  full point matrix and test leading/interrupted/trailing gaps in both orientations.
- A rounded worksheet double cannot preserve uint64. Admit canonical decimal
  text separately, test UINT64_MAX/overflow and return text from settings and
  execution. Ordinary MC path conversion must not narrow size_t to int.
- The public mean constructor is intentionally private. Build a local immutable
  mean snapshot from accepted getters; never reprice to create a base handle.
- Requested path recording cap and effective curvature cap differ. Preserve both;
  label maxima per path and keep worksheet ownership outside the native cap.
- A zero-direction result still computes one base. Empty XLL spills use empty
  text; verify logical shape independently of the physical one-cell spill.
- Sealed observations must survive changed external history. Passive query tests
  and rejected nested factories must preserve the caller's graph and seeds.
- Two informational costs cover both evaluation entries and small batch/segment
  boundaries. Unequal ownership is not a regression or speedup verdict; no native,
  Python, rate, Dupire or PDE matrix expansion is justified by this adapter.

No author question remains. Actual Windows runtime and exact-head publication
gates remain required after the local tests.
