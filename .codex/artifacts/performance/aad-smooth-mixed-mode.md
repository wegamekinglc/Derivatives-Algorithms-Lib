# Smooth mixed-mode scoped performance plan

Status: measurement pending; implementation and analytic acceptance first.

Baseline is merged #527, `19a34172a23dd85041d7bfcfd694b63bf6529c6d`.
Its accepted native/public archives and complete build provenance may be reused
only with verified identities. The new module does not change ordinary native
node layout, first-order operations or existing financial adapters.

Map `dal-cpp/dal/math/aad/forwardoverreverse.hpp` and its implementation to two
complete smooth requests: one polynomial with a single direction and one smooth
European Black-Scholes kernel with multiple directions. Compare complete
request latency, actual recording/sweep counts, checksums against independent
analytic products, and tape/cleanup peaks with the existing bump-over-AAD
alternative. Record that the methods have different approximation semantics.

Use two rounds of ten interleaved pairs per selected case, warmup, calibrated
repetitions and alternating first side. Keep every sample and immutable
source/dependency/configuration/archive/executable identity. These measurements
characterize the new prototype; they do not establish a generic speedup or a
regression in unchanged native callers.

Empty inputs/directions, signs, tiny directions, primitive domains, aliased
operands, recording modes, budgets and failures are functional boundaries.
Unchanged tape kernels, market calibration, PDE, LSMC, portfolios and complete
parameter matrices are excluded. Expand only for a concrete failure or newly
identified caller gap, recorded before execution. Required current-head CI
and actual diagnostic/platform test logs remain merge gates.
