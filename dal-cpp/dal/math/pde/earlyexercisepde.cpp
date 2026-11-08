#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string_view>

#include <dal/math/pde/earlyexercisepdeinternal.hpp>
#include <dal/storage/canonicaljson_internal.hpp>

namespace Dal {
#include <dal/auto/MG_EarlyExercisePdeLevelKind_enum.inc>
#include <dal/auto/MG_EarlyExercisePdeStatus_enum.inc>
#include <dal/auto/MG_EarlyExerciseStyle_enum.inc>
} // namespace Dal

namespace Dal::PDE {
    namespace {
        using Status_ = EarlyExercisePdeStatus_::Value_;
        using Kind_ = EarlyExercisePdeLevelKind_::Value_;
        using EarlyExercisePdeInternal::PlanCounts_;
        using EarlyExercisePdeInternal::PlanStep_;

        void Check(bool valid, const String_& field, const char* constraint) {
            REQUIRE(valid, "InvalidEarlyExercisePdeRequest; " + field + "; " + constraint);
        }
        template <class T_> String_ ValueField(const String_& field, T_ value) {
            std::ostringstream text;
            text.precision(std::numeric_limits<double>::max_digits10);
            text << value;
            return field + "; value=" + String_(text.str());
        }
        String_ Indexed(const char* field, size_t i) { return String_(field) + "[" + String_(std::to_string(i)) + "]"; }
        size_t Add(size_t a, size_t b) {
            Check(b <= (std::numeric_limits<size_t>::max)() - a, "extent", "size_t addition overflow");
            return a + b;
        }
        size_t Multiply(size_t a, size_t b) {
            Check(a == 0 || b <= (std::numeric_limits<size_t>::max)() / a, "extent", "size_t multiplication overflow");
            return a * b;
        }
        void Budget(size_t needed, size_t available, const char* field) {
            REQUIRE(needed <= available, String_("EarlyExercisePdeBudgetExceeded; ") + field + "=" + String_(std::to_string(available)) +
                                             "; required_upper_bound=" + String_(std::to_string(needed)));
        }
        double Years(Date_ date, Date_ start) { return (date - start) / 365.0; }
        double Upper(const EarlyExercisePdeRequest_& r) { return r.settings_.upper_.value_or(4.0 * std::max(r.market_.spot_, r.contract_.strike_)); }
        bool American(const EarlyExercisePdeRequest_& r) { return r.contract_.exerciseStyle_ == EarlyExerciseStyle_::Value_::AMERICAN; }

        struct Coefficients_ {
            double minus_, plus_;
            bool upwind_;
        };
        Coefficients_ Coefficients(double vol, double rate, double yield, int i) {
            const double diffusion = 0.5 * (vol * i) * (vol * i);
            const double drift = (rate - yield) * i;
            const bool upwind = diffusion < 0.5 * std::abs(drift);
            return {diffusion + (upwind ? std::max(-drift, 0.0) : -0.5 * drift), diffusion + (upwind ? std::max(drift, 0.0) : 0.5 * drift), upwind};
        }
        double PlanPoint(double low, double high, size_t j, size_t subdivisions) {
            return j == subdivisions ? low : high - (high - low) * j / subdivisions;
        }
        size_t StableSubdivisions(double low, double high, double theta, double rate, double maxDiagonal, int emitted, int limit) {
            Check(high > low, "time_grid", "substep endpoints must be representable and strictly separated");
            size_t subdivisions = 1;
            for (;;) {
                Budget(Add(emitted, subdivisions), limit, "max_actual_steps");
                bool valid = true;
                for (size_t j = 0; j < subdivisions; ++j) {
                    const double dt = PlanPoint(low, high, j, subdivisions) - PlanPoint(low, high, j + 1, subdivisions);
                    Check(dt > 0.0, "time_grid", "cannot bisect representable substep");
                    if (!(1.0 + theta * dt * rate > 0.0) || !(1.0 - (1.0 - theta) * dt * maxDiagonal >= 0.0)) {
                        valid = false;
                        break;
                    }
                }
                if (valid)
                    return subdivisions;
                subdivisions = Multiply(subdivisions, 2);
            }
        }
        void PreviousExercise(const EarlyExercisePdeContract_& c, Date_ right, int* exercise) {
            while (*exercise >= 0 && c.exerciseDates_[*exercise] >= right)
                --*exercise;
        }
        void PreviousCarry(const EarlyExercisePdeMarket_& market, Date_ right, int* carry) {
            while (*carry > 0 && market.carry_[*carry - 1].endDate_ >= right)
                --*carry;
        }
        Date_ LeftAnchor(const EarlyExercisePdeRequest_& r, int carry, int exercise) {
            Date_ left = carry > 0 ? r.market_.carry_[carry - 1].endDate_ : r.contract_.evaluationDate_;
            if (exercise >= 0 && r.contract_.exerciseDates_[exercise] > left)
                left = r.contract_.exerciseDates_[exercise];
            return left;
        }
        double SegmentDiagonal(const EarlyExercisePdeMarket_& market, int nodes, int carry, PlanCounts_* counts) {
            const auto& segment = market.carry_[carry];
            double maxDiagonal = segment.rate_;
            int upwind = 0;
            for (int i = 1; i < nodes - 1; ++i) {
                const auto a = Coefficients(market.volatility_, segment.rate_, segment.dividendYield_, i);
                Check(std::isfinite(a.minus_) && std::isfinite(a.plus_) && std::isfinite(a.minus_ + a.plus_ + segment.rate_), Indexed("operator", i),
                      "coefficients must be finite");
                maxDiagonal = std::max(maxDiagonal, a.minus_ + a.plus_ + segment.rate_);
                upwind += a.upwind_;
            }
            counts->upwind_ = std::max(counts->upwind_, upwind);
            return maxDiagonal;
        }
        int IntervalSteps(const EarlyExercisePdeRequest_& r, Date_ left, Date_ right, PlanCounts_* counts) {
            const uint64_t numerator = uint64_t(r.settings_.ordinarySteps_) * uint64_t(right - left);
            const auto days = uint64_t(r.contract_.maturityDate_ - r.contract_.evaluationDate_);
            const auto base = std::max(uint64_t(1), (numerator + days - 1) / days);
            Budget(Add(counts->base_, base), r.settings_.maxActualSteps_, "max_actual_steps");
            counts->base_ += static_cast<int>(base);
            return static_cast<int>(base);
        }
        template <class F_>
        void EmitParts(const EarlyExercisePdeRequest_& r,
                       PlanStep_ step,
                       double rate,
                       double maxDiagonal,
                       std::optional<Date_> date,
                       PlanCounts_* counts,
                       F_ emit) {
            const int parts = step.damped_ ? 2 : 1;
            for (int part = 0; part < parts; ++part) {
                const double high = part == 0 ? step.source_ : step.target_ + (step.source_ - step.target_) * 0.5;
                const double low = part + 1 == parts ? step.target_ : step.target_ + (step.source_ - step.target_) * 0.5;
                const auto endpoint = part + 1 == parts ? date : std::nullopt;
                const size_t subdivisions =
                    StableSubdivisions(low, high, step.theta_, rate, maxDiagonal, counts->steps_, r.settings_.maxActualSteps_);
                for (size_t j = 0; j < subdivisions; ++j) {
                    emit(PlanStep_{PlanPoint(low, high, j + 1, subdivisions), PlanPoint(low, high, j, subdivisions), step.theta_, step.carry_,
                                   step.damped_, j + 1 == subdivisions ? endpoint : std::nullopt});
                    ++counts->steps_;
                }
            }
        }
        PlanStep_ BaseStep(double lo, double hi, int b, int base, int carry, int* damping, PlanCounts_* counts, double theta) {
            const double source = b == 0 ? hi : hi - (hi - lo) * b / base;
            const double target = b + 1 == base ? lo : hi - (hi - lo) * (b + 1) / base;
            const bool damped = *damping > 0;
            if (damped) {
                --*damping;
                ++counts->damping_;
            }
            return {target, source, damped ? 1.0 : theta, carry, damped, std::nullopt};
        }
        struct StabilityBounds_ {
            double rate_, diagonal_;
        };
        StabilityBounds_
        SegmentBounds(const EarlyExercisePdeRequest_& r, int carry, const Vector_<EarlyExercisePdeMarket_>& extraMarkets, PlanCounts_* counts) {
            StabilityBounds_ bounds{r.market_.carry_[carry].rate_, SegmentDiagonal(r.market_, r.settings_.gridPoints_, carry, counts)};
            for (const auto& market : extraMarkets) {
                bounds.diagonal_ = std::max(bounds.diagonal_, SegmentDiagonal(market, r.settings_.gridPoints_, carry, counts));
                bounds.rate_ = std::min(bounds.rate_, market.carry_[carry].rate_);
            }
            return bounds;
        }
        template <class F_> PlanCounts_ Plan(const EarlyExercisePdeRequest_& r, F_ emit, const Vector_<EarlyExercisePdeMarket_>& extraMarkets = {}) {
            PlanCounts_ counts;
            const auto& c = r.contract_;
            const auto& s = r.settings_;
            Date_ right = c.maturityDate_;
            int carry = static_cast<int>(r.market_.carry_.size()) - 1;
            int exercise = static_cast<int>(c.exerciseDates_.size()) - 1;
            int damping = s.theta_ == 0.5 ? s.rannacherIntervals_ : 0;
            while (right > c.evaluationDate_) {
                PreviousExercise(c, right, &exercise);
                PreviousCarry(r.market_, right, &carry);
                const Date_ left = LeftAnchor(r, carry, exercise);
                const auto bounds = SegmentBounds(r, carry, extraMarkets, &counts);
                const int base = IntervalSteps(r, left, right, &counts);
                const double lo = Years(left, c.evaluationDate_), hi = Years(right, c.evaluationDate_);
                for (int b = 0; b < base; ++b) {
                    const auto step = BaseStep(lo, hi, b, base, carry, &damping, &counts, s.theta_);
                    EmitParts(r, step, bounds.rate_, bounds.diagonal_, b + 1 == base ? std::optional<Date_>(left) : std::nullopt, &counts, emit);
                }
                right = left;
                if (s.theta_ == 0.5 && exercise >= 0 && c.exerciseDates_[exercise] == left)
                    damping = s.rannacherIntervals_;
            }
            return counts;
        }

