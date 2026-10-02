//
// Created by Codex on 2026/10/2.
//

#include <gtest/gtest.h>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>
#include <dal/platform/initall.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

TEST(PublicGSRSLVTest, TestFactoriesArchiveAndScriptAdjoints) {
    RegisterAll_::Init();
    const Date_ today(2026, 10, 2);
    const auto curve = NewGSRCurveData("curve", today, "USD", {today, today.AddDays(1095)}, {0.0, -0.09}, {}, Matrix_<>(0, 0));
    MultiFactorGSRVolSettings_ vol;
    vol.factorNames_ = {"level"};
    vol.gKnotDates_ = vol.hKnotDates_ = {today};
    vol.gValues_ = Matrix_<>(1, 1, 0.0);
    vol.hValues_ = vol.correlations_ = Matrix_<>(1, 1, 1.0);
    const auto gaussian = NewMultiFactorGSRModelData("rates", curve, NewMultiFactorGSRVolData("vol", vol));
    const auto leverage = NewGSRLeverageData("leverage", {-0.02, 0.02}, {0.0}, Matrix_<>(2, 1, 1.0));
    GSRSLVSettings_ settings;
    settings.maxStep_ = 0.25;
    const auto original = NewGSRSLVModelData("smile", gaussian, leverage, settings);
    const auto model = handle_cast<ModelData_>(JSON::ReadString(JSON::WriteString(*original), true));
    const auto product = NewScriptProduct("bond", {Cell_(today.AddDays(365))}, {"pay PAYS FIX(IR[USD,DF,2028-10-01])"});
    ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    MonteCarloSettings_ simulation;
    simulation.enableAad_ = true;
    simulation.useBb_ = true;
    const auto result = ValueByMonteCarlo(product, model, 16, valuation, simulation);
    ASSERT_NEAR(result.at("PV"), std::exp(-0.06), 1e-12);
    ASSERT_NEAR(result.at("d_logdf:OIS:2029-10-01"), std::exp(-0.06) * 2.0 / 3.0, 1e-12);
    ASSERT_DOUBLE_EQ(result.at("d_volOfVol"), 0.0);
    ASSERT_DOUBLE_EQ(result.at("d_leverage:0:0"), 0.0);
    ASSERT_THROW(NewGSRSLVModelData("bad", NewBSModelData("bs", 100.0, 0.2, 0.03, 0.0), leverage), Exception_);
    ASSERT_THROW(NewGSRSLVModelData("bad", Handle_<ModelData_>(), leverage), Exception_);
}
