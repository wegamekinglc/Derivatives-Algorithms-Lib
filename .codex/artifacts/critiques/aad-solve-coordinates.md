# Explicit solve-coordinate critique

Verdict: Proceed with caveats.

## Blocking issues

None in the revised numeric scope. The specification explicitly separates
numeric acceptance from the still-required native integration.

## Significant concerns

- A symmetric factory must accept invertible indefinite matrices: an SPD-only
  implementation would silently narrow the contract. Add a pivoted example.
- Off-diagonal gradients need a sum, not an average or a factor-two shortcut
  based on assumed symmetry of Lambda X^T. Use a nonsymmetric seed/RHS example.
- Band counts and row offsets need wide intermediate arithmetic; reject dense
  n*n beyond the allocator's representable double-buffer extent before allocating,
  even for narrow bands. Matrix storage and coordinate counts use size_t;
  an arbitrary INT_MAX element-count restriction would narrow valid layouts.
- Dense-reference tests alone could pass an implementation that allocates the
  unwanted dense adjoint. Exact budget admission must establish O(p+n*m)
  reverse storage. General LU remains dense; do not claim sparse speedups.
- Overflow, failure and concurrent const calls must leave the owning cache
  usable. RHS-only reverse must avoid unused coordinate overflow.

## Minor notes

Constant-size layout values and rowwise traversal avoid an extra owned mapping
table. Factory names and result fields align with the existing numeric operator.
The installed-header check must include the new header, not only source builds.

## Counter-proposals and author questions

Do not build dense native Number matrices from packed parameters: fixed zero
slots and n*n retained bindings would defeat the planned native resource goal.
Keep native integration in its own PR after numeric merge. No user clarification
is needed to implement the numeric contract.
