#pragma once

#include <array>

#include <dal/platform/host.hpp>

#include <dal/math/aad/detail/secondordertape.hpp>

namespace Dal::AAD::SecondOrderDetail {
    template <class E_> constexpr bool IsExpression = std::is_base_of_v<Expression_, std::decay_t<E_>>;
    template <class E_> constexpr bool IsOperand = IsExpression<E_> || std::is_arithmetic_v<std::decay_t<E_>>;
    template <class A_, class B_>
    using EnableBinary_ = std::enable_if_t<IsOperand<A_> && IsOperand<B_> && (IsExpression<A_> || IsExpression<B_>), int>;

    struct Constant_ {
        double value_;
        static constexpr size_t leaves_ = 0;
    };

    template <class E_> auto Operand(const E_& expression) {
        if constexpr (IsExpression<E_>)
            return expression;
        else
            return Constant_{static_cast<double>(expression)};
    }

    template <class E_> constexpr size_t Leaves() {
        if constexpr (std::is_same_v<E_, SecondOrderNumber_>)
            return 1;
        else
            return E_::leaves_;
    }

    template <class Op_, class A_> struct Unary_ : Expression_ {
        Op_ op_;
        A_ input_;
        static constexpr size_t leaves_ = Leaves<A_>();
    };

    template <class Op_, class A_, class B_> struct Binary_ : Expression_ {
        A_ left_;
        B_ right_;
        static constexpr size_t leaves_ = Leaves<A_>() + Leaves<B_>();
    };

    template <class Op_, class A_> auto Unary(Op_ op, const A_& input) { return Unary_<Op_, A_>{{}, op, input}; }

    template <class Op_, class A_, class B_> auto Binary(const A_& a, const B_& b) {
        return Binary_<Op_, decltype(Operand(a)), decltype(Operand(b))>{{}, Operand(a), Operand(b)};
    }

    template <class T_> struct Dual_ {
        double value_;
        T_ tangent_;
    };

    template <class T_> struct UnaryValue_ {
        Dual_<T_> result_;
        Weight_<T_> weight_;
    };

    template <class T_> struct BinaryValue_ {
        Dual_<T_> result_;
        Weight_<T_> left_, right_;
    };

    template <class T_> UnaryValue_<T_> Local(double value, T_ weight, T_ weightTangent, T_ seed) {
        return {{value, Mul(weight, seed)}, {Guard(weight), Guard(weightTangent)}};
    }

    template <class T_> T_ Quotient(T_ a, T_ b) { return a / b; }

    template <class T_> T_ Exponential(double x) {
        if constexpr (std::is_same_v<T_, double>) {
            const double value = std::exp(x);
            if (value == 0.0)
                Promote("exponential underflow");
            return Guard(value);
        } else {
            return Wide_(std::exp(static_cast<long double>(x)));
        }
    }

    template <class T_> T_ Power(double x, double exponent) {
        if constexpr (std::is_same_v<T_, double>) {
            const double value = std::pow(x, exponent);
            if (x != 0.0 && value == 0.0)
                Promote("power underflow");
            return Guard(value);
        } else {
            return Wide_(std::pow(static_cast<long double>(x), static_cast<long double>(exponent)));
        }
    }

