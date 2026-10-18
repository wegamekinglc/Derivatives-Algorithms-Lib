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
        template <class F_> PlanCounts_ Plan(const EarlyExercisePdeRequest_& r, F_ emit) {
            PlanCounts_ counts;
            const auto& c = r.contract_;
            const auto& s = r.settings_;
            Date_ right = c.maturityDate_;
            int carry = static_cast<int>(r.market_.carry_.size()) - 1;
            int exercise = static_cast<int>(c.exerciseDates_.size()) - 1;
            int damping = s.theta_ == 0.5 ? s.rannacherIntervals_ : 0;
            while (right > c.evaluationDate_) {
                while (exercise >= 0 && c.exerciseDates_[exercise] >= right)
                    --exercise;
                while (carry > 0 && r.market_.carry_[carry - 1].endDate_ >= right)
                    --carry;
                Date_ left = carry > 0 ? r.market_.carry_[carry - 1].endDate_ : c.evaluationDate_;
                if (exercise >= 0 && c.exerciseDates_[exercise] > left)
                    left = c.exerciseDates_[exercise];
                const auto& segment = r.market_.carry_[carry];
                double maxDiagonal = segment.rate_;
                int upwind = 0;
                for (int i = 1; i < s.gridPoints_ - 1; ++i) {
                    const auto a = Coefficients(r.market_.volatility_, segment.rate_, segment.dividendYield_, i);
                    Check(std::isfinite(a.minus_) && std::isfinite(a.plus_) && std::isfinite(a.minus_ + a.plus_ + segment.rate_),
                          Indexed("operator", i), "coefficients must be finite");
                    maxDiagonal = std::max(maxDiagonal, a.minus_ + a.plus_ + segment.rate_);
                    upwind += a.upwind_;
                }
                counts.upwind_ = std::max(counts.upwind_, upwind);
                const uint64_t numerator = uint64_t(s.ordinarySteps_) * uint64_t(right - left);
                const auto days = uint64_t(c.maturityDate_ - c.evaluationDate_);
                const auto base = std::max(uint64_t(1), (numerator + days - 1) / days);
                Budget(Add(counts.base_, base), s.maxActualSteps_, "max_actual_steps");
                counts.base_ += static_cast<int>(base);
                const double lo = Years(left, c.evaluationDate_), hi = Years(right, c.evaluationDate_);
                for (int b = 0; b < static_cast<int>(base); ++b) {
                    const double source = b == 0 ? hi : hi - (hi - lo) * b / base;
                    const double target = b + 1 == static_cast<int>(base) ? lo : hi - (hi - lo) * (b + 1) / base;
                    const bool damped = damping > 0;
                    if (damped) {
                        --damping;
                        ++counts.damping_;
                    }
                    const double theta = damped ? 1.0 : s.theta_;
                    const int parts = damped ? 2 : 1;
                    const auto split = [&](double low, double high, std::optional<Date_> date) {
                        Check(high > low, "time_grid", "substep endpoints must be representable and strictly separated");
                        size_t subdivisions = 1;
                        const auto point = [&](size_t j) { return j == subdivisions ? low : high - (high - low) * j / subdivisions; };
                        for (;;) {
                            Budget(Add(counts.steps_, subdivisions), s.maxActualSteps_, "max_actual_steps");
                            bool valid = true;
                            for (size_t j = 0; j < subdivisions; ++j) {
                                const double dt = point(j) - point(j + 1);
                                Check(dt > 0.0, "time_grid", "cannot bisect representable substep");
                                if (!(1.0 + theta * dt * segment.rate_ > 0.0) || !(1.0 - (1.0 - theta) * dt * maxDiagonal >= 0.0)) {
                                    valid = false;
                                    break;
                                }
                            }
                            if (valid)
                                break;
                            subdivisions = Multiply(subdivisions, 2);
                        }
                        for (size_t j = 0; j < subdivisions; ++j) {
                            emit(PlanStep_{point(j + 1), point(j), theta, carry, damped, j + 1 == subdivisions ? date : std::nullopt});
                            ++counts.steps_;
                        }
                    };
                    for (int part = 0; part < parts; ++part) {
                        const double high = part == 0 ? source : target + (source - target) * 0.5;
                        const double low = part + 1 == parts ? target : target + (source - target) * 0.5;
                        split(low, high, b + 1 == static_cast<int>(base) && part + 1 == parts ? std::optional<Date_>(left) : std::nullopt);
                    }
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
        double Boundary(const EarlyExercisePdeRequest_& r, const Vector_<CarryIntegral_>& carry, double t, double spot) {
            const bool call = r.contract_.optionType_ == OptionType_::Value_::CALL;
            const auto at = Integral(carry, t);
            double best = 0.0;
            const auto consider = [&](double e) {
                const auto to = Integral(carry, e);
                best = std::max(best, DiscountedPayoff(spot, r.contract_.strike_, call, to.first - at.first, to.second - at.second));
            };
            if (!American(r)) {
                for (const auto& date : r.contract_.exerciseDates_) {
                    const double e = Years(date, r.contract_.evaluationDate_);
                    if (e > t)
                        consider(e);
                }
                return best;
            }
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

        std::optional<int> Solve(Scratch_* a, double penalty, bool shifted) {
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
            for (int i = n - 2; i > 0; --i) {
                a->iterate_[i] = (a->solveRhs_[i] - (i < n - 2 ? a->plus_[i] * a->iterate_[i + 1] : 0.0)) / a->pivots_[i];
                a->current_[i] = (shifted ? a->payoff_[i] : 0.0) + a->iterate_[i] / a->scale_;
            }
            return std::nullopt;
        }
        double BackwardError(const Scratch_& a, double penalty, bool shifted) {
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
                if (!std::isfinite(numerator) || !std::isfinite(denominator))
                    return std::numeric_limits<double>::infinity();
                const double error =
                    denominator == 0.0 ? (numerator == 0.0 ? 0.0 : std::numeric_limits<double>::infinity()) : numerator / denominator;
                maximum = std::max(maximum, error);
            }
            return maximum;
        }
        template <class T_> size_t CapacityBytes(const Vector_<T_>& x) { return Multiply(x.capacity(), sizeof(T_)); }
    } // namespace

    void ValidateEarlyExercisePdeSettings(const EarlyExercisePdeSettings_& s) {
        Check(s.gridPoints_ >= 5 && s.gridPoints_ <= 100001, ValueField("settings.grid_points", s.gridPoints_), "must be in 5..100001");
        Check(s.ordinarySteps_ > 0, ValueField("settings.ordinary_steps", s.ordinarySteps_), "must be positive");
        Check(s.theta_ == 0.5 || s.theta_ == 1.0, ValueField("settings.theta", s.theta_), "must be 0.5 or 1.0");
        Check(s.rannacherIntervals_ >= 0 && s.rannacherIntervals_ <= s.ordinarySteps_,
              ValueField("settings.rannacher_intervals", s.rannacherIntervals_), "must be in 0..ordinary_steps");
        Check(!s.upper_ || (std::isfinite(*s.upper_) && *s.upper_ > 0.0), ValueField("settings.upper", s.upper_.value_or(0.0)),
              "must be finite and positive or unset");
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

    void ValidateEarlyExercisePdeRequest(const EarlyExercisePdeRequest_& r) {
        ValidateEarlyExercisePdeSettings(r.settings_);
        const auto& c = r.contract_;
        Check(c.evaluationDate_ >= Date::Minimum(), ValueField("contract.evaluation_date", Date::ToExcel(c.evaluationDate_)), "must be a valid date");
        Check(c.maturityDate_ > c.evaluationDate_, ValueField("contract.maturity_date", Date::ToExcel(c.maturityDate_)),
              "must be later than evaluation_date");
        Check(std::isfinite(c.strike_) && c.strike_ > 0.0, ValueField("contract.strike", c.strike_), "must be finite and positive");
        Check(c.optionType_ == OptionType_::Value_::CALL || c.optionType_ == OptionType_::Value_::PUT,
              ValueField("contract.option_type", c.optionType_.String() ? c.optionType_.String() : "<unset>"), "must be CALL or PUT");
        Check(c.exerciseStyle_ == EarlyExerciseStyle_::Value_::AMERICAN || c.exerciseStyle_ == EarlyExerciseStyle_::Value_::BERMUDAN,
              ValueField("contract.exercise_style", c.exerciseStyle_.String() ? c.exerciseStyle_.String() : "<unset>"),
              "must be AMERICAN or BERMUDAN");
        Check(std::isfinite(r.market_.spot_) && r.market_.spot_ > 0.0, ValueField("market.spot", r.market_.spot_), "must be finite and positive");
        Check(std::isfinite(r.market_.volatility_) && r.market_.volatility_ >= 0.0, ValueField("market.volatility", r.market_.volatility_),
              "must be finite and nonnegative");
        const double upper = Upper(r);
        Check(std::isfinite(upper) && upper > std::max(c.strike_, r.market_.spot_), ValueField("settings.upper", upper),
              "must be finite and exceed spot and strike");
        Check(
            std::isfinite(r.settings_.absoluteTolerance_ + r.settings_.relativeTolerance_ * std::max(c.strike_, r.market_.spot_)),
            ValueField("settings.tolerance", r.settings_.absoluteTolerance_ + r.settings_.relativeTolerance_ * std::max(c.strike_, r.market_.spot_)),
            "combined price tolerance must be finite");
        double lastNode = 0.0;
        for (int i = 1; i < r.settings_.gridPoints_; ++i) {
            const double node = upper * (double(i) / (r.settings_.gridPoints_ - 1));
            Check(node > lastNode && std::isfinite(node), ValueField(Indexed("grid", i), node), "nodes must be finite and strictly increasing");
            lastNode = node;
        }
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
        if (American(r))
            Check(c.exerciseDates_.empty(), ValueField("contract.exercise_dates", c.exerciseDates_.size()), "must be empty for AMERICAN");
        else {
            Check(!c.exerciseDates_.empty(), ValueField("contract.exercise_dates", c.exerciseDates_.size()), "must be nonempty for BERMUDAN");
            for (size_t i = 0; i < c.exerciseDates_.size(); ++i) {
                const auto d = c.exerciseDates_[i];
                Check(d >= c.evaluationDate_ && d <= c.maturityDate_ && (i == 0 || d > c.exerciseDates_[i - 1]),
                      ValueField(Indexed("contract.exercise_dates", i), Date::ToExcel(d)), "must be strictly increasing in [evaluation,maturity]");
            }
            Check(c.exerciseDates_.back() == c.maturityDate_, ValueField("contract.exercise_dates.back", Date::ToExcel(c.exerciseDates_.back())),
                  "must equal maturity_date");
        }
    }

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
        static_assert(sizeof(EarlyExercisePdeCarrySegment_) <= 32 && sizeof(Date_) <= 8);
        static_assert(sizeof(EarlyExercisePdeTimeNode_) <= 64 && sizeof(PlanStep_) <= 64 && sizeof(CarryIntegral_) <= 64);
        static_assert(sizeof(EarlyExercisePdeStepDiagnostic_) <= 256 && sizeof(EarlyExercisePdeExerciseLevel_) <= 64);
        static_assert(sizeof(EarlyExercisePdeExerciseRegion_) <= 32 && sizeof(EarlyExercisePdeResult_) <= 4096);
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
        Vector_<CarryIntegral_> carry;
        carry.reserve(c);
        double start = 0.0, rate = 0.0, yield = 0.0;
        for (const auto& x : r.market_.carry_) {
            const double end = Years(x.endDate_, r.contract_.evaluationDate_);
            rate += (end - start) * x.rate_;
            yield += (end - start) * x.dividendYield_;
            carry.push_back({end, x.rate_, x.dividendYield_, rate, yield});
            start = end;
        }
        result.grid_.Resize(n);
        result.values_.Resize(n);
        result.timeGrid_.Resize(m + 1);
        result.steps_.reserve(m);
        result.levels_.reserve(levels);
        result.regions_.reserve(regions);
        e.regionCapacity_ = result.regions_.capacity();
        const auto node = [&](double t, std::optional<Date_> date) {
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
        };
        for (size_t k = 0; k < m; ++k)
            result.timeGrid_[m - 1 - k] = node(plan[k].target_, plan[k].date_);
        result.timeGrid_[m] = node(Years(r.contract_.maturityDate_, r.contract_.evaluationDate_), r.contract_.maturityDate_);
        Scratch_ a(n);
        for (size_t i = 0; i < n; ++i) {
            result.grid_[i] = i + 1 == n ? *s.upper_ : *s.upper_ * (double(i) / (n - 1));
            const double intrinsic = result.grid_[i] - r.contract_.strike_;
            a.payoff_[i] = std::max(r.contract_.optionType_ == OptionType_::Value_::CALL ? intrinsic : -intrinsic, 0.0);
            result.values_[i] = a.payoff_[i];
        }
        constexpr size_t fixedWorkspace = sizeof(Scratch_) + sizeof(plan) + sizeof(carry) + sizeof(counts) + sizeof(PlanStep_) +
                                          sizeof(EarlyExercisePdeStepDiagnostic_) + sizeof(EarlyExercisePdeExerciseLevel_) +
                                          sizeof(EarlyExercisePdeFailure_) + sizeof(Coefficients_) + 32 * sizeof(double) + 16 * sizeof(int);
        static_assert(fixedWorkspace <= 4096);
        e.peakWorkspaceBytes_ = fixedWorkspace + a.Bytes() + CapacityBytes(plan) + CapacityBytes(carry);
        Budget(e.peakWorkspaceBytes_, workspace, "workspace_storage_layout");
        result.levels_.push_back({static_cast<int>(m), r.contract_.maturityDate_, EarlyExercisePdeLevelKind_(Kind_::TERMINAL), 0, 0, e.epsilon_});
        const auto fail = [&](Status_ status, EarlyExercisePdeStepDiagnostic_* step, const String_& reason, std::optional<int> row = std::nullopt,
                              std::optional<double> residual = std::nullopt, std::optional<double> threshold = std::nullopt) {
            result.status_ = EarlyExercisePdeStatus_(status);
            result.failure_ = EarlyExercisePdeFailure_{result.status_,
                                                       step->timeIndex_,
                                                       step->solveCount_,
                                                       result.timeGrid_[step->timeIndex_].calendarDate_,
                                                       row,
                                                       residual,
                                                       threshold,
                                                       e.workUnits_,
                                                       reason};
        };
        for (size_t k = 0; k < m; ++k) {
            const auto& p = plan[k];
            const int target = static_cast<int>(m - 1 - k);
            EarlyExercisePdeStepDiagnostic_ step;
            step.rollbackStep_ = static_cast<int>(k + 1);
            step.timeIndex_ = target;
            step.sourceTimeIndex_ = target + 1;
            step.dt_ = p.source_ - p.target_;
            step.theta_ = p.theta_;
            step.damped_ = p.damped_;
            step.projection_ = !american && result.timeGrid_[target].isExerciseDate_;
            ++e.actualSteps_;
            try {
                a.current_.front() = Boundary(r, carry, p.target_, 0.0);
                a.current_.back() = Boundary(r, carry, p.target_, *s.upper_);
            } catch (const Exception_& error) {
                fail(Status_::NONFINITE, &step, String_(error.what()));
            }
            const auto& segment = r.market_.carry_[p.carry_];
            for (int i = 1; i < static_cast<int>(n) - 1 && result.IsSuccess(); ++i) {
                const auto coeff = Coefficients(r.market_.volatility_, segment.rate_, segment.dividendYield_, i);
                const double diagonal = 1.0 + p.theta_ * step.dt_ * (coeff.minus_ + coeff.plus_ + segment.rate_);
                const double explicitDiagonal = 1.0 - (1.0 - p.theta_) * step.dt_ * (coeff.minus_ + coeff.plus_ + segment.rate_);
                if (!(diagonal > 0.0 && explicitDiagonal >= 0.0 && 1.0 + p.theta_ * step.dt_ * segment.rate_ > 0.0)) {
                    fail(Status_::NON_MONOTONE_OPERATOR, &step, "matrix sufficient monotonicity conditions failed", i);
                    break;
                }
                a.minus_[i] = -p.theta_ * step.dt_ * coeff.minus_ / diagonal;
                a.plus_[i] = -p.theta_ * step.dt_ * coeff.plus_ / diagonal;
                double rhs = explicitDiagonal * result.values_[i] +
                             (1.0 - p.theta_) * step.dt_ * (coeff.minus_ * result.values_[i - 1] + coeff.plus_ * result.values_[i + 1]);
                rhs /= diagonal;
                if (i == 1)
                    rhs -= a.minus_[i] * a.current_.front();
                if (i == static_cast<int>(n) - 2)
                    rhs -= a.plus_[i] * a.current_.back();
                a.rhs_[i] = rhs;
                a.correction_[i] = rhs - a.payoff_[i] - (i > 1 ? a.minus_[i] * a.payoff_[i - 1] : 0.0) -
                                   (i < static_cast<int>(n) - 2 ? a.plus_[i] * a.payoff_[i + 1] : 0.0);
                if (!american)
                    a.correction_[i] = rhs;
                a.current_[i] = american ? std::max(result.values_[i], a.payoff_[i]) : result.values_[i];
                a.iterate_[i] = a.current_[i] - (american ? a.payoff_[i] : 0.0);
                if (!std::isfinite(diagonal) || !std::isfinite(rhs) || !std::isfinite(a.correction_[i]))
                    fail(Status_::NONFINITE, &step, "nonfinite assembled row", i);
            }
            if (american && result.IsSuccess() && !Residuals(a, scale, &step))
                fail(Status_::NONFINITE, &step, "nonfinite original LCP residual");
            double magnitude = 0.0;
            for (int i = 1; i < static_cast<int>(n) - 1; ++i)
                magnitude = std::max({magnitude, std::abs(a.rhs_[i]), std::abs(a.correction_[i]), a.payoff_[i]});
            int exponent = 0;
            std::frexp(magnitude, &exponent);
            // Uniform powers of two cancel in physical backward errors and protect tiny diffusion tails.
            a.scale_ = std::ldexp(1.0, std::clamp(800 - exponent, -1000, 900));
            for (int i = 1; i < static_cast<int>(n) - 1; ++i)
                a.iterate_[i] *= a.scale_;
            const int limit = american ? s.maxIterationsPerStep_ : 1;
            while (result.IsSuccess() && (!american ? step.solveCount_ == 0 : !Admitted(step, e.epsilon_)) && step.solveCount_ < limit) {
                if (n - 2 > s.maxWorkUnits_ - e.workUnits_) {
                    fail(Status_::WORK_LIMIT, &step, "max_work_units exhausted before linear solve");
                    break;
                }
                e.workUnits_ += n - 2;
                ++e.totalSolves_;
                ++step.solveCount_;
                std::copy(a.iterate_.begin(), a.iterate_.end(), a.previous_.begin());
                const double penalty = american ? s.penaltyFactor_ : 0.0;
                const auto pivot = Solve(&a, penalty, american);
                if (pivot) {
                    fail(Status_::LINEAR_SOLVE_FAILURE, &step, "nonpositive or nonfinite pivot", pivot);
                    break;
                }
                step.iterationChange_ = 0.0;
                for (int i = 1; i < static_cast<int>(n) - 1; ++i) {
                    if (!std::isfinite(a.current_[i]) || std::abs(a.current_[i]) > (std::numeric_limits<double>::max)()) {
                        fail(Status_::NONFINITE, &step, "nonfinite linear solution", i);
                        break;
                    }
                    step.iterationChange_ = std::max(step.iterationChange_, std::abs(a.iterate_[i] - a.previous_[i]) / a.scale_);
                }
                if (!result.IsSuccess())
                    break;
                const double error = BackwardError(a, penalty, american);
                MaxOptional(&step.linearBackwardError_, error);
                if (american && !Residuals(a, scale, &step))
                    fail(Status_::NONFINITE, &step, "nonfinite original LCP residual");
                if (!std::isfinite(error) || error > s.linearBackwardErrorLimit_)
                    fail(Status_::LINEAR_SOLVE_FAILURE, &step, "linear backward-error limit exceeded", std::nullopt, error,
                         s.linearBackwardErrorLimit_);
            }
            if (result.IsSuccess() && american && !Admitted(step, e.epsilon_))
                fail(step.iterationChange_ <= e.epsilon_ ? Status_::LCP_RESIDUAL_LIMIT : Status_::MAX_ITERATIONS, &step,
                     "original LCP or iteration-change tolerance not reached", std::nullopt, step.complementarityResidual_, e.epsilon_);
            if (result.IsSuccess() && step.projection_) {
                std::copy(a.current_.begin(), a.current_.end(), a.previous_.begin());
                double change = 0.0, residual = 0.0, obstacle = 0.0;
                for (size_t i = 0; i < n; ++i) {
                    const double projected = std::max(a.current_[i], a.payoff_[i]);
                    change = std::max(change, static_cast<double>(projected - a.current_[i]));
                    a.current_[i] = projected;
                    residual = std::max(residual, std::abs(std::max(a.previous_[i], a.payoff_[i]) - a.current_[i]));
                    obstacle = std::max(obstacle, a.payoff_[i] - a.current_[i]);
                }
                step.projectionChange_ = change;
                step.projectionResidual_ = residual;
                step.obstacleViolation_ = obstacle;
                step.normalizedObstacleViolation_ = obstacle / scale;
            }
            if (result.IsSuccess()) {
                step.completed_ = true;
                ++e.completedSteps_;
                if (american || step.projection_) {
                    EarlyExercisePdeExerciseLevel_ level{target,
                                                         result.timeGrid_[target].calendarDate_,
                                                         EarlyExercisePdeLevelKind_(american ? Kind_::AMERICAN_LCP : Kind_::BERMUDAN_PROJECTION),
                                                         result.regions_.size(),
                                                         0,
                                                         e.epsilon_};
                    const auto contact = [&](int i) {
                        if (a.payoff_[i] <= 0.0 || std::abs(a.current_[i] - a.payoff_[i]) > e.epsilon_)
                            return false;
                        if (i == 0 || i == static_cast<int>(n) - 1)
                            return true;
                        return american ? Dual(a, i) >= -e.epsilon_ : a.previous_[i] <= a.payoff_[i] + e.epsilon_;
                    };
                    for (int i = 0; i < static_cast<int>(n); ++i) {
                        if (!contact(i))
                            continue;
                        const int first = i;
                        while (i + 1 < static_cast<int>(n) && contact(i + 1))
                            ++i;
                        REQUIRE(result.regions_.size() < regions, "EarlyExercisePde: region capacity invariant");
                        result.regions_.push_back({target, static_cast<int>(level.regionCount_), first, i, first == 0, i + 1 == static_cast<int>(n)});
                        ++level.regionCount_;
                    }
                    result.levels_.push_back(level);
                }
                for (size_t i = 0; i < n; ++i)
                    result.values_[i] = static_cast<double>(a.current_[i]);
            }
            e.maxIterations_ = std::max(e.maxIterations_, step.solveCount_);
            MaxOptional(&e.maxLinearBackwardError_, step.linearBackwardError_);
            MaxOptional(&e.maxObstacleViolation_, step.obstacleViolation_);
            MaxOptional(&e.maxDualViolation_, step.dualViolation_);
            MaxOptional(&e.maxComplementarityResidual_, step.complementarityResidual_);
            result.steps_.push_back(step);
            if (!result.IsSuccess())
                break;
        }
        std::reverse(result.steps_.begin(), result.steps_.end());
        std::reverse(result.levels_.begin(), result.levels_.end());
        if (result.IsSuccess()) {
            const auto it = std::lower_bound(result.grid_.begin(), result.grid_.end(), r.market_.spot_);
            const size_t high = static_cast<size_t>(it - result.grid_.begin());
            const double weight = (r.market_.spot_ - result.grid_[high - 1]) / (result.grid_[high] - result.grid_[high - 1]);
            result.price_ = weight == 1.0 ? result.values_[high] : (1.0 - weight) * result.values_[high - 1] + weight * result.values_[high];
        }
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
