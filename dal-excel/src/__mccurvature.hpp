//
// Created by Codex on 2026/10/11.
//

#pragma once

#include <dal-public/src/montecarlocurvature.hpp>

#include "__curvaturerequest.hpp"

#if defined(_WIN32) && defined(DAL_EXCEL_TEST_API_EXPORTS)
#define DAL_MC_CURVATURE_API __declspec(dllexport)
#elif defined(_WIN32) && defined(DAL_EXCEL_TEST_API_IMPORTS)
#define DAL_MC_CURVATURE_API __declspec(dllimport)
#else
#define DAL_MC_CURVATURE_API
#endif

namespace Dal::Script {
    inline const char* RiskValueType(const SegmentedMonteCarloSettings_&) { return "SegmentedMonteCarloSettings"; }
} // namespace Dal::Script

namespace Dal {
    inline const char* RiskValueType(const BlackScholesMonteCarloPlan_&) { return "BlackScholesMonteCarloPlan"; }
    inline const char* RiskValueType(const BlackScholesMonteCarloCurvatureResult_&) { return "BlackScholesMonteCarloCurvatureResult"; }

    namespace Excel {
        struct MonteCarloMeanSnapshot_ {
            BlackScholesMonteCarloPlan_ plan_;
            Vector_<> point_;
            Script::SegmentedMonteCarloSettings_ settings_;
            Script::SegmentedMonteCarloResult_ mean_;
        };

        inline const char* RiskValueType(const MonteCarloMeanSnapshot_&) { return "BlackScholesMonteCarloResult"; }
    } // namespace Excel

    using StorableSegmentedMonteCarloSettings_ = Excel::StorableRiskValue_<Script::SegmentedMonteCarloSettings_>;
    using StorableBlackScholesMonteCarloPlan_ = Excel::StorableRiskValue_<BlackScholesMonteCarloPlan_>;
    using StorableBlackScholesMonteCarloResult_ = Excel::StorableRiskValue_<Excel::MonteCarloMeanSnapshot_>;
    using StorableBlackScholesMonteCarloCurvatureResult_ = Excel::StorableRiskValue_<BlackScholesMonteCarloCurvatureResult_>;

    DAL_MC_CURVATURE_API void SegmentedMonteCarloSettings_New(const String_&, const Matrix_<Cell_>&, Handle_<StorableSegmentedMonteCarloSettings_>*);
    DAL_MC_CURVATURE_API void SegmentedMonteCarloSettings_Get_Configuration(const Handle_<StorableSegmentedMonteCarloSettings_>&, Matrix_<Cell_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloPlan_New(const String_&,
                                                             const Handle_<ScriptProductData_>&,
                                                             const Handle_<StorableScriptValuationSettings_>&,
                                                             double,
                                                             Handle_<StorableBlackScholesMonteCarloPlan_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloPlan_Get_Product(const Handle_<StorableBlackScholesMonteCarloPlan_>&,
                                                                     Handle_<ScriptProductData_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloPlan_Get_Valuation(const Handle_<StorableBlackScholesMonteCarloPlan_>&,
                                                                       Handle_<StorableScriptValuationSettings_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloPlan_Get_Smoothing(const Handle_<StorableBlackScholesMonteCarloPlan_>&, double*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloPlan_Get_ParameterLabels(const Handle_<StorableBlackScholesMonteCarloPlan_>&, Vector_<String_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloPlan_Get_ScriptConstants(const Handle_<StorableBlackScholesMonteCarloPlan_>&, Matrix_<Cell_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloPlan_Get_Observations(const Handle_<StorableBlackScholesMonteCarloPlan_>&, Matrix_<Cell_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloResult_New(const String_&,
                                                               const Handle_<StorableBlackScholesMonteCarloPlan_>&,
                                                               const Matrix_<Cell_>&,
                                                               double,
                                                               const Handle_<StorableSegmentedMonteCarloSettings_>&,
                                                               Handle_<StorableBlackScholesMonteCarloResult_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloResult_Get_Value(const Handle_<StorableBlackScholesMonteCarloResult_>&, double*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloResult_Get_Gradient(const Handle_<StorableBlackScholesMonteCarloResult_>&, Matrix_<Cell_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloResult_Get_ParameterLabels(const Handle_<StorableBlackScholesMonteCarloResult_>&,
                                                                               Vector_<String_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloResult_Get_Execution(const Handle_<StorableBlackScholesMonteCarloResult_>&, Matrix_<Cell_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloResult_Get_Plan(const Handle_<StorableBlackScholesMonteCarloResult_>&,
                                                                    Handle_<StorableBlackScholesMonteCarloPlan_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloResult_Get_Point(const Handle_<StorableBlackScholesMonteCarloResult_>&, Matrix_<Cell_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloResult_Get_Settings(const Handle_<StorableBlackScholesMonteCarloResult_>&,
                                                                        Handle_<StorableSegmentedMonteCarloSettings_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloCurvatureResult_New(const String_&,
                                                                        const Handle_<StorableBlackScholesMonteCarloPlan_>&,
                                                                        const Matrix_<Cell_>&,
                                                                        double,
                                                                        const Handle_<StorableBumpOverAADRequest_>&,
                                                                        const Handle_<StorableSegmentedMonteCarloSettings_>&,
                                                                        Handle_<StorableBlackScholesMonteCarloCurvatureResult_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloCurvatureResult_Get_Point(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>&,
                                                                              Matrix_<Cell_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloCurvatureResult_Get_Directions(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>&,
                                                                                   Matrix_<Cell_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloCurvatureResult_Get_Steps(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>&,
                                                                              Matrix_<Cell_>*);
    DAL_MC_CURVATURE_API void
    BlackScholesMonteCarloCurvatureResult_Get_HessianProducts(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>&, Matrix_<Cell_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloCurvatureResult_Get_Shape(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>&,
                                                                              Matrix_<Cell_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloCurvatureResult_Get_Execution(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>&,
                                                                                  Matrix_<Cell_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloCurvatureResult_Get_Base(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>&,
                                                                             Handle_<StorableBlackScholesMonteCarloResult_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloCurvatureResult_Get_Plan(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>&,
                                                                             Handle_<StorableBlackScholesMonteCarloPlan_>*);
    DAL_MC_CURVATURE_API void BlackScholesMonteCarloCurvatureResult_Get_Settings(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>&,
                                                                                 Handle_<StorableSegmentedMonteCarloSettings_>*);
} // namespace Dal

#undef DAL_MC_CURVATURE_API
