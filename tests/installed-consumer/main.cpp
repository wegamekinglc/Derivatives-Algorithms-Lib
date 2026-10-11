//
// Created by Codex on 2026/10/10.
//

#include <cmath>
#include <iostream>
#include <numeric>

#include <dal-public/src/calendar.hpp>
#include <dal-public/src/dupirecurvature.hpp>
#include <dal-public/src/dupirerisk.hpp>
#include <dal-public/src/europeanpderisk.hpp>
#include <dal-public/src/global.hpp>
#include <dal-public/src/interp.hpp>
#include <dal-public/src/lsmccurvature.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/montecarlocurvature.hpp>
#include <dal-public/src/ratecurvature.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/storage.hpp>
#include <dal-public/src/value.hpp>
#include <dal/math/aad/forwardoverreverse.hpp>
#include <dal/model/dupirerisk.hpp>
#include <dal/model/ivs.hpp>
#include <dal/script/lsmccurvature.hpp>
#include <dal/script/simulation.hpp>
#include <dal/utilities/numerics.hpp>

namespace {
    bool CheckOwningEuropeanPdeBoundary() {
        Dal::EuropeanPdeRiskRequest_ request;
        request.settings_.gridPoints_ = 9;
        request.settings_.ordinarySteps_ = 8;
        request.numericPayloadBudgetBytes_ = 688;
        const auto resolved = Dal::ResolveEuropeanPdeSettings(request.settings_);
        if (resolved.spotIndex_ != 2 || request.settings_.spotIndex_)
            return false;
        const auto result = Dal::EvaluateEuropeanPdeRisk(request);
        return std::abs(result.prices_[0] - 4.153690693968586) < 1e-10 && std::abs(result.jacobian_(1, 2) - 0.8605082862555484) < 1e-9 &&
               result.transposeBackwardErrors_.Rows() == 10 && result.execution_.numericPayloadBytes_ == 688 &&
               result.method_ == "NativeAADFixedGridEuropeanTheta";
    }

    bool CheckOwningLsmcBoundary() {
        Dal::Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Dal::Date_(2026, 10, 10);
        auto simulation = Dal::DefaultRiskMonteCarloSettings();
        simulation.lsmcTrainingPaths_ = 32;
        const auto product =
            Dal::NewScriptProduct("installed-lsmc", {Dal::Cell_("K"), Dal::Cell_(Dal::Date_(2027, 4, 10))}, {"45", "EXERCISE 2 * K"});
        const auto plan = Dal::PlanBlackScholesLsmc(product, simulation, valuation);
        Dal::AAD::BumpOverAADRequest_ bumps;
        bumps.directions_ = Dal::Matrix_<>(1, 5, 0.0);
        bumps.directions_(0, 4) = 1.0;
        bumps.steps_ = {0.1};
        const auto result = Dal::ValueByBlackScholesLsmcWithCurvature(plan, {100, 0.2, 0, 0, 50}, 3, bumps);
        return std::abs(result.Curvature().Value() - 100) < 1e-10 && std::abs(result.Curvature().Gradient()[4] - 2) < 1e-10 &&
               std::abs(result.Curvature().HessianProducts()(0, 4)) < 1e-9 && result.Curvature().BasePolicy().size() == 1 &&
               result.Plan().Contract().Name() == "installed-lsmc" && result.Plan().ScriptConstants()[0] == 45;
    }

