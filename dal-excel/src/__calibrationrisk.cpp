//
// Created by Codex on 2026/10/5.
//

#include "__calibrationrisk.hpp"

#include <type_traits>

#include "__calibrationinput.hpp"
#include "__platform.hpp"

// clang-format off
/*IF--------------------------------------------------------------------------
public CalibrationPullback_New
    Create an owning common calibration boundary
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateCalibrationTextInput(xl_name, "CalibrationPullback_New; name");
calibration is handle
    Frozen Dupire calibration or captured native curve provenance
&outputs
result is handle StorableCalibrationPullback
    Immutable common calibration boundary
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationParameterAdjoints_New
    Copy canonical parameter adjoints into an owning common seed
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateCalibrationTextInput(xl_name, "CalibrationParameterAdjoints_New; name");
calibration is handle StorableCalibrationPullback
    Common boundary
+argName = "adjoints"; Excel::ValidateCalibrationAdjointsInput(xl_adjoints, "CalibrationParameterAdjoints_New; adjoints", calibration->val_.ParameterRows(), calibration->val_.ParameterCols());
+const Excel::ScriptSettingsInput_ adjointsInput(xl_adjoints); xl_adjoints = adjointsInput.Get();
adjoints is number[][]
    Finite raw PV derivatives in the canonical parameter matrix
&outputs
result is handle StorableCalibrationParameterAdjoints
    Owned parameter seeds
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationDirectQuoteAdjoints_New
    Copy direct quote adjoints into an owning common seed
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateCalibrationTextInput(xl_name, "CalibrationDirectQuoteAdjoints_New; name");
calibration is handle StorableCalibrationPullback
    Common boundary
+argName = "adjoints"; Excel::ValidateCalibrationAdjointsInput(xl_adjoints, "CalibrationDirectQuoteAdjoints_New; adjoints", calibration->val_.QuoteRows(), calibration->val_.QuoteCols());
+const Excel::ScriptSettingsInput_ adjointsInput(xl_adjoints); xl_adjoints = adjointsInput.Get();
adjoints is number[][]
    Finite direct PV derivatives in the canonical quote matrix
&outputs
result is handle StorableCalibrationDirectQuoteAdjoints
    Owned direct quote seeds
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationQuoteRisk_New
    Map passive parameter adjoints into separated raw quote derivatives
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateCalibrationTextInput(xl_name, "CalibrationQuoteRisk_New; name");
calibration is handle StorableCalibrationPullback
    Common boundary
parameters is handle StorableCalibrationParameterAdjoints
    Owned parameter seeds
+xl_direct = Excel::ScriptScalarInput(xl_direct);
&optional
direct is handle StorableCalibrationDirectQuoteAdjoints
    Blank omits the direct contribution
&outputs
result is handle StorableCalibrationQuoteRisk
    Owned calibration, direct and total contributions
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationPullback_Get_Source
    Project the owning typed source without recalibration
&inputs
calibration is handle StorableCalibrationPullback
    Common boundary
&outputs
source is handle
    Frozen Dupire calibration or native curve provenance
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationPullback_Get_Provenance
    Project common method, units, dimensions and source identity metadata
&inputs
calibration is handle StorableCalibrationPullback
    Common boundary
&outputs
provenance is cell[][]
    Key/value metadata; source handle retains complete identity content
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationParameterAdjoints_Get_Calibration
    Project the owning boundary of parameter seeds
&inputs
parameters is handle StorableCalibrationParameterAdjoints
    Owned parameter seeds
&outputs
calibration is handle StorableCalibrationPullback
    Owning common boundary
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationParameterAdjoints_Get_Adjoints
    Copy canonical parameter seeds
&inputs
parameters is handle StorableCalibrationParameterAdjoints
    Owned parameter seeds
&outputs
adjoints is number[][]
    Detached parameter matrix
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationDirectQuoteAdjoints_Get_Calibration
    Project the owning boundary of direct quote seeds
&inputs
direct is handle StorableCalibrationDirectQuoteAdjoints
    Owned direct quote seeds
&outputs
calibration is handle StorableCalibrationPullback
    Owning common boundary
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationDirectQuoteAdjoints_Get_Adjoints
    Copy canonical direct quote seeds
&inputs
direct is handle StorableCalibrationDirectQuoteAdjoints
    Owned direct quote seeds
&outputs
adjoints is number[][]
    Detached quote matrix
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationQuoteRisk_Get_Calibration
    Project the owning boundary of a common quote result
&inputs
result is handle StorableCalibrationQuoteRisk
    Completed common quote risk
&outputs
calibration is handle StorableCalibrationPullback
    Owning common boundary
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationQuoteRisk_Get_Adjoints
    Copy one common quote contribution without another pullback
&inputs
result is handle StorableCalibrationQuoteRisk
    Completed common quote risk
+argName = "contribution"; Excel::ValidateCalibrationTextInput(xl_contribution, "CalibrationQuoteRisk_Get_Adjoints; contribution");
&optional
contribution is string
    Blank or total, calibration, direct
&outputs
adjoints is number[][]
    Detached raw quote derivative matrix
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CalibrationQuoteRisk_Get_Provenance
    Project common quote result provenance
&inputs
result is handle StorableCalibrationQuoteRisk
    Completed common quote risk
&outputs
provenance is cell[][]
    Key/value method, unit, boundary, dimensions and typed source metadata
-IF-------------------------------------------------------------------------*/
// clang-format on