        struct CarryIntegral_ {
            double end_, rate_, yield_, cumulativeRate_, cumulativeYield_;
        };
        using Integral_ = std::pair<double, double>;
        Integral_ Integral(const Vector_<CarryIntegral_>& carry, double t) {
            const auto it = std::lower_bound(carry.begin(), carry.end(), t, [](const auto& x, double y) { return x.end_ < y; });
            const auto& x = it == carry.end() ? carry.back() : *it;
            return {x.cumulativeRate_ - (x.end_ - t) * x.rate_, x.cumulativeYield_ - (x.end_ - t) * x.yield_};
        }
        double DiscountedPayoff(double spot, double strike, bool call, double rate, double yield) {
            const long double a = spot == 0.0 ? -std::numeric_limits<long double>::infinity() : std::log((long double)spot) - yield;
            const long double b = std::log((long double)strike) - rate;
            long double value = 0.0;
            if (call && a > b)
                value = std::exp(a) * -std::expm1(b - a);
            else if (!call && b > a)
                value = std::exp(b) * -std::expm1(a - b);
            Check(std::isfinite(value) && value <= (std::numeric_limits<double>::max)(), "boundary", "discounted payoff must be representable");
            return static_cast<double>(value);
        }
        template <class F_> void BermudanBoundaryCandidates(const EarlyExercisePdeRequest_& r, double t, F_ consider) {
            for (const auto& date : r.contract_.exerciseDates_) {
                const double e = Years(date, r.contract_.evaluationDate_);
                if (e > t)
                    consider(e);
            }
        }
        template <class F_>
        void AmericanBoundaryCandidates(
            const EarlyExercisePdeRequest_& r, const Vector_<CarryIntegral_>& carry, double t, double spot, const Integral_& at, F_ consider) {
            consider(t);
            double start = t;
            for (const auto& x : carry) {
                if (x.end_ <= t)
                    continue;
                consider(x.end_);
                if (spot > 0.0 && x.rate_ * x.yield_ > 0.0 && x.rate_ != x.yield_) {
                    const auto from = Integral(carry, start);
                    const double logRatio = std::log(std::abs(x.rate_)) + std::log(r.contract_.strike_) - (from.first - at.first) -
                                            std::log(std::abs(x.yield_)) - std::log(spot) + (from.second - at.second);
                    const double h = logRatio / (x.rate_ - x.yield_);
                    if (h > 0.0 && h < x.end_ - start)
                        consider(start + h);
                }
                start = x.end_;
            }
        }
        double Boundary(const EarlyExercisePdeRequest_& r, const Vector_<CarryIntegral_>& carry, double t, double spot) {
            const bool call = r.contract_.optionType_ == OptionType_::Value_::CALL;
            const auto at = Integral(carry, t);
            double best = 0.0;
            const auto consider = [&](double e) {
                const auto to = Integral(carry, e);
                best = std::max(best, DiscountedPayoff(spot, r.contract_.strike_, call, to.first - at.first, to.second - at.second));
            };
            if (!American(r))
                BermudanBoundaryCandidates(r, t, consider);
            else
                AmericanBoundaryCandidates(r, carry, t, spot, at, consider);
            return best;
        }
        void MaxOptional(std::optional<double>* target, const std::optional<double>& value) {
            if (value)
                *target = std::max(target->value_or(0.0), *value);
        }
        // Binary64 significands with separate exponents retain diffusion tails beyond hardware underflow.
        struct RangeValue_ {
            double fraction_;
            int exponent_ = 0;

            explicit RangeValue_(double fraction, int exponent = 0) : fraction_(fraction), exponent_(exponent) {}
            static RangeValue_ Pack(double fraction, int exponent) {
                if (fraction == 0.0)
                    return RangeValue_(0.0);
                if (exponent > -500 && exponent < 1000)
                    return RangeValue_(std::scalbn(fraction, exponent));
                return RangeValue_(fraction, exponent);
            }
            [[nodiscard]] RangeValue_ Times(double coefficient) const {
                const double product = fraction_ * coefficient;
                if (fraction_ == 0.0 || coefficient == 0.0 || (exponent_ == 0 && std::isfinite(product) && std::abs(product) >= 0x1p-500))
                    return RangeValue_(product);
                int leftExponent = 0, rightExponent = 0;
                const double left = std::frexp(fraction_, &leftExponent), right = std::frexp(coefficient, &rightExponent);
                return Pack(left * right, exponent_ + leftExponent + rightExponent);
            }
            [[nodiscard]] RangeValue_ DividedBy(double divisor) const {
                const double quotient = fraction_ / divisor;
                if (fraction_ == 0.0 || (exponent_ == 0 && std::isfinite(quotient) && std::abs(quotient) >= 0x1p-500))
                    return RangeValue_(quotient);
                int leftExponent = 0, rightExponent = 0;
                const double left = std::frexp(fraction_, &leftExponent), right = std::frexp(divisor, &rightExponent);
                return Pack(left / right, exponent_ + leftExponent - rightExponent);
            }
            [[nodiscard]] RangeValue_ Minus(const RangeValue_& other) const {
                if (exponent_ == 0 && other.exponent_ == 0)
                    return RangeValue_(fraction_ - other.fraction_);
                if (other.fraction_ == 0.0)
                    return *this;
                if (fraction_ == 0.0)
                    return RangeValue_(-other.fraction_, other.exponent_);
                int leftExponent = 0, rightExponent = 0;
                const double left = std::frexp(fraction_, &leftExponent), right = std::frexp(other.fraction_, &rightExponent);
                leftExponent += exponent_;
                rightExponent += other.exponent_;
                const int common = std::max(leftExponent, rightExponent);
                return Pack(std::scalbn(left, leftExponent - common) - std::scalbn(right, rightExponent - common), common);
            }
            [[nodiscard]] double Value() const { return exponent_ == 0 ? fraction_ : std::scalbn(fraction_, exponent_); }
        };
        struct Scratch_ {
            Vector_<> minus_, plus_, rhs_, payoff_, correction_;
            Vector_<> current_, iterate_, previous_, pivots_, solveRhs_;
            Vector_<int> iterateExponents_, previousExponents_, rhsExponents_;
            double scale_ = 1.0;
            bool extendedRange_ = false;
            explicit Scratch_(size_t n)
                : minus_(n), plus_(n), rhs_(n), payoff_(n), correction_(n), current_(n), iterate_(n), previous_(n), pivots_(n), solveRhs_(n),
                  iterateExponents_(n, 0), previousExponents_(n, 0), rhsExponents_(n, 0) {}
            [[nodiscard]] size_t Bytes() const {
                return sizeof(double) * (minus_.capacity() + plus_.capacity() + rhs_.capacity() + payoff_.capacity() + correction_.capacity()) +
                       sizeof(double) *
                           (current_.capacity() + iterate_.capacity() + previous_.capacity() + pivots_.capacity() + solveRhs_.capacity()) +
                       sizeof(int) * (iterateExponents_.capacity() + previousExponents_.capacity() + rhsExponents_.capacity());
            }
        };
        double Dual(const Scratch_& a, int i) {
            long double w = (long double)a.current_[i] - a.rhs_[i];
            if (i > 1)
                w += (long double)a.minus_[i] * a.current_[i - 1];
            if (i + 2 < static_cast<int>(a.current_.size()))
                w += (long double)a.plus_[i] * a.current_[i + 1];
            return static_cast<double>(w);
        }
        bool Residuals(const Scratch_& a, double scale, EarlyExercisePdeStepDiagnostic_* d) {
            double obstacle = 0.0, dual = 0.0, complementarity = 0.0;
            for (int i = 1; i + 1 < static_cast<int>(a.current_.size()); ++i) {
                const double gap = a.current_[i] - a.payoff_[i], w = Dual(a, i);
                if (!std::isfinite(gap) || !std::isfinite(w))
                    return false;
                obstacle = std::max(obstacle, -gap);
                dual = std::max(dual, -w);
                complementarity = std::max(complementarity, std::abs(std::min(gap, w)));
            }
            d->obstacleViolation_ = obstacle;
            d->dualViolation_ = dual;
            d->complementarityResidual_ = complementarity;
            d->normalizedObstacleViolation_ = obstacle / scale;
            d->normalizedDualViolation_ = dual / scale;
            d->normalizedComplementarityResidual_ = complementarity / scale;
            return true;
        }
        bool Admitted(const EarlyExercisePdeStepDiagnostic_& d, double epsilon) {
            return d.iterationChange_ <= epsilon && *d.obstacleViolation_ <= epsilon && *d.dualViolation_ <= epsilon &&
                   *d.complementarityResidual_ <= epsilon;
        }

