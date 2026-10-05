//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <dal-public/src/calibrationrisk.hpp>

namespace Dal {
    struct CalibrationRiskRequest_ {
        std::optional<Vector_<String_>> inputs_;
        std::optional<Vector_<>> reportFactors_;
        std::optional<size_t> numericPayloadBudgetBytes_;
    };

    struct CalibrationQuoteCoordinate_ {
        String_ id_;
        String_ label_;
        size_t ordinal_ = 0;
        int row_ = 0;
        int column_ = 0;
        String_ nativeUnit_;
        double reportScale_ = 1.0;
        std::optional<double> value_;
        std::optional<double> strike_;
        std::optional<double> maturity_;
        std::optional<String_> blockKey_;
        std::optional<int> blockOrdinal_;
    };

    class CalibrationRiskPlan_ {
        CalibrationPullback_ calibration_;
        Vector_<CalibrationQuoteCoordinate_> completeAxis_;
        Vector_<CalibrationQuoteCoordinate_> selectedAxis_;
        Vector_<size_t> selectedOrdinals_;
        size_t numericPayloadBytes_;

        CalibrationRiskPlan_(
            const CalibrationPullback_&, Vector_<CalibrationQuoteCoordinate_>&&, Vector_<CalibrationQuoteCoordinate_>&&, Vector_<size_t>&&, size_t);
        friend CalibrationRiskPlan_ PlanCalibrationRiskRequest(const CalibrationPullback_&, const CalibrationRiskRequest_&);

    public:
        [[nodiscard]] const CalibrationPullback_& Calibration() const { return calibration_; }
        [[nodiscard]] const Vector_<CalibrationQuoteCoordinate_>& CompleteInputAxis() const { return completeAxis_; }
        [[nodiscard]] const Vector_<CalibrationQuoteCoordinate_>& InputAxis() const { return selectedAxis_; }
        [[nodiscard]] const Vector_<size_t>& SelectedOrdinals() const { return selectedOrdinals_; }
        [[nodiscard]] size_t NumericPayloadBytes() const { return numericPayloadBytes_; }
    };

    [[nodiscard]] size_t CalibrationRiskPayloadBytes(size_t quoteRows, size_t quoteCols);
    [[nodiscard]] CalibrationRiskPlan_ PlanCalibrationRiskRequest(const CalibrationPullback_& calibration,
                                                                  const CalibrationRiskRequest_& request = {});

    class CalibrationRiskResult_ {
        CalibrationRiskPlan_ plan_;
        CalibrationQuoteRisk_ quoteRisk_;

        CalibrationRiskResult_(const CalibrationRiskPlan_&, CalibrationQuoteRisk_&&);
        friend CalibrationRiskResult_ PullbackCalibrationWithRisk(const CalibrationRiskPlan_&,
                                                                  const CalibrationParameterAdjoints_&,
                                                                  const std::optional<CalibrationDirectQuoteAdjoints_>&);

    public:
        [[nodiscard]] const CalibrationRiskPlan_& Plan() const { return plan_; }
        [[nodiscard]] const CalibrationQuoteRisk_& QuoteRisk() const { return quoteRisk_; }
        [[nodiscard]] Matrix_<> Jacobian() const;
        [[nodiscard]] Matrix_<> CalibrationJacobian() const;
        [[nodiscard]] Matrix_<> DirectJacobian() const;
        [[nodiscard]] Matrix_<> ReportedJacobian() const;
    };

    [[nodiscard]] CalibrationRiskResult_ PullbackCalibrationWithRisk(const CalibrationRiskPlan_& plan,
                                                                     const CalibrationParameterAdjoints_& parameters,
                                                                     const std::optional<CalibrationDirectQuoteAdjoints_>& direct = {});
} // namespace Dal
