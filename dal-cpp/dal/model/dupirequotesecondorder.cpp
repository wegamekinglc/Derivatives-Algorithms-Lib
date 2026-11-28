#include <cfenv>
#include <climits>
#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>

#include <dal/model/detail/dupirequoteplan.hpp>
#include <dal/model/detail/dupirethomas.hpp>

namespace Dal::DupireSecondOrderDetail {
    void ValidationError(Reason reason, const std::string& message) {
        AAD::SecondOrderDetail::Access_::Fail(reason, AAD::SecondOrderStage_::Value_::VALIDATION, {}, message);
    }

    namespace {
        std::string RoundingModeName(int mode) {
            switch (mode) {
#if defined(FE_DOWNWARD)
            case FE_DOWNWARD:
                return "FE_DOWNWARD";
#endif
#if defined(FE_UPWARD)
            case FE_UPWARD:
                return "FE_UPWARD";
#endif
#if defined(FE_TOWARDZERO)
            case FE_TOWARDZERO:
                return "FE_TOWARDZERO";
#endif
            default:
                return std::to_string(mode);
            }
        }

        [[noreturn]] void ReportRoundingMode(const char* entry, int mode) {
            const auto requirement = std::string(entry) + " requires FE_TONEAREST, ";
            ValidationError(Reason::UNSUPPORTED_OPERATION,
                            requirement + (mode < 0 ? "fegetround() failed" : "current mode is " + RoundingModeName(mode)));
        }

        void RequireNearestRounding(const char* entry) {
            const int mode = std::fegetround();
            if (mode != FE_TONEAREST)
                ReportRoundingMode(entry, mode);
        }

        bool MatchesRegisteredSpread(const double& actual, const double& registered) {
            static_assert(sizeof(double) * CHAR_BIT == 64 && std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53 &&
                              std::numeric_limits<double>::max_exponent == 1024,
                          "Fixed Dupire quote identity requires binary64");
            // Compare object representations so DAZ cannot treat a subnormal
            // as zero. The registered point uses the positive zero spelling.
            constexpr double POSITIVE_ZERO = 0.0, NEGATIVE_ZERO = -0.0;
            return std::memcmp(&actual, &registered, sizeof(double)) == 0 ||
                   (std::memcmp(&registered, &POSITIVE_ZERO, sizeof(double)) == 0 && std::memcmp(&actual, &NEGATIVE_ZERO, sizeof(double)) == 0);
        }

        std::string PdeNode(int step, int space) { return " at PDE step " + std::to_string(step) + ", space index " + std::to_string(space); }

        double RequirePositive(Bound_ value, const char* field, const std::string& where) {
            if (value.value_ <= 0.0)
                ValidationError(Reason::OUTSIDE_DOMAIN, std::string(field) + " is nonpositive" + where);
            const double lower = value.Lower();
            if (!(lower > 0.0) || !std::isfinite(value.Upper()))
                ValidationError(Reason::NUMERICAL_FAILURE, std::string(field) + " has no resolved positive lower bound" + where);
            return lower;
        }

        StencilCall_ PrepareCall(double strike, double time) {
            const double forward = SPOT * std::exp((RATE - DIVIDEND) * time);
            return {Bracket(QUOTE_STRIKES, strike), Bracket(QUOTE_TIMES, time), strike, std::exp(-RATE * time), forward, std::sqrt(time),
                    std::log(forward / strike)};
        }

        template <size_t N_> double Distance(const std::array<double, N_>& axis, double value) {
            double result = std::numeric_limits<double>::infinity();
            for (double coordinate : axis)
                result = std::min(result, std::abs(value - coordinate));
            return result;
        }

        template <size_t N_> void RegisteredAxis(const Vector_<>& axis, const std::array<double, N_>& expected, const char* field) {
            if (axis.size() != expected.size())
                ValidationError(Reason::OUTSIDE_DOMAIN, std::string("unregistered ") + field + " length");
            for (size_t i = 0; i < axis.size(); ++i)
                if (axis[i] != expected[i])
                    ValidationError(Reason::OUTSIDE_DOMAIN, std::string("unregistered ") + field + " index " + std::to_string(i));
        }

