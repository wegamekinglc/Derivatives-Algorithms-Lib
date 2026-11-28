#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <type_traits>

#include <dal/model/detail/dupirequotequalification.hpp>
#include <dal/model/dupirequotesecondorder.hpp>

namespace Dal::AAD {
    class RecordingScope_;
}

namespace Dal::DupireSecondOrderDetail {
    inline constexpr double SPOT = 100.0, STRIKE = 105.0, RATE = 0.05, DIVIDEND = 0.02, BASE_VOL = 0.2;
    inline constexpr int SPACE_NODES = 81, TIME_STEPS = 64, INTERIOR = SPACE_NODES - 2;
    inline constexpr double HALF_WIDTH = 0.8, LOG_LOW = -HALF_WIDTH, DX = 0.02, DT = 1.0 / TIME_STEPS;
    inline constexpr std::array<double, 6> QUOTE_SPREADS{0.008, 0.012, 0.0, 0.004, 0.006, 0.009};
    inline constexpr std::array<double, 3> QUOTE_STRIKES{75.0, 105.0, 135.0};
    inline constexpr std::array<double, 2> QUOTE_TIMES{0.4, 1.2};
    inline constexpr std::array<double, 9> LOCAL_STRIKES{60.0, 70.0, 80.0, 90.0, 100.0, 110.0, 120.0, 130.0, 140.0};
    inline constexpr std::array<double, 2> LOCAL_TIMES{0.5, 1.0};
    inline constexpr std::array<int, 2> FIRST{1, 0}, LAST{7, 8};

    struct Margins_ {
        double iv_ = std::numeric_limits<double>::infinity();
        double ckk_ = std::numeric_limits<double>::infinity();
        double numerator_ = std::numeric_limits<double>::infinity();
        double variance_ = std::numeric_limits<double>::infinity();
        double curvatureUlps_ = std::numeric_limits<double>::infinity();
        double pivot_ = std::numeric_limits<double>::infinity();
        double ivLower_ = iv_, stdDevLower_ = iv_, ckkLower_ = iv_, numeratorLower_ = iv_, varianceLower_ = iv_;
        double leftBoundary_ = iv_, rightBoundary_ = iv_, diagonalMargin_ = iv_, pivotLower_ = iv_, normalizedMargin_ = iv_;
        double price_ = 0.0;
    };

    struct Bracket_ {
        int left_ = 0, right_ = 0;
        double weight_ = 0.0;

        bool SameCell(const Bracket_& rhs) const { return left_ == rhs.left_ && right_ == rhs.right_; }
    };

    template <size_t N_> Bracket_ Bracket(const std::array<double, N_>& axis, double x) {
        const auto hi = std::upper_bound(axis.begin(), axis.end(), x);
        if (hi == axis.begin())
            return {};
        const auto right = static_cast<int>(hi - axis.begin());
        if (hi == axis.end())
            return {right - 1, right - 1, 0.0};
        const int left = right - 1;
        return {left, right, (x - axis[left]) / (axis[right] - axis[left])};
    }

    template <class T_, class V_> T_ Interpolate(const V_& values, const Bracket_& bracket) {
        const auto& left = values[bracket.left_];
        if (bracket.left_ == bracket.right_)
            return left;
        return left + (values[bracket.right_] - left) * bracket.weight_;
    }

    struct StencilCall_ {
        Bracket_ strikes_, times_;
        double strike_ = 0.0, discount_ = 0.0, forward_ = 0.0, rootTime_ = 0.0, logMoneyness_ = 0.0;

        template <class T_> T_ Vol(const Dal::Vector_<T_>& quotes) const {
            const auto atStrike = [&](int row) -> T_ {
                const int start = 2 * row;
                const auto& left = quotes[start + times_.left_];
                return left + (quotes[start + times_.right_] - left) * times_.weight_;
            };
            const T_ left = atStrike(strikes_.left_);
            if (strikes_.left_ == strikes_.right_)
                return BASE_VOL + left;
            const T_ spread = left + (atStrike(strikes_.right_) - left) * strikes_.weight_;
            return BASE_VOL + spread;
        }

        template <class T_> T_ Price(const Dal::Vector_<T_>& quotes, Margins_* margins, int strikeIndex, int timeIndex) const {
            using std::erfc;
            constexpr double INV_ROOT_TWO = 0.7071067811865475244;
            const T_ vol = Vol(quotes);
            if constexpr (std::is_same_v<T_, double>) {
                if (margins) {
                    REQUIRE(std::isfinite(vol) && vol > 0.0, "Dupire benchmark requires positive stencil IV");
                    margins->iv_ = std::min(margins->iv_, vol);
                }
            }
            const T_ stdDev = vol * rootTime_;
            if constexpr (std::is_same_v<T_, Bound_>) {
                CertifyCalibration(vol, "IV", strikeIndex, timeIndex, margins->ivLower_);
                CertifyCalibration(stdDev, "standard deviation", strikeIndex, timeIndex, margins->stdDevLower_);
            }
            const T_ d2 = logMoneyness_ / stdDev - 0.5 * stdDev;
            const T_ d1 = d2 + stdDev;
            return discount_ * (forward_ * (0.5 * erfc(-INV_ROOT_TWO * d1)) - strike_ * (0.5 * erfc(-INV_ROOT_TWO * d2)));
        }
    };

