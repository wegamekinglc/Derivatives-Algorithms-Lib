//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <dal/model/gsrmultidata.hpp>

/*IF--------------------------------------------------------------------------
storable GSRLeverageData
    Positive local leverage by short-rate shift and ACT/365 time
version 1
&members
name is ?string
rateShifts is number[]
times is number[]
values is number[][]
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable GSRSLVModelData
    Single-currency GSR stochastic local volatility
version 1
&members
name is ?string
gaussian is handle MultiFactorGSRModelData
leverage is handle GSRLeverageData
kappa is number
volOfVol is number
varianceCorrelations is number[]
maxStep is number
-IF-------------------------------------------------------------------------*/

namespace Dal {
    struct GSRLeverageData_ : Storable_ {
        Vector_<> rateShifts_, times_;
        Matrix_<> values_;

        GSRLeverageData_(const String_& name, const Vector_<>& rateShifts, const Vector_<>& times, const Matrix_<>& values);
        void Write(Archive::Store_& dst) const override;
    };

    struct GSRSLVSettings_ {
        double kappa_ = 1.0, volOfVol_ = 0.5;
        Vector_<> varianceCorrelations_;
        double maxStep_ = 1.0 / 52.0;
    };

    struct GSRSLVModelData_ : ModelData_ {
        Handle_<MultiFactorGSRModelData_> gaussian_;
        Handle_<GSRLeverageData_> leverage_;
        double kappa_, volOfVol_;
        Vector_<> varianceCorrelations_;
        double maxStep_;

        GSRSLVModelData_(const String_& name,
                         const Handle_<MultiFactorGSRModelData_>& gaussian,
                         const Handle_<GSRLeverageData_>& leverage,
                         const GSRSLVSettings_& settings = {})
            : GSRSLVModelData_(name, gaussian, leverage, settings.kappa_, settings.volOfVol_, settings.varianceCorrelations_, settings.maxStep_) {}
        GSRSLVModelData_(const String_& name,
                         const Handle_<MultiFactorGSRModelData_>& gaussian,
                         const Handle_<GSRLeverageData_>& leverage,
                         double kappa,
                         double volOfVol,
                         const Vector_<>& varianceCorrelations,
                         double maxStep);
        // Correlations among the rate and variance drivers, excluding the independent bridge.
        [[nodiscard]] Matrix_<> DriverCorrelation() const;
        // Correlations among all simulation factors in kernel order: rate drivers, variance
        // driver, then the independent bridge driver.
        [[nodiscard]] Matrix_<> FactorCorrelations() const;
        // Deduplicated ACT/365 times where the piecewise g/H or leverage kernels turn over.
        [[nodiscard]] Vector_<> BreakpointTimes() const;
        // Labels of all kernel risk parameters in registration order: curve, g/H, kappa,
        // volOfVol, then the leverage grid.
        [[nodiscard]] Vector_<String_> RiskLabels() const;
        void Write(Archive::Store_& dst) const override;

    private:
        [[nodiscard]] std::unique_ptr<ModelData_> MutantModel(const String_* newName, const Slide_* slide) const override;
    };
} // namespace Dal