    bool CheckSegmentedMonteCarloBoundary() {
        Dal::Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Dal::Date_(2026, 1, 2);
        const auto product = Dal::NewScriptProduct("installed-segmented", {Dal::Cell_("SCALE"), Dal::Cell_(Dal::Date_(2026, 1, 2))},
                                                   {"2", "pay PAYS SCALE * FIX(EQ[INSTALLED]) ^ 2"});
        const auto plan = Dal::PlanBlackScholesMonteCarlo(product, valuation);
        const Dal::Vector_<> point{10, 0.2, 0.03, 0.01, 2};
        const auto mean = Dal::ValueByBlackScholesSegmentedMonteCarlo(plan, point, 1);
        Dal::AAD::BumpOverAADRequest_ bumps;
        bumps.directions_ = Dal::Matrix_<>(1, 5, 0.0);
        bumps.directions_(0, 0) = 1.0;
        bumps.steps_ = {0.25};
        const auto result = Dal::ValueByBlackScholesMonteCarloWithCurvature(plan, point, 1, bumps);
        return std::abs(mean.Mean().MeanValue() - 200) < 1e-10 && std::abs(mean.Mean().MeanGradient()[0] - 40) < 1e-10 &&
               std::abs(result.Curvature().HessianProducts()(0, 0) - 4) < 1e-9 &&
               result.Plan().Valuation().evaluationDate_ == valuation.evaluationDate_;
    }

    bool CheckSmoothCurvature() {
        Dal::AAD::ForwardOverReverseRequest_ request;
        request.directions_ = Dal::Matrix_<>(1, 1, 1.0);
        const auto result = Dal::AAD::EvaluateForwardOverReverse(
            [](Dal::AAD::RecordingScope_*, const Dal::Vector_<Dal::AAD::ForwardOverReverseNumber_>& x) { return x[0] * x[0] * x[0] * x[0]; }, {2.0},
            request);
        return result.Value() == 16.0 && result.Gradient()[0] == 32.0 && result.HessianProducts()(0, 0) == 48.0 &&
               result.DirectionalDerivatives()[0] == 32.0 && result.Execution().reverseSweeps_ == 2;
    }

    bool CheckLsmcCurvature() {
        Dal::Script::MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = true;
        simulation.smooth_ = 2.0;
        simulation.lsmcTrainingPaths_ = 128;
        Dal::Script::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Dal::Date_(2026, 10, 10);
        const Dal::Script::ScriptProductData_ product("", {Dal::Cell_(Dal::Date_(2027, 4, 10)), Dal::Cell_(Dal::Date_(2027, 10, 10))},
                                                      {"EXERCISE 100 - spot()", "EXERCISE 100 - spot()"});
        Dal::AAD::BlackScholes_<> model(100.0, 0.2, 0.05, 0.0);
        const auto prepared =
            std::make_shared<const Dal::Script::PreparedScript_>(Dal::Script::PrepareScript(product, &model, valuation, simulation));
        Dal::AAD::BumpOverAADRequest_ bumps;
        bumps.directions_ = Dal::Matrix_<>(2, 4, 0.0);
        bumps.directions_(0, 0) = 1.0;
        bumps.directions_(1, 0) = -1.0;
        bumps.steps_ = {0.1, 0.1};
        const auto result = Dal::Script::EvaluateBlackScholesLsmcCurvature(prepared, {100.0, 0.2, 0.05, 0.0}, 128, bumps);
        return result.BasePolicy().size() == 2 && result.Gradient().size() == 4 && result.Execution().gradientEvaluations_ == 5 &&
               std::isfinite(result.HessianProducts()(0, 0)) && result.HessianProducts()(0, 0) == -result.HessianProducts()(1, 0) &&
               result.Execution().method_ == "BumpOverFrozenNativeLsmcAAD";
    }

    class ConsumerIVS_ final : public Dal::AAD::IVS_ {
    public:
        explicit ConsumerIVS_(double rate = 0.0, double dividend = 0.0) : IVS_(100.0, rate, dividend) {}
        [[nodiscard]] double ImpliedVol(double, double) const override { return 0.2; }
    };

