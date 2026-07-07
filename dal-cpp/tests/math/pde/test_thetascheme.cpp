//
// Created by dal-implementer on 2026/7/8.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/distribution/black.hpp>
#include <dal/math/pde/pdegrid.hpp>
#include <dal/math/pde/thetascheme.hpp>
#include <dal/protocol/optiontype.hpp>
#include <dal/utilities/exceptions.hpp>

#include <memory>

using namespace Dal;
using namespace Dal::PDE;

namespace {
    constexpr double kRate = 0.05;
    constexpr double kDiv = 0.03;
    constexpr double kVol = 0.15;
    constexpr double kStrike = 120.0;
    constexpr double kSpot = 100.0;
    constexpr double kT = 3.0;
    constexpr double kMaxX = 500.0;

    struct BlackScholesPde_ {
        CoordinateVector_ x;
        Vector_<CoordinateVector_> grids;
        Vector_<> loc;
        Handle_<ScalarCoeff_> disc;
        Handle_<VectorCoeff_> mu;
        Handle_<MatrixCoeff_> var;

        explicit BlackScholesPde_(int n)
            : x(MakeUniformGrid(0.0, kMaxX, n)), grids(1, x), loc(GridLocations(x)), disc(NewConstCoeff(kRate)),
              mu(NewVectorCoeff([](double s) { return (kRate - kDiv) * s; })), var(NewMatrixCoeff([](double s) { return kVol * kVol * s * s; })) {}
    };

    Vector_<std::shared_ptr<Cube_<>>> TerminalCallValue(const Vector_<>& loc) {
        Vector_<std::shared_ptr<Cube_<>>> vals(1, std::make_shared<Cube_<>>(1, 1, static_cast<int>(loc.size())));
        for (int k = 0; k < static_cast<int>(loc.size()); ++k)
            (*vals[0])(0, 0, k) = std::max(loc[k] - kStrike, 0.0);
        return vals;
    }

    void ApplyBoundary(Vector_<std::shared_ptr<Cube_<>>>* vals, int step, double dt) {
        const double tau = step * dt;
        (*(*vals)[0])(0, 0, 0) = 0.0;
        (*(*vals)[0])(0, 0, (*vals)[0]->SizeK() - 1) = kMaxX * std::exp(-kDiv * tau) - std::exp(-kRate * tau) * kStrike;
    }

    Vector_<std::shared_ptr<Cube_<>>> RollCall(const BlackScholesPde_& pde, double theta, int steps) {
        Vector_<std::shared_ptr<Cube_<>>> vals = TerminalCallValue(pde.loc);
        Vector_<std::shared_ptr<Cube_<>>> next(1, std::make_shared<Cube_<>>(1, 1, static_cast<int>(pde.loc.size())));
        ThetaScheme_ scheme(theta);
        const double dt = kT / steps;
        scheme.Prepare(dt, pde.grids, *pde.disc, *pde.mu, *pde.var);
        for (int n = 0; n < steps; ++n) {
            ApplyBoundary(&next, n + 1, dt);
            scheme(dt, pde.grids, vals, *pde.disc, *pde.mu, *pde.var, &next);
            vals.Swap(&next);
        }
        return vals;
    }

    int SpotIndex(const Vector_<>& loc) {
        for (int i = 0; i < static_cast<int>(loc.size()); ++i)
            if (loc[i] == kSpot)
                return i;
        THROW("spot must land exactly on the test grid");
    }
} // namespace

TEST(ThetaSchemeTest, TestPrepareAndRollReuseSingleDecomposition) {
    BlackScholesPde_ pde(41);
    auto vals = TerminalCallValue(pde.loc);
    vals.push_back(std::make_shared<Cube_<>>(1, 1, static_cast<int>(pde.loc.size())));
    for (int k = 0; k < static_cast<int>(pde.loc.size()); ++k)
        (*vals[1])(0, 0, k) = (*vals[0])(0, 0, k) + 1.0;

    ThetaScheme_ scheme(0.5);
    const double dt = 0.01;
    scheme.Prepare(dt, pde.grids, *pde.disc, *pde.mu, *pde.var);
    ASSERT_EQ(scheme.Decompositions(), 1);

    for (int n = 0; n < 10; ++n)
        scheme(dt, pde.grids, vals, *pde.disc, *pde.mu, *pde.var, &vals);
    ASSERT_EQ(scheme.Decompositions(), 1);
    ASSERT_GT((*vals[0])(0, 0, pde.loc.size() / 2), 0.0);
}

TEST(ThetaSchemeTest, TestExplicitThetaSkipsDecomposition) {
    BlackScholesPde_ pde(41);
    auto vals = TerminalCallValue(pde.loc);
    ThetaScheme_ scheme(0.0);
    const double dt = 0.0001;
    scheme.Prepare(dt, pde.grids, *pde.disc, *pde.mu, *pde.var);
    ASSERT_EQ(scheme.Decompositions(), 0);
    scheme(dt, pde.grids, vals, *pde.disc, *pde.mu, *pde.var, &vals);
    ASSERT_EQ(scheme.Decompositions(), 0);
}