    struct Plus_ {
        template <bool ActiveLeft_, bool ActiveRight_, class T_> static BinaryValue_<T_> Apply(Dual_<T_> a, Dual_<T_> b) {
            const auto tangent = ActiveLeft_ && ActiveRight_ ? Add(a.tangent_, b.tangent_) : ActiveLeft_ ? a.tangent_ : b.tangent_;
            return {{a.value_ + b.value_, tangent}, {T_(1), T_(0)}, {T_(1), T_(0)}};
        }
    };
    struct Minus_ {
        template <bool ActiveLeft_, bool ActiveRight_, class T_> static BinaryValue_<T_> Apply(Dual_<T_> a, Dual_<T_> b) {
            const auto tangent = ActiveLeft_ && ActiveRight_ ? Add(a.tangent_, -b.tangent_) : ActiveLeft_ ? a.tangent_ : -b.tangent_;
            return {{a.value_ - b.value_, tangent}, {T_(1), T_(0)}, {T_(-1), T_(0)}};
        }
    };
    struct Times_ {
        template <bool ActiveLeft_, bool ActiveRight_, class T_> static BinaryValue_<T_> Apply(Dual_<T_> a, Dual_<T_> b) {
            const T_ av(a.value_), bv(b.value_);
            const auto tangent = ActiveLeft_ && ActiveRight_ ? Add(Mul(av, b.tangent_), Mul(bv, a.tangent_))
                                 : ActiveLeft_               ? Mul(bv, a.tangent_)
                                                             : Mul(av, b.tangent_);
            return {{a.value_ * b.value_, tangent}, {bv, b.tangent_}, {av, a.tangent_}};
        }
    };
    struct Divide_ {
        template <bool ActiveLeft_, bool ActiveRight_, class T_> static BinaryValue_<T_> Apply(Dual_<T_> a, Dual_<T_> b) {
            if (b.value_ == 0.0)
                Error(Reason::OUTSIDE_DOMAIN, "division requires a nonzero denominator");
            const double value = a.value_ / b.value_;
            const T_ inv = Quotient(T_(1), T_(b.value_));
            if constexpr (!ActiveRight_)
                return {{value, Mul(inv, a.tangent_)}, {inv, T_(0)}, {T_(0), T_(0)}};
            const T_ w = -Mul(Quotient(T_(a.value_), T_(b.value_)), inv);
            const T_ invTangent = ActiveLeft_ ? -Mul(inv, Mul(inv, b.tangent_)) : T_(0);
            const T_ tangent = ActiveLeft_ ? Add(Mul(inv, a.tangent_), Mul(w, b.tangent_)) : Mul(w, b.tangent_);
            return {{value, tangent}, {inv, invTangent}, {w, -Add(Mul(inv, tangent), Mul(w, Mul(inv, b.tangent_)))}};
        }
    };
    struct Negate_ {
        template <class T_> UnaryValue_<T_> Apply(Dual_<T_> a) const { return {{-a.value_, -a.tangent_}, {T_(-1), T_(0)}}; }
    };
    struct Exp_ {
        template <class T_> UnaryValue_<T_> Apply(Dual_<T_> a) const {
            const auto w = Exponential<T_>(a.value_);
            if constexpr (std::is_same_v<T_, double>)
                return Local(w, w, Mul(w, a.tangent_), a.tangent_);
            else
                return Local(std::exp(a.value_), w, Mul(w, a.tangent_), a.tangent_);
        }
    };
    struct Log_ {
        template <class T_> UnaryValue_<T_> Apply(Dual_<T_> a) const {
            if (!(a.value_ > 0.0))
                Error(Reason::OUTSIDE_DOMAIN, "log requires a positive argument");
            const auto w = Quotient(T_(1), T_(a.value_));
            return Local(std::log(a.value_), w, -Mul(w, Mul(w, a.tangent_)), a.tangent_);
        }
    };
    struct Sqrt_ {
        template <class T_> UnaryValue_<T_> Apply(Dual_<T_> a) const {
            if (!(a.value_ > 0.0))
                Error(Reason::OUTSIDE_DOMAIN, "sqrt requires a positive argument");
            const double value = std::sqrt(a.value_);
            const auto w = Quotient(T_(0.5), T_(value));
            return Local(value, w, -Mul(w, Quotient(Mul(T_(0.5), a.tangent_), T_(a.value_))), a.tangent_);
        }
    };
    struct FixedPower_ {
        double exponent_;