        template <bool EXTENDED> FORCE_INLINE void BackSubstitute(Scratch_* a, bool shifted) {
            const int n = static_cast<int>(a->current_.size());
            for (int i = n - 2; i > 0; --i) {
                if constexpr (EXTENDED) {
                    const auto right = i < n - 2 ? RangeValue_(a->iterate_[i + 1], a->iterateExponents_[i + 1]).Times(a->plus_[i]) : RangeValue_(0.0);
                    const auto value = RangeValue_(a->solveRhs_[i], a->rhsExponents_[i]).Minus(right).DividedBy(a->pivots_[i]);
                    a->iterate_[i] = value.fraction_;
                    a->iterateExponents_[i] = value.exponent_;
                    a->current_[i] = (shifted ? a->payoff_[i] : 0.0) + value.DividedBy(a->scale_).Value();
                } else {
                    a->iterate_[i] = (a->solveRhs_[i] - (i < n - 2 ? a->plus_[i] * a->iterate_[i + 1] : 0.0)) / a->pivots_[i];
                    a->current_[i] = (shifted ? a->payoff_[i] : 0.0) + a->iterate_[i] / a->scale_;
                }
            }
        }
        void ExtendedForwardRow(Scratch_* a, int i, double factor) {
            const auto rhs = RangeValue_(a->solveRhs_[i]).Minus(RangeValue_(a->solveRhs_[i - 1], a->rhsExponents_[i - 1]).Times(factor));
            a->solveRhs_[i] = rhs.fraction_;
            a->rhsExponents_[i] = rhs.exponent_;
        }
        void ExtendSolveRange(Scratch_* a, bool shifted) {
            // Reuse the same factors; only the two triangular sweeps need a wider exponent range.
            for (int i = 1; i + 1 < static_cast<int>(a->current_.size()); ++i) {
                a->solveRhs_[i] = a->correction_[i] * a->scale_;
                a->rhsExponents_[i] = 0;
                if (i > 1)
                    ExtendedForwardRow(a, i, a->minus_[i] / a->pivots_[i - 1]);
            }
            a->extendedRange_ = true;
            BackSubstitute<true>(a, shifted);
        }
        // The exercise style fixes penalty/shift constants across both triangular sweeps.
        template <bool EXTENDED> FORCE_INLINE std::optional<int> Solve(Scratch_* a, double penalty, bool shifted) {
            const int n = static_cast<int>(a->current_.size());
            for (int i = 1; i < n - 1; ++i) {
                const double diagonal = 1.0 + (a->previous_[i] < 0.0 ? penalty : 0.0);
                a->pivots_[i] = diagonal;
                a->solveRhs_[i] = a->correction_[i] * a->scale_;
                if constexpr (EXTENDED)
                    a->rhsExponents_[i] = 0;
                if (i > 1) {
                    const double factor = a->minus_[i] / a->pivots_[i - 1];
                    a->pivots_[i] -= factor * a->plus_[i - 1];
                    if constexpr (EXTENDED)
                        ExtendedForwardRow(a, i, factor);
                    else
                        a->solveRhs_[i] -= factor * a->solveRhs_[i - 1];
                }
                if (!std::isfinite(a->pivots_[i]) || a->pivots_[i] <= 0.0)
                    return i;
            }
            BackSubstitute<EXTENDED>(a, shifted);
            return std::nullopt;
        }
        double RelativeBackwardError(double numerator, double denominator) {
            if (!std::isfinite(numerator) || !std::isfinite(denominator))
                return std::numeric_limits<double>::infinity();
            return denominator == 0.0 ? (numerator == 0.0 ? 0.0 : std::numeric_limits<double>::infinity()) : numerator / denominator;
        }
        double TailBackwardError(const Scratch_& a, int i, double lambda, bool shifted) {
            const auto value = [&](int j) {
                return RangeValue_((shifted ? a.payoff_[j] : 0.0) * a.scale_).Minus(RangeValue_(-a.iterate_[j], a.iterateExponents_[j]));
            };
            const RangeValue_ terms[] = {value(i).Times(1.0 + lambda), i > 1 ? value(i - 1).Times(a.minus_[i]) : RangeValue_(0.0),
                                         i + 2 < static_cast<int>(a.current_.size()) ? value(i + 1).Times(a.plus_[i]) : RangeValue_(0.0),
                                         RangeValue_(-(a.rhs_[i] * a.scale_ + lambda * (a.payoff_[i] * a.scale_)))};
            int common = std::numeric_limits<int>::lowest();
            for (const auto& term : terms)
                if (term.fraction_ != 0.0)
                    common = std::max(common, term.exponent_);
            if (common == std::numeric_limits<int>::lowest())
                return 0.0;
            double residual = 0.0, denominator = 0.0;
            for (const auto& term : terms) {
                const double scaled = term.fraction_ == 0.0 ? 0.0 : std::scalbn(term.fraction_, term.exponent_ - common);
                residual += scaled;
                denominator += std::abs(scaled);
            }
            return RelativeBackwardError(std::abs(residual), denominator);
        }
        template <bool EXTENDED> FORCE_INLINE std::pair<double, bool> BackwardError(const Scratch_& a, double penalty, bool shifted) {
            double maximum = 0.0;
            bool needsExtendedRange = false;
            for (int i = 1; i + 1 < static_cast<int>(a.current_.size()); ++i) {
                const double lambda = a.previous_[i] < 0.0 ? penalty : 0.0;
                if (EXTENDED && (a.iterateExponents_[i] != 0 || (i > 1 && a.iterateExponents_[i - 1] != 0) ||
                                 (i + 2 < static_cast<int>(a.current_.size()) && a.iterateExponents_[i + 1] != 0))) {
                    maximum = std::max(maximum, TailBackwardError(a, i, lambda, shifted));
                    continue;
                }
                const auto value = [&](int j) { return (shifted ? a.payoff_[j] * a.scale_ : 0.0) + a.iterate_[j]; };
                const double rhs = a.rhs_[i] * a.scale_ + lambda * (a.payoff_[i] * a.scale_);
                const double center = (1.0 + lambda) * value(i);
                const double left = i > 1 ? a.minus_[i] * value(i - 1) : 0.0;
                const double right = i + 2 < static_cast<int>(a.current_.size()) ? a.plus_[i] * value(i + 1) : 0.0;
                const double numerator = std::abs(((center + left) + right) - rhs);
                const double denominator = std::abs(rhs) + std::abs(center) + std::abs(left) + std::abs(right);
                // Tiny rows can round both residual and denominator to zero before their ratio fails.
                if constexpr (!EXTENDED)
                    needsExtendedRange = needsExtendedRange || (denominator > 0.0 && denominator < 0x1p-500);
                const double error = RelativeBackwardError(numerator, denominator);
                maximum = std::max(maximum, error);
            }
            return {maximum, needsExtendedRange};
        }
        template <class T_> size_t CapacityBytes(const Vector_<T_>& x) { return Multiply(x.capacity(), sizeof(T_)); }

        struct StorageLayout_ {
            size_t levels_, regions_, payload_, workspace_;
        };
        StorageLayout_ AdmitStorage(const EarlyExercisePdeRequest_& r, const PlanCounts_& counts) {
            const size_t n = r.settings_.gridPoints_, m = counts.steps_, c = r.market_.carry_.size(), d = r.contract_.exerciseDates_.size();
            const size_t levels = American(r) ? Add(m, 1) : d;
            const size_t regions = Multiply(levels - 1, (n + 1) / 2);
            const size_t payload =
                Add(Add(Add(Add(Add(Add(Add(4096, Multiply(32, c)), Multiply(8, d)), Multiply(16, n)), Multiply(64, m + 1)), Multiply(256, m)),
                        Multiply(64, levels)),
                    Multiply(32, regions));
            const size_t workspace = Add(Add(Add(Add(4096, Multiply(256, n)), Multiply(64, m + 1)), Multiply(64, c)), Multiply(16, d));
            Budget(payload, r.settings_.numericPayloadBudgetBytes_, "numeric_payload_budget_bytes");
            Budget(workspace, r.settings_.workspaceBudgetBytes_, "workspace_budget_bytes");
            return {levels, regions, payload, workspace};
        }
    } // namespace

    namespace {
        void ValidateGridSettings(const EarlyExercisePdeSettings_& s) {
            Check(s.gridPoints_ >= 5 && s.gridPoints_ <= 100001, ValueField("settings.grid_points", s.gridPoints_), "must be in 5..100001");
            Check(s.ordinarySteps_ > 0, ValueField("settings.ordinary_steps", s.ordinarySteps_), "must be positive");
            Check(s.theta_ == 0.5 || s.theta_ == 1.0, ValueField("settings.theta", s.theta_), "must be 0.5 or 1.0");
            Check(s.rannacherIntervals_ >= 0 && s.rannacherIntervals_ <= s.ordinarySteps_,
                  ValueField("settings.rannacher_intervals", s.rannacherIntervals_), "must be in 0..ordinary_steps");
            Check(!s.upper_ || (std::isfinite(*s.upper_) && *s.upper_ > 0.0), ValueField("settings.upper", s.upper_.value_or(0.0)),
                  "must be finite and positive or unset");
        }
        void ValidatePrecisionSettings(const EarlyExercisePdeSettings_& s) {
            Check(std::isfinite(s.penaltyFactor_) && s.penaltyFactor_ >= 1.0 && s.penaltyFactor_ <= 1e12,
                  ValueField("settings.penalty_factor", s.penaltyFactor_), "must be in 1..1e12");
            Check(std::isfinite(s.absoluteTolerance_) && s.absoluteTolerance_ > 0.0, ValueField("settings.absolute_tolerance", s.absoluteTolerance_),
                  "must be finite and positive");
            Check(std::isfinite(s.relativeTolerance_) && s.relativeTolerance_ > 0.0 && s.relativeTolerance_ <= 1.0,
                  ValueField("settings.relative_tolerance", s.relativeTolerance_), "must be in (0,1]");
            Check(std::isfinite(s.linearBackwardErrorLimit_) && s.linearBackwardErrorLimit_ > 0.0 && s.linearBackwardErrorLimit_ <= 1.0,
                  ValueField("settings.linear_backward_error_limit", s.linearBackwardErrorLimit_), "must be in (0,1]");
            Check(s.maxIterationsPerStep_ > 0, ValueField("settings.max_iterations_per_step", s.maxIterationsPerStep_), "must be positive");
            Check(s.maxActualSteps_ > 0, ValueField("settings.max_actual_steps", s.maxActualSteps_), "must be positive");
        }
    } // namespace
    void ValidateEarlyExercisePdeSettings(const EarlyExercisePdeSettings_& s) {
        ValidateGridSettings(s);
        ValidatePrecisionSettings(s);
    }

