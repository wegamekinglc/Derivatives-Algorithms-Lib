# Automatic Adjoint Differentiation (AAD)

Documentation of the AAD framework in `dal/math/aad/`.

## File Map

| File                         | Purpose                                                                        |
|------------------------------|--------------------------------------------------------------------------------|
| `dal/math/aad/expr.hpp`      | Expression template hierarchy, operator overloads, and dual-path `Number_`     |
| `dal/math/aad/tape.hpp`      | `Tape_` declaration (native `BlockList_`-based tape or XAD wrapper)            |
| `dal/math/aad/tape.cpp`      | Propagation, mark, rewind, and clear for both native and XAD paths             |
| `dal/math/aad/node.hpp`      | `TapNode_` — per-operation node storing local derivatives and adjoint pointers |
| `dal/math/aad/blocklist.hpp` | `BlockList_<T_, BLOCK_SIZE>` — segmented arena allocator backing the tape      |
| `dal/math/aad/aad.hpp`       | Multi-result support (`SetNumResultsForAAD`), `PutOnTape`, `Clear`             |
| `dal/math/aad/aad.cpp`       | Static initializers for `TapNode_::numAdj_` and `Tape_::multi_`                |
| `dal/math/aad/sample.hpp`    | `Sample_<T_>` and `Scenario_<T_>` — container for AAD-aware market scenarios   |

## Design Overview

The library implements reverse-mode automatic differentiation (AAD) — the algorithm behind backpropagation in neural networks and adjoint Greeks in finance. There are two compilation paths controlled by `DAL_USE_XAD_AAD`:

1. **Native expression-template path** (default) — taken from Antoine Savine's book *Modern Computational Finance: AAD and Parallel Simulations* (Wiley, 2018). Uses C++ expression templates to record operations onto a thread-local tape, then propagates adjoints backward through the tape.

2. **XAD path** — delegates to the external XAD library. `Number_` becomes `xad::adj<double>::active_type`, and the tape wraps `xad::adj<double>::tape_type`.

Both paths expose identical free-function APIs (`Value()`, `Adjoint()`, `PropagateToStart()`, `Clear()`, `Mark()`, etc.), so the rest of the library is source-compatible with either.

## Core Types

### Expression Template Hierarchy

```
Expression_<E_>                       ← CRTP base, implicit double conversion
├── Number_                           ← leaf: double value + tape node pointer
├── BinaryExpression_<L, R, OP>       ← a + b, a * b, a - b, a / b, pow, max, min
└── UnaryExpression_<ARG, OP>         ← exp(a), log(a), sqrt(a), abs(a), NPDF, NCDF, etc.
```

### Number_

`Number_` (`dal/math/aad/expr.hpp:469`) is the active type — a `double` with a link to a tape node. It has `numNumbers_ = 1`, a compile-time constant indicating it contributes one leaf to any expression tree.

**Construction from an expression** — `Number_ n = a * b + c / d;` does two things:
1. Evaluates the expression tree to a `double` via `Value(e)`
2. Calls `FromExpr(e)` which:
   - Creates a tape node with `CreateMultiNode<N_>()` where `N_` is the compile-time-known number of leaf variables
   - Calls `e.PushAdjoint<N_, 0>(*node, 1.0)` to walk the expression tree recursively

**PushAdjoint** — each expression type recursively pushes its chain-rule contributions:
- `BinaryExpression_`: pushes `adjoint * LeftDerivative` to the LHS, `adjoint * RightDerivative` to the RHS
- `UnaryExpression_`: pushes `adjoint * Derivative` to the argument
- `Number_` (leaf): records its adjoint pointer and derivative into the node's arrays

After `FromExpr`, the tape node stores:
- `pDerivatives_[i]` — the local partial derivative w.r.t. each argument
- `pAdjPtrs_[i]` — a pointer to each argument's adjoint accumulator

### Operator Policy Structs

Each operator is a stateless struct providing `Eval` and derivative functions:

| Policy Struct | Operation  | ∂L/∂lhs                     | ∂L/∂rhs                     |
|---------------|------------|-----------------------------|-----------------------------|
| `OPMult_`     | `l * r`    | `r`                         | `l`                         |
| `OPAdd_`      | `l + r`    | `1`                         | `1`                         |
| `OPSub_`      | `l - r`    | `1`                         | `-1`                        |
| `OPDiv_`      | `l / r`    | `1/r`                       | `-l/r²`                     |
| `OPPow_`      | `l^r`      | `r * v / l`                 | `log(l) * v`                |
| `OPMax_`      | `max(l,r)` | `1` if `l>r`, `0` otherwise | `1` if `r>l`, `0` otherwise |
| `OPMin_`      | `min(l,r)` | `1` if `l<r`, `0` otherwise | `1` if `r<l`, `0` otherwise |

