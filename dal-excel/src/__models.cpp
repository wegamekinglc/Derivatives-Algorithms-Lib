//
// Created by wegam on 2022/11/20.
//

#include "__curve_storable.hpp"
#include "__models_test_api.hpp"
#include "__platform.hpp"
#include <dal-public/src/models.hpp>

/*IF--------------------------------------------------------------------------
public BSModelData_New
    Black - Scholes model's data description
&inputs
name is string
    A name for the object being created
spot is number
    current spot value
vol is number
    volatility of the underlying
rate is number
    risk-free rate
div is number
    dividend rate
&outputs
model is handle ModelData
    The model data
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public DupireModelData_New
    Dupire local volality model's data description
&inputs
name is string
    A name for the object being created
spot is number
    current spot value
rate is number
    risk-free rate
repo is number
    repo rate including dividend
spots is number[]
    local vol surface spots data
times is number[]
    local vol surface times data
vols is number[][]
    local vol surface data
&outputs
model is handle ModelData
    The model data
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public CorrelatedBSModelData_New
    Create correlated multi-equity Black-Scholes model data
&inputs
name is string
    Model name
indices is string[]
    EQ index names in factor order
spots is number[]
    Initial spots
vols is number[]
    Volatilities
divs is number[]
    Dividend yields
rate is number
    Domestic deterministic rate
correlations is number[][]
    Correlation matrix in index order
&outputs
model is handle ModelData
    Model data handle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public HybridBSEquityData_New
    Create a named hybrid Black-Scholes equity component
&inputs
name is string
    Component name
index is string
    EQ index name
currency is string
    Domestic currency
factor is string
    Named Brownian factor
spot is number
    Initial spot
vol is number
    Volatility
div is number
    Dividend yield
&outputs
component is handle HybridComponentData
    Component handle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public HybridDeterministicRateData_New
    Create a deterministic domestic-rate component
&inputs
name is string
    Component name
currency is string
    Domestic currency
rate is number
    Continuous rate
&outputs
component is handle HybridComponentData
    Component handle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public HybridLogDfRateData_New
    Create a domestic rate component from model-time log discount factors
&inputs
name is string
    Component name
currency is string
    Domestic currency
times is number[]
    ACT/365 times from the valuation date, starting at zero
logDF is number[]
    Log discount factors, starting at zero
&optional
scheme is string
    Log discount-factor interpolation (default LOG_LINEAR)
&outputs
component is handle HybridComponentData
    Component handle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public HybridLogDfRateDataFromCurve_New
    Snapshot a dated discount curve on the hybrid model time axis
&inputs
name is string
    Component name
curve is handle StorableDiscountCurve
    Calibrated domestic discount curve
evaluationDate is date
    Model valuation date
nodeDates is date[]
    Increasing snapshot dates starting at the valuation date
&optional
scheme is string
    Log discount-factor interpolation (default LOG_LINEAR)
&outputs
component is handle HybridComponentData
    Component handle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public HybridConstantCorrelationData_New
    Create a named constant-factor correlation provider
&inputs
name is string
    Provider name
factors is string[]
    Factor names in matrix order
correlations is number[][]
    Positive-definite correlation matrix
&outputs
provider is handle HybridCorrelationData
    Correlation handle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public HybridModelData_New
    Compose named equity and deterministic-rate components
&inputs
name is string
    Model name
domesticCurrency is string
    Domestic currency
components is handle[]
    Equity and rate component handles
correlation is handle HybridCorrelationData
    Named factor correlation handle
&outputs
model is handle ModelData
    Hybrid model data handle
-IF-------------------------------------------------------------------------*/

namespace Dal {
    using Dal::ModelData_;
    namespace {
        void BSModelData_New(const String_& name, double spot, double vol, double rate, double div, Handle_<ModelData_>* model) {
            NewBSModelData(name, spot, vol, rate, div).swap(*model);
        }

