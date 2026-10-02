//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <dal/curve/quoteriskprovenance.hpp>
#include <dal/model/gsrdata.hpp>

namespace Dal {
    class GSRCurveQuoteRisk_ {
        Handle_<GSRCurveData_> snapshot_;
        Vector_<String_> quoteNames_, quoteUnits_;
        Matrix_<> logDFQuoteJacobian_;

        GSRCurveQuoteRisk_(const Handle_<GSRCurveData_>& snapshot,
                           const Vector_<String_>& names,
                           const Vector_<String_>& units,
                           const Matrix_<>& jacobian)
            : snapshot_(snapshot), quoteNames_(names), quoteUnits_(units), logDFQuoteJacobian_(jacobian) {}
        friend GSRCurveQuoteRisk_ BuildGSRCurveQuoteRisk(
            const GSRCurveData_&, const RatePricingMarket_&, const RateQuoteRiskProvenance_&, const String_&, const Vector_<String_>&);

    public:
        const GSRCurveData_& Snapshot() const { return *snapshot_; }
        const Vector_<String_>& QuoteNames() const { return quoteNames_; }
        const Vector_<String_>& QuoteUnits() const { return quoteUnits_; }
        const Matrix_<>& LogDFQuoteJacobian() const { return logDFQuoteJacobian_; }
        Handle_<GSRCurveData_> Shifted(size_t quote, double bump) const;
    };

    GSRCurveQuoteRisk_ BuildGSRCurveQuoteRisk(const GSRCurveData_& snapshot,
                                              const RatePricingMarket_& market,
                                              const RateQuoteRiskProvenance_& provenance,
                                              const String_& discountComponent,
                                              const Vector_<String_>& projectionComponents = {});
} // namespace Dal