        template <class T_> UnaryValue_<T_> Apply(Dual_<T_> a) const {
            const double p = exponent_;
            if (p == 0.0)
                return {{1.0, T_(0)}, {T_(0), T_(0)}};
            if (p == 1.0)
                return {a, {T_(1), T_(0)}};
            if (!std::isfinite(p) || (a.value_ < 0.0 && p != std::trunc(p)) || (a.value_ == 0.0 && (p < 0.0 || p != std::trunc(p))))
                Error(Reason::OUTSIDE_DOMAIN, "pow requires positive base or an admissible integer exponent");
            if (a.value_ == 0.0)
                return Local(0.0, T_(0), p == 2.0 ? Mul(T_(2), a.tangent_) : T_(0), a.tangent_);
            const T_ w = Mul(T_(p), Power<T_>(a.value_, p - 1.0));
            const T_ dot = Zero(a.tangent_) ? T_(0) : Mul(Mul(T_(p - 1.0), Quotient(w, T_(a.value_))), a.tangent_);
            return Local(std::pow(a.value_, p), w, dot, a.tangent_);
        }
    };
    struct VariablePower_ {
        template <bool ActiveLeft_, bool ActiveRight_, class T_> static BinaryValue_<T_> Apply(Dual_<T_> a, Dual_<T_> b) {
            if (!(a.value_ > 0.0))
                Error(Reason::OUTSIDE_DOMAIN, "pow with an active exponent requires a positive base");
            const double value = std::pow(a.value_, b.value_);
            const T_ av(a.value_), bv(b.value_), y = Power<T_>(a.value_, b.value_), logA(std::log(a.value_));
            const T_ wa = Mul(bv, Quotient(y, av)), wb = Mul(y, logA);
            const T_ ratio = Quotient(a.tangent_, av);
            const T_ tangent = Add(Mul(wa, a.tangent_), Mul(wb, b.tangent_));
            const T_ dotA = Add(Quotient(Add(Mul(b.tangent_, y), Mul(bv, tangent)), av), -Mul(wa, ratio));
            return {{value, tangent}, {wa, dotA}, {wb, Add(Mul(tangent, logA), Mul(y, ratio))}};
        }
    };
    struct Erfc_ {
        template <class T_> UnaryValue_<T_> Apply(Dual_<T_> a) const {
            const double z = a.value_;
            const T_ w = Mul(T_(-1.12837916709551257389615890312154517L), Exponential<T_>(-z * z));
            return Local(std::erfc(z), w, Mul(Mul(T_(-2.0 * z), w), a.tangent_), a.tangent_);
        }
    };
    struct NormalCdf_ {
        template <class T_> UnaryValue_<T_> Apply(Dual_<T_> a) const {
            const double z = a.value_;
            const T_ w = Mul(T_(0.39894228040143267793994605993438187L), Exponential<T_>(-0.5 * z * z));
            return Local(0.5 * std::erfc(-0.7071067811865475244 * z), w, Mul(Mul(T_(-z), w), a.tangent_), a.tangent_);
        }
    };
    struct NormalPdf_ {
        template <class T_> UnaryValue_<T_> Apply(Dual_<T_> a) const {
            constexpr double INV_ROOT_2PI = 0.39894228040143267794;
            const double z = a.value_;
            const T_ p = Mul(T_(0.39894228040143267793994605993438187L), Exponential<T_>(-0.5 * z * z));
            const T_ w = Mul(T_(-z), p);
            const T_ dot = Mul(Mul(T_(z * z - 1.0), p), a.tangent_);
            return Local(INV_ROOT_2PI * std::exp(-0.5 * z * z), w, dot, a.tangent_);
        }
    };

#define DAL_SECOND_ORDER_BINARY(symbol, operation)                                                                                                   \
    template <class A_, class B_, EnableBinary_<A_, B_> = 0> auto operator symbol(const A_& a, const B_& b) { return Binary<operation>(a, b); }
    DAL_SECOND_ORDER_BINARY(+, Plus_)
    DAL_SECOND_ORDER_BINARY(-, Minus_)
    DAL_SECOND_ORDER_BINARY(*, Times_)
    DAL_SECOND_ORDER_BINARY(/, Divide_)
#undef DAL_SECOND_ORDER_BINARY

    template <class A_, EnableExpression_<A_> = 0> auto operator-(const A_& a) { return Unary(Negate_{}, a); }
    template <class A_, EnableExpression_<A_> = 0> auto operator+(const A_& a) { return a; }
    template <class A_, EnableExpression_<A_> = 0> auto exp(const A_& a) { return Unary(Exp_{}, a); }
    template <class A_, EnableExpression_<A_> = 0> auto log(const A_& a) { return Unary(Log_{}, a); }
    template <class A_, EnableExpression_<A_> = 0> auto sqrt(const A_& a) { return Unary(Sqrt_{}, a); }
    template <class A_, EnableExpression_<A_> = 0> auto erfc(const A_& a) { return Unary(Erfc_{}, a); }
    template <class A_, EnableExpression_<A_> = 0> auto NCDF(const A_& a) { return Unary(NormalCdf_{}, a); }
    template <class A_, EnableExpression_<A_> = 0> auto NPDF(const A_& a) { return Unary(NormalPdf_{}, a); }
    template <class A_, EnableExpression_<A_> = 0> auto pow(const A_& a, double b) { return Unary(FixedPower_{b}, a); }
    template <class A_, class B_, std::enable_if_t<IsOperand<A_> && IsExpression<B_>, int> = 0> auto pow(const A_& a, const B_& b) {
        return Binary<VariablePower_>(a, b);
    }

