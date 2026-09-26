//
// Created by Codex on 2026/9/27.
//

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <future>
#include <limits>

#include <dal/curve/tapeguard.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/model/correlatedblackscholes.hpp>
#include <dal/model/dupire.hpp>
#include <dal/model/factory.hpp>
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

namespace {
    CorrelatedBSSettings_ Settings(const Vector_<CorrelatedBSAsset_>& assets, const Matrix_<>& correlations) {
        CorrelatedBSSettings_ result;
        result.assets_ = assets;
        result.rate_ = 0.05;
        result.correlations_ = correlations;
        return result;
    }

    Matrix_<> Correlation(double rho) {
        Matrix_<> result(2, 2, 0.0);
        result(0, 0) = result(1, 1) = 1.0;
        result(0, 1) = result(1, 0) = rho;
        return result;
    }

    template <class T_> AAD::Scenario_<T_> Path(const Vector_<AAD::SampleDef_>& definitions) {
        AAD::Scenario_<T_> path;
        AAD::AllocatePath(definitions, path);
        AAD::InitializePath(path);
        return path;
    }

    void ExpectInvalidCorrelation(const Matrix_<>& correlation, const String_& message) {
        const auto settings = Settings({{"EQ[AAA]", 100.0, 0.2, 0.0}, {"EQ[BBB]", 120.0, 0.3, 0.0}}, correlation);
        const Handle_<ModelData_> data(std::make_shared<CorrelatedBSModelData_>("invalid", settings));
        try {
            static_cast<void>(CreateModel<double>(data));
            FAIL() << "invalid correlation accepted";
        } catch (const Exception_& error) {
            ASSERT_NE(std::string(error.what()).find(message.c_str()), std::string::npos);
        }
    }
} // namespace

TEST(ModelTest, TestCorrelatedBlackScholesNamedOutputs) {
    CorrelatedBSSettings_ settings;
    settings.assets_ = {{"EQ[AAA]", 100.0, 0.2, 0.01}, {"EQ[BBB]", 120.0, 0.3, 0.02}};
    settings.rate_ = 0.05;
    settings.correlations_ = Matrix_<>(2, 2, 0.0);
    settings.correlations_(0, 0) = settings.correlations_(1, 1) = 1.0;
    settings.correlations_(0, 1) = settings.correlations_(1, 0) = 0.35;

    const Handle_<ModelData_> data(std::make_shared<CorrelatedBSModelData_>("basket", settings));
    auto model = CreateModel<double>(data);
    const Vector_<> timeline{0.0, 1.0};
    Vector_<AAD::SampleDef_> definitions(2);
    definitions[0].indexNames_ = {"EQ[BBB]", "EQ[AAA]"};
    definitions[1].indexNames_ = {"EQ[BBB]", "EQ[AAA]"};
    model->Allocate(timeline, definitions);
    model->Init(timeline, definitions);
    ASSERT_EQ(model->SimDim(), 2);

    AAD::Scenario_<> path;
    AAD::AllocatePath(definitions, path);
    AAD::InitializePath(path);
    model->GeneratePath({0.4, -0.7}, &path);

    const double x0 = 0.4;
    const double x1 = 0.35 * x0 + std::sqrt(1.0 - 0.35 * 0.35) * -0.7;
    ASSERT_NEAR(path[0].observations_[0], 120.0, 1e-10);
    ASSERT_NEAR(path[0].observations_[1], 100.0, 1e-10);
    ASSERT_NEAR(path[1].observations_[0], 120.0 * std::exp(0.05 - 0.02 - 0.5 * 0.3 * 0.3 + 0.3 * x1), 1e-10);
    ASSERT_NEAR(path[1].observations_[1], 100.0 * std::exp(0.05 - 0.01 - 0.5 * 0.2 * 0.2 + 0.2 * x0), 1e-10);
    ASSERT_NEAR(path[1].numeraire_, std::exp(0.05), 1e-10);
}

