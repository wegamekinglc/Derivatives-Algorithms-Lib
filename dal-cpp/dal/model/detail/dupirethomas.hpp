#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include <dal/math/aad/secondorder.hpp>

namespace Dal::DupireSecondOrderDetail::Thomas {
    namespace Tape = AAD::SecondOrderDetail;
    using Pair_ = Tape::Weight_<double>;

    inline double Accumulate(double accumulated, double term) {
        const double sum = accumulated + term;
        const double scale = std::max(std::abs(accumulated), std::abs(term));
        if (!(scale <= 0x1p48) || (sum != 0.0 && std::abs(sum) < 0x1p-900))
            Tape::Promote("Dupire Thomas derivative range");
        if (accumulated != 0.0 && term != 0.0 &&
            (std::abs(sum) <= 0x1p-38 * scale || ((sum == accumulated || sum == term) && std::min(std::abs(accumulated), std::abs(term)) > 0x1p-48)))
            Tape::Promote("Dupire Thomas derivative cancellation");
        return sum;
    }

    inline double SubtractDerivativeProduct(double rhs, double coefficient, double solved, const char* protection) {
        const double term = coefficient * solved;
        const double difference = rhs - term;
        // A product rounded to rhs can leave a real residual even when the
        // binary64 difference is zero. Replay from the inputs before a nearly
        // cancelled difference feeds another row. With a normal product within
        // a factor of two of rhs, the arithmetic outside this guard preserves
        // the product to within 2^-25 of the difference, at any admitted scale.
        if (std::abs(difference) < 0x1p-27 * std::abs(rhs))
            Tape::Promote(protection);
        return difference;
    }

    template <size_t N_> using Row_ = std::array<double, N_>;
    template <size_t N_> using Band_ = std::array<Row_<N_>, 3>;
    template <size_t N_, size_t S_> using History_ = std::array<Row_<N_>, S_>;

    template <size_t N_> struct Factor_ {
        Band_<N_> values_, dots_;
        std::array<std::array<Tape::Slot_, N_>, 3> slots_;
        std::array<std::array<Pair_, N_>, 3> seeds_;
        Row_<N_> inverse_, scaledUpper_, transposedLower_;

        void Set(const Tape::Arena_& arena, size_t row, const std::array<AAD::SecondOrderNumber_, 3>& active) {
            for (size_t band = 0; band < active.size(); ++band) {
                const auto slot = Tape::Access_::Slot(active[band], arena);
                slots_[band][row] = slot;
                values_[band][row] = Tape::Access_::Value(active[band]);
                dots_[band][row] = Tape::Guard(Tape::Access_::Tangent<double>(active[band], arena, slot));
                seeds_[band][row] = {0.0, 0.0};
            }
            auto pivot = values_[1][row];
            if (row != 0) {
                pivot -= values_[0][row] * scaledUpper_[row - 1];
                transposedLower_[row - 1] = values_[0][row] * inverse_[row - 1];
            }
            if (!(pivot > 0.0) || !std::isfinite(pivot))
                Tape::Promote("Dupire Thomas pivot outside certified range");
            inverse_[row] = 1.0 / pivot;
            if (row + 1 != N_)
                scaledUpper_[row] = values_[2][row] * inverse_[row];
        }

        template <bool Derivative_ = false> void Forward(Row_<N_>* rhs) const {
            for (size_t i = 0; i < N_; ++i) {
                if (i != 0) {
                    if constexpr (Derivative_)
                        (*rhs)[i] = SubtractDerivativeProduct((*rhs)[i], values_[0][i], (*rhs)[i - 1], "Dupire Thomas tangent elimination cancellation");
                    else
                        (*rhs)[i] -= values_[0][i] * (*rhs)[i - 1];
                }
                (*rhs)[i] *= inverse_[i];
            }
            for (size_t i = N_ - 1; i-- > 0;) {
                if constexpr (Derivative_)
                    (*rhs)[i] = SubtractDerivativeProduct((*rhs)[i], scaledUpper_[i], (*rhs)[i + 1], "Dupire Thomas tangent back substitution cancellation");
                else
                    (*rhs)[i] -= scaledUpper_[i] * (*rhs)[i + 1];
            }
        }

        void Transpose(Row_<N_>* rhs) const {
            for (size_t i = 0; i < N_; ++i) {
                if (i != 0)
                    (*rhs)[i] = SubtractDerivativeProduct((*rhs)[i], values_[2][i - 1], (*rhs)[i - 1], "Dupire Thomas transpose forward substitution cancellation");
                (*rhs)[i] *= inverse_[i];
            }
            for (size_t i = N_ - 1; i-- > 0;)
                (*rhs)[i] = SubtractDerivativeProduct((*rhs)[i], transposedLower_[i], (*rhs)[i + 1], "Dupire Thomas transpose back substitution cancellation");
        }

        template <bool Transpose_> void CheckResidual(const Row_<N_>& rhs, const Row_<N_>& solution) const {
            for (size_t i = 0; i < N_; ++i) {
                const double diagonal = values_[1][i] * solution[i];
                const double lower = i ? (Transpose_ ? values_[2][i - 1] : values_[0][i]) * solution[i - 1] : 0.0;
                const double upper = i + 1 != N_ ? (Transpose_ ? values_[0][i + 1] : values_[2][i]) * solution[i + 1] : 0.0;
                const double scale = std::abs(rhs[i]) + std::abs(diagonal) + std::abs(lower) + std::abs(upper);
                if (!(std::abs((diagonal + lower + upper) - rhs[i]) <= 0x1p-44 * scale) || !(scale <= 0x1p48))
                    Tape::Promote("Dupire Thomas solve residual or range");
            }
        }

        void ForwardStep(double right, const Row_<N_>& previous, const Row_<N_>& previousDot, Row_<N_>* y, Row_<N_>* dy) const {
            auto rhs = previous;
            rhs.back() -= values_[2].back() * right;
            *y = rhs;
            Forward(y);
            CheckResidual<false>(rhs, *y);

            rhs = previousDot;
            rhs.back() = Accumulate(rhs.back(), -dots_[2].back() * right);
            for (size_t i = 0; i < N_; ++i) {
                auto value = Accumulate(rhs[i], -dots_[1][i] * (*y)[i]);
                if (i != 0)
                    value = Accumulate(value, -dots_[0][i] * (*y)[i - 1]);
                if (i + 1 != N_)
                    value = Accumulate(value, -dots_[2][i] * (*y)[i + 1]);
                rhs[i] = value;
            }
            *dy = rhs;
            Forward<true>(dy);
            CheckResidual<false>(rhs, *dy);
        }

        void Seed(size_t band, size_t row, double outgoing, double outgoingDot, double input, double inputDot) {
            auto& seed = seeds_[band][row];
            seed.value_ = Accumulate(seed.value_, -outgoing * input);
            const auto dot = Accumulate(-outgoingDot * input, -outgoing * inputDot);
            seed.tangent_ = Accumulate(seed.tangent_, dot);
        }

        void ReverseStep(double right, const Row_<N_>& y, const Row_<N_>& dy, Row_<N_>* adjoint, Row_<N_>* adjointDot) {
            auto rhs = *adjoint;
            Transpose(adjoint);
            CheckResidual<true>(rhs, *adjoint);

            rhs = *adjointDot;
            for (size_t i = 0; i < N_; ++i) {
                auto value = Accumulate(rhs[i], -dots_[1][i] * (*adjoint)[i]);
                if (i != 0)
                    value = Accumulate(value, -dots_[2][i - 1] * (*adjoint)[i - 1]);
                if (i + 1 != N_)
                    value = Accumulate(value, -dots_[0][i + 1] * (*adjoint)[i + 1]);
                rhs[i] = value;
            }
            *adjointDot = rhs;
            Transpose(adjointDot);
            CheckResidual<true>(rhs, *adjointDot);

            for (size_t i = 0; i < N_; ++i) {
                const double bar = (*adjoint)[i], dbar = (*adjointDot)[i];
                Seed(1, i, bar, dbar, y[i], dy[i]);
                if (i != 0)
                    Seed(0, i, bar, dbar, y[i - 1], dy[i - 1]);
                if (i + 1 != N_)
                    Seed(2, i, bar, dbar, y[i + 1], dy[i + 1]);
                else
                    Seed(2, i, bar, dbar, right, 0.0);
            }
        }
    };