    template <class T_> struct EdgeWriter_ {
        Slot_* rhs_;
        Weight_<T_>* weights_;
        Slot_ size_ = 0;

        void AddEdge(Slot_ slot, Weight_<T_> weight) {
            if (slot != PASSIVE) {
                rhs_[size_] = slot;
                weights_[size_] = weight;
                ++size_;
            }
        }
    };

    template <class T_, class Edges_> struct Prepared_ {
        Dual_<T_> dual_;
        Edges_ edges_;
        static constexpr bool active_ = Edges_::active_;
    };

    template <class T_, bool Active_> struct LeafEdges_ {
        Slot_ slot_;
        static constexpr bool active_ = Active_;
        static constexpr size_t preparationBytes_ = sizeof(Prepared_<T_, LeafEdges_>);
        template <bool Root_> void Push(Weight_<T_> weight, EdgeWriter_<T_>& scratch) const { scratch.AddEdge(slot_, weight); }
    };

    template <class T_, class A_> struct UnaryEdges_ {
        Weight_<T_> weight_;
        A_ input_;
        static constexpr bool active_ = A_::active_;
        static constexpr size_t preparationBytes_ = sizeof(Dual_<T_>) + sizeof(Weight_<T_>) + A_::preparationBytes_;

        template <bool Root_> void Push(Weight_<T_> weight, EdgeWriter_<T_>& scratch) const {
            input_.template Push<false>(Root_ ? weight_ : Compose(weight, weight_), scratch);
        }
    };

    template <class T_, class A_, class B_> struct BinaryEdges_ {
        Weight_<T_> leftWeight_, rightWeight_;
        A_ left_;
        B_ right_;
        static constexpr bool active_ = A_::active_ || B_::active_;
        static constexpr size_t preparationBytes_ = sizeof(Dual_<T_>) + 2 * sizeof(Weight_<T_>) + A_::preparationBytes_ + B_::preparationBytes_;

        template <bool Root_> void Push(Weight_<T_> weight, EdgeWriter_<T_>& scratch) const {
            if constexpr (A_::active_)
                left_.template Push<false>(Root_ ? leftWeight_ : Compose(weight, leftWeight_), scratch);
            if constexpr (B_::active_)
                right_.template Push<false>(Root_ ? rightWeight_ : Compose(weight, rightWeight_), scratch);
        }
    };

    template <class T_, class E_> struct EdgesFor_;
    template <class T_> struct EdgesFor_<T_, SecondOrderNumber_> {
        using Type_ = LeafEdges_<T_, true>;
    };
    template <class T_> struct EdgesFor_<T_, Constant_> {
        using Type_ = LeafEdges_<T_, false>;
    };
    template <class T_, class E_> using ExpressionEdges_ = typename EdgesFor_<T_, E_>::Type_;

    template <class T_, class Op_, class A_> struct EdgesFor_<T_, Unary_<Op_, A_>> {
        using Type_ = UnaryEdges_<T_, ExpressionEdges_<T_, A_>>;
    };
    template <class T_, class Op_, class A_, class B_> struct EdgesFor_<T_, Binary_<Op_, A_, B_>> {
        using Type_ = BinaryEdges_<T_, ExpressionEdges_<T_, A_>, ExpressionEdges_<T_, B_>>;
    };

    inline double FinitePrimal(double value) {
        if (!std::isfinite(value))
            Error(Reason::NUMERICAL_FAILURE, "nonfinite primal value");
        return value;
    }

    template <class T_> FORCE_INLINE void CheckOperand(double v) {
        FinitePrimal(v);
        if constexpr (std::is_same_v<T_, double>)
            (void)Guard(v);
    }

    template <class T_> FORCE_INLINE auto Prepare(const SecondOrderNumber_& value, Arena_& arena) -> Prepared_<T_, LeafEdges_<T_, true>> {
        const auto slot = Access_::Slot(value, arena);
        const auto primal = Access_::Value(value);
        CheckOperand<T_>(primal);
        return {{primal, Access_::Tangent<T_>(value, arena, slot)}, {slot}};
    }

