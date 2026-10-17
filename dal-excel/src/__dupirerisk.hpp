//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <dal-public/src/dupirerisk.hpp>

#include "__risk.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_DUPIRE_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_DUPIRE_API __declspec(dllimport)
#else
#define DAL_DUPIRE_API
#endif

namespace Dal {
    namespace Excel {
        struct DupireGrid_ {
            Vector_<> inclusionSpots_;
            double maxSpotSpacing_;
            Vector_<> inclusionTimes_;
            double maxTimeSpacing_;
        };

        inline const char* DupireValueType(const DupireGrid_&) { return "DupireGrid"; }
        inline const char* DupireValueType(const DupireRiskInputs_&) { return "DupireRiskInputs"; }
        inline const char* DupireValueType(const DupireCalibrationSnapshot_&) { return "DupireCalibration"; }
        inline const char* DupireValueType(const DupireParameterAdjoints_&) { return "DupireParameterAdjoints"; }
        inline const char* DupireValueType(const DupireDirectQuoteAdjoints_&) { return "DupireDirectQuoteAdjoints"; }
        inline const char* DupireValueType(const DupireQuoteRisk_&) { return "DupireQuoteRisk"; }
        inline const char* DupireValueType(const DupireScriptQuoteRisk_&) { return "DupireScriptQuoteRisk"; }
        inline const char* DupireValueType(const AAD::MertonIVS_&) { return "MertonIVS"; }

        template <class T_> struct StorableDupireValue_ : Storable_ {
            const T_ val_;
            StorableDupireValue_(const String_& name, T_ value) : Storable_(DupireValueType(value), name), val_(std::move(value)) {}
            void Write(Archive::Store_&) const override { THROW("DupireArchiveUnsupported: Dupire handles do not support serialization"); }
        };
    } // namespace Excel

    using StorableDupireGrid_ = Excel::StorableDupireValue_<Excel::DupireGrid_>;
    using StorableDupireRiskInputs_ = Excel::StorableDupireValue_<DupireRiskInputs_>;
    using StorableDupireCalibration_ = Excel::StorableDupireValue_<DupireCalibrationSnapshot_>;
    using StorableDupireParameterAdjoints_ = Excel::StorableDupireValue_<DupireParameterAdjoints_>;
    using StorableDupireDirectQuoteAdjoints_ = Excel::StorableDupireValue_<DupireDirectQuoteAdjoints_>;
    using StorableDupireQuoteRisk_ = Excel::StorableDupireValue_<DupireQuoteRisk_>;
    using StorableDupireScriptQuoteRisk_ = Excel::StorableDupireValue_<DupireScriptQuoteRisk_>;
    using StorableMertonIVS_ = Excel::StorableDupireValue_<AAD::MertonIVS_>;

