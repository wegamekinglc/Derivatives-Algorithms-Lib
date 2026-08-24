//
// Created by dal-implementer on 2026/8/1.
//

#pragma once

#include <cmath>
#include <exception>
#include <utility>
#include <variant>

#include <dal/curve/ratecashflowpricing.hpp>
#include <dal/curve/tapeguard.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/curve/yczerorate.hpp>

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
#define DAL_RATE_RISK_NATIVE_AAD 1
#else
#define DAL_RATE_RISK_NATIVE_AAD 0
#endif

namespace Dal::RateCashflowPricingInternal {
#if DAL_RATE_RISK_NATIVE_AAD
    // Test-only observation seam: when non-null, every completed node-sensitivity sweep stores the
    // native tape's live node count here (measured after propagation, before the TapeGuard_ rewind).
    // An unregistered-constant AAD::Number_ passive curve still yields correct values and gradient
    // width, so only this direct count distinguishes it from a truly passive double curve.
    inline int* g_nodeSensitivityTapeSizeSink = nullptr;
#endif

    // Test instrumentation for the sweep engine shared by the single-trade and batch entry points:
    // the hoisted per-trade passive prices and per-curve preparations actually performed.
    inline int g_nodeSensitivityPassivePriceCount = 0;
    inline int g_nodeSensitivityPreparationCount = 0;

    using NodeSensitivityCurve_ = std::variant<std::monostate,
                                               const Tape::DiscountPWC_<double>*,
                                               const Tape::DiscountPWLF_<double>*,
                                               const Tape::DiscountLogDF_<double>*,
                                               const Tape::DiscountZeroRate_<double>*>;

    inline NodeSensitivityCurve_ ClassifyNodeSensitivityCurve(const DiscountCurve_& curve) {
        if (const auto* pwc = dynamic_cast<const Tape::DiscountPWC_<double>*>(&curve))
            return pwc;
        if (const auto* pwlf = dynamic_cast<const Tape::DiscountPWLF_<double>*>(&curve))
            return pwlf;
        if (const auto* logDf = dynamic_cast<const Tape::DiscountLogDF_<double>*>(&curve))
            return logDf;
        if (const auto* zero = dynamic_cast<const Tape::DiscountZeroRate_<double>*>(&curve))
            return zero;
        return std::monostate{};
    }

    struct NodeSensitivityCandidate_ {
        double pv_ = 0.0;
        Vector_<> gradient_;
    };

    // AAD family-eligibility registry: the rate families whose node-sensitivity stage is
    // open. A family enters this set only once its multi-component AAD stage is onboarded;
    // the per-trade gate additionally requires the terms alternative to match the family,
    // so terms-mismatch trades keep hitting the family gate by construction.
    inline Vector_<RateInstrumentType_> AadEnabledRateFamilies() {
        return {RateInstrumentType_::Value_::DEPOSIT, RateInstrumentType_::Value_::FRA, RateInstrumentType_::Value_::FUTURE,
                RateInstrumentType_::Value_::OIS,     RateInstrumentType_::Value_::IRS, RateInstrumentType_::Value_::BASIS_SWAP,
                RateInstrumentType_::Value_::XCCY};
    }

    inline RateTradeNodeSensitivityResult_ NodeSensitivityFailure(const String_& reason) {
        RateTradeNodeSensitivityResult_ result;
        result.reason_ = reason;
        return result;
    }

    inline RateTradeNodeSensitivityResult_ FinalizeNodeSensitivityCandidate(NodeSensitivityCandidate_ candidate, int expectedParameterCount) {
        if (expectedParameterCount < 0 || static_cast<int>(candidate.gradient_.size()) != expectedParameterCount || !std::isfinite(candidate.pv_))
            return NodeSensitivityFailure("AAD_EVALUATION_FAILED");
        for (const double value : candidate.gradient_)
            if (!std::isfinite(value))
                return NodeSensitivityFailure("AAD_EVALUATION_FAILED");
        return {true, candidate.pv_, std::move(candidate.gradient_), String_()};
    }

    template <class Runner_> RateTradeNodeSensitivityResult_ RunNodeSensitivityAADStage(int expectedParameterCount, Runner_&& runner) {
        try {
            TapeGuard_ guard(AAD::Tape());
            NodeSensitivityCandidate_ candidate = std::forward<Runner_>(runner)();
#if DAL_RATE_RISK_NATIVE_AAD
            if (g_nodeSensitivityTapeSizeSink)
                *g_nodeSensitivityTapeSizeSink = AAD::Tape()->nodes_.Size();
#endif
            return FinalizeNodeSensitivityCandidate(std::move(candidate), expectedParameterCount);
        } catch (const std::exception&) {
            return NodeSensitivityFailure("AAD_EVALUATION_FAILED");
        }
    }
} // namespace Dal::RateCashflowPricingInternal
