//
// Created by Codex on 2026/10/10.
//

#include <cmath>
#include <numeric>

#include <dal-public/src/calendar.hpp>
#include <dal-public/src/dupirecurvature.hpp>
#include <dal-public/src/dupirerisk.hpp>
#include <dal-public/src/global.hpp>
#include <dal-public/src/interp.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/ratecurvature.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/storage.hpp>
#include <dal-public/src/value.hpp>
#include <dal/model/dupirerisk.hpp>
#include <dal/model/ivs.hpp>
#include <dal/utilities/numerics.hpp>

namespace {
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
        return result.Point().size() == 1 && result.Execution().calibrations_ == 3 && std::abs(result.HessianProducts()(0, 0) - 2.0) < 1e-10;
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
