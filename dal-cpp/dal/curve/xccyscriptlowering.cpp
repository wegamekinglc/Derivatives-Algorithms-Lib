#include <dal/platform/platform.hpp>

#include <dal/currency/currencydata.hpp>
#include <dal/curve/xccyscript_internal.hpp>
#include <dal/indice/detail/snapshoterror.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/script/event.hpp>
#include <dal/time/dateutils.hpp>

namespace Dal::XccyScriptInternal {
    using namespace CalibrationInternal;
    using namespace Script;
    namespace {
        const String_ CODE = "UnsupportedFastEvaluator";
        const Node_& Arg(const Node_& node, size_t i, const CalibrationDiagnostic_& d) {
            Check(i < node.arguments_.size(), CODE, "/source/rows", "complete XCCY expression", "", d);
            return *node.arguments_[i];
        }
        std::optional<double> Literal(const Node_& node) {
            if (const auto* c = dynamic_cast<const NodeConst_*>(&node))
                return c->constVal_;
            if (dynamic_cast<const NodeUMinus_*>(&node)) {
                const auto value = Literal(*node.arguments_[0]);
                if (value)
                    return -*value;
            }
            return {};
        }
        double Number(const Node_& node, const CalibrationDiagnostic_& d) {
            const auto value = Literal(node);
            Check(value && std::isfinite(*value), CODE, "/source/rows", "finite literal", "", d);
            return *value;
        }
        bool Slot(const Node_& node, const String_& key) {
            const auto* slot = dynamic_cast<const NodeConstVar_*>(&node);
            return slot && Bytes(slot->name_) == Bytes(key);
        }
        Vector_<const Node_*> Factors(const Node_& node, size_t size) {
            Vector_<const Node_*> result(size);
            const Node_* current = &node;
            for (size_t i = size - 1; i > 0; --i) {
                if (!dynamic_cast<const NodeMulti_*>(current) || current->arguments_.size() != 2)
                    return {};
                result[i] = current->arguments_[1].get();
                current = current->arguments_[0].get();
            }
            result[0] = current;
            return result;
        }
        struct Coupon_ {
            XccyCouponPeriod_ period_;
            const Node_* notional_ = nullptr;
            Ccy_ currency_;
            PeriodLength_ tenor_;
            int sign_ = 0;
            bool spread_ = false;
            CalibrationDiagnostic_ source_;
        };
        const Index::Libor_& CanonicalFix(const NodeFix_& fix, const CalibrationDiagnostic_& d) {
            const auto* libor = dynamic_cast<const Index::Libor_*>(fix.index_.get());
            Check(libor && Cell::IsEmpty(libor->start_) && fix.fixingDate_ &&
                      (libor->tenor_ == TradedRate_::Value_::LIBOR_3M_LCH || libor->tenor_ == TradedRate_::Value_::LIBOR_6M_LCH),
                  "UnsupportedCalibrationFixingTime", "/source/rows", "canonical midnight 3M/6M rate with concrete fixing", fix.index_->Name(), d);
            return *libor;
        }
        Coupon_ Coupon(const NodeFix_& fix, const Vector_<const Node_*>& factors, const CalibrationDiagnostic_& d) {
            const auto* libor = &CanonicalFix(fix, d);
            Coupon_ result;
            result.source_ = d;
            result.currency_ = libor->ccy_;
            result.tenor_ = libor->tenor_.Period();
            const double sign = Number(*factors[0], d);
            Check(std::abs(sign) == 1., CODE, "/source/rows", "unit coupon direction", "", d);
            result.sign_ = static_cast<int>(sign);
            result.notional_ = factors[1];
            auto& period = result.period_;
            period.rateIndexName_ = fix.index_->Name();
            period.rateFixingTime_ = DateTime_(*fix.fixingDate_);
            period.schedule_.fixingDate_ = *fix.fixingDate_;
            period.schedule_.accrualStart_ = libor->StartDate(period.rateFixingTime_);
            period.schedule_.accrualEnd_ = Date::NominalMaturity(period.schedule_.accrualStart_, result.tenor_, result.currency_);
            period.schedule_.paymentDate_ = *d.eventDate_;
            period.accrual_.dcf_ = Number(*factors[2], d);
            const auto basis = Ccy::Conventions::LiborDayBasis()(result.currency_);
            Check(period.accrual_.dcf_ > 0. && period.accrual_.dcf_ == basis(period.schedule_.accrualStart_, period.schedule_.accrualEnd_, nullptr) &&
                      period.schedule_.paymentDate_ >= period.schedule_.accrualEnd_,
                  CODE, "/source/rows", "exact rate accrual and arrears payment", "", d);
            const auto& spread = *factors[3]->arguments_[1];
            result.spread_ = Slot(spread, "K");
            Check(result.spread_ || Literal(spread) == 0., CODE, "/source/rows", "one spread leg and zero non-spread leg", "", d);
            return result;
        }
        std::optional<Coupon_> TryCoupon(const NodePays_& payment, const XccyScriptContext_& context, const CalibrationDiagnostic_& d) {
            const auto& amount = Arg(payment, 1, d);
            Check(dynamic_cast<const NodeMulti_*>(&amount) && Slot(Arg(amount, 0, d), "N"), CODE, "/source/rows", "N*(complete XCCY cashflow)", "",
                  d);
            const auto factors = Factors(Arg(amount, 1, d), 5);
            if (factors.empty() || !dynamic_cast<const NodeAdd_*>(factors[3]))
                return {};
            const auto* fix = dynamic_cast<const NodeFix_*>(&Arg(*factors[3], 0, d));
            Check(fix != nullptr, CODE, "/source/rows", "canonical coupon FIX plus spread", "", d);
            auto result = Coupon(*fix, factors, d);
            const bool foreign = result.currency_ == context.pair_.foreign_;
            Check(foreign || result.currency_ == context.pair_.domestic_, CODE, "/source/rows", "configured leg currency", result.currency_.String(),
                  d);
            Check(foreign ? dynamic_cast<const NodeSpot_*>(factors[4]) != nullptr : Literal(*factors[4]) == 1., CODE, "/source/rows",
                  "foreign payment-date model FX conversion or domestic unit conversion", "", d);
            return result;
        }
        double Bound(const CalibrationInstrument_& instrument, const String_& key) {
            for (const auto& value : instrument.Binding())
                if (Bytes(value.key_) == Bytes(key))
                    return value.value_;
            Fail(CODE, "/parameters", "XCCY effective N/K binding", key);
        }
        void Leg(const Vector_<Coupon_>& coupons, bool foreign, XccyCashflowPlan_* plan, const CalibrationDiagnostic_& d) {
            Check(!coupons.empty(), CODE, "/source/rows", "coupons on both currency legs", "", d);
            auto& periods = foreign ? plan->foreignPeriods_ : plan->domesticPeriods_;
            auto& index = foreign ? plan->config_.convention_.foreignIndex_ : plan->config_.convention_.domesticIndex_;
            index.useProjectionCurve_ = true;
            index.forecastTenor_ = coupons[0].tenor_;
            for (const auto& coupon : coupons) {
                Check(coupon.tenor_ == index.forecastTenor_ && coupon.sign_ == coupons[0].sign_ && coupon.spread_ == coupons[0].spread_ &&
                          (periods.empty() || periods.back().schedule_.accrualEnd_ == coupon.period_.schedule_.accrualStart_),
                      CODE, "/source/rows", "contiguous consistent canonical leg", "", coupon.source_);
                periods.push_back(coupon.period_);
            }
        }
        XccyResetEvent_ Reset(const Coupon_& coupon, size_t i, double ratio, const XccyCashflowPlan_& plan) {
            const auto f = Factors(*coupon.notional_, 2);
            Check(!f.empty() && Number(*f[0], coupon.source_) == ratio, CODE, "/source/rows", "Nf times domestic-per-foreign FX reset", "",
                  coupon.source_);
            const auto* fx = dynamic_cast<const NodeFix_*>(f[1]);
            Check(fx && fx->fixingDate_ && Bytes(fx->index_->Name()) == Bytes(FxIndexName(plan.config_.pair_)) &&
                      *fx->fixingDate_ <= coupon.period_.schedule_.accrualStart_ && plan.resets_.size() == i - 1,
                  "UnsupportedCalibrationFixingTime", "/source/rows", "consecutive canonical midnight FX reset before effective date", "",
                  coupon.source_);
            return {coupon.period_.schedule_.accrualStart_, DateTime_(*fx->fixingDate_), static_cast<int>(i)};
        }
        void Notionals(const Vector_<Coupon_>& domestic, const Vector_<Coupon_>& foreign, XccyCashflowPlan_* plan) {
            const double ratio = Number(*foreign[0].notional_, foreign[0].source_);
            Check(ratio > 0., CODE, "/source/rows", "positive foreign/domestic notional ratio", "", foreign[0].source_);
            plan->config_.foreignNotional_ = plan->config_.domesticNotional_ * ratio;
            Check(std::isfinite(plan->config_.foreignNotional_), CODE, "/source/rows", "finite resolved foreign notional", "", foreign[0].source_);
            for (const auto& coupon : foreign)
                Check(Number(*coupon.notional_, coupon.source_) == ratio, CODE, "/source/rows", "constant foreign notional", "", coupon.source_);
            Check(Number(*domestic[0].notional_, domestic[0].source_) == 1., CODE, "/source/rows", "initial domestic N", "", domestic[0].source_);
            for (size_t i = 1; i < domestic.size(); ++i) {
                const auto& coupon = domestic[i];
                if (Literal(*coupon.notional_) == 1.) {
                    Check(plan->resets_.empty(), CODE, "/source/rows", "fixed or consistently reset domestic notionals", "", coupon.source_);
                    continue;
                }
                plan->resets_.push_back(Reset(coupon, i, ratio, *plan));
            }
            if (!plan->resets_.empty())
                plan->config_.notionalMode_ = XccyNotionalMode_::Value_::RESETTABLE;
        }
        bool SameLeaf(const Node_& left, const Node_& right) {
            if (const auto* c = dynamic_cast<const NodeConst_*>(&left))
                return c->constVal_ == static_cast<const NodeConst_&>(right).constVal_;
            if (const auto* c = dynamic_cast<const NodeConstVar_*>(&left)) {
                const auto& r = static_cast<const NodeConstVar_&>(right);
                return Bytes(c->name_) == Bytes(r.name_) && c->constVal_ == r.constVal_;
            }
            if (const auto* v = dynamic_cast<const NodeVar_*>(&left))
                return Bytes(v->name_) == Bytes(static_cast<const NodeVar_&>(right).name_);
            if (const auto* f = dynamic_cast<const NodeFix_*>(&left)) {
                const auto& r = static_cast<const NodeFix_&>(right);
                return Bytes(f->index_->Name()) == Bytes(r.index_->Name()) && f->fixingDate_ == r.fixingDate_;
            }
            if (const auto* p = dynamic_cast<const NodePays_*>(&left))
                return p->paymentDate_ == static_cast<const NodePays_&>(right).paymentDate_;
            return true;
        }
        bool SameAst(const Node_& left, const Node_& right) {
            if (typeid(left) != typeid(right) || left.arguments_.size() != right.arguments_.size() || !SameLeaf(left, right))
                return false;
            for (size_t i = 0; i < left.arguments_.size(); ++i)
                if (!SameAst(*left.arguments_[i], *right.arguments_[i]))
                    return false;
            return true;
        }
        void Verify(const CalibrationInstrument_& instrument,
                    const CalibrationXccyDescriptor_& descriptor,
                    const Vector_<const NodePays_*>& payments,
                    const Vector_<CalibrationDiagnostic_>& sources) {
            auto spec = instrument.Definition()->Spec();
            spec.source_.rows_ = ExportRows(descriptor.plan_, descriptor.coupon_, descriptor.receiveNonSpreadPaySpread_, sources[0]);
            const auto expected =
                NewScriptCalibrationInstrument({instrument.Id(), NewScriptInstrumentDefinition(spec), {}, instrument.Pillar()})->Product();
            size_t i = 0;
            for (size_t event = 0; event < expected->Events().size(); ++event)
                for (const auto& node : expected->Events()[event]) {
                    Check(i < payments.size() && sources[i].eventDate_ == expected->EventDates()[event] && SameAst(*payments[i], *node), CODE,
                          "/source/rows", "complete XCCY coupon/principal/payment/FX structure", "", sources[std::min(i, sources.size() - 1)]);
                    ++i;
                }
            Check(i == payments.size(), CODE, "/source/rows", "no extra or missing XCCY payments", "", sources[0]);
        }
        void Exchanges(const Vector_<Date_>& dates, XccyCashflowPlan_* plan) {
            for (const auto& date : dates) {
                if (date == plan->start_)
                    plan->config_.convention_.initialNotionalExchange_ = true;
                else if (date == plan->maturity_)
                    plan->config_.convention_.finalNotionalExchange_ = true;
                else if (!plan->resets_.empty())
                    plan->config_.notionalMode_ = XccyNotionalMode_::Value_::MARK_TO_MARKET;
            }
        }
        void ValidateSpec(const ScriptInstrumentDefinitionSpec_& spec, const CalibrationDiagnostic_& d) {
            Check(spec.parameters_.size() == 2 && spec.source_.settings_.defaultIndex_.empty() && spec.source_.settings_.regressionFeatures_.empty(),
                  CODE, "/source/settings", "model FX spot bound by complete currency legs and N/K only", "", d);
            const auto& parameter = spec.parameters_[1];
            Check(Bytes(parameter.key_) == "K" && parameter.role_ == ScriptParameterRole_::Value_::Coupon &&
                      Bytes(parameter.unit_.Token()) == "BasisSpread",
                  CODE, "/parameters/1", "K in signed decimal BasisSpread units", "", d);
        }
        void Measures(const ScriptInstrumentDefinitionSpec_& spec, bool receive, const CalibrationDiagnostic_& d) {
            for (const auto& measure : spec.measures_)
                if (measure.kind_ != CalibrationMeasureKind_::Value_::Price)
                    Check(measure.kind_ == CalibrationMeasureKind_::Value_::ParAffineSpread && measure.annuitySign_ == (receive ? -1 : 1), CODE,
                          "/measures", "ParAffineSpread with source payment direction and signed annuity", "", d);
        }
        Vector_<Date_> Collect(const Vector_<const NodePays_*>& payments,
                               const Vector_<CalibrationDiagnostic_>& sources,
                               const XccyScriptContext_& context,
                               Vector_<Coupon_>* domestic,
                               Vector_<Coupon_>* foreign) {
            Vector_<Date_> exchanges;
            for (size_t i = 0; i < payments.size(); ++i) {
                if (const auto coupon = TryCoupon(*payments[i], context, sources[i]))
                    (coupon->currency_ == context.pair_.domestic_ ? domestic : foreign)->push_back(*coupon);
                else
                    exchanges.push_back(*sources[i].eventDate_);
            }
            const auto sort = [](auto* coupons) {
                std::sort(coupons->begin(), coupons->end(),
                          [](const auto& a, const auto& b) { return a.period_.schedule_.accrualStart_ < b.period_.schedule_.accrualStart_; });
            };
            sort(domestic);
            sort(foreign);
            return exchanges;
        }
        Handle_<MarketFixingSnapshot_>
        Capture(const XccyScriptContext_& context, const Vector_<FixingRequest_>& requests, const CalibrationDiagnostic_& d) {
            if (context.fixings_)
                return context.fixings_;
            try {
                return SnapshotGlobalFixings(requests);
            } catch (const Dal::Detail::SnapshotFixingError_& error) {
                const auto& request = error.Request();
                Fail("CalibrationNumericalFailure", "/context/fixings",
                     "valid frozen fixing " + request.indexName_ + " at " + DateTime::ToString(request.fixingTime_), String_(error.what()), d);
            } catch (const std::exception& error) {
                Fail("CalibrationNumericalFailure", "/context/fixings", "valid frozen historical snapshot", String_(error.what()), d);
            }
        }
    } // namespace
    CalibrationXccy_ Lower(const CalibrationInstrument_& instrument,
                           const XccyScriptContext_& context,
                           const Vector_<const NodePays_*>& payments,
                           const Vector_<CalibrationDiagnostic_>& sources) {
        const auto& d = sources[0];
        const auto& spec = instrument.Definition()->Spec();
        ValidateSpec(spec, d);
        CalibrationXccyDescriptor_ result;
        auto& plan = result.plan_;
        plan.config_.pair_ = context.pair_;
        plan.config_.domesticNotional_ = Bound(instrument, "N");
        result.coupon_ = Bound(instrument, "K");
        result.valuationTime_ = context.valuationTime_;
        Vector_<Coupon_> domestic, foreign;
        const auto exchanges = Collect(payments, sources, context, &domestic, &foreign);
        Leg(domestic, false, &plan, d);
        Leg(foreign, true, &plan, d);
        Check(domestic[0].spread_ != foreign[0].spread_ && domestic[0].sign_ == -foreign[0].sign_, CODE, "/source/rows",
              "opposite leg directions with exactly one spread leg", "", d);
        plan.config_.convention_.spreadOnForeignLeg_ = foreign[0].spread_;
        result.receiveNonSpreadPaySpread_ = (foreign[0].spread_ ? foreign[0].sign_ : domestic[0].sign_) == -1;
        plan.start_ = plan.domesticPeriods_[0].schedule_.accrualStart_;
        plan.maturity_ = plan.domesticPeriods_.back().schedule_.accrualEnd_;
        Check(plan.foreignPeriods_[0].schedule_.accrualStart_ == plan.start_ && plan.foreignPeriods_.back().schedule_.accrualEnd_ == plan.maturity_,
              CODE, "/source/rows", "common resolved leg start/maturity", "", d);
        Notionals(domestic, foreign, &plan);
        Exchanges(exchanges, &plan);
        Measures(spec, result.receiveNonSpreadPaySpread_, d);
        Verify(instrument, result, payments, sources);
        const auto requests = RequiredHistoricalFixings(plan, context.valuationTime_);
        result.fixings_ = Capture(context, requests, d);
        for (const auto& request : requests)
            Check(result.fixings_->Find(request.indexName_, request.fixingTime_).has_value(), "MissingFixing", "/context/fixings",
                  "exact historical " + request.indexName_ + " at " + Date::ToString(request.fixingTime_.Date()) + " 00:00", "", d);
        return std::make_shared<const CalibrationXccyDescriptor_>(std::move(result));
    }
} // namespace Dal::XccyScriptInternal