    template <class T_> FORCE_INLINE auto Prepare(const Constant_& value, Arena_&) -> Prepared_<T_, LeafEdges_<T_, false>> {
        CheckOperand<T_>(value.value_);
        return {{value.value_, T_(0)}, {PASSIVE}};
    }

    template <class T_, class E_, bool Active_> FORCE_INLINE Dual_<T_> Prepare(const E_& expression, Arena_& arena, LeafEdges_<T_, Active_>& edges) {
        const auto leaf = Prepare<T_>(expression, arena);
        edges = leaf.edges_;
        return leaf.dual_;
    }

    template <class T_, class Op_, class A_, class B_, class L_, class R_>
    Dual_<T_> Prepare(const Binary_<Op_, A_, B_>& expression, Arena_& arena, BinaryEdges_<T_, L_, R_>& edges);

    template <class T_, class Op_, class A_, class E_>
    Dual_<T_> Prepare(const Unary_<Op_, A_>& expression, Arena_& arena, UnaryEdges_<T_, E_>& edges) {
        const auto operand = Prepare<T_>(expression.input_, arena, edges.input_);
        const auto local = expression.op_.Apply(operand);
        if constexpr (std::is_same_v<T_, Wide_>)
            FinitePrimal(local.result_.value_);
        edges.weight_ = local.weight_;
        return local.result_;
    }

    template <class T_, class Op_, class A_, class B_, class L_, class R_>
    Dual_<T_> Prepare(const Binary_<Op_, A_, B_>& expression, Arena_& arena, BinaryEdges_<T_, L_, R_>& edges) {
        const auto left = Prepare<T_>(expression.left_, arena, edges.left_);
        const auto right = Prepare<T_>(expression.right_, arena, edges.right_);
        const auto local = Op_::template Apply<L_::active_, R_::active_>(left, right);
        if constexpr (std::is_same_v<T_, Wide_>)
            FinitePrimal(local.result_.value_);
        edges.leftWeight_ = local.left_;
        edges.rightWeight_ = local.right_;
        return local.result_;
    }

    template <class T_, size_t N_, class Emit_>
    SecondOrderNumber_ Append(Arena_& arena, const Dual_<T_>& result, size_t temporary, const Emit_& emit) {
        static_assert(N_ < PASSIVE, "second-order expression exceeds the edge index range");
        constexpr auto n = static_cast<Slot_>(N_);
        if (arena.slots_ >= PASSIVE - 1 || arena.edges_ >= PASSIVE - n)
            Error(Reason::CAPACITY_EXCEEDED, "second-order slot or edge limit exceeded");
        auto& weights = arena.template Weights<T_>();
        const auto edgeEnd = arena.edges_ + n;
        if ((arena.statements_ >= arena.statement_.capacity_) | (edgeEnd > arena.edgeCapacity_)) {
            const Scratch_ duringGrowth(arena, temporary);
            arena.template GrowStatement<T_>(edgeEnd);
        }
        EdgeWriter_<T_> edges{arena.rhs_.Data() + arena.edges_, weights.Data() + arena.edges_};
        emit(edges);
        if (edges.size_ == 0)
            return SecondOrderNumber_(result.value_);
        const auto slot = arena.slots_;
        double tangent = 0.0;
        if constexpr (std::is_same_v<T_, double>) {
            tangent = result.tangent_;
        } else {
            const Scratch_ duringGrowth(arena, temporary);
            arena.Ensure(arena.wideTangent_, slot, slot + 1);
            arena.wideTangent_.Data()[slot] = result.tangent_;
        }
        arena.edges_ += edges.size_;
        arena.statement_.Data()[arena.statements_++] = {arena.edges_, slot};
        ++arena.slots_;
        return Access_::Bind(arena, slot, result.value_, tangent);
    }

    template <class T_, class E_> SecondOrderNumber_ Materialize(Arena_& arena, const E_& expression) {
        using Edges = ExpressionEdges_<T_, E_>;
        // Keep the preparation charge for each subtree's dual and weights.
        constexpr size_t temporary = sizeof(EdgeWriter_<T_>) + Edges::preparationBytes_;
        arena.AccountTemporary(temporary);
        Edges recipe;
        const auto value = Prepare<T_>(expression, arena, recipe);
        auto recorded = Append<T_, Leaves<E_>()>(arena, value, temporary, [&](auto& edges) { recipe.template Push<true>({T_(1), T_(0)}, edges); });
        arena.AccountLogical<T_>(false, temporary);
        return recorded;
    }