namespace Dal {
    namespace {
        void CheckCalibrationText(const String_& value, const String_& field) {
            REQUIRE(value.find('\0') == String_::npos, "InvalidCalibrationPullback: " + field + "; embedded NUL is unsupported");
        }

        template <class T_> const T_& CheckedCalibrationValue(const Handle_<Excel::StorableCalibrationValue_<T_>>& handle, const String_& field) {
            REQUIRE(handle, "InvalidCalibrationPullback: " + field + "; handle is null");
            return handle->val_;
        }

        CalibrationPullback_ BoundaryFromHandle(const Handle_<Storable_>& calibration) {
            REQUIRE(calibration, "InvalidCalibrationPullback: CalibrationPullback_New; calibration handle is null");
            if (const auto frozen = handle_cast<StorableDupireCalibration_>(calibration))
                return NewCalibrationPullback(frozen->val_);
            if (const auto curve = handle_cast<StorableRateQuoteRiskProvenance_>(calibration)) {
                REQUIRE(curve->Native(),
                        "InvalidCalibrationPullback: CalibrationPullback_New; calibration requires native provenance; reason=" + curve->reason_);
                return NewCalibrationPullback(*curve->val_);
            }
            THROW("InvalidCalibrationPullback: CalibrationPullback_New; calibration must be a frozen Dupire or native curve provenance handle");
        }

        template <class T_>
        void NewCalibrationSeeds(const String_& name,
                                 const Handle_<StorableCalibrationPullback_>& calibration,
                                 const Matrix_<>& adjoints,
                                 const String_& function,
                                 Handle_<Excel::StorableCalibrationValue_<T_>>* result) {
            CheckCalibrationText(name, function + "; name");
            const auto& boundary = CheckedCalibrationValue(calibration, function + "; calibration");
            result->reset(new Excel::StorableCalibrationValue_<T_>(name, T_(boundary, adjoints)));
        }

        template <class T_>
        void ProjectCalibration(const Handle_<Excel::StorableCalibrationValue_<T_>>& handle,
                                const String_& field,
                                Handle_<StorableCalibrationPullback_>* calibration) {
            const auto& value = CheckedCalibrationValue(handle, field);
            calibration->reset(new StorableCalibrationPullback_(String_(), value.Calibration()));
        }

