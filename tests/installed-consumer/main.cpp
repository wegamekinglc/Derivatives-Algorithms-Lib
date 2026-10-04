#include <cmath>
#include <numeric>

#include <dal-public/src/calendar.hpp>
#include <dal-public/src/dupirerisk.hpp>
#include <dal-public/src/global.hpp>
#include <dal-public/src/interp.hpp>
#include <dal-public/src/models.hpp>
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