        void Registered(bool matches, const char* field) {
            if (!matches)
                ValidationError(Reason::OUTSIDE_DOMAIN, std::string("unregistered ") + field);
        }

        std::string AxisDescription(const DupireQuoteCoordinate_& coordinate) {
            std::ostringstream stream;
            stream << coordinate.id_.c_str() << '(' << std::setprecision(std::numeric_limits<double>::max_digits10) << coordinate.strike_ << ", "
                   << coordinate.maturity_ << ')';
            return stream.str();
        }

        void ValidateRequestAxis(const Vector_<DupireQuoteCoordinate_>& actual, const Vector_<DupireQuoteCoordinate_>& expected, int cols) {
            if (actual.size() != expected.size() || cols != static_cast<int>(expected.size()))
                ValidationError(Reason::INVALID_INPUT, "axis length and direction column count must both equal " + std::to_string(expected.size()));
            if (&actual == &expected)
                return;
            for (size_t col = 0; col < expected.size(); ++col) {
                const auto& a = actual[col];
                const auto& e = expected[col];
                if (!std::equal(a.id_.begin(), a.id_.end(), e.id_.begin(), e.id_.end()) || a.strike_ != e.strike_ || a.maturity_ != e.maturity_)
                    ValidationError(Reason::INVALID_INPUT, "axis differs at column " + std::to_string(col) + ", expected " + AxisDescription(e) +
                                                               ", actual " + AxisDescription(a));
            }
        }
    } // namespace

    void CertifyCalibration(Bound_ value, const char* field, int strike, int time, double& minimum) {
        if (value.Lower() > 0.0 && std::isfinite(value.Upper())) {
            minimum = std::min(minimum, value.Lower());
            return;
        }
        (void)RequirePositive(value, field, " at calibration strike index " + std::to_string(strike) + ", maturity index " + std::to_string(time));
    }

    void CertifyRow(Bound_ lower, Bound_ diagonal, Bound_ upper, int step, int space, Margins_& margins) {
        if (lower.value_ > 0.0 || upper.value_ > 0.0)
            ValidationError(Reason::OUTSIDE_DOMAIN, "positive off-diagonal coefficient" + PdeNode(step, space));
        if (!(lower.Upper() <= 0.0 && upper.Upper() <= 0.0))
            ValidationError(Reason::NUMERICAL_FAILURE, "off-diagonal sign is unresolved" + PdeNode(step, space));
        const Bound_ dominance = diagonal + lower + upper;
        if (!(dominance.Lower() > 0.0) || !std::isfinite(dominance.Upper()))
            (void)RequirePositive(dominance, "row diagonal dominance", PdeNode(step, space));
        margins.diagonalMargin_ = std::min(margins.diagonalMargin_, dominance.Lower());

        // With |c'| < 1 at the previous row, b - |a| bounds the
        // next pivot. Reserve roundoff for pivot and normalized upper.
        const Bound_ pivot = space == 0 ? diagonal : diagonal + lower;
        const double pivotLower = pivot.Lower() - 8.0 * EPSILON * (std::abs(diagonal.value_) + std::abs(lower.value_));
        const double normalizedMargin = 1.0 + upper.Lower() / pivotLower - 8.0 * EPSILON;
        if (!(pivotLower > 0.0 && normalizedMargin > 0.0) || !std::isfinite(pivotLower))
            ValidationError(Reason::NUMERICAL_FAILURE, "Thomas pivot or normalized upper is unresolved" + PdeNode(step, space));
        margins.pivotLower_ = std::min(margins.pivotLower_, pivotLower);
        margins.normalizedMargin_ = std::min(margins.normalizedMargin_, normalizedMargin);
    }

    void CertifyActualPivot(double value, int step, int space, double& minimum) {
        if (!(value > 0.0) || !std::isfinite(value))
            ValidationError(Reason::NUMERICAL_FAILURE, "invalid computed Thomas pivot" + PdeNode(step, space));
        minimum = std::min(minimum, value);
    }

