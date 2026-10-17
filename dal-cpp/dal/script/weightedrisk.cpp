//
// Created by Codex on 2026-10-06.
//

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

#include <dal/platform/platform.hpp>
#include <dal/script/riskaxisinternal.hpp>
#include <dal/script/weightedrisk.hpp>

namespace Dal::Script {
    namespace {
        Vector_<RiskOutputCoordinate_> SelectOutputs(const Vector_<RiskOutputCoordinate_>& axis,
                                                     const std::optional<Vector_<String_>>& requested,
                                                     const char* kind = "InvalidWeightedRiskRequest") {
            const Vector_<String_> defaults = {"payoff"};
            const auto& ids = requested ? *requested : defaults;
            REQUIRE2(!ids.empty(), String_(kind) + ": output selection must be nonempty; field=outputs", ScriptError_);
            std::map<String_, size_t> available;
            for (size_t slot = 0; slot < axis.size(); ++slot)
                available.emplace(axis[slot].id_, slot);
            std::set<String_> selected;
            Vector_<RiskOutputCoordinate_> outputs;
            outputs.reserve(ids.size());
            for (const auto& id : ids) {
                REQUIRE2(selected.insert(id).second, String_(kind) + ": repeated output ID; output=" + id, ScriptError_);
                const auto found = available.find(id);
                REQUIRE2(found != available.end(), String_(kind) + ": unknown scalar output ID; output=" + id, ScriptError_);
                outputs.push_back(axis[found->second]);
            }
            return outputs;
        }

        void ValidateWeights(const std::optional<Vector_<double>>& weights, size_t count) {
            REQUIRE2(!weights || weights->size() == count, "InvalidWeightedRiskRequest: weight count must match selected outputs; field=weights",
                     ScriptError_);
            if (weights)
                for (size_t component = 0; component < weights->size(); ++component)
                    REQUIRE2(std::isfinite((*weights)[component]),
                             "InvalidWeightedRiskRequest: weight must be finite; component=" + String_(std::to_string(component)), ScriptError_);
        }

        void ValidateProduct(const ScriptProduct_& product,
                             const Date_& date,
                             const char* requestKind = "InvalidWeightedRiskRequest",
                             const char* unsupportedKind = "UnsupportedWeightedRisk",
                             const char* estimator = "weighted") {
            REQUIRE2(date.IsValid(), String_(requestKind) + ": valuation date must be valid; field=evaluationDate", ScriptError_);
            REQUIRE2(!product.ContainsExercise(), String_(unsupportedKind) + ": EXERCISE requires a separate " + estimator + " estimator",
                     ScriptError_);
            REQUIRE2(std::any_of(product.ParsedEventDates().begin(), product.ParsedEventDates().end(),
                                 [&](const Date_& eventDate) { return eventDate >= date; }),
                     String_(unsupportedKind) + ": fully expired products are not supported", ScriptError_);
        }

        void ValidateOutputAxis(const Vector_<RiskOutputCoordinate_>& expected,
                                const Vector_<RiskOutputCoordinate_>& prepared,
                                const char* kind = "InvalidWeightedRiskPlan") {
            REQUIRE2(expected.size() == prepared.size(), String_(kind) + ": prepared output axis extent changed", ScriptError_);
            for (size_t slot = 0; slot < expected.size(); ++slot)
                REQUIRE2(expected[slot].id_ == prepared[slot].id_ && expected[slot].label_ == prepared[slot].label_ &&
                             expected[slot].slot_ == prepared[slot].slot_,
                         String_(kind) + ": prepared output coordinate changed; output=" + expected[slot].id_, ScriptError_);
        }

