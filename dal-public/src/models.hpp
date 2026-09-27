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
#include <dal/platform/consts.hpp>

namespace Dal {
    FORCE_INLINE Handle_<ModelData_> NewBSModelData(const String_& name, double spot, double vol, double rate, double div) {
        return Handle_<ModelData_>(new BSModelData_(name, spot, vol, rate, div));
    }

    FORCE_INLINE Handle_<ModelData_> NewCorrelatedBSModelData(const String_& name, const CorrelatedBSSettings_& settings) {
        return Handle_<ModelData_>(new CorrelatedBSModelData_(name, settings));
    }

    FORCE_INLINE Handle_<ModelData_> NewHybridModelData(const String_& name, const HybridSettings_& settings) {
        return Handle_<ModelData_>(new HybridModelData_(name, settings));
    }

    FORCE_INLINE Handle_<HybridComponentData_> NewHybridLogDfRateData(
        const String_& name, const String_& currency, const Vector_<>& times, const Vector_<>& logDF, const String_& scheme = "LOG_LINEAR") {
        return Handle_<HybridComponentData_>(new HybridLogDfRateData_(name, currency, times, logDF, scheme));
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

    FORCE_INLINE Handle_<ModelData_> NewDupireModelData(
        const String_& name, double spot, double rate, double repo, const Vector_<>& spots, const Vector_<>& times, const Matrix_<>& vols) {
        return Handle_<ModelData_>(new DupireModelData_(name, spot, rate, repo, spots, times, vols));
    }
} // namespace Dal
