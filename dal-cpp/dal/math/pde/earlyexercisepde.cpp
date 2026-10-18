#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>

#include <dal/math/pde/earlyexercisepde.hpp>

namespace Dal {
#include <dal/auto/MG_EarlyExercisePdeLevelKind_enum.inc>
#include <dal/auto/MG_EarlyExercisePdeStatus_enum.inc>
#include <dal/auto/MG_EarlyExerciseStyle_enum.inc>
} // namespace Dal

namespace Dal::PDE {
    namespace {
        using Status_ = EarlyExercisePdeStatus_::Value_;
        using Kind_ = EarlyExercisePdeLevelKind_::Value_;

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
        struct PlanStep_ {
            double target_, source_, theta_;
            int carry_;
            bool damped_;
            std::optional<Date_> date_;
        };
        struct PlanCounts_ {
            int steps_ = 0, base_ = 0, damping_ = 0, upwind_ = 0;
        };

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
        double SegmentDiagonal(const EarlyExercisePdeRequest_& r, int carry, PlanCounts_* counts) {
            const auto& segment = r.market_.carry_[carry];
            double maxDiagonal = segment.rate_;
            int upwind = 0;
            for (int i = 1; i < r.settings_.gridPoints_ - 1; ++i) {
                const auto a = Coefficients(r.market_.volatility_, segment.rate_, segment.dividendYield_, i);
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
        void
        EmitParts(const EarlyExercisePdeRequest_& r, PlanStep_ step, double maxDiagonal, std::optional<Date_> date, PlanCounts_* counts, F_ emit) {
            const int parts = step.damped_ ? 2 : 1;
            for (int part = 0; part < parts; ++part) {
                const double high = part == 0 ? step.source_ : step.target_ + (step.source_ - step.target_) * 0.5;
                const double low = part + 1 == parts ? step.target_ : step.target_ + (step.source_ - step.target_) * 0.5;
                const auto endpoint = part + 1 == parts ? date : std::nullopt;
                const size_t subdivisions = StableSubdivisions(low, high, step.theta_, r.market_.carry_[step.carry_].rate_, maxDiagonal,
                                                               counts->steps_, r.settings_.maxActualSteps_);
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
        template <class F_> PlanCounts_ Plan(const EarlyExercisePdeRequest_& r, F_ emit) {
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
                const double maxDiagonal = SegmentDiagonal(r, carry, &counts);
                const int base = IntervalSteps(r, left, right, &counts);
                const double lo = Years(left, c.evaluationDate_), hi = Years(right, c.evaluationDate_);
                for (int b = 0; b < base; ++b) {
                    const auto step = BaseStep(lo, hi, b, base, carry, &damping, &counts, s.theta_);
                    EmitParts(r, step, maxDiagonal, b + 1 == base ? std::optional<Date_>(left) : std::nullopt, &counts, emit);
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
        struct Scratch_ {
            Vector_<> minus_, plus_, rhs_, payoff_, correction_;
            Vector_<> current_, iterate_, previous_, pivots_, solveRhs_;
            double scale_ = 1.0;
            explicit Scratch_(size_t n)
                : minus_(n), plus_(n), rhs_(n), payoff_(n), correction_(n), current_(n), iterate_(n), previous_(n), pivots_(n), solveRhs_(n) {}
            [[nodiscard]] size_t Bytes() const {
                return sizeof(double) * (minus_.capacity() + plus_.capacity() + rhs_.capacity() + payoff_.capacity() + correction_.capacity()) +
                       sizeof(double) *
                           (current_.capacity() + iterate_.capacity() + previous_.capacity() + pivots_.capacity() + solveRhs_.capacity());
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

        FORCE_INLINE void BackSubstitute(Scratch_* a, bool shifted) {
            const int n = static_cast<int>(a->current_.size());
            for (int i = n - 2; i > 0; --i) {
                a->iterate_[i] = (a->solveRhs_[i] - (i < n - 2 ? a->plus_[i] * a->iterate_[i + 1] : 0.0)) / a->pivots_[i];
                a->current_[i] = (shifted ? a->payoff_[i] : 0.0) + a->iterate_[i] / a->scale_;
            }
        }
        // The exercise style fixes penalty/shift constants across both triangular sweeps.
        FORCE_INLINE std::optional<int> Solve(Scratch_* a, double penalty, bool shifted) {
            const int n = static_cast<int>(a->current_.size());
            for (int i = 1; i < n - 1; ++i) {
                const double diagonal = 1.0 + (a->previous_[i] < 0.0 ? penalty : 0.0);
                a->pivots_[i] = diagonal;
                a->solveRhs_[i] = a->correction_[i] * a->scale_;
                if (i > 1) {
                    const double factor = a->minus_[i] / a->pivots_[i - 1];
                    a->pivots_[i] -= factor * a->plus_[i - 1];
                    a->solveRhs_[i] -= factor * a->solveRhs_[i - 1];
                }
                if (!std::isfinite(a->pivots_[i]) || a->pivots_[i] <= 0.0)
                    return i;
            }
            BackSubstitute(a, shifted);
            return std::nullopt;
        }
        double RelativeBackwardError(double numerator, double denominator) {
            if (!std::isfinite(numerator) || !std::isfinite(denominator))
                return std::numeric_limits<double>::infinity();
            return denominator == 0.0 ? (numerator == 0.0 ? 0.0 : std::numeric_limits<double>::infinity()) : numerator / denominator;
        }
        FORCE_INLINE double BackwardError(const Scratch_& a, double penalty, bool shifted) {
            double maximum = 0.0;
            for (int i = 1; i + 1 < static_cast<int>(a.current_.size()); ++i) {
                const double lambda = a.previous_[i] < 0.0 ? penalty : 0.0;
                const auto value = [&](int j) { return (shifted ? a.payoff_[j] * a.scale_ : 0.0) + a.iterate_[j]; };
                const double rhs = a.rhs_[i] * a.scale_ + lambda * (a.payoff_[i] * a.scale_);
                const double center = (1.0 + lambda) * value(i);
                const double left = i > 1 ? a.minus_[i] * value(i - 1) : 0.0;
                const double right = i + 2 < static_cast<int>(a.current_.size()) ? a.plus_[i] * value(i + 1) : 0.0;
                const double numerator = std::abs(((center + left) + right) - rhs);
                const double denominator = std::abs(rhs) + std::abs(center) + std::abs(left) + std::abs(right);
                const double error = RelativeBackwardError(numerator, denominator);
                maximum = std::max(maximum, error);
            }
            return maximum;
        }
        template <class T_> size_t CapacityBytes(const Vector_<T_>& x) { return Multiply(x.capacity(), sizeof(T_)); }
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
        template <bool AMERICAN> struct Rollback_ {
            const EarlyExercisePdeRequest_& r_;
            const Vector_<CarryIntegral_>& carry_;
            const RollbackOutput_ out_;
            const size_t regionCapacity_;
            const double scale_;
            Scratch_ a_;

            Rollback_(const EarlyExercisePdeRequest_& r, const Vector_<CarryIntegral_>& carry, RollbackOutput_ output, size_t regionCapacity)
                : r_(r), carry_(carry), out_(output), regionCapacity_(regionCapacity), scale_(std::max(r.market_.spot_, r.contract_.strike_)),
                  a_(r.settings_.gridPoints_) {}
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
                for (int i = 1; i < static_cast<int>(n) - 1; ++i)
                    a_.iterate_[i] *= a_.scale_;
            }
            void UpdateIterationChange(EarlyExercisePdeStepDiagnostic_* step) {
                const int n = r_.settings_.gridPoints_;
                step->iterationChange_ = 0.0;
                for (int i = 1; i < static_cast<int>(n) - 1; ++i) {
                    if (!std::isfinite(a_.current_[i]) || std::abs(a_.current_[i]) > (std::numeric_limits<double>::max)()) {
                        Fail(Status_::NONFINITE, step, "nonfinite linear solution", i);
                        break;
                    }
                    step->iterationChange_ = std::max(step->iterationChange_, std::abs(a_.iterate_[i] - a_.previous_[i]) / a_.scale_);
                }
            }
            void CheckSolveResiduals(double penalty, EarlyExercisePdeStepDiagnostic_* step) {
                const double error = BackwardError(a_, penalty, AMERICAN);
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
                const double penalty = AMERICAN ? r_.settings_.penaltyFactor_ : 0.0;
                const auto pivot = Solve(&a_, penalty, AMERICAN);
                if (pivot) {
                    Fail(Status_::LINEAR_SOLVE_FAILURE, step, "nonpositive or nonfinite pivot", pivot);
                    return false;
                }
                UpdateIterationChange(step);
                if (!IsSuccess())
                    return false;
                CheckSolveResiduals(penalty, step);
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
        template <bool AMERICAN>
        FORCE_INLINE void RunRollback(const EarlyExercisePdeRequest_& r,
                                      const Vector_<CarryIntegral_>& carry,
                                      const Vector_<PlanStep_>& plan,
                                      const PlanCounts_& counts,
                                      RollbackOutput_ output,
                                      Vector_<>* grid,
                                      Vector_<EarlyExercisePdeStepDiagnostic_>* steps,
                                      size_t regions) {
            const size_t m = plan.size();
            Rollback_<AMERICAN> rollback(r, carry, output, regions);
            InitializeGrid(r, grid, output.values_, &rollback.a_);
            constexpr size_t fixedWorkspace = sizeof(Rollback_<AMERICAN>) + sizeof(plan) + sizeof(carry) + sizeof(counts) + sizeof(PlanStep_) +
                                              sizeof(EarlyExercisePdeStepDiagnostic_) + sizeof(EarlyExercisePdeExerciseLevel_) +
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

    EarlyExercisePdeResult_ EvaluateEarlyExercisePde(const EarlyExercisePdeRequest_& input) {
        ValidateEarlyExercisePdeRequest(input);
        const auto counts = Plan(input, [](const PlanStep_&) {});
        const size_t n = input.settings_.gridPoints_, m = counts.steps_, c = input.market_.carry_.size(), d = input.contract_.exerciseDates_.size();
        const bool american = American(input);
        const size_t levels = american ? Add(m, 1) : d;
        const size_t regions = Multiply(levels - 1, (n + 1) / 2);
        const size_t payload =
            Add(Add(Add(Add(Add(Add(Add(4096, Multiply(32, c)), Multiply(8, d)), Multiply(16, n)), Multiply(64, m + 1)), Multiply(256, m)),
                    Multiply(64, levels)),
                Multiply(32, regions));
        const size_t workspace = Add(Add(Add(Add(4096, Multiply(256, n)), Multiply(64, m + 1)), Multiply(64, c)), Multiply(16, d));
        Budget(payload, input.settings_.numericPayloadBudgetBytes_, "numeric_payload_budget_bytes");
        Budget(workspace, input.settings_.workspaceBudgetBytes_, "workspace_budget_bytes");
        EarlyExercisePdeResult_ result;
        result.request_ = input;
        result.request_.settings_.upper_ = Upper(input);
        const auto& r = result.request_;
        const auto& s = r.settings_;
        auto& e = result.execution_;
        e.plannedSteps_ = counts.steps_;
        e.baseSteps_ = counts.base_;
        e.dampingAddedSteps_ = counts.damping_;
        e.stabilityAddedSteps_ = counts.steps_ - counts.base_ - counts.damping_;
        e.upwindRows_ = counts.upwind_;
        e.effectiveRannacherIntervals_ = s.theta_ == 0.5 ? s.rannacherIntervals_ : 0;
        e.plannedRegionCapacity_ = regions;
        e.plannedNumericPayloadBytes_ = payload;
        e.plannedWorkspaceBytes_ = workspace;
        const double scale = std::max(r.market_.spot_, r.contract_.strike_);
        e.epsilon_ = s.absoluteTolerance_ + s.relativeTolerance_ * scale;
        Vector_<PlanStep_> plan;
        plan.reserve(m);
        Plan(r, [&](const PlanStep_& step) { plan.push_back(step); });
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
        if (american)
            RunRollback<true>(r, carry, plan, counts, output, &result.grid_, &result.steps_, regions);
        else
            RunRollback<false>(r, carry, plan, counts, output, &result.grid_, &result.steps_, regions);
        std::reverse(result.steps_.begin(), result.steps_.end());
        std::reverse(result.levels_.begin(), result.levels_.end());
        if (result.IsSuccess())
            result.price_ = InterpolatedPrice(result.grid_, result.values_, r.market_.spot_);
        e.numericPayloadBytes_ = sizeof(result) + CapacityBytes(r.market_.carry_) + CapacityBytes(r.contract_.exerciseDates_) +
                                 CapacityBytes(result.grid_) + CapacityBytes(result.values_) + CapacityBytes(result.timeGrid_) +
                                 CapacityBytes(result.steps_) + CapacityBytes(result.levels_) + CapacityBytes(result.regions_);
        Budget(e.numericPayloadBytes_, payload, "numeric_payload_storage_layout");
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
