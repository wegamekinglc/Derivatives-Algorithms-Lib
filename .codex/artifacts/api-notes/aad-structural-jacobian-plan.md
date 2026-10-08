# Structural Jacobian numeric API

Audience: C++ developers implementing a verified structural reverse caller.
Controlling [specification](../specs/aad-structural-jacobian-plan.md).

## Current and proposed surfaces

Keep `HarvestCurveJacobian` and its optional extraction widths unchanged.
Add `dal/math/aad/structuraljacobian.hpp` in namespace `Dal::AAD`, with no
dependency on active Number or tape headers:

```cpp
struct StructuralJacobianSettings_ {
    std::optional<size_t> numericPayloadBudgetBytes_;
};

class StructuralJacobianPlan_ {
public:
    size_t Inputs() const;
    size_t Outputs() const;
    size_t ColorCount() const;
    const Vector_<size_t>& RowSupport(size_t row) const;
    std::optional<size_t> RowColor(size_t row) const;
    const Vector_<size_t>& ColorRows(size_t color) const;
    size_t ResultBytes() const;
    size_t DirectionBytes() const;
    size_t NumericPayloadBytes() const;
};

StructuralJacobianPlan_ PlanStructuralJacobian(
    size_t inputs,
    const Vector_<Vector_<size_t>>& rowSupports,
    const StructuralJacobianSettings_& settings = {});

Matrix_<> RecoverStructuralJacobian(
    const StructuralJacobianPlan_& plan,
    const Matrix_<>& colorGradients);
```

The plan owns normalized supports and immutable color metadata. Optional colors
distinguish empty rows without a public sentinel. Getters validate indices.
Recovery owns its full result; source supports/gradients are borrowed only during
the call. Reassign a moved-from plan before using its getters or recovery.
Matrix dimensions and checked payload/budget admission follow the
specification, including zero budgets and empty but wide axes.

## Typical use and constraints

```cpp
const auto plan = PlanStructuralJacobian(4, {{0, 1}, {2}, {1, 3}});
Matrix_<> directions(2, 4, 0.0);
// The verified caller fills each direction with its complete seeded VJP.
const auto jacobian = RecoverStructuralJacobian(plan, directions);
```

This example demonstrates shape/ownership only; filling zeros does not validate
the caller's derivative implementation. Supports require independent structural
proof. Unknown support is conservatively full. This API performs no recording,
seeding, cache invalidation, automatic strategy selection or binding projection.

Reject bad dimensions/indices, overflow, insufficient numeric budget, wrong
gradient shape and every non-finite gradient before result publication. Errors
identify the operation and offending constraint. Published guides state the
proof boundary directly; numeric validation never implies graph dependency proof.

No existing signature/default changes. No Python/Excel strategy is exposed before
the subsequent native/provider contracts exist. Open questions: none for this
boundary; later binding strategy settings remain separate work.
