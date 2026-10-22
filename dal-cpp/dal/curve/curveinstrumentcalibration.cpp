#include <dal/platform/platform.hpp>

#include <set>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveinstrumentcalibration.hpp>
#include <dal/curve/curvescriptevaluation_internal.hpp>
#include <dal/storage/json.hpp>

namespace Dal {
    using namespace CalibrationInternal;
    namespace {
        CurveCalibrationSpec_ NativeGeometry(const CurveInstrumentCalibrationSpec_& spec) {
            const auto& g = spec.geometry_;
            CurveCalibrationSpec_ result;
            result.today_ = g.today_;
            result.ccy_ = g.ccy_;
            result.curveName_ = g.curveName_;
            result.knotDates_ = g.knotDates_;
            result.parameterization_ = g.parameterization_;
            result.logDfScheme_ = g.logDfScheme_;
            result.baseCurve_ = g.baseCurve_;
            result.targetCollateral_ = g.targetCollateral_;
            result.targetTenor_ = g.targetTenor_;
            result.calibrateDiscountCurve_ = g.calibrateDiscountCurve_;
            result.liborBasis_ = g.liborBasis_;
            result.discountCurves_ = spec.market_.discountCurves_;
            result.forwardCurves_ = spec.market_.forwardCurves_;
            result.initialGuess_ = spec.solver_.initialGuess_;
            result.initialGuessPerNode_ = spec.solver_.initialGuessPerNode_;
            return result;
        }
        CurveDefinition_ Definition(const CurveInstrumentCalibrationSpec_& spec) {
            const auto& g = spec.geometry_;
            return MakeCurveDefinition(g.curveName_, g.ccy_, g.parameterization_, g.logDfScheme_, g.knotDates_, g.today_, g.liborBasis_);
        }
        CurveInstrumentCalibrationSpec_ ResolveKnots(CurveInstrumentCalibrationSpec_ spec) {
            auto& g = spec.geometry_;
            Token(g.knotPolicy_, "InvalidCalibrationPlan", "/geometry/knot_policy");
            if (g.parameterization_ == CurveParameterization_::Value_::LOG_DISCOUNT)
                Check(!g.knotDates_.empty() && g.knotDates_.front() == g.today_, "InvalidCalibrationPlan", "/geometry/knot_dates/0",
                      "explicit LOG_DISCOUNT anchor equal to today");
            if (g.knotPolicy_ != CurveKnotPolicy_::Value_::INPUT) {
                std::set<Date_> dates;
                if (g.knotPolicy_ == CurveKnotPolicy_::Value_::AUGMENTED)
                    dates.insert(g.knotDates_.begin(), g.knotDates_.end());
                if (g.parameterization_ == CurveParameterization_::Value_::LOG_DISCOUNT)
                    dates.insert(g.today_);
                for (const auto& observation : spec.observations_) {
                    const auto& pillar = observation->Instrument()->Pillar();
                    Check(pillar && *pillar > g.today_, "InvalidCalibrationPlan", "/instrument/pillar", "future pillar for observation knot policy");
                    dates.insert(*pillar);
                }
                g.knotDates_ = Vector_<Date_>(dates.begin(), dates.end());
            }
            g.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
            return spec;
        }
        CalibrationDiagnostic_ RowDiagnostic(const CalibrationObservation_& row, CalibrationStage_ stage, CalibrationDiagnostic_ d = {}) {
            d.stage_ = stage;
            d.observationId_ = row.Id();
            d.instrumentId_ = row.Instrument()->Id();
            d.quoteId_ = row.Quote()->Id();
            d.measureId_ = row.MeasureId();
            return d;
        }
        void
        ValidateNativeJacobian(const Matrix_<>& jacobian, const CurveInstrumentCalibrationSpec_& spec, const Vector_<CurveFreeParameter_>& labels) {
            Check(jacobian.Rows() == static_cast<int>(spec.observations_.size()) && jacobian.Cols() == static_cast<int>(labels.size()),
                  "CalibrationNumericalFailure", "/quote_jacobian", "raw measure rows by fixed native parameter columns");
            for (int i = 0; i < jacobian.Rows(); ++i)
                for (int j = 0; j < jacobian.Cols(); ++j)
                    if (!std::isfinite(jacobian(i, j))) {
                        auto d = RowDiagnostic(*spec.observations_[i], CalibrationStage_::Value_::Evaluate);
                        d.parameterKey_ = String_(labels[j].component_.String()) + ":" + Date::ToString(labels[j].date_);
                        Fail("CalibrationNumericalFailure", "/quote_jacobian/" + OrdinalText(i) + "/" + OrdinalText(j),
                             "finite raw model-measure partial", NumberText(jacobian(i, j)), d);
                    }
        }
        [[noreturn]] void CurveFailure(CurveInstrumentCalibrationResult_ result, CalibrationDiagnostic_ diagnostic) {
            if (!result.axes_.observations_.empty()) {
                const auto& row = *result.axes_.observations_.front();
                diagnostic.observationId_ = diagnostic.observationId_.value_or(row.Id());
                diagnostic.instrumentId_ = diagnostic.instrumentId_.value_or(row.Instrument()->Id());
                diagnostic.quoteId_ = diagnostic.quoteId_.value_or(row.Quote()->Id());
                diagnostic.measureId_ = diagnostic.measureId_.value_or(row.MeasureId());
            }
            const String_ prefix = diagnostic.code_ + ": ";
            if (Bytes(diagnostic.message_).compare(0, prefix.size(), Bytes(prefix)) != 0)
                diagnostic.message_ = prefix + diagnostic.field_.value_or("/solver") + "; " + diagnostic.message_;
            auto failure = std::make_shared<CalibrationFailure_>();
            failure->diagnostic_ = diagnostic;
            failure->report_.reset(new CalibrationReport_(diagnostic.stage_, result.axes_, {diagnostic}));
            result.report_ = failure->report_;
            result.termination_ = diagnostic.code_;
            failure->candidate_.reset(new CurveInstrumentCalibrationCandidate_(std::move(result)));
            throw CalibrationError_(__FILE__, __LINE__, __func__, failure);
        }
        void PreparationSettings(const CurveInstrumentCalibrationSpec_& spec, const CurveInstrumentCalibrationOptions_& options) {
            Check(spec.valuation_.evaluationDate_ && *spec.valuation_.evaluationDate_ == spec.geometry_.today_ && spec.valuation_.fixings_,
                  "CalibrationContextMismatch", "/valuation", "matching explicit valuation date and frozen fixing snapshot");
            Token(spec.valuation_.todayFixingPolicy_, "InvalidCalibrationFixingPolicy", "/valuation/today_fixing_policy");
            Token(options.evaluation_, "UnsupportedFastEvaluator", "/options/evaluation");
            Check(options.evaluation_ != CalibrationEvaluation_::Value_::ScriptMC, "UnsupportedFastEvaluator", "/options/evaluation",
                  "deterministic curve execution; MC revaluation is separate");
            const auto derivative = options.jacobianMode_;
            Check(derivative == CalibrationDerivative_::Value_::FiniteDifference || derivative == CalibrationDerivative_::Value_::AAD ||
                      derivative == CalibrationDerivative_::Value_::Auto,
                  "DerivativeUnavailable", "/options/jacobian_mode", "qualified curve FD or AAD");
            Check(std::isfinite(options.differenceStep_) && options.differenceStep_ > 0., "InvalidCalibrationPlan", "/options/difference_step",
                  "finite positive FD step");
        }
        struct RowContext_ {
            CalibrationPreparationContext_ script_;
            CandidateCurveProvider_<double> curves_;
        };
        RowContext_ StructuralContext(const CurveInstrumentCalibrationSpec_& spec, const CalibrationObservation_& observation) {
            const auto& g = spec.geometry_;
            const auto& header = observation.Instrument()->Definition()->Spec().header_;
            const auto route = std::find_if(spec.market_.routes_.begin(), spec.market_.routes_.end(),
                                            [&](const auto& entry) { return Bytes(entry.id_) == Bytes(header.discountRoute_); });
            const auto diagnostic = RowDiagnostic(observation, CalibrationStage_::Value_::Prepare);
            Check(route != spec.market_.routes_.end(), "CalibrationContextMismatch", "/routes/" + header.discountRoute_, "bound discount route", "",
                  diagnostic);
            Check(!route->projection_ && route->collateral_ == header.collateralType_, "CalibrationContextMismatch",
                  "/routes/" + header.discountRoute_, "discount route with contract collateral", "", diagnostic);
            const auto geometry = NativeGeometry(spec);
            const auto initial = ResolveCurveCalibrationInitialGuess(geometry);
            CandidateCurveProvider_<double> provider(geometry, spec.market_.routes_, initial);
            Vector_<Date_> dates = {g.today_};
            for (const auto& date : Definition(spec).nodeDates_)
                if (date > g.today_)
                    dates.push_back(date);
            Vector_<> logDf;
            for (const auto& date : dates)
                logDf.push_back(std::log(provider.Discount(header.discountRoute_, g.today_, date)));
            CalibrationPreparationContext_ context;
            context.valuation_ = spec.valuation_;
            context.currency_ = Ccy_(g.ccy_);
            context.discountRoute_ = header.discountRoute_;
            context.curve_.reset(new GSRCurveData_("curve-structural-admission", g.today_, g.ccy_, dates, logDf, {}, {}));
            return {std::move(context), std::move(provider)};
        }
        void AdmittedObservation(const CalibrationCurveObservation_& observation,
                                 const CurveInstrumentCalibrationSpec_& spec,
                                 const std::optional<Date_>& pillar,
                                 const CandidateCurveProvider_<double>& provider) {
            if (observation.known_)
                return;
            const auto route = std::find_if(spec.market_.routes_.begin(), spec.market_.routes_.end(),
                                            [&](const auto& r) { return Bytes(r.id_) == Bytes(observation.route_); });
            Check(route != spec.market_.routes_.end() && route->projection_ == observation.projection_, "CalibrationContextMismatch",
                  "/routes/" + observation.route_, "explicit matching observation route", observation.index_, observation.source_);
            Check(!observation.projection_ || route->tenor_ == observation.tenor_, "CalibrationContextMismatch", "/routes/" + observation.route_,
                  "projection tenor matching authoritative index", observation.index_, observation.source_);
            if (observation.projection_)
                static_cast<void>(CurveScriptInternal::Observe(observation, provider));
            if (pillar)
                Check(*pillar >= observation.end_, "InvalidCalibrationPlan", "/instrument/pillar", "pillar covers every observation dependency",
                      observation.index_, observation.source_);
        }
        bool CoveredCashflow(const CalibrationCashflow_& flow, const CurveInstrumentCalibrationSpec_& spec, bool family) {
            return (family || (flow.amount_ && flow.observationIndex_.empty())) && flow.eventDate_ >= spec.geometry_.today_ &&
                   flow.eventDate_ <= spec.geometry_.knotDates_.back();
        }
        void AdmittedCashflows(const PreparedCalibrationInstrument_& plan,
                               const CurveInstrumentCalibrationSpec_& spec,
                               const CandidateCurveProvider_<double>& provider) {
            CalibrationDiagnostic_ d;
            d.instrumentId_ = plan.Instrument()->Id();
            d.measureId_ = plan.Measure().id_;
            Check(plan.Capability().affine_, "UnsupportedFastEvaluator", "/instrument", "deterministic shared cashflows", "", d);
            const auto* family = std::get_if<CalibrationCurveFamily_>(&plan.Descriptor());
            if (family)
                for (const auto& observation : (*family)->observations_)
                    AdmittedObservation(observation, spec, plan.Instrument()->Pillar(), provider);
            for (const auto& flow : plan.Cashflows())
                Check(CoveredCashflow(flow, spec, family != nullptr), "InvalidCalibrationPlan", "/instrument/cashflows",
                      "live fixed cashflows covered by native curve nodes", "", d);
            if (plan.Instrument()->Pillar())
                Check(*plan.Instrument()->Pillar() >= plan.Cashflows().back().eventDate_ &&
                          *plan.Instrument()->Pillar() <= spec.geometry_.knotDates_.back(),
                      "InvalidCalibrationPlan", "/instrument/pillar", "pillar covers cashflow dependency within curve layout", "", d);
        }
        PreparedCalibrationInstrument_ PrepareObservation(const CalibrationObservation_& observation,
                                                          const CurveInstrumentCalibrationSpec_& spec,
                                                          const CurveInstrumentCalibrationOptions_& options) try {
            CalibrationEvaluationOptions_ preparation;
            preparation.evaluation_ = options.evaluation_;
            preparation.derivativeMethod_ = CalibrationDerivative_::Value_::None;
            const auto context = StructuralContext(spec, observation);
            auto plan = PrepareCalibrationInstrument(observation.Instrument(), observation.MeasureId(), context.script_, preparation);
            AdmittedCashflows(plan, spec, context.curves_);
            return plan;
        } catch (const CalibrationError_& error) {
            CurveFailure({}, RowDiagnostic(observation, CalibrationStage_::Value_::Prepare, error.Failure().diagnostic_));
        }
        template <class T_>
        T_ CashflowMeasure(const PreparedCalibrationInstrument_& plan, const CandidateCurveProvider_<T_>& provider, const Date_& today) {
            if (std::holds_alternative<CalibrationCurveFamily_>(plan.Descriptor()))
                return CurveScriptInternal::Measure(plan, provider, today);
            T_ a(0.), b(0.), price(0.);
            const auto& route = plan.Instrument()->Definition()->Spec().header_.discountRoute_;
            for (const auto& flow : plan.Cashflows()) {
                const T_ discount = provider.Discount(route, today, flow.eventDate_);
                a += flow.a_ * discount;
                b += flow.b_ * discount;
                price += *flow.amount_ * discount;
            }
            if (plan.Measure().kind_ == CalibrationMeasureKind_::Value_::Price)
                return price;
            const auto& measure = plan.Measure();
            Check(std::isfinite(static_cast<double>(a)) && std::isfinite(static_cast<double>(b)), "CalibrationNumericalFailure", "/coefficients",
                  "finite affine coefficients");
            Check(measure.annuitySign_ && measure.minAbsAnnuity_, "UnsupportedCalibrationMeasure", "/measure", "affine measure controls");
            Check(static_cast<double>(a) * *measure.annuitySign_ >= *measure.minAbsAnnuity_, "InvalidCalibrationAnnuity", "/annuity",
                  "finite annuity with declared sign and minimum magnitude");
            return -b / a;
        }
        template <class T_>
        Vector_<T_>
        Measures(const CurveInstrumentCalibrationSpec_& spec, const Vector_<PreparedCalibrationInstrument_>& plans, const Vector_<T_>& parameters) {
            const CandidateCurveProvider_<T_> provider(NativeGeometry(spec), spec.market_.routes_, parameters);
            Vector_<T_> result;
            for (size_t i = 0; i < plans.size(); ++i) {
                try {
                    const T_ value = CashflowMeasure(plans[i], provider, spec.geometry_.today_);
                    Check(std::isfinite(static_cast<double>(value)), "CalibrationNumericalFailure", "/measure", "finite raw measure");
                    result.push_back(value);
                } catch (const CalibrationError_& error) {
                    CurveFailure({}, RowDiagnostic(*spec.observations_[i], CalibrationStage_::Value_::Evaluate, error.Failure().diagnostic_));
                }
            }
            return result;
        }
        double L2(const Vector_<>& values) {
            double result = 0.;
            for (double value : values)
                result = std::hypot(result, value);
            return result;
        }
        CurveInstrumentCalibrationResult_ LayoutResult(const CurveDefinition_& definition, const CalibrationObservationAxes_& axes) {
            CurveInstrumentCalibrationResult_ result;
            result.axes_ = axes;
            result.definition_ = definition;
            result.freeParameters_ = DescribeCurveFreeParameters(definition);
            return result;
        }
        Json_ FrozenMarket(const CurveInstrumentMarket_& market) {
            auto discounts = Json_::Array(), forwards = Json_::Array();
            for (const auto& [collateral, curve] : market.discountCurves_) {
                Check(static_cast<bool>(curve), "InvalidCalibrationPlan", "/market/discounts", "nonnull frozen background curve");
                discounts.array_.push_back(Obj({{"collateral", Str(collateral.String())}, {"curve", Parse(JSON::WriteString(*curve))}}));
            }
            for (const auto& [tenor, curve] : market.forwardCurves_) {
                Check(static_cast<bool>(curve), "InvalidCalibrationPlan", "/market/projections", "nonnull frozen background curve");
                forwards.array_.push_back(Obj({{"tenor", Str(tenor.String())}, {"curve", Parse(JSON::WriteString(*curve))}}));
            }
            return Obj({{"discounts", discounts}, {"projections", forwards}, {"routes", Array(market.routes_, [](const auto& route) {
                                                                                  return Obj({{"id", Str(route.id_)},
                                                                                              {"projection", Bool(route.projection_)},
                                                                                              {"collateral", Str(route.collateral_.String())},
                                                                                              {"tenor", route.projection_ ? Str(route.tenor_.String())
                                                                                                                          : Json_()}});
                                                                              })}});
        }
        String_ CurvePlanIdentity(const CurveInstrumentCalibrationSpec_& spec,
                                  const Vector_<PreparedCalibrationInstrument_>& plans,
                                  const CurveInstrumentCalibrationOptions_& options) {
            const auto geometry = NativeGeometry(spec);
            const CandidateCurveProvider_<double> provider(geometry, spec.market_.routes_, ResolveCurveCalibrationInitialGuess(geometry));
            return Fingerprint(Obj({{"adapter", Str("dal.native-curve-instruments/1")},
                                    {"curve", Parse(JSON::WriteString(*provider.Target()))},
                                    {"discount_target", Bool(spec.geometry_.calibrateDiscountCurve_)},
                                    {"collateral", Str(spec.geometry_.targetCollateral_.String())},
                                    {"tenor", spec.geometry_.calibrateDiscountCurve_ ? Json_() : Str(spec.geometry_.targetTenor_.String())},
                                    {"market", FrozenMarket(spec.market_)},
                                    {"plans", Array(plans, [](const auto& plan) { return Str(plan.PlanIdentity()); })},
                                    {"observations", Array(spec.observations_,
                                                           [](const auto& row) {
                                                               return Obj({{"id", Str(row->Id())},
                                                                           {"quote", Str(row->Quote()->Fingerprint())},
                                                                           {"scale", Num(row->ErrorScale())}});
                                                           })},
                                    {"derivative", Str(options.jacobianMode_.String())},
                                    {"difference_step", Num(options.differenceStep_)}}));
        }
        String_ CandidateIdentity(const String_& plan, const Vector_<>& parameters) {
            return Fingerprint(Obj({{"plan", Str(plan)}, {"parameters", Array(parameters, [](double value) { return Num(value); })}}));
        }
        void Residuals(CurveInstrumentCalibrationResult_* result) {
            for (size_t i = 0; i < result->modelMeasures_.size(); ++i) {
                const auto& row = result->axes_.observations_[i];
                const double raw = result->modelMeasures_[i] - row->Quote()->Value(), scaled = raw / row->ErrorScale();
                Check(std::isfinite(raw) && std::isfinite(scaled), "CalibrationNumericalFailure", "/residuals", "finite raw and scaled residuals", "",
                      RowDiagnostic(*row, CalibrationStage_::Value_::Evaluate));
                result->rawResiduals_.push_back(raw);
                result->scaledResiduals_.push_back(scaled);
                result->maxAbsScaledResidual_ = std::max(result->maxAbsScaledResidual_, std::abs(scaled));
            }
            result->rawNorm_ = L2(result->rawResiduals_);
            result->scaledNorm_ = L2(result->scaledResiduals_);
            Check(std::isfinite(result->rawNorm_) && std::isfinite(result->scaledNorm_), "CalibrationNumericalFailure", "/residual_norms",
                  "representable raw and scaled L2 norms", "",
                  RowDiagnostic(*result->axes_.observations_.front(), CalibrationStage_::Value_::Evaluate));
        }
    } // namespace