    Kernel_::Kernel_(double callStrike, double halfWidth)
        : diffusion_(0.0), drift_(0.0), lowerBoundaryMargin_(std::numeric_limits<double>::infinity()), upperBoundaryMargin_(lowerBoundaryMargin_) {
        if (!(std::isfinite(callStrike) && callStrike > 0.0 && std::isfinite(halfWidth) && halfWidth > 0.0))
            ValidationError(Reason::INVALID_INPUT, "strike and PDE half-width must be finite and positive");
        const double dx = (2.0 * halfWidth) / (SPACE_NODES - 1);
        diffusion_ = DT / (dx * dx);
        drift_ = DT / (2.0 * dx);
        if (!std::isfinite(diffusion_) || !std::isfinite(drift_) || !(diffusion_ > 0.0 && drift_ > 0.0))
            ValidationError(Reason::NUMERICAL_FAILURE, "unresolved PDE step size");
        for (int step = 0; step <= TIME_STEPS; ++step) {
            const double tau = step * DT;
            const double lower = SPOT * std::exp(-halfWidth - DIVIDEND * tau);
            const double upper = SPOT * std::exp(halfWidth - DIVIDEND * tau);
            const double strike = callStrike * std::exp(-RATE * tau);
            const double leftGap = strike - lower, rightGap = upper - strike;
            const std::string where = " at PDE time step " + std::to_string(step);
            lowerBoundaryMargin_ =
                std::min(lowerBoundaryMargin_, RequirePositive({leftGap, 8 * EPSILON * (strike + lower)}, "left discounted boundary margin", where));
            upperBoundaryMargin_ = std::min(upperBoundaryMargin_,
                                            RequirePositive({rightGap, 8 * EPSILON * (strike + upper)}, "right discounted boundary margin", where));
            if (step > 0) {
                rightBoundary_[step - 1] = rightGap;
                time_[step - 1] = Bracket(LOCAL_TIMES, 1.0 - (step - 0.5) * DT);
            }
        }
        for (size_t t = 0; t < LOCAL_TIMES.size(); ++t) {
            const double time = LOCAL_TIMES[t], ht = 1e-4 * time;
            for (int row = FIRST[t]; row <= LAST[t]; ++row) {
                const double strike = LOCAL_STRIKES[row], hk = 1e-4 * strike;
                auto& stencil = stencils_[t][row];
                stencil = {strike,
                           hk,
                           ht,
                           PrepareCall(strike, time),
                           PrepareCall(strike, time - ht),
                           PrepareCall(strike, time + ht),
                           PrepareCall(strike - hk, time),
                           PrepareCall(strike + hk, time)};
                if (!(stencil.base_.strikes_.SameCell(stencil.strikeMinus_.strikes_) &&
                      stencil.base_.strikes_.SameCell(stencil.strikePlus_.strikes_) && stencil.base_.times_.SameCell(stencil.timeMinus_.times_) &&
                      stencil.base_.times_.SameCell(stencil.timePlus_.times_)))
                    ValidationError(Reason::OUTSIDE_DOMAIN, "quote interpolation knot inside stencil at strike index " + std::to_string(row) +
                                                                ", maturity index " + std::to_string(t));
                strikeMargin_ = std::min(strikeMargin_, Distance(QUOTE_STRIKES, strike) - hk);
                timeMargin_ = std::min(timeMargin_, Distance(QUOTE_TIMES, time) - ht);
            }
        }
        std::array<double, LOCAL_STRIKES.size()> logSpots;
        for (size_t j = 0; j < logSpots.size(); ++j)
            logSpots[j] = std::log(LOCAL_STRIKES[j] / SPOT);
        for (int row = 0; row < INTERIOR; ++row) {
            const double x = -halfWidth + dx * (row + 1);
            space_[row] = Bracket(logSpots, x);
            payoff_[row] = std::max(SPOT * std::exp(x) - callStrike, 0.0);
        }
    }