    namespace {
        void ValidateContract(const EarlyExercisePdeContract_& c) {
            Check(c.evaluationDate_ >= Date::Minimum(), ValueField("contract.evaluation_date", Date::ToExcel(c.evaluationDate_)),
                  "must be a valid date");
            Check(c.maturityDate_ > c.evaluationDate_, ValueField("contract.maturity_date", Date::ToExcel(c.maturityDate_)),
                  "must be later than evaluation_date");
            Check(std::isfinite(c.strike_) && c.strike_ > 0.0, ValueField("contract.strike", c.strike_), "must be finite and positive");
            Check(c.optionType_ == OptionType_::Value_::CALL || c.optionType_ == OptionType_::Value_::PUT,
                  ValueField("contract.option_type", c.optionType_.String() ? c.optionType_.String() : "<unset>"), "must be CALL or PUT");
            Check(c.exerciseStyle_ == EarlyExerciseStyle_::Value_::AMERICAN || c.exerciseStyle_ == EarlyExerciseStyle_::Value_::BERMUDAN,
                  ValueField("contract.exercise_style", c.exerciseStyle_.String() ? c.exerciseStyle_.String() : "<unset>"),
                  "must be AMERICAN or BERMUDAN");
        }
        void ValidateMarketGrid(const EarlyExercisePdeRequest_& r) {
            const auto& c = r.contract_;
            Check(std::isfinite(r.market_.spot_) && r.market_.spot_ > 0.0, ValueField("market.spot", r.market_.spot_), "must be finite and positive");
            Check(std::isfinite(r.market_.volatility_) && r.market_.volatility_ >= 0.0, ValueField("market.volatility", r.market_.volatility_),
                  "must be finite and nonnegative");
            const double upper = Upper(r);
            Check(std::isfinite(upper) && upper > std::max(c.strike_, r.market_.spot_), ValueField("settings.upper", upper),
                  "must be finite and exceed spot and strike");
            Check(std::isfinite(r.settings_.absoluteTolerance_ + r.settings_.relativeTolerance_ * std::max(c.strike_, r.market_.spot_)),
                  ValueField("settings.tolerance",
                             r.settings_.absoluteTolerance_ + r.settings_.relativeTolerance_ * std::max(c.strike_, r.market_.spot_)),
                  "combined price tolerance must be finite");
            double lastNode = 0.0;
            for (int i = 1; i < r.settings_.gridPoints_; ++i) {
                const double node = upper * (double(i) / (r.settings_.gridPoints_ - 1));
                Check(node > lastNode && std::isfinite(node), ValueField(Indexed("grid", i), node), "nodes must be finite and strictly increasing");
                lastNode = node;
            }
        }
        void ValidateCarry(const EarlyExercisePdeRequest_& r) {
            const auto& c = r.contract_;
            const double upper = Upper(r);
            Check(!r.market_.carry_.empty(), ValueField("market.carry", r.market_.carry_.size()), "must be nonempty and end at maturity_date");
            Date_ previous = c.evaluationDate_;
            long double rate = 0.0L, yield = 0.0L, maxRate = 0.0L, maxYield = 0.0L;
            for (size_t i = 0; i < r.market_.carry_.size(); ++i) {
                const auto& x = r.market_.carry_[i];
                const auto field = Indexed("market.carry", i);
                Check(x.endDate_ > previous && x.endDate_ <= c.maturityDate_, ValueField(field + ".end_date", Date::ToExcel(x.endDate_)),
                      "must be strictly increasing in (evaluation,maturity]");
                Check(std::isfinite(x.rate_), ValueField(field + ".rate", x.rate_), "must be finite");
                Check(std::isfinite(x.dividendYield_), ValueField(field + ".dividend_yield", x.dividendYield_), "must be finite");
                const double dt = Years(x.endDate_, previous);
                rate += (long double)x.rate_ * dt;
                yield += (long double)x.dividendYield_ * dt;
                Check(std::abs(rate) <= (std::numeric_limits<double>::max)() && std::abs(yield) <= (std::numeric_limits<double>::max)(), field,
                      "carry integrals must be representable");
                Check(std::exp(maxRate - rate) * c.strike_ <= (std::numeric_limits<double>::max)() &&
                          std::exp(maxYield - yield) * upper <= (std::numeric_limits<double>::max)(),
                      field, "discounted boundary terms must be representable");
                maxRate = std::max(maxRate, rate);
                maxYield = std::max(maxYield, yield);
                previous = x.endDate_;
            }
            Check(previous == c.maturityDate_, ValueField("market.carry.back.end_date", Date::ToExcel(previous)), "must equal maturity_date");
        }
        void ValidateExerciseDates(const EarlyExercisePdeRequest_& r) {
            const auto& c = r.contract_;
            if (American(r))
                Check(c.exerciseDates_.empty(), ValueField("contract.exercise_dates", c.exerciseDates_.size()), "must be empty for AMERICAN");
            else {
                Check(!c.exerciseDates_.empty(), ValueField("contract.exercise_dates", c.exerciseDates_.size()), "must be nonempty for BERMUDAN");
                for (size_t i = 0; i < c.exerciseDates_.size(); ++i) {
                    const auto d = c.exerciseDates_[i];
                    Check(d >= c.evaluationDate_ && d <= c.maturityDate_ && (i == 0 || d > c.exerciseDates_[i - 1]),
                          ValueField(Indexed("contract.exercise_dates", i), Date::ToExcel(d)),
                          "must be strictly increasing in [evaluation,maturity]");
                }
                Check(c.exerciseDates_.back() == c.maturityDate_, ValueField("contract.exercise_dates.back", Date::ToExcel(c.exerciseDates_.back())),
                      "must equal maturity_date");
            }
        }
    } // namespace
    void ValidateEarlyExercisePdeRequest(const EarlyExercisePdeRequest_& r) {
        ValidateEarlyExercisePdeSettings(r.settings_);
        ValidateContract(r.contract_);
        ValidateMarketGrid(r);
        ValidateCarry(r);
        ValidateExerciseDates(r);
    }

