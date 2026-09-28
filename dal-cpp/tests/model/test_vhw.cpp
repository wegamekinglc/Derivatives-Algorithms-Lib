//
// Created by Codex on 2026/9/28.
//

#include <gtest/gtest.h>

#include <cmath>

#include <dal/platform/platform.hpp>

#include <dal/currency/currencydata.hpp>
#include <dal/curve/tapeguard.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/model/factory.hpp>
#include <dal/model/vhwdata.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/json.hpp>
#include <dal/time/schedules.hpp>

namespace {
    Dal::String_ CashSettledSwaptionExercise(const Dal::Date_& exercise, double strike) {
        const Dal::Ccy_ currency("USD");
        const Dal::Index::Swap_ swap(currency, "5Y");
        const Dal::Date_ start = swap.StartDate(Dal::DateTime_(exercise, 0.0));
        const Dal::Date_ maturity = Dal::Date::ParseIncrement("5Y")->FwdFrom(start);
        const auto& fixed = Dal::Ccy::Conventions::SwapFixedLeg()(currency);
        const auto periods = Dal::MakeSchedulePeriods(start, maturity, fixed.paymentFrequency_, fixed.accrualHolidays_, 0, Dal::Holidays::None(),
                                                      fixed.paymentLag_, fixed.paymentHolidays_, Dal::DateGeneration_("Forward"),
                                                      fixed.businessDayConvention_, fixed.paymentConvention_, fixed.endOfMonth_);
        Dal::String_ annuity;
        for (const auto& period : periods) {
            if (!annuity.empty())
                annuity += " + ";
            const double accrual = fixed.dayBasis_(period.accrualStart_, period.accrualEnd_, period.dayCountContext_.get());
            annuity += Dal::String::FromDouble(accrual) + " * FIX(IR[USD,DF," + Dal::Date::ToString(period.paymentDate_) + "])";
        }
        return "EXERCISE MAX((FIX(IR[USD,SWAP,5Y]) - " + Dal::String::FromDouble(strike) + ") * (" + annuity + "), 0)";
    }
} // namespace

using namespace Dal;

TEST(ModelTest, TestVhwZeroVolatilityRepricesInitialCurve) {
    const Date_ today(2026, 9, 28);
    const Date_ oneYear(2027, 9, 28);
    const Date_ twoYears(2028, 9, 28);
    const Handle_<VHWCurveData_> curve(
        new VHWCurveData_("curve", today, "USD", {today, oneYear, twoYears}, {0.0, -0.03, -0.06}, {}, Matrix_<>(0, 0)));
    const Handle_<VHWVolData_> vol(new VHWVolData_("vol", {today}, {0.0}, {today}, {1.0}));
    const Handle_<ModelData_> data(new VHWModelData_("vhw", curve, vol));
    auto model = CreateModel<double>(data);
    const Vector_<> timeline{0.0, 1.0};
    Vector_<AAD::SampleDef_> defs(2);
    model->Allocate(timeline, defs);
    model->Init(timeline, defs);
    AAD::Scenario_<> path;
    AAD::AllocatePath(defs, path);
    model->GeneratePath(Vector_<>(model->SimDim(), 0.3), &path);
    ASSERT_NEAR(path[0].numeraire_, 1.0, 1e-12);
    ASSERT_NEAR(path[1].numeraire_, std::exp(0.03), 1e-10);
}

TEST(ModelTest, TestVhwRateIndexNamesParseToExistingCanonicalNames) {
    ASSERT_EQ(Index::Parse("IR[USD,DF,2028-09-28]")->Name(), String_("IR[DF]:USD,2028-09-28"));
    ASSERT_EQ(Index::Parse("IR[USD,LIBOR_3M_LCH]")->Name(), String_("IR:USD,LIBOR_3M_LCH"));
    ASSERT_EQ(Index::Parse("IR[USD,SWAP,5Y]")->Name(), String_("IR:USD,5Y"));
}

TEST(ModelTest, TestVhwScriptBondPayoffRepricesInitialCurve) {
    const Date_ today(2026, 9, 28);
    const Date_ oneYear(2027, 9, 28);
    const Date_ twoYears(2028, 9, 28);
    const Handle_<VHWCurveData_> curve(
        new VHWCurveData_("curve", today, "USD", {today, oneYear, twoYears}, {0.0, -0.03, -0.06}, {}, Matrix_<>(0, 0)));
    const Handle_<VHWVolData_> vol(new VHWVolData_("vol", {today}, {0.0}, {today}, {1.0}));
    const Handle_<ModelData_> model(new VHWModelData_("vhw", curve, vol));
    const Script::ScriptProductData_ product("bond", {Cell_(oneYear)}, {"pay PAYS FIX(IR[USD,DF,2028-09-28])"});
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    const auto results = Script::MCSimulation<double>(product, model, 1, valuation);
    ASSERT_NEAR(results.aggregated_, std::exp(-0.06), 1e-10);
}