        void DupireModelData_New(const String_& name,
                                 double spot,
                                 double rate,
                                 double repo,
                                 const Vector_<>& spots,
                                 const Vector_<>& times,
                                 const Matrix_<>& vols,
                                 Handle_<ModelData_>* model) {
            NewDupireModelData(name, spot, rate, repo, spots, times, vols).swap(*model);
        }
    } // namespace

    void CorrelatedBSModelData_New(const String_& name,
                                   const Vector_<String_>& indices,
                                   const Vector_<>& spots,
                                   const Vector_<>& vols,
                                   const Vector_<>& divs,
                                   double rate,
                                   const Matrix_<>& correlations,
                                   Handle_<ModelData_>* model) {
        Handle_<ModelData_>(new CorrelatedBSModelData_(name, indices, spots, vols, divs, rate, correlations)).swap(*model);
    }

    void HybridBSEquityData_New(const String_& name,
                                const String_& index,
                                const String_& currency,
                                const String_& factor,
                                double spot,
                                double vol,
                                double div,
                                Handle_<HybridComponentData_>* component) {
        Handle_<HybridComponentData_>(new HybridBSEquityData_(name, index, currency, factor, spot, vol, div)).swap(*component);
    }

    void HybridDeterministicRateData_New(const String_& name, const String_& currency, double rate, Handle_<HybridComponentData_>* component) {
        Handle_<HybridComponentData_>(new HybridDeterministicRateData_(name, currency, rate)).swap(*component);
    }

    void HybridLogDfRateData_New(const String_& name,
                                 const String_& currency,
                                 const Vector_<>& times,
                                 const Vector_<>& logDF,
                                 const String_& scheme,
                                 Handle_<HybridComponentData_>* component) {
        NewHybridLogDfRateData(name, currency, times, logDF, scheme.empty() ? "LOG_LINEAR" : scheme).swap(*component);
    }

    void HybridLogDfRateDataFromCurve_New(const String_& name,
                                          const Handle_<StorableDiscountCurve_>& curve,
                                          const Date_& evaluationDate,
                                          const Vector_<Date_>& nodeDates,
                                          const String_& scheme,
                                          Handle_<HybridComponentData_>* component) {
        REQUIRE(curve && curve->val_, "InvalidHybridCurve: source discount curve is required");
        NewHybridLogDfRateDataFromCurve(name, *curve->val_, evaluationDate, nodeDates, scheme.empty() ? "LOG_LINEAR" : scheme).swap(*component);
    }

    void HybridConstantCorrelationData_New(const String_& name,
                                           const Vector_<String_>& factors,
                                           const Matrix_<>& correlations,
                                           Handle_<HybridCorrelationData_>* provider) {
        Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_(name, factors, correlations)).swap(*provider);
    }

    void HybridModelData_New(const String_& name,
                             const String_& domesticCurrency,
                             const Vector_<Handle_<Storable_>>& components,
                             const Handle_<HybridCorrelationData_>& correlation,
                             Handle_<ModelData_>* model) {
        Vector_<Handle_<HybridComponentData_>> typed;
        for (const auto& component : components) {
            const auto cast = handle_cast<HybridComponentData_>(component);
            REQUIRE(cast, "InvalidHybridComponent: expected a hybrid component handle");
            typed.push_back(cast);
        }
        NewHybridModelData(name, HybridSettings_{domesticCurrency, typed, correlation}).swap(*model);
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_BSModelData_New_public.inc>
#include <dal-excel/auto/MG_CorrelatedBSModelData_New_public.inc>
#include <dal-excel/auto/MG_DupireModelData_New_public.inc>
#include <dal-excel/auto/MG_HybridBSEquityData_New_public.inc>
#include <dal-excel/auto/MG_HybridConstantCorrelationData_New_public.inc>
#include <dal-excel/auto/MG_HybridDeterministicRateData_New_public.inc>
#include <dal-excel/auto/MG_HybridLogDfRateDataFromCurve_New_public.inc>
#include <dal-excel/auto/MG_HybridLogDfRateData_New_public.inc>
#include <dal-excel/auto/MG_HybridModelData_New_public.inc>
#endif
} // namespace Dal
