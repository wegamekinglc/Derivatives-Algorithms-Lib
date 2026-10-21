#include <dal/platform/platform.hpp>

#include <limits>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/depositscript.hpp>
#include <dal/curve/jointrate.hpp>
#include <dal/curve/logdfinterp.hpp>
#include <dal/math/rootfind.hpp>
#include <dal/time/dateutils.hpp>

namespace Dal {
    using namespace CalibrationInternal;

    namespace {
        CalibrationDiagnostic_ DepositDiagnostic(const CalibrationInstrument_& instrument, CalibrationStage_ stage) {
            CalibrationDiagnostic_ d;
            d.instrumentId_ = instrument.Id();
            d.stage_ = stage;
            return d;
        }
        CalibrationDiagnostic_ ImportDiagnostic(const Deposit_& native, const DepositScriptImportSettings_& settings) {
            CalibrationDiagnostic_ d;
            d.stage_ = CalibrationStage_::Value_::Input;
            d.instrumentId_ = settings.instrumentId_;
            Token(settings.currency_, "InvalidCalibrationDefinition", "/import/currency", d);
            Check(std::isfinite(settings.notional_) && settings.notional_ > 0., "InvalidCalibrationParameterBinding", "/import/notional",
                  "explicit finite positive lender notional", "", d);
            Check(std::isfinite(native.MarketRate()), "InvalidCalibrationParameterBinding", "/import/coupon", "finite fixed coupon", "", d);
            return d;
        }
        pair<SchedulePeriod_, double> ImportPeriod(const Deposit_& native, const CalibrationDiagnostic_& d) {
            const auto [start, end] = native.TimeSpan();
            const auto& convention = native.FloatConvention();
            Check(start.IsValid() && end.IsValid() && start < end, "UnsupportedCalibrationProduct", "/import/dates", "valid start<end", "", d);
            Check(!convention.useProjectionCurve_ && convention.collateral_ == CollateralType_::Value_::OIS, "UnsupportedCalibrationProduct",
                  "/import/convention", "forecast=discount with OIS collateral", "", d);
            Check(start == native.TradeDate(), "UnsupportedCalibrationProduct", "/import/start", "trade=start at origination", "", d);
            try {
                const auto period = BuildSinglePeriodSchedule(start, end, convention, CouponMonths(start, end));
                return {period, convention.dayBasis_(period.accrualStart_, period.accrualEnd_, period.dayCountContext_.get())};
            } catch (const Exception_& error) {
                Fail("UnsupportedCalibrationProduct", "/import/convention", "resolvable native adjustment and day count", String_(error.what()), d);
            }
        }
        void
        CandidateDomain(const Handle_<GSRCurveData_>& candidate, const Vector_<Date_>& dates, const Ccy_& currency, const CalibrationDiagnostic_& d) {
            const String_ code = "CalibrationContextMismatch";
            Check(candidate && candidate->evaluationDate_ == dates.front() && candidate->nodeDates_ == dates, code, "/candidate/node_dates",
                  "fixed prepared node layout and valuation date", "", d);
            Check(candidate->currency_ == currency.String() && candidate->projectionTenors_.empty() && candidate->projectionLogDF_.Rows() == 0, code,
                  "/candidate/routes", "same-currency discount=forecast without projection curves", "", d);
            Check(candidate->discountLogDF_.size() == dates.size() && candidate->discountLogDF_.front() == 0., code, "/candidate/log_df",
                  "one value per node and fixed zero anchor", "", d);
            for (double x : candidate->discountLogDF_)
                Check(std::isfinite(x), "CalibrationNumericalFailure", "/candidate/log_df", "finite native coordinates", "", d);
        }
        String_ CandidateIdentity(const GSRCurveData_& candidate, const String_& base) {
            return Fingerprint(Obj({{"plan", Str(base)},
                                    {"adapter", Str("dal.deposit-log-linear/1")},
                                    {"currency", Str(candidate.currency_)},
                                    {"dates", Array(candidate.nodeDates_, [](const Date_& date) { return Str(Date::ToString(date)); })},
                                    {"log_df", Array(candidate.discountLogDF_, [](double x) { return Num(x); })}}));
        }
        [[noreturn]] void FitFailure(const String_& code, const DepositCurveFit_& fit, CalibrationDiagnostic_ diagnostic) {
            diagnostic.stage_ = CalibrationStage_::Value_::Solve;
            diagnostic.code_ = code;
            diagnostic.field_ = diagnostic.field_.value_or("/solver");
            const String_ state = "evaluations=" + String::FromInt(fit.evaluations_) + "; raw=" + NumberText(fit.rawResidual_) +
                                  "; scaled=" + NumberText(fit.scaledResidual_);
            diagnostic.actual_ = diagnostic.actual_.value_or("") + "; last validated " + state;
            diagnostic.expected_ = diagnostic.expected_.value_or("bracketed ParRate root with abs(scaled residual)<1 inside evaluation budget");
            diagnostic.message_ = code + ": " + *diagnostic.field_ + "; " + diagnostic.message_ + "; " + state;
            auto failure = std::make_shared<CalibrationFailure_>();
            failure->diagnostic_ = diagnostic;
            failure->candidate_ = Handle_<Storable_>(fit.curve_);
            failure->report_.reset(new CalibrationReport_(diagnostic.stage_, fit.axes_, {diagnostic}));
            throw CalibrationError_(__FILE__, __LINE__, __func__, failure);
        }
        CalibrationDiagnostic_ FitDiagnostic(const PreparedDepositCurve_& plan, const Handle_<CalibrationObservation_>& observation) {
            const auto& instrument = *plan.PricePlan().Instrument();
            auto d = DepositDiagnostic(instrument, CalibrationStage_::Value_::Input);
            Check(observation && Bytes(observation->Instrument()->InstrumentIdentity()) == Bytes(instrument.InstrumentIdentity()) &&
                      Bytes(observation->MeasureId()) == "ParRate",
                  "InvalidCalibrationPlan", "/observation", "ParRate observation for the prepared binding", "", d);
            d.observationId_ = observation->Id();
            d.quoteId_ = observation->Quote()->Id();
            d.measureId_ = "ParRate";
            return d;
        }
        void
        SolverSettings(const Handle_<GSRCurveData_>& initial, int freeNode, double lower, double upper, int budget, const CalibrationDiagnostic_& d) {
            Check(initial && freeNode > 0 && freeNode < static_cast<int>(initial->nodeDates_.size()), "InvalidCalibrationPlan", "/solver/free_node",
                  "one non-anchor native logDF coordinate", "", d);
            Check(std::isfinite(lower) && std::isfinite(upper) && lower < upper && budget > 0, "InvalidCalibrationPlan", "/solver/settings",
                  "finite ordered bracket and positive evaluation budget", "", d);
        }
        struct NodeFitEvaluator_ {
            const PreparedDepositCurve_& plan_;
            const CalibrationObservation_& observation_;
            const GSRCurveData_& initial_;
            int freeNode_, budget_;
            CalibrationDiagnostic_ diagnostic_;