TEST(ModelTest, TestVhwBermudanBondOptionUsesStochasticDiscounting) {
    const Date_ today(2026, 9, 28);
    const Date_ first(2027, 3, 28);
    const Date_ second(2027, 9, 28);
    const Date_ maturity(2028, 9, 28);
    const Handle_<VHWCurveData_> curve(
        new VHWCurveData_("curve", today, "USD", {today, first, second, maturity}, {0.0, -0.015, -0.03, -0.06}, {}, Matrix_<>(0, 0)));
    const Handle_<VHWVolData_> vol(new VHWVolData_("vol", {today}, {0.02}, {today}, {1.0}));
    const Handle_<ModelData_> model(new VHWModelData_("vhw", curve, vol));
    Script::ScriptProductSettings_ contract;
    contract.regressionFeatures_ = {"IR[USD,DF,2028-09-28]"};
    const Script::ScriptProductData_ product(
        "bond_option", {Cell_(first), Cell_(second)},
        {"EXERCISE MAX(FIX(IR[USD,DF,2028-09-28]) - 0.94, 0)", "EXERCISE MAX(FIX(IR[USD,DF,2028-09-28]) - 0.94, 0)"}, contract);
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    Script::MonteCarloSettings_ simulation;
    simulation.lsmcTrainingPaths_ = 128;
    const auto results = Script::MCSimulation<double>(product, model, 128, valuation, simulation);
    ASSERT_TRUE(std::isfinite(results.aggregated_));
    ASSERT_GT(results.aggregated_, 0.0);

    const Script::ScriptProductData_ parBond("par_bond", {Cell_(first), Cell_(second)},
                                             {"EXERCISE FIX(IR[USD,DF,2028-09-28])", "EXERCISE FIX(IR[USD,DF,2028-09-28])"}, contract);
    simulation.lsmcTrainingPaths_ = 4096;
    const auto parPrice = Script::MCSimulation<double>(parBond, model, 8192, valuation, simulation).aggregated_ / 8192.0;
    ASSERT_NEAR(parPrice, std::exp(-0.06), 3e-3);

    simulation.enableAad_ = true;
    const auto aad = Script::MCSimulation<AAD::Number_>(product, model, 512, valuation, simulation);
    ASSERT_TRUE(std::isfinite(aad.aggregated_));
    for (const double risk : aad.risks_)
        ASSERT_TRUE(std::isfinite(risk));
}

TEST(ModelTest, TestVhwConditionalBondAndDiscountStepMatchGaussianFormula) {
    const Date_ today(2025, 1, 1);
    const Date_ oneYear(2026, 1, 1);
    const Date_ twoYears(2027, 1, 1);
    const Handle_<VHWCurveData_> curve(
        new VHWCurveData_("curve", today, "USD", {today, oneYear, twoYears}, {0.0, -0.03, -0.06}, {}, Matrix_<>(0, 0)));
    const Handle_<VHWVolData_> vol(new VHWVolData_("vol", {today}, {0.01}, {today}, {1.0}));
    auto model = CreateModel<double>(Handle_<ModelData_>(new VHWModelData_("vhw", curve, vol)));
    Vector_<AAD::SampleDef_> defs(2);
    defs[1].indexNames_ = {"IR[USD,DF,2027-01-01]"};
    model->Allocate({0.0, 1.0}, defs);
    model->Init({0.0, 1.0}, defs);
    AAD::Scenario_<> path;
    AAD::AllocatePath(defs, path);
    model->GeneratePath({0.0}, &path);
    ASSERT_NEAR(path[1].numeraire_, std::exp(0.03 + 0.0000125), 1e-12);
    ASSERT_NEAR(path[1].observations_[0], std::exp(-0.03 - 0.0001), 1e-12);
}