    struct Stencil_ {
        double strike_ = 0.0, strikeStep_ = 0.0, timeStep_ = 0.0;
        StencilCall_ base_, timeMinus_, timePlus_, strikeMinus_, strikePlus_;

        template <class T_> T_ LocalVol(const Dal::Vector_<T_>& quotes, Margins_* margins, int strikeIndex, int timeIndex) const {
            using std::sqrt;
            const T_ mid = base_.Price(quotes, margins, strikeIndex, timeIndex);
            const T_ earlier = timeMinus_.Price(quotes, margins, strikeIndex, timeIndex),
                     later = timePlus_.Price(quotes, margins, strikeIndex, timeIndex);
            const T_ ct = (later - earlier) * 0.5 / timeStep_;
            const T_ left = strikeMinus_.Price(quotes, margins, strikeIndex, timeIndex),
                     right = strikePlus_.Price(quotes, margins, strikeIndex, timeIndex);
            const T_ centered = left + right - 2.0 * mid;
            const T_ ckk = centered / strikeStep_ / strikeStep_;
            const T_ ck = (right - left) * 0.5 / strikeStep_;
            const T_ numerator = ct + DIVIDEND * mid + (RATE - DIVIDEND) * strike_ * ck;
            if constexpr (std::is_same_v<T_, Bound_>) {
                CertifyCalibration(ckk, "C_KK", strikeIndex, timeIndex, margins->ckkLower_);
                CertifyCalibration(numerator, "Dupire numerator", strikeIndex, timeIndex, margins->numeratorLower_);
            }
            const T_ scaledVariance = 2.0 * numerator / ckk;
            if constexpr (std::is_same_v<T_, Bound_>)
                CertifyCalibration(scaledVariance / (strike_ * strike_), "local variance", strikeIndex, timeIndex, margins->varianceLower_);
            if constexpr (std::is_same_v<T_, double>) {
                if (margins) {
                    const double scale = std::abs(left) + std::abs(right) + 2.0 * std::abs(mid);
                    const double resolution = centered / (std::numeric_limits<double>::epsilon() * scale);
                    const double variance = scaledVariance / (strike_ * strike_);
                    REQUIRE(std::isfinite(resolution) && resolution > 8.0 && std::isfinite(ckk) && ckk > 0.0 && std::isfinite(numerator) &&
                                numerator > 0.0 && std::isfinite(variance) && variance > 0.0,
                            "Dupire benchmark stencil is outside its smooth resolved domain");
                    margins->curvatureUlps_ = std::min(margins->curvatureUlps_, resolution);
                    margins->ckk_ = std::min(margins->ckk_, ckk);
                    margins->numerator_ = std::min(margins->numerator_, numerator);
                    margins->variance_ = std::min(margins->variance_, variance);
                }
            }
            return sqrt(scaledVariance) / strike_;
        }
    };

    template <class T_> using Surface_ = std::array<std::array<T_, LOCAL_STRIKES.size()>, LOCAL_TIMES.size()>;

    class Kernel_ {
        std::array<std::array<Stencil_, LOCAL_STRIKES.size()>, LOCAL_TIMES.size()> stencils_;
        std::array<Bracket_, INTERIOR> space_;
        std::array<Bracket_, TIME_STEPS> time_;
        std::array<double, INTERIOR> payoff_;
        std::array<double, TIME_STEPS> rightBoundary_;
        double diffusion_, drift_;
        double lowerBoundaryMargin_, upperBoundaryMargin_;
        double strikeMargin_ = std::numeric_limits<double>::infinity();
        double timeMargin_ = std::numeric_limits<double>::infinity();

    public:
        explicit Kernel_(double strike = STRIKE, double halfWidth = HALF_WIDTH);
        [[nodiscard]] Margins_ Certify(const Vector_<>& quotes) const;

        [[nodiscard]] double StrikeMargin() const { return strikeMargin_; }
        [[nodiscard]] double TimeMargin() const { return timeMargin_; }

