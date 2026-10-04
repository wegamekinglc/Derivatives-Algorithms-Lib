//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <memory>
#include <optional>

#include <dal/model/surface/lvmodel.hpp>

namespace Dal {
    namespace AAD {
        class IVS_;
    } // namespace AAD

    struct DupireRiskInputs_ {
        Vector_<> quoteStrikes_;
        Vector_<> quoteMaturities_;
        Matrix_<> quoteSpreads_;
        Vector_<> inclusionSpots_;
        double maxSpotSpacing_ = 0.0;
        Vector_<> inclusionTimes_;
        double maxTimeSpacing_ = 0.0;
    };

    struct DupireParameterAdjoints_;
    struct DupireDirectQuoteAdjoints_;
    class DupireQuoteRisk_;

    class DupireCalibrationSnapshot_ {
        struct Data_;
        std::shared_ptr<const Data_> data_;

        explicit DupireCalibrationSnapshot_(std::shared_ptr<const Data_> data);
        friend DupireCalibrationSnapshot_ CalibrateDupireWithRisk(const AAD::IVS_&, const DupireRiskInputs_&, const String_&);
        friend DupireQuoteRisk_ PullbackDupireCalibration(const DupireCalibrationSnapshot_&,
                                                          const DupireParameterAdjoints_&,
                                                          const std::optional<DupireDirectQuoteAdjoints_>&);

    public:
        [[nodiscard]] const DupireRiskInputs_& Inputs() const;
        [[nodiscard]] const Handle_<LocalVolSurfaceData_>& Surface() const;
        [[nodiscard]] double Spot() const;
        [[nodiscard]] double Rate() const;
        [[nodiscard]] double DividendYield() const;
        [[nodiscard]] const String_& Algorithm() const;
        [[nodiscard]] bool Matches(const DupireCalibrationSnapshot_& other) const;
    };

    DupireCalibrationSnapshot_ CalibrateDupireWithRisk(const AAD::IVS_& baseIvs, const DupireRiskInputs_& inputs, const String_& name = {});

    struct DupireParameterAdjoints_ {
        DupireCalibrationSnapshot_ calibration_;
        Matrix_<> adjoints_;
    };

    struct DupireDirectQuoteAdjoints_ {
        DupireCalibrationSnapshot_ calibration_;
        Matrix_<> adjoints_;
    };

    class DupireQuoteRisk_ {
        DupireCalibrationSnapshot_ calibration_;
        Matrix_<> calibrationAdjoints_;
        Matrix_<> directAdjoints_;
        Matrix_<> totalAdjoints_;
        String_ method_ = "NativeAADCalibrationVJP";
        String_ unit_ = "decimal-vol";
        String_ boundary_ = "FixedBaseIVSDeterministicCarryFixedGrids";

        DupireQuoteRisk_(const DupireCalibrationSnapshot_&, Matrix_<>&&, Matrix_<>&&, Matrix_<>&&);
        friend DupireQuoteRisk_ PullbackDupireCalibration(const DupireCalibrationSnapshot_&,
                                                          const DupireParameterAdjoints_&,
                                                          const std::optional<DupireDirectQuoteAdjoints_>&);

    public:
        [[nodiscard]] const DupireCalibrationSnapshot_& Calibration() const { return calibration_; }
        [[nodiscard]] const Matrix_<>& CalibrationAdjoints() const { return calibrationAdjoints_; }
        [[nodiscard]] const Matrix_<>& DirectAdjoints() const { return directAdjoints_; }
        [[nodiscard]] const Matrix_<>& TotalAdjoints() const { return totalAdjoints_; }
        [[nodiscard]] const String_& Method() const { return method_; }
        [[nodiscard]] const String_& Unit() const { return unit_; }
        [[nodiscard]] const String_& Boundary() const { return boundary_; }
    };

    DupireQuoteRisk_ PullbackDupireCalibration(const DupireCalibrationSnapshot_& snapshot,
                                               const DupireParameterAdjoints_& parameterAdjoints,
                                               const std::optional<DupireDirectQuoteAdjoints_>& directQuoteAdjoints = {});
} // namespace Dal
