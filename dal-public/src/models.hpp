//
// Created by wegam on 2022/11/20.
//

#pragma once

#include <cmath>

#include <dal/curve/discount.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/model/correlatedblackscholes.hpp>
#include <dal/model/dupire.hpp>
#include <dal/model/hybriddata.hpp>
#include <dal/model/gsrdata.hpp>
#include <dal/curve/yc.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/platform/consts.hpp>

namespace Dal {
    Handle_<ModelData_> NewCorrelatedBSModelData(const String_& name,
                                                 const Vector_<String_>& indices,
                                                 const Vector_<>& spots,
                                                 const Vector_<>& vols,
                                                 const Vector_<>& divs,
                                                 double rate,
                                                 const Matrix_<>& correlations);
    Handle_<HybridComponentData_> NewHybridBSEquityData(
        const String_& name, const String_& index, const String_& currency, const String_& factor, double spot, double vol, double div);
    Handle_<HybridComponentData_> NewHybridDeterministicRateData(const String_& name, const String_& currency, double rate);
    Handle_<HybridCorrelationData_>
    NewHybridConstantCorrelationData(const String_& name, const Vector_<String_>& factors, const Matrix_<>& correlations);

    FORCE_INLINE Handle_<ModelData_> NewBSModelData(const String_& name, double spot, double vol, double rate, double div) {
        return Handle_<ModelData_>(new BSModelData_(name, spot, vol, rate, div));
    }

    FORCE_INLINE Handle_<ModelData_> NewCorrelatedBSModelData(const String_& name, const CorrelatedBSSettings_& settings) {
        return Handle_<ModelData_>(new CorrelatedBSModelData_(name, settings));
    }

    FORCE_INLINE Handle_<ModelData_> NewHybridModelData(const String_& name, const HybridSettings_& settings) {
        return Handle_<ModelData_>(new HybridModelData_(name, settings));
    }

    FORCE_INLINE Handle_<LocalVolSurfaceData_>
    NewLocalVolSurfaceData(const String_& name, const Vector_<>& spots, const Vector_<>& times, const Matrix_<>& vols) {
        return Handle_<LocalVolSurfaceData_>(new LocalVolSurfaceData_(name, spots, times, vols));
    }

    FORCE_INLINE Handle_<LocalVolSurfaceData_> NewLocalVolSurfaceDataFromIVS(
        const String_& name, const AAD::IVS_& ivs, const Vector_<>& spots, double maxSpotSpacing, const Vector_<>& times, double maxTimeSpacing) {
        return CalibrateDupireLocalVolSurface(name, ivs, spots, maxSpotSpacing, times, maxTimeSpacing);
    }

    FORCE_INLINE Handle_<HybridComponentData_> NewHybridLocalVolEquityData(const String_& name,
                                                                           const String_& index,
                                                                           const String_& currency,
                                                                           const String_& factor,
                                                                           double spot,
                                                                           double div,
                                                                           const Handle_<LocalVolSurfaceData_>& surface,
                                                                           double maxStep = 1.0 / 12.0) {
        return Handle_<HybridComponentData_>(new HybridLocalVolEquityData_(name, index, currency, factor, spot, div, surface, maxStep));
    }

    FORCE_INLINE Handle_<HybridComponentData_>
    NewHybridGSRRateData(const String_& name, const String_& factor, const Handle_<GSRCurveData_>& curve, const Handle_<GSRVolData_>& vol) {
        return Handle_<HybridComponentData_>(new HybridGSRRateData_(name, factor, curve, vol));
    }

    FORCE_INLINE Handle_<ModelData_> NewBSLocalVolModelData(const String_& name,
                                                            const String_& index,
                                                            const String_& currency,
                                                            const String_& factor,
                                                            const BSModelData_& bs,
                                                            const Handle_<LocalVolSurfaceData_>& surface,
                                                            double maxStep = 1.0 / 12.0) {
        Matrix_<> identity(1, 1, 1.0);
        HybridSettings_ settings;
        settings.domesticCurrency_ = currency;
        settings.components_ = {NewHybridLocalVolEquityData("equity", index, currency, factor, bs.spot_, bs.div_, surface, maxStep),
                                Handle_<HybridComponentData_>(new HybridDeterministicRateData_("rate", currency, bs.rate_))};
        settings.correlation_ = Handle_<HybridCorrelationData_>(new HybridConstantCorrelationData_("correlation", {factor}, identity));
        return NewHybridModelData(name, settings);
    }

    FORCE_INLINE Handle_<GSRCurveData_> NewGSRCurveData(const String_& name,
                                                        const Date_& evaluationDate,
                                                        const String_& currency,
                                                        const Vector_<Date_>& nodeDates,
                                                        const Vector_<>& discountLogDF,
                                                        const Vector_<String_>& projectionTenors,
                                                        const Matrix_<>& projectionLogDF) {
        return Handle_<GSRCurveData_>(new GSRCurveData_(name, evaluationDate, currency, nodeDates, discountLogDF, projectionTenors,
                                                       projectionLogDF));
    }