            // Calibration updates the last validated candidate for owned failure reporting.
            double Evaluate(double theta, DepositCurveFit_* fit) const {
                Check(fit->evaluations_ < budget_, "CalibrationBudgetExceeded", "/solver", "evaluation within budget", "", diagnostic_);
                auto values = initial_.discountLogDF_;
                Check(values.size() == initial_.nodeDates_.size(), "InvalidCalibrationPlan", "/candidate/log_df", "one value per node", "",
                      diagnostic_);
                values[freeNode_] = theta;
                Handle_<GSRCurveData_> candidate(new GSRCurveData_("deposit-fit", initial_.evaluationDate_, initial_.currency_, initial_.nodeDates_,
                                                                   values, initial_.projectionTenors_, initial_.projectionLogDF_));
                const auto value = plan_.Evaluate(candidate);
                const double raw = value.parRate_.value_ - observation_.Quote()->Value(), scaled = raw / observation_.ErrorScale();
                if (!std::isfinite(scaled))
                    Fail("CalibrationNumericalFailure", "/solver/residual", "finite scaled residual", NumberText(scaled), diagnostic_);
                fit->curve_ = candidate;
                fit->value_ = value;
                ++fit->evaluations_;
                fit->rawResidual_ = raw;
                fit->scaledResidual_ = scaled;
                return scaled;
            }
        };
    } // namespace

