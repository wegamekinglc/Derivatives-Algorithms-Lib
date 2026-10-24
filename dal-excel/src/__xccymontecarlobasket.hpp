#pragma once

#include "__xccymontecarlo.hpp"
#include <dal-public/src/xccymontecarlobasket.hpp>

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_XCCY_BASKET_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_XCCY_BASKET_API __declspec(dllimport)
#else
#define DAL_XCCY_BASKET_API
#endif

namespace Dal {
    using StorableXccyMonteCarloBasketMembers_ = StorableCalibrationValue_<Vector_<XccyMcBasketMember_>>;
    using StorableXccyMonteCarloBasketRequest_ = StorableCalibrationValue_<XccyMonteCarloBasketRequest_>;
    using StorablePreparedXccyMonteCarloBasket_ = StorableCalibrationValue_<PreparedXccyMonteCarloBasket_>;
    using StorableXccyMonteCarloBasketResult_ = StorableCalibrationValue_<XccyMonteCarloBasketResult_>;
    using StorableXccyMonteCarloBasketPartial_ = StorableCalibrationValue_<Handle_<XccyMcBasketPartial_>>;
    struct XccyMonteCarloBasketOutcome_ {
        String_ status_;
        std::optional<CalibrationDiagnostic_> diagnostic_;
        Handle_<CalibrationReport_> report_;
        Handle_<StorablePreparedXccyMonteCarloBasket_> prepared_;
        Handle_<XccyGaussianModelData_> model_;
        Handle_<StorableXccyMonteCarloBasketResult_> result_;
        Handle_<XccyMcBasketPartial_> partial_;
    };
    using StorableXccyMonteCarloBasketOutcome_ = StorableCalibrationValue_<XccyMonteCarloBasketOutcome_>;

    DAL_XCCY_BASKET_API void XccyMonteCarloBasketMembers_New(const Matrix_<Cell_>& members, Handle_<StorableXccyMonteCarloBasketMembers_>* value);
    DAL_XCCY_BASKET_API void XccyMonteCarloBasketRequest_New(const Cell_& phase,
                                                             const Handle_<StorableCalibrationMonteCarloSampling_>& sampling,
                                                             const Handle_<StorableMarketFixingSnapshot_>& fixings,
                                                             const Matrix_<Cell_>& settings,
                                                             Handle_<StorableXccyMonteCarloBasketRequest_>* value);
    DAL_XCCY_BASKET_API void XccyBasket_PrepareMonteCarlo(const Handle_<StorableXccyMonteCarloBasketMembers_>& members,
                                                          const Handle_<XccyGaussianModelData_>& model,
                                                          const Handle_<StorableXccyMonteCarloBasketRequest_>& request,
                                                          Handle_<StorableXccyMonteCarloBasketOutcome_>* outcome);
    DAL_XCCY_BASKET_API void XccyBasket_EvaluateMonteCarlo(const Handle_<StorablePreparedXccyMonteCarloBasket_>& prepared,
                                                           const Handle_<XccyGaussianModelData_>& candidate,
                                                           Handle_<StorableXccyMonteCarloBasketOutcome_>* outcome);
    DAL_XCCY_BASKET_API void XccyMonteCarloBasketOutcome_Get_Status(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome, String_* status);
    DAL_XCCY_BASKET_API void XccyMonteCarloBasketOutcome_Get_Report(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome,
                                                                    Handle_<CalibrationReport_>* report);
    DAL_XCCY_BASKET_API void XccyMonteCarloBasketOutcome_Get_Prepared(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome,
                                                                      Handle_<StorablePreparedXccyMonteCarloBasket_>* prepared);
    DAL_XCCY_BASKET_API void XccyMonteCarloBasketOutcome_Get_Model(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome,
                                                                   Handle_<XccyGaussianModelData_>* model);
    DAL_XCCY_BASKET_API void XccyMonteCarloBasketOutcome_Get_Result(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome,
                                                                    Handle_<StorableXccyMonteCarloBasketResult_>* result);
    DAL_XCCY_BASKET_API void XccyMonteCarloBasketOutcome_Get_Partial(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome,
                                                                     Handle_<StorableXccyMonteCarloBasketPartial_>* partial);
    DAL_XCCY_BASKET_API void XccyMonteCarloBasketOutcome_Get_Json(const Handle_<StorableXccyMonteCarloBasketOutcome_>& outcome,
                                                                  Vector_<String_>* json);
    DAL_XCCY_BASKET_API void XccyMonteCarloBasketPartial_Get_Json(const Handle_<StorableXccyMonteCarloBasketPartial_>& partial,
                                                                  Vector_<String_>* json);
#if defined(DAL_EXCEL_TEST_API_EXPORTS) || defined(DAL_EXCEL_TEST_API_IMPORTS) || defined(DAL_EXCEL_API_TESTS_PORTABLE)
    DAL_XCCY_BASKET_API void XccyBasketFailureForTesting(const XccyMcBasketError_& error, Handle_<StorableXccyMonteCarloBasketOutcome_>* outcome);
#endif
} // namespace Dal

#undef DAL_XCCY_BASKET_API