    DAL_DUPIRE_API void MertonIVS_New(const String_& name, const Matrix_<Cell_>& settings, Handle_<StorableMertonIVS_>* base);
    DAL_DUPIRE_API void DupireModelData_New(const String_& name,
                                            const Handle_<StorableDupireCalibration_>& calibration,
                                            const String_& index,
                                            const String_& currency,
                                            const String_& factor,
                                            double maxStep,
                                            Handle_<ModelData_>* model);
    DAL_DUPIRE_API void DupireDirectQuoteAdjoints_New(const String_& name,
                                                      const Handle_<StorableDupireCalibration_>& calibration,
                                                      const Matrix_<>& adjoints,
                                                      Handle_<StorableDupireDirectQuoteAdjoints_>* direct);
    DAL_DUPIRE_API void DupireParameterAdjoints_FromRisk(const String_& name,
                                                         const Handle_<StorableRiskResult_>& valuation,
                                                         const Handle_<StorableDupireCalibration_>& calibration,
                                                         const String_& component,
                                                         Handle_<StorableDupireParameterAdjoints_>* parameters);
    DAL_DUPIRE_API void DupireScriptQuoteRisk_New(const String_& name,
                                                  const Handle_<StorableRiskResult_>& valuation,
                                                  const Handle_<StorableDupireCalibration_>& calibration,
                                                  const String_& component,
                                                  const Handle_<StorableDupireDirectQuoteAdjoints_>& direct,
                                                  Handle_<StorableDupireScriptQuoteRisk_>* result);
    DAL_DUPIRE_API void DupireCalibration_Get_Surface(const Handle_<StorableDupireCalibration_>& calibration, Handle_<LocalVolSurfaceData_>* surface);
    DAL_DUPIRE_API void DupireCalibration_Get_Spots(const Handle_<StorableDupireCalibration_>& calibration, Vector_<>* spots);
    DAL_DUPIRE_API void DupireCalibration_Get_Times(const Handle_<StorableDupireCalibration_>& calibration, Vector_<>* times);
    DAL_DUPIRE_API void DupireCalibration_Get_Vols(const Handle_<StorableDupireCalibration_>& calibration, Matrix_<>* vols);
    DAL_DUPIRE_API void DupireCalibration_Get_Quotes(const Handle_<StorableDupireCalibration_>& calibration, Matrix_<Cell_>* quotes);
    DAL_DUPIRE_API void DupireCalibration_Get_Provenance(const Handle_<StorableDupireCalibration_>& calibration, Matrix_<Cell_>* provenance);
    DAL_DUPIRE_API void DupireParameterAdjoints_Get_Adjoints(const Handle_<StorableDupireParameterAdjoints_>& parameters, Matrix_<>* adjoints);
    DAL_DUPIRE_API void DupireDirectQuoteAdjoints_Get_Adjoints(const Handle_<StorableDupireDirectQuoteAdjoints_>& direct, Matrix_<>* adjoints);
    DAL_DUPIRE_API void DupireQuoteRisk_Get_Provenance(const Handle_<StorableDupireQuoteRisk_>& result, Matrix_<Cell_>* provenance);
    DAL_DUPIRE_API void DupireScriptQuoteRisk_Get_Valuation(const Handle_<StorableDupireScriptQuoteRisk_>& result,
                                                            Handle_<StorableRiskResult_>* valuation);
    DAL_DUPIRE_API void DupireScriptQuoteRisk_Get_QuoteRisk(const Handle_<StorableDupireScriptQuoteRisk_>& result,
                                                            Handle_<StorableDupireQuoteRisk_>* quoteRisk);
    DAL_DUPIRE_API void DupireScriptQuoteRisk_Get_Provenance(const Handle_<StorableDupireScriptQuoteRisk_>& result, Matrix_<Cell_>* provenance);

    DAL_DUPIRE_API void DupireGrid_New(const String_& name,
                                       const Vector_<>& inclusionSpots,
                                       double maxSpotSpacing,
                                       const Vector_<>& inclusionTimes,
                                       double maxTimeSpacing,
                                       Handle_<StorableDupireGrid_>* grid);
    DAL_DUPIRE_API void DupireRiskInputs_New(const String_& name,
                                             const Vector_<>& quoteStrikes,
                                             const Vector_<>& quoteMaturities,
                                             const Matrix_<>& quoteSpreads,
                                             const Handle_<StorableDupireGrid_>& grid,
                                             Handle_<StorableDupireRiskInputs_>* inputs);
    DAL_DUPIRE_API void DupireCalibration_New(const String_& name,
                                              const Handle_<Storable_>& base,
                                              const Handle_<StorableDupireRiskInputs_>& inputs,
                                              Handle_<StorableDupireCalibration_>* calibration);
    DAL_DUPIRE_API void DupireParameterAdjoints_New(const String_& name,
                                                    const Handle_<StorableDupireCalibration_>& calibration,
                                                    const Matrix_<>& adjoints,
                                                    Handle_<StorableDupireParameterAdjoints_>* parameters);
    DAL_DUPIRE_API void DupireQuoteRisk_New(const String_& name,
                                            const Handle_<StorableDupireCalibration_>& calibration,
                                            const Handle_<StorableDupireParameterAdjoints_>& parameters,
                                            const Handle_<StorableDupireDirectQuoteAdjoints_>& direct,
                                            Handle_<StorableDupireQuoteRisk_>* result);
    DAL_DUPIRE_API void
    DupireQuoteRisk_Get_Adjoints(const Handle_<StorableDupireQuoteRisk_>& result, const String_& contribution, Matrix_<>* adjoints);
} // namespace Dal

#undef DAL_DUPIRE_API