    namespace {
        struct RollbackOutput_ {
            const Vector_<EarlyExercisePdeTimeNode_>& timeGrid_;
            Vector_<>* values_;
            Vector_<EarlyExercisePdeExerciseLevel_>* levels_;
            Vector_<EarlyExercisePdeExerciseRegion_>* regions_;
            EarlyExercisePdeExecution_* execution_;
            EarlyExercisePdeStatus_* status_;
            std::optional<EarlyExercisePdeFailure_>* failure_;
        };
        struct StepOperator_ {
            double volatility_, rate_, yield_, theta_, dt_;
            int nodes_;
        };
        static_assert(sizeof(StepOperator_) <= 6 * sizeof(double)); // Within the fixed scalar workspace allowance.
        struct AssemblyArrays_ {
            const double *values_, *payoff_;
            double *minus_, *plus_, *rhs_, *correction_, *current_, *iterate_;
        };
        static_assert(sizeof(AssemblyArrays_) <= 8 * sizeof(double)); // Within the fixed scalar workspace allowance.
        template <bool TEST_SEAM> struct ControlState_ {
            explicit ControlState_(const EarlyExercisePdeInternal::RollbackControl_*) {}
        };
        template <> struct ControlState_<true> {
            const EarlyExercisePdeInternal::RollbackControl_* control_ = nullptr;
            explicit ControlState_(const EarlyExercisePdeInternal::RollbackControl_* control) : control_(control) {}
        };
        template <bool AMERICAN, bool TEST_SEAM = false> struct Rollback_ : ControlState_<TEST_SEAM> {
            const EarlyExercisePdeRequest_& r_;
            const Vector_<CarryIntegral_>& carry_;
            const RollbackOutput_ out_;
            const size_t regionCapacity_;
            const double scale_;
            Scratch_ a_;

            Rollback_(const EarlyExercisePdeRequest_& r,
                      const Vector_<CarryIntegral_>& carry,
                      RollbackOutput_ output,
                      size_t regionCapacity,
                      const EarlyExercisePdeInternal::RollbackControl_* control)
                : ControlState_<TEST_SEAM>(control), r_(r), carry_(carry), out_(output), regionCapacity_(regionCapacity),
                  scale_(std::max(r.market_.spot_, r.contract_.strike_)), a_(r.settings_.gridPoints_) {}
            bool IsSuccess() const { return *out_.status_ == Status_::SUCCESS; }
            void Fail(Status_ status,
                      const EarlyExercisePdeStepDiagnostic_* step,
                      const String_& reason,
                      std::optional<int> row = std::nullopt,
                      std::optional<double> residual = std::nullopt,
                      std::optional<double> threshold = std::nullopt) {
                *out_.status_ = EarlyExercisePdeStatus_(status);
                *out_.failure_ =
                    EarlyExercisePdeFailure_{*out_.status_, step->timeIndex_, step->solveCount_, out_.timeGrid_[step->timeIndex_].calendarDate_,
                                             row,           residual,         threshold,         out_.execution_->workUnits_,
                                             reason};
            }
            void PrepareBoundaries(const PlanStep_& p, EarlyExercisePdeStepDiagnostic_* step) {
                try {
                    a_.current_.front() = Boundary(r_, carry_, p.target_, 0.0);
                    a_.current_.back() = Boundary(r_, carry_, p.target_, *r_.settings_.upper_);
                } catch (const Exception_& error) {
                    Fail(Status_::NONFINITE, step, String_(error.what()));
                }
            }
            double RowRhs(int i,
                          const Coefficients_& coeff,
                          const StepOperator_& op,
                          double diagonal,
                          double explicitDiagonal,
                          const AssemblyArrays_& rows) const {
                double rhs = explicitDiagonal * rows.values_[i] +
                             (1.0 - op.theta_) * op.dt_ * (coeff.minus_ * rows.values_[i - 1] + coeff.plus_ * rows.values_[i + 1]);
                rhs /= diagonal;
                if (i == 1)
                    rhs -= rows.minus_[i] * rows.current_[0];
                if (i == op.nodes_ - 2)
                    rhs -= rows.plus_[i] * rows.current_[op.nodes_ - 1];
                return rhs;
            }
            double Correction(int i, double rhs, int n, const AssemblyArrays_& rows) const {
                return rhs - rows.payoff_[i] - (i > 1 ? rows.minus_[i] * rows.payoff_[i - 1] : 0.0) -
                       (i < n - 2 ? rows.plus_[i] * rows.payoff_[i + 1] : 0.0);
            }
            void InitializeRow(int i, double rhs, int n, const AssemblyArrays_* rows) {
                rows->rhs_[i] = rhs;
                rows->correction_[i] = AMERICAN ? Correction(i, rhs, n, *rows) : rhs;
                rows->current_[i] = AMERICAN ? std::max(rows->values_[i], rows->payoff_[i]) : rows->values_[i];
                rows->iterate_[i] = rows->current_[i] - (AMERICAN ? rows->payoff_[i] : 0.0);
            }
            static bool Monotone(double diagonal, double explicitDiagonal, double discountDiagonal) {
                return diagonal > 0.0 && explicitDiagonal >= 0.0 && discountDiagonal > 0.0;
            }
            static bool FiniteRow(double diagonal, double rhs, double correction) {
                return std::isfinite(diagonal) && std::isfinite(rhs) && std::isfinite(correction);
            }
            bool AssembleRow(int i, const StepOperator_& op, const AssemblyArrays_* rows, EarlyExercisePdeStepDiagnostic_* step) {
                const auto coeff = Coefficients(op.volatility_, op.rate_, op.yield_, i);
                const double diagonal = 1.0 + op.theta_ * op.dt_ * (coeff.minus_ + coeff.plus_ + op.rate_);
                const double explicitDiagonal = 1.0 - (1.0 - op.theta_) * op.dt_ * (coeff.minus_ + coeff.plus_ + op.rate_);
                if (!Monotone(diagonal, explicitDiagonal, 1.0 + op.theta_ * op.dt_ * op.rate_)) {
                    Fail(Status_::NON_MONOTONE_OPERATOR, step, "matrix sufficient monotonicity conditions failed", i);
                    return false;
                }
                rows->minus_[i] = -op.theta_ * op.dt_ * coeff.minus_ / diagonal;
                rows->plus_[i] = -op.theta_ * op.dt_ * coeff.plus_ / diagonal;
                const double rhs = RowRhs(i, coeff, op, diagonal, explicitDiagonal, *rows);
                InitializeRow(i, rhs, op.nodes_, rows);
                if (!FiniteRow(diagonal, rhs, rows->correction_[i])) {
                    Fail(Status_::NONFINITE, step, "nonfinite assembled row", i);
                    return false;
                }
                return true;
            }
            // Keep allocation/alias information visible across the row assembly loop.
            FORCE_INLINE void Assemble(const PlanStep_& p, EarlyExercisePdeStepDiagnostic_* step) {
                PrepareBoundaries(p, step);
                if (!IsSuccess())
                    return;
                const int n = r_.settings_.gridPoints_;
                const auto& segment = r_.market_.carry_[p.carry_];
                const StepOperator_ op{r_.market_.volatility_, segment.rate_, segment.dividendYield_, p.theta_, step->dt_, n};
                const AssemblyArrays_ rows{&(*out_.values_)[0], &a_.payoff_[0],     &a_.minus_[0],   &a_.plus_[0],
                                           &a_.rhs_[0],         &a_.correction_[0], &a_.current_[0], &a_.iterate_[0]};
                for (int i = 1; i < n - 1; ++i) {
                    if (!AssembleRow(i, op, &rows, step))
                        break;
                }
                if (AMERICAN && IsSuccess() && !Residuals(a_, scale_, step))
                    Fail(Status_::NONFINITE, step, "nonfinite original LCP residual");
            }
            void ScaleIterates() {
                const int n = r_.settings_.gridPoints_;
                double magnitude = 0.0;
                for (int i = 1; i < static_cast<int>(n) - 1; ++i)
                    magnitude = std::max({magnitude, std::abs(a_.rhs_[i]), std::abs(a_.correction_[i]), a_.payoff_[i]});
                int exponent = 0;
                std::frexp(magnitude, &exponent);
                // Uniform powers of two cancel in physical backward errors and protect tiny diffusion tails.
                a_.scale_ = std::ldexp(1.0, std::clamp(800 - exponent, -1000, 900));
                if (a_.extendedRange_) {
                    std::fill(a_.iterateExponents_.begin(), a_.iterateExponents_.end(), 0);
                    std::fill(a_.previousExponents_.begin(), a_.previousExponents_.end(), 0);
                    a_.extendedRange_ = false;
                }
                for (int i = 1; i < static_cast<int>(n) - 1; ++i)
                    a_.iterate_[i] *= a_.scale_;
            }
            template <bool EXTENDED> void UpdateIterationChange(EarlyExercisePdeStepDiagnostic_* step) {
                const int n = r_.settings_.gridPoints_;
                step->iterationChange_ = 0.0;
                for (int i = 1; i < static_cast<int>(n) - 1; ++i) {
                    if (!std::isfinite(a_.current_[i]) || std::abs(a_.current_[i]) > (std::numeric_limits<double>::max)()) {
                        Fail(Status_::NONFINITE, step, "nonfinite linear solution", i);
                        break;
                    }
                    double change;
                    if constexpr (EXTENDED)
                        change = RangeValue_(a_.iterate_[i], a_.iterateExponents_[i])
                                     .Minus(RangeValue_(a_.previous_[i], a_.previousExponents_[i]))
                                     .DividedBy(a_.scale_)
                                     .Value();
                    else
                        change = (a_.iterate_[i] - a_.previous_[i]) / a_.scale_;
                    step->iterationChange_ = std::max(step->iterationChange_, std::abs(change));
                }
            }
            void CheckSolveResiduals(double penalty, EarlyExercisePdeStepDiagnostic_* step) {
                auto measured = a_.extendedRange_ ? BackwardError<true>(a_, penalty, AMERICAN) : BackwardError<false>(a_, penalty, AMERICAN);
                if (!a_.extendedRange_ && measured.second) {
                    ExtendSolveRange(&a_, AMERICAN);
                    UpdateIterationChange<true>(step);
                    if (!IsSuccess())
                        return;
                    measured = BackwardError<true>(a_, penalty, AMERICAN);
                }
                const double error = measured.first;
                MaxOptional(&step->linearBackwardError_, error);
                if (AMERICAN && !Residuals(a_, scale_, step))
                    Fail(Status_::NONFINITE, step, "nonfinite original LCP residual");
                if (!std::isfinite(error) || error > r_.settings_.linearBackwardErrorLimit_)
                    Fail(Status_::LINEAR_SOLVE_FAILURE, step, "linear backward-error limit exceeded", std::nullopt, error,
                         r_.settings_.linearBackwardErrorLimit_);
            }
            bool SolveOnce(EarlyExercisePdeStepDiagnostic_* step) {
                const size_t n = a_.current_.size();
                auto& e = *out_.execution_;
                if (n - 2 > r_.settings_.maxWorkUnits_ - e.workUnits_) {
                    Fail(Status_::WORK_LIMIT, step, "max_work_units exhausted before linear solve");
                    return false;
                }
                e.workUnits_ += n - 2;
                ++e.totalSolves_;
                ++step->solveCount_;
                std::copy(a_.iterate_.begin(), a_.iterate_.end(), a_.previous_.begin());
                if (a_.extendedRange_)
                    std::copy(a_.iterateExponents_.begin(), a_.iterateExponents_.end(), a_.previousExponents_.begin());
                const double penalty = AMERICAN ? r_.settings_.penaltyFactor_ : 0.0;
                const auto pivot = a_.extendedRange_ ? Solve<true>(&a_, penalty, AMERICAN) : Solve<false>(&a_, penalty, AMERICAN);
                if (pivot) {
                    Fail(Status_::LINEAR_SOLVE_FAILURE, step, "nonpositive or nonfinite pivot", pivot);
                    return false;
                }
                if (a_.extendedRange_)
                    UpdateIterationChange<true>(step);
                else
                    UpdateIterationChange<false>(step);
                if (!IsSuccess())
                    return false;
                CheckSolveResiduals(penalty, step);
                if constexpr (TEST_SEAM)
                    if (this->control_->failFirstSolve_ && e.totalSolves_ == 1) {
                        step->linearBackwardError_ = 2.0 * r_.settings_.linearBackwardErrorLimit_;
                        Fail(Status_::LINEAR_SOLVE_FAILURE, step, "private solver test seam", std::nullopt, step->linearBackwardError_,
                             r_.settings_.linearBackwardErrorLimit_);
                    }
                return true;
            }
            bool NeedsSolve(const EarlyExercisePdeStepDiagnostic_& step) const {
                return !AMERICAN ? step.solveCount_ == 0 : !Admitted(step, out_.execution_->epsilon_);
            }
            void CheckConvergence(EarlyExercisePdeStepDiagnostic_* step) {
                if (IsSuccess() && AMERICAN && !Admitted(*step, out_.execution_->epsilon_))
                    Fail(step->iterationChange_ <= out_.execution_->epsilon_ ? Status_::LCP_RESIDUAL_LIMIT : Status_::MAX_ITERATIONS, step,
                         "original LCP or iteration-change tolerance not reached", std::nullopt, step->complementarityResidual_,
                         out_.execution_->epsilon_);
            }
            void SolveStep(EarlyExercisePdeStepDiagnostic_* step) {
                const int limit = AMERICAN ? r_.settings_.maxIterationsPerStep_ : 1;
                while (IsSuccess() && NeedsSolve(*step) && step->solveCount_ < limit) {
                    if (!SolveOnce(step))
                        break;
                    if constexpr (TEST_SEAM)
                        if (IsSuccess() && this->control_->extraFirstSolve_ && out_.execution_->totalSolves_ == 1)
                            SolveOnce(step);
                }
                CheckConvergence(step);
            }
            void Project(EarlyExercisePdeStepDiagnostic_* step) {
                const size_t n = a_.current_.size();
                std::copy(a_.current_.begin(), a_.current_.end(), a_.previous_.begin());
                double change = 0.0, residual = 0.0, obstacle = 0.0;
                for (size_t i = 0; i < n; ++i) {
                    const double projected = std::max(a_.current_[i], a_.payoff_[i]);
                    change = std::max(change, static_cast<double>(projected - a_.current_[i]));
                    a_.current_[i] = projected;
                    residual = std::max(residual, std::abs(std::max(a_.previous_[i], a_.payoff_[i]) - a_.current_[i]));
                    obstacle = std::max(obstacle, a_.payoff_[i] - a_.current_[i]);
                }
                step->projectionChange_ = change;
                step->projectionResidual_ = residual;
                step->obstacleViolation_ = obstacle;
                step->normalizedObstacleViolation_ = obstacle / scale_;
            }
            bool Contact(int i) const {
                const int n = r_.settings_.gridPoints_;
                if (a_.payoff_[i] <= 0.0 || std::abs(a_.current_[i] - a_.payoff_[i]) > out_.execution_->epsilon_)
                    return false;
                if (i == 0 || i == static_cast<int>(n) - 1)
                    return true;
                return AMERICAN ? Dual(a_, i) >= -out_.execution_->epsilon_ : a_.previous_[i] <= a_.payoff_[i] + out_.execution_->epsilon_;
            }
            void RecordExercise(EarlyExercisePdeStepDiagnostic_* step) {
                const int target = step->timeIndex_, n = r_.settings_.gridPoints_;
                EarlyExercisePdeExerciseLevel_ level{target,
                                                     out_.timeGrid_[target].calendarDate_,
                                                     EarlyExercisePdeLevelKind_(AMERICAN ? Kind_::AMERICAN_LCP : Kind_::BERMUDAN_PROJECTION),
                                                     (*out_.regions_).size(),
                                                     0,
                                                     out_.execution_->epsilon_};
                for (int i = 0; i < static_cast<int>(n); ++i) {
                    if (!Contact(i))
                        continue;
                    const int first = i;
                    while (i + 1 < static_cast<int>(n) && Contact(i + 1))
                        ++i;
                    REQUIRE((*out_.regions_).size() < regionCapacity_, "EarlyExercisePde: region capacity invariant");
                    (*out_.regions_).push_back({target, static_cast<int>(level.regionCount_), first, i, first == 0, i + 1 == static_cast<int>(n)});
                    ++level.regionCount_;
                }
                (*out_.levels_).push_back(level);
            }
            void CompleteStep(EarlyExercisePdeStepDiagnostic_* step) {
                if (!IsSuccess())
                    return;
                step->completed_ = true;
                ++out_.execution_->completedSteps_;
                if (AMERICAN || step->projection_)
                    RecordExercise(step);
                for (size_t i = 0; i < a_.current_.size(); ++i)
                    (*out_.values_)[i] = static_cast<double>(a_.current_[i]);
            }
            // Keep the step kernel separate from request setup to limit register pressure.
#if defined(_MSC_VER)
            __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
            __attribute__((noinline))
#endif
            EarlyExercisePdeStepDiagnostic_ RunStep(const PlanStep_& p, int rollbackStep, int target) {
                EarlyExercisePdeStepDiagnostic_ step;
                step.rollbackStep_ = rollbackStep;
                step.timeIndex_ = target;
                step.sourceTimeIndex_ = target + 1;
                step.dt_ = p.source_ - p.target_;
                step.theta_ = p.theta_;
                step.damped_ = p.damped_;
                step.projection_ = !AMERICAN && out_.timeGrid_[target].isExerciseDate_;
                ++out_.execution_->actualSteps_;
                Assemble(p, &step);
                ScaleIterates();
                SolveStep(&step);
                if (IsSuccess() && step.projection_)
                    Project(&step);
                CompleteStep(&step);
                auto& e = *out_.execution_;
                e.maxIterations_ = std::max(e.maxIterations_, step.solveCount_);
                MaxOptional(&e.maxLinearBackwardError_, step.linearBackwardError_);
                MaxOptional(&e.maxObstacleViolation_, step.obstacleViolation_);
                MaxOptional(&e.maxDualViolation_, step.dualViolation_);
                MaxOptional(&e.maxComplementarityResidual_, step.complementarityResidual_);
                return step;
            }
        };
        Vector_<CarryIntegral_> CarryIntegrals(const EarlyExercisePdeRequest_& r) {
            Vector_<CarryIntegral_> carry;
            carry.reserve(r.market_.carry_.size());
            double start = 0.0, rate = 0.0, yield = 0.0;
            for (const auto& x : r.market_.carry_) {
                const double end = Years(x.endDate_, r.contract_.evaluationDate_);
                rate += (end - start) * x.rate_;
                yield += (end - start) * x.dividendYield_;
                carry.push_back({end, x.rate_, x.dividendYield_, rate, yield});
                start = end;
            }
            return carry;
        }
        EarlyExercisePdeTimeNode_ TimeNode(const EarlyExercisePdeRequest_& r, double t, std::optional<Date_> date) {
            const bool american = American(r);
            EarlyExercisePdeTimeNode_ x;
            x.timeYears_ = t;
            x.calendarDate_ = date;
            x.isExerciseDate_ = american;
            if (date) {
                x.isMaturity_ = *date == r.contract_.maturityDate_;
                x.isExerciseDate_ = american || std::binary_search(r.contract_.exerciseDates_.begin(), r.contract_.exerciseDates_.end(), *date);
                x.isCarryKnot_ =
                    std::any_of(r.market_.carry_.begin(), r.market_.carry_.end(), [&](const auto& segment) { return segment.endDate_ == *date; });
            }
            return x;
        }
        void InitializeTimeGrid(const EarlyExercisePdeRequest_& r, const Vector_<PlanStep_>& plan, Vector_<EarlyExercisePdeTimeNode_>* timeGrid) {
            const size_t m = plan.size();
            for (size_t k = 0; k < m; ++k)
                (*timeGrid)[m - 1 - k] = TimeNode(r, plan[k].target_, plan[k].date_);
            (*timeGrid)[m] = TimeNode(r, Years(r.contract_.maturityDate_, r.contract_.evaluationDate_), r.contract_.maturityDate_);
        }
        void InitializeGrid(const EarlyExercisePdeRequest_& r, Vector_<>* grid, Vector_<>* values, Scratch_* a) {
            const size_t n = grid->size();
            for (size_t i = 0; i < n; ++i) {
                (*grid)[i] = i + 1 == n ? *r.settings_.upper_ : *r.settings_.upper_ * (double(i) / (n - 1));
                const double intrinsic = (*grid)[i] - r.contract_.strike_;
                a->payoff_[i] = std::max(r.contract_.optionType_ == OptionType_::Value_::CALL ? intrinsic : -intrinsic, 0.0);
                (*values)[i] = a->payoff_[i];
            }
        }
        template <bool AMERICAN, bool TEST_SEAM = false>
        FORCE_INLINE void RunRollback(const EarlyExercisePdeRequest_& r,
                                      const Vector_<CarryIntegral_>& carry,
                                      const Vector_<PlanStep_>& plan,
                                      const PlanCounts_& counts,
                                      RollbackOutput_ output,
                                      Vector_<>* grid,
                                      Vector_<EarlyExercisePdeStepDiagnostic_>* steps,
                                      size_t regions,
                                      const EarlyExercisePdeInternal::RollbackControl_* control = nullptr) {
            const size_t m = plan.size();
            Rollback_<AMERICAN, TEST_SEAM> rollback(r, carry, output, regions, control);
            InitializeGrid(r, grid, output.values_, &rollback.a_);
            constexpr size_t fixedWorkspace = sizeof(Rollback_<AMERICAN, TEST_SEAM>) + sizeof(plan) + sizeof(carry) + sizeof(counts) +
                                              sizeof(PlanStep_) + sizeof(EarlyExercisePdeStepDiagnostic_) + sizeof(EarlyExercisePdeExerciseLevel_) +
                                              sizeof(EarlyExercisePdeFailure_) + sizeof(Coefficients_) + 32 * sizeof(double) + 16 * sizeof(int);
            static_assert(fixedWorkspace <= 4096);
            output.execution_->peakWorkspaceBytes_ = fixedWorkspace + rollback.a_.Bytes() + CapacityBytes(plan) + CapacityBytes(carry);
            Budget(output.execution_->peakWorkspaceBytes_, output.execution_->plannedWorkspaceBytes_, "workspace_storage_layout");
            output.levels_->push_back(
                {static_cast<int>(m), r.contract_.maturityDate_, EarlyExercisePdeLevelKind_(Kind_::TERMINAL), 0, 0, output.execution_->epsilon_});
            for (size_t k = 0; k < m; ++k) {
                steps->push_back(rollback.RunStep(plan[k], static_cast<int>(k + 1), static_cast<int>(m - 1 - k)));
                if (!rollback.IsSuccess())
                    break;
            }
        }
        double InterpolatedPrice(const Vector_<double>& grid, const Vector_<double>& values, double spot) {
            const auto it = std::lower_bound(grid.begin(), grid.end(), spot);
            const size_t high = static_cast<size_t>(it - grid.begin());
            const double weight = (spot - grid[high - 1]) / (grid[high] - grid[high - 1]);
            return weight == 1.0 ? values[high] : (1.0 - weight) * values[high - 1] + weight * values[high];
        }
    } // namespace

