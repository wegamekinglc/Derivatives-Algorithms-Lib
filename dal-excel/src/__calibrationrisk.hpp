//
// Created by Codex on 2026/10/5.
//

#pragma once

#include <dal-public/src/calibrationrisk.hpp>

#include "__curve_storable.hpp"
#include "__dupirerisk.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_CALIBRATION_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_CALIBRATION_API __declspec(dllimport)
#else
#define DAL_CALIBRATION_API
#endif

namespace Dal {
    namespace Excel {
        inline const char* CalibrationValueType(const CalibrationPullback_&) { return "CalibrationPullback"; }
        inline const char* CalibrationValueType(const CalibrationParameterAdjoints_&) { return "CalibrationParameterAdjoints"; }
        inline const char* CalibrationValueType(const CalibrationDirectQuoteAdjoints_&) { return "CalibrationDirectQuoteAdjoints"; }
        inline const char* CalibrationValueType(const CalibrationQuoteRisk_&) { return "CalibrationQuoteRisk"; }

        template <class T_> struct StorableCalibrationValue_ : Storable_ {
            const T_ val_;
            StorableCalibrationValue_(const String_& name, T_ value) : Storable_(CalibrationValueType(value), name), val_(std::move(value)) {}
            void Write(Archive::Store_&) const override {
                THROW("CalibrationArchiveUnsupported: common calibration handles do not support serialization");
            }
        };
    } // namespace Excel

    using StorableCalibrationPullback_ = Excel::StorableCalibrationValue_<CalibrationPullback_>;
    using StorableCalibrationParameterAdjoints_ = Excel::StorableCalibrationValue_<CalibrationParameterAdjoints_>;
    using StorableCalibrationDirectQuoteAdjoints_ = Excel::StorableCalibrationValue_<CalibrationDirectQuoteAdjoints_>;
    using StorableCalibrationQuoteRisk_ = Excel::StorableCalibrationValue_<CalibrationQuoteRisk_>;

    DAL_CALIBRATION_API void
    CalibrationPullback_New(const String_& name, const Handle_<Storable_>& calibration, Handle_<StorableCalibrationPullback_>* result);
    DAL_CALIBRATION_API void CalibrationParameterAdjoints_New(const String_& name,
                                                              const Handle_<StorableCalibrationPullback_>& calibration,
                                                              const Matrix_<>& adjoints,
                                                              Handle_<StorableCalibrationParameterAdjoints_>* result);
    DAL_CALIBRATION_API void CalibrationDirectQuoteAdjoints_New(const String_& name,
                                                                const Handle_<StorableCalibrationPullback_>& calibration,
                                                                const Matrix_<>& adjoints,
                                                                Handle_<StorableCalibrationDirectQuoteAdjoints_>* result);
    DAL_CALIBRATION_API void CalibrationQuoteRisk_New(const String_& name,
                                                      const Handle_<StorableCalibrationPullback_>& calibration,
                                                      const Handle_<StorableCalibrationParameterAdjoints_>& parameters,
                                                      const Handle_<StorableCalibrationDirectQuoteAdjoints_>& direct,
                                                      Handle_<StorableCalibrationQuoteRisk_>* result);

    DAL_CALIBRATION_API void CalibrationPullback_Get_Source(const Handle_<StorableCalibrationPullback_>& calibration, Handle_<Storable_>* source);
    DAL_CALIBRATION_API void CalibrationPullback_Get_Provenance(const Handle_<StorableCalibrationPullback_>& calibration, Matrix_<Cell_>* provenance);
    DAL_CALIBRATION_API void CalibrationParameterAdjoints_Get_Calibration(const Handle_<StorableCalibrationParameterAdjoints_>& parameters,
                                                                          Handle_<StorableCalibrationPullback_>* calibration);
    DAL_CALIBRATION_API void CalibrationParameterAdjoints_Get_Adjoints(const Handle_<StorableCalibrationParameterAdjoints_>& parameters,
                                                                       Matrix_<>* adjoints);
    DAL_CALIBRATION_API void CalibrationDirectQuoteAdjoints_Get_Calibration(const Handle_<StorableCalibrationDirectQuoteAdjoints_>& direct,
                                                                            Handle_<StorableCalibrationPullback_>* calibration);
    DAL_CALIBRATION_API void CalibrationDirectQuoteAdjoints_Get_Adjoints(const Handle_<StorableCalibrationDirectQuoteAdjoints_>& direct,
                                                                         Matrix_<>* adjoints);
    DAL_CALIBRATION_API void CalibrationQuoteRisk_Get_Calibration(const Handle_<StorableCalibrationQuoteRisk_>& result,
                                                                  Handle_<StorableCalibrationPullback_>* calibration);
    DAL_CALIBRATION_API void
    CalibrationQuoteRisk_Get_Adjoints(const Handle_<StorableCalibrationQuoteRisk_>& result, const String_& contribution, Matrix_<>* adjoints);
    DAL_CALIBRATION_API void CalibrationQuoteRisk_Get_Provenance(const Handle_<StorableCalibrationQuoteRisk_>& result, Matrix_<Cell_>* provenance);
} // namespace Dal

#undef DAL_CALIBRATION_API
