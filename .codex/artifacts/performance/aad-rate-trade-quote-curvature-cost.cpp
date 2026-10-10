//
// Created by Codex on 2026/10/10.
//

#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>

#include <dal-public/src/global.hpp>
#include <dal-public/src/ratecurvature.hpp>
#include <dal/curve/curveparameterization.hpp>
#include <dal/curve/rateparameterjacobian.hpp>
#include <dal/curve/ycconst.hpp>

#include "ratejacobianfixtures.hpp"
#include "ratetradecurvaturefixtures.hpp"
#include "ratexccycurvaturefixtures.hpp"

#if DAL_TRADE_CURVATURE_COST_MODE < 2
#include <dal/curve/ratetradeobjective_internal.hpp>
#endif

namespace {
    using namespace Dal;

    [[maybe_unused]] CurveCalibrationSpec_ SingleSpec() {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2025, 1, 2);
        spec.ccy_ = "USD";
        spec.curveName_ = "cost";
        spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.tolerance_ = 1e-14;
        spec.knotDates_ = {spec.today_};
        for (int year = 1; year <= 2; ++year) {
            const auto maturity = Date::AddMonths(spec.today_, 12 * year);
            spec.knotDates_.push_back(maturity);
            spec.instruments_.push_back(Handle_<YCInstrument_>(new Deposit_(spec.today_, maturity, 0.02 + 0.005 * year, DayBasis::Act365F())));
        }
        return spec;
    }

    template <class F_> void Measure(const F_& evaluate) {
        (void)evaluate();
        const auto start = std::chrono::steady_clock::now();
        double checksum = 0.0;
        for (int repeat = 0; repeat < 5; ++repeat)
            checksum += evaluate();
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << std::setprecision(17) << "seconds " << seconds << " checksum " << checksum << '\n';
    }

#if DAL_TRADE_CURVATURE_COST_MODE < 2
    struct Materials_ {
        RateCalibrationSnapshot_ calibration_;
        RatePricingMarket_ market_;
        Vector_<RateTradeDefinition_> trades_;
        RateTradeQuoteCurvatureSettings_ settings_;
    };

    Materials_ SingleMaterials() {
        const auto spec = SingleSpec();
        auto solved = CalibrateYieldCurve(spec, CurveCalibrationOptions_{});
        RatePricingMarket_ market;
        market.valuationTime_ = DateTime_(spec.today_);
        market.resultCurrency_ = Ccy_(spec.ccy_);
        market.curveComponents_[spec.curveName_] = Handle_<DiscountCurve_>(std::move(solved.curve_));
        return {NewRateCalibration(spec),
                std::move(market),
                RateTradeCurvatureFixtures::SingleFamilyTrades(spec),
                {{1.0, -0.3, 0.2, 0.4, -0.5, 0.7}, {}}};
    }

    RateTradeDefinition_ XccyTrade(const CrossCurrencyCalibrationSpec_& spec) {
        auto trade = RateJacobianFixtures::Xccy();
        trade.tradeDate_ = spec.today_;
        trade.startDate_ = Date::AddMonths(spec.today_, 1);
        trade.maturityDate_ = Date::AddMonths(trade.startDate_, 31);
        auto& terms = std::get<XccyTradeTerms_>(trade.terms_);
        terms.config_ = spec.instruments_.front()->Config();
        terms.positionCount_ = 0.01;
        return trade;
    }

    void BindFixedRoots(RatePricingMarket_* market) {
        const auto owner = market->xccyMarket_;
        const auto bind = [&](const DiscountCurve_& curve) {
            const String_ key = "fixed:xccy:" + String_(std::to_string(market->curveComponents_.size()));
            market->curveComponents_[key] = Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(owner, &curve));
        };
        for (const auto* block : {&owner->DomesticBlock(), &owner->ForeignBlock()}) {
            for (const auto& entry : block->DiscountCurves())
                bind(*entry.second);
            for (const auto& entry : block->ForwardCurves())
                bind(*entry.second);
        }
    }

    Materials_ StagedMaterials() {
        const auto spec = RateXccyCurvatureFixtures::StagedSpec();
        const auto solved = CalibrateCrossCurrencyMarket(spec, CrossCurrencyCalibrationOptions_{});
        RatePricingMarket_ market;
        market.valuationTime_ = spec.valuationTime_;
        market.resultCurrency_ = spec.basisPair_.domestic_;
        market.fixings_ = solved.market_.Fixings();
        market.xccyMarket_ = std::make_shared<CrossCurrencyMarket_>(solved.market_);
        market.curveComponents_["basis:xccy_basis_USD"] = solved.basisCurves_.at(spec.basisPair_);
        BindFixedRoots(&market);
        return {NewRateCalibration(spec), std::move(market), {XccyTrade(spec)}, {}};
    }

    void BindCurrency(const JointCurrencyCurveSpec_& spec, const Handle_<CurveBlock_>& block, const String_& group, RatePricingMarket_* market) {
        for (size_t i = 0; i < spec.curves_.size(); ++i) {
            const auto& declaration = spec.curves_[i];
            const auto& curve = declaration.calibrateDiscountCurve_ ? block->Discount(declaration.targetCollateral_)
                                                                    : block->Forward(declaration.targetTenor_, declaration.targetCollateral_);
            const String_ key = group + ":" + String_(std::to_string(i)) + ":" + declaration.curveName_;
            market->curveComponents_[key] = Handle_<DiscountCurve_>(std::shared_ptr<const DiscountCurve_>(block, &curve));
        }
    }

    Materials_ JointMaterials() {
        const auto spec = RateXccyCurvatureFixtures::JointSpec(true);
        const auto solved = CalibrateJointXccyMarket(spec, JointXccyCalibrationOptions_{});
        RatePricingMarket_ market;
        market.valuationTime_ = spec.valuationTime_;
        market.resultCurrency_ = spec.pair_.domestic_;
        market.fixings_ = solved.fixings_;
        auto xccy = std::make_shared<CrossCurrencyMarket_>(solved.domesticCurveBlock_, solved.foreignCurveBlock_, spec.fxSpot_, spec.valuationTime_,
                                                           spec.collateralCurrency_, solved.fixings_);
        xccy->SetBasisCurve(solved.basisCurve_);
        market.xccyMarket_ = xccy;
        BindCurrency(spec.domestic_, solved.domesticCurveBlock_, "domestic", &market);
        BindCurrency(spec.foreign_, solved.foreignCurveBlock_, "foreign", &market);
        market.curveComponents_["basis:" + spec.basis_.curveName_] = solved.basisCurve_;
        return {NewRateCalibration(spec), std::move(market), {XccyTrade(RateXccyCurvatureFixtures::StagedSpec())}, {}};
    }
