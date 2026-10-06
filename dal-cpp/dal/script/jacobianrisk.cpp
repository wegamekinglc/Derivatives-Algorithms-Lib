//
// Created by Codex on 2026/10/06.
//

#include <cmath>
#include <limits>

#include <dal/platform/platform.hpp>
#include <dal/script/blockedreplay.hpp>
#include <dal/script/jacobianrisk.hpp>
#include <dal/script/riskaxisinternal.hpp>

namespace Dal::Script {
    namespace {
        void ValidateJacobianWidths(const Detail::AADBlockReplayResult_& source, const JacobianRiskPlan_& plan) {
            REQUIRE2(!source.actualWidths_.empty() && source.actualWidths_.size() == source.replayAttempts_,
                     "InvalidJacobianRiskResult: widths and replay attempts must agree", ScriptError_);
            size_t remaining = plan.OutputAxis().size();
            for (const auto width : source.actualWidths_) {
                REQUIRE2(width > 0 && width <= plan.Request().maxBlockWidth_ && width == source.actualWidths_.front(),
                         "InvalidJacobianRiskResult: invalid executed block width", ScriptError_);
                REQUIRE2(remaining > 0, "InvalidJacobianRiskResult: unexpected extra replay", ScriptError_);
                remaining -= std::min(width, remaining);
            }
            REQUIRE2(remaining == 0, "InvalidJacobianRiskResult: output rows were not fully replayed", ScriptError_);
        }

        void ValidateJacobianExecution(const Detail::AADBlockReplayResult_& source, int paths, const JacobianRiskPlan_& plan) {
            REQUIRE2(paths > 0, "InvalidJacobianRiskResult: paths must be a positive integer", ScriptError_);
            if (plan.EnableAad())
                ValidateJacobianWidths(source, plan);
            else {
                REQUIRE2(source.actualWidths_.empty() && source.replayAttempts_ == 1 && source.peakTapeBytes_ == 0,
                         "InvalidJacobianRiskResult: passive execution cannot report native recording blocks", ScriptError_);
            }
            REQUIRE2(source.executedPaths_ == Detail::ReplayExtentProduct(source.replayAttempts_, static_cast<size_t>(paths)),
                     "InvalidJacobianRiskResult: forward path count must include every replay", ScriptError_);
            REQUIRE2(!plan.Request().recordingCapacityBudgetBytes_ || source.peakTapeBytes_ <= *plan.Request().recordingCapacityBudgetBytes_,
                     "InvalidJacobianRiskResult: recording peak exceeds requested capacity", ScriptError_);
            REQUIRE2(!plan.Request().scratchCapacityBudgetBytes_ || source.peakScratchBytes_ <= *plan.Request().scratchCapacityBudgetBytes_,
                     "InvalidJacobianRiskResult: scratch peak exceeds requested capacity", ScriptError_);
        }

        void ValidateJacobianNumbers(const Detail::AADBlockReplayResult_& source, const JacobianRiskPlan_& plan) {
            REQUIRE2(source.values_.size() == plan.OutputAxis().size() && source.jacobian_.Rows() == static_cast<int>(plan.OutputAxis().size()) &&
                         source.jacobian_.Cols() == static_cast<int>(plan.InputAxis().size()),
                     "InvalidJacobianRiskResult: numeric and selected axis dimensions disagree", ScriptError_);
            for (size_t row = 0; row < source.values_.size(); ++row) {
                const auto& output = plan.OutputAxis()[row].id_;
                REQUIRE2(std::isfinite(source.values_[row]), "InvalidJacobianRiskResult: non-finite mean value; output=" + output, ScriptError_);
                for (size_t column = 0; column < plan.InputAxis().size(); ++column) {
                    const auto& input = plan.InputAxis()[column];
                    const auto value = source.jacobian_(static_cast<int>(row), static_cast<int>(column));
                    REQUIRE2(std::isfinite(value) && std::isfinite(value * input.reportScale_),
                             "InvalidJacobianRiskResult: non-finite raw or reported risk; output=" + output + "; input=" + input.id_, ScriptError_);
                }
            }
        }
    } // namespace

