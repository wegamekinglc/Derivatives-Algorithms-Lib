#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/curve/xccygaussianmomentmath.hpp>
#include <dal/curve/xccyscript_internal.hpp>
#include <dal/time/dateutils.hpp>

namespace Dal {
    namespace XccyMomentInternal {
        using namespace CalibrationInternal;
        const CalibrationXccyDescriptor_& Descriptor(const PreparedCalibrationInstrument_& prepared) {
            const auto* descriptor = std::get_if<CalibrationXccy_>(&prepared.Descriptor());
            Check(descriptor && *descriptor, "InvalidCalibrationPlan", "/plan", "complete admitted XCCY source descriptor");
            return **descriptor;
        }
        std::optional<double> Known(const CalibrationXccyDescriptor_& descriptor, const String_& name, const DateTime_& time) {
            if (time > descriptor.valuationTime_)
                return {};
            const auto known = descriptor.fixings_->Find(name, time);
            Check(time == descriptor.valuationTime_ || known.has_value(), "MissingFixing", "/context/fixings", "exact strict-past fixing", name);
            return known;
        }
        XccyGaussianMomentTerm_ Notional(const CalibrationXccyDescriptor_& descriptor, size_t period, const Date_& payment, bool foreign) {
            const auto& plan = descriptor.plan_;
            const auto& config = plan.config_;
            const double ratio = config.foreignNotional_ / config.domesticNotional_;
            XccyGaussianMomentTerm_ term;
            term.payment_ = payment;
            term.foreignPayment_ = foreign;
            term.weight_ = config.domesticNotional_;
            if (foreign) {
                term.weight_ *= ratio;
            } else if (period > 0 && config.notionalMode_ != XccyNotionalMode_::Value_::FIXED) {
                const auto& reset = plan.resets_[period - 1];
                term.weight_ *= ratio;
                const auto known = Known(descriptor, FxIndexName(config.pair_), reset.fxFixingTime_);
                if (known) {
                    Check(std::isfinite(*known) && *known > 0., "CalibrationNumericalFailure", "/context/fixings", "positive finite reset FX");
                    term.weight_ *= *known;
                } else
                    term.reset_ = reset.fxFixingTime_.Date();
            }
            return term;
        }
        void CouponRate(const CalibrationXccyDescriptor_& descriptor,
                        const XccyCouponPeriod_& period,
                        bool foreign,
                        XccyGaussianMomentTerm_ term,
                        Vector_<XccyGaussianMomentTerm_>* terms) {
            const auto known = Known(descriptor, period.rateIndexName_, period.rateFixingTime_);
            if (known) {
                term.weight_ *= *known;
                terms->push_back(std::move(term));
                return;
            }
            term.weight_ /= period.accrual_.dcf_;
            auto growth = term;
            growth.growth_ = period;
            growth.foreignRate_ = foreign;
            const auto& index = foreign ? descriptor.plan_.config_.convention_.foreignIndex_ : descriptor.plan_.config_.convention_.domesticIndex_;
            growth.projectionRoute_ = String::FromInt(index.forecastTenor_.Months()) + "M";
            terms->push_back(std::move(growth));
            term.weight_ = -term.weight_;
            terms->push_back(std::move(term));
        }
        void Coupons(const CalibrationXccyDescriptor_& descriptor, bool foreign, Vector_<XccyGaussianMomentTerm_>* terms) {
            const auto& plan = descriptor.plan_;
            const auto& periods = foreign ? plan.foreignPeriods_ : plan.domesticPeriods_;
            const bool spread = foreign == plan.config_.convention_.spreadOnForeignLeg_;
            const double direction = (descriptor.receiveNonSpreadPaySpread_ ? 1. : -1.) * (spread ? -1. : 1.);
            for (size_t i = 0; i < periods.size(); ++i) {
                const auto& period = periods[i];
                if (period.schedule_.paymentDate_ < descriptor.valuationTime_.Date())
                    continue;
                auto term = Notional(descriptor, i, period.schedule_.paymentDate_, foreign);
                term.weight_ *= direction * period.accrual_.dcf_;
                if (spread) {
                    auto annuity = term;
                    annuity.annuity_ = true;
                    terms->push_back(std::move(annuity));
                }
                CouponRate(descriptor, period, foreign, std::move(term), terms);
            }
        }
        void Principal(const CalibrationXccyDescriptor_& descriptor, const Date_& date, bool final, Vector_<XccyGaussianMomentTerm_>* terms) {
            if (date < descriptor.valuationTime_.Date())
                return;
            const auto& plan = descriptor.plan_;
            const double sign =
                (descriptor.receiveNonSpreadPaySpread_ ? 1. : -1.) * (plan.config_.convention_.spreadOnForeignLeg_ ? 1. : -1.) * (final ? 1. : -1.);
            for (const bool foreign : {false, true}) {
                auto term = Notional(descriptor, final ? plan.domesticPeriods_.size() - 1 : 0, date, foreign);
                term.weight_ *= foreign ? -sign : sign;
                terms->push_back(std::move(term));
            }
        }
        void Mtm(const CalibrationXccyDescriptor_& descriptor, Vector_<XccyGaussianMomentTerm_>* terms) {
            const auto& plan = descriptor.plan_;
            if (plan.config_.notionalMode_ != XccyNotionalMode_::Value_::MARK_TO_MARKET)
                return;
            const double sign = (descriptor.receiveNonSpreadPaySpread_ ? 1. : -1.) * (plan.config_.convention_.spreadOnForeignLeg_ ? 1. : -1.);
            for (size_t i = 0; i < plan.resets_.size(); ++i) {
                const auto date = plan.resets_[i].effectiveDate_;
                if (date < descriptor.valuationTime_.Date())
                    continue;
                auto next = Notional(descriptor, i + 1, date, false), prior = Notional(descriptor, i, date, false);
                next.weight_ *= sign;
                prior.weight_ *= -sign;
                terms->push_back(std::move(next));
                terms->push_back(std::move(prior));
            }
        }
        void Match(const XccyGaussianMomentProgram_& program, const XccyGaussianModelData_& model) {
            const auto& descriptor = Descriptor(program.Prepared());
            const auto& market = *model.market_;
            const auto& header = program.Prepared().Instrument()->Definition()->Spec().header_;
            Check(market.valuationTime_ == descriptor.valuationTime_ && market.pair_ == descriptor.plan_.config_.pair_ &&
                      market.collateralCurrency_ == header.collateralCurrency_ && market.discountRoute_ == header.discountRoute_,
                  "CalibrationContextMismatch", "/model/market", "same valuation, pair, domestic collateral and source discount route");
        }
        template <class T_> void ValidateParameters(const XccyGaussianMomentInputs_& inputs, const Vector_<T_>& parameters) {
            Check(parameters.size() == inputs.values_.size(), "CalibrationContextMismatch", "/model/parameters", "complete moment input layout");
            for (size_t i = 0; i < parameters.size(); ++i)
                Check(std::isfinite(Value(parameters[i])), "CalibrationNumericalFailure", "/model/parameters", "finite numeric model input",
                      inputs.labels_[i]);
            Check(Value(parameters[0]) > 0., "InvalidCalibrationPlan", "/model/market/fxSpot", "positive finite spot");
            for (size_t i = 1; i < inputs.sigmaOffsets_[0]; ++i)
                Check(std::isfinite(std::exp(Value(parameters[i]))) && std::exp(Value(parameters[i])) > 0., "InvalidCalibrationPlan",
                      "/model/market/logDF", "positive finite discounts");
            for (size_t i = inputs.sigmaOffsets_[0]; i < parameters.size(); ++i) {
                Check(Value(parameters[i]) >= 0., "InvalidCalibrationPlan", "/model/dynamics/sigma", "nonnegative sigma");
                if constexpr (!std::is_same_v<T_, double>)
                    Check(Value(parameters[i]) > 0., "DerivativeUnavailable", "/model/dynamics/sigma", "interior positive sigma for AAD");
            }
        }
        template <class T_>
        XccyGaussianMomentValues_<T_>
        Evaluate(const XccyGaussianMomentProgram_& program, const XccyGaussianModelData_& model, const Vector_<T_>& parameters) {
            Match(program, model);
            const auto inputs = XccyGaussianMomentInputs(model);
            auto values = parameters;
            if (values.empty())
                for (const double value : inputs.values_)
                    values.push_back(T_(value));
            ValidateParameters(inputs, values);
            Environment_<T_> environment(model, inputs, values);
            T_ a(0.), b(0.);
            for (const auto& term : program.Terms()) {
                auto exponent = environment.Discount(term.payment_);
                if (term.foreignPayment_)
                    exponent = Add(exponent, environment.Fx(term.payment_));
                if (term.reset_)
                    exponent = Add(exponent, environment.Fx(*term.reset_));
                if (term.growth_)
                    exponent = Add(exponent, environment.Growth(*term.growth_, term.foreignRate_, term.projectionRoute_));
                const T_ value = term.weight_ * environment.Expectation(exponent);
                (term.annuity_ ? a : b) += value;
            }
            Check(std::isfinite(Value(a)), "CalibrationNumericalFailure", "/affine/a", "finite representable A");
            Check(std::isfinite(Value(b)), "CalibrationNumericalFailure", "/affine/b", "finite representable B");
            const T_ price = b + Descriptor(program.Prepared()).coupon_ * a;
            Check(std::isfinite(Value(price)), "CalibrationNumericalFailure", "/measure", "finite contract Price");
            return {a, b, price};
        }
    } // namespace XccyMomentInternal