    namespace {
        bool ResolvedDifference(double parameter, double lower, double upper) {
            return std::isfinite(lower) && std::isfinite(upper) && lower < parameter && parameter < upper && std::isfinite(upper - lower) &&
                   upper - lower > 0.;
        }
        std::pair<double, double> DifferencePoints(double parameter, double bump, const CurveFreeParameter_& label, CalibrationDiagnostic_ d) {
            const double lower = parameter - bump, upper = parameter + bump;
            d.parameterKey_ = String_(label.component_.String()) + ":" + Date::ToString(label.date_);
            Check(ResolvedDifference(parameter, lower, upper), "CalibrationNumericalFailure", "/options/difference_step",
                  "finite resolvable two-sided native parameter perturbations", NumberText(bump), d);
            return {lower, upper};
        }
        template <class F_>
        Matrix_<> NativeDifferenceJacobian(const Vector_<>& parameters,
                                           int rows,
                                           double bump,
                                           const Vector_<CurveFreeParameter_>& labels,
                                           const CalibrationDiagnostic_& d,
                                           const F_& measures) {
            Matrix_<> result(rows, static_cast<int>(parameters.size()));
            auto up = parameters, down = parameters;
            for (int j = 0; j < result.Cols(); ++j) {
                const auto [lower, upper] = DifferencePoints(parameters[j], bump, labels[j], d);
                up[j] = upper;
                down[j] = lower;
                const auto plus = measures(up), minus = measures(down);
                Check(plus.size() == static_cast<size_t>(rows) && minus.size() == static_cast<size_t>(rows), "CalibrationNumericalFailure",
                      "/quote_jacobian", "fixed raw measure row count", "", d);
                for (int i = 0; i < rows; ++i)
                    result(i, j) = (plus[i] - minus[i]) / (upper - lower);
                up[j] = down[j] = parameters[j];
            }
            return result;
        }
    } // namespace