TEST(ModelTest, TestCorrelatedBlackScholesMatchesSingleAssetBS) {
    const auto settings = Settings({{"EQ[AAA]", 100.0, 0.2, 0.01}}, Matrix_<>(1, 1, 1.0));
    AAD::CorrelatedBlackScholes_<> multi({"EQ[AAA]"}, {100.0}, {0.2}, {0.01}, 0.05, settings.correlations_);
    AAD::BlackScholes_<> legacy(100.0, 0.2, 0.05, 0.01);
    const Vector_<> timeline{0.0, 0.4, 1.0};
    Vector_<AAD::SampleDef_> definitions(3);
    for (auto& definition : definitions)
        definition.indexNames_ = {"EQ[AAA]"};
    multi.Allocate(timeline, definitions);
    legacy.Allocate(timeline, definitions);
    multi.Init(timeline, definitions);
    legacy.Init(timeline, definitions);
    ASSERT_EQ(multi.SimDim(), legacy.SimDim());
    auto multiPath = Path<double>(definitions);
    auto legacyPath = Path<double>(definitions);
    for (const Vector_<> gaussian : {Vector_<>{0.3, -0.8}, Vector_<>{-1.1, 0.7}}) {
        multi.GeneratePath(gaussian, &multiPath);
        legacy.GeneratePath(gaussian, &legacyPath);
        for (size_t date = 0; date < timeline.size(); ++date) {
            ASSERT_NEAR(multiPath[date].spot_, legacyPath[date].spot_, 1e-12);
            ASSERT_NEAR(multiPath[date].numeraire_, legacyPath[date].numeraire_, 1e-12);
            ASSERT_NEAR(multiPath[date].observations_[0], legacyPath[date].observations_[0], 1e-12);
        }
    }
}

TEST(ModelTest, TestCorrelatedBlackScholesValidatesInputsAndOutputs) {
    auto correlation = Correlation(0.25);
    ExpectInvalidCorrelation(Matrix_<>(1, 1, 1.0), "dimensions");
    correlation(0, 0) = 0.8;
    ExpectInvalidCorrelation(correlation, "diagonal");
    correlation = Correlation(0.25);
    correlation(0, 1) = 0.35;
    ExpectInvalidCorrelation(correlation, "symmetric");
    ExpectInvalidCorrelation(Correlation(1.0), "positive definite");
    correlation = Correlation(0.25);
    correlation(0, 1) = std::numeric_limits<double>::quiet_NaN();
    ExpectInvalidCorrelation(correlation, "non-finite");

    ASSERT_THROW((AAD::CorrelatedBlackScholes_<>({"EQ[AAA]", "EQ[AAA]"}, {100.0, 120.0}, {0.2, 0.3}, {0.0, 0.0}, 0.05, Correlation(0.0))),
                 Exception_);
    ASSERT_THROW((AAD::CorrelatedBlackScholes_<>({"EQ[AAA]", "EQ[BBB]"}, {100.0, 120.0}, {0.2, -0.3}, {0.0, 0.0}, 0.05, Correlation(0.0))),
                 Exception_);
    ASSERT_THROW((AAD::CorrelatedBlackScholes_<>({"EQ[AAA]", "FX[EUR/USD]"}, {100.0, 120.0}, {0.2, 0.3}, {0.0, 0.0}, 0.05, Correlation(0.0))),
                 Exception_);

    AAD::CorrelatedBlackScholes_<> model({"EQ[AAA]", "EQ[BBB]"}, {100.0, 120.0}, {0.2, 0.3}, {0.0, 0.0}, 0.05, Correlation(0.0));
    const Vector_<> timeline{0.0, 1.0};
    Vector_<AAD::SampleDef_> definitions(2);
    definitions[1].indexNames_ = {"EQ[AAA]", "EQ[AAA]", "EQ[BBB]"};
    ASSERT_NO_THROW(model.Allocate(timeline, definitions));
    model.Init(timeline, definitions);
    auto path = Path<double>(definitions);
    model.GeneratePath({0.0, 0.0}, &path);
    ASSERT_DOUBLE_EQ(path[1].observations_[0], path[1].observations_[1]);
    definitions[1].indexNames_ = {"EQ[CCC]"};
    ASSERT_THROW(model.Allocate(timeline, definitions), Exception_);
    ASSERT_THROW((AAD::CorrelatedBlackScholes_<>({"EQ[AAA]"}, {std::numeric_limits<double>::infinity()}, {0.2}, {0.0}, 0.05, Matrix_<>(1, 1, 1.0))),
                 Exception_);
    ASSERT_THROW((AAD::CorrelatedBlackScholes_<>({"EQ[AAA]"}, {100.0}, {0.2}, {0.0}, std::numeric_limits<double>::quiet_NaN(), Matrix_<>(1, 1, 1.0))),
                 Exception_);
}

