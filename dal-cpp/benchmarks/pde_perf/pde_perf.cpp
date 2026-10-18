// PDE time-stepping micro-benchmark.
// Prices a European call via ThetaScheme_ with explicit, Crank-Nicolson, and
// implicit schemes. Each timed iteration rebuilds the grid, prepares the
// operator, and runs the full rollback loop.

#include <dal/platform/platform.hpp>

#include <dal/benchmarks/bench.hpp>
#include <dal/math/pde/earlyexercisepde.hpp>
#include <dal/math/pde/pdegrid.hpp>
#include <dal/math/pde/thetascheme.hpp>
#include <dal/math/vectors.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

using namespace Dal;
using namespace Dal::PDE;

namespace {
    constexpr int kSpaceSteps = 200;
    constexpr int kTimeSteps = 200;
    constexpr int kExplicitTimeSteps = 2000;
    constexpr double kStrike = 120.0;
    constexpr double kRate = 0.05;
    constexpr double kDiv = 0.03;
    constexpr double kVol = 0.15;
    constexpr double kT = 3.0;

    struct SchemeCase_ {
        const char* name;
        double theta;
        int timeSteps;
    };

    const SchemeCase_ kSchemeCases[] = {
        {"ThetaScheme rollback (200x2000 explicit)", 0.0, kExplicitTimeSteps},
        {"ThetaScheme rollback (200x200 CN)", 0.5, kTimeSteps},
        {"ThetaScheme rollback (200x200 implicit)", 1.0, kTimeSteps},
    };

    double PriceEuropeanCall(double theta, int timeSteps) {
        const double minX = 0.0;
        const double maxX = 500.0;
        const int numX = kSpaceSteps + 1;
        const CoordinateVector_ x = MakeUniformGrid(minX, maxX, numX);
        const Vector_<CoordinateVector_> grids(1, x);
        const Vector_<> loc = GridLocations(x);

        Vector_<std::shared_ptr<Cube_<>>> vals(1, std::make_shared<Cube_<>>(1, 1, numX));
        for (int k = 0; k < numX; ++k)
            (*vals[0])(0, 0, k) = std::max(loc[k] - kStrike, 0.0);
        Vector_<std::shared_ptr<Cube_<>>> next(1, std::make_shared<Cube_<>>(1, 1, numX));

        const Handle_<ScalarCoeff_> disc(NewConstCoeff(kRate));
        const Handle_<VectorCoeff_> mu(NewVectorCoeff([](double s) { return (kRate - kDiv) * s; }));
        const Handle_<MatrixCoeff_> var(NewMatrixCoeff([](double s) { return kVol * kVol * s * s; }));
        ThetaScheme_ scheme(theta);
        const double dt = kT / timeSteps;
        scheme.Prepare(dt, grids, *disc, *mu, *var);
        for (int n = 0; n < timeSteps; ++n) {
            (*next[0])(0, 0, 0) = 0.0;
            (*next[0])(0, 0, numX - 1) = maxX * std::exp(-kDiv * (n + 1) * dt) - std::exp(-kRate * (n + 1) * dt) * kStrike;
            scheme(dt, grids, vals, *disc, *mu, *var, &next);
            vals.Swap(&next);
        }
        return (*vals[0])(0, 0, numX / 2);
    }

    EarlyExercisePdeRequest_ EarlyRequest(const char* mode) {
        EarlyExercisePdeRequest_ r;
        r.contract_ = {Date_(2026, 10, 18), Date_(2027, 10, 18), 100.0, OptionType_("PUT"), EarlyExerciseStyle_("BERMUDAN"), {}};
        r.market_ = {100.0, 0.2, {{r.contract_.maturityDate_, 0.05, 0.0}}};
        if (std::string(mode) == "American" || std::string(mode) == "NearZero")
            r.contract_.exerciseStyle_ = EarlyExerciseStyle_("AMERICAN");
        else if (std::string(mode) == "Daily") {
            for (auto date = r.contract_.evaluationDate_; date <= r.contract_.maturityDate_; ++date)
                r.contract_.exerciseDates_.push_back(date);
        } else
            r.contract_.exerciseDates_ = {Date_(2027, 3, 4), r.contract_.maturityDate_};
        if (std::string(mode) == "NearZero") {
            r.market_.volatility_ = 1e-8;
            r.market_.spot_ = 80.0;
            r.market_.carry_.front().rate_ = -0.2;
            r.market_.carry_.front().dividendYield_ = -0.1;
            r.settings_.theta_ = 0.5;
            r.settings_.ordinarySteps_ = 1;
            r.settings_.rannacherIntervals_ = 0;
        }
        return r;
    }
} // namespace

int main(int argc, char** argv) {
    Dal::RegisterAll_::Init();
    constexpr int kRepeats = 10;
    Bench::PrintHeader();

    if (argc == 2 && std::string(argv[1]) == "--early-exercise") {
        for (const char* name : {"Bermudan", "American", "Daily", "NearZero"}) {
            const auto request = EarlyRequest(name);
            double sink = 0.0;
            auto timing = Bench::Run(
                name,
                [&]() {
                    const auto result = EvaluateEarlyExercisePde(request);
                    REQUIRE(result.IsSuccess(), "early exercise performance request failed");
                    sink += result.Price();
                },
                2, kRepeats);
            Bench::Print(timing);
            const auto result = EvaluateEarlyExercisePde(request);
            const auto& e = result.Execution();
            std::cout << name << " price=" << result.Price() << " steps=" << e.actualSteps_ << " solves=" << e.totalSolves_
                      << " work=" << e.workUnits_ << " payload=" << e.numericPayloadBytes_ << " workspace=" << e.peakWorkspaceBytes_ << '\n';
            Bench::DoNotOptimize(&sink);
        }
        return 0;
    }

    for (const SchemeCase_& schemeCase : kSchemeCases) {
        double sink = 0.0;
        auto r = Bench::Run(schemeCase.name, [&]() { sink += PriceEuropeanCall(schemeCase.theta, schemeCase.timeSteps); }, 2, kRepeats);
        Bench::Print(r);
        Bench::DoNotOptimize(&sink);
    }

    return 0;
}
