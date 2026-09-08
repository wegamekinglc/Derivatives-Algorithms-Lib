// Public-only reference inputs mirrored by Python's joint_inputs fixture.
#pragma once

#include <dal-public/src/curveinstrument.hpp>
#include <dal-public/src/curvepricing.hpp>
#include <dal-public/src/curveprotocol.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace JointQuoteRiskPublicFixture {
    inline std::vector<std::array<std::string, 5>> ParityRows() {
        std::ifstream input(std::filesystem::path(__FILE__).parent_path() / "data/generic_joint_quote_risk_v2.csv");
        REQUIRE(input.is_open(), "Cannot read generic joint parity reference");
        std::string line;
        REQUIRE(static_cast<bool>(std::getline(input, line)), "Missing generic joint parity header");
        std::vector<std::array<std::string, 5>> result;
        while (std::getline(input, line)) {
            std::istringstream fields(line);
            std::array<std::string, 5> row;
            for (auto& field : row)
                REQUIRE(static_cast<bool>(std::getline(fields, field, ',')), "Malformed generic joint parity row");
            result.push_back(row);
        }
        REQUIRE(result.size() == 5, "Incomplete generic joint parity reference");
        return result;
    }

    inline Dal::JointMultiCurveCalibrationSpec_ Spec(bool layered = false) {
        using namespace Dal;
        JointMultiCurveCalibrationSpec_ spec;
        spec.today_ = Date_(2025, 1, 2);
        spec.ccy_ = "USD";
        spec.tolerance_ = 1.0e-11;
        spec.initialGuess_ = 0.025;
        spec.maxEvaluations_ = 1000;
        spec.maxRestarts_ = 100;
        const auto basis = DayBasis_New("ACT_365F");
        const auto fixed = RateLegConvention_New(PeriodLength_New("6M"), basis);
        const auto floating = RateLegConvention_New(PeriodLength_New("3M"), basis);
        for (int block = 0; block < 2; ++block) {
            JointCurveDeclaration_ declaration;
            declaration.curveName_ = "repeated_name";
            declaration.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
            declaration.calibrateDiscountCurve_ = block == 0;
            declaration.targetCollateral_ = CollateralType_OIS();
            declaration.targetTenor_ = block == 0 ? PeriodLength_() : PeriodLength_New("3M");
            declaration.baseLayeredOverDiscount_ = layered && block == 1;
            const auto index = RateIndexConvention_New(PeriodLength_New("3M"), basis, CollateralType_OIS(), block != 0);
            for (int ordinal = 0; ordinal < (block == 0 ? 3 : 2); ++ordinal) {
                const Date_ maturity(2026 + ordinal, 1, 2);
                declaration.knotDates_.push_back(Date_(2025 + ordinal, 7, 2));
                const double quote = (block == 0 ? 0.02 : 0.035) + 0.001 * ordinal;
                declaration.instruments_.push_back(block == 0 ? DepositNew(spec.today_, spec.today_, maturity, quote, index)
                                                              : SwapNew(spec.today_, spec.today_, maturity, quote, fixed, index, floating));
            }
            spec.curves_.push_back(declaration);
        }
        return spec;
    }

    inline Dal::RateTradeDefinition_ Trade() {
        using namespace Dal;
        RateTradeDefinition_ trade;
        trade.instrumentId_ = "joint-irs";
        trade.instrumentType_ = RateInstrumentType_::Value_::IRS;
        trade.tradeDate_ = Date_(2025, 1, 2);
        trade.startDate_ = trade.tradeDate_;
        trade.maturityDate_ = Date_(2027, 1, 2);
        trade.currencyOrPair_ = Ccy_("USD");
        IrsTradeTerms_ terms;
        terms.value_.notional_ = 1000000.0;
        terms.value_.contractRate_ = 0.03;
        terms.value_.payFixed_ = true;
        const auto basis = DayBasis_New("ACT_365F");
        terms.value_.fixedLeg_ = RateLegConvention_New(PeriodLength_New("6M"), basis);
        terms.value_.floatLeg_ = RateLegConvention_New(PeriodLength_New("3M"), basis);
        terms.value_.floatIndex_ = RateIndexConvention_New(PeriodLength_New("3M"), basis, CollateralType_OIS(), true);
        terms.value_.fixingIdentity_ = {"USD-3M", 11, 0};
        terms.value_.discountComponentKey_ = "discount";
        terms.value_.forecastComponentKey_ = "forward";
        trade.terms_ = terms;
        return trade;
    }

    inline Dal::RatePricingMarket_ Market(const Dal::JointMultiCurveCalibrationResult_& result) {
        Dal::RatePricingMarket_ market;
        market.valuationTime_ = Dal::DateTime_(Dal::Date_(2025, 1, 2));
        market.resultCurrency_ = Dal::Ccy_("USD");
        market.curveComponents_["discount"] = result.discountCurves_.begin()->second;
        market.curveComponents_["forward"] = result.forwardCurves_.begin()->second;
        market.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_());
        return market;
    }

    inline Dal::RateQuoteRiskProvenanceConfig_ Config() { return {"python-generic-joint", {{"curve:0", "discount"}, {"curve:1", "forward"}}}; }
} // namespace JointQuoteRiskPublicFixture