    JacobianRiskPlan_ PlanJacobianRiskRequest(const ScriptProduct_& indexedProduct,
                                              const Vector_<RiskCoordinate_>& completeInputAxis,
                                              const Date_& evaluationDate,
                                              const JacobianRiskRequest_& request,
                                              bool enableAad) {
        Detail::RequireLiveRiskProduct(indexedProduct, evaluationDate, "InvalidJacobianRiskRequest", "UnsupportedJacobianRisk", "Jacobian");
        REQUIRE2(request.maxBlockWidth_ > 0 && request.maxBlockWidth_ <= AAD::ADJ_SIZE,
                 "InvalidJacobianRiskRequest: maxBlockWidth must be positive and at most ADJ_SIZE; field=maxBlockWidth", ScriptError_);
        JacobianRiskPlan_ plan;
        plan.request_ = request;
        plan.completeOutputAxis_ = ScriptRiskOutputAxis(indexedProduct);
        plan.outputAxis_ = Detail::SelectRiskOutputs(plan.completeOutputAxis_, request.selection_.outputs_, "InvalidJacobianRiskRequest");
        auto inputs = request.selection_;
        inputs.outputs_.reset();
        inputs.numericPayloadBudgetBytes_.reset();
        inputs = PlanScalarRiskRequest(completeInputAxis, inputs, enableAad);
        plan.inputPositions_ = Detail::RiskInputPositions(completeInputAxis, inputs);
        plan.inputAxis_.reserve(plan.inputPositions_.size());
        for (size_t column = 0; column < plan.inputPositions_.size(); ++column) {
            auto coordinate = completeInputAxis[plan.inputPositions_[column]];
            if (inputs.reportFactors_)
                coordinate.reportScale_ = (*inputs.reportFactors_)[column];
            plan.inputAxis_.push_back(std::move(coordinate));
        }
        plan.numericPayloadBytes_ = RiskResultPayloadBytes(plan.outputAxis_.size(), plan.inputAxis_.size());
        REQUIRE2(!request.selection_.numericPayloadBudgetBytes_ || plan.numericPayloadBytes_ <= *request.selection_.numericPayloadBudgetBytes_,
                 "RiskResultBudgetExceeded: Jacobian numeric payload exceeds numericPayloadBudgetBytes", ScriptError_);
        REQUIRE2(plan.outputAxis_.size() <= static_cast<size_t>(std::numeric_limits<int>::max()),
                 "InvalidJacobianRiskRequest: output count exceeds matrix dimension limit", ScriptError_);
        plan.completeInputAxis_ = completeInputAxis;
        plan.evaluationDate_ = evaluationDate;
        plan.enableAad_ = enableAad;
        return plan;
    }

    void ValidateJacobianRiskPreparedAxes(const JacobianRiskPlan_& plan,
                                          const ScriptProduct_& preparedProduct,
                                          const Vector_<RiskCoordinate_>& preparedInputAxis) {
        Detail::RequireLiveRiskProduct(preparedProduct, plan.EvaluationDate(), "InvalidJacobianRiskPlan", "UnsupportedJacobianRisk", "Jacobian");
        REQUIRE2(preparedProduct.EvaluationDate() && *preparedProduct.EvaluationDate() == plan.EvaluationDate(),
                 "InvalidJacobianRiskPlan: prepared valuation date changed", ScriptError_);
        Detail::ValidatePreparedRiskOutputs(plan.CompleteOutputAxis(), ScriptRiskOutputAxis(preparedProduct), "InvalidJacobianRiskPlan");
        Detail::ValidatePreparedRiskInputs(plan.CompleteInputAxis(), preparedInputAxis, "InvalidJacobianRiskPlan");
    }

    JacobianRiskResult_::JacobianRiskResult_(Detail::AADBlockReplayResult_&& source,
                                             const JacobianRiskPlan_& plan,
                                             const RiskResultProvenance_& provenance)
        : values_(std::move(source.values_)), jacobian_(std::move(source.jacobian_)), outputAxis_(plan.OutputAxis()),
          completeOutputAxis_(plan.CompleteOutputAxis()), inputAxis_(plan.InputAxis()), completeInputAxis_(plan.CompleteInputAxis()),
          provenance_(provenance), execution_{std::move(source.actualWidths_), source.replayAttempts_, source.executedPaths_, source.peakTapeBytes_,
                                              source.peakScratchBytes_} {}

    Matrix_<> JacobianRiskResult_::ReportedJacobian() const {
        auto reported = jacobian_;
        for (int row = 0; row < reported.Rows(); ++row)
            for (size_t column = 0; column < inputAxis_.size(); ++column)
                reported(row, static_cast<int>(column)) *= inputAxis_[column].reportScale_;
        return reported;
    }

    JacobianRiskResult_ ProjectJacobianRiskResult(Detail::AADBlockReplayResult_&& source,
                                                  int paths,
                                                  const JacobianRiskPlan_& plan,
                                                  const RiskResultProvenance_& provenance) {
        Detail::ValidateRiskResultMetadata(provenance, paths);
        REQUIRE2(provenance.method_ == (plan.EnableAad() ? "NativeAAD" : "PriceOnly"),
                 "InvalidJacobianRiskResult: execution method differs from plan", ScriptError_);
        ValidateJacobianExecution(source, paths, plan);
        ValidateJacobianNumbers(source, plan);
        return JacobianRiskResult_(std::move(source), plan, provenance);
    }
} // namespace Dal::Script
