#include <dal/platform/platform.hpp>

#include <dal/curve/curvescript_internal.hpp>
#include <dal/script/event.hpp>

namespace Dal::CurveScriptInternal {
    using namespace CalibrationInternal;
    namespace {
        char Operation(const Script::Node_& node) {
            if (dynamic_cast<const Script::NodeAdd_*>(&node))
                return '+';
            if (dynamic_cast<const Script::NodeSub_*>(&node))
                return '-';
            if (dynamic_cast<const Script::NodeMulti_*>(&node))
                return '*';
            if (dynamic_cast<const Script::NodeDiv_*>(&node))
                return '/';
            return 0;
        }
        size_t Append(CalibrationCurvePayment_* flow, CalibrationCurveInstruction_ instruction) {
            flow->program_.push_back(instruction);
            return flow->program_.size() - 1;
        }
        bool IsOne(const Script::Node_& node) {
            const auto* literal = dynamic_cast<const Script::NodeConst_*>(&node);
            return literal && literal->constVal_ == 1.;
        }
        const Script::NodeFix_* ReciprocalFixing(const Script::Node_& node) {
            if (!dynamic_cast<const Script::NodeDiv_*>(&node) || !IsOne(*node.arguments_[0]))
                return nullptr;
            return dynamic_cast<const Script::NodeFix_*>(node.arguments_[1].get());
        }
        class Lowerer_ {
            CalibrationCurvePayment_* flow_;
            CalibrationCurveFamilyDescriptor_* descriptor_;
            const CalibrationPreparationContext_& context_;

            std::optional<size_t> Leaf(const Script::Node_& node) {
                if (const auto* literal = dynamic_cast<const Script::NodeConst_*>(&node))
                    return Append(flow_, {'c', 0, 0, literal->constVal_, false});
                if (const auto* slot = dynamic_cast<const Script::NodeConstVar_*>(&node)) {
                    Require(slot->name_ == "K", "UnsupportedFastEvaluator", flow_->source_, "N occurs once as the outer notional factor");
                    return Append(flow_, {'k', 0, 0, slot->constVal_, true});
                }
                if (const auto* fixing = dynamic_cast<const Script::NodeFix_*>(&node)) {
                    descriptor_->observations_.push_back(Observation(*fixing, context_, flow_->source_));
                    return Append(flow_, {'f', descriptor_->observations_.size() - 1});
                }
                return {};
            }
            std::optional<size_t> Forward(const Script::Node_& node) {
                if (!dynamic_cast<const Script::NodeDiv_*>(&node))
                    return {};
                const auto& numerator = *node.arguments_[0];
                if (!dynamic_cast<const Script::NodeSub_*>(&numerator))
                    return {};
                const auto* fixing = ReciprocalFixing(*numerator.arguments_[0]);
                const auto* alpha = dynamic_cast<const Script::NodeConst_*>(node.arguments_[1].get());
                if (!fixing || !alpha || !IsOne(*numerator.arguments_[1]))
                    return {};
                descriptor_->observations_.push_back(Observation(*fixing, context_, flow_->source_));
                return Append(flow_, {'r', descriptor_->observations_.size() - 1, 0, alpha->constVal_, false});
            }
            size_t Binary(const Script::Node_& node) {
                const char operation = Operation(node);
                Require(operation != 0, "UnsupportedFastEvaluator", flow_->source_, "bounded affine coupon arithmetic and admitted FIX");
                const size_t left = Lower(*node.arguments_[0]), right = Lower(*node.arguments_[1]);
                const bool a = flow_->program_[left].dependent_, b = flow_->program_[right].dependent_;
                Require(!(operation == '*' && a && b) && !(operation == '/' && b), "UnsupportedCalibrationMeasure", flow_->source_,
                        "structurally affine K with K-independent divisors");
                return Append(flow_, {operation, left, right, 0., a || b});
            }

        public:
            Lowerer_(CalibrationCurvePayment_* flow, CalibrationCurveFamilyDescriptor_* descriptor, const CalibrationPreparationContext_& context)
                : flow_(flow), descriptor_(descriptor), context_(context) {}
            size_t Lower(const Script::Node_& node) {
                if (const auto leaf = Leaf(node))
                    return *leaf;
                if (const auto forward = Forward(node))
                    return *forward;
                if (dynamic_cast<const Script::NodeUPlus_*>(&node))
                    return Lower(*node.arguments_[0]);
                if (dynamic_cast<const Script::NodeUMinus_*>(&node)) {
                    const auto left = Append(flow_, {}), right = Lower(*node.arguments_[0]);
                    return Append(flow_, {'-', left, right, 0., flow_->program_[right].dependent_});
                }
                return Binary(node);
            }
        };
        CalibrationCurvePayment_ Payment(const Script::NodePays_& node,
                                         const CalibrationDiagnostic_& source,
                                         const CalibrationPreparationContext_& context,
                                         CalibrationCurveFamilyDescriptor_* descriptor) {
            CalibrationCurvePayment_ flow{*source.eventDate_, {}, source};
            Require(flow.payment_ >= *context.valuation_.evaluationDate_ && flow.payment_ <= context.curve_->nodeDates_.back(),
                    "UnsupportedFastEvaluator", source, "live payment covered by frozen curve layout");
            const auto& amount = *node.arguments_[1];
            const auto* notional = amount.arguments_.empty() ? nullptr : dynamic_cast<const Script::NodeConstVar_*>(amount.arguments_[0].get());
            Require(dynamic_cast<const Script::NodeMulti_*>(&amount) && notional && notional->name_ == "N", "UnsupportedFastEvaluator", source,
                    "complete N*(coupon expression) in contract Price units");
            const auto left = Append(&flow, {'c', 0, 0, notional->constVal_});
            const auto right = Lowerer_(&flow, descriptor, context).Lower(*amount.arguments_[1]);
            static_cast<void>(Append(&flow, {'*', left, right, 0., flow.program_[right].dependent_}));
            return flow;
        }
    } // namespace
    bool Candidate(const CalibrationInstrument_& instrument) {
        const auto& parameters = instrument.Definition()->Spec().parameters_;
        return parameters.size() == 2 && std::any_of(parameters.begin(), parameters.end(),
                                                     [](const auto& p) { return p.key_ == "K" && p.role_ == ScriptParameterRole_::Value_::Coupon; });
    }
    std::shared_ptr<const CalibrationCurveFamilyDescriptor_> Lower(const CalibrationInstrument_& instrument,
                                                                   const CalibrationPreparationContext_& context,
                                                                   const Vector_<const Script::NodePays_*>& payments,
                                                                   const Vector_<CalibrationDiagnostic_>& sources) {
        if (!Candidate(instrument))
            return {};
        CalibrationCurveFamilyDescriptor_ result;
        for (const auto& value : instrument.Binding())
            if (value.key_ == "K")
                result.coupon_ = value.value_;
        for (size_t i = 0; i < payments.size(); ++i)
            result.payments_.push_back(Payment(*payments[i], sources[i], context, &result));
        return std::make_shared<const CalibrationCurveFamilyDescriptor_>(std::move(result));
    }
} // namespace Dal::CurveScriptInternal