TEST(ModelTest, TestVhwDiscountedBondsAreMartingalesOnEventGrid) {
    const Date_ today(2025, 1, 1);
    const Date_ oneYear(2026, 1, 1);
    const Date_ twoYears(2027, 1, 1);
    const Handle_<VHWCurveData_> curve(
        new VHWCurveData_("curve", today, "USD", {today, oneYear, twoYears}, {0.0, -0.03, -0.06}, {}, Matrix_<>(0, 0)));
    const Handle_<VHWVolData_> vol(new VHWVolData_("vol", {today}, {0.02}, {today}, {1.0}));
    auto model = CreateModel<double>(Handle_<ModelData_>(new VHWModelData_("vhw", curve, vol)));
    Vector_<AAD::SampleDef_> defs(2);
    defs[1].indexNames_ = {"IR[USD,DF,2027-01-01]"};
    model->Allocate({0.0, 1.0}, defs);
    model->Init({0.0, 1.0}, defs);
    double discountedBond = 0.0;
    double discount = 0.0;
    const double nodes[] = {0.0, std::sqrt(3.0), -std::sqrt(3.0)};
    const double weights[] = {2.0 / 3.0, 1.0 / 6.0, 1.0 / 6.0};
    for (size_t i = 0; i < 3; ++i) {
        AAD::Scenario_<> path;
        AAD::AllocatePath(defs, path);
        model->GeneratePath({nodes[i]}, &path);
        discountedBond += weights[i] * path[1].observations_[0] / path[1].numeraire_;
        discount += weights[i] / path[1].numeraire_;
    }
    ASSERT_NEAR(discount, std::exp(-0.03), 1e-10);
    ASSERT_NEAR(discountedBond, std::exp(-0.06), 1e-10);
}

TEST(ModelTest, TestVhwNumeraireCarriesCurveNodeAdjoint) {
    const Date_ today(2026, 9, 28);
    const Date_ oneYear(2027, 9, 28);
    const Handle_<VHWCurveData_> curve(new VHWCurveData_("curve", today, "USD", {today, oneYear}, {0.0, -0.03}, {}, Matrix_<>(0, 0)));
    const Handle_<VHWVolData_> vol(new VHWVolData_("vol", {today}, {0.0}, {today}, {1.0}));
    const TapeGuard_ guard(AAD::Tape());
    auto model = CreateModel<AAD::Number_>(Handle_<ModelData_>(new VHWModelData_("vhw", curve, vol)));
    Vector_<AAD::SampleDef_> defs(2);
    model->Allocate({0.0, 1.0}, defs);
    AAD::Scenario_<AAD::Number_> path;
    AAD::AllocatePath(defs, path);
    AAD::Rewind(*AAD::Tape());
    for (auto* parameter : model->Parameters())
        AAD::PutOnTape(*parameter);
    AAD::NewRecording(*AAD::Tape());
    model->Init({0.0, 1.0}, defs);
    model->GeneratePath({0.0}, &path);
    AAD::Number_ payoff = 1.0 / path[1].numeraire_;
    AAD::Adjoint(payoff) = 1.0;
    AAD::PropagateToStart(*AAD::Tape());
    ASSERT_NEAR(AAD::Value(payoff), std::exp(-0.03), 1e-10);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[0]), std::exp(-0.03), 1e-10);
}

TEST(ModelTest, TestVhwProjectionLiborAndSwapUseRateCurve) {
    const Date_ today(2026, 9, 28);
    const Date_ observation(2027, 9, 28);
    const Date_ horizon(2037, 9, 28);
    const double years = (horizon - today) / DAYS_PER_YEAR;
    Matrix_<> projection(1, 3, 0.0);
    projection(0, 1) = -0.03;
    projection(0, 2) = -0.03 * years;
    const Handle_<VHWCurveData_> curve(
        new VHWCurveData_("curve", today, "USD", {today, observation, horizon}, {0.0, -0.02, -0.02 * years}, {"3M"}, projection));
    const Handle_<VHWVolData_> vol(new VHWVolData_("vol", {today}, {0.0}, {today}, {1.0}));
    auto model = CreateModel<double>(Handle_<ModelData_>(new VHWModelData_("vhw", curve, vol)));
    Vector_<AAD::SampleDef_> defs(2);
    defs[1].indexNames_ = {"IR[USD,LIBOR_3M_LCH]", "IR[USD,SWAP,5Y]"};
    model->Allocate({0.0, 1.0}, defs);
    model->Init({0.0, 1.0}, defs);
    AAD::Scenario_<> path;
    AAD::AllocatePath(defs, path);
    model->GeneratePath({0.0}, &path);
    const auto index = Index::Parse("IR[USD,LIBOR_3M_LCH]");
    const auto* libor = dynamic_cast<const Index::Libor_*>(index.get());
    ASSERT_TRUE(libor);
    const Date_ start = libor->StartDate(DateTime_(observation, 0.0));
    const Date_ end = Date::NominalMaturity(start, libor->tenor_.Period(), libor->ccy_);
    const double accrual = Ccy::Conventions::LiborDayBasis()(libor->ccy_)(start, end, nullptr);
    ASSERT_NEAR(path[1].observations_[0], (std::exp(0.03 * (end - start) / DAYS_PER_YEAR) - 1.0) / accrual, 1e-10);
    ASSERT_GT(path[1].observations_[1], 0.02);
    ASSERT_LT(path[1].observations_[1], 0.04);
}