    static_assert(sizeof(EarlyExercisePdeCarrySegment_) <= 32 && sizeof(Date_) <= 8);
    static_assert(sizeof(EarlyExercisePdeTimeNode_) <= 64 && sizeof(PlanStep_) <= 64 && sizeof(CarryIntegral_) <= 64);
    static_assert(sizeof(EarlyExercisePdeStepDiagnostic_) <= 256 && sizeof(EarlyExercisePdeExerciseLevel_) <= 64);
    static_assert(sizeof(EarlyExercisePdeExerciseRegion_) <= 32 && sizeof(EarlyExercisePdeResult_) <= 4096);

    namespace EarlyExercisePdeInternal {
        namespace {
            struct CanonicalWriter_ {
                std::string* output_;
                size_t bytes_ = 0;
                void Integer(uint64_t value, int width = 8) {
                    bytes_ = Add(bytes_, width);
                    if (output_)
                        for (int i = 0; i < width; ++i)
                            output_->push_back(static_cast<char>((value >> (8 * i)) & 0xff));
                }
                void Real(double value) {
                    static_assert(sizeof(double) == sizeof(uint64_t) && std::numeric_limits<double>::is_iec559);
                    uint64_t bits = 0;
                    if (value != 0.0)
                        std::memcpy(&bits, &value, sizeof(bits));
                    Integer(bits);
                }
                void Date(std::optional<Date_> date) {
                    Integer(date.has_value(), 1);
                    if (date)
                        Integer(static_cast<uint64_t>(static_cast<int64_t>(Dal::Date::ToExcel(*date))));
                }
                void Text(std::string_view value) {
                    const size_t length = value.size();
                    Integer(length);
                    bytes_ = Add(bytes_, length);
                    if (output_)
                        output_->append(value.data(), length);
                }
            };
            size_t CanonicalPlan(const Plan_& plan, std::string* output) {
                CanonicalWriter_ w{output};
                const auto& r = plan.Request();
                const auto& s = r.settings_;
                w.Integer(1, 4);
                w.Date(r.contract_.evaluationDate_);
                w.Date(r.contract_.maturityDate_);
                w.Real(plan.Upper());
                w.Integer(s.gridPoints_);
                w.Real(plan.Upper() / (s.gridPoints_ - 1));
                w.Integer(plan.Grid().size());
                for (double node : plan.Grid())
                    w.Real(node);
                w.Integer(plan.TimeGrid().size());
                for (const auto& node : plan.TimeGrid()) {
                    w.Real(node.timeYears_);
                    w.Date(node.calendarDate_);
                    w.Integer(node.isExerciseDate_, 1);
                    w.Integer(node.isCarryKnot_, 1);
                    w.Integer(node.isMaturity_, 1);
                }
                w.Integer(plan.Steps().size());
                for (size_t i = 0; i < plan.Steps().size(); ++i) {
                    const auto& step = plan.Steps()[plan.Steps().size() - 1 - i];
                    w.Integer(i + 1);
                    w.Integer(i);
                    w.Real(step.source_ - step.target_);
                    w.Real(step.theta_);
                    w.Integer(step.carry_);
                    w.Integer(step.damped_, 1);
                    w.Integer(!American(r) && plan.TimeGrid()[i].isExerciseDate_, 1);
                }
                w.Real(plan.Epsilon());
                w.Real(s.theta_);
                w.Integer(s.theta_ == 0.5 ? s.rannacherIntervals_ : 0);
                w.Real(s.penaltyFactor_);
                w.Integer(s.maxIterationsPerStep_);
                w.Real(s.linearBackwardErrorLimit_);
                w.Text("LINEAR_SPOT");
                return w.bytes_;
            }
            EarlyExercisePdeRequest_ ResolvedScenario(const EarlyExercisePdeRequest_& input, double upper) {
                auto result = input;
                result.settings_.upper_ = upper;
                ValidateEarlyExercisePdeRequest(result);
                return result;
            }
            void CheckCarryLayout(const EarlyExercisePdeMarket_& market, const EarlyExercisePdeMarket_& base) {
                Check(market.carry_.size() == base.carry_.size(), "plan.carry", "scenario must preserve carry layout");
                for (size_t i = 0; i < market.carry_.size(); ++i)
                    Check(market.carry_[i].endDate_ == base.carry_[i].endDate_, "plan.carry", "scenario must preserve carry dates");
            }
            void CheckPlanContract(const EarlyExercisePdeContract_& c, const EarlyExercisePdeContract_& b) {
                Check(c.evaluationDate_ == b.evaluationDate_ && c.maturityDate_ == b.maturityDate_ && c.strike_ == b.strike_ &&
                          c.optionType_ == b.optionType_ && c.exerciseStyle_ == b.exerciseStyle_ && c.exerciseDates_ == b.exerciseDates_,
                      "plan.contract", "scenario must preserve contract");
            }
            void CheckPlanSettings(const EarlyExercisePdeSettings_& s, const EarlyExercisePdeSettings_& t) {
                Check(s.gridPoints_ == t.gridPoints_ && s.ordinarySteps_ == t.ordinarySteps_ && s.theta_ == t.theta_ &&
                          s.rannacherIntervals_ == t.rannacherIntervals_,
                      "plan.settings", "scenario must preserve grid settings");
                Check(s.penaltyFactor_ == t.penaltyFactor_ && s.absoluteTolerance_ == t.absoluteTolerance_ &&
                          s.relativeTolerance_ == t.relativeTolerance_ && s.maxIterationsPerStep_ == t.maxIterationsPerStep_ &&
                          s.linearBackwardErrorLimit_ == t.linearBackwardErrorLimit_,
                      "plan.settings", "scenario must preserve numerical settings");
            }
            void CheckPlanRequest(const EarlyExercisePdeRequest_& r, const EarlyExercisePdeRequest_& base) {
                CheckPlanContract(r.contract_, base.contract_);
                CheckPlanSettings(r.settings_, base.settings_);
                CheckCarryLayout(r.market_, base.market_);
            }
            void CheckFixedPlan(const EarlyExercisePdeRequest_& input, const Plan_& plan) {
                Check(!input.settings_.upper_ || *input.settings_.upper_ == plan.Upper(), "plan.upper", "scenario must use fixed upper");
                const auto r = ResolvedScenario(input, plan.Upper());
                CheckPlanRequest(r, plan.Request());
                Check(plan.Epsilon() <=
                          r.settings_.absoluteTolerance_ + r.settings_.relativeTolerance_ * std::max(r.market_.spot_, r.contract_.strike_),
                      "plan.epsilon", "fixed tolerance must not relax scenario tolerance");
            }
            PlanCounts_ ScenarioCounts(const EarlyExercisePdeRequest_& r, const Plan_* fixed) {
                if (!fixed) {
                    ValidateEarlyExercisePdeRequest(r);
                    return Plan(r, [](const PlanStep_&) {});
                }
                auto counts = fixed->Counts();
                counts.upwind_ = 0;
                for (size_t i = 0; i < r.market_.carry_.size(); ++i)
                    SegmentDiagonal(r.market_, r.settings_.gridPoints_, static_cast<int>(i), &counts);
                return counts;
            }
            EarlyExercisePdeRequest_ ResultRequest(const EarlyExercisePdeRequest_& input, const Plan_* fixed) {
                auto r = input;
                r.settings_.upper_ = fixed ? fixed->Upper() : Upper(input);
                if (fixed)
                    ValidateEarlyExercisePdeRequest(r);
                return r;
            }
            EarlyExercisePdeExecution_
            Execution(const EarlyExercisePdeRequest_& r, const PlanCounts_& counts, const StorageLayout_& layout, double epsilon) {
                EarlyExercisePdeExecution_ e;
                e.plannedSteps_ = counts.steps_;
                e.baseSteps_ = counts.base_;
                e.dampingAddedSteps_ = counts.damping_;
                e.stabilityAddedSteps_ = counts.steps_ - counts.base_ - counts.damping_;
                e.upwindRows_ = counts.upwind_;
                e.effectiveRannacherIntervals_ = r.settings_.theta_ == 0.5 ? r.settings_.rannacherIntervals_ : 0;
                e.plannedRegionCapacity_ = layout.regions_;
                e.plannedNumericPayloadBytes_ = layout.payload_;
                e.plannedWorkspaceBytes_ = layout.workspace_;
                e.epsilon_ = epsilon;
                return e;
            }
            Vector_<PlanStep_> SinglePlan(const EarlyExercisePdeRequest_& r, int steps, bool fixed) {
                Vector_<PlanStep_> plan;
                if (!fixed) {
                    plan.reserve(steps);
                    Plan(r, [&](const PlanStep_& step) { plan.push_back(step); });
                }
                return plan;
            }
        } // namespace

