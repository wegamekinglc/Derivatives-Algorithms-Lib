# LSMC curvature C++ surface

Status: active API decision.

Add `EvaluateBlackScholesLsmcCurvature(prepared, parameters, pathCount, bumps)` in
`dal/script/lsmccurvature.hpp`. Preparation is an owning
`shared_ptr<const PreparedScript_>` created with native AAD simulation settings.
The four required arguments mirror the existing financial curvature entry and
make every outer direction and step explicit.

`PreparedModelType()` retains the dynamic type used by executable preparation,
without retaining its model state. This entry requires the concrete native
`AAD::BlackScholes_<double>` type. Hybrid and custom-derived preparations fail
before training even if all their observations are Black–Scholes-compatible.
The numeric point supplies replacement parameters within that same model family.

`BlackScholesLsmcCurvatureResult_` exposes `Value()`, `Gradient()`, `Point()`,
`Directions()`, `Steps()`, `HessianProducts()`, `Prepared()`, `BasePolicy()` and
`Execution()`. Numeric input order is spot/vol/rate/div followed by prepared
constants; display labels need not be unique, and no label-keyed map is used.

Execution retains the distinct frozen/retrained method, gradient-request count,
pricing/training/validation/replicate counts and maximum replay recording
capacity/cleanup reserve. Requested inner-relative policy step remains in the
immutable preparation's simulation settings; actual inner steps follow the
documented per-coordinate rule at each outer point. A gradient-request count is
not a reverse-sweep count.

No automatic step, symmetry correction, hard-price substitution, regression AD
or error-bar invention is added. Existing LSMC and segmented curvature APIs keep
their meanings. Preparation and policy numeric storage are excluded explicitly
from the returned curvature-output budget. Fully expired/non-exercise or
non-native preparations fail with LSMC-specific context.

Typical use prepares an EXERCISE script against a Black–Scholes model, selects
Frozen or RetrainedBump in simulation settings, supplies a spot unit direction
and reads column zero as a finite-step spot Gamma. Python/Excel projection is
reserved for the planned binding stage.