TEST(ModelTest, TestVhwBermudanCashSettledSwaptionUsesExistingLsmExercise) {
    const Date_ today(2026, 9, 28);
    const Date_ first(2027, 3, 28);
    const Date_ second(2027, 9, 28);
    const Date_ horizon(2034, 9, 28);
    const double years = (horizon - today) / DAYS_PER_YEAR;
    Matrix_<> projection(1, 2, 0.0);
    projection(0, 1) = -0.03 * years;
    const Handle_<VHWCurveData_> curve(new VHWCurveData_("curve", today, "USD", {today, horizon}, {0.0, -0.02 * years}, {"3M"}, projection));
    const Handle_<VHWVolData_> vol(new VHWVolData_("vol", {today}, {0.01}, {today}, {1.0}));
    const Handle_<ModelData_> model(new VHWModelData_("vhw", curve, vol));
    Script::ScriptProductSettings_ contract;
    contract.regressionFeatures_ = {"IR[USD,SWAP,5Y]"};
    const Script::ScriptProductData_ product("bermudan_swaption", {Cell_(first), Cell_(second)},
                                             {CashSettledSwaptionExercise(first, 0.025), CashSettledSwaptionExercise(second, 0.025)}, contract);
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today;
    Script::MonteCarloSettings_ simulation;
    simulation.lsmcTrainingPaths_ = 512;
    const double tree = Script::MCSimulation<double>(product, model, 512, valuation, simulation).aggregated_ / 512.0;
    simulation.compiled_ = true;
    const double compiled = Script::MCSimulation<double>(product, model, 512, valuation, simulation).aggregated_ / 512.0;
    ASSERT_TRUE(std::isfinite(tree));
    ASSERT_GT(tree, 0.0);
    ASSERT_NEAR(compiled, tree, 1e-10);
}

TEST(ModelTest, TestVhwRejectsInvalidTenorsDatesAndVolatility) {
    const Date_ today(2026, 9, 28);
    const Date_ year(2027, 9, 28);
    Matrix_<> duplicateProjection(2, 2, 0.0);
    ASSERT_THROW(VHWCurveData_("duplicate", today, "USD", {today, year}, {0.0, -0.03}, {"3M", "QUARTERLY"}, duplicateProjection), Exception_);
    ASSERT_THROW(VHWVolData_("negative", {today}, {-0.01}, {today}, {1.0}), Exception_);
    ASSERT_THROW(VHWVolData_("mean_reversion", {today}, {0.01}, {today}, {0.0}), Exception_);

    const Handle_<VHWCurveData_> curve(new VHWCurveData_("curve", today, "USD", {today, year}, {0.0, -0.03}, {}, Matrix_<>(0, 0)));
    const Handle_<VHWVolData_> vol(new VHWVolData_("vol", {today}, {0.01}, {today}, {1.0}));
    const Handle_<ModelData_> data(new VHWModelData_("vhw", curve, vol));
    auto model = CreateModel<double>(data);
    Vector_<AAD::SampleDef_> defs(2);
    defs[1].indexNames_ = {"IR[USD,DF,2028-09-28]"};
    model->Allocate({0.0, 1.0}, defs);
    ASSERT_THROW(model->Init({0.0, 1.0}, defs), Exception_);

    const Script::ScriptProductData_ product("bond", {Cell_(year)}, {"pay PAYS 1"});
    Script::ScriptValuationSettings_ valuation;
    valuation.evaluationDate_ = today.AddDays(1);
    ASSERT_THROW(Script::MCSimulation<double>(product, data, 16, valuation), Exception_);
}

TEST(ModelTest, TestVhwModelDataRoundTripsCurveAndVolatility) {
    const Date_ today(2026, 9, 28);
    const Date_ year(2027, 9, 28);
    Matrix_<> projection(1, 2, 0.0);
    projection(0, 1) = -0.04;
    const Handle_<VHWCurveData_> curve(new VHWCurveData_("curve", today, "USD", {today, year}, {0.0, -0.03}, {"3M"}, projection));
    const Handle_<VHWVolData_> vol(new VHWVolData_("vol", {today}, {0.01}, {today}, {1.0}));
    const Handle_<VHWModelData_> data(new VHWModelData_("vhw", curve, vol));
    const auto restored = handle_cast<VHWModelData_>(JSON::ReadString(JSON::WriteString(*data), false));
    ASSERT_TRUE(restored);
    ASSERT_EQ(restored->curve_->evaluationDate_, today);
    ASSERT_EQ(restored->curve_->currency_, String_("USD"));
    ASSERT_EQ(restored->curve_->projectionTenors_[0], String_("3M"));
    ASSERT_DOUBLE_EQ(restored->curve_->projectionLogDF_(0, 1), -0.04);
    ASSERT_DOUBLE_EQ(restored->vol_->gValues_[0], 0.01);
    ASSERT_DOUBLE_EQ(restored->vol_->hValues_[0], 1.0);
}
