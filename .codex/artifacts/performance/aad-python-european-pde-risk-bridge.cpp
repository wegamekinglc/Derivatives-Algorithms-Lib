//
// Created by Codex on 2026/10/10.
//

#include <algorithm>
#include <array>
#include <exception>

#include <dal/math/aad/native.hpp>

#include "test-support/europeanaadfd.hpp"

#if defined(DAL_PDE_OWNING_BOUNDARY)
#include <dal-public/src/europeanpderisk.hpp>
#endif

extern "C" int DalEuropeanNativeRisk(int nodes, int intervals, double* output) {
    try {
        using namespace Dal::AAD;
        auto mode = SetNumResultsForAAD(true, 2);
        RecordingScope_ scope;
        std::array<Number_, 3> parameters;
        const std::array<double, 3> point = {0.05, 0.20, 110.0};
        for (int coordinate = 0; coordinate < 3; ++coordinate)
            scope.RegisterInput(parameters[coordinate], point[coordinate]);
        scope.StartRecording();
        auto recorded = Example::RecordEuropeanOptions(&scope, {nodes, intervals}, parameters);
        scope.FinishRecording();
        scope.ClearAdjoints();
        for (size_t layer = 0; layer < 2; ++layer)
            NativeOperations_::SetSeed(recorded.prices_[layer], 1.0, layer);
        const auto reports = ReverseWithSolveAccuracy(&scope);
        for (int layer = 0; layer < 2; ++layer) {
            output[layer] = Value(recorded.prices_[layer]);
            for (int coordinate = 0; coordinate < 3; ++coordinate)
                output[2 + 3 * layer + coordinate] = NativeOperations_::ReadAdjoint(parameters[coordinate], layer);
        }
        output[8] = recorded.largestForwardError_;
        output[9] = 0.0;
        for (const auto& report : reports.Entries())
            for (double error : report.transposeBackwardErrors_)
                output[9] = std::max(output[9], error);
        scope.Close();
        return 0;
    } catch (const std::exception&) {
        return 1;
    }
}

#if defined(DAL_PDE_OWNING_BOUNDARY)
extern "C" int DalEuropeanOwningRisk(int nodes, int intervals, double* output) {
    try {
        Dal::EuropeanPdeRiskRequest_ request;
        request.settings_.gridPoints_ = nodes;
        request.settings_.ordinarySteps_ = intervals;
        const auto result = Dal::EvaluateEuropeanPdeRisk(request);
        for (int layer = 0; layer < 2; ++layer) {
            output[layer] = result.prices_[layer];
            for (int coordinate = 0; coordinate < 3; ++coordinate)
                output[2 + 3 * layer + coordinate] = result.jacobian_(layer, coordinate);
        }
        output[8] = *std::max_element(result.forwardBackwardErrors_.begin(), result.forwardBackwardErrors_.end());
        output[9] = *std::max_element(result.transposeBackwardErrors_.begin(), result.transposeBackwardErrors_.end());
        return 0;
    } catch (const std::exception&) {
        return 1;
    }
}
#endif