        PlanLayout_ PreviewPlan(const EarlyExercisePdeRequest_& r, const Vector_<EarlyExercisePdeMarket_>& extraMarkets) {
            const auto counts = Plan(r, [](const PlanStep_&) {}, extraMarkets);
            const auto storage = AdmitStorage(r, counts);
            const size_t n = r.settings_.gridPoints_, m = counts.steps_;
            const size_t plan = Add(Add(Add(Add(4096, Multiply(8, n)), Multiply(128, m + 1)), Multiply(32, r.market_.carry_.size())),
                                    Multiply(8, r.contract_.exerciseDates_.size()));
            const size_t canonical = Add(Add(256, Multiply(8, n)), Multiply(128, m + 1));
            return {counts, storage.payload_, storage.workspace_, plan, Add(4096, Multiply(4, Add(canonical, 72)))};
        }

        Plan_ BuildPlan(const EarlyExercisePdeRequest_& input, const Vector_<EarlyExercisePdeMarket_>& extraMarkets) {
            ValidateEarlyExercisePdeRequest(input);
            Plan_ result;
            result.request_ = ResolvedScenario(input, Upper(input));
            const auto& r = result.request_;
            result.epsilon_ = r.settings_.absoluteTolerance_ + r.settings_.relativeTolerance_ * std::max(r.market_.spot_, r.contract_.strike_);
            for (const auto& market : extraMarkets) {
                CheckCarryLayout(market, r.market_);
                auto scenario = r;
                scenario.market_ = market;
                ValidateEarlyExercisePdeRequest(scenario);
                result.epsilon_ = std::min(result.epsilon_, r.settings_.absoluteTolerance_ +
                                                                r.settings_.relativeTolerance_ * std::max(market.spot_, r.contract_.strike_));
            }
            result.basePlannedSteps_ = Plan(r, [](const PlanStep_&) {}).steps_;
            result.counts_ = Plan(r, [](const PlanStep_&) {}, extraMarkets);
            AdmitStorage(r, result.counts_);
            const size_t n = r.settings_.gridPoints_, m = result.counts_.steps_;
            const size_t envelope = Add(Add(Add(Add(4096, Multiply(8, n)), Multiply(128, m + 1)), Multiply(32, r.market_.carry_.size())),
                                        Multiply(8, r.contract_.exerciseDates_.size()));
            Budget(envelope, r.settings_.numericPayloadBudgetBytes_, "numeric_payload_budget_bytes");
            result.grid_.Resize(n);
            for (size_t i = 0; i < n; ++i)
                result.grid_[i] = i + 1 == n ? *r.settings_.upper_ : *r.settings_.upper_ * (double(i) / (n - 1));
            result.steps_.reserve(m);
            Plan(r, [&](const PlanStep_& step) { result.steps_.push_back(step); }, extraMarkets);
            result.timeGrid_.Resize(m + 1);
            InitializeTimeGrid(r, result.steps_, &result.timeGrid_);
            result.canonicalBytes_ = CanonicalPlan(result, nullptr);
            result.canonicalWorkspaceBytes_ = Add(4096, Multiply(4, Add(result.canonicalBytes_, 72)));
            Budget(result.canonicalWorkspaceBytes_, r.settings_.workspaceBudgetBytes_, "workspace_budget_bytes");
            std::string canonical;
            canonical.reserve(result.canonicalBytes_);
            Budget(Add(4096, Add(canonical.capacity(), Multiply(2, Add(result.canonicalBytes_, 72)))), result.canonicalWorkspaceBytes_,
                   "canonical_storage_layout");
            CanonicalPlan(result, &canonical);
            size_t hashPeak = 0;
            result.id_ = "p3v1:" + CanonicalJson::FingerprintCanonical(canonical, &hashPeak);
            result.storageBytes_ =
                Add(Add(Add(Add(Add(Add(sizeof(result), CapacityBytes(r.market_.carry_)), CapacityBytes(r.contract_.exerciseDates_)),
                                CapacityBytes(result.grid_)),
                            CapacityBytes(result.timeGrid_)),
                        CapacityBytes(result.steps_)),
                    Add(result.id_.capacity(), 1));
            Budget(result.storageBytes_, envelope, "plan_storage_layout");
            const size_t canonicalPeak = Add(Add(canonical.capacity(), 1), hashPeak);
            result.peakTemporaryBytes_ = Add(result.storageBytes_ - Add(result.id_.capacity(), 1), canonicalPeak);
            return result;
        }

