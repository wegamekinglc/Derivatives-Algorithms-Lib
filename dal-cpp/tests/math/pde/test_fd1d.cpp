//
// Created by dal-implementer on 2026-6-28.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/pde/fd1d.hpp>
#include <dal/math/pde/meshers/uniform1dmesher.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/algorithms.hpp>

using namespace Dal;
using namespace Dal::PDE;

namespace {
    // Builds a Black-Scholes-like FD1D_ over a uniform spot grid. Coefficients
    // are time-homogeneous (depend on spot only), so the operator is identical
    // at every timestep.
    struct BlackScholesSetup_ {
        static constexpr int kN = 41;
        static constexpr double kStrike = 100.0;
        static constexpr double kRate = 0.05;
        static constexpr double kVol = 0.2;
        Uniform1DMesher_ x{0.0, 200.0, kN};
        std::unique_ptr<FD1D_> fd;

        BlackScholesSetup_() {
            fd = std::make_unique<FD1D_>(x);
            fd->Init();
            fd->Mu() = kRate * x.Locations();
            fd->R() = Vector_<>(kN, kRate);
            fd->Var() = kVol * kVol * x.Locations() * x.Locations();
            fd->Res() = Apply([](double s) { return std::max(s - kStrike, 0.0); }, x.Locations());
        }
    };
} // namespace

TEST(FD1DTest, TestInitZeroDecompositions) {
    BlackScholesSetup_ s;
    // After Init, no implicit decomposition has been built yet.
    ASSERT_EQ(s.fd->DecompositionsSinceInit(), 0);
}

TEST(FD1DTest, TestTimeHomogeneousReusesDecomposition) {
    BlackScholesSetup_ s;
    const double dt = 0.01;
    const double theta = 0.5;

    for (int n = 0; n < 50; ++n)
        s.fd->RollBwd(dt, theta, s.fd->Res());

    // Time-homogeneous coefficients + constant (dt, theta): the implicit
    // operator is byte-identical at every step, so the decomposition must be
    // built exactly once and reused for the remaining 49 steps.
    ASSERT_EQ(s.fd->DecompositionsSinceInit(), 1);

    // Sanity: a positive payoff rolled back should produce a positive price.
    ASSERT_GT(s.fd->Res()[s.kN / 2], 0.0);
}

TEST(FD1DTest, TestChangingCoefficientsBustsCache) {
    BlackScholesSetup_ s;
    const double dt = 0.01;
    const double theta = 0.5;

    s.fd->RollBwd(dt, theta, s.fd->Res());
    ASSERT_EQ(s.fd->DecompositionsSinceInit(), 1);

    // Mutate mu_ (as a time-dependent-coefficient Dupire step would). The
    // cached decomposition is now stale and must be rebuilt.
    s.fd->Mu()[s.kN / 2] += 1.0;
    s.fd->RollBwd(dt, theta, s.fd->Res());
    ASSERT_EQ(s.fd->DecompositionsSinceInit(), 2);

    // Same coefficients again -> cache hit, no new decomposition.
    s.fd->RollBwd(dt, theta, s.fd->Res());
    ASSERT_EQ(s.fd->DecompositionsSinceInit(), 2);
}

TEST(FD1DTest, TestChangingDtBustsCache) {
    BlackScholesSetup_ s;
    const double theta = 0.5;

    s.fd->RollBwd(0.01, theta, s.fd->Res());
    ASSERT_EQ(s.fd->DecompositionsSinceInit(), 1);

    // Different dt -> different implicit operator -> cache miss.
    s.fd->RollBwd(0.02, theta, s.fd->Res());
    ASSERT_EQ(s.fd->DecompositionsSinceInit(), 2);

    // Back to the original dt with unchanged coeffs -> the operator matches
    // the most recent (dt=0.02) cache only if dt matches; here it does not,
    // so a new decomposition is required.
    s.fd->RollBwd(0.01, theta, s.fd->Res());
    ASSERT_EQ(s.fd->DecompositionsSinceInit(), 3);
}

TEST(FD1DTest, TestExplicitThetaSkipsDecomposition) {
    BlackScholesSetup_ s;
    // Pure explicit (theta = 0): only the MultiplyLeft branch runs, no
    // decomposition is needed at all.
    s.fd->RollBwd(0.01, 0.0, s.fd->Res());
    ASSERT_EQ(s.fd->DecompositionsSinceInit(), 0);
}

TEST(FD1DTest, TestReinitClearsCache) {
    BlackScholesSetup_ s;
    s.fd->RollBwd(0.01, 0.5, s.fd->Res());
    ASSERT_EQ(s.fd->DecompositionsSinceInit(), 1);

    s.fd->Init();
    ASSERT_EQ(s.fd->DecompositionsSinceInit(), 0);

    // After re-init the cache is empty, so the next roll decomposes again.
    s.fd->RollBwd(0.01, 0.5, s.fd->Res());
    ASSERT_EQ(s.fd->DecompositionsSinceInit(), 1);
}

TEST(FD1DTest, TestCachedPathByteIdenticalToUncached) {
    // The cached decomposition must produce bit-for-bit identical results to a
    // fresh Decompose() on the same operator. Run the same rollout twice from
    // identical initial conditions: once with cache hits (time-homogeneous
    // coefficients, 1 decomposition total) and once where every step busts the
    // cache by calling Init() between rolls (which clears the cache but leaves
    // the operator byte-identical since coefficients are re-set the same way).
    // The final states must be identical.
    const double dt = 0.01;
    const double theta = 0.5;
    const int steps = 30;

    auto setup = [](FD1D_& fd, const Uniform1DMesher_& x, const Vector_<>& v0) {
        fd.Init();
        fd.Mu() = BlackScholesSetup_::kRate * x.Locations();
        fd.R() = Vector_<>(BlackScholesSetup_::kN, BlackScholesSetup_::kRate);
        fd.Var() = BlackScholesSetup_::kVol * BlackScholesSetup_::kVol * x.Locations() * x.Locations();
        fd.Res() = v0;
    };

    Uniform1DMesher_ x(0.0, 200.0, BlackScholesSetup_::kN);
    Vector_<> v0 = Apply(
        [](double s) { return std::max(s - BlackScholesSetup_::kStrike, 0.0); }, x.Locations());

    // Cached rollout: 1 decomposition shared across all steps.
    Vector_<> cached;
    {
        FD1D_ fd(x);
        setup(fd, x, v0);
        for (int n = 0; n < steps; ++n)
            fd.RollBwd(dt, theta, fd.Res());
        cached = fd.Res();
        ASSERT_EQ(fd.DecompositionsSinceInit(), 1);
    }

    // Bust-per-step rollout: Init() clears the cache before each roll, so each
    // step performs a fresh Decompose() on the same operator. (Init() also
    // resets the decomposition counter, so the count after the loop reflects
    // only the final step.)
    Vector_<> busted;
    {
        FD1D_ fd(x);
        setup(fd, x, v0);
        for (int n = 0; n < steps; ++n) {
            fd.Init();
            fd.Mu() = BlackScholesSetup_::kRate * x.Locations();
            fd.R() = Vector_<>(BlackScholesSetup_::kN, BlackScholesSetup_::kRate);
            fd.Var() = BlackScholesSetup_::kVol * BlackScholesSetup_::kVol * x.Locations() * x.Locations();
            fd.RollBwd(dt, theta, fd.Res());
        }
        busted = fd.Res();
    }

    ASSERT_EQ(cached.size(), busted.size());
    for (int i = 0; i < static_cast<int>(cached.size()); ++i)
        ASSERT_DOUBLE_EQ(cached[i], busted[i]);
}