    bool CheckHybridPullback(const Dal::DupireCalibrationSnapshot_& calibration) {
        Dal::HybridSettings_ settings;
        settings.domesticCurrency_ = "USD";
        settings.components_ = {Dal::NewHybridLocalVolEquityData("equity", "EQ[INSTALLED]", "USD", "W_EQ", calibration.Spot(),
                                                                 calibration.DividendYield(), calibration.Surface(), 0.25),
                                Dal::NewHybridDeterministicRateData("rate", "USD", calibration.Rate())};
        settings.correlation_ = Dal::NewHybridConstantCorrelationData("correlation", {"W_EQ"}, Dal::Matrix_<>(1, 1, 1.0));
        const auto model = Dal::NewHybridModelData("installed-hybrid", settings);
        const auto product = Dal::NewScriptProduct("installed-quotes", {Dal::Cell_(Dal::Date_(2027, 9, 12))},
                                                   {"pay PAYS FIX(EQ[INSTALLED]) * FIX(EQ[INSTALLED]) / 100"});
        Dal::ScriptValuationSettings_ valuation;
        valuation.evaluationDate_ = Dal::Date_(2026, 9, 12);
        const auto source = Dal::ValueByMonteCarloWithRisk(product, model, 257, {}, valuation);
        const auto convenience = Dal::NewDupireModelData("installed-hybrid", calibration, "EQ[INSTALLED]", "USD", "W_EQ", 0.25);
        const auto comparable = Dal::ValueByMonteCarloWithRisk(product, convenience, 257, {}, valuation);
        if (comparable.Values() != source.Values() || comparable.Jacobian().Rows() != source.Jacobian().Rows() ||
            comparable.Jacobian().Cols() != source.Jacobian().Cols() ||
            !std::equal(comparable.Jacobian().begin(), comparable.Jacobian().end(), source.Jacobian().begin()))
            return false;
        const auto extracted = Dal::ExtractDupireParameterAdjoints(source, calibration, "equity");
        const auto reference = Dal::PullbackDupireCalibration(calibration, extracted);
        const auto result = Dal::PullbackDupireScriptRisk(source, calibration, "equity");
        return extracted.adjoints_(0, 0) == source.Jacobian()(0, 2) && result.Valuation().Values()[0] == source.Values()[0] &&
               result.Method() == "NativeAADThenNativeAADCalibrationVJP" &&
               std::equal(reference.TotalAdjoints().begin(), reference.TotalAdjoints().end(), result.QuoteRisk().TotalAdjoints().begin());
    }

    bool CheckFlatConvenience(const Dal::DupireRiskInputs_& inputs) {
        const Dal::BSModelData_ model("flat", 100.0, 0.2, 0.05, 0.02);
        const auto actual = Dal::CalibrateDupireWithRisk(model, inputs);
        const auto expected = Dal::CalibrateDupireWithRisk(ConsumerIVS_(model.rate_, model.div_), inputs);
        return actual.Matches(expected) && actual.Rate() == 0.05 && actual.DividendYield() == 0.02;
    }

    bool CheckQuoteCurvature(const Dal::DupireCalibrationSnapshot_& calibration) {
        const auto model = Dal::NewDupireModelData("installed-curvature", calibration, "EQ[INSTALLED]", "USD", "W_EQ", 0.25);
        const auto product =
            Dal::NewScriptProduct("installed-direct", {Dal::Cell_("QUOTE"), Dal::Cell_(Dal::Date_(2027, 9, 12))}, {"0", "pay PAYS QUOTE * QUOTE"});
        Dal::DupireScriptCurvatureRequest_ request;
        request.risk_.numPaths_ = 17;
        request.risk_.directBindings_ = {{0, "quote:0"}};
        request.risk_.valuation_.evaluationDate_ = Dal::Date_(2026, 9, 12);
        request.risk_.simulation_.compiled_ = true;
        request.bumps_.directions_ = Dal::Matrix_<>(1, 4, 0.0);
        request.bumps_.directions_(0, 0) = 1.0;
        request.bumps_.steps_ = {1e-4};
        const auto result = Dal::ValueByMonteCarloWithDupireCurvature(Dal::PlanDupireScriptCurvature(product, model, calibration, "equity", request));
        return result.InputAxis().size() == 4 && result.Execution().quoteGradientEvaluations_ == 3 &&
               std::abs(result.HessianProducts()(0, 0) - 2.0) < 1e-10;
    }