    PreparedCurveInstruments_::PreparedCurveInstruments_(const CurveInstrumentCalibrationSpec_& spec,
                                                         const CurveInstrumentCalibrationOptions_& options)
        : spec_(spec), options_(options), axes_(BuildCalibrationObservationAxes(spec.observations_)) {
        try {
            Check(!spec.observations_.empty(), "InvalidCalibrationObservation", "/observations", "nonempty ordered observations");
            PreparationSettings(spec, options);
            spec_ = ResolveKnots(spec_);
            definition_ = Dal::Definition(spec_);
            for (const auto& observation : spec_.observations_) {
                auto plan = PrepareObservation(*observation, spec_, options);
                auto capability = plan.Capability();
                capability.derivativeMethod_ = options_.jacobianMode_ == CalibrationDerivative_::Value_::AAD
                                                   ? CalibrationDerivative_::Value_::AAD
                                                   : CalibrationDerivative_::Value_::FiniteDifference;
                capabilities_.push_back(capability);
                plans_.push_back(std::move(plan));
            }
            planIdentity_ = CurvePlanIdentity(spec_, plans_, options_);
            frozenSpec_ = std::make_shared<const CurveInstrumentCalibrationSpec_>(spec_);
        } catch (const CalibrationError_& error) {
            auto d = error.Failure().diagnostic_;
            d.stage_ = CalibrationStage_::Value_::Prepare;
            CurveInstrumentCalibrationResult_ result;
            result.axes_ = axes_;
            CurveFailure(std::move(result), d);
        } catch (const std::exception& error) {
            CalibrationDiagnostic_ d;
            d.code_ = "InvalidCalibrationPlan";
            d.stage_ = CalibrationStage_::Value_::Prepare;
            d.field_ = "/geometry";
            d.message_ = String_(error.what());
            CurveInstrumentCalibrationResult_ result;
            result.axes_ = axes_;
            CurveFailure(std::move(result), d);
        }
    }
    std::unique_ptr<Underdetermined::Jacobian_> PreparedCurveInstruments_::Jacobian(const Vector_<>& parameters) const try {
        if (options_.jacobianMode_ != CalibrationDerivative_::Value_::AAD)
            return nullptr;
        auto result = TapeResidualJacobian(parameters, [&](const Vector_<AAD::Number_>& active) { return Measures(spec_, plans_, active); });
        ValidateNativeJacobian(static_cast<const XCurveJacobian_&>(*result).j_, spec_, DescribeCurveFreeParameters(definition_));
        return result;
    } catch (const CalibrationError_& error) {
        auto d = error.Failure().diagnostic_;
        d.stage_ = CalibrationStage_::Value_::Evaluate;
        CurveFailure(Describe(), d);
    } catch (const std::exception& error) {
        auto d = RowDiagnostic(*axes_.observations_.front(), CalibrationStage_::Value_::Evaluate);
        d.code_ = "CalibrationNumericalFailure";
        d.field_ = "/quote_jacobian";
        d.message_ = String_(error.what());
        CurveFailure(Describe(), d);
    }
    CurveInstrumentCalibrationResult_ PreparedCurveInstruments_::Describe() const {
        auto result = LayoutResult(definition_, axes_);
        result.resolvedSpec_ = frozenSpec_;
        result.capabilities_ = capabilities_;
        result.planIdentity_ = planIdentity_;
        return result;
    }
    CurveInstrumentCalibrationResult_ PreparedCurveInstruments_::Evaluate(const Vector_<>& parameters, bool derivatives) const try {
        auto result = Describe();
        result.parameters_ = parameters;
        result.modelMeasures_ = Measures(spec_, plans_, parameters);
        result.candidateIdentity_ = CandidateIdentity(planIdentity_, parameters);
        result.curve_ = Handle_<DiscountCurve_>(CandidateCurveProvider_<double>(NativeGeometry(spec_), spec_.market_.routes_, parameters).Target());
        Residuals(&result);
        if (derivatives) {
            result.derivativeMethod_ = options_.jacobianMode_ == CalibrationDerivative_::Value_::AAD
                                           ? CalibrationDerivative_::Value_::AAD
                                           : CalibrationDerivative_::Value_::FiniteDifference;
            if (auto jacobian = Jacobian(parameters)) {
                result.quoteJacobian_ = static_cast<XCurveJacobian_&>(*jacobian).j_;
            } else {
                result.quoteJacobian_ =
                    NativeDifferenceJacobian(parameters, static_cast<int>(plans_.size()), options_.differenceStep_, result.freeParameters_,
                                             RowDiagnostic(*spec_.observations_.front(), CalibrationStage_::Value_::Evaluate),
                                             [&](const auto& x) { return Measures(spec_, plans_, x); });
            }
            ValidateNativeJacobian(result.quoteJacobian_, spec_, result.freeParameters_);
        }
        result.report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Evaluate, axes_));
        return result;
    } catch (const CalibrationError_& error) {
        auto d = error.Failure().diagnostic_;
        d.stage_ = CalibrationStage_::Value_::Evaluate;
        CurveFailure(Describe(), d);
    } catch (const std::exception& error) {
        auto d = RowDiagnostic(*axes_.observations_.front(), CalibrationStage_::Value_::Evaluate);
        d.code_ = "CalibrationNumericalFailure";
        d.message_ = String_(error.what());
        CurveFailure(Describe(), d);
    }
    CurveInstrumentCalibrationCandidate_::CurveInstrumentCalibrationCandidate_(CurveInstrumentCalibrationResult_ result)
        : Storable_("CurveInstrumentCalibrationCandidate", "candidate"), result_(std::move(result)) {}
    void CurveInstrumentCalibrationCandidate_::Write(Archive::Store_&) const {
        Fail("CalibrationPersistenceUnsupported", "/candidate", "session-owned result; instrument serialization is separate");
    }

    namespace {
        Vector_<> NativeParameters(const Vector_<>& coordinates, const Vector_<>& initial, const Vector_<>& scales) {
            Check(coordinates.size() == initial.size() && scales.size() == initial.size(), "InvalidCalibrationPlan", "/coordinates",
                  "fixed native coordinate layout");
            auto result = initial;
            for (size_t j = 0; j < result.size(); ++j)
                result[j] += scales[j] * coordinates[j];
            return result;
        }
        class CurveFitFunction_ final : public Underdetermined::Function_ {
            const PreparedCurveInstruments_& plan_;
            const Vector_<>& initial_;
            const Vector_<>& scales_;

        public:
            CurveFitFunction_(const PreparedCurveInstruments_& plan, const Vector_<>& initial, const Vector_<>& scales)
                : plan_(plan), initial_(initial), scales_(scales) {}
            Vector_<> F(const Vector_<>& coordinates) const override {
                return plan_.Evaluate(NativeParameters(coordinates, initial_, scales_), false).rawResiduals_;
            }
            std::unique_ptr<Underdetermined::Jacobian_> Gradient(const Vector_<>& coordinates, const Vector_<>&) const override {
                auto result = plan_.Jacobian(NativeParameters(coordinates, initial_, scales_));
                if (result) {
                    auto& j = static_cast<XCurveJacobian_&>(*result).j_;
                    for (int i = 0; i < j.Rows(); ++i)
                        for (int k = 0; k < j.Cols(); ++k)
                            j(i, k) *= scales_[k];
                }
                return result;
            }
        };
        void ApproximateSettings(const CurveInstrumentSolver_& solver) {
            if (solver.solveMode_ == CurveSolveMode_::Value_::APPROXIMATE) {
                Check(solver.fitWeight_ && std::isfinite(*solver.fitWeight_) && *solver.fitWeight_ > 0., "InvalidCalibrationObjective",
                      "/solver/fit_weight", "explicit finite positive dimensionless lambda");
                Check(solver.fitNormTolerance_ && std::isfinite(*solver.fitNormTolerance_) && *solver.fitNormTolerance_ > 0.,
                      "InvalidCalibrationObjective", "/solver/fit_norm_tolerance", "explicit finite positive scaled L2 eta");
            }
        }
        void SolverSettings(const CurveInstrumentSolver_& solver, CurveInstrumentCalibrationResult_* result) {
            Token(solver.solveMode_, "InvalidCalibrationObjective", "/solver/mode");
            Check(solver.maxEvaluations_ > 0 && solver.maxRestarts_ > 0, "InvalidCalibrationObjective", "/solver/budget", "positive budgets");
            Check(std::isfinite(solver.smoothingWeight_) && solver.smoothingWeight_ > 0., "InvalidCalibrationObjective", "/solver/smoothing_weight",
                  "finite positive native smoothing metric");
            result->solveMode_ = solver.solveMode_;
            result->fitWeight_ = solver.fitWeight_;
            result->fitNormTolerance_ = solver.fitNormTolerance_;
            ApproximateSettings(solver);
            result->parameterScales_ = solver.parameterScales_.empty() ? Vector_<>(result->initialParameters_.size(), 1.) : solver.parameterScales_;
            Check(result->parameterScales_.size() == result->initialParameters_.size(), "InvalidCalibrationObjective", "/solver/parameter_scales",
                  "one parameter scale per free native coordinate");
            for (double scale : result->parameterScales_)
                Check(std::isfinite(scale) && scale > 0., "InvalidCalibrationObjective", "/solver/parameter_scales", "finite positive scales");
        }
        std::unique_ptr<Sparse::TriDiagonal_> FitWeights(const CurveInstrumentSolver_& solver, CurveInstrumentCalibrationResult_* result) {
            Vector_<Date_> dates;
            for (const auto& parameter : result->freeParameters_)
                dates.push_back(parameter.date_);
            auto weights = BuildCurveCalibrationWeights(dates, 1, solver.smoothingWeight_);
            const int n = weights->Size();
            result->nativeWeights_.Resize(n, n);
            result->weights_.Resize(n, n);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j <= i; ++j) {
                    const double native = (*weights)(i, j);
                    const double value = native * result->parameterScales_[i] * result->parameterScales_[j];
                    Check(std::isfinite(value), "InvalidCalibrationObjective", "/solver/weights", "representable S'W_theta*S");
                    Check(native == 0. || value != 0., "InvalidCalibrationObjective", "/solver/weights",
                          "nonzero metric entries remain representable");
                    result->nativeWeights_(i, j) = result->nativeWeights_(j, i) = native;
                    result->weights_(i, j) = result->weights_(j, i) = value;
                    if (i - j <= 1) {
                        weights->Set(i, j, value);
                        weights->Set(j, i, value);
                    }
                }
            result->objectiveIdentity_ = Fingerprint(Obj({{"plan", Str(result->planIdentity_)},
                                                          {"mode", Str(result->solveMode_.String())},
                                                          {"theta0", Array(result->initialParameters_, [](double value) { return Num(value); })},
                                                          {"scales", Array(result->parameterScales_, [](double value) { return Num(value); })},
                                                          {"smoothing_weight", Num(solver.smoothingWeight_)},
                                                          {"lambda", Optional(result->fitWeight_, [](double value) { return Num(value); })},
                                                          {"eta", Optional(result->fitNormTolerance_, [](double value) { return Num(value); })}}));
            return weights;
        }
        void FitState(const PreparedCurveInstruments_& plan,
                      const Underdetermined::FitResult_& solved,
                      const CurveInstrumentCalibrationOptions_& options,
                      CurveInstrumentCalibrationResult_* result) {
            result->evaluations_ = solved.evaluations_;
            result->gradients_ = solved.gradients_;
            result->termination_ = solved.termination_;
            result->fitWithinTolerance_ = solved.fitWithinTolerance_;
            if (solved.parameters_.empty())
                return;
            result->coordinates_ = solved.parameters_;
            result->parameters_ = NativeParameters(solved.parameters_, result->initialParameters_, result->parameterScales_);
            result->candidateIdentity_ = CandidateIdentity(result->planIdentity_, result->parameters_);
            ++result->diagnosticEvaluations_;
            const auto values = plan.Evaluate(result->parameters_, false);
            result->curve_ = values.curve_;
            result->modelMeasures_ = values.modelMeasures_;
            result->rawResiduals_ = solved.rawResiduals_;
            result->scaledResiduals_ = solved.scaledResiduals_;
            result->rawNorm_ = L2(solved.rawResiduals_);
            result->scaledNorm_ = L2(solved.scaledResiduals_);
            for (double value : solved.scaledResiduals_)
                result->maxAbsScaledResidual_ = std::max(result->maxAbsScaledResidual_, std::abs(value));
            if (solved.gradients_ > 0)
                result->derivativeMethod_ = options.jacobianMode_ == CalibrationDerivative_::Value_::AAD
                                                ? CalibrationDerivative_::Value_::AAD
                                                : CalibrationDerivative_::Value_::FiniteDifference;
        }
        void DiagnosticJacobian(const PreparedCurveInstruments_& plan,
                                const CurveInstrumentCalibrationOptions_& options,
                                CurveInstrumentCalibrationResult_* result) {
            if (!options.computeQuoteJacobian_ || !result->fitWithinTolerance_)
                return;
            result->diagnosticEvaluations_ +=
                options.jacobianMode_ == CalibrationDerivative_::Value_::AAD ? 1 : 1 + 2 * static_cast<int>(result->parameters_.size());
            result->diagnosticGradients_ = 1;
            result->quoteJacobian_ = plan.Evaluate(result->parameters_).quoteJacobian_;
            result->derivativeMethod_ = options.jacobianMode_ == CalibrationDerivative_::Value_::AAD
                                            ? CalibrationDerivative_::Value_::AAD
                                            : CalibrationDerivative_::Value_::FiniteDifference;
        }
        void ValidateStageContexts(const MultiCurveInstrumentCalibrationSpec_& spec) {
            Check(!spec.stages_.empty(), "InvalidCalibrationPlan", "/stages", "nonempty ordered curve stages");
            const auto& first = spec.stages_.front().geometry_;
            for (size_t i = 1; i < spec.stages_.size(); ++i) {
                const auto& geometry = spec.stages_[i].geometry_;
                Check(geometry.ccy_ == first.ccy_ && geometry.today_ == first.today_, "CalibrationContextMismatch",
                      "/stages/" + OrdinalText(i) + "/geometry", "same currency and valuation across ordered curve stages");
            }
        }
        [[noreturn]] void UnsuccessfulFit(const Underdetermined::FitResult_& solved, const CurveInstrumentCalibrationResult_& result) {
            auto d = RowDiagnostic(*result.axes_.observations_.front(), CalibrationStage_::Value_::Solve);
            d.code_ = solved.termination_;
            d.field_ = "/solver";
            d.expected_ = result.solveMode_ == CurveSolveMode_::Value_::EXACT ? "finite scaled infinity norm <=1" : "finite scaled L2 norm <=eta";
            d.actual_ = "raw norm=" + NumberText(result.rawNorm_) + "; scaled L2=" + NumberText(result.scaledNorm_) +
                        "; evaluations=" + OrdinalText(result.evaluations_) + "; gradients=" + OrdinalText(result.gradients_);
            if (solved.cause_)
                try {
                    std::rethrow_exception(solved.cause_);
                } catch (const CalibrationError_& error) {
                    d = error.Failure().diagnostic_;
                    d.stage_ = CalibrationStage_::Value_::Solve;
                } catch (const std::exception& error) {
                    d.message_ = String_(error.what());
                }
            CurveFailure(result, d);
        }
    } // namespace
    CurveInstrumentCalibrationResult_ CalibrateCurveInstruments(const CurveInstrumentCalibrationSpec_& spec,
                                                                const CurveInstrumentCalibrationOptions_& options) {
        CurveInstrumentCalibrationResult_ result;
        try {
            result.axes_ = BuildCalibrationObservationAxes(spec.observations_);
            const PreparedCurveInstruments_ plan(spec, options);
            result = plan.Describe();
            result.initialParameters_ = ResolveCurveCalibrationInitialGuess(NativeGeometry(plan.Spec()));
            SolverSettings(spec.solver_, &result);
            const auto weights = FitWeights(spec.solver_, &result);
            const CurveFitFunction_ function(plan, result.initialParameters_, result.parameterScales_);
            Vector_<> scales;
            for (const auto& row : result.axes_.observations_)
                scales.push_back(row->ErrorScale());
            Dictionary_ dictionary;
            dictionary.Insert(KEY_MAX_EVALUATIONS, Cell_(static_cast<double>(spec.solver_.maxEvaluations_)));
            dictionary.Insert(KEY_MAX_RESTARTS, Cell_(static_cast<double>(spec.solver_.maxRestarts_)));
            Underdetermined::FitSettings_ settings;
            settings.exact_ = spec.solver_.solveMode_ == CurveSolveMode_::Value_::EXACT;
            settings.fitWeight_ = spec.solver_.fitWeight_;
            settings.fitNormTolerance_ = spec.solver_.fitNormTolerance_;
            settings.differenceStep_ = options.differenceStep_;
            const auto solved = Underdetermined::Fit(function, Vector_<>(result.initialParameters_.size(), 0.), scales, *weights,
                                                     UnderdeterminedControls_(dictionary), settings);
            FitState(plan, solved, options, &result);
            if (!solved.fitWithinTolerance_)
                UnsuccessfulFit(solved, result);
            DiagnosticJacobian(plan, options, &result);
            result.report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Solve, result.axes_));
            return result;
        } catch (const CalibrationError_& error) {
            CurveFailure(std::move(result), error.Failure().diagnostic_);
        } catch (const std::exception& error) {
            CalibrationDiagnostic_ d;
            d.code_ = "CalibrationNumericalFailure";
            d.stage_ = result.initialParameters_.empty() ? CalibrationStage_(CalibrationStage_::Value_::Prepare)
                                                         : CalibrationStage_(CalibrationStage_::Value_::Solve);
            d.message_ = String_(error.what());
            CurveFailure(std::move(result), d);
        }
    }
    MultiCurveInstrumentCalibrationResult_ CalibrateCurveInstruments(const MultiCurveInstrumentCalibrationSpec_& spec,
                                                                     const CurveInstrumentCalibrationOptions_& options) {
        ValidateStageContexts(spec);
        MultiCurveInstrumentCalibrationResult_ result;
        for (auto stage : spec.stages_) {
            for (const auto& entry : result.discountCurves_)
                stage.market_.discountCurves_.insert(entry);
            for (const auto& entry : result.forwardCurves_)
                stage.market_.forwardCurves_.insert(entry);
            if (!stage.geometry_.calibrateDiscountCurve_ && !stage.geometry_.baseCurve_) {
                const auto found = stage.market_.discountCurves_.find(stage.geometry_.targetCollateral_);
                Check(found != stage.market_.discountCurves_.end(), "CalibrationContextMismatch", "/stages/base",
                      "previously calibrated or explicit discount base");
                stage.geometry_.baseCurve_ = found->second;
            }
            auto calibrated = CalibrateCurveInstruments(stage, options);
            if (stage.geometry_.calibrateDiscountCurve_)
                result.discountCurves_[stage.geometry_.targetCollateral_] = calibrated.curve_;
            else
                result.forwardCurves_[stage.geometry_.targetTenor_] = calibrated.curve_;
            result.stages_.push_back(std::move(calibrated));
        }
        return result;
    }
    LegacyCurveObjectiveMapping_ ImportLegacyCurveObjective(const CurveCalibrationSpec_& spec, const CurveCalibrationOptions_& options) {
        ValidateCurveCalibrationSpec(spec);
        LegacyCurveObjectiveMapping_ result;
        result.nativeSpec_ = spec;
        result.nativeOptions_ = options;
        result.rows_ = static_cast<int>(spec.instruments_.size());
        result.legacyTau_ = spec.tolerance_;
        result.legacyFitTolerance_ = spec.fitTolerance_;
        const double ratio = spec.tolerance_ / spec.fitTolerance_;
        const double weight = static_cast<double>(result.rows_) * ratio * ratio;
        if (std::isfinite(weight) && weight > 0.)
            result.fitWeight_ = weight;
        result.fitNormTolerance_ = spec.fitTolerance_;
        return result;
    }
    CurveCalibrationResult_ CalibrateLegacyCurve(const LegacyCurveObjectiveMapping_& imported) {
        return CalibrateYieldCurve(imported.nativeSpec_, imported.nativeOptions_);
    }
} // namespace Dal