    XccyGaussianMomentProgram_ LowerXccyGaussianMoments(const PreparedCalibrationInstrument_& prepared) {
        const auto& descriptor = XccyMomentInternal::Descriptor(prepared);
        Vector_<XccyGaussianMomentTerm_> terms;
        XccyMomentInternal::Coupons(descriptor, false, &terms);
        XccyMomentInternal::Coupons(descriptor, true, &terms);
        const auto& plan = descriptor.plan_;
        if (plan.config_.convention_.initialNotionalExchange_)
            XccyMomentInternal::Principal(descriptor, plan.start_, false, &terms);
        if (plan.config_.convention_.finalNotionalExchange_)
            XccyMomentInternal::Principal(descriptor, plan.maturity_, true, &terms);
        XccyMomentInternal::Mtm(descriptor, &terms);
        return XccyGaussianMomentProgram_(prepared, std::move(terms));
    }
    XccyGaussianMomentInputs_ XccyGaussianMomentInputs(const XccyGaussianModelData_& model) {
        const auto& market = *model.market_;
        XccyGaussianMomentInputs_ inputs;
        inputs.labels_ = {String_("fxspot:") + market.pair_.foreign_.String() + "/" + market.pair_.domestic_.String()};
        inputs.values_ = {market.fxSpot_};
        inputs.curves_ = {market.domestic_, market.foreign_, market.basis_};
        inputs.curves_.Append(market.projections_);
        for (const auto& curve : inputs.curves_) {
            inputs.curveOffsets_.push_back(inputs.values_.size());
            for (size_t i = 1; i < curve->nodeDates_.size(); ++i) {
                inputs.labels_.push_back("logdf:" + curve->currency_ + ":" + curve->route_ + ":" + Date::ToString(curve->nodeDates_[i]));
                inputs.values_.push_back(curve->logDF_[i]);
            }
        }
        const std::array<Vector_<>, 3> sigmas{model.dynamics_->domesticSigma_, model.dynamics_->foreignSigma_, model.dynamics_->fxSigma_};
        const Vector_<String_> names{market.pair_.domestic_.String(), market.pair_.foreign_.String(),
                                     String_(market.pair_.foreign_.String()) + "/" + market.pair_.domestic_.String()};
        for (size_t driver = 0; driver < 3; ++driver) {
            inputs.sigmaOffsets_[driver] = inputs.values_.size();
            for (size_t i = 0; i < sigmas[driver].size(); ++i) {
                inputs.labels_.push_back("sigma:" + names[driver] + ":" + Date::ToString(model.dynamics_->bucketEndDates_[i]));
                inputs.values_.push_back(sigmas[driver][i]);
            }
        }
        return inputs;
    }
    Matrix_<> XccyGaussianMomentNativeMap(const XccyGaussianModelData_& model, Vector_<String_>* labels) {
        const auto inputs = XccyGaussianMomentInputs(model);
        Vector_<String_> native{inputs.labels_[0]};
        for (const auto& curve : inputs.curves_)
            native.Append(curve->nativeLabels_);
        for (size_t i = inputs.sigmaOffsets_[0]; i < inputs.labels_.size(); ++i)
            native.push_back(inputs.labels_[i]);
        Matrix_<> map(static_cast<int>(inputs.values_.size()), static_cast<int>(native.size()), 0.);
        map(0, 0) = 1.;
        size_t column = 1;
        for (size_t i = 0; i < inputs.curves_.size(); ++i) {
            const auto& curve = *inputs.curves_[i];
            for (size_t row = 1; row < curve.nodeDates_.size(); ++row)
                for (size_t col = 0; col < curve.nativeLabels_.size(); ++col)
                    map(static_cast<int>(inputs.curveOffsets_[i] + row - 1), static_cast<int>(column + col)) =
                        curve.snapshotJacobian_(static_cast<int>(row), static_cast<int>(col));
            column += curve.nativeLabels_.size();
        }
        for (size_t i = inputs.sigmaOffsets_[0]; i < inputs.values_.size(); ++i)
            map(static_cast<int>(i), static_cast<int>(column++)) = 1.;
        if (labels)
            *labels = std::move(native);
        return map;
    }
    template <class T_> T_ XccyGaussianMomentValues_<T_>::Value(const PreparedCalibrationInstrument_& prepared) const {
        using namespace CalibrationInternal;
        if (prepared.Measure().kind_ == CalibrationMeasureKind_::Value_::Price)
            return price_;
        const auto& measure = prepared.Measure();
        CalibrationDiagnostic_ diagnostic;
        diagnostic.stage_ = CalibrationStage_::Value_::Evaluate;
        diagnostic.instrumentId_ = prepared.Instrument()->Id();
        diagnostic.measureId_ = measure.id_;
        Check(measure.annuitySign_ && measure.minAbsAnnuity_ && Dal::Value(a_) * *measure.annuitySign_ >= *measure.minAbsAnnuity_,
              "InvalidCalibrationAnnuity", "/annuity", "declared signed annuity and floor", NumberText(Dal::Value(a_)), diagnostic);
        const T_ par = -b_ / a_;
        Check(std::isfinite(Dal::Value(par)), "CalibrationNumericalFailure", "/measure", "finite signed decimal par", "", diagnostic);
        return par;
    }
    template <class T_>
    XccyGaussianMomentValues_<T_>
    EvaluateXccyGaussianMoments(const XccyGaussianMomentProgram_& program, const XccyGaussianModelData_& model, const Vector_<T_>& parameters) {
        try {
            return XccyMomentInternal::Evaluate(program, model, parameters);
        } catch (const CalibrationError_& error) {
            auto diagnostic = error.Failure().diagnostic_;
            diagnostic.stage_ = CalibrationStage_::Value_::Evaluate;
            diagnostic.instrumentId_ = program.Prepared().Instrument()->Id();
            diagnostic.measureId_ = program.Prepared().Measure().id_;
            CalibrationInternal::Fail(diagnostic.code_, diagnostic.field_.value_or("/model"), diagnostic.expected_.value_or("valid moments"),
                                      diagnostic.actual_.value_or(""), diagnostic);
        }
    }
    template struct XccyGaussianMomentValues_<double>;
    template struct XccyGaussianMomentValues_<AAD::Number_>;
    template XccyGaussianMomentValues_<double>
    EvaluateXccyGaussianMoments(const XccyGaussianMomentProgram_&, const XccyGaussianModelData_&, const Vector_<double>&);
    template XccyGaussianMomentValues_<AAD::Number_>
    EvaluateXccyGaussianMoments(const XccyGaussianMomentProgram_&, const XccyGaussianModelData_&, const Vector_<AAD::Number_>&);
} // namespace Dal