    template <size_t N_, size_t S_, size_t G_> struct Rollback_ {
        std::array<Factor_<N_>, G_> factors_;
        std::array<size_t, S_> group_;
        History_<N_, S_> values_, tangents_;

        template <class C_>
        AAD::SecondOrderNumber_ Evaluate(Tape::Arena_& arena,
                                         const Row_<N_>& payoff,
                                         const std::array<double, S_>& right,
                                         size_t center,
                                         const C_& coefficients,
                                         History_<N_, S_>* trace = nullptr) {
            size_t groups = 0;
            const Row_<N_> zero{};
            for (size_t step = 0; step < S_; ++step) {
                coefficients(step, [&](const auto& row) {
                    if (groups == G_)
                        Tape::Promote("Dupire Thomas coefficient group outside fixed plan");
                    auto& factor = factors_[groups++];
                    for (size_t i = 0; i < N_; ++i)
                        factor.Set(arena, i, row(i));
                });
                group_[step] = groups - 1;
                factors_[group_[step]].ForwardStep(right[step], step ? values_[step - 1] : payoff, step ? tangents_[step - 1] : zero, &values_[step],
                                                   &tangents_[step]);
            }
            if (groups != G_)
                Tape::Promote("Dupire Thomas coefficient group outside fixed plan");
            if (trace)
                *trace = values_;
            Row_<N_> adjoint{}, adjointDot{};
            adjoint[center] = 1.0;
            for (size_t step = S_; step-- > 0;)
                factors_[group_[step]].ReverseStep(right[step], values_[step], tangents_[step], &adjoint, &adjointDot);

            if (arena.recordingFailure_)
                std::rethrow_exception(arena.recordingFailure_);
            if (arena.slots_ == Tape::PASSIVE)
                Tape::Error(Tape::Reason::CAPACITY_EXCEEDED, "Dupire Thomas result slot is exhausted");
            auto& buffer = arena.adjoint_;
            const auto root = arena.slots_;
            arena.Ensure(buffer, 0, static_cast<size_t>(root) + 1);
            std::fill_n(buffer.Data(), static_cast<size_t>(root) + 1, Pair_{0.0, 0.0});
            for (size_t group = 0; group < groups; ++group) {
                const auto& factor = factors_[group];
                for (size_t band = 0; band < 3; ++band)
                    for (size_t i = band == 0 ? 1 : 0; i < N_; ++i) {
                        const auto slot = factor.slots_[band][i];
                        if (slot == Tape::PASSIVE)
                            continue;
                        auto& destination = buffer.Data()[slot];
                        const auto seed = factor.seeds_[band][i];
                        destination = {Accumulate(destination.value_, seed.value_), Accumulate(destination.tangent_, seed.tangent_)};
                    }
            }
            ++arena.slots_;
            arena.adjointsSeeded_ = true;
            arena.AccountLogical<double>(true);
            return Tape::Access_::Bind(arena, root, values_.back()[center], tangents_.back()[center]);
        }
    };
} // namespace Dal::DupireSecondOrderDetail::Thomas
