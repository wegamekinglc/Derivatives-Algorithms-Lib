#include <dal/platform/platform.hpp>

#include <dal/curve/compoundedois_internal.hpp>
#include <dal/curve/compoundedoisscript_internal.hpp>
#include <dal/script/event.hpp>

namespace Dal::CompoundedOisInternal {
    using namespace CalibrationInternal;
    using CurveScriptInternal::Require;
    namespace {
        using Script::Node_;
        void Match(bool valid, const CalibrationDiagnostic_& source) {
            Require(valid, "CompoundedOisSourceMismatch", source, "complete bound source matching frozen compounded OIS tables");
        }
        bool Literal(const Node_& node, double value) {
            const auto* literal = dynamic_cast<const Script::NodeConst_*>(&node);
            return literal && literal->constVal_ == value;
        }
        bool Slot(const Node_& node, const String_& key, double value) {
            const auto* slot = dynamic_cast<const Script::NodeConstVar_*>(&node);
            return slot && slot->name_ == key && slot->constVal_ == value;
        }
        const Node_& Signed(const Node_& node, bool negative, const CalibrationDiagnostic_& d) {
            if (!negative)
                return node;
            Match(dynamic_cast<const Script::NodeUMinus_*>(&node), d);
            return *node.arguments_[0];
        }
        size_t Append(CalibrationCurvePayment_* flow, CalibrationCurveInstruction_ instruction) {
            flow->program_.push_back(instruction);
            return flow->program_.size() - 1;
        }
        Vector_<std::pair<size_t, size_t>> Ranges(const PreparedCompoundedOis_& tables) {
            Vector_<std::pair<size_t, size_t>> result(tables.Spec().coupons_.size());
            size_t first = 0;
            for (size_t i = 0; i < result.size(); ++i) {
                size_t last = first;
                while (last < tables.Observations().size() && tables.Observations()[last].couponRow_ == i)
                    ++last;
                result[i] = {first, last};
                first = last;
            }
            return result;
        }
        CalibrationDiagnostic_ ObservationSource(const CalibrationDiagnostic_& d, const CompoundedOisScriptSpec_& s, size_t row) {
            auto result = d;
            result.observationId_ = s.observations_[row].couponId_;
            result.field_ = "/observations/" + OrdinalText(row);
            return result;
        }
        const Script::NodeFix_& Factor(const Node_& node, const CompoundedOisScriptSpec_& s, size_t row, const CalibrationDiagnostic_& d) {
            Match(dynamic_cast<const Script::NodeAdd_*>(&node) && Literal(*node.arguments_[0], 1.), d);
            const auto& term = *node.arguments_[1];
            Match(dynamic_cast<const Script::NodeMulti_*>(&term) && Literal(*term.arguments_[0], s.observations_[row].accrual_), d);
            const auto* fix = dynamic_cast<const Script::NodeFix_*>(term.arguments_[1].get());
            Match(fix && dynamic_cast<const Index::Overnight_*>(fix->index_.get()), d);
            Match(Bytes(fix->index_->Name()) == Bytes(Index(s, row).Name()) && fix->fixingDate_ == s.observations_[row].fixingTime_.Date(), d);
            return *fix;
        }
        void Factors(const Node_& product,
                     const CompoundedOisScriptSpec_& s,
                     const std::pair<size_t, size_t>& range,
                     const CalibrationPreparationContext_& context,
                     bool included,
                     CalibrationCurveFamilyDescriptor_* descriptor,
                     CalibrationCurvePayment_* flow) {
            Vector_<const Node_*> pending{&product};
            size_t row = range.first, cumulative = 0;
            while (!pending.empty()) {
                const auto* node = pending.back();
                pending.pop_back();
                if (dynamic_cast<const Script::NodeMulti_*>(node)) {
                    pending.push_back(node->arguments_[1].get());
                    pending.push_back(node->arguments_[0].get());
                    continue;
                }
                Match(row < range.second, flow->source_);
                const auto d = ObservationSource(flow->source_, s, row);
                const auto& fix = Factor(*node, s, row, d);
                if (included) {
                    descriptor->observations_.push_back(CurveScriptInternal::Observation(fix, context, d));
                    flow->locations_.push_back(d);
                    const size_t factor =
                        Append(flow, {'g', descriptor->observations_.size() - 1, 0, s.observations_[row].accrual_, false, flow->locations_.size()});
                    cumulative = Append(flow, {'p', cumulative, factor, 0., false, flow->locations_.size()});
                }
                ++row;
            }
            Match(row == range.second, flow->source_);
        }
        void Fixed(const Node_& node, const CompoundedOisScriptSpec_& s, size_t row, CalibrationCurvePayment_* flow) {
            const bool pay = s.direction_ == "PayFixed";
            const auto& term = Signed(node, pay, flow->source_);
            const auto alpha = s.fixedLeg_[row].accrual_;
            Match(dynamic_cast<const Script::NodeMulti_*>(&term) && Literal(*term.arguments_[0], alpha) && Slot(*term.arguments_[1], "K", s.coupon_),
                  flow->source_);
            const size_t k = Append(flow, {'k', 0, 0, s.coupon_, true});
            const size_t weight = Append(flow, {'c', 0, 0, pay ? -alpha : alpha});
            Append(flow, {'*', k, weight, 0., true});
        }
        void Floating(const Node_& node,
                      const CompoundedOisScriptSpec_& s,
                      const std::pair<size_t, size_t>& range,
                      const CalibrationPreparationContext_& context,
                      bool included,
                      CalibrationCurveFamilyDescriptor_* descriptor,
                      CalibrationCurvePayment_* flow) {
            const bool receive = s.direction_ == "ReceiveFixed";
            const auto& term = Signed(node, receive, flow->source_);
            Match(dynamic_cast<const Script::NodeSub_*>(&term) && Literal(*term.arguments_[1], 1.), flow->source_);
            Factors(*term.arguments_[0], s, range, context, included, descriptor, flow);
            const size_t product = flow->program_.size() - 1;
            Append(flow, {'-', receive ? 0 : product, receive ? product : 0});
        }
        CalibrationCurvePayment_ Payment(const Script::NodePays_& payment,
                                         const CalibrationDiagnostic_& source,
                                         size_t ordinal,
                                         const PreparedCompoundedOis_& tables,
                                         const CalibrationPreparationContext_& context,
                                         const Vector_<std::pair<size_t, size_t>>& ranges,
                                         CalibrationCurveFamilyDescriptor_* descriptor) {
            const auto& s = tables.Spec();
            const bool fixed = ordinal < s.fixedLeg_.size();
            const size_t row = fixed ? ordinal : ordinal - s.fixedLeg_.size();
            const Date_ date = fixed ? s.fixedLeg_[row].payment_ : s.coupons_[row].payment_;
            const bool included = fixed ? tables.FixedPayments()[row].included_ : tables.CouponPayments()[row].included_;
            Match(source.eventDate_ == date && !payment.paymentDate_, source);
            Require(!included || date <= context.curve_->nodeDates_.back(), "CalibrationContextMismatch", source,
                    "live OIS payment within curve horizon");
            const auto& amount = *payment.arguments_[1];
            Match(dynamic_cast<const Script::NodeMulti_*>(&amount) && Slot(*amount.arguments_[0], "N", s.notional_), source);
            CalibrationCurvePayment_ flow{date, {{'c', 0, 0, 1.}}, source, {}};
            const auto& term = *amount.arguments_[1];
            if (fixed)
                Fixed(term, s, row, &flow);
            else
                Floating(term, s, ranges[row], context, included, descriptor, &flow);
            const size_t coefficient = flow.program_.size() - 1;
            const size_t n = Append(&flow, {'c', 0, 0, s.notional_});
            Append(&flow, {'*', n, coefficient, 0., fixed});
            return flow;
        }
        void Context(const CalibrationInstrument_& instrument, const CalibrationPreparationContext_& c) {
            const auto& tables = *c.compoundedOis_;
            const auto& s = tables.Spec();
            Check(c.currency_ == s.currency_ && Bytes(c.discountRoute_) == Bytes(s.discountRoute_), "CalibrationContextMismatch", "/context",
                  "frozen OIS currency and discount route");
            Check(Bytes(instrument.Id()) == Bytes(s.instrumentId_) && CurveScriptInternal::Candidate(instrument), "CompoundedOisSourceMismatch",
                  "/instrument", "frozen instrument with only N and K");
            Check(Fingerprint(ValuationJson(c.valuation_)) == Fingerprint(ValuationJson(tables.Valuation())), "CalibrationContextMismatch",
                  "/context/valuation", "same frozen OIS valuation and fixing snapshot");
            for (const auto& measure : instrument.Definition()->Spec().measures_)
                if (measure.kind_ != CalibrationMeasureKind_::Value_::Price)
                    Check(measure.annuitySign_ == (s.direction_ == "PayFixed" ? -1 : 1) && measure.minAbsAnnuity_ == s.minAbsAnnuity_,
                          "CompoundedOisSourceMismatch", "/measures", "frozen OIS annuity sign and floor");
        }
    } // namespace
    CalibrationCurveFamily_ Lower(const CalibrationInstrument_& instrument,
                                  const CalibrationPreparationContext_& context,
                                  const Vector_<const Script::NodePays_*>& payments,
                                  const Vector_<CalibrationDiagnostic_>& sources) {
        Context(instrument, context);
        const auto& tables = *context.compoundedOis_;
        const auto& s = tables.Spec();
        const size_t count = s.fixedLeg_.size() + s.coupons_.size();
        Check(payments.size() == count && instrument.Definition()->Spec().source_.rows_.size() == count + 2, "CompoundedOisSourceMismatch",
              "/source/rows", "two definitions and exactly one source row per fixed payment and coupon");
        const auto ranges = Ranges(tables);
        Vector_<bool> seen(count, false);
        auto result = std::make_shared<CalibrationCurveFamilyDescriptor_>();
        result->compounded_ = true;
        result->coupon_ = s.coupon_;
        for (size_t i = 0; i < payments.size(); ++i) {
            const auto& d = sources[i];
            Match(d.sourceRow_ && *d.sourceRow_ >= 3 && *d.sourceRow_ <= static_cast<int>(count + 2), d);
            const size_t row = *d.sourceRow_ - 3;
            Match(!seen[row], d);
            seen[row] = true;
            auto flow = Payment(*payments[i], d, row, tables, context, ranges, result.get());
            if (flow.payment_ >= *context.valuation_.evaluationDate_)
                result->payments_.push_back(std::move(flow));
        }
        return result;
    }
} // namespace Dal::CompoundedOisInternal