TEST(ModelTest, TestLegacyModelsStillRejectMultipleOutputSlots) {
    AAD::BlackScholes_<> blackScholes(100.0, 0.2, 0.05, 0.01);
    AAD::Dupire_<> dupire(100.0, 0.05, 0.01, {80.0, 120.0}, {0.0, 1.0}, Matrix_<>(2, 2, 0.2));
    const Vector_<> timeline{0.0, 1.0};
    Vector_<AAD::SampleDef_> definitions(2);
    definitions[1].indexNames_ = {"EQ[AAA]", "EQ[BBB]"};
    ASSERT_THROW(blackScholes.Allocate(timeline, definitions), Exception_);
    ASSERT_THROW(dupire.Allocate(timeline, definitions), Exception_);
}

TEST(ModelTest, TestCorrelatedBlackScholesTwoAssetCovarianceAndZeroVol) {
    constexpr std::array<double, 5> nodes{-2.856970013872806, -1.355626179974266, 0.0, 1.355626179974266, 2.856970013872806};
    constexpr std::array<double, 5> weights{0.0112574113277207, 0.222075922005613, 0.533333333333333, 0.222075922005613, 0.0112574113277207};
    const Vector_<> timeline{0.0, 0.4, 1.0};
    Vector_<AAD::SampleDef_> definitions(3);
    definitions[0].indexNames_ = {"EQ[BBB]"};
    definitions[1].indexNames_ = {"EQ[AAA]", "EQ[BBB]"};
    definitions[2].indexNames_ = {"EQ[BBB]", "EQ[AAA]"};
    for (const double rho : {0.0, 0.6, -0.5}) {
        AAD::CorrelatedBlackScholes_<> model({"EQ[AAA]", "EQ[BBB]"}, {100.0, 120.0}, {0.2, 0.3}, {0.01, 0.02}, 0.05, Correlation(rho));
        model.Allocate(timeline, definitions);
        model.Init(timeline, definitions);
        ASSERT_EQ(model.SimDim(), 4);
        auto path = Path<double>(definitions);
        double meanFirst = 0.0;
        double meanSecond = 0.0;
        double meanProduct = 0.0;
        double meanSpotFirst = 0.0;
        double meanSpotSecond = 0.0;
        for (size_t i = 0; i < nodes.size(); ++i)
            for (size_t j = 0; j < nodes.size(); ++j)
                for (size_t k = 0; k < nodes.size(); ++k)
                    for (size_t l = 0; l < nodes.size(); ++l) {
                        const double weight = weights[i] * weights[j] * weights[k] * weights[l];
                        model.GeneratePath({nodes[i], nodes[j], nodes[k], nodes[l]}, &path);
                        ASSERT_DOUBLE_EQ(path[0].observations_[0], 120.0);
                        const double firstSpot = path[2].observations_[1];
                        const double secondSpot = path[2].observations_[0];
                        const double firstLog = std::log(firstSpot);
                        const double secondLog = std::log(secondSpot);
                        meanSpotFirst += weight * firstSpot;
                        meanSpotSecond += weight * secondSpot;
                        meanFirst += weight * firstLog;
                        meanSecond += weight * secondLog;
                        meanProduct += weight * firstLog * secondLog;
                    }
        ASSERT_NEAR(meanSpotFirst, 100.0 * std::exp(0.05 - 0.01), 1e-6);
        ASSERT_NEAR(meanSpotSecond, 120.0 * std::exp(0.05 - 0.02), 1e-6);
        ASSERT_NEAR(meanProduct - meanFirst * meanSecond, 0.2 * 0.3 * rho, 1e-11);
    }

    AAD::CorrelatedBlackScholes_<> zeroVol({"EQ[AAA]", "EQ[BBB]"}, {100.0, 120.0}, {0.2, 0.0}, {0.01, 0.02}, 0.05, Correlation(-0.5));
    zeroVol.Allocate(timeline, definitions);
    zeroVol.Init(timeline, definitions);
    auto path = Path<double>(definitions);
    zeroVol.GeneratePath({-1.0, 2.0, 0.5, -0.7}, &path);
    ASSERT_NEAR(path[2].observations_[0], 120.0 * std::exp(0.03), 1e-10);
}