    template <class E_> constexpr bool IsLeaf = std::is_same_v<E_, SecondOrderNumber_> || std::is_same_v<E_, Constant_>;

    // A statement holds leaf slots and local factors in two contiguous buffers.
    // Preparation fills factors in place, with compile-time offsets for each
    // subtree. Traversal still composes root-to-leaf only after tape admission.
    struct CompactView_ {
        Slot_* slots_;
        Weight_<double>* factors_;
    };

    template <class E_> struct CompactEvaluation_;

    template <> struct CompactEvaluation_<SecondOrderNumber_> {
        static constexpr size_t factors_ = 0;
        static FORCE_INLINE Dual_<double> Evaluate(const SecondOrderNumber_& number, Arena_& arena, CompactView_ view) {
            const auto leaf = Prepare<double>(number, arena);
            view.slots_[0] = leaf.edges_.slot_;
            return leaf.dual_;
        }
        template <bool Root_> static FORCE_INLINE void Write(CompactView_ view, Weight_<double> weight, EdgeWriter_<double>& destination) {
            destination.AddEdge(view.slots_[0], weight);
        }
    };

    template <> struct CompactEvaluation_<Constant_> {
        static constexpr size_t factors_ = 0;
        static FORCE_INLINE Dual_<double> Evaluate(const Constant_& constant, Arena_& arena, CompactView_) {
            return Prepare<double>(constant, arena).dual_;
        }
    };

    template <class Op_, class A_> struct CompactEvaluation_<Unary_<Op_, A_>> {
        using Child_ = CompactEvaluation_<A_>;
        static constexpr size_t factors_ = 1 + Child_::factors_;

        static FORCE_INLINE Dual_<double> Evaluate(const Unary_<Op_, A_>& expression, Arena_& arena, CompactView_ view) {
            const auto operand = Child_::Evaluate(expression.input_, arena, {view.slots_, view.factors_ + 1});
            const auto local = expression.op_.Apply(operand);
            view.factors_[0] = local.weight_;
            return local.result_;
        }

        template <bool Root_> static FORCE_INLINE void Write(CompactView_ view, Weight_<double> weight, EdgeWriter_<double>& destination) {
            const auto local = view.factors_[0];
            Child_::template Write<false>({view.slots_, view.factors_ + 1}, Root_ ? local : Compose(weight, local), destination);
        }
    };

    template <class Op_, class A_, class B_> struct CompactEvaluation_<Binary_<Op_, A_, B_>> {
        using Left_ = CompactEvaluation_<A_>;
        using Right_ = CompactEvaluation_<B_>;
        static constexpr bool LEFT = Leaves<A_>() != 0, RIGHT = Leaves<B_>() != 0;
        static constexpr size_t LOCAL = LEFT + RIGHT, factors_ = LOCAL + Left_::factors_ + Right_::factors_;

        static FORCE_INLINE Dual_<double> Evaluate(const Binary_<Op_, A_, B_>& expression, Arena_& arena, CompactView_ view) {
            const auto left = Left_::Evaluate(expression.left_, arena, {view.slots_, view.factors_ + LOCAL});
            const auto right = Right_::Evaluate(expression.right_, arena, {view.slots_ + Leaves<A_>(), view.factors_ + LOCAL + Left_::factors_});
            const auto local = Op_::template Apply<LEFT, RIGHT>(left, right);
            if constexpr (LEFT)
                view.factors_[0] = local.left_;
            if constexpr (RIGHT)
                view.factors_[LEFT] = local.right_;
            return local.result_;
        }

        template <bool Root_> static FORCE_INLINE void Write(CompactView_ view, Weight_<double> weight, EdgeWriter_<double>& destination) {
            if constexpr (LEFT) {
                const auto local = view.factors_[0];
                Left_::template Write<false>({view.slots_, view.factors_ + LOCAL}, Root_ ? local : Compose(weight, local), destination);
            }
            if constexpr (RIGHT) {
                const auto local = view.factors_[LEFT];
                Right_::template Write<false>({view.slots_ + Leaves<A_>(), view.factors_ + LOCAL + Left_::factors_},
                                              Root_ ? local : Compose(weight, local), destination);
            }
        }
    };