        void ValidateInputAxis(const Vector_<RiskCoordinate_>& expected,
                               const Vector_<RiskCoordinate_>& prepared,
                               const char* kind = "InvalidWeightedRiskPlan") {
            REQUIRE2(expected.size() == prepared.size(), String_(kind) + ": prepared input axis extent changed", ScriptError_);
            const auto identity = [](const RiskCoordinate_& coordinate) {
                return std::tie(coordinate.id_, coordinate.label_, coordinate.family_, coordinate.ordinal_, coordinate.value_, coordinate.nativeUnit_,
                                coordinate.physicalUnit_, coordinate.reportScale_);
            };
            for (size_t input = 0; input < expected.size(); ++input)
                REQUIRE2(identity(expected[input]) == identity(prepared[input]),
                         String_(kind) + ": prepared input coordinate changed; input=" + expected[input].id_, ScriptError_);
        }
    } // namespace

    namespace Detail {
        Vector_<RiskOutputCoordinate_>
        SelectRiskOutputs(const Vector_<RiskOutputCoordinate_>& axis, const std::optional<Vector_<String_>>& requested, const char* requestKind) {
            return SelectOutputs(axis, requested, requestKind);
        }

        void RequireLiveRiskProduct(
            const ScriptProduct_& product, const Date_& date, const char* requestKind, const char* unsupportedKind, const char* estimator) {
            ValidateProduct(product, date, requestKind, unsupportedKind, estimator);
        }

        void ValidatePreparedRiskOutputs(const Vector_<RiskOutputCoordinate_>& expected,
                                         const Vector_<RiskOutputCoordinate_>& prepared,
                                         const char* planKind) {
            ValidateOutputAxis(expected, prepared, planKind);
        }

        void ValidatePreparedRiskInputs(const Vector_<RiskCoordinate_>& expected, const Vector_<RiskCoordinate_>& prepared, const char* planKind) {
            ValidateInputAxis(expected, prepared, planKind);
        }
    } // namespace Detail

    Vector_<RiskOutputCoordinate_> ScriptRiskOutputAxis(const ScriptProduct_& indexedProduct) {
        const auto& names = indexedProduct.VarNames();
        REQUIRE2(indexedProduct.PayOffIdx() < names.size(), "InvalidWeightedRiskRequest: indexed scalar default receiver is required; output=payoff",
                 ScriptError_);
        Vector_<RiskOutputCoordinate_> axis;
        axis.reserve(names.size());
        for (size_t slot = 0; slot < names.size(); ++slot)
            axis.push_back({slot == indexedProduct.PayOffIdx() ? String_("payoff") : "output:" + String_(std::to_string(slot)), names[slot], slot});
        return axis;
    }

    size_t WeightedRiskResultPayloadBytes(size_t components, size_t inputs) {
        const size_t maximumElements = std::numeric_limits<size_t>::max() / sizeof(double);
        REQUIRE2(components > 0 && inputs < maximumElements, "InvalidWeightedRiskRequest: invalid or overflowing numeric result extent",
                 ScriptError_);
        REQUIRE2(components <= (maximumElements - 1 - inputs) / 2, "InvalidWeightedRiskRequest: numeric result payload byte count overflows",
                 ScriptError_);
        return (1 + inputs + 2 * components) * sizeof(double);
    }