TEST(ModelTest, TestCorrelatedBlackScholesMomentAndBasketPayoff) {
    Matrix_<> correlation(3, 3, 0.0);
    correlation(0, 0) = correlation(1, 1) = correlation(2, 2) = 1.0;
    correlation(0, 1) = correlation(1, 0) = 0.35;
    correlation(0, 2) = correlation(2, 0) = -0.2;
    correlation(1, 2) = correlation(2, 1) = 0.25;
    const std::array<double, 3> spots{100.0, 110.0, 90.0};
    const std::array<double, 3> vols{0.2, 0.3, 0.15};
    const std::array<double, 3> divs{0.01, 0.02, 0.0};
    constexpr double maturity = 1.25;
    AAD::CorrelatedBlackScholes_<> model({"EQ[AAA]", "EQ[BBB]", "EQ[CCC]"}, {spots[0], spots[1], spots[2]}, {vols[0], vols[1], vols[2]},
                                         {divs[0], divs[1], divs[2]}, 0.05, correlation);
    const Vector_<> timeline{maturity};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].indexNames_ = {"EQ[AAA]", "EQ[BBB]", "EQ[CCC]"};
    model.Allocate(timeline, definitions);
    model.Init(timeline, definitions);
    ASSERT_EQ(model.SimDim(), 3);
    auto path = Path<double>(definitions);

    // Five-point standard-normal Gauss-Hermite cubature is deterministic and
    // integrates the quadratic log-spot moments exactly.
    constexpr std::array<double, 5> nodes{-2.856970013872806, -1.355626179974266, 0.0, 1.355626179974266, 2.856970013872806};
    constexpr std::array<double, 5> weights{0.0112574113277207, 0.222075922005613, 0.533333333333333, 0.222075922005613, 0.0112574113277207};
    std::array<double, 3> meanSpot{};
    std::array<double, 3> meanLog{};
    std::array<double, 3> pairLogProducts{};
    double basketValue = 0.0;
    for (size_t i = 0; i < nodes.size(); ++i)
        for (size_t j = 0; j < nodes.size(); ++j)
            for (size_t k = 0; k < nodes.size(); ++k) {
                const double weight = weights[i] * weights[j] * weights[k];
                model.GeneratePath({nodes[i], nodes[j], nodes[k]}, &path);
                const auto& observations = path[0].observations_;
                const std::array<double, 3> logs{std::log(observations[0]), std::log(observations[1]), std::log(observations[2])};
                for (size_t asset = 0; asset < 3; ++asset) {
                    meanSpot[asset] += weight * observations[asset];
                    meanLog[asset] += weight * logs[asset];
                }
                pairLogProducts[0] += weight * logs[0] * logs[1];
                pairLogProducts[1] += weight * logs[0] * logs[2];
                pairLogProducts[2] += weight * logs[1] * logs[2];
                basketValue += weight * observations[0] * observations[2] / path[0].numeraire_;
            }
    for (size_t asset = 0; asset < 3; ++asset)
        ASSERT_NEAR(meanSpot[asset], spots[asset] * std::exp((0.05 - divs[asset]) * maturity), 1e-6);
    ASSERT_NEAR(pairLogProducts[0] - meanLog[0] * meanLog[1], vols[0] * vols[1] * correlation(0, 1) * maturity, 1e-11);
    ASSERT_NEAR(pairLogProducts[1] - meanLog[0] * meanLog[2], vols[0] * vols[2] * correlation(0, 2) * maturity, 1e-11);
    ASSERT_NEAR(pairLogProducts[2] - meanLog[1] * meanLog[2], vols[1] * vols[2] * correlation(1, 2) * maturity, 1e-11);
    const double expectedBasket = spots[0] * spots[2] * std::exp((0.05 - divs[0] - divs[2] + vols[0] * vols[2] * correlation(0, 2)) * maturity);
    ASSERT_NEAR(basketValue, expectedBasket, 1e-5);
}

