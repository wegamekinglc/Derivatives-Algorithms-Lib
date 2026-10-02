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

/*IF--------------------------------------------------------------------------
public GSRCurveData_New
    Dated initial discount and projection curve nodes for a GSR model
&inputs
name is string
    Curve snapshot name
evaluationDate is date
    Valuation date and first node
currency is string
    Model currency
nodeDates is date[]
    Strictly increasing node dates beginning at the valuation date
discountLogDF is number[]
    OIS log discount factors beginning at zero
projectionTenors is string[]
    Optional projection tenors
projectionLogDF is number[][]
    One log discount factor row per projection tenor
&outputs
curve is handle GSRCurveData
    GSR curve snapshot
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSRCurveDataFromCurveBlock_New
    Snapshot a curve block on specified dates for a GSR model
&inputs
name is string
    Curve snapshot name
block is handle StorableCurveBlock
    Source yield curve block
evaluationDate is date
    Valuation date and first node
nodeDates is date[]
    Snapshot node dates
projectionTenors is string[]
    Projection tenors to snapshot
&outputs
curve is handle GSRCurveData
    GSR curve snapshot
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSRVolData_New
    Piecewise-constant g and H inputs for a GSR model
&inputs
name is string
    Volatility data name
gKnotDates is date[]
    State volatility knot dates
gValues is number[]
    Nonnegative state volatility values
hKnotDates is date[]
    Bond loading knot dates
hValues is number[]
    Positive bond loading values
&outputs
vol is handle GSRVolData
    GSR volatility data
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public GSRModelData_New
    Compose a one-factor GSR rate model
&inputs
name is string
    Model name
curve is handle GSRCurveData
    Initial curve snapshot
vol is handle GSRVolData
    Piecewise-constant g and H
&outputs
model is handle ModelData
    GSR model data
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MultiFactorGSRVolData_New
    Named multi-factor Gaussian rate volatility
&inputs
name is string
    Volatility data name
factorNames is string[]
    Factor names in Brownian order
gKnotDates is date[]
    State volatility knot dates
gValues is number[][]
    Nonnegative volatility, factor rows and date columns
hKnotDates is date[]
    Bond loading knot dates
hValues is number[][]
    Signed loading, factor rows and date columns
correlations is number[][]
    Positive semidefinite factor correlation matrix
&outputs
vol is handle MultiFactorGSRVolData
    GSR volatility data
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MultiFactorGSRModelData_New
    Compose a multi-factor Gaussian short rate model
&inputs
name is string
    Model name
curve is handle GSRCurveData
    Initial curve snapshot
vol is handle MultiFactorGSRVolData
    Named g and H factor inputs
&outputs
model is handle ModelData
    GSR model data
-IF-------------------------------------------------------------------------*/

namespace Dal {
    using Dal::ModelData_;
    namespace {
        void BSModelData_New(const String_& name, double spot, double vol, double rate, double div, Handle_<ModelData_>* model) {
            NewBSModelData(name, spot, vol, rate, div).swap(*model);
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
        NewCorrelatedBSModelData(name, indices, spots, vols, divs, rate, correlations).swap(*model);
    }

    void HybridBSEquityData_New(const String_& name,
                                const String_& index,
                                const String_& currency,
                                const String_& factor,
                                double spot,
                                double vol,
                                double div,
                                Handle_<HybridComponentData_>* component) {
        NewHybridBSEquityData(name, index, currency, factor, spot, vol, div).swap(*component);
    }