    WeightedRiskPlan_ PlanWeightedRiskRequest(const ScriptProduct_& indexedProduct,
                                              const Vector_<RiskCoordinate_>& completeInputAxis,
                                              const Date_& evaluationDate,
                                              const WeightedRiskRequest_& request,
                                              bool enableAad) {
        ValidateProduct(indexedProduct, evaluationDate);
        auto completeOutputs = ScriptRiskOutputAxis(indexedProduct);
        auto outputs = SelectOutputs(completeOutputs, request.selection_.outputs_);
        ValidateWeights(request.weights_, outputs.size());
        auto inputRequest = request.selection_;
        inputRequest.outputs_.reset();
        inputRequest.numericPayloadBudgetBytes_.reset();
        inputRequest = PlanScalarRiskRequest(completeInputAxis, inputRequest, enableAad);
        const size_t inputs = inputRequest.inputs_ ? inputRequest.inputs_->size() : completeInputAxis.size();
        const size_t payload = WeightedRiskResultPayloadBytes(outputs.size(), inputs);
        REQUIRE2(!request.selection_.numericPayloadBudgetBytes_ || payload <= *request.selection_.numericPayloadBudgetBytes_,
                 "RiskResultBudgetExceeded: weighted numeric payload exceeds numericPayloadBudgetBytes", ScriptError_);
        inputRequest.numericPayloadBudgetBytes_ = request.selection_.numericPayloadBudgetBytes_;
        WeightedRiskPlan_ plan;
        plan.weights_ = request.weights_ ? *request.weights_ : Vector_<double>(outputs.size(), 1.0);
        plan.outputAxis_ = std::move(outputs);
        plan.completeOutputAxis_ = std::move(completeOutputs);
        plan.completeInputAxis_ = completeInputAxis;
        plan.inputRequest_ = std::move(inputRequest);
        plan.evaluationDate_ = evaluationDate;
        plan.enableAad_ = enableAad;
        plan.numericPayloadBytes_ = payload;
        return plan;
    }

    void ValidateWeightedRiskPreparedAxes(const WeightedRiskPlan_& plan,
                                          const ScriptProduct_& preparedProduct,
                                          const Vector_<RiskCoordinate_>& preparedInputAxis) {
        ValidateProduct(preparedProduct, plan.EvaluationDate());
        REQUIRE2(preparedProduct.EvaluationDate() && *preparedProduct.EvaluationDate() == plan.EvaluationDate(),
                 "InvalidWeightedRiskPlan: prepared valuation date changed", ScriptError_);
        ValidateOutputAxis(plan.CompleteOutputAxis(), ScriptRiskOutputAxis(preparedProduct));
        ValidateInputAxis(plan.CompleteInputAxis(), preparedInputAxis);
    }

    WeightedRiskResult_::WeightedRiskResult_(RiskResult_&& objective, const WeightedRiskPlan_& plan, Vector_<double>&& componentMeans)
        : objective_(std::move(objective)), outputAxis_(plan.OutputAxis()), weights_(plan.Weights()), componentMeans_(std::move(componentMeans)) {}

    WeightedRiskResult_ ProjectWeightedMonteCarloRiskResult(const SimResults_& source,
                                                            const Vector_<double>& componentSums,
                                                            int paths,
                                                            const WeightedRiskPlan_& plan,
                                                            const RiskResultProvenance_& provenance) {
        REQUIRE2(paths > 0 && componentSums.size() == plan.OutputAxis().size(), "InvalidWeightedRiskResult: component or path extent changed",
                 ScriptError_);
        REQUIRE2(provenance.method_ == (plan.EnableAad() ? "NativeAAD" : "PriceOnly"),
                 "InvalidWeightedRiskResult: execution method differs from plan", ScriptError_);
        std::ostringstream identity;
        identity.precision(std::numeric_limits<double>::max_digits10);
        identity << "weighted[";
        for (size_t component = 0; component < plan.OutputAxis().size(); ++component) {
            if (component)
                identity << ';';
            identity << plan.OutputAxis()[component].id_ << '=' << plan.Weights()[component];
        }
        identity << ']';
        const auto context = identity.str();
        auto objective =
            ProjectMonteCarloObjectiveRiskResult(source, paths, plan.CompleteInputAxis(), plan.InputRequest(), provenance, context.c_str());
        Vector_<double> means(componentSums.size());
        for (size_t component = 0; component < componentSums.size(); ++component) {
            means[component] = componentSums[component] / static_cast<double>(paths);
            REQUIRE2(std::isfinite(componentSums[component]) && std::isfinite(means[component]),
                     "InvalidWeightedRiskResult: non-finite component sum or mean; output=" + plan.OutputAxis()[component].id_, ScriptError_);
        }
        return WeightedRiskResult_(std::move(objective), plan, std::move(means));
    }
} // namespace Dal::Script
