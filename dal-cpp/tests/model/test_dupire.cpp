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
#include <dal/utilities/exceptions.hpp>

using namespace Dal;

namespace {
    class FlatBlackScholesIVS_ final : public AAD::IVS_ {
        double vol_;

    public:
        FlatBlackScholesIVS_(double spot, double rate, double repo, double vol) : IVS_(spot, rate, repo), vol_(vol) {}

        [[nodiscard]] double ImpliedVol(double, double) const override { return vol_; }
    };

    // Strike-independent implied vol sqrt(level + slope * T); the Dupire inversion must
    // recover the analytic local vol sqrt(level + 2 * slope * T) at every strike.
    class TermStructureIVS_ final : public AAD::IVS_ {
        double varianceLevel_;
        double varianceSlope_;

    public:
        TermStructureIVS_(double spot, double rate, double repo, double level, double slope)
            : IVS_(spot, rate, repo), varianceLevel_(level), varianceSlope_(slope) {}

        [[nodiscard]] double ImpliedVol(double, double mat) const override {
            return std::sqrt(varianceLevel_ + varianceSlope_ * mat);
        }
    };

    // Implied vol ~ T^(-3/4), so total variance sigma^2 * T ~ T^(-1/2) decreases with
    // maturity: a calendar-arbitrage surface for which the Dupire numerator is negative.
    class CalendarArbitrageIVS_ final : public AAD::IVS_ {
        double vol_;

    public:
        CalendarArbitrageIVS_(double spot, double vol) : IVS_(spot), vol_(vol) {}

        [[nodiscard]] double ImpliedVol(double, double mat) const override {
            return vol_ * std::pow(mat, -0.75);
        }
    };

    double NormalCdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

    double BlackScholesCallOracle(double spot, double strike, double rate, double repo, double vol, double expiry) {
        const double stdDev = vol * std::sqrt(expiry);
        const double forward = spot * std::exp((rate - repo) * expiry);
        const double d1 = std::log(forward / strike) / stdDev + 0.5 * stdDev;
        return std::exp(-rate * expiry) * (forward * NormalCdf(d1) - strike * NormalCdf(d1 - stdDev));
    }

    // Gauss-Hermite price of a European call on a (possibly multi-step) Dupire_ model. With a
    // flat local-vol surface every log-Euler step is exact, so feeding gaussian[j] = x / sqrt(N)
    // across the N = SimDim() uniform steps reproduces the exact N(0, T) terminal move and the
    // one-dimensional quadrature remains exact up to its own (tiny) truncation error.
    double DupireQuadratureCallPrice(AAD::Dupire_<>* model,
                                     const Vector_<AAD::SampleDef_>& definitions,
                                     double strike,
                                     double gaussianScale) {
        AAD::Scenario_<> path;
        AAD::AllocatePath(definitions, path);
        AAD::InitializePath(path);

        constexpr int numQuadratureNodes = 96;
        Vector_<> nodes(numQuadratureNodes);
        Vector_<> weights(numQuadratureNodes);
        Quadrature::NCDFGaussHermiteWeights(&nodes, &weights);

        Vector_<> gaussian(model->SimDim());
        double price = 0.0;
        for (int i = 0; i < numQuadratureNodes; ++i) {
            for (size_t j = 0; j < gaussian.size(); ++j)
                gaussian[j] = nodes[i] * gaussianScale;
            model->GeneratePath(gaussian, &path);
            price += weights[i] * std::max(path[0].spot_ - strike, 0.0) / path[0].numeraire_;
        }
        return price;
    }