    Margins_ Kernel_::Certify(const Vector_<>& quotes) const {
        Margins_ margins;
        margins.leftBoundary_ = lowerBoundaryMargin_;
        margins.rightBoundary_ = upperBoundaryMargin_;
        const Vector_<Bound_> bounded(quotes.begin(), quotes.end());
        const auto price = Rollback(Calibrate(bounded, &margins), &margins).value_;
        if (!std::isfinite(price) || price != (*this)(nullptr, quotes))
            ValidationError(Reason::NUMERICAL_FAILURE, "qualified price disagrees with binary64 calibration and rollback");
        margins.price_ = price;
        return margins;
    }

    AAD::SecondOrderNumber_ Kernel_::RollbackSingleDirection(const Vector_<AAD::SecondOrderNumber_>& quotes, PrimalTrace_* trace) const {
        using namespace Thomas;
        auto& arena = *Tape::activeArena;
        if (arena.wide_)
            return (*this)(nullptr, quotes);

        // The plan already certifies diagonal dominance and pivot margins.
        // Restrict seed scale and spread before checking derivative arithmetic
        // during the solve. The scalar wide replay uses the submitted seed.
        double maxSeed = 0.0, minSeed = std::numeric_limits<double>::infinity();
        for (const auto& quote : quotes) {
            const auto slot = Tape::Access_::Slot(quote, arena);
            const auto seed = std::abs(Tape::Access_::Tangent<double>(quote, arena, slot));
            if (seed != 0.0) {
                maxSeed = std::max(maxSeed, seed);
                minSeed = std::min(minSeed, seed);
            }
        }
        if (maxSeed > 0x1p20 || (maxSeed != 0.0 && (maxSeed < 0x1p-20 || minSeed < 0x1p-20 * maxSeed)))
            Tape::Promote("Dupire Thomas direction range");

        using Number = AAD::SecondOrderNumber_;
        using Work = Rollback_<INTERIOR, TIME_STEPS, 33>;
        const auto vols = Calibrate(quotes);
        std::array<std::array<Number, INTERIOR>, LOCAL_TIMES.size()> slices;
        for (size_t t = 0; t < LOCAL_TIMES.size(); ++t)
            for (int i = 0; i < INTERIOR; ++i)
                slices[t][i] = Interpolate<Number>(vols[t], space_[i]);
        const Tape::Scratch_ storage(arena, sizeof(Work));
        const auto work = std::unique_ptr<Work>(new Work);
        const auto coefficients = [&](size_t step, const auto& makeFactor) {
            const auto& t = time_[step];
            if (step == 0 || !t.SameCell(time_[step - 1]) || t.weight_ != time_[step - 1].weight_)
                makeFactor([&](size_t i) {
                    const Number vol = t.left_ == t.right_ ? slices[t.left_][i]
                                                           : Number(slices[t.left_][i] + (slices[t.right_][i] - slices[t.left_][i]) * t.weight_);
                    return MatrixRow(vol);
                });
        };
        return work->Evaluate(arena, payoff_, rightBoundary_, INTERIOR / 2, coefficients, trace);
    }
} // namespace Dal::DupireSecondOrderDetail

namespace Dal {
    using namespace DupireSecondOrderDetail;

    struct DupireQuoteSecondOrderPlan_::Data_ {
        DupireEuropeanCall_ call_;
        String_ algorithm_;
        Kernel_ kernel_;
        Vector_<> point_;
        Vector_<DupireQuoteCoordinate_> axis_;
        Margins_ margins_;
    };

    DupireQuoteSecondOrderPlan_::DupireQuoteSecondOrderPlan_(std::shared_ptr<const Data_> data) : data_(std::move(data)) {}
    const Vector_<DupireQuoteCoordinate_>& DupireQuoteSecondOrderPlan_::InputAxis() const { return data_->axis_; }

