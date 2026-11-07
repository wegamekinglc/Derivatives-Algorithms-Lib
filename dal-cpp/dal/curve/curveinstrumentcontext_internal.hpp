#pragma once

#include <dal/curve/curveinstrumentcalibration.hpp>
#include <dal/storage/canonicaljson_internal.hpp>

namespace Dal {
    class CurveInstrumentCalibrationEvidence_ {
        const String_ configurationIdentity_, planIdentity_, candidateIdentity_, objectiveIdentity_, metricIdentity_;
        CurveInstrumentCalibrationEvidence_(String_ configuration, String_ plan, String_ candidate = "", String_ objective = "", String_ metric = "")
            : configurationIdentity_(std::move(configuration)), planIdentity_(std::move(plan)), candidateIdentity_(std::move(candidate)),
              objectiveIdentity_(std::move(objective)), metricIdentity_(std::move(metric)) {}
        friend class PreparedCurveInstruments_;
        friend CurveInstrumentCalibrationResult_ CalibrateCurveInstruments(const CurveInstrumentCalibrationSpec_&,
                                                                           const CurveInstrumentCalibrationOptions_&);

    public:
        [[nodiscard]] const String_& ConfigurationIdentity() const { return configurationIdentity_; }
        [[nodiscard]] const String_& PlanIdentity() const { return planIdentity_; }
        [[nodiscard]] const String_& CandidateIdentity() const { return candidateIdentity_; }
        [[nodiscard]] const String_& ObjectiveIdentity() const { return objectiveIdentity_; }
        [[nodiscard]] const String_& MetricIdentity() const { return metricIdentity_; }
    };

    namespace CurveInstrumentInternal {
        CurveCalibrationSpec_ NativeGeometry(const CurveInstrumentCalibrationSpec_& spec);
        bool HasOvernightContext(const CurveInstrumentCalibrationSpec_& spec);
        CurveInstrumentCalibrationSpec_ FreezeOvernightContext(CurveInstrumentCalibrationSpec_ spec);
        CanonicalJson::Json_ MarketJson(const CurveInstrumentMarket_& market);
        CanonicalJson::Json_ CurveJson(const Handle_<DiscountCurve_>& curve);
        CanonicalJson::Json_ Dates(const Vector_<Date_>& dates);
        CanonicalJson::Json_ Numbers(const Vector_<>& values);
        CanonicalJson::Json_ ObservationJson(const Handle_<CalibrationObservation_>& row);
        CanonicalJson::Json_ ContextJson(const CurveInstrumentCalibrationSpec_& spec, bool validateSolver = true);
        String_ ConfigurationIdentity(const CurveInstrumentCalibrationSpec_& spec, const CurveInstrumentCalibrationOptions_& options);
        String_ ObjectiveIdentity(const CurveInstrumentCalibrationResult_& result, const CurveInstrumentSolver_& solver);
        String_ MetricIdentity(const Matrix_<>& metric);
    } // namespace CurveInstrumentInternal
} // namespace Dal