TEST(ThetaSchemeTest, TestInPlaceAndOutOfPlaceRollsMatchBitwise) {
    BlackScholesPde_ pde(41);
    auto inPlace = TerminalCallValue(pde.loc);
    auto outSource = TerminalCallValue(pde.loc);
    Vector_<std::shared_ptr<Cube_<>>> outTarget(1, nullptr);

    ThetaScheme_ scheme(0.5);
    const double dt = 0.01;
    scheme.Prepare(dt, pde.grids, *pde.disc, *pde.mu, *pde.var);
    scheme(dt, pde.grids, inPlace, *pde.disc, *pde.mu, *pde.var, &inPlace);
    scheme(dt, pde.grids, outSource, *pde.disc, *pde.mu, *pde.var, &outTarget);

    ASSERT_NE(outTarget[0], nullptr);
    ASSERT_EQ(outTarget[0]->SizeI(), 1);
    ASSERT_EQ(outTarget[0]->SizeJ(), 1);
    ASSERT_EQ(outTarget[0]->SizeK(), static_cast<int>(pde.loc.size()));
    for (int k = 0; k < static_cast<int>(pde.loc.size()); ++k)
        ASSERT_DOUBLE_EQ((*inPlace[0])(0, 0, k), (*outTarget[0])(0, 0, k));
}

TEST(ThetaSchemeTest, TestCrankNicolsonUsesTargetBoundaryInImplicitHalfStep) {
    const CoordinateVector_ x = MakeUniformGrid(0.0, 2.0, 3);
    const Vector_<CoordinateVector_> grids(1, x);

    const Handle_<ScalarCoeff_> disc(NewConstCoeff(0.0));
    const Handle_<VectorCoeff_> mu(NewConstCoeff(Vector_<>{0.0}));
    Matrix_<> variance(1, 1);
    variance(0, 0) = 2.0;
    const Handle_<MatrixCoeff_> var(NewConstCoeff(variance));

    Vector_<std::shared_ptr<Cube_<>>> oldVals(1, std::make_shared<Cube_<>>(1, 1, 3));
    for (int k = 0; k < 3; ++k)
        (*oldVals[0])(0, 0, k) = 0.0;

    Vector_<std::shared_ptr<Cube_<>>> newVals(1, std::make_shared<Cube_<>>(1, 1, 3));
    (*newVals[0])(0, 0, 0) = 0.0;
    (*newVals[0])(0, 0, 1) = -999.0;
    (*newVals[0])(0, 0, 2) = 10.0;

    ThetaScheme_ scheme(0.5);
    const double dt = 0.1;
    scheme.Prepare(dt, grids, *disc, *mu, *var);
    scheme(dt, grids, oldVals, *disc, *mu, *var, &newVals);

    ASSERT_EQ((*newVals[0])(0, 0, 0), 0.0);
    ASSERT_EQ((*newVals[0])(0, 0, 2), 10.0);
    ASSERT_NEAR((*newVals[0])(0, 0, 1), 0.5 / 1.1, 1e-12);
}

TEST(ThetaSchemeTest, TestPreparedStateValidationThrowsOnStaleInputs) {
    BlackScholesPde_ pde(41);
    auto vals = TerminalCallValue(pde.loc);
    ThetaScheme_ scheme(0.5);
    const double dt = 0.01;

    ASSERT_THROW(scheme(dt, pde.grids, vals, *pde.disc, *pde.mu, *pde.var, &vals), Exception_);

    scheme.Prepare(dt, pde.grids, *pde.disc, *pde.mu, *pde.var);
    ASSERT_THROW(scheme(0.02, pde.grids, vals, *pde.disc, *pde.mu, *pde.var, &vals), Exception_);

    const Handle_<ScalarCoeff_> otherDisc(NewConstCoeff(kRate));
    ASSERT_THROW(scheme(dt, pde.grids, vals, *otherDisc, *pde.mu, *pde.var, &vals), Exception_);

    double mutableRate = kRate;
    const Handle_<ScalarCoeff_> timeDependentDisc(NewScalarCoeff([&](double) { return mutableRate; }));
    ThetaScheme_ guarded(0.5);
    guarded.Prepare(dt, pde.grids, *timeDependentDisc, *pde.mu, *pde.var);
    mutableRate = 0.07;
    ASSERT_THROW(guarded(dt, pde.grids, vals, *timeDependentDisc, *pde.mu, *pde.var, &vals), Exception_);
}

TEST(ThetaSchemeTest, TestCrankNicolsonEuropeanCallMatchesBlackPrice) {
    const int numX = 201;
    const int numT = 200;
    BlackScholesPde_ pde(numX);
    const auto vals = RollCall(pde, 0.5, numT);
    const int spotIndex = SpotIndex(pde.loc);
    const double calculated = (*vals[0])(0, 0, spotIndex);

    const double discounts = std::exp(-kRate * kT);
    const double fwd = std::exp((kRate - kDiv) * kT) * kSpot;
    const double volStd = std::sqrt(kT) * kVol;
    const double benchmark = discounts * Distribution::BlackOpt(fwd, volStd, kStrike, OptionType_::Value_::CALL);
    ASSERT_NEAR(calculated, benchmark, 0.05);
}