#endif

    [[maybe_unused]] double Checksum(const RateQuoteCurvatureResult_& result) {
        return result.Value() + std::accumulate(result.Gradient().begin(), result.Gradient().end(), 0.0) +
               std::accumulate(result.HessianProducts().begin(), result.HessianProducts().end(), 0.0);
    }
} // namespace

namespace {
#if DAL_TRADE_CURVATURE_COST_MODE == 3
    void RunParameterJacobian() {
        const auto market = RateJacobianFixtures::ComponentMarket();
        const Dal::Vector_<Dal::RateCurveParameterCoordinate_> axis = {{"C", 0}, {"D", 0}, {"A", 0}, {"E", 0}, {"B", 0}};
        Dal::Vector_<Dal::RateTradeDefinition_> trades;
        const Dal::Vector_<Dal::RateTradeDefinition_> templates = {RateJacobianFixtures::Irs(), RateJacobianFixtures::Deposit("deposit", "D"),
                                                                   RateJacobianFixtures::Fra()};
        for (size_t i = 0; i < 32; ++i)
            trades.push_back(templates[i % templates.size()]);
        Measure([&] {
            const auto result = Dal::RateTradeParameterJacobian(trades, market, axis);
            return std::accumulate(result.jacobian_.begin(), result.jacobian_.end(), 0.0);
        });
    }
#else
    Dal::AAD::BumpOverAADRequest_ CurvatureRequest(const Dal::RateCalibrationSnapshot_& calibration) {
        Dal::AAD::BumpOverAADRequest_ request;
        request.directions_ = Dal::Matrix_<>(1, static_cast<int>(calibration.Point().size()), 0.6);
        request.directions_(0, 0) = -0.8;
        request.steps_ = {2e-4};
        return request;
    }
#if DAL_TRADE_CURVATURE_COST_MODE == 2
    void RunGenericCurvature() {
        const auto calibration = Dal::NewRateCalibration(SingleSpec());
        const auto request = CurvatureRequest(calibration);
        Measure([&] {
            return Checksum(Dal::EvaluateRateQuoteCurvature(
                [](auto*, const auto& inputs) -> Dal::AAD::Number_ {
                    Dal::AAD::Number_ value(0.0);
                    for (const auto& input : inputs)
                        value += input * input;
                    return value;
                },
                calibration, request));
        });
    }
#else
    void RunFinancialCurvature(const std::string& kind) {
        const auto materials = kind == "staged" ? StagedMaterials() : kind == "joint" ? JointMaterials() : SingleMaterials();
        const auto& calibration = materials.calibration_;
        const auto request = CurvatureRequest(calibration);
        Dal::Vector_<Dal::RateCurveParameterCoordinate_> axis;
        for (const auto& coordinate : calibration.Provenance().Axis().parameters_)
            axis.push_back(
                {calibration.Provenance().ComponentKeyByParameterBlock().at(coordinate.blockKey_), static_cast<size_t>(coordinate.blockOrdinal_)});
        Measure([&] {
#if DAL_TRADE_CURVATURE_COST_MODE == 0
            return Checksum(Dal::EvaluateRateTradeQuoteCurvature(materials.trades_, calibration, request, materials.settings_).Curvature());
#else
            const auto captured = Dal::RateCashflowPricingInternal::NewRateTradeObjective(
                materials.trades_, materials.market_, axis, {materials.settings_.weights_, materials.settings_.fixings_, calibration.Point().size()});
            return Checksum(Dal::EvaluateRateQuoteCurvature(captured.objective_, calibration, request));
#endif
        });
    }
#endif
#endif
} // namespace

int main(int argc, char** argv) {
    Dal::InitGlobalData(1);
    (void)argc;
    (void)argv;
#if DAL_TRADE_CURVATURE_COST_MODE == 3
    RunParameterJacobian();
#elif DAL_TRADE_CURVATURE_COST_MODE == 2
    RunGenericCurvature();
#else
    RunFinancialCurvature(argc > 1 ? argv[1] : "single");
#endif
    return 0;
}