TEST(ModelTest, TestCorrelatedBlackScholesAadRisksAndClone) {
    const TapeGuard_ guard(AAD::Tape());
    AAD::CorrelatedBlackScholes_<AAD::Number_> original({"EQ[AAA]", "EQ[BBB]"}, {100.0, 120.0}, {0.2, 0.3}, {0.01, 0.02}, 0.05, Correlation(-0.4));
    auto model = original.Clone();
    for (size_t i = 0; i < original.NumParams(); ++i)
        ASSERT_NE(model->Parameters()[i], original.Parameters()[i]);
    ASSERT_EQ(model->ParameterLabels(),
              (Vector_<String_>{"spot:EQ[AAA]", "vol:EQ[AAA]", "div:EQ[AAA]", "spot:EQ[BBB]", "vol:EQ[BBB]", "div:EQ[BBB]", "rate"}));
    *model->Parameters()[0] = 105.0;
    ASSERT_DOUBLE_EQ(AAD::Value(*original.Parameters()[0]), 100.0);

    const Vector_<> timeline{1.0};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].indexNames_ = {"EQ[AAA]", "EQ[BBB]"};
    model->Allocate(timeline, definitions);
    auto path = Path<AAD::Number_>(definitions);
    AAD::Rewind(*AAD::Tape());
    for (auto* parameter : model->Parameters())
        AAD::PutOnTape(*parameter);
    AAD::NewRecording(*AAD::Tape());
    model->Init(timeline, definitions);
    model->GeneratePath({0.4, -0.7}, &path);
    AAD::Number_ payoff = path[0].observations_[1] / path[0].numeraire_;
    const double normal = -0.4 * 0.4 + std::sqrt(1.0 - 0.4 * 0.4) * -0.7;
    const double expected = 120.0 * std::exp(-0.02 - 0.5 * 0.3 * 0.3 + 0.3 * normal);
    ASSERT_NEAR(AAD::Value(payoff), expected, 1e-10);
    AAD::Adjoint(payoff) = 1.0;
    AAD::PropagateToStart(*AAD::Tape());
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[0]), 0.0, 1e-10);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[3]), expected / 120.0, 1e-10);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[4]), expected * (normal - 0.3), 1e-10);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[5]), -expected, 1e-10);
    ASSERT_NEAR(AAD::Adjoint(*model->Parameters()[6]), 0.0, 1e-10);
}

TEST(ModelTest, TestCorrelatedBlackScholesSingleAssetAadParity) {
    const TapeGuard_ guard(AAD::Tape());
    AAD::CorrelatedBlackScholes_<AAD::Number_> multi({"EQ[AAA]"}, {100.0}, {0.2}, {0.01}, 0.05, Matrix_<>(1, 1, 1.0));
    AAD::BlackScholes_<AAD::Number_> legacy(100.0, 0.2, 0.05, 0.01);
    const Vector_<> timeline{1.0};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].indexNames_ = {"EQ[AAA]"};
    const auto risk = [&](AAD::Model_<AAD::Number_>* model) {
        model->Allocate(timeline, definitions);
        auto path = Path<AAD::Number_>(definitions);
        AAD::Rewind(*AAD::Tape());
        for (auto* parameter : model->Parameters())
            AAD::PutOnTape(*parameter);
        AAD::NewRecording(*AAD::Tape());
        model->Init(timeline, definitions);
        model->GeneratePath({0.4}, &path);
        AAD::Number_ pv = path[0].observations_[0] / path[0].numeraire_;
        AAD::Adjoint(pv) = 1.0;
        AAD::PropagateToStart(*AAD::Tape());
        Vector_<> result{AAD::Value(pv)};
        for (auto* parameter : model->Parameters())
            result.push_back(AAD::Adjoint(*parameter));
        return result;
    };
    const auto newRisks = risk(&multi);
    const auto oldRisks = risk(&legacy);
    ASSERT_EQ(newRisks.size(), 5);
    ASSERT_NEAR(newRisks[0], oldRisks[0], 1e-10); // PV
    ASSERT_NEAR(newRisks[1], oldRisks[1], 1e-10); // spot
    ASSERT_NEAR(newRisks[2], oldRisks[2], 1e-10); // vol
    ASSERT_NEAR(newRisks[3], oldRisks[4], 1e-10); // div
    ASSERT_NEAR(newRisks[4], oldRisks[3], 1e-10); // rate
}