        template <class T_> Surface_<T_> Calibrate(const Dal::Vector_<T_>& quotes, Margins_* margins = nullptr) const {
            Surface_<T_> vols;
            for (size_t t = 0; t < LOCAL_TIMES.size(); ++t) {
                for (int j = FIRST[t]; j <= LAST[t]; ++j)
                    vols[t][j] = stencils_[t][j].LocalVol(quotes, margins, j, static_cast<int>(t));
                for (int j = 0; j < FIRST[t]; ++j)
                    vols[t][j] = vols[t][FIRST[t]];
                for (int j = LAST[t] + 1; j < static_cast<int>(LOCAL_STRIKES.size()); ++j)
                    vols[t][j] = vols[t][LAST[t]];
            }
            return vols;
        }

        template <class T_> T_ Rollback(const Surface_<T_>& vols, Margins_* margins = nullptr) const {
            std::array<std::array<T_, INTERIOR>, LOCAL_TIMES.size()> slices;
            for (size_t t = 0; t < LOCAL_TIMES.size(); ++t)
                for (int i = 0; i < INTERIOR; ++i)
                    slices[t][i] = Interpolate<T_>(vols[t], space_[i]);
            Dal::Vector_<T_> values;
            values.reserve(INTERIOR);
            for (double payoff : payoff_)
                values.emplace_back(payoff);
            std::array<T_, INTERIOR> lower, invPivot;
            std::array<T_, INTERIOR - 1> scaledUpper;
            T_ boundaryUpper(0.0);
            for (int step = 0; step < TIME_STEPS; ++step) {
                const auto& t = time_[step];
                // Equal passive interpolation plans give exactly the same
                // active matrix. Reuse its factors within this recording.
                if (step == 0 || !t.SameCell(time_[step - 1]) || t.weight_ != time_[step - 1].weight_) {
                    for (int i = 0; i < INTERIOR; ++i) {
                        const T_ vol = t.left_ == t.right_ ? slices[t.left_][i]
                                                           : T_(slices[t.left_][i] + (slices[t.right_][i] - slices[t.left_][i]) * t.weight_);
                        const T_ diffusion = 0.5 * vol * vol;
                        const T_ drift = RATE - DIVIDEND - diffusion;
                        lower[i] = drift_ * drift - diffusion_ * diffusion;
                        T_ upper = -drift_ * drift - diffusion_ * diffusion;
                        T_ pivot = 1.0 + RATE * DT + (2.0 * diffusion_) * diffusion;
                        if constexpr (std::is_same_v<T_, Bound_>)
                            CertifyRow(lower[i], pivot, upper, step, i, *margins);
                        if (i != 0)
                            pivot -= lower[i] * scaledUpper[i - 1];
                        if constexpr (std::is_same_v<T_, Bound_>)
                            CertifyActualPivot(pivot.value_, step, i, margins->pivot_);
                        if constexpr (std::is_same_v<T_, double>) {
                            if (margins) {
                                REQUIRE(std::isfinite(pivot) && pivot > 0.0, "Dupire benchmark requires a positive Thomas pivot");
                                margins->pivot_ = std::min(margins->pivot_, pivot);
                            }
                        }
                        invPivot[i] = 1.0 / pivot;
                        if (i + 1 != INTERIOR)
                            scaledUpper[i] = upper * invPivot[i];
                        else
                            boundaryUpper = std::move(upper);
                    }
                }
                for (int i = 0; i < INTERIOR; ++i) {
                    if (i + 1 == INTERIOR)
                        values[i] -= boundaryUpper * rightBoundary_[step];
                    if (i != 0)
                        values[i] -= lower[i] * values[i - 1];
                    values[i] *= invPivot[i];
                }
                for (int i = INTERIOR - 2; i >= 0; --i)
                    values[i] -= scaledUpper[i] * values[i + 1];
            }
            return values[INTERIOR / 2];
        }

        template <class T_> T_ operator()(Dal::AAD::RecordingScope_*, const Dal::Vector_<T_>& quotes) const { return Rollback(Calibrate(quotes)); }
    };

    struct SnapshotAccess_ {
        static void ValidateFixedBase(const DupireCalibrationSnapshot_& snapshot);
    };

    struct PlanAccess_ {
        static const Kernel_& Kernel(const DupireQuoteSecondOrderPlan_& plan);
        static const Margins_& Margins(const DupireQuoteSecondOrderPlan_& plan);
        static const Vector_<>& Point(const DupireQuoteSecondOrderPlan_& plan);
        static DupireQuoteSecondOrderResult_ Evaluate(const DupireQuoteSecondOrderPlan_& plan,
                                                      const Vector_<DupireQuoteCoordinate_>& axis,
                                                      const Matrix_<>& directions,
                                                      const AAD::SecondOrderOptions_& options);
        static const DupireEuropeanCall_& Call(const DupireQuoteSecondOrderPlan_& plan);
        static const String_& Algorithm(const DupireQuoteSecondOrderPlan_& plan);
    };
} // namespace Dal::DupireSecondOrderDetail
