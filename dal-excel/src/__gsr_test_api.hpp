//
// Created by Codex on 2026/10/2.
//

#pragma once

#include "__platform.hpp"

#include <dal-public/src/gsr.hpp>
#include <dal-public/src/types.hpp>

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_EXCEL_GSR_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_EXCEL_GSR_API __declspec(dllimport)
#else
#define DAL_EXCEL_GSR_API
#endif

namespace Dal {
    template <class T_> struct GSRValueHandle_ : Storable_ {
        T_ value_;
        GSRValueHandle_(const char* type, const T_& value) : Storable_(type, String_()), value_(value) {}
        void Write(Archive::Store_&) const override { THROW("GSR worksheet value handles do not support archives"); }
    };
    using StorableGSRFixedCoupon_ = GSRValueHandle_<GSRFixedCoupon_>;
    using StorableGSRFloatingCoupon_ = GSRValueHandle_<GSRFloatingCoupon_>;
    using StorableGSRCalibrationQuote_ = GSRValueHandle_<GSRCalibrationQuote_>;
    using StorableGSRCalibrationResult_ = GSRValueHandle_<GSRCalibrationResult_>;
    struct StorableGSREuropeanOption_ : Storable_ {
        GSREuropeanOption_ option_;
        explicit StorableGSREuropeanOption_(const GSREuropeanOption_& option) : Storable_("GSREuropeanOption", String_()), option_(option) {}
        void Write(Archive::Store_&) const override { THROW("GSR worksheet option handles do not support archives"); }
    };

    DAL_EXCEL_GSR_API void
    GSRBondOption_New(const Date_& expiry, const Date_& maturity, double strike, const String_& type, Handle_<StorableGSREuropeanOption_>* option);
    DAL_EXCEL_GSR_API void GSRFixedCoupon_New(const Date_& payment, double accrual, Handle_<StorableGSRFixedCoupon_>* coupon);
    DAL_EXCEL_GSR_API void GSRFloatingCoupon_New(const Date_& fixing,
                                                 const Date_& start,
                                                 const Date_& end,
                                                 const Date_& payment,
                                                 double indexAccrual,
                                                 double couponAccrual,
                                                 const String_& tenor,
                                                 Handle_<StorableGSRFloatingCoupon_>* coupon);
    DAL_EXCEL_GSR_API void GSRCaplet_New(const Date_& expiry,
                                         const Handle_<StorableGSRFloatingCoupon_>& coupon,
                                         double strike,
                                         const String_& type,
                                         Handle_<StorableGSREuropeanOption_>* option);
    DAL_EXCEL_GSR_API void GSRSwaption_New(const Date_& expiry,
                                           const Vector_<Handle_<Storable_>>& fixed,
                                           const Vector_<Handle_<Storable_>>& floating,
                                           double strike,
                                           const String_& type,
                                           Handle_<StorableGSREuropeanOption_>* option);
    DAL_EXCEL_GSR_API void GSR_EuropeanOptionPrice(const Handle_<ModelData_>& model,
                                                   const Handle_<StorableGSREuropeanOption_>& option,
                                                   const Matrix_<Cell_>& settings,
                                                   Matrix_<Cell_>* result);
    DAL_EXCEL_GSR_API void GSRCalibrationQuote_New(const String_& name,
                                                   const Handle_<StorableGSREuropeanOption_>& option,
                                                   double price,
                                                   double priceScale,
                                                   Handle_<StorableGSRCalibrationQuote_>* quote);
    DAL_EXCEL_GSR_API void Calibrate_GSRVolatility(const Handle_<ModelData_>& initial,
                                                   const Vector_<Handle_<Storable_>>& quotes,
                                                   const Matrix_<>& parameters,
                                                   const Matrix_<Cell_>& settings,
                                                   Handle_<StorableGSRCalibrationResult_>* result);
    DAL_EXCEL_GSR_API void
    GSRCalibrationResult_Get(const Handle_<StorableGSRCalibrationResult_>& result, const String_& attribute, Matrix_<Cell_>* value);
    DAL_EXCEL_GSR_API void GSRCalibrationResult_Get_Model(const Handle_<StorableGSRCalibrationResult_>& result, Handle_<ModelData_>* model);
} // namespace Dal

#undef DAL_EXCEL_GSR_API
