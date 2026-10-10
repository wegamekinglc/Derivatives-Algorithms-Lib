//
// Created by Codex on 2026/10/10.
//

#include <dal/platform/platform.hpp>

#include <cmath>
#include <limits>
#include <utility>

#include <dal/math/aad/tapecapacity.hpp>

#include <dal-public/src/europeanpderisk.hpp>

namespace Dal {
    namespace {
        size_t NumericPayloadBytes(const EuropeanPdeSettings_& settings) {
            const size_t steps = static_cast<size_t>(settings.ordinarySteps_) + 2;
            const size_t nodes = static_cast<size_t>(settings.gridPoints_);
            const size_t maxDoubles = std::numeric_limits<size_t>::max() / sizeof(double);
            REQUIRE(nodes <= maxDoubles - 17 && steps <= (maxDoubles - 17 - nodes) / 6,
                    "EuropeanPdeRisk: numeric payload extent exceeds size_t range");
            return sizeof(double) * (17 + nodes + 6 * steps);
        }

        void CopyPricesAndRisks(const AAD::EuropeanThetaRecording_& recorded,
                                const std::array<AAD::Number_, 3>& parameters,
                                EuropeanPdeRiskResult_* result) {
            for (int layer = 0; layer < 2; ++layer) {
                result->prices_[layer] = AAD::Value(recorded.prices_[layer]);
                REQUIRE(std::isfinite(result->prices_[layer]), "EuropeanPdeRisk: output prices must be finite");
                for (int coordinate = 0; coordinate < 3; ++coordinate) {
                    const double derivative = AAD::NativeOperations_::ReadAdjoint(parameters[coordinate], layer);
                    REQUIRE(std::isfinite(derivative), "EuropeanPdeRisk: output Jacobian entries must be finite");
                    result->jacobian_(layer, coordinate) = derivative;
                }
            }
        }

        void CopyTransposeErrors(const AAD::EuropeanThetaRecording_& recorded, const AAD::SolveAccuracyReports_& reports, Matrix_<>* errors) {
            const auto& entries = reports.Entries();
            REQUIRE(entries.size() == recorded.events_.size() && entries.size() == static_cast<size_t>(errors->Rows()),
                    "EuropeanPdeRisk: unexpected transpose diagnostic count");
            // The owned chain executes every step once, in reverse chronological order.
            for (int step = 0; step < errors->Rows(); ++step) {
                const auto& report = entries[entries.size() - 1 - static_cast<size_t>(step)];
                REQUIRE(report.event_ == recorded.events_[step], "EuropeanPdeRisk: transpose diagnostic event does not match its chronological step");
                const auto& source = report.transposeBackwardErrors_;
                REQUIRE(source.Rows() == 2 && source.Cols() == 2, "EuropeanPdeRisk: unexpected transpose diagnostic shape");
                for (int layer = 0; layer < 2; ++layer)
                    for (int channel = 0; channel < 2; ++channel)
                        (*errors)(step, 2 * layer + channel) = source(layer, channel);
            }
        }
    } // namespace

    EuropeanPdeRiskResult_ EvaluateEuropeanPdeRisk(const EuropeanPdeRiskRequest_& request) {
        AAD::RequireRecordingModeChangeAllowed();
        auto* tape = AAD::Tape();
        REQUIRE(tape->nodes_.OccupiedSlots() == 0 && tape->ReverseEventCount() == 0, "EuropeanPdeRisk: caller must have an empty native tape");
        auto fixed = request;
        fixed.settings_ = AAD::ResolveEuropeanThetaSettings(request.settings_);
        const size_t payload = NumericPayloadBytes(fixed.settings_);
        REQUIRE(!fixed.numericPayloadBudgetBytes_ || payload <= *fixed.numericPayloadBudgetBytes_,
                "EuropeanPdeRisk: numeric_payload_budget_bytes is below the retained result payload");
        EuropeanPdeRiskResult_ result;
        result.request_ = fixed;
        result.grid_ = AAD::EuropeanThetaGrid(fixed.settings_, fixed.point_);
        result.spot_ = result.grid_[*fixed.settings_.spotIndex_];
        result.execution_.actualSteps_ = fixed.settings_.ordinarySteps_ + 2;
        result.execution_.numericPayloadBytes_ = payload;
        result.jacobian_ = Matrix_<>(2, 3);
        result.transposeBackwardErrors_ = Matrix_<>(result.execution_.actualSteps_, 4);
        auto mode = AAD::SetNumResultsForAAD(true, 2);
        AAD::TapeCapacityBudget_ budget(fixed.recordingCapacityBudgetBytes_.value_or(std::numeric_limits<size_t>::max()));
        AAD::TapeCapacityScope_ capacity(&budget, true);
        {
            AAD::RecordingScope_ recording;
            std::array<AAD::Number_, 3> parameters;
            for (int coordinate = 0; coordinate < 3; ++coordinate)
                recording.RegisterInput(parameters[coordinate], fixed.point_[coordinate]);
            recording.StartRecording();
            auto recorded = AAD::RecordEuropeanOptions(&recording, fixed.settings_, parameters, true);
            recording.FinishRecording();
            recording.ClearAdjoints();
            for (size_t layer = 0; layer < 2; ++layer)
                AAD::NativeOperations_::SetSeed(recorded.prices_[layer], 1.0, layer);
            const auto reports = AAD::ReverseWithSolveAccuracy(&recording);
            CopyPricesAndRisks(recorded, parameters, &result);
            result.forwardBackwardErrors_ = std::move(recorded.forwardBackwardErrors_);
            CopyTransposeErrors(recorded, reports, &result.transposeBackwardErrors_);
            result.execution_.reverseScratchPeakBytes_ = tape->ReverseScratchPeakBytes();
            recording.Close();
        }
        result.execution_.peakTapeBytes_ = budget.PeakCapacityBytes();
        result.execution_.cleanupReserveBytes_ = AAD::TapeCleanupCapacityBytes();
        capacity.Close();
        return result;
    }
} // namespace Dal