        Matrix_<Cell_> CalibrationProvenance(const CalibrationPullback_& boundary) {
            Vector_<std::pair<String_, Cell_>> fields{{"domain", Cell_(boundary.Domain())},
                                                      {"method", Cell_(boundary.Method())},
                                                      {"unit", Cell_(boundary.Unit())},
                                                      {"boundary", Cell_(boundary.Boundary())},
                                                      {"parameter_rows", Cell_(double(boundary.ParameterRows()))},
                                                      {"parameter_cols", Cell_(double(boundary.ParameterCols()))},
                                                      {"quote_rows", Cell_(double(boundary.QuoteRows()))},
                                                      {"quote_cols", Cell_(double(boundary.QuoteCols()))}};
            std::visit(
                [&](const auto& source) {
                    using Source_ = std::decay_t<decltype(source)>;
                    if constexpr (std::is_same_v<Source_, DupireCalibrationSnapshot_>) {
                        fields.push_back({"source_kind", Cell_("DupireCalibration")});
                        fields.push_back({"source_identity", Cell_("CompleteFrozenCalibrationContent")});
                        fields.push_back({"algorithm", Cell_(source.Algorithm())});
                        fields.push_back({"spot", Cell_(source.Spot())});
                        fields.push_back({"rate", Cell_(source.Rate())});
                        fields.push_back({"dividend_yield", Cell_(source.DividendYield())});
                    } else {
                        fields.push_back({"source_kind", Cell_(source.Kind())});
                        fields.push_back({"source_identity", Cell_("CompleteCapturedCurveContent")});
                        fields.push_back({"calibration_id", Cell_(source.CalibrationId())});
                        fields.push_back({"axis_scheme", Cell_(source.Axis().scheme_)});
                        fields.push_back({"axis_fingerprint", Cell_(source.Axis().fingerprint_)});
                        fields.push_back({"state_scheme", Cell_(source.State().scheme_)});
                        fields.push_back({"state_fingerprint", Cell_(source.State().fingerprint_)});
                        fields.push_back({"calibration_record_bytes", Cell_(double(source.CalibrationRecord().size()))});
                        fields.push_back({"tolerance", Cell_(source.Tolerance())});
                    }
                },
                boundary.Source());
            Matrix_<Cell_> result(static_cast<int>(fields.size()), 2);
            for (int row = 0; row < result.Rows(); ++row) {
                result(row, 0) = fields[row].first;
                result(row, 1) = fields[row].second;
            }
            return result;
        }
    } // namespace

    void CalibrationPullback_New(const String_& name, const Handle_<Storable_>& calibration, Handle_<StorableCalibrationPullback_>* result) {
        CheckCalibrationText(name, "CalibrationPullback_New; name");
        result->reset(new StorableCalibrationPullback_(name, BoundaryFromHandle(calibration)));
    }

    void CalibrationParameterAdjoints_New(const String_& name,
                                          const Handle_<StorableCalibrationPullback_>& calibration,
                                          const Matrix_<>& adjoints,
                                          Handle_<StorableCalibrationParameterAdjoints_>* result) {
        NewCalibrationSeeds(name, calibration, adjoints, "CalibrationParameterAdjoints_New", result);
    }

    void CalibrationDirectQuoteAdjoints_New(const String_& name,
                                            const Handle_<StorableCalibrationPullback_>& calibration,
                                            const Matrix_<>& adjoints,
                                            Handle_<StorableCalibrationDirectQuoteAdjoints_>* result) {
        NewCalibrationSeeds(name, calibration, adjoints, "CalibrationDirectQuoteAdjoints_New", result);
    }

    void CalibrationQuoteRisk_New(const String_& name,
                                  const Handle_<StorableCalibrationPullback_>& calibration,
                                  const Handle_<StorableCalibrationParameterAdjoints_>& parameters,
                                  const Handle_<StorableCalibrationDirectQuoteAdjoints_>& direct,
                                  Handle_<StorableCalibrationQuoteRisk_>* result) {
        CheckCalibrationText(name, "CalibrationQuoteRisk_New; name");
        const auto& boundary = CheckedCalibrationValue(calibration, "CalibrationQuoteRisk_New; calibration");
        const auto& seeds = CheckedCalibrationValue(parameters, "CalibrationQuoteRisk_New; parameters");
        const auto contribution = direct ? std::optional<CalibrationDirectQuoteAdjoints_>(direct->val_) : std::nullopt;
        result->reset(new StorableCalibrationQuoteRisk_(name, PullbackCalibration(boundary, seeds, contribution)));
    }

