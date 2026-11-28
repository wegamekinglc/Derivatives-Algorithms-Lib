#include <algorithm>
#include <cfenv>
#include <cmath>

#include <dal/math/analytics/detail/spotcallrisk.hpp>

namespace Dal::SpotCallDetail {
    namespace {
        namespace Tape = AAD::SecondOrderDetail;
        using Number_ = AAD::SecondOrderNumber_;

        [[noreturn]] void Validation(Tape::Reason reason, const char* message) { Tape::Access_::Fail(reason, Tape::Stage::VALIDATION, {}, message); }

        double AddProtected(double left, double right) {
            const double sum = left + right;
            const double scale = std::max(std::abs(left), std::abs(right));
            if (std::abs(sum) < 0x1p-28 * scale || ((sum == left || sum == right) && std::min(std::abs(left), std::abs(right)) > 0x1p-42))
                Tape::Promote("Spot call derivative cancellation");
            return sum;
        }

        bool InRange(double value) { return value >= 0x1p-20 && value <= 0x1p20; }
    } // namespace

    FixedSpotCall_::FixedSpotCall_(double strike, double time, double rate, double dividend, std::array<int, 2> axes)
        : strike_(strike), time_(time), axes_(axes) {
        if (std::fegetround() != FE_TONEAREST)
            Validation(Tape::Reason::UNSUPPORTED_OPERATION, "fixed spot call terms require round-to-nearest");
        if (!std::isfinite(strike) || !std::isfinite(time) || !std::isfinite(rate) || !std::isfinite(dividend))
            Validation(Tape::Reason::INVALID_INPUT, "fixed spot call terms must be finite");
        if (!(strike > 0.0 && time > 0.0))
            Validation(Tape::Reason::OUTSIDE_DOMAIN, "fixed spot call requires positive strike and expiry");
        carry_ = rate - dividend;
        rootTime_ = std::sqrt(time);
        spotDiscount_ = std::exp(-dividend * time);
        strikePv_ = strike * std::exp(-rate * time);
        if (!(std::isfinite(carry_) && std::isfinite(spotDiscount_) && spotDiscount_ > 0.0 && std::isfinite(strikePv_) && strikePv_ > 0.0))
            Validation(Tape::Reason::OUTSIDE_DOMAIN, "fixed spot call terms have unresolved carry or discount");
        if (axes_[0] < 0 || axes_[0] > 1 || axes_[1] < 0 || axes_[1] > 1 || (axes_[0] == 1 && axes_[1] == 1))
            Validation(Tape::Reason::INVALID_INPUT, "fixed spot call axes must reference its one or two coordinates");
    }

    Number_ FixedSpotCall_::Record(const Vector_<Number_>& inputs) const {
        auto& arena = *Tape::activeArena;
        if (arena.recordingFailure_)
            std::rethrow_exception(arena.recordingFailure_);
        if (arena.retry_)
            Tape::Promote();
        const auto& spot = inputs[axes_[0]];
        const auto& vol = inputs[axes_[1]];
        if (arena.wide_)
            return Scalar(spot, vol);

        constexpr size_t temporary = sizeof(Tape::EdgeWriter_<double>) + sizeof(Tape::Dual_<double>) + 2 * sizeof(Tape::Weight_<double>) +
                                     8 * sizeof(double) + 2 * sizeof(Tape::Slot_);
        try {
            arena.AccountTemporary(temporary);
            const auto spotSlot = Tape::Access_::Slot(spot, arena), volSlot = Tape::Access_::Slot(vol, arena);
            const double s = Tape::Access_::Value(spot), sigma = Tape::Access_::Value(vol);
            const double sDot = Tape::Access_::Tangent<double>(spot, arena, spotSlot), sigmaDot = Tape::Access_::Tangent<double>(vol, arena, volSlot);
            const double forwardPv = s * spotDiscount_, u = sigma * rootTime_;
            const double d1 = D1(s, sigma), d2 = d1 - u;
            if (!(InRange(s) && InRange(sigma) && InRange(spotDiscount_) && InRange(rootTime_) && InRange(forwardPv) && InRange(strikePv_) &&
                  InRange(u) && std::abs(d1) <= 20.0 && std::abs(d2) <= 20.0 && std::abs(sDot) <= 0x1p20 && std::abs(sigmaDot) <= 0x1p20))
                Tape::Promote("Spot call derivative range");

            constexpr double invRootTwo = 0.7071067811865475244, invRootTwoPi = 0.39894228040143267794;
            const double cdf1 = 0.5 * std::erfc(-invRootTwo * d1), cdf2 = 0.5 * std::erfc(-invRootTwo * d2);
            const double density = invRootTwoPi * std::exp((-0.5 * d1) * d1);
            const double relativeSpot = sDot / s;
            const double d1Dot = AddProtected(relativeSpot, -d2 * (rootTime_ * sigmaDot)) / u;
            const double delta = spotDiscount_ * cdf1, vega = Tape::Guard(rootTime_ * (forwardPv * density));
            const auto deltaDot = Tape::Guard(spotDiscount_ * density * d1Dot);
            const auto vegaDot = Tape::Guard(vega * AddProtected(relativeSpot, -d1 * d1Dot));
            const Tape::Dual_<double> local{forwardPv * cdf1 - strikePv_ * cdf2, Tape::Guard(AddProtected(delta * sDot, vega * sigmaDot))};
            const auto result = Tape::Append<double, 2>(arena, local, temporary, [&](auto& edges) {
                edges.AddEdge(spotSlot, {delta, deltaDot});
                edges.AddEdge(volSlot, {vega, vegaDot});
            });
            arena.AccountLogical<double>(false, temporary);
            return result;
        } catch (const Tape::Retry_&) {
            throw;
        } catch (...) {
            arena.RecordFailure();
        }
    }

    AAD::SecondOrderResult_
    FixedSpotCall_::Evaluate(const Vector_<>& point, const Matrix_<>& directions, const AAD::SecondOrderOptions_& options) const {
        if (std::fegetround() != FE_TONEAREST)
            Validation(Tape::Reason::UNSUPPORTED_OPERATION, "fixed spot call requires round-to-nearest");
        if (point.size() != static_cast<size_t>(1 + std::max(axes_[0], axes_[1])))
            Validation(Tape::Reason::INVALID_INPUT, "fixed spot call point does not match its axes");
        for (double input : point) {
            if (!std::isfinite(input))
                Validation(Tape::Reason::INVALID_INPUT, "fixed spot call point must be finite");
            if (!(input > 0.0))
                Validation(Tape::Reason::OUTSIDE_DOMAIN, "fixed spot call requires positive spot and volatility");
        }
        auto result = AAD::EvaluateSecondOrder([this](const auto& inputs) { return Record(inputs); }, point, directions, options);
        const auto& work = result.Execution();
        const auto protectedRows = work.safePathActivations_;
        const auto rows = static_cast<size_t>(std::max(1, directions.Rows()));
        Tape::Access_::SetMethod(&result, protectedRows == 0      ? "NativeSpotCallDoubleWeights"
                                          : protectedRows == rows ? "NativeSpotCallWideReplay"
                                                                  : "NativeSpotCallMixedReplay");
        return result;
    }
} // namespace Dal::SpotCallDetail