    bool CheckRateCurvature() {
        Dal::CurveCalibrationSpec_ spec;
        spec.today_ = Dal::Date_(2025, 1, 2);
        spec.ccy_ = "USD";
        spec.curveName_ = "installed-rate";
        spec.tolerance_ = 1e-14;
        spec.parameterization_ = Dal::CurveParameterization_::Value_::LOG_DISCOUNT;
        spec.knotDates_ = {spec.today_, Dal::Date::AddMonths(spec.today_, 12)};
        spec.instruments_ = {
            Dal::Handle_<Dal::YCInstrument_>(new Dal::Deposit_(spec.today_, spec.knotDates_.back(), 0.03, Dal::DayBasis::Act365F()))};
        const auto calibration = Dal::NewRateCalibration(spec);
        Dal::AAD::BumpOverAADRequest_ request;
        request.directions_ = Dal::Matrix_<>(1, 1, 1.0);
        request.steps_ = {1e-4};
        const auto result =
            Dal::EvaluateRateQuoteCurvature([](auto*, const auto& x) -> Dal::AAD::Number_ { return x[1] * x[1]; }, calibration, request);
        if (result.Point().size() != 1 || result.Execution().calibrations_ != 3 || std::abs(result.HessianProducts()(0, 0) - 2.0) >= 1e-10)
            return false;
        Dal::RateTradeDefinition_ trade;
        trade.instrumentId_ = "installed-deposit";
        trade.instrumentType_ = Dal::RateInstrumentType_::Value_::DEPOSIT;
        trade.tradeDate_ = spec.today_;
        trade.startDate_ = spec.today_;
        trade.maturityDate_ = spec.knotDates_.back();
        trade.currencyOrPair_ = Dal::Ccy_(spec.ccy_);
        Dal::DepositTradeTerms_ terms;
        terms.notional_ = 1.0;
        terms.contractRate_ = 0.02;
        terms.discountComponentKey_ = spec.curveName_;
        terms.index_ = static_cast<const Dal::Deposit_&>(*spec.instruments_.front()).FloatConvention();
        trade.terms_ = terms;
        const auto portfolio = Dal::EvaluateRateTradeQuoteCurvature({trade}, calibration, request);
        const double accrual = terms.index_.dayBasis_(trade.startDate_, trade.maturityDate_, nullptr);
        const double payoff = 1.0 + terms.contractRate_ * accrual, denominator = 1.0 + 0.03 * accrual;
        const auto& curvature = portfolio.Curvature();
        const bool accepted = portfolio.Currency() == Dal::Ccy_("USD") && curvature.Execution().objectiveReverseSweeps_ == 3 &&
                              std::abs(curvature.Value() - (payoff / denominator - 1.0)) < 1e-10 &&
                              std::abs(curvature.Gradient()[0] + payoff * accrual / (denominator * denominator)) < 1e-9 &&
                              std::abs(curvature.HessianProducts()(0, 0) - 2.0 * payoff * accrual * accrual / std::pow(denominator, 3)) < 1e-7;
        if (!accepted)
            std::cerr << "Installed rate-trade curvature: " << curvature.Value() << ", " << curvature.Gradient()[0] << ", "
                      << curvature.HessianProducts()(0, 0) << "; accrual=" << accrual << '\n';
        return accepted;
    }

    bool CheckXccyFactoryExports() {
        bool staged = false, joint = false;
        try {
            (void)Dal::NewRateCalibration(Dal::CrossCurrencyCalibrationSpec_());
        } catch (const Dal::Exception_&) {
            staged = true;
        }
        try {
            (void)Dal::NewRateCalibration(Dal::JointXccyCalibrationSpec_());
        } catch (const Dal::Exception_&) {
            joint = true;
        }
        return staged && joint;
    }
} // namespace