    Handle_<CalibrationInstrument_> NewDepositScriptInstrument(const Deposit_& native, const DepositScriptImportSettings_& settings) {
        const auto diagnostic = ImportDiagnostic(native, settings);
        const auto [period, alpha] = ImportPeriod(native, diagnostic);
        Check(period.accrualStart_ == native.TradeDate(), "UnsupportedCalibrationProduct", "/import/start", "adjusted start equals valuation", "",
              diagnostic);
        ScriptInstrumentDefinitionSpec_ s;
        const auto price = CalibrationValueUnit_::Parse("Price:" + String_(settings.currency_.String()));
        s.header_.payCurrency_ = s.header_.resultCurrency_ = s.header_.collateralCurrency_ = settings.currency_;
        s.header_.discountRoute_ = settings.discountRoute_;
        s.source_.rows_ = {{ScriptSourceRowKind_::Value_::Definition, "N", NumberText(settings.notional_)},
                           {ScriptSourceRowKind_::Value_::Definition, "K", NumberText(native.MarketRate())},
                           {ScriptSourceRowKind_::Value_::Definition, "ALPHA", NumberText(alpha)},
                           {ScriptSourceRowKind_::Value_::Event, Date::ToString(period.accrualStart_), "pay PAYS -N"},
                           {ScriptSourceRowKind_::Value_::Event, Date::ToString(period.accrualEnd_), "pay PAYS N*(1+K*ALPHA)"}};
        s.parameters_ = {{"N", ScriptParameterType_::Value_::Real, price, ScriptParameterRole_::Value_::Notional, true, 0., {}, false, true},
                         {"K", ScriptParameterType_::Value_::Real, CalibrationValueUnit_::Parse("Rate"), ScriptParameterRole_::Value_::Coupon, true},
                         {"ALPHA",
                          ScriptParameterType_::Value_::Real,
                          CalibrationValueUnit_::Parse("YearFraction"),
                          ScriptParameterRole_::Value_::Accrual,
                          false,
                          0.,
                          {},
                          false,
                          true}};
        s.measures_ = {{"Price", CalibrationMeasureKind_::Value_::Price, "PAY", {}, price},
                       {"ParRate", CalibrationMeasureKind_::Value_::ParAffineRate, "PAY", String_("K"), CalibrationValueUnit_::Parse("Rate"), 1,
                        settings.minAbsAnnuity_}};
        return NewScriptCalibrationInstrument({settings.instrumentId_, NewScriptInstrumentDefinition(s), {}, period.accrualEnd_});
    }

    PreparedDepositCurve_::PreparedDepositCurve_(const Handle_<CalibrationInstrument_>& instrument, const CalibrationPreparationContext_& context)
        : pricePlan_(PrepareCalibrationInstrument(instrument, "Price", context)),
          parPlan_(PrepareCalibrationInstrument(instrument, "ParRate", context)) {
        const auto d = DepositDiagnostic(*instrument, CalibrationStage_::Value_::Prepare);
        Check(std::holds_alternative<CalibrationDepositDescriptor_>(pricePlan_.Descriptor()), "UnsupportedFastEvaluator", "/instrument",
              "Deposit cashflow descriptor", "", d);
        Check(!context.model_ && context.smoothing_ == 0. && Bytes(context.conventionState_) == "dal.fixed-dates/1", "UnsupportedFastEvaluator",
              "/context", "deterministic hard fixed-date curve context; construct MC model separately", "", d);
        nodeDates_ = context.curve_->nodeDates_;
        CandidateDomain(context.curve_, nodeDates_, context.currency_, d);
        const auto& descriptor = std::get<CalibrationDepositDescriptor_>(pricePlan_.Descriptor());
        Check(descriptor.maturity_ <= nodeDates_.back(), "UnsupportedFastEvaluator", "/candidate/node_dates", "nodes covering maturity", "", d);
        Vector_<> times;
        for (const auto& date : nodeDates_)
            times.push_back((date - nodeDates_.front()) / 365.);
        const LogDfInterpolation_ interpolation(times, LogDfScheme_("LOG_LINEAR"));
        maturityWeights_ = interpolation.WeightsAt((descriptor.maturity_ - nodeDates_.front()) / 365.);
    }