Unary operators with a scalar operand use `OPMultD_`, `OPAddD_`, `OPSubDL_`/`OPSubDR_`, etc., which have a single `Derivative(r, v, d)` method.

Math functions:

| Policy Struct   | Function  | Derivative                     |
|-----------------|-----------|--------------------------------|
| `OPExp_`        | `exp(r)`  | `v` (itself)                   |
| `OPLog_`        | `log(r)`  | `1/r`                          |
| `OPSqrt_`       | `sqrt(r)` | `0.5/v`                        |
| `OPAbs_`        | `\|r\|`   | `1` if `r>0`, `-1` if `r<0`    |
| `OPNormalDens_` | `NPDF(r)` | `-r * v`                       |
| `OPNormalCdf_`  | `NCDF(r)` | `NPDF(r)`                      |
| `OPErfc_`       | `erfc(r)` | `-1.12837916709551 * exp(-r²)` |

### Operator Overloading

Overloaded operators return expression types rather than computing values. For example:

```cpp
template <class LHS_, class RHS_>
BinaryExpression_<LHS_, RHS_, OPAdd_> operator+(const Expression_<LHS_>& lhs,
                                                const Expression_<RHS_>& rhs);
```

When a scalar is one side, a `UnaryExpression_` is returned instead (e.g., `a * 3.0` → `UnaryExpression_<Number_, OPMultD_>(a, 3.0)`), which avoids storing the scalar as a full tape argument.

## The Tape

The tape (`dal/math/aad/tape.hpp`) is a `thread_local` singleton accessed via `AAD::Tape()`:

### Legacy Path Tape

```cpp
class Tape_ {
    bool multi_;                                         // multi-output mode flag
    BlockList_<double, ADJ_SIZE>     adjointsMulti_;     // 32K-entry adjoint blocks
    BlockList_<double, DATA_SIZE>    ders_;              // 64K-entry derivative blocks
    BlockList_<double*, DATA_SIZE>   argPtrs_;           // 64K-entry adjoint-pointer blocks
    BlockList_<TapNode_, BLOCK_SIZE> nodes_;             // 16K-entry node blocks
};
```

**BlockList_ Allocator** (`dal/math/aad/blocklist.hpp`):

A `BlockList_<T_, BLOCK_SIZE>` is a `std::list<std::array<T_, BLOCK_SIZE>>` with cursor-based allocation. Key behaviors:

- **Allocation**: `EmplaceBack(args...)` placement-news elements into the current block. When the block fills up, a new block is allocated and linked.
- **No per-element deallocation**: The tape is a linear log — you never free individual nodes. Memory grows monotonically.
- **Mark/Rewind**: `SetMark()` saves the current position; `RewindToMark()` restores it. This resets the logical cursor without touching any memory — O(1).
- **Clear**: drops all blocks and starts over.
- **Propagation iteration**: `Begin()` → `End()` bidirectional iterators traverse the tape. Propagation walks `--it` from end to start.

The block sizes are chosen for cache efficiency:
- `BLOCK_SIZE = 16384` nodes per block (≈512KB at 32 bytes/node)
- `ADJ_SIZE = 32768` adjoints per block
- `DATA_SIZE = 65536` derivatives/pointers per block

### XAD Path Tape

```cpp
class Tape_ {
    using tape_type = xad::adj<double>::tape_type;
    tape_type tape_;
    tape_type::position_type start_;
    tape_type::position_type mark_;
};
```

A thin wrapper — XAD handles all tape management internally. The `start_` and `mark_` positions mirror the native path's mark/rewind semantics.

### RecordNode

`RecordNode<N_>()` (`tape.hpp:51`) allocates a new node and, when `N_ > 0`, allocates space for `N_` derivatives and `N_` adjoint pointers. The `N_` template parameter is a compile-time constant from the expression tree.

## Propagation: Reverse-Mode Chain Rule

After the forward pass records all operations onto the tape, reverse-mode propagation computes adjoints:

### Single-Output Case

`TapNode_::PropagateOne()` (`node.hpp:43`):

```cpp
void PropagateOne() {
    if (!n_ || std::abs(adjoint_) <= EPSILON)
        return;
    for (size_t i = 0; i < n_; ++i)
        *(pAdjPtrs_[i]) += adjoint_ * pDerivatives_[i];
}
```

Each node multiplies its incoming adjoint by the local partial derivatives and accumulates into its arguments' adjoint storage. This propagates the chain rule backward through the computation graph.

### Multi-Output Case

`TapNode_::PropagateAll()` (`node.hpp:51`) handles vector-valued functions using the multi-adjoint arrays:

```cpp
void PropagateAll() {
    if (!n_ || std::all_of(pAdjoints_, pAdjoints_ + numAdj_, [](double x) { ... }))
        return;
    for (size_t i = 0; i < n_; ++i) {
        double* adjPtr = pAdjPtrs_[i];
        double ders = pDerivatives_[i];
        for (size_t j = 0; j < numAdj_; ++j)
            adjPtr[j] += ders * pAdjoints_[j];
    }
}
```