    DupireQuoteSecondOrderPlan_ PrepareDupireEuropeanCallQuoteRisk(const DupireCalibrationSnapshot_& calibration,
                                                                   const DupireEuropeanCall_& call,
                                                                   const DupireEuropeanCallPdeGrid_& grid) {
        RequireNearestRounding("PrepareDupireEuropeanCallQuoteRisk");
        if (!(std::isfinite(call.strike_) && call.strike_ > 0.0 && std::isfinite(call.maturity_) && call.maturity_ > 0.0))
            ValidationError(Reason::INVALID_INPUT, "strike and maturity must be finite and positive");
        if (!(std::isfinite(grid.logSpotHalfWidth_) && grid.logSpotHalfWidth_ > 0.0))
            ValidationError(Reason::INVALID_INPUT, "logSpotHalfWidth must be finite and positive");
        if (grid.spaceNodes_ < 3 || grid.spaceNodes_ % 2 == 0)
            ValidationError(Reason::INVALID_INPUT, "spaceNodes must be odd and at least three");
        if (grid.timeSteps_ <= 0)
            ValidationError(Reason::INVALID_INPUT, "timeSteps must be positive");

        Registered(call.strike_ == STRIKE, "strike");
        Registered(call.maturity_ == 1.0, "maturity");
        Registered(grid.logSpotHalfWidth_ == HALF_WIDTH, "logSpotHalfWidth");
        Registered(grid.spaceNodes_ == SPACE_NODES, "spaceNodes");
        Registered(grid.timeSteps_ == TIME_STEPS, "timeSteps");
        Registered(calibration.Spot() == SPOT, "spot");
        Registered(calibration.Rate() == RATE, "rate");
        Registered(calibration.DividendYield() == DIVIDEND, "dividend yield");
        Registered(calibration.Algorithm() == "DupireCentral1e-4FixedBaseBand/v1", "calibration algorithm");
        const auto& inputs = calibration.Inputs();
        RegisteredAxis(inputs.quoteStrikes_, QUOTE_STRIKES, "quote strike");
        RegisteredAxis(inputs.quoteMaturities_, QUOTE_TIMES, "quote maturity");
        RegisteredAxis(inputs.inclusionSpots_, LOCAL_STRIKES, "inclusion spot");
        RegisteredAxis(inputs.inclusionTimes_, LOCAL_TIMES, "inclusion time");
        Registered(inputs.maxSpotSpacing_ == 10.0, "maxSpotSpacing");
        Registered(inputs.maxTimeSpacing_ == 0.5, "maxTimeSpacing");
        RegisteredAxis(calibration.Surface()->spots_, LOCAL_STRIKES, "completed spot");
        RegisteredAxis(calibration.Surface()->times_, LOCAL_TIMES, "completed time");
        for (size_t i = 0; i < QUOTE_SPREADS.size(); ++i)
            if (!MatchesRegisteredSpread(inputs.quoteSpreads_.Data()[i], QUOTE_SPREADS[i]))
                ValidationError(Reason::OUTSIDE_DOMAIN, "unregistered spread at quote:" + std::to_string(i));
        SnapshotAccess_::ValidateFixedBase(calibration);

        auto data = std::make_shared<DupireQuoteSecondOrderPlan_::Data_>(DupireQuoteSecondOrderPlan_::Data_{
            call, calibration.Algorithm(), Kernel_(), Vector_<>(QUOTE_SPREADS.begin(), QUOTE_SPREADS.end()), {}, {}});
        for (double strike : QUOTE_STRIKES)
            for (double time : QUOTE_TIMES)
                data->axis_.push_back({String_("quote:" + std::to_string(data->axis_.size())), strike, time});
        data->margins_ = data->kernel_.Certify(data->point_);
        return DupireQuoteSecondOrderPlan_(std::move(data));
    }

    const Kernel_& PlanAccess_::Kernel(const DupireQuoteSecondOrderPlan_& plan) { return plan.data_->kernel_; }
    const Margins_& PlanAccess_::Margins(const DupireQuoteSecondOrderPlan_& plan) { return plan.data_->margins_; }
    const Vector_<>& PlanAccess_::Point(const DupireQuoteSecondOrderPlan_& plan) { return plan.data_->point_; }
    const DupireEuropeanCall_& PlanAccess_::Call(const DupireQuoteSecondOrderPlan_& plan) { return plan.data_->call_; }
    const String_& PlanAccess_::Algorithm(const DupireQuoteSecondOrderPlan_& plan) { return plan.data_->algorithm_; }

