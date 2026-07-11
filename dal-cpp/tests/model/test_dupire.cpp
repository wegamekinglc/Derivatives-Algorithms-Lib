//
// Created by wegam on 2024/8/29.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

#include <dal/math/integral/quadrature.hpp>
#include <dal/model/dupire.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;

namespace {
    class FlatBlackScholesIVS_ final : public AAD::IVS_ {
        double vol_;

    public:
        FlatBlackScholesIVS_(double spot, double rate, double repo, double vol) : IVS_(spot, rate, repo), vol_(vol) {}

        [[nodiscard]] double ImpliedVol(double, double) const override { return vol_; }
    };

    double NormalCdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

    double BlackScholesCallOracle(double spot, double strike, double rate, double repo, double vol, double expiry) {
        const double stdDev = vol * std::sqrt(expiry);
        const double forward = spot * std::exp((rate - repo) * expiry);
        const double d1 = std::log(forward / strike) / stdDev + 0.5 * stdDev;
        return std::exp(-rate * expiry) * (forward * NormalCdf(d1) - strike * NormalCdf(d1 - stdDev));
    }
} // namespace

TEST(ModelTest, TestDupireModelData) {
    auto model_data = DupireModelData_("my_model", 100.0, 0.05, 0.01, Vector_<>(1), Vector_<>(1), Matrix_<>(1, 1));
    auto dst = JSON::WriteString(model_data);

    Handle_<Storable_> rtn = JSON::ReadString(dst, true);
    ASSERT_NEAR(std::dynamic_pointer_cast<const DupireModelData_>(rtn)->spot_, 100.0, 1e-8);
    ASSERT_NEAR(std::dynamic_pointer_cast<const DupireModelData_>(rtn)->rate_, 0.05, 1e-8);
}

TEST(ModelTest, TestDupireMutantModelRename) {
    Vector_<> spots = {90.0, 100.0, 110.0};
    Vector_<> times = {0.25, 0.5, 1.0};
    Matrix_<> vols(3, 3, 0.2);
    DupireModelData_ model_data("my_model", 100.0, 0.05, 0.01, spots, times, vols);
    ModelData_& base = model_data;
    Vector_<Handle_<Slide_>> noSlides;
    std::unique_ptr<ModelData_> renamed(base.MutantModel(String_("renamed"), noSlides));
    auto dup = dynamic_cast<const DupireModelData_*>(renamed.get());
    ASSERT_TRUE(dup != nullptr);
    ASSERT_EQ(dup->Name(), String_("renamed"));
    ASSERT_NEAR(dup->spot_, 100.0, 1e-10);
    ASSERT_NEAR(dup->rate_, 0.05, 1e-10);
    ASSERT_NEAR(dup->repo_, 0.01, 1e-10);
    ASSERT_EQ(dup->spots_, spots);
    ASSERT_EQ(dup->times_, times);
    ASSERT_EQ(dup->vols_.Rows(), vols.Rows());
    ASSERT_EQ(dup->vols_.Cols(), vols.Cols());
    ASSERT_NEAR(dup->vols_(0, 0), 0.2, 1e-10);
}

TEST(ModelTest, TestDupireRateAwareFlatVolRepricesVanillas) {
    constexpr double spot = 100.0;
    constexpr double rate = 0.05;
    constexpr double repo = 0.02;
    constexpr double vol = 0.25;
    constexpr double expiry = 1.5;
    constexpr double localVolTolerance = 3e-6;
    constexpr double repricingTolerance = 6e-2;

    const FlatBlackScholesIVS_ ivs(spot, rate, repo, vol);
    const auto calibration = AAD::DupireCalib(ivs, Vector_<>{60.0, 80.0, 100.0, 120.0, 140.0}, 5.0, Vector_<>{0.25, 0.75, expiry}, 0.25);
    for (const double localVol : calibration.lVols_)
        ASSERT_NEAR(localVol, vol, localVolTolerance);

    // A single exact log-Euler step makes the DAL model expectation one-dimensional. The deterministic
    // quadrature tolerance covers the vanilla payoff kink and is far smaller than a carry-formula error.
    AAD::Dupire_<> model(spot, rate, repo, calibration.spots_, calibration.times_, calibration.lVols_, expiry);
    const Vector_<> timeline{expiry};
    Vector_<AAD::SampleDef_> definitions(1);
    definitions[0].forwardMats_ = Vector_<Vector_<>>(1, Vector_<>{expiry});
    model.Allocate(timeline, definitions);
    model.Init(timeline, definitions);
    ASSERT_EQ(model.SimDim(), 1u);

    AAD::Scenario_<> path;
    AAD::AllocatePath(definitions, path);
    AAD::InitializePath(path);

    constexpr int numQuadratureNodes = 96;
    Vector_<> nodes(numQuadratureNodes);
    Vector_<> weights(numQuadratureNodes);
    Quadrature::NCDFGaussHermiteWeights(&nodes, &weights);
    const Vector_<> strikes{80.0, 100.0, 120.0};
    Vector_<> modelPrices(strikes.size(), 0.0);
    Vector_<> gaussian(model.SimDim());
    for (int i = 0; i < numQuadratureNodes; ++i) {
        gaussian[0] = nodes[i];
        model.GeneratePath(gaussian, &path);
        for (size_t j = 0; j < strikes.size(); ++j)
            modelPrices[j] += weights[i] * std::max(path[0].spot_ - strikes[j], 0.0) / path[0].numeraire_;
    }

    for (size_t i = 0; i < strikes.size(); ++i) {
        const double oracle = BlackScholesCallOracle(spot, strikes[i], rate, repo, vol, expiry);
        ASSERT_NEAR(modelPrices[i], oracle, repricingTolerance) << "strike=" << strikes[i];
    }
}