    void HybridDeterministicRateData_New(const String_& name, const String_& currency, double rate, Handle_<HybridComponentData_>* component) {
        NewHybridDeterministicRateData(name, currency, rate).swap(*component);
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
        NewHybridConstantCorrelationData(name, factors, correlations).swap(*provider);
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

    void GSRCurveData_New(const String_& name,
                          const Date_& evaluationDate,
                          const String_& currency,
                          const Vector_<Date_>& nodeDates,
                          const Vector_<>& discountLogDF,
                          const Vector_<String_>& projectionTenors,
                          const Matrix_<>& projectionLogDF,
                          Handle_<GSRCurveData_>* curve) {
        NewGSRCurveData(name, evaluationDate, currency, nodeDates, discountLogDF, projectionTenors, projectionLogDF).swap(*curve);
    }

    void GSRCurveDataFromCurveBlock_New(const String_& name,
                                        const Handle_<StorableCurveBlock_>& block,
                                        const Date_& evaluationDate,
                                        const Vector_<Date_>& nodeDates,
                                        const Vector_<String_>& projectionTenors,
                                        Handle_<GSRCurveData_>* curve) {
        REQUIRE(block && block->val_, "InvalidGSRCurve: source curve block is required");
        NewGSRCurveDataFromYieldCurve(name, *block->val_, evaluationDate, nodeDates, projectionTenors).swap(*curve);
    }

    void GSRVolData_New(const String_& name,
                        const Vector_<Date_>& gKnotDates,
                        const Vector_<>& gValues,
                        const Vector_<Date_>& hKnotDates,
                        const Vector_<>& hValues,
                        Handle_<GSRVolData_>* vol) {
        NewGSRVolData(name, gKnotDates, gValues, hKnotDates, hValues).swap(*vol);
    }

    void GSRModelData_New(const String_& name, const Handle_<GSRCurveData_>& curve, const Handle_<GSRVolData_>& vol, Handle_<ModelData_>* model) {
        NewGSRModelData(name, curve, vol).swap(*model);
    }
    void MultiFactorGSRVolData_New(const String_& name,
                                   const Vector_<String_>& factorNames,
                                   const Vector_<Date_>& gKnotDates,
                                   const Matrix_<>& gValues,
                                   const Vector_<Date_>& hKnotDates,
                                   const Matrix_<>& hValues,
                                   const Matrix_<>& correlations,
                                   Handle_<MultiFactorGSRVolData_>* vol) {
        MultiFactorGSRVolSettings_ settings;
        settings.factorNames_ = factorNames;
        settings.gKnotDates_ = gKnotDates;
        settings.gValues_ = gValues;
        settings.hKnotDates_ = hKnotDates;
        settings.hValues_ = hValues;
        settings.correlations_ = correlations;
        NewMultiFactorGSRVolData(name, settings).swap(*vol);
    }
    void MultiFactorGSRModelData_New(const String_& name,
                                     const Handle_<GSRCurveData_>& curve,
                                     const Handle_<MultiFactorGSRVolData_>& vol,
                                     Handle_<ModelData_>* model) {
        NewMultiFactorGSRModelData(name, curve, vol).swap(*model);
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_BSModelData_New_public.inc>
#include <dal-excel/auto/MG_CorrelatedBSModelData_New_public.inc>
#include <dal-excel/auto/MG_GSRCurveDataFromCurveBlock_New_public.inc>
#include <dal-excel/auto/MG_GSRCurveData_New_public.inc>
#include <dal-excel/auto/MG_GSRModelData_New_public.inc>
#include <dal-excel/auto/MG_GSRVolData_New_public.inc>
#include <dal-excel/auto/MG_HybridBSEquityData_New_public.inc>
#include <dal-excel/auto/MG_HybridConstantCorrelationData_New_public.inc>
#include <dal-excel/auto/MG_HybridDeterministicRateData_New_public.inc>
#include <dal-excel/auto/MG_HybridLogDfRateDataFromCurve_New_public.inc>
#include <dal-excel/auto/MG_HybridLogDfRateData_New_public.inc>
#include <dal-excel/auto/MG_HybridModelData_New_public.inc>
#include <dal-excel/auto/MG_MultiFactorGSRModelData_New_public.inc>
#include <dal-excel/auto/MG_MultiFactorGSRVolData_New_public.inc>
#endif
} // namespace Dal