    DupireQuoteSecondOrderResult_ PlanAccess_::Evaluate(const DupireQuoteSecondOrderPlan_& plan,
                                                        const Vector_<DupireQuoteCoordinate_>& axis,
                                                        const Matrix_<>& directions,
                                                        const AAD::SecondOrderOptions_& options) {
        RequireNearestRounding("EvaluateDupireQuoteSecondOrder");
        ValidateRequestAxis(axis, plan.InputAxis(), directions.Cols());
        const auto scalar = [kernel = &Kernel(plan)](const auto& x) { return (*kernel)(nullptr, x); };
        if (directions.Rows() == 1) {
            const auto rollback = [kernel = &Kernel(plan)](const Vector_<AAD::SecondOrderNumber_>& x) { return kernel->RollbackSingleDirection(x); };
            return DupireQuoteSecondOrderResult_(plan, DupireQuoteSecondOrderRisk_(AAD::EvaluateSecondOrder(rollback, Point(plan), directions, options)));
        }
        return DupireQuoteSecondOrderResult_(
            plan, directions.Rows() <= 1 ? DupireQuoteSecondOrderRisk_(AAD::EvaluateSecondOrder(scalar, Point(plan), directions, options))
                                         : DupireQuoteSecondOrderRisk_(AAD::EvaluateSharedSecondOrder(scalar, Point(plan), directions, options)));
    }

    DupireQuoteSecondOrderResult_ EvaluateDupireQuoteSecondOrder(const DupireQuoteSecondOrderPlan_& plan,
                                                                 const Vector_<DupireQuoteCoordinate_>& inputAxis,
                                                                 const Matrix_<>& directions,
                                                                 const AAD::SecondOrderOptions_& options) {
        return PlanAccess_::Evaluate(plan, inputAxis, directions, options);
    }

    DupireQuoteSecondOrderResult_::DupireQuoteSecondOrderResult_(const DupireQuoteSecondOrderPlan_& plan, DupireQuoteSecondOrderRisk_&& risk)
        : plan_(plan), risk_(std::move(risk)) {}
    const DupireEuropeanCall_& DupireQuoteSecondOrderResult_::Call() const { return PlanAccess_::Call(plan_); }
    const String_& DupireQuoteSecondOrderResult_::CalibrationAlgorithm() const { return PlanAccess_::Algorithm(plan_); }
    const String_& DupireQuoteSecondOrderResult_::QuoteUnit() const {
        static const String_ unit("decimal-vol");
        return unit;
    }

    double DupireQuoteSecondOrderRisk_::Value() const {
        return std::visit([](const auto& r) { return r.Value(); }, risk_);
    }
    const Vector_<>& DupireQuoteSecondOrderRisk_::Point() const {
        return std::visit([](const auto& r) -> const Vector_<>& { return r.Point(); }, risk_);
    }
    const Vector_<>& DupireQuoteSecondOrderRisk_::Gradient() const {
        return std::visit([](const auto& r) -> const Vector_<>& { return r.Gradient(); }, risk_);
    }
    const Matrix_<>& DupireQuoteSecondOrderRisk_::Directions() const {
        return std::visit([](const auto& r) -> const Matrix_<>& { return r.Directions(); }, risk_);
    }
    const Vector_<>& DupireQuoteSecondOrderRisk_::DirectionalDerivatives() const {
        return std::visit([](const auto& r) -> const Vector_<>& { return r.DirectionalDerivatives(); }, risk_);
    }
    const Matrix_<>& DupireQuoteSecondOrderRisk_::HessianProducts() const {
        return std::visit([](const auto& r) -> const Matrix_<>& { return r.HessianProducts(); }, risk_);
    }
} // namespace Dal