    void CalibrationPullback_Get_Source(const Handle_<StorableCalibrationPullback_>& calibration, Handle_<Storable_>* source) {
        const auto& boundary = CheckedCalibrationValue(calibration, "CalibrationPullback_Get_Source; calibration");
        *source = std::visit(
            [](const auto& value) -> Handle_<Storable_> {
                using Source_ = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Source_, DupireCalibrationSnapshot_>)
                    return Handle_<Storable_>(new StorableDupireCalibration_(String_(), value));
                else
                    return Handle_<Storable_>(new StorableRateQuoteRiskProvenance_(value));
            },
            boundary.Source());
    }

    void CalibrationPullback_Get_Provenance(const Handle_<StorableCalibrationPullback_>& calibration, Matrix_<Cell_>* provenance) {
        *provenance = CalibrationProvenance(CheckedCalibrationValue(calibration, "CalibrationPullback_Get_Provenance; calibration"));
    }

    void CalibrationParameterAdjoints_Get_Calibration(const Handle_<StorableCalibrationParameterAdjoints_>& parameters,
                                                      Handle_<StorableCalibrationPullback_>* calibration) {
        ProjectCalibration(parameters, "CalibrationParameterAdjoints_Get_Calibration; parameters", calibration);
    }

    void CalibrationParameterAdjoints_Get_Adjoints(const Handle_<StorableCalibrationParameterAdjoints_>& parameters, Matrix_<>* adjoints) {
        *adjoints = CheckedCalibrationValue(parameters, "CalibrationParameterAdjoints_Get_Adjoints; parameters").Adjoints();
    }

    void CalibrationDirectQuoteAdjoints_Get_Calibration(const Handle_<StorableCalibrationDirectQuoteAdjoints_>& direct,
                                                        Handle_<StorableCalibrationPullback_>* calibration) {
        ProjectCalibration(direct, "CalibrationDirectQuoteAdjoints_Get_Calibration; direct", calibration);
    }

    void CalibrationDirectQuoteAdjoints_Get_Adjoints(const Handle_<StorableCalibrationDirectQuoteAdjoints_>& direct, Matrix_<>* adjoints) {
        *adjoints = CheckedCalibrationValue(direct, "CalibrationDirectQuoteAdjoints_Get_Adjoints; direct").Adjoints();
    }

    void CalibrationQuoteRisk_Get_Calibration(const Handle_<StorableCalibrationQuoteRisk_>& result,
                                              Handle_<StorableCalibrationPullback_>* calibration) {
        ProjectCalibration(result, "CalibrationQuoteRisk_Get_Calibration; result", calibration);
    }

    void CalibrationQuoteRisk_Get_Adjoints(const Handle_<StorableCalibrationQuoteRisk_>& result, const String_& contribution, Matrix_<>* adjoints) {
        CheckCalibrationText(contribution, "CalibrationQuoteRisk_Get_Adjoints; contribution");
        const auto& value = CheckedCalibrationValue(result, "CalibrationQuoteRisk_Get_Adjoints; result");
        if (contribution.empty() || contribution == "total")
            *adjoints = value.TotalAdjoints();
        else if (contribution == "calibration")
            *adjoints = value.CalibrationAdjoints();
        else if (contribution == "direct")
            *adjoints = value.DirectAdjoints();
        else
            THROW("InvalidCalibrationPullback: CalibrationQuoteRisk_Get_Adjoints; contribution must be total, calibration or direct");
    }

    void CalibrationQuoteRisk_Get_Provenance(const Handle_<StorableCalibrationQuoteRisk_>& result, Matrix_<Cell_>* provenance) {
        *provenance = CalibrationProvenance(CheckedCalibrationValue(result, "CalibrationQuoteRisk_Get_Provenance; result").Calibration());
    }
    // clang-format off
#ifdef _WIN32
#include <dal-excel/auto/MG_CalibrationPullback_New_public.inc>
#include <dal-excel/auto/MG_CalibrationParameterAdjoints_New_public.inc>
#include <dal-excel/auto/MG_CalibrationDirectQuoteAdjoints_New_public.inc>
#include <dal-excel/auto/MG_CalibrationQuoteRisk_New_public.inc>
#include <dal-excel/auto/MG_CalibrationPullback_Get_Source_public.inc>
#include <dal-excel/auto/MG_CalibrationPullback_Get_Provenance_public.inc>
#include <dal-excel/auto/MG_CalibrationParameterAdjoints_Get_Calibration_public.inc>
#include <dal-excel/auto/MG_CalibrationParameterAdjoints_Get_Adjoints_public.inc>
#include <dal-excel/auto/MG_CalibrationDirectQuoteAdjoints_Get_Calibration_public.inc>
#include <dal-excel/auto/MG_CalibrationDirectQuoteAdjoints_Get_Adjoints_public.inc>
#include <dal-excel/auto/MG_CalibrationQuoteRisk_Get_Calibration_public.inc>
#include <dal-excel/auto/MG_CalibrationQuoteRisk_Get_Adjoints_public.inc>
#include <dal-excel/auto/MG_CalibrationQuoteRisk_Get_Provenance_public.inc>
#endif
    // clang-format on
} // namespace Dal