TEST(ModelTest, TestCorrelatedBlackScholesReusesAadPathAcrossTapeMarks) {
    const TapeGuard_ guard(AAD::Tape());
    AAD::CorrelatedBlackScholes_<AAD::Number_> model({"EQ[AAA]", "EQ[BBB]"}, {100.0, 120.0}, {0.2, 0.3}, {0.01, 0.02}, 0.05, Correlation(0.35));
    const Vector_<> timeline{1.0};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].indexNames_ = {"EQ[BBB]", "EQ[AAA]"};
    model.Allocate(timeline, definitions);
    auto path = Path<AAD::Number_>(definitions);
    AAD::Number_ zero = 0.0;
    AAD::Rewind(*AAD::Tape());
    for (auto* parameter : model.Parameters())
        AAD::PutOnTape(*parameter);
    AAD::PutOnTape(zero);
    AAD::NewRecording(*AAD::Tape());
    model.Init(timeline, definitions);
    AAD::Mark(*AAD::Tape());
    double expectedSpotRisk = 0.0;
    for (const Vector_<> gaussian : {Vector_<>{0.0, 0.0}, Vector_<>{0.4, -0.7}}) {
        AAD::RewindToMark(*AAD::Tape());
        model.GeneratePath(gaussian, &path);
        AAD::Number_ payoff = AAD::PayoffRoot(path[0].observations_[0] / path[0].numeraire_, zero);
        expectedSpotRisk += AAD::Value(payoff) / 120.0;
        AAD::Adjoint(payoff) = 1.0;
        AAD::PropagateToMark(*AAD::Tape());
    }
    AAD::PropagateMarkToStart(*AAD::Tape());
    ASSERT_NEAR(AAD::Adjoint(*model.Parameters()[3]), expectedSpotRisk, 1e-10);
}

TEST(ModelTest, TestCorrelatedBlackScholesArchiveAndBridge) {
    const auto settings = Settings({{"EQ[AAA]", 100.0, 0.2, 0.01}, {"EQ[BBB]", 120.0, 0.0, 0.02}}, Correlation(0.0));
    const CorrelatedBSModelData_ data("basket", settings);
    const auto restored = handle_cast<CorrelatedBSModelData_>(JSON::ReadString(JSON::WriteString(data), false));
    ASSERT_TRUE(restored);
    ASSERT_EQ(restored->indices_, data.indices_);
    ASSERT_EQ(restored->spots_, data.spots_);
    ASSERT_EQ(restored->vols_, data.vols_);
    ASSERT_EQ(restored->divs_, data.divs_);
    ASSERT_EQ(restored->parameterLabels_, data.parameterLabels_);
    ASSERT_DOUBLE_EQ(restored->correlations_(0, 1), 0.0);
    auto model = CreateModel<double>(Handle_<ModelData_>(restored));
    const Vector_<> timeline{1.0};
    const Vector_<AAD::SampleDef_> definitions(1);
    model->Allocate(timeline, definitions);
    ASSERT_THROW((void)Script::CreateRNG("sobol", *model, true), Exception_);
    ASSERT_NO_THROW((void)Script::CreateRNG("sobol", *model, false));
    AAD::CorrelatedBlackScholes_<> oneAsset({"EQ[AAA]"}, {100.0}, {0.2}, {0.01}, 0.05, Matrix_<>(1, 1, 1.0));
    oneAsset.Allocate(timeline, definitions);
    ASSERT_NO_THROW((void)Script::CreateRNG("sobol", oneAsset, true));
}

TEST(ModelTest, TestCorrelatedBlackScholesCloneThreadDeterminism) {
    AAD::CorrelatedBlackScholes_<> original({"EQ[AAA]", "EQ[BBB]"}, {100.0, 120.0}, {0.2, 0.3}, {0.01, 0.02}, 0.05, Correlation(0.3));
    auto clone = original.Clone();
    const Vector_<> timeline{0.0, 0.5, 1.0};
    Vector_<AAD::SampleDef_> definitions(3);
    for (auto& definition : definitions)
        definition.indexNames_ = {"EQ[BBB]", "EQ[AAA]"};
    original.Allocate(timeline, definitions);
    clone->Allocate(timeline, definitions);
    original.Init(timeline, definitions);
    clone->Init(timeline, definitions);
    const Vector_<> gaussian{0.1, -0.2, 0.3, -0.4};
    const auto run = [&](const AAD::Model_<double>& model) {
        auto path = Path<double>(definitions);
        model.GeneratePath(gaussian, &path);
        return path[2].observations_;
    };
    auto first = std::async(std::launch::async, [&] { return run(original); });
    auto second = std::async(std::launch::async, [&] { return run(*clone); });
    ASSERT_EQ(first.get(), second.get());
}