    DepositCurveValue_ PreparedDepositCurve_::Evaluate(const Handle_<GSRCurveData_>& candidate) const {
        const auto& instrument = *pricePlan_.Instrument();
        const auto d = DepositDiagnostic(instrument, CalibrationStage_::Value_::Evaluate);
        CandidateDomain(candidate, nodeDates_, instrument.Definition()->Spec().header_.payCurrency_, d);
        const double discount = std::exp(Interp::ApplyInterpWeights(candidate->discountLogDF_, maturityWeights_));
        Matrix_<> partials(2, static_cast<int>(nodeDates_.size() - 1), 0.);
        for (const auto& [index, weight] : maturityWeights_)
            if (index > 0)
                partials(1, index - 1) += weight * discount;
        DepositCurveValue_ result;
        result.coefficients_ = parPlan_.Coefficients({1., discount}, partials);
        result.price_.value_ = pricePlan_.Price({1., discount});
        const auto& descriptor = std::get<CalibrationDepositDescriptor_>(pricePlan_.Descriptor());
        for (size_t j = 0; j < result.coefficients_.da_.size(); ++j) {
            const double derivative = result.coefficients_.db_[j] + descriptor.coupon_ * result.coefficients_.da_[j];
            if (!std::isfinite(derivative))
                Fail("CalibrationNumericalFailure", "/derivatives", "finite Price partials", NumberText(derivative), d);
            result.price_.derivatives_.push_back(derivative);
        }
        result.parRate_ = parPlan_.AffineMeasure(result.coefficients_);
        result.parRate_.value_ = Tape::ForwardRateFromDf(discount, descriptor.accrual_);
        Check(std::isfinite(result.parRate_.value_), "CalibrationNumericalFailure", "/measure", "finite explicit DF/accrual kernel value", "", d);
        result.parameterLabels_ = candidate->RiskLabels();
        result.candidateIdentity_ = CandidateIdentity(*candidate, parPlan_.PlanIdentity());
        return result;
    }

    DepositCurveFit_ FitDepositCurveNode(const PreparedDepositCurve_& plan,
                                         const Handle_<CalibrationObservation_>& observation,
                                         const Handle_<GSRCurveData_>& initial,
                                         int freeNode,
                                         double lower,
                                         double upper,
                                         int maxEvaluations) {
        const auto diagnostic = FitDiagnostic(plan, observation);
        SolverSettings(initial, freeNode, lower, upper, maxEvaluations, diagnostic);
        DepositCurveFit_ fit;
        fit.axes_ = BuildCalibrationObservationAxes({observation});
        const NodeFitEvaluator_ evaluator{plan, *observation, *initial, freeNode, maxEvaluations, diagnostic};
        try {
            const double low = evaluator.Evaluate(lower, &fit);
            if (std::abs(low) < 1.)
                return fit;
            const double high = evaluator.Evaluate(upper, &fit);
            if (std::abs(high) < 1.)
                return fit;
            Check(std::signbit(low) != std::signbit(high), "CalibrationFitFailed", "/solver", "bracketed ParRate root", "", diagnostic);
            BracketedBrent_ solver({lower, low}, {upper, high}, std::numeric_limits<double>::epsilon());
            for (;;) {
                const double residual = evaluator.Evaluate(solver.NextX(), &fit);
                if (std::abs(residual) < 1.)
                    return fit;
                solver.PutY(residual);
            }
        } catch (const CalibrationError_& error) {
            auto cause = error.Failure().diagnostic_;
            cause.instrumentId_ = diagnostic.instrumentId_;
            cause.observationId_ = diagnostic.observationId_;
            cause.quoteId_ = diagnostic.quoteId_;
            cause.measureId_ = cause.measureId_.value_or(*diagnostic.measureId_);
            FitFailure(cause.code_, fit, cause);
        } catch (const std::exception& error) {
            auto cause = diagnostic;
            cause.message_ = String_(error.what());
            FitFailure("CalibrationNumericalFailure", fit, cause);
        }
    }
} // namespace Dal
