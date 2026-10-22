#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/model/gsrrateoption_internal.hpp>
#include <dal/script/calibrationrateoption_internal.hpp>
#include <dal/script/event.hpp>

namespace Dal::CalibrationRateOptionInternal {
    using namespace CalibrationInternal;
    using Script::Node_;
    using Script::NodeAdd_;
    using Script::NodeConst_;
    using Script::NodeConstVar_;
    using Script::NodeFix_;
    using Script::NodeMax_;
    using Script::NodeMulti_;
    using Script::NodeSub_;
    namespace {
        template <class N_> bool Is(const Node_& node) { return dynamic_cast<const N_*>(&node) != nullptr; }
        bool Slot(const Node_& node, const String_& name) {
            const auto* slot = dynamic_cast<const NodeConstVar_*>(&node);
            return slot && slot->name_ == name;
        }
        const Node_* Maximum(const Node_& node) {
            if (!Is<NodeMax_>(node) || node.arguments_.size() != 2)
                return nullptr;
            const auto* zero = dynamic_cast<const NodeConst_*>(node.arguments_[1].get());
            return zero && zero->constVal_ == 0. && Is<NodeSub_>(*node.arguments_[0]) ? node.arguments_[0].get() : nullptr;
        }
        double Accrual(const Node_& node, const CalibrationDiagnostic_& d) {
            const auto* value = dynamic_cast<const NodeConst_*>(&node);
            Check(value && std::isfinite(value->constVal_) && value->constVal_ > 0., "UnsupportedFastEvaluator", "/source/rows",
                  "positive literal coupon accrual", "", d);
            return value->constVal_;
        }
        double Binding(
            const CalibrationInstrument_& instrument, const String_& key, ScriptParameterRole_ role, const String_& unit, CalibrationDiagnostic_ d) {
            const auto& parameters = instrument.Definition()->Spec().parameters_;
            const auto p = std::find_if(parameters.begin(), parameters.end(), [&](const auto& x) { return Bytes(x.key_) == Bytes(key); });
            Check(p != parameters.end(), "UnsupportedFastEvaluator", "/parameters", "rate option N and K", "", d);
            d.parameterKey_ = key;
            const String_ path = "/parameters/" + OrdinalText(static_cast<size_t>(p - parameters.begin()));
            Check(p->role_ == role && p->unit_.Token() == unit && p->type_ == ScriptParameterType_::Value_::Real && p->bindable_,
                  "UnsupportedFastEvaluator", path, "bindable Real " + key + "/" + role.String() + "/" + unit, "", d);
            for (const auto& value : instrument.Binding())
                if (Bytes(value.key_) == Bytes(key))
                    return value.value_;
            Fail("UnsupportedFastEvaluator", path, "effective binding", "", d);
        }
        const NodeFix_& Fix(const Node_& node, const Date_& expiry, const CalibrationDiagnostic_& d) {
            const auto* fix = dynamic_cast<const NodeFix_*>(&node);
            Check(fix && fix->fixingDate_.value_or(expiry) == expiry, "UnsupportedFastEvaluator", "/source/rows",
                  "observation at the frozen exercise date", "", d);
            return *fix;
        }
        Date_ Discount(const Node_& node, const Date_& expiry, const Ccy_& currency, const CalibrationDiagnostic_& d) {
            const auto& fix = Fix(node, expiry, d);
            const auto* df = dynamic_cast<const Index::DF_*>(fix.index_.get());
            Check(df && df->ccy_ == currency && Cell::IsEmpty(df->start_) && Cell::IsDate(df->maturity_), "UnsupportedFastEvaluator", "/source/rows",
                  "same-currency absolute discount maturity without start offset", "", d);
            const auto payment = Cell::ToDate(df->maturity_);
            Check(payment > expiry, "UnsupportedFastEvaluator", "/source/rows", "discount maturity after exercise", "", d);
            return payment;
        }
        FloatingCoupon_ Floating(
            const Node_& node, const Date_& expiry, const Date_& payment, double accrual, const Ccy_& currency, const CalibrationDiagnostic_& d) {
            const auto& fix = Fix(node, expiry, d);
            const auto* libor = dynamic_cast<const Index::Libor_*>(fix.index_.get());
            Check(libor && libor->ccy_ == currency && Cell::IsDate(libor->start_), "UnsupportedFastEvaluator", "/source/rows",
                  "same-currency Libor with concrete index start", "", d);
            Check(libor->tenor_ == TradedRate_::Value_::LIBOR_3M_LCH || libor->tenor_ == TradedRate_::Value_::LIBOR_6M_LCH,
                  "UnsupportedFastEvaluator", "/source/rows", "canonical 3M or 6M LCH Libor", "", d);
            return GaussianRateOptionInternal::Coupon(expiry, Cell::ToDate(libor->start_), payment, accrual,
                                                      libor->tenor_.Period().Months() == 3 ? "3M" : "6M", currency, d);
        }
        template <class T_, class F_> Vector_<T_> Sum(const Node_& node, const F_& term) {
            if (!Is<NodeAdd_>(node))
                return {term(node)};
            auto result = Sum<T_>(*node.arguments_[0], term);
            const auto rhs = Sum<T_>(*node.arguments_[1], term);
            for (const auto& value : rhs)
                result.push_back(value);
            return result;
        }
        Caplet_ Caplet(const Node_& amount, const Payment_& payment, const Ccy_& currency, double strike) {
            const auto& d = payment.source_;
            const double alpha = Accrual(*amount.arguments_[0], d);
            const auto* difference = Maximum(*amount.arguments_[1]);
            Check(difference != nullptr, "UnsupportedFastEvaluator", "/source/rows", "literal accrual times hard rate option", "", d);
            const bool call = Slot(*difference->arguments_[1], "K");
            Check(call || Slot(*difference->arguments_[0], "K"), "UnsupportedFastEvaluator", "/source/rows", "rate minus K or K minus rate", "", d);
            const auto* fix = dynamic_cast<const NodeFix_*>(difference->arguments_[call ? 0 : 1].get());
            Check(fix != nullptr, "UnsupportedFastEvaluator", "/source/rows", "one Libor observation", "", d);
            const auto expiry = fix->fixingDate_.value_or(payment.date_);
            const auto coupon = Floating(*fix, expiry, payment.date_, alpha, currency, d);
            return {expiry,
                    coupon.start_,
                    coupon.end_,
                    coupon.payment_,
                    coupon.indexAccrual_,
                    alpha,
                    coupon.tenor_,
                    strike,
                    OptionType_(call ? "CALL" : "PUT")};
        }
        FixedCoupon_ FixedTerm(const Node_& node, const Payment_& payment, const Ccy_& currency) {
            Check(Is<NodeMulti_>(node), "UnsupportedFastEvaluator", "/source/rows", "fixed accrual times discount", "", payment.source_);
            return {Discount(*node.arguments_[1], payment.date_, currency, payment.source_), Accrual(*node.arguments_[0], payment.source_)};
        }
        FloatingCoupon_ FloatingTerm(const Node_& node, const Payment_& payment, const Ccy_& currency) {
            const auto& d = payment.source_;
            Check(Is<NodeMulti_>(node) && Is<NodeMulti_>(*node.arguments_[0]), "UnsupportedFastEvaluator", "/source/rows",
                  "floating accrual times exercise Libor times end discount", "", d);
            const auto& coupon = *node.arguments_[0];
            const auto end = Discount(*node.arguments_[1], payment.date_, currency, d);
            auto result = Floating(*coupon.arguments_[1], payment.date_, end, Accrual(*coupon.arguments_[0], d), currency, d);
            Check(result.end_ == end, "UnsupportedFastEvaluator", "/source/rows", "floating payment at index end", "", d);
            return result;
        }
        Swaption_ Swaption(const Node_& difference, const Payment_& payment, const Ccy_& currency, double strike) {
            const auto isStrike = [](const Node_& n) { return Is<NodeMulti_>(n) && Slot(*n.arguments_[0], "K"); };
            const bool call = isStrike(*difference.arguments_[1]);
            Check(call || isStrike(*difference.arguments_[0]), "UnsupportedFastEvaluator", "/source/rows", "floating PV versus K times annuity", "",
                  payment.source_);
            const auto& fixed = *difference.arguments_[call ? 1 : 0]->arguments_[1];
            const auto& floating = *difference.arguments_[call ? 0 : 1];
            return {payment.date_, Sum<FixedCoupon_>(fixed, [&](const Node_& n) { return FixedTerm(n, payment, currency); }),
                    Sum<FloatingCoupon_>(floating, [&](const Node_& n) { return FloatingTerm(n, payment, currency); }), strike,
                    OptionType_(call ? "CALL" : "PUT")};
        }
    } // namespace
    std::optional<CalibrationRateOptionDescriptor_>
    Match(const Vector_<Payment_>& payments, const CalibrationInstrument_& instrument, const CalibrationPreparationContext_& context) {
        if (payments.size() != 1)
            return {};
        const auto& payment = payments[0];
        const auto& amount = *payment.node_->arguments_[1];
        if (!Is<NodeMulti_>(amount) || !Slot(*amount.arguments_[0], "N"))
            return {};
        const auto& option = *amount.arguments_[1];
        const auto* difference = Maximum(option);
        if (!difference && !Is<NodeMulti_>(option))
            return {};
        Check(instrument.Definition()->Spec().parameters_.size() == 2, "UnsupportedFastEvaluator", "/parameters",
              "exactly N/Notional and K/RateStrike; no ALPHA", "", payment.source_);
        const double notional =
            Binding(instrument, "N", ScriptParameterRole_::Value_::Notional, "Price:" + String_(context.currency_.String()), payment.source_);
        const double strike = Binding(instrument, "K", ScriptParameterRole_::Value_::RateStrike, "Rate", payment.source_);
        if (difference)
            return CalibrationRateOptionDescriptor_{Swaption(*difference, payment, context.currency_, strike), notional};
        return CalibrationRateOptionDescriptor_{Caplet(option, payment, context.currency_, strike), notional};
    }
    String_ Rule(const CalibrationRateOptionDescriptor_& descriptor) {
        return std::holds_alternative<Caplet_>(descriptor.option_) ? "dal.gaussian-caplet/1" : "dal.gaussian-swaption/1";
    }
} // namespace Dal::CalibrationRateOptionInternal