    size_t FindIndex(const Vector_<>& xs, double x) {
        return static_cast<size_t>(std::lower_bound(xs.begin(), xs.end(), x) - xs.begin());
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

TEST(ModelTest, TestDupireCalibRecoversTermStructureLocalVol) {
    constexpr double spot = 100.0;
    constexpr double rate = 0.05;
    constexpr double repo = 0.02;
    constexpr double varianceLevel = 0.04;
    constexpr double varianceSlope = 0.02;
    constexpr double localVolTolerance = 1e-4;

    const TermStructureIVS_ ivs(spot, rate, repo, varianceLevel, varianceSlope);
    const Vector_<> inclSpots{80.0, 100.0, 120.0};
    const Vector_<> inclTimes{0.5, 1.0, 1.5};
    const auto calibration = AAD::DupireCalib(ivs, inclSpots, 5.0, inclTimes, 0.25);

    for (size_t j = 0; j < calibration.times_.size(); ++j) {
        const double expected = std::sqrt(varianceLevel + 2.0 * varianceSlope * calibration.times_[j]);
        for (size_t i = 0; i < calibration.spots_.size(); ++i) {
            ASSERT_NEAR(calibration.lVols_(static_cast<int>(i), static_cast<int>(j)), expected, localVolTolerance)
                << "spot=" << calibration.spots_[i] << " time=" << calibration.times_[j];
        }
    }
}

TEST(ModelTest, TestDupireCalibGridFillRespectsInclusionsAndMaxSpacing) {
    const FlatBlackScholesIVS_ ivs(100.0, 0.05, 0.02, 0.2);
    const Vector_<> inclSpots{80.0, 100.0, 120.0};
    const Vector_<> inclTimes{0.5, 1.5};
    constexpr double maxDs = 5.0;
    constexpr double maxDt = 0.25;

    const auto calibration = AAD::DupireCalib(ivs, inclSpots, maxDs, inclTimes, maxDt);

    ASSERT_TRUE(std::is_sorted(calibration.spots_.begin(), calibration.spots_.end()));
    ASSERT_TRUE(std::is_sorted(calibration.times_.begin(), calibration.times_.end()));
    for (const double s : inclSpots)
        ASSERT_TRUE(std::binary_search(calibration.spots_.begin(), calibration.spots_.end(), s)) << "spot=" << s;
    for (const double t : inclTimes)
        ASSERT_TRUE(std::binary_search(calibration.times_.begin(), calibration.times_.end(), t)) << "time=" << t;
    for (size_t i = 1; i < calibration.spots_.size(); ++i)
        ASSERT_LE(calibration.spots_[i] - calibration.spots_[i - 1], maxDs + 1e-9);
    for (size_t j = 1; j < calibration.times_.size(); ++j)
        ASSERT_LE(calibration.times_[j] - calibration.times_[j - 1], maxDt + 1e-9);

    ASSERT_EQ(calibration.lVols_.Rows(), static_cast<int>(calibration.spots_.size()));
    ASSERT_EQ(calibration.lVols_.Cols(), static_cast<int>(calibration.times_.size()));
}

TEST(ModelTest, TestDupireCalibLocalVolBoundedAcrossGridDensity) {
    constexpr double vol = 0.25;
    constexpr double localVolTolerance = 2e-6;
    const FlatBlackScholesIVS_ ivs(100.0, 0.05, 0.02, vol);
    const Vector_<> inclSpots{80.0, 100.0, 120.0};
    const Vector_<> inclTimes{0.5, 1.0, 1.5};

    const auto coarse = AAD::DupireCalib(ivs, inclSpots, 10.0, inclTimes, 0.5);
    const auto fine = AAD::DupireCalib(ivs, inclSpots, 2.5, inclTimes, 0.125);

    for (const double localVol : coarse.lVols_)
        ASSERT_NEAR(localVol, vol, localVolTolerance);
    for (const double localVol : fine.lVols_)
        ASSERT_NEAR(localVol, vol, localVolTolerance);

    // The inversion is pointwise in (spot, time), so shared grid nodes must agree exactly
    // across densities: refinement changes coverage, never calibrated values.
    const size_t coarseSpot = FindIndex(coarse.spots_, 100.0);
    const size_t fineSpot = FindIndex(fine.spots_, 100.0);
    const size_t coarseTime = FindIndex(coarse.times_, 1.0);
    const size_t fineTime = FindIndex(fine.times_, 1.0);
    ASSERT_LT(coarseSpot, coarse.spots_.size());
    ASSERT_LT(fineSpot, fine.spots_.size());
    ASSERT_LT(coarseTime, coarse.times_.size());
    ASSERT_LT(fineTime, fine.times_.size());
    ASSERT_DOUBLE_EQ(coarse.spots_[coarseSpot], 100.0);
    ASSERT_DOUBLE_EQ(fine.spots_[fineSpot], 100.0);
    ASSERT_DOUBLE_EQ(coarse.times_[coarseTime], 1.0);
    ASSERT_DOUBLE_EQ(fine.times_[fineTime], 1.0);
    ASSERT_DOUBLE_EQ(coarse.lVols_(static_cast<int>(coarseSpot), static_cast<int>(coarseTime)),
                     fine.lVols_(static_cast<int>(fineSpot), static_cast<int>(fineTime)));
}

TEST(ModelTest, TestDupireRepricingStableAcrossTimeSteps) {
    constexpr double spot = 100.0;
    constexpr double rate = 0.05;
    constexpr double repo = 0.02;
    constexpr double vol = 0.25;
    constexpr double expiry = 1.5;
    constexpr double strike = 100.0;
    constexpr double repricingTolerance = 6e-2;
    constexpr double stabilityTolerance = 1e-3;

    const FlatBlackScholesIVS_ ivs(spot, rate, repo, vol);
    const auto calibration = AAD::DupireCalib(ivs, Vector_<>{60.0, 80.0, 100.0, 120.0, 140.0}, 5.0, Vector_<>{0.25, 0.75, expiry}, 0.25);
    const double oracle = BlackScholesCallOracle(spot, strike, rate, repo, vol, expiry);

    Vector_<> prices;
    for (const int numSteps : {1, 4, 16}) {
        const double maxDt = expiry / numSteps;
        AAD::Dupire_<> model(spot, rate, repo, calibration.spots_, calibration.times_, calibration.lVols_, maxDt);
        const Vector_<> timeline{expiry};
        Vector_<AAD::SampleDef_> definitions(1);
        definitions[0].forwardMats_ = Vector_<Vector_<>>(1, Vector_<>{expiry});
        model.Allocate(timeline, definitions);
        model.Init(timeline, definitions);
        ASSERT_EQ(model.SimDim(), static_cast<size_t>(numSteps));

        const double price = DupireQuadratureCallPrice(&model, definitions, strike, 1.0 / std::sqrt(numSteps));
        ASSERT_NEAR(price, oracle, repricingTolerance) << "numSteps=" << numSteps;
        prices.push_back(price);
    }
    ASSERT_NEAR(prices[0], prices[1], stabilityTolerance);
    ASSERT_NEAR(prices[1], prices[2], stabilityTolerance);
}

TEST(ModelTest, TestDupireMutantModelRejectsSlides) {
    Vector_<> spots = {90.0, 100.0, 110.0};
    Vector_<> times = {0.25, 0.5, 1.0};
    Matrix_<> vols(3, 3, 0.2);
    DupireModelData_ model_data("my_model", 100.0, 0.05, 0.01, spots, times, vols);
    ModelData_& base = model_data;

    Vector_<Handle_<Slide_>> slides;
    slides.push_back(Handle_<Slide_>(new Slide_));
    ASSERT_THROW(static_cast<void>(base.MutantModel(String_("renamed"), slides)), Dal::Exception_);
}

TEST(ModelTest, TestDupireCalibRejectsInvalidAxes) {
    const FlatBlackScholesIVS_ ivs(100.0, 0.05, 0.02, 0.2);
    const Vector_<> spots{80.0, 100.0, 120.0};
    const Vector_<> times{0.5, 1.0, 1.5};

    {
        // empty spot axis
        ASSERT_THROW(AAD::DupireCalib(ivs, Vector_<>{}, 5.0, times, 0.25), Dal::Exception_);
    }
    {
        // empty time axis
        ASSERT_THROW(AAD::DupireCalib(ivs, spots, 5.0, Vector_<>{}, 0.25), Dal::Exception_);
    }
    {
        // unsorted spot axis
        ASSERT_THROW(AAD::DupireCalib(ivs, Vector_<>{120.0, 80.0, 100.0}, 5.0, times, 0.25), Dal::Exception_);
    }
    {
        // unsorted time axis
        ASSERT_THROW(AAD::DupireCalib(ivs, spots, 5.0, Vector_<>{1.5, 0.5, 1.0}, 0.25), Dal::Exception_);
    }
}

TEST(ModelTest, TestDupireLocalVolNaNUnderCalendarArbitrage) {
    const CalendarArbitrageIVS_ ivs(100.0, 0.2);

    // The Dupire numerator 2 * dC/dT is negative on a calendar-arbitrage surface, so the
    // inversion yields NaN local vols rather than throwing; this documents that contract.
    ASSERT_TRUE(std::isnan(ivs.LocalVol(100.0, 1.0)));

    const auto calibration = AAD::DupireCalib(ivs, Vector_<>{80.0, 100.0, 120.0}, 10.0, Vector_<>{1.0}, 0.5);
    bool sawNaN = false;
    for (const double localVol : calibration.lVols_)
        sawNaN = sawNaN || std::isnan(localVol);
    ASSERT_TRUE(sawNaN);
}
