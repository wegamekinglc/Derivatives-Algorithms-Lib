# Segmented Monte Carlo design critique

Verdict: Proceed with caveats.

Significant concerns addressed before implementation:

- ThreadNum() scratch can alias when multiple external callers actively execute
  tasks with slot zero. Exclusive request-local lanes avoid that shared slot.
- A fresh IRN per batch repeats linear seeking from its seed and causes quadratic
  work. Persistent lanes seek monotonically with a fixed lane cap.
- One owning result/future per batch for the entire request grows coordinator
  memory with total paths. Drained bounded waves permit ordered buffer reuse.
- A permanent admission prototype would add another full RNG. Clone before
  scheduling and move the prototype into lane zero.

Remaining caveats: IRN skip work scales with lane count; no universal parallel
speedup is promised. Finite double-precision batch sums are required even when
an ideal higher-precision mean would be finite. Pool lifecycle failures must drain
accepted work; captured owners must outlive the task group. Bitwise reproducibility
covers numerical results, not tape high-water marks affected by earlier requests.
Complete MC heap accounting must include retained tapes and lane/coordinator
storage. Keep segmentation explicit and publish measured tradeoffs.