int main() {
    if (!CheckOwningEuropeanPdeBoundary())
        return 17;
    if (!CheckSmoothCurvature())
        return 1;
    Dal::InitGlobalData(1);
    const auto product = Dal::NewScriptProduct("installed-risk", {Dal::Cell_(Dal::Date_(2026, 9, 22))}, {"pay PAYS SPOT()"});
    const auto model = Dal::NewBSModelData("installed-model", 100.0, 0.0, 0.0, 0.0);
    Dal::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = Dal::Date_(2026, 9, 12);
    Dal::Script::RiskRequest_ request;
    request.inputs_ = Dal::Vector_<Dal::String_>{"model:0"};
    const auto risk = Dal::ValueByMonteCarloWithRisk(product, model, 1, request, valuation);
    const auto reference = Dal::ValueByMonteCarlo(product, model, 1, valuation, Dal::DefaultRiskMonteCarloSettings());
    if (risk.Values()[0] != reference.at("PV") || risk.Jacobian()(0, 0) != reference.at("d_spot") || std::abs(risk.Values()[0] - 100.0) > 1e-10 ||
        std::abs(risk.Jacobian()(0, 0) - 1.0) > 1e-10 || risk.InputAxis()[0].id_ != "model:0")
        return 6;
    const ConsumerIVS_ ivs;
    const Dal::DupireRiskInputs_ inputs{{90.0, 110.0}, {0.5, 1.0}, Dal::Matrix_<>(2, 2, 0.0), {80.0, 100.0, 120.0}, 10.0, {0.5, 1.0}, 0.5};
    const auto calibration = Dal::CalibrateDupireWithRisk(ivs, inputs);
    const auto& vols = calibration.Surface()->vols_;
    const auto quoteRisk =
        Dal::PullbackDupireCalibration(calibration, Dal::DupireParameterAdjoints_{calibration, Dal::Matrix_<>(vols.Rows(), vols.Cols(), 1.0)});
    const double nodes = static_cast<double>(vols.Rows() * vols.Cols());
    const double parallelQuoteRisk = std::accumulate(quoteRisk.TotalAdjoints().begin(), quoteRisk.TotalAdjoints().end(), 0.0);
    if (std::abs(parallelQuoteRisk - nodes) > 3e-5 * nodes || quoteRisk.Unit() != "decimal-vol" || quoteRisk.TotalAdjoints().Rows() != 2 ||
        quoteRisk.TotalAdjoints().Cols() != 2)
        return 7;
    if (!CheckHybridPullback(calibration))
        return 8;
    if (!CheckFlatConvenience(inputs))
        return 9;
    if (!CheckQuoteCurvature(calibration))
        return 11;
    if (!CheckRateCurvature())
        return 12;
    if (!CheckXccyFactoryExports())
        return 13;
    if (!CheckLsmcCurvature())
        return 14;
    if (!CheckSegmentedMonteCarloBoundary())
        return 15;
    if (!CheckOwningLsmcBoundary())
        return 16;
    const auto merton = Dal::NewMertonIVS(100.0, 0.2, 0.08, -0.1, 0.15);
    const Dal::AAD::MertonIVS_ mertonReference(100.0, 0.2, 0.08, -0.1, 0.15);
    if (merton.ImpliedVol(105.0, 0.4) != mertonReference.ImpliedVol(105.0, 0.4))
        return 10;
    const Dal::Date_ start(2026, 1, 1);
    if (start.AddDays(1) - start != 1)
        return 1;

    const Dal::Vector_<> x{0.0, 1.0};
    const Dal::Vector_<> y{1.0, 3.0};
    if (Dal::Accumulate(y) != 4.0 || Dal::InnerProduct(x, y) != 3.0)
        return 5;
    const auto interp = Dal::Interp1NewLinear("installed-consumer", x, y);
    if (interp.IsEmpty() || Dal::Interp1Get(interp, {0.5})[0] != 2.0)
        return 2;

    const Dal::Holidays_ holidays("");
    if (Dal::NextBusinessDay(holidays, Dal::Date_(2026, 1, 31)) != Dal::Date_(2026, 2, 2))
        return 3;

    const auto payload = Dal::WriteObjectJson(*interp);
    const auto restored = Dal::handle_cast<Dal::Interp1_>(Dal::ReadObjectJson(payload.data(), payload.size()));
    return restored.IsEmpty() || Dal::Interp1Get(restored, {0.5})[0] != 2.0 ? 4 : 0;
}