        struct ResultAccess_ {
            static EarlyExercisePdeResult_ Run(const EarlyExercisePdeRequest_& input, const Plan_* fixed, const RollbackControl_* control = nullptr);
            static void SanitizePrice(EarlyExercisePdeResult_* result, const RollbackControl_* control);
        };
    } // namespace EarlyExercisePdeInternal

    EarlyExercisePdeResult_
    EarlyExercisePdeInternal::ResultAccess_::Run(const EarlyExercisePdeRequest_& input, const Plan_* fixed, const RollbackControl_* control) {
        const auto counts = ScenarioCounts(input, fixed);
        Budget(counts.steps_, input.settings_.maxActualSteps_, "max_actual_steps");
        const size_t n = input.settings_.gridPoints_, m = counts.steps_;
        const bool american = American(input);
        const auto layout = AdmitStorage(input, counts);
        const size_t levels = layout.levels_, regions = layout.regions_, payload = layout.payload_;
        EarlyExercisePdeResult_ result;
        result.request_ = ResultRequest(input, fixed);
        const auto& r = result.request_;
        const auto& s = r.settings_;
        auto& e = result.execution_;
        const double scale = std::max(r.market_.spot_, r.contract_.strike_);
        e = Execution(r, counts, layout, fixed ? fixed->Epsilon() : s.absoluteTolerance_ + s.relativeTolerance_ * scale);
        const auto singlePlan = SinglePlan(r, counts.steps_, fixed != nullptr);
        const auto& plan = fixed ? fixed->steps_ : singlePlan;
        const auto carry = CarryIntegrals(r);
        result.grid_.Resize(n);
        result.values_.Resize(n);
        result.timeGrid_.Resize(m + 1);
        result.steps_.reserve(m);
        result.levels_.reserve(levels);
        result.regions_.reserve(regions);
        e.regionCapacity_ = result.regions_.capacity();
        InitializeTimeGrid(r, plan, &result.timeGrid_);
        const RollbackOutput_ output{result.timeGrid_, &result.values_, &result.levels_, &result.regions_, &e, &result.status_, &result.failure_};
        if (american && control)
            RunRollback<true, true>(r, carry, plan, counts, output, &result.grid_, &result.steps_, regions, control);
        else if (control)
            RunRollback<false, true>(r, carry, plan, counts, output, &result.grid_, &result.steps_, regions, control);
        else if (american)
            RunRollback<true>(r, carry, plan, counts, output, &result.grid_, &result.steps_, regions);
        else
            RunRollback<false>(r, carry, plan, counts, output, &result.grid_, &result.steps_, regions);
        std::reverse(result.steps_.begin(), result.steps_.end());
        std::reverse(result.levels_.begin(), result.levels_.end());
        if (result.IsSuccess())
            result.price_ = InterpolatedPrice(result.grid_, result.values_, r.market_.spot_);
        // The fixed plan owns its steps; only singlePlan belongs to this rollback.
        e.peakWorkspaceBytes_ -= CapacityBytes(plan) - CapacityBytes(singlePlan);
        e.numericPayloadBytes_ = sizeof(result) + CapacityBytes(r.market_.carry_) + CapacityBytes(r.contract_.exerciseDates_) +
                                 CapacityBytes(result.grid_) + CapacityBytes(result.values_) + CapacityBytes(result.timeGrid_) +
                                 CapacityBytes(result.steps_) + CapacityBytes(result.levels_) + CapacityBytes(result.regions_);
        Budget(e.numericPayloadBytes_, payload, "numeric_payload_storage_layout");
        return result;
    }

    EarlyExercisePdeResult_ EvaluateEarlyExercisePde(const EarlyExercisePdeRequest_& input) {
        return EarlyExercisePdeInternal::ResultAccess_::Run(input, nullptr);
    }

    EarlyExercisePdeResult_ EarlyExercisePdeInternal::Rollback(const EarlyExercisePdeRequest_& input, const Plan_& plan) {
        CheckFixedPlan(input, plan);
        return ResultAccess_::Run(input, &plan);
    }

    void EarlyExercisePdeInternal::ResultAccess_::SanitizePrice(EarlyExercisePdeResult_* result, const RollbackControl_* control) {
        if (!result->IsSuccess())
            return;
        if (control && control->nonfinitePrice_)
            result->price_ = std::numeric_limits<double>::infinity();
        if (result->price_ && std::isfinite(*result->price_))
            return;
        result->status_ = EarlyExercisePdeStatus_(EarlyExercisePdeStatus_::Value_::NONFINITE);
        result->price_.reset();
        result->failure_ = EarlyExercisePdeFailure_{result->status_,
                                                    0,
                                                    0,
                                                    result->request_.contract_.evaluationDate_,
                                                    {},
                                                    {},
                                                    {},
                                                    result->execution_.workUnits_,
                                                    "nonfinite interpolated price"};
    }

    EarlyExercisePdeResult_
    EarlyExercisePdeInternal::RollbackForRisk(const EarlyExercisePdeRequest_& input, const Plan_& plan, const RollbackControl_* control) {
        CheckFixedPlan(input, plan);
        auto result = ResultAccess_::Run(input, &plan, control);
        ResultAccess_::SanitizePrice(&result, control);
        return result;
    }

    double EarlyExercisePdeResult_::Price() const {
        REQUIRE(IsSuccess() && price_,
                String_("EarlyExercisePde; ") + status_.String() + "; price unavailable; " + (failure_ ? failure_->reason_ : String_()));
        return *price_;
    }
    const Vector_<double>& EarlyExercisePdeResult_::Values() const {
        REQUIRE(IsSuccess(),
                String_("EarlyExercisePde; ") + status_.String() + "; values unavailable; " + (failure_ ? failure_->reason_ : String_()));
        return values_;
    }
    String_ EarlyExercisePdeResult_::Method() const {
        return American(request_) ? "BlackScholesAmericanThetaPenalty" : "BlackScholesBermudanThetaProjection";
    }
} // namespace Dal::PDE