    FORCE_INLINE Handle_<GSRVolData_> NewGSRVolData(const String_& name,
                                                    const Vector_<Date_>& gKnotDates,
                                                    const Vector_<>& gValues,
                                                    const Vector_<Date_>& hKnotDates,
                                                    const Vector_<>& hValues) {
        return Handle_<GSRVolData_>(new GSRVolData_(name, gKnotDates, gValues, hKnotDates, hValues));
    }

    FORCE_INLINE Handle_<ModelData_> NewGSRModelData(const String_& name,
                                                    const Handle_<GSRCurveData_>& curve,
                                                    const Handle_<GSRVolData_>& vol) {
        return Handle_<ModelData_>(new GSRModelData_(name, curve, vol));
    }

    FORCE_INLINE Handle_<GSRCurveData_> NewGSRCurveDataFromYieldCurve(const String_& name,
                                                                       const YieldCurve_& source,
                                                                       const Date_& evaluationDate,
                                                                       const Vector_<Date_>& nodeDates,
                                                                       const Vector_<String_>& projectionTenors) {
        REQUIRE(nodeDates.size() >= 2 && nodeDates.front() == evaluationDate,
                "InvalidGSRCurve: snapshot nodes must start at the evaluation date and contain at least two dates");
        const CollateralType_ collateral(CollateralType_::Value_::OIS);
        REQUIRE(source.HasDiscount(collateral), "InvalidGSRCurve: source has no OIS discount curve");
        const auto snapshot = [&](const DiscountCurve_& curve) {
            Vector_<> values(nodeDates.size(), 0.0);
            for (size_t i = 1; i < nodeDates.size(); ++i) {
                REQUIRE(nodeDates[i] > nodeDates[i - 1], "InvalidGSRCurve: snapshot dates must be strictly increasing");
                const double discount = curve(evaluationDate, nodeDates[i]);
                REQUIRE(std::isfinite(discount) && discount > 0.0, "InvalidGSRCurve: source discount factor must be finite and positive");
                values[i] = std::log(discount);
            }
            return values;
        };
        const auto discount = snapshot(source.Discount(collateral));
        Matrix_<> projection(static_cast<int>(projectionTenors.size()), static_cast<int>(nodeDates.size()), 0.0);
        for (size_t row = 0; row < projectionTenors.size(); ++row) {
            const PeriodLength_ tenor(projectionTenors[row]);
            REQUIRE(source.HasForward(tenor), "InvalidGSRCurve: source has no requested projection tenor " + projectionTenors[row]);
            const auto values = snapshot(source.Forward(tenor, collateral));
            for (size_t col = 0; col < values.size(); ++col)
                projection(static_cast<int>(row), static_cast<int>(col)) = values[col];
        }
        return NewGSRCurveData(name, evaluationDate, source.ccy_.String(), nodeDates, discount, projectionTenors, projection);
    }

    FORCE_INLINE Handle_<HybridComponentData_> NewHybridLogDfRateData(
        const String_& name, const String_& currency, const Vector_<>& times, const Vector_<>& logDF, const String_& scheme = "LOG_LINEAR") {
        return Handle_<HybridComponentData_>(std::make_shared<HybridLogDfRateData_>(name, currency, times, logDF, scheme));
    }

    FORCE_INLINE Handle_<HybridComponentData_> NewHybridLogDfRateDataFromCurve(const String_& name,
                                                                               const DiscountCurve_& curve,
                                                                               const Date_& evaluationDate,
                                                                               const Vector_<Date_>& nodeDates,
                                                                               const String_& scheme = "LOG_LINEAR") {
        REQUIRE(nodeDates.size() >= 2 && nodeDates.front() == evaluationDate,
                "InvalidHybridCurve: first snapshot date must equal the evaluation date and at least two dates are required");
        Vector_<> times, logDF;
        times.reserve(nodeDates.size());
        logDF.reserve(nodeDates.size());
        for (size_t i = 0; i < nodeDates.size(); ++i) {
            REQUIRE(i == 0 || nodeDates[i] > nodeDates[i - 1],
                    "InvalidHybridCurve: snapshot dates must be strictly increasing at node " + String::FromInt(static_cast<int>(i)));
            times.push_back((nodeDates[i] - evaluationDate) / DAYS_PER_YEAR);
            if (i == 0) {
                logDF.push_back(0.0);
                continue;
            }
            const double discount = curve(evaluationDate, nodeDates[i]);
            REQUIRE(std::isfinite(discount) && discount > 0.0,
                    "InvalidHybridCurve: source discount factor must be finite and positive at node " + String::FromInt(static_cast<int>(i)));
            logDF.push_back(std::log(discount));
        }
        return NewHybridLogDfRateData(name, curve.ccy_.String(), times, logDF, scheme);
    }

} // namespace Dal
