#include <dal/platform/platform.hpp>

#include <set>

#include <dal/curve/calibration_internal.hpp>
#include <dal/curve/curveinstrumentcontext_internal.hpp>
#include <dal/curve/curveinstrumentevaluation_internal.hpp>
#include <dal/curve/curvenativepartials_internal.hpp>
#include <dal/curve/curvescriptevaluation.hpp>

namespace Dal {
    using namespace CalibrationInternal;
    using namespace CurveInstrumentInternal;
    namespace {
        struct EvaluationPlan_ {
            CurveInstrumentCalibrationSpec_ spec_;
            std::shared_ptr<const CandidateCurveGeometry_> geometry_;
            Vector_<PreparedCalibrationInstrument_> measures_;
        };
        EvaluationPlan_ Prepare(const Handle_<CalibrationInstrument_>& instrument, const CurveScriptEvaluationRequest_& request) {
            Check(bool(instrument), "InvalidCalibrationPlan", "/instrument", "nonnull instrument");
            Check(request.geometry_.knotPolicy_ == CurveKnotPolicy_::Value_::INPUT, "InvalidCalibrationPlan", "/geometry/knot_policy",
                  "explicit INPUT geometry for direct evaluation");
            Check(!request.measureIds_.empty(), "UnsupportedCalibrationMeasure", "/measure_ids", "nonempty ordered measures");
            Token(request.derivativeMethod_, "DerivativeUnavailable", "/derivative_method");
            Check(request.derivativeMethod_ != CalibrationDerivative_::Value_::Analytic, "DerivativeUnavailable", "/derivative_method",
                  "None, AAD, FiniteDifference or Auto");
            EvaluationPlan_ result;
            auto& spec = result.spec_;
            spec.geometry_ = request.geometry_;
            spec.market_ = request.market_;
            spec.valuation_ = request.valuation_;
            spec.solver_.initialGuessPerNode_ = request.parameters_;
            if (request.compoundedOis_) {
                Check(Bytes(request.compoundedOis_->Spec().instrumentId_) == Bytes(instrument->Id()), "CompoundedOisSourceMismatch",
                      "/compounded_ois", "frozen tables for the requested instrument");
                spec.compoundedOis_.push_back(request.compoundedOis_);
            }
            spec = FreezeOvernightContext(std::move(spec));
            CurveInstrumentCalibrationOptions_ options;
            options.evaluation_ = request.evaluation_;
            options.jacobianMode_ = request.derivativeMethod_ == CalibrationDerivative_::Value_::AAD
                                        ? CalibrationDerivative_::Value_::AAD
                                        : CalibrationDerivative_::Value_::FiniteDifference;
            options.differenceStep_ = request.differenceStep_;
            std::set<std::string> ids;
            for (const auto& id : request.measureIds_) {
                Check(ids.insert(Bytes(id)).second, "UnsupportedCalibrationMeasure", "/measure_ids", "distinct case-sensitive measure IDs");
                auto prepared = PrepareCurvePrice(spec, instrument, id, options);
                result.geometry_ = prepared.geometry_;
                result.measures_.push_back(std::move(prepared.plan_));
            }
            return result;
        }
        template <class T_> Vector_<T_> Values(const EvaluationPlan_& plan, const CandidateCurveProvider_<T_>& provider) {
            Vector_<T_> result;
            CurveScriptInternal::EvaluationScratch_<T_> scratch;
            std::optional<CurveScriptInternal::Affine_<T_>> coefficients;
            if (plan.measures_.size() == 2 && CurveScriptInternal::CanShareCoefficients(plan.measures_[0], plan.measures_[1]))
                coefficients = CurveScriptInternal::Coefficients(plan.measures_[0], provider, plan.spec_.geometry_.today_, &scratch);
            for (const auto& measure : plan.measures_) {
                const T_ value = coefficients ? CurveScriptInternal::Measure(measure, *coefficients)
                                              : CashflowMeasure(measure, provider, plan.spec_.geometry_.today_, &scratch);
                Check(std::isfinite(static_cast<double>(value)), "CalibrationNumericalFailure", "/measure", "finite raw measure", "",
                      Diagnostic(*measure.Instrument(), measure.Measure().id_));
                result.push_back(value);
            }
            return result;
        }
        template <class T_> Vector_<T_> Values(const EvaluationPlan_& plan, const Vector_<T_>& parameters) {
            return Values(plan, CandidateCurveProvider_<T_>(plan.geometry_, parameters));
        }
        Matrix_<> Partials(const EvaluationPlan_& plan, const CurveScriptEvaluationRequest_& request, const Vector_<CurveFreeParameter_>& labels) {
            if (request.derivativeMethod_ == CalibrationDerivative_::Value_::None)
                return {};
            if (request.derivativeMethod_ == CalibrationDerivative_::Value_::AAD) {
                const auto jacobian = TapeResidualJacobian(request.parameters_, [&](const auto& active) { return Values(plan, active); });
                return static_cast<const XCurveJacobian_&>(*jacobian).j_;
            }
            return NativeDifferenceJacobian(request.parameters_, static_cast<int>(plan.measures_.size()), request.differenceStep_, labels,
                                            Diagnostic(*plan.measures_.front().Instrument(), plan.measures_.front().Measure().id_),
                                            [&](const auto& parameters) { return Values(plan, parameters); });
        }
        void ValidatePartials(const CurveScriptEvaluationResult_& result) {
            for (int i = 0; i < result.nativePartials_.Rows(); ++i)
                for (int j = 0; j < result.nativePartials_.Cols(); ++j) {
                    auto d = Diagnostic(*result.instrument_, result.measureIds_[i]);
                    d.stage_ = CalibrationStage_::Value_::Evaluate;
                    const auto& parameter = result.freeParameters_[j];
                    d.parameterKey_ = String_(parameter.component_.String()) + ":" + Date::ToString(parameter.date_);
                    Check(std::isfinite(result.nativePartials_(i, j)), "CalibrationNumericalFailure", "/native_partials", "finite raw native partial",
                          "", d);
                }
        }
        Vector_<CurveScriptCashflowResult_> TableCashflows(const PreparedCompoundedOis_& tables) {
            Vector_<CurveScriptCashflowResult_> result;
            const auto& s = tables.Spec();
            for (size_t i = 0; i < s.fixedLeg_.size(); ++i) {
                const auto& state = tables.FixedPayments()[i];
                result.push_back({"fixed_leg", "", i, i + 3, 0, s.fixedLeg_[i].payment_, state.included_, state.settled_});
            }
            for (size_t i = 0; i < s.coupons_.size(); ++i) {
                const auto& row = s.coupons_[i];
                const auto& state = tables.CouponPayments()[i];
                result.push_back({"coupons", row.couponId_, i, result.size() + 3, 0, row.payment_, state.included_, state.settled_});
            }
            std::map<Date_, size_t> statements;
            for (auto& row : result)
                row.statement_ = statements[row.payment_]++;
            return result;
        }
        Vector_<CurveScriptObservationResult_> TableObservations(const PreparedCompoundedOis_& tables) {
            Vector_<CurveScriptObservationResult_> result;
            const auto& s = tables.Spec();
            const auto payments = TableCashflows(tables);
            for (size_t i = 0; i < s.observations_.size(); ++i) {
                const auto& row = s.observations_[i];
                const auto& info = tables.Observations()[i];
                const auto& payment = payments[s.fixedLeg_.size() + info.couponRow_];
                result.push_back({i,
                                  payment.sourceRow_,
                                  payment.statement_,
                                  row.couponId_,
                                  s.fixingIndex_,
                                  s.projectionRoute_,
                                  "Settled",
                                  row.fixingTime_,
                                  row.start_,
                                  row.end_,
                                  row.rateStart_,
                                  row.rateEnd_,
                                  row.accrual_,
                                  info.rateAccrual_,
                                  {}});
            }
            return result;
        }
        Vector_<> ObservationDetails(const CalibrationCurveFamilyDescriptor_& family,
                                     const CandidateCurveProvider_<double>& provider,
                                     CurveScriptEvaluationResult_* result) {
            Vector_<> rates;
            for (const auto& observation : family.observations_) {
                const auto rate = CurveScriptInternal::Observe(observation, provider);
                rates.push_back(rate);
                if (observation.overnight_) {
                    auto& row = result->observations_[observation.tableRow_];
                    row.rate_ = rate;
                    row.source_ = observation.known_ ? "Historical" : "Projection";
                }
            }
            return rates;
        }
        void Details(const EvaluationPlan_& plan, const CandidateCurveProvider_<double>& provider, CurveScriptEvaluationResult_* result) {
            const auto& first = plan.measures_.front();
            const auto* family = std::get_if<CalibrationCurveFamily_>(&first.Descriptor());
            const auto& tables = plan.spec_.compoundedOis_;
            if (!tables.empty()) {
                result->cashflows_ = TableCashflows(*tables.front());
                result->observations_ = TableObservations(*tables.front());
            }
            const auto today = plan.spec_.geometry_.today_;
            const auto& route = provider.Route(first.Instrument()->Definition()->Spec().header_.discountRoute_, false);
            const auto binding = provider.Bind(route);
            CurveScriptInternal::EvaluationScratch_<double> scratch;
            const auto rates = family ? ObservationDetails(**family, provider, result) : Vector_<>();
            for (size_t i = 0; i < first.Cashflows().size(); ++i) {
                const auto& flow = first.Cashflows()[i];
                double amount = flow.amount_.value_or(0.);
                if (family) {
                    const auto ab =
                        CurveScriptInternal::EvaluatePayment<double>((*family)->payments_[i], [&](size_t j) { return rates[j]; }, &scratch);
                    amount = ab.b_ + (*family)->coupon_ * ab.a_;
                }
                CurveScriptCashflowResult_ row{"source",
                                               "",
                                               i,
                                               static_cast<size_t>(flow.sourceRow_),
                                               static_cast<size_t>(flow.statement_),
                                               flow.eventDate_,
                                               true,
                                               false,
                                               amount,
                                               provider.Discount(binding, route, today, flow.eventDate_)};
                row.pv_ = amount * *row.discount_;
                if (tables.empty())
                    result->cashflows_.push_back(row);
                else {
                    auto& destination = result->cashflows_[row.sourceRow_ - 3];
                    destination.amount_ = row.amount_;
                    destination.discount_ = row.discount_;
                    destination.pv_ = row.pv_;
                }
            }
        }
        CalibrationReportSnapshot_ Report(const CurveScriptEvaluationResult_& result, const EvaluationPlan_& plan) {
            CalibrationReportSnapshot_ data;
            data.summary_.domain_ = "CurveScript";
            data.summary_.status_ = "Evaluated";
            data.summary_.contextIdentity_ = result.contextIdentity_;
            data.execution_.valuationDate_ = plan.spec_.valuation_.evaluationDate_;
            data.execution_.todayFixingPolicy_ = plan.spec_.valuation_.todayFixingPolicy_.String();
            for (size_t i = 0; i < plan.measures_.size(); ++i) {
                const auto& measure = plan.measures_[i];
                CalibrationReportObservation_ row;
                row.instrument_ = result.instrument_;
                row.measureId_ = result.measureIds_[i];
                row.unit_ = result.measureUnits_[i];
                row.representation_ = measure.Measure().kind_ == CalibrationMeasureKind_::Value_::Price ? "Price" : "Scalar";
                row.backend_ = measure.Capability().backend_.String();
                row.derivativeMethod_ = result.derivativeMethod_.String();
                row.modelValue_ = result.values_[i];
                row.planIdentity_ = measure.PlanIdentity();
                data.observations_.push_back(row);
                data.plans_.push_back({{},
                                       row.measureId_,
                                       measure.Capability().ruleId_,
                                       row.backend_,
                                       row.derivativeMethod_,
                                       true,
                                       {},
                                       {},
                                       result.instrument_->Definition()->SourceIdentity(),
                                       result.instrument_->Definition()->DefinitionIdentity(),
                                       result.instrument_->BindingIdentity(),
                                       measure.CompilerIdentity(),
                                       measure.ContextIdentity(),
                                       measure.PlanIdentity()});
            }
            for (size_t i = 0; i < result.freeParameters_.size(); ++i) {
                const auto& p = result.freeParameters_[i];
                data.parameters_.push_back({String_(p.component_.String()) + ":" + Date::ToString(p.date_),
                                            p.component_ == CurveFreeParameterComponent_::Value_::LOG_DISCOUNT_FACTOR ? "Dimensionless" : "Rate",
                                            {},
                                            result.parameters_[i]});
            }
            return data;
        }
    } // namespace
    CurveScriptEvaluationResult_ EvaluateCurveScriptInstrument(const Handle_<CalibrationInstrument_>& instrument,
                                                               const CurveScriptEvaluationRequest_& request) {
        auto stage = CalibrationStage_(CalibrationStage_::Value_::Prepare);
        try {
            const auto plan = Prepare(instrument, request);
            stage = CalibrationStage_::Value_::Evaluate;
            CurveScriptEvaluationResult_ result;
            result.instrument_ = plan.measures_.front().Instrument();
            result.parameters_ = request.parameters_;
            result.measureIds_ = request.measureIds_;
            for (const auto& measure : plan.measures_)
                result.measureUnits_.push_back(measure.Measure().unit_.Token());
            const auto& g = plan.spec_.geometry_;
            const auto definition =
                MakeCurveDefinition(g.curveName_, g.ccy_, g.parameterization_, g.logDfScheme_, g.knotDates_, g.today_, g.liborBasis_);
            result.freeParameters_ = DescribeCurveFreeParameters(definition);
            const CandidateCurveProvider_<double> provider(plan.geometry_, request.parameters_);
            result.curve_ = Handle_<DiscountCurve_>(provider.Target());
            result.values_ = Values(plan, provider);
            result.nativePartials_ = Partials(plan, request, result.freeParameters_);
            result.derivativeMethod_ = request.derivativeMethod_ == CalibrationDerivative_::Value_::Auto
                                           ? CalibrationDerivative_(CalibrationDerivative_::Value_::FiniteDifference)
                                           : request.derivativeMethod_;
            ValidatePartials(result);
            Details(plan, provider, &result);
            result.contextIdentity_ = plan.geometry_->Identity();
            result.candidateIdentity_ = Fingerprint(Obj({{"context", Str(result.contextIdentity_)},
                                                         {"parameters", Numbers(request.parameters_)},
                                                         {"instrument", Str(result.instrument_->InstrumentIdentity())}}));
            result.report_.reset(new CalibrationReport_(stage, {}, {}, Report(result, plan)));
            return result;
        } catch (const CalibrationError_& error) {
            auto d = error.Failure().diagnostic_;
            d.stage_ = stage;
            if (instrument)
                d.instrumentId_ = instrument->Id();
            Fail(d.code_, d.field_.value_or("/evaluation"), d.expected_.value_or("valid deterministic curve evaluation"), d.actual_.value_or(""), d);
        } catch (const std::exception& error) {
            CalibrationDiagnostic_ d;
            d.stage_ = stage;
            if (instrument)
                d.instrumentId_ = instrument->Id();
            Fail("CalibrationNumericalFailure", "/evaluation", "valid deterministic curve evaluation", String_(error.what()), d);
        }
    }
} // namespace Dal