Multi-output mode is activated via `SetNumResultsForAAD(multi, num_results)` (`aad.hpp:28`). When `multi_` is true, `RecordNode` allocates `numAdj_` adjoints per node instead of one.

### Propagation Functions

| Function                     | Propagates from | To            |
|------------------------------|-----------------|---------------|
| `PropagateToStart(tape)`     | End of tape     | Start         |
| `PropagateToMark(tape)`      | End of tape     | Mark position |
| `PropagateMarkToStart(tape)` | Mark position   | Start         |

Implementation (`tape.cpp:24`):
```cpp
void PropagateAdjoints(Iterator_ from, Iterator_ to) {
    auto it = from;
    while (it != to) {
        it->PropagateOne();
        --it;
    }
    it->PropagateOne();  // last node at 'to'
}
```

### Usage Pattern

```cpp
auto* tape = AAD::Tape();
Clear(*tape);

// Seed inputs
AAD::Number_ x0 = 1.0, x1 = 2.0, x2 = 3.0;
PutOnTape(x0); PutOnTape(x1); PutOnTape(x2);

// Forward pass — records operations
AAD::Number_ y = x0 * x1 + log(x2);

// Seed output adjoint
Adjoint(y) = 1.0;

// Reverse pass
PropagateToStart(*tape);

// Read gradients
double dx0 = Adjoint(x0);  // ∂y/∂x₀
double dx1 = Adjoint(x1);  // ∂y/∂x₁
double dx2 = Adjoint(x2);  // ∂y/∂x₂
```

## Dual-Path Architecture

The `#ifndef DAL_USE_XAD_AAD` / `#else` / `#endif` pattern in `expr.hpp` and `tape.hpp` provides two complete AAD backends behind identical free-function APIs:

| Operation                     | Native Path                                            | XAD Path                                   |
|-------------------------------|--------------------------------------------------------|--------------------------------------------|
| `Number_`                     | Custom expression-template type                        | `xad::adj<double>::active_type`            |
| `Value(n)`                    | Returns `n.value_`                                     | `xad::value(n)`                            |
| `Adjoint(n)`                  | Returns `n.node_->Adjoint()`                           | `xad::derivative(n)`                       |
| `PutOnTape(n)`                | Creates zero-arg node                                  | `tape_.registerInput(n)`                   |
| `Tape()`                      | Returns `thread_local Tape_` with `BlockList_` storage | Returns `thread_local Tape_` with XAD tape |
| `PropagateToStart(t)`         | Walks native tape backward                             | `tape_.computeAdjointsTo(start_)`          |
| `Mark(t)` / `RewindToMark(t)` | Saves/restores `BlockList_` cursor positions           | Saves/restores tape positions              |
| Operator overloads            | Return `BinaryExpression_`/`UnaryExpression_`          | Imported via `using xad::operator*` etc.   |

The native path records operations eagerly during the forward pass — each expression assignment creates a tape node. The XAD path delegates recording to the XAD library, which may use a similar or different internal representation.

### Build Configuration

`DAL_USE_XAD_AAD` defaults to `off` in both top-level `CMakeLists.txt` and the shipped CMake presets. To enable:

```bash
cmake --preset=Release-linux -DDAL_USE_XAD_AAD=on ..
```

The XAD headers are expected at `externals/xad/`. Both paths are covered by the same test infrastructure.

## Parallel AAD in Monte Carlo

The library supports parallel AAD where each thread independently records and processes against its own thread-local tape:

1. **One-time setup** — model parameters and const variables are placed on tape and marked
2. **Per-path loop** — rewinds to mark, generates path with `AAD::Number_` active values, evaluates payoff, sets result adjoint to 1.0, propagates to mark (adjoints accumulate across paths)
3. **Finalization** — propagates from mark to start, reads adjoints from model parameters, divides by path count

This pathwise-adjoint pattern gives full portfolio Greeks in a single parallel MC run. The thread-local tape eliminates all synchronization overhead during both the forward pass and the per-path propagation.

## Integration with Script Engine

The script engine's `Evaluator_<T_>` and `MCSimulation<T_>` are templated on the value type. Instantiated with `AAD::Number_`:

- Script expressions compile to operations on `AAD::Number_`, recording the computation graph automatically
- `MCSimulation<AAD::Number_>` sets up the parallel AAD described above
- Discontinuous payoffs (digital options) are smoothed via fuzzy evaluation in the domain processor

## See Also

- `yield_curve.md` for the curve construction framework that uses AAD for risk
- `underdetermined_search.md` for the solver used in curve calibration
- `tests/math/aad/` for direct AAD tests
- `examples/aad/` for standalone AAD examples