    template <class E_> SecondOrderNumber_ MaterializeNarrow(Arena_& arena, const E_& expression) {
        using Evaluator_ = CompactEvaluation_<E_>;
        constexpr size_t preparation = ExpressionEdges_<double, E_>::preparationBytes_;
        constexpr size_t temporary = sizeof(EdgeWriter_<double>) + preparation;
        arena.AccountTemporary(temporary);
        std::array<Slot_, Leaves<E_>()> slots;
        std::array<Weight_<double>, Evaluator_::factors_> factors;
        static_assert(sizeof(slots) + sizeof(factors) <= preparation, "compact statement must fit its preparation charge");
        const CompactView_ view{slots.data(), factors.data()};
        const auto value = Evaluator_::Evaluate(expression, arena, view);
        auto result =
            Append<double, Leaves<E_>()>(arena, value, temporary, [&](auto& edges) { Evaluator_::template Write<true>(view, {1.0, 0.0}, edges); });
        arena.AccountLogical<double>(false, temporary);
        return result;
    }

    template <class Op_, class A_, class B_, std::enable_if_t<IsLeaf<A_> && IsLeaf<B_>, int> = 0>
    FORCE_INLINE SecondOrderNumber_ MaterializeNarrow(Arena_& arena, const Binary_<Op_, A_, B_>& expression) {
        using Left = decltype(Prepare<double>(expression.left_, arena));
        using Right = decltype(Prepare<double>(expression.right_, arena));
        constexpr size_t temporary = sizeof(EdgeWriter_<double>) + sizeof(BinaryValue_<double>) + sizeof(Left) + sizeof(Right);
        arena.AccountTemporary(temporary);
        const auto left = Prepare<double>(expression.left_, arena);
        const auto right = Prepare<double>(expression.right_, arena);
        const auto local = Op_::template Apply<Left::active_, Right::active_>(left.dual_, right.dual_);
        const auto result = Append<double, Leaves<Binary_<Op_, A_, B_>>()>(arena, local.result_, temporary, [&](auto& edges) {
            if constexpr (Left::active_)
                edges.AddEdge(left.edges_.slot_, local.left_);
            if constexpr (Right::active_)
                edges.AddEdge(right.edges_.slot_, local.right_);
        });
        arena.AccountLogical<double>(false, temporary);
        return result;
    }

    template <class E_>
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline, cold))
#endif
    SecondOrderNumber_ MaterializeProtected(Arena_& arena, const E_& expression) {
        return Materialize<Wide_>(arena, expression);
    }

    template <class E_> SecondOrderNumber_ Materialize(const E_& expression) {
        auto* const arena = activeArena;
        if (!arena || !arena->liveDirection_)
            Error(Reason::LIFECYCLE, "second-order expression requires the current recording");
        if (arena->recordingFailure_)
            std::rethrow_exception(arena->recordingFailure_);
        if (arena->retry_)
            Promote();
        try {
            if (arena->wide_)
                return MaterializeProtected(*arena, expression);
            return MaterializeNarrow(*arena, expression);
        } catch (const Retry_&) {
            throw;
        } catch (...) {
            arena->RecordFailure();
        }
    }
} // namespace Dal::AAD::SecondOrderDetail

namespace Dal::AAD {
    using SecondOrderDetail::erfc;
    using SecondOrderDetail::exp;
    using SecondOrderDetail::log;
    using SecondOrderDetail::NCDF;
    using SecondOrderDetail::NPDF;
    using SecondOrderDetail::pow;
    using SecondOrderDetail::sqrt;

    template <class E_, SecondOrderDetail::EnableExpression_<E_>>
    SecondOrderNumber_::SecondOrderNumber_(const E_& expression) : SecondOrderNumber_(SecondOrderDetail::Materialize(expression)) {}

    template <class E_> SecondOrderNumber_& SecondOrderNumber_::operator+=(const E_& rhs) { return *this = SecondOrderNumber_(*this + rhs); }
    template <class E_> SecondOrderNumber_& SecondOrderNumber_::operator-=(const E_& rhs) { return *this = SecondOrderNumber_(*this - rhs); }
    template <class E_> SecondOrderNumber_& SecondOrderNumber_::operator*=(const E_& rhs) { return *this = SecondOrderNumber_(*this * rhs); }
    template <class E_> SecondOrderNumber_& SecondOrderNumber_::operator/=(const E_& rhs) { return *this = SecondOrderNumber_(*this / rhs); }
} // namespace Dal::AAD
