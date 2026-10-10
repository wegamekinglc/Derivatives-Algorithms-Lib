//
// Created by Codex on 2026/10/10.
//

#pragma once

#include <array>
#include <optional>

#include <dal/math/aad/europeantheta.hpp>

namespace Dal {
    using EuropeanPdeSettings_ = AAD::EuropeanThetaSettings_;

    struct EuropeanPdeRiskRequest_ {
        std::array<double, 3> point_ = {0.05, 0.20, 110.0};
        EuropeanPdeSettings_ settings_;
        std::optional<size_t> numericPayloadBudgetBytes_;
        std::optional<size_t> recordingCapacityBudgetBytes_;
    };

    struct EuropeanPdeRiskExecution_ {
        int actualSteps_ = 0;
        size_t numericPayloadBytes_ = 0;
        size_t peakTapeBytes_ = 0;
        size_t cleanupReserveBytes_ = 0;
        size_t reverseScratchPeakBytes_ = 0;
    };

    struct EuropeanPdeRiskResult_ {
        EuropeanPdeRiskRequest_ request_;
        std::array<double, 2> prices_{};
        Matrix_<> jacobian_;
        Vector_<> grid_;
        double spot_ = 0.0;
        Matrix_<> forwardBackwardErrors_, transposeBackwardErrors_;
        EuropeanPdeRiskExecution_ execution_;
        String_ method_ = "NativeAADFixedGridEuropeanTheta";
        Vector_<String_> payoffLabels_ = {"Call", "Put"};
        Vector_<String_> parameterLabels_ = {"Rate", "Volatility", "Strike"};
        Vector_<String_> parameterUnits_ = {"price per decimal rate", "price per decimal volatility", "price per strike price unit"};
        Vector_<String_> transposeErrorLabels_ = {"CallLayer/CallSeed", "CallLayer/PutSeed", "PutLayer/CallSeed", "PutLayer/PutSeed"};
    };

    [[nodiscard]] EuropeanPdeRiskResult_ EvaluateEuropeanPdeRisk(const EuropeanPdeRiskRequest_& request);
} // namespace Dal
