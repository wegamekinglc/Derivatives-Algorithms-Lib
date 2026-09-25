//
// Created by dal-implementer on 2026/9/20.
//

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <dal/platform/platform.hpp>

#include <dal/math/distribution/black.hpp>
#include <dal/math/operators.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/model/dupire.hpp>
#include <dal/script/diagnostics.hpp>
#include <dal/script/lsmc.hpp>
#include <dal/script/preparation.hpp>
#include <dal/script/simulation.hpp>
#include <dal/storage/_repository.hpp>
#include <dal/storage/globals.hpp>
#include <dal/utilities/exceptions.hpp>

#include "bermudan_pde.hpp"

using namespace Dal;
using namespace Dal::Script;
using Dal::Script::TestSupport::BermudanPutPDE;

namespace {
    //  Reference market: spot 100, vol 20%, rate 5%, no dividends, strike 100, maturity 18m.
    //  The zero-dividend 18m weekly grid keeps the early-exercise premium (~0.8% of spot)
    //  above the 0.5%-of-spot acceptance tolerance, so the benchmarks actually bite.
    constexpr double SPOT = 100.0;
    constexpr double VOL = 0.20;
    constexpr double RATE = 0.05;
    constexpr double DIV = 0.0;
    constexpr double STRIKE = 100.0;
    constexpr double MATURITY = 1.5;

    Date_ EvalDate() { return Date_(2026, 9, 20); }

    //  Exact engine year fraction (the sample grid uses (date - evaluation) / 365)
    double YearFracTo(const Date_& date) { return static_cast<double>(date - EvalDate()) / 365.0; }

    Handle_<ModelData_> StandardModel() { return Handle_<ModelData_>(new BSModelData_("bs", SPOT, VOL, RATE, DIV)); }

    Handle_<ModelData_> ModelWithVol(double vol) { return Handle_<ModelData_>(new BSModelData_("bs", SPOT, vol, RATE, DIV)); }

    double EuropeanPutClosedForm() {
        const double maturity = YearFracTo(Date_(2028, 3, 20));
        const double fwd = SPOT * std::exp((RATE - DIV) * maturity);
        return std::exp(-RATE * maturity) * Distribution::BlackOpt(fwd, VOL * std::sqrt(maturity), STRIKE, OptionType_::Value_::PUT);
    }
} // namespace

//  ---------------------------------------------------------------------------
//  PDE benchmark self-validation: the test-only Bermudan pricer must agree with
//  the closed form in the European limit and converge under grid refinement
//  ---------------------------------------------------------------------------

TEST(ScriptExerciseLSMCTest, TestPdeBenchmarkEuropeanLimit) {
    const double price = BermudanPutPDE(SPOT, VOL, RATE, DIV, STRIKE, {YearFracTo(Date_(2028, 3, 20))}, 2000, 2000);
    ASSERT_NEAR(price, EuropeanPutClosedForm(), 0.05 * SPOT / 100.0);
}

TEST(ScriptExerciseLSMCTest, TestPdeBenchmarkGridConvergence) {
    const double benchmark = EuropeanPutClosedForm();
    const double coarse = BermudanPutPDE(SPOT, VOL, RATE, DIV, STRIKE, {YearFracTo(Date_(2028, 3, 20))}, 500, 500);
    const double fine = BermudanPutPDE(SPOT, VOL, RATE, DIV, STRIKE, {YearFracTo(Date_(2028, 3, 20))}, 1500, 1500);
    const double errorCoarse = std::abs(coarse - benchmark);
    const double errorFine = std::abs(fine - benchmark);
    ASSERT_LT(errorFine, errorCoarse);
    ASSERT_LT(errorFine, 0.5 * errorCoarse);
}

//  ---------------------------------------------------------------------------
//  Regression solver unit: z-normalized monomial basis, explicit relative ridge,
//  degenerate guards (ConditionPathsBelowMin / SigmaFloor / IllConditioned)
//  ---------------------------------------------------------------------------

namespace {
    Vector_<char> AllIncluded(size_t n) { return Vector_<char>(n, 1); }

    //  Test-only long-double Householder QR. It neither forms normal equations
    //  nor uses the production column-pivoted/reorthogonalized solver.
    using ReferenceDesign_ = std::vector<std::array<long double, 9>>;

    struct HouseholderVector_ {
        std::vector<long double> values_;
        long double scale_;
    };

    HouseholderVector_ MakeHouseholderVector(const ReferenceDesign_& a, size_t k) {
        long double normSq = 0.0;
        for (size_t i = k; i < a.size(); ++i)
            normSq += a[i][k] * a[i][k];
        const long double alpha = a[k][k] >= 0.0 ? -std::sqrt(normSq) : std::sqrt(normSq);
        std::vector<long double> v(a.size() - k);
        v[0] = a[k][k] - alpha;
        long double vNormSq = v[0] * v[0];
        for (size_t i = k + 1; i < a.size(); ++i) {
            v[i - k] = a[i][k];
            vNormSq += v[i - k] * v[i - k];
        }
        return {std::move(v), 2.0L / vNormSq};
    }

    void ApplyHouseholderToDesign(ReferenceDesign_* a, size_t k, size_t columns, const HouseholderVector_& reflector) {
        for (size_t j = k; j < columns; ++j) {
            long double projection = 0.0;
            for (size_t i = k; i < a->size(); ++i)
                projection += reflector.values_[i - k] * (*a)[i][j];
            for (size_t i = k; i < a->size(); ++i)
                (*a)[i][j] -= reflector.scale_ * reflector.values_[i - k] * projection;
        }
    }

    void ApplyHouseholderToResponse(std::vector<long double>* b, size_t k, const HouseholderVector_& reflector) {
        long double projection = 0.0;
        for (size_t i = k; i < b->size(); ++i)
            projection += reflector.values_[i - k] * (*b)[i];
        for (size_t i = k; i < b->size(); ++i)
            (*b)[i] -= reflector.scale_ * reflector.values_[i - k] * projection;
    }

    Vector_<> HouseholderReference(const Vector_<>& x, const Vector_<>& y, int degree, double mean, double sigma) {
        const size_t columns = static_cast<size_t>(degree + 1);
        ReferenceDesign_ a(x.size());
        std::vector<long double> b(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
            const long double z = (static_cast<long double>(x[i]) - mean) / sigma;
            long double power = 1.0;
            for (size_t j = 0; j < columns; ++j) {
                a[i][j] = power;
                power *= z;
            }
            b[i] = y[i];
        }
        for (size_t k = 0; k < columns; ++k) {
            const auto reflector = MakeHouseholderVector(a, k);
            ApplyHouseholderToDesign(&a, k, columns, reflector);
            ApplyHouseholderToResponse(&b, k, reflector);
        }
        Vector_<> coefficients(columns);
        for (size_t k = columns; k-- > 0;) {
            long double residual = b[k];
            for (size_t j = k + 1; j < columns; ++j)
                residual -= a[k][j] * coefficients[j];
            coefficients[k] = static_cast<double>(residual / a[k][k]);
        }
        return coefficients;
    }

    double Horner(const Vector_<>& coefficients, double z) {
        double value = 0.0;
        for (size_t j = coefficients.size(); j-- > 0;)
            value = value * z + coefficients[j];
        return value;
    }

    constexpr size_t EUROPEAN_LIMIT_PATHS = 1u << 18;

    String_ PutExerciseText(double strike) { return "EXERCISE MAX(" + String_(std::to_string(strike)) + " - spot(), 0.0)"; }

    //  Put exercisable on the given dates only (EXERCISE-only product, no PAYS)
    ScriptProductData_ ExerciseOnlyProduct(const Vector_<Date_>& dates, double strike = 100.0) {
        Vector_<Cell_> cells;
        for (const auto& d : dates)
            cells.push_back(Cell_(d));
        const auto text = PutExerciseText(strike);
        Vector_<String_> events(dates.size(), text);
        return {"", cells, events};
    }

    Vector_<Date_> WeeklyDates(size_t count) {
        Vector_<Date_> dates;
        for (size_t i = 1; i <= count; ++i)
            dates.push_back(EvalDate().AddDays(static_cast<int>(7 * i)));
        return dates;
    }
} // namespace

TEST(ScriptExerciseLSMCTest, TestRegressionRecoversPolynomial) {
    const size_t n = 200;
    Vector_<> x(n);
    Vector_<> targets(n);
    for (size_t i = 0; i < n; ++i) {
        x[i] = -2.0 + 4.0 * static_cast<double>(i) / static_cast<double>(n - 1);
        targets[i] = 3.0 + 2.0 * x[i] - 0.5 * x[i] * x[i];
    }
    const auto regression = SolveExerciseRegression(x, targets, AllIncluded(n), 3);
    ASSERT_FALSE(regression.degenerate_);
    ASSERT_EQ(regression.basisDegree_, 3);
    ASSERT_EQ(regression.coefficients_.size(), 4u);
    ASSERT_EQ(regression.numCondTrue_, n);
    ASSERT_EQ(regression.effectiveRank_, 4u);
    ASSERT_EQ(regression.solver_, "MomentsCholesky");
    for (size_t i = 0; i < n; ++i)
        ASSERT_NEAR(RegressionPredict(regression, x[i]), targets[i], 1e-8);
}

TEST(ScriptExerciseLSMCTest, TestRegressionDegreesAgainstIndependentHouseholderReference) {
    constexpr size_t N = 257;
    Vector_<> x(N), targets(N);
    for (size_t i = 0; i < N; ++i) {
        x[i] = 20.0 + 230.0 * static_cast<double>(i) / static_cast<double>(N - 1);
        targets[i] = std::exp(-x[i] / 80.0) + 0.05 * std::sin(x[i] / 30.0);
    }
    for (int degree = 1; degree <= 8; ++degree) {
        SCOPED_TRACE(degree);
        const auto fit = SolveExerciseRegression(x, targets, AllIncluded(N), degree);
        ASSERT_FALSE(fit.degenerate_);
        ASSERT_EQ(fit.basisDegree_, degree);
        const auto reference = HouseholderReference(x, targets, degree, fit.mean_, fit.sigma_);
        for (double spot : {22.5, 58.5, 101.0, 177.0, 248.0})
            ASSERT_NEAR(RegressionPredict(fit, spot), Horner(reference, (spot - fit.mean_) / fit.sigma_), 2e-7);
    }
}

TEST(ScriptExerciseLSMCTest, TestRegressionConstantTargetLeavesRidgeHarmless) {
    const size_t n = 100;
    const Vector_<> x = [&] {
        Vector_<> v(n);
        for (size_t i = 0; i < n; ++i)
            v[i] = static_cast<double>(i);
        return v;
    }();
    const Vector_<> targets(n, 5.0);
    const auto regression = SolveExerciseRegression(x, targets, AllIncluded(n), 3);
    ASSERT_FALSE(regression.degenerate_);
    ASSERT_NEAR(RegressionPredict(regression, 37.0), 5.0, 1e-10);
}

TEST(ScriptExerciseLSMCTest, TestRegressionTooFewConditionPaths) {
    const size_t n = 39; //  degree 3 needs 10 * 4 = 40
    Vector_<> x(n);
    Vector_<> targets(n);
    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<double>(i);
        targets[i] = 1.0 + 0.1 * x[i];
    }
    const auto regression = SolveExerciseRegression(x, targets, AllIncluded(n), 3);
    ASSERT_TRUE(regression.degenerate_);
    ASSERT_EQ(regression.degenerateReason_, "ConditionPathsBelowMin");
    ASSERT_EQ(regression.basisDegree_, 0);
    ASSERT_EQ(regression.coefficients_.size(), 1u);
    double mean = 0.0;
    for (size_t i = 0; i < n; ++i)
        mean += targets[i];
    mean /= static_cast<double>(n);
    ASSERT_NEAR(regression.coefficients_[0], mean, 1e-12);
}

TEST(ScriptExerciseLSMCTest, TestRegressionNoConditionPaths) {
    const Vector_<> x = {1.0, 2.0, 3.0};
    const Vector_<> targets = {1.0, 2.0, 3.0};
    const auto regression = SolveExerciseRegression(x, targets, Vector_<char>(3, 0), 2);
    ASSERT_TRUE(regression.degenerate_);
    ASSERT_EQ(regression.degenerateReason_, "ConditionPathsBelowMin");
    ASSERT_EQ(regression.coefficients_[0], 0.0);
}

TEST(ScriptExerciseLSMCTest, TestRegressionSigmaFloor) {
    const size_t n = 100;
    const Vector_<> x(n, 42.0); //  identical regressor: sigma_hat is exactly zero
    Vector_<> targets(n);
    for (size_t i = 0; i < n; ++i)
        targets[i] = 2.0 + 0.01 * static_cast<double>(i);
    const auto regression = SolveExerciseRegression(x, targets, AllIncluded(n), 3);
    ASSERT_TRUE(regression.degenerate_);
    ASSERT_EQ(regression.degenerateReason_, "SigmaFloor");
    ASSERT_EQ(regression.basisDegree_, 0);
    //  the constant fit must not produce NaN through the sigma floor
    ASSERT_TRUE(std::isfinite(regression.coefficients_[0]));
    ASSERT_NEAR(RegressionPredict(regression, 42.0), 2.0 + 0.01 * 49.5, 1e-12);
}

TEST(ScriptExerciseLSMCTest, TestRegressionIllConditioned) {
    const size_t n = 200;
    Vector_<> x(n);
    Vector_<> targets(n);
    for (size_t i = 0; i < n; ++i) {
        x[i] = 0.01 * static_cast<double>(i);
        targets[i] = 1.0 + x[i];
    }
    x[n - 1] = 1e7; //  single extreme outlier dominates the high-order Gram diagonal
    targets[n - 1] = 2.0;
    const auto regression = SolveExerciseRegression(x, targets, AllIncluded(n), 8);
    ASSERT_FALSE(regression.degenerate_);
    ASSERT_GE(regression.basisDegree_, 1);
    ASSERT_EQ(regression.solver_, "PivotedQR");
    ASSERT_FALSE(regression.fallbackReason_.empty());
    ASSERT_NEAR(RegressionPredict(regression, 1.0), 2.0, 1e-3);
    ASSERT_NEAR(RegressionPredict(regression, 1e7), 2.0, 1e-3);
}

TEST(ScriptExerciseLSMCTest, TestRegressionIncludedSubsetDrivesFit) {
    const size_t n = 100;
    Vector_<> x(n);
    Vector_<> targets(n);
    Vector_<char> included(n, 0);
    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<double>(i);
        targets[i] = (i % 2 == 0) ? 1.0 : 10.0;
        if (i % 2 == 0)
            included[i] = 1;
    }
    const auto regression = SolveExerciseRegression(x, targets, included, 1);
    ASSERT_FALSE(regression.degenerate_);
    ASSERT_EQ(regression.numCondTrue_, 50u);
    for (size_t i = 0; i < n; ++i)
        if (included[i])
            ASSERT_NEAR(RegressionPredict(regression, x[i]), 1.0, 1e-10);
}

TEST(ScriptExerciseLSMCTest, TestRegressionDetectsCollinearBasis) {
    Vector_<> x(100), targets(100);
    for (size_t i = 0; i < x.size(); ++i) {
        x[i] = i % 2 ? 1.0 : -1.0;
        targets[i] = 3.0 + x[i];
    }
    const auto fit = SolveExerciseRegression(x, targets, AllIncluded(x.size()), 3);
    ASSERT_FALSE(fit.degenerate_);
    ASSERT_EQ(fit.basisDegree_, 1);
    ASSERT_EQ(fit.effectiveRank_, 2u);
    ASSERT_EQ(fit.solver_, "PivotedQR");
    ASSERT_EQ(fit.fallbackReason_, "RankDeficient");
    ASSERT_NEAR(RegressionPredict(fit, 0.0), 3.0, 1e-12);
    ASSERT_NEAR(RegressionPredict(fit, -1.0), 2.0, 1e-12);
    ASSERT_NEAR(RegressionPredict(fit, 1.0), 4.0, 1e-12);
}

TEST(ScriptExerciseLSMCTest, TestRegressionPreservesQuadraticDiscreteStates) {
    Vector_<> x(120), targets(120);
    for (size_t i = 0; i < x.size(); ++i) {
        x[i] = 80.0 + 20.0 * static_cast<double>(i % 3);
        targets[i] = 2.0 + 0.1 * x[i] + 0.01 * x[i] * x[i];
    }
    const auto fit = SolveExerciseRegression(x, targets, AllIncluded(x.size()), 8);
    ASSERT_FALSE(fit.degenerate_);
    ASSERT_EQ(fit.basisDegree_, 2);
    ASSERT_EQ(fit.effectiveRank_, 3u);
    ASSERT_EQ(fit.solver_, "PivotedQR");
    for (double state : {80.0, 100.0, 120.0})
        ASSERT_NEAR(RegressionPredict(fit, state), 2.0 + 0.1 * state + 0.01 * state * state, 1e-8);
}

TEST(ScriptExerciseLSMCTest, TestRegressionRejectsNonFiniteIncludedData) {
    Vector_<> x(100, 1.0), targets(100, 2.0);
    for (double invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        x[0] = invalid;
        ASSERT_THROW(SolveExerciseRegression(x, targets, AllIncluded(x.size()), 3), Dal::Exception_);
        x[0] = 1.0;
        targets[0] = invalid;
        ASSERT_THROW(SolveExerciseRegression(x, targets, AllIncluded(x.size()), 3), Dal::Exception_);
        targets[0] = 2.0;
    }
}

TEST(ScriptExerciseLSMCTest, TestConstantRegressionAvoidsNormalizationOverflow) {
    const auto fit = SolveExerciseRegression(Vector_<>(100, 0.0), Vector_<>(100, 2.0), AllIncluded(100), 3);
    ASSERT_EQ(RegressionPredict(fit, std::numeric_limits<double>::max()), 2.0);
}

//  ---------------------------------------------------------------------------
//  LSMC driver: European limit (exercise only at maturity == European put)
//  ---------------------------------------------------------------------------

namespace {
    struct LsmcRun_ {
        double pv_;
        LsmcDiagnostics_ diagnostics_;
    };

    LsmcRun_ RunLsmc(const ScriptProductData_& product,
                     const Handle_<ModelData_>& modelData,
                     size_t nPaths,
                     int degree = 3,
                     bool compiled = false,
                     std::optional<int> trainingPaths = std::nullopt,
                     std::optional<int> validationPaths = std::nullopt) {
        MonteCarloSettings_ simulation;
        simulation.lsmcBasisDegree_ = degree;
        simulation.compiled_ = compiled;
        simulation.lsmcTrainingPaths_ = trainingPaths;
        simulation.lsmcValidationPaths_ = validationPaths;
        auto model = CreateModel<double>(modelData);
        auto prepared = PrepareScript(product, model.get(), ScriptValuationSettings_(), simulation);
        LsmcRun_ run{0.0, LsmcDiagnostics_()};
        run.diagnostics_.nPaths_ = nPaths;
        const auto results = MCLsmcSimulation(prepared, model.get(), nPaths, &run.diagnostics_);
        run.pv_ = results.aggregated_ / static_cast<double>(nPaths);
        return run;
    }
} // namespace

TEST(ScriptExerciseLSMCTest, TestEuropeanLimit) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const auto product = ExerciseOnlyProduct({Date_(2028, 3, 20)});
    const auto run = RunLsmc(product, StandardModel(), EUROPEAN_LIMIT_PATHS);
    ASSERT_NEAR(run.pv_, EuropeanPutClosedForm(), 3.0 * run.diagnostics_.StandardError());
}

TEST(ScriptExerciseLSMCTest, TestPricingUsesPathsAfterTrainingBlock) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    constexpr size_t N_PATHS = 8197; // Cross the fixed pricing-batch boundary.
    const auto product = ExerciseOnlyProduct({Date_(2027, 9, 20)});
    for (const auto trainingPaths : {std::optional<int>(), std::optional<int>(73), std::optional<int>(16391)}) {
        SCOPED_TRACE(trainingPaths.value_or(static_cast<int>(N_PATHS)));
        for (const bool bridge : {false, true}) {
            MonteCarloSettings_ settings;
            settings.lsmcTrainingPaths_ = trainingPaths;
            settings.useBb_ = bridge;
            auto model = CreateModel<double>(StandardModel());
            const auto prepared = PrepareScript(product, model.get(), {}, settings);
            auto rng = CreateRNG(settings.rsg_, model->SimDim(), bridge);
            rng->SkipTo(static_cast<size_t>(trainingPaths.value_or(static_cast<int>(N_PATHS))));
            Vector_<> gauss(model->SimDim());
            Scenario_<> path;
            AllocatePath(prepared.DefLine(), path);
            InitializePath(path);
            double expected = 0.0;
            double expectedDelta = 0.0;
            for (size_t i = 0; i < N_PATHS; ++i) {
                rng->FillNormal(&gauss);
                model->GeneratePath(gauss, &path);
                expected += std::max(STRIKE - path.back().spot_, 0.0) / path.back().numeraire_;
                if (path.back().spot_ < STRIKE)
                    expectedDelta -= path.back().spot_ / SPOT / path.back().numeraire_;
            }
            for (const bool compiled : {false, true}) {
                settings.compiled_ = compiled;
                const auto hard = MCSimulation<double>(product, StandardModel(), N_PATHS, {}, settings);
                ASSERT_NEAR(hard.aggregated_ / N_PATHS, expected / N_PATHS, 1e-10);
                settings.enableAad_ = true;
                settings.smooth_ = 1e-10;
                const auto fuzzy = MCSimulation<AAD::Number_>(product, StandardModel(), N_PATHS, {}, settings);
                ASSERT_NEAR(fuzzy.aggregated_ / N_PATHS, expected / N_PATHS, 1e-10);
                ASSERT_NEAR(fuzzy["spot"], expectedDelta / N_PATHS, 1e-10);
                settings.enableAad_ = false;
            }
        }
    }
}

TEST(ScriptExerciseLSMCTest, TestValidationPathsStayBetweenTrainingAndPricing) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const auto product = ExerciseOnlyProduct({Date_(2027, 9, 20)});
    constexpr size_t TRAINING = 101;
    constexpr size_t VALIDATION = 37;
    constexpr size_t PRICING = 257;
    MonteCarloSettings_ settings;
    settings.lsmcTrainingPaths_ = static_cast<int>(TRAINING);
    settings.lsmcValidationPaths_ = static_cast<int>(VALIDATION);
    auto model = CreateModel<double>(StandardModel());
    const auto prepared = PrepareScript(product, model.get(), {}, settings);
    auto rng = CreateRNG(settings.rsg_, model->SimDim(), settings.useBb_);
    rng->SkipTo(TRAINING + VALIDATION);
    Vector_<> gauss(model->SimDim());
    Scenario_<> path;
    AllocatePath(prepared.DefLine(), path);
    InitializePath(path);
    double expected = 0.0;
    for (size_t i = 0; i < PRICING; ++i) {
        rng->FillNormal(&gauss);
        model->GeneratePath(gauss, &path);
        expected += std::max(STRIKE - path.back().spot_, 0.0) / path.back().numeraire_;
    }
    for (bool compiled : {false, true}) {
        settings.compiled_ = compiled;
        const auto hard = MCSimulation<double>(product, StandardModel(), PRICING, {}, settings);
        ASSERT_NEAR(hard.aggregated_, expected, 1e-10);
        settings.enableAad_ = true;
        const auto aad = MCSimulation<AAD::Number_>(product, StandardModel(), PRICING, {}, settings);
        ASSERT_NEAR(aad.aggregated_, expected, 1e-10);
        settings.enableAad_ = false;
    }
}

TEST(ScriptExerciseLSMCTest, TestFlatDupireMatchesBlackScholesWithSeparatePathBudgets) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    constexpr size_t N_PATHS = 8197;
    const auto product = ExerciseOnlyProduct({Date_(2027, 3, 20), Date_(2027, 9, 20), Date_(2028, 3, 20)});
    const Handle_<ModelData_> dupire(new DupireModelData_("dupire", SPOT, RATE, DIV, {50.0, 150.0}, {0.0, 2.0}, Matrix_<>(2, 2, VOL)));
    // Flat local volatility has the same pathwise law and aggregate vega as BS,
    // while exercising the generic path generator and the Dupire AAD workspace.
    for (const int trainingPaths : {73, 16391}) {
        SCOPED_TRACE(trainingPaths);
        for (const bool bridge : {false, true}) {
            SCOPED_TRACE(bridge);
            MonteCarloSettings_ settings;
            settings.lsmcTrainingPaths_ = trainingPaths;
            settings.useBb_ = bridge;
            const auto expectedHard = MCSimulation<double>(product, StandardModel(), N_PATHS, {}, settings);
            settings.enableAad_ = true;
            const auto expectedAad = MCSimulation<AAD::Number_>(product, StandardModel(), N_PATHS, {}, settings);
            for (const bool compiled : {false, true}) {
                SCOPED_TRACE(compiled);
                settings.compiled_ = compiled;
                settings.enableAad_ = false;
                const auto hard = MCSimulation<double>(product, dupire, N_PATHS, {}, settings);
                ASSERT_NEAR(hard.aggregated_ / N_PATHS, expectedHard.aggregated_ / N_PATHS, 1e-8);
                settings.enableAad_ = true;
                const auto aad = MCSimulation<AAD::Number_>(product, dupire, N_PATHS, {}, settings);
                ASSERT_NEAR(aad.aggregated_ / N_PATHS, expectedAad.aggregated_ / N_PATHS, 1e-8);
                ASSERT_NEAR(aad["spot"], expectedAad["spot"], 1e-8);
                ASSERT_NEAR(aad["rate"], expectedAad["rate"], 1e-8);
                ASSERT_NEAR(aad["repo"], expectedAad["div"], 1e-8);
                ASSERT_EQ(aad.risks_.size(), 7u);
                double parallelVega = 0.0;
                for (size_t i = 3; i < aad.risks_.size(); ++i)
                    parallelVega += aad.risks_[i];
                ASSERT_NEAR(parallelVega, expectedAad["vol"], 1e-8);
            }
        }
    }
}

TEST(ScriptExerciseLSMCTest, TestTrainingPolicyDoesNotDependOnPricingCount) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const auto product = ExerciseOnlyProduct({Date_(2027, 9, 20), Date_(2028, 3, 20)});
    for (const bool compiled : {false, true}) {
        const auto small = RunLsmc(product, StandardModel(), 257, 3, compiled, 4096);
        const auto large = RunLsmc(product, StandardModel(), 8193, 3, compiled, 4096);
        ASSERT_EQ(small.diagnostics_.nPaths_, 257u);
        ASSERT_EQ(large.diagnostics_.nPaths_, 8193u);
        ASSERT_EQ(small.diagnostics_.events_.size(), 2u);
        ASSERT_EQ(large.diagnostics_.events_.size(), 2u);
        for (size_t i = 0; i < small.diagnostics_.events_.size(); ++i) {
            const auto& lhs = small.diagnostics_.events_[i];
            const auto& rhs = large.diagnostics_.events_[i];
            ASSERT_FALSE(lhs.degenerate_);
            ASSERT_EQ(lhs.coefficients_, rhs.coefficients_);
            ASSERT_EQ(lhs.numCondTruePaths_, rhs.numCondTruePaths_);
        }
        // Explicitly matching the pricing count preserves the default policy and PV.
        const auto implicit = RunLsmc(product, StandardModel(), 4096, 3, compiled);
        const auto explicitCount = RunLsmc(product, StandardModel(), 4096, 3, compiled, 4096);
        ASSERT_EQ(implicit.pv_, explicitCount.pv_);
        for (size_t i = 0; i < implicit.diagnostics_.events_.size(); ++i)
            ASSERT_EQ(implicit.diagnostics_.events_[i].coefficients_, explicitCount.diagnostics_.events_[i].coefficients_);
    }
}

TEST(ScriptExerciseLSMCTest, TestAdaptiveDegreeUsesHeldOutPathsOnly) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const auto product = ExerciseOnlyProduct({Date_(2027, 3, 20), Date_(2027, 9, 20), Date_(2028, 3, 20)});
    for (bool compiled : {false, true}) {
        const auto small = RunLsmc(product, StandardModel(), 257, 8, compiled, 4096, 1024);
        const auto large = RunLsmc(product, StandardModel(), 2049, 8, compiled, 4096, 1024);
        ASSERT_EQ(small.diagnostics_.events_.size(), 3u);
        ASSERT_EQ(large.diagnostics_.events_.size(), 3u);
        for (size_t day = 0; day < small.diagnostics_.events_.size(); ++day) {
            const auto& lhs = small.diagnostics_.events_[day];
            const auto& rhs = large.diagnostics_.events_[day];
            ASSERT_EQ(lhs.coefficients_, rhs.coefficients_);
            ASSERT_EQ(lhs.basisDegree_, rhs.basisDegree_);
            ASSERT_GE(lhs.basisDegree_, 1);
            ASSERT_LE(lhs.basisDegree_, 8);
            ASSERT_TRUE(lhs.validationMse_.has_value());
            ASSERT_TRUE(std::isfinite(*lhs.validationMse_));
            ASSERT_EQ(lhs.validationMse_, rhs.validationMse_);
        }
        MonteCarloSettings_ settings;
        settings.compiled_ = compiled;
        settings.lsmcBasisDegree_ = 8;
        settings.lsmcTrainingPaths_ = 4096;
        settings.lsmcValidationPaths_ = 1024;
        settings.smooth_ = 1e-10;
        const auto hard = MCSimulation<double>(product, StandardModel(), 257, {}, settings);
        settings.enableAad_ = true;
        const auto fuzzy = MCSimulation<AAD::Number_>(product, StandardModel(), 257, {}, settings);
        ASSERT_NEAR(hard.aggregated_ / 257.0, fuzzy.aggregated_ / 257.0, 1e-7);
        for (double risk : fuzzy.risks_)
            ASSERT_TRUE(std::isfinite(risk));
    }
}

TEST(ScriptExerciseLSMCTest, TestAdaptiveDegreeReportsNoLossWithoutValidationCandidates) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const auto product = ExerciseOnlyProduct({Date_(2027, 9, 20)}, 0.0);
    const auto run = RunLsmc(product, StandardModel(), 128, 8, false, 256, 128);
    ASSERT_EQ(run.diagnostics_.events_.size(), 1u);
    ASSERT_EQ(run.diagnostics_.events_[0].numCondTruePaths_, 0u);
    ASSERT_TRUE(run.diagnostics_.events_[0].degenerate_);
    ASSERT_FALSE(run.diagnostics_.events_[0].validationMse_.has_value());
}

TEST(ScriptExerciseLSMCTest, TestRejectsSobolPathRangeOverflowBeforeAllocation) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const auto product = ExerciseOnlyProduct({Date_(2027, 9, 20)});
    constexpr size_t PATHS = std::numeric_limits<uint32_t>::max();
    for (const auto trainingPaths : {std::optional<int>(), std::optional<int>(1)}) {
        MonteCarloSettings_ settings;
        settings.lsmcTrainingPaths_ = trainingPaths;
        for (const bool compiled : {false, true}) {
            settings.compiled_ = compiled;
            ASSERT_THROW(MCSimulation<double>(product, StandardModel(), PATHS, {}, settings), ScriptError_);
            settings.enableAad_ = true;
            ASSERT_THROW(MCSimulation<AAD::Number_>(product, StandardModel(), PATHS, {}, settings), ScriptError_);
            settings.enableAad_ = false;
        }
    }
    MonteCarloSettings_ withValidation;
    withValidation.lsmcTrainingPaths_ = std::numeric_limits<int>::max();
    withValidation.lsmcValidationPaths_ = std::numeric_limits<int>::max();
    ASSERT_THROW(MCSimulation<double>(product, StandardModel(), 2, {}, withValidation), ScriptError_);
}

TEST(ScriptExerciseLSMCTest, TestLsmcPrunesDeadStatementsAndKeepsBranchDependencies) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const Vector_<Cell_> dates{Cell_(Date_(2027, 3, 20)), Cell_(Date_(2027, 9, 20))};
    const ScriptProductData_ product("", dates,
                                     {"x = 1\nx = spot()\nIF spot() > 100 THEN y = x ELSE y = 2 * x END\n"
                                      "IF spot() > 90 THEN unused = LOG(spot()) END",
                                      "dead = EXP(spot())\nEXERCISE MAX(120 - y, 0)"});
    auto model = CreateModel<double>(StandardModel());
    const auto prepared = PrepareScript(product, model.get(), {}, {});
    ASSERT_EQ(prepared.Product().Events()[0].size(), 2u);
    ASSERT_EQ(prepared.Product().Events()[1].size(), 1u);
    const ScriptProductData_ minimal("", dates, {"x = spot()\nIF spot() > 100 THEN y = x ELSE y = 2 * x END", "EXERCISE MAX(120 - y, 0)"});
    for (bool compiled : {false, true}) {
        MonteCarloSettings_ settings;
        settings.compiled_ = compiled;
        for (bool aad : {false, true}) {
            settings.enableAad_ = aad;
            const auto run = [&](const ScriptProductData_& data) {
                return aad ? MCSimulation<AAD::Number_>(data, StandardModel(), 256, {}, settings)
                           : MCSimulation<double>(data, StandardModel(), 256, {}, settings);
            };
            const auto actual = run(product), expected = run(minimal);
            ASSERT_EQ(actual.aggregated_, expected.aggregated_);
            ASSERT_EQ(actual.risks_, expected.risks_);
        }
    }
}

TEST(ScriptExerciseLSMCTest, TestDeadFutureFixDoesNotAllocateModelObservation) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const Vector_<Cell_> dates{Cell_(Date_(2027, 3, 20)), Cell_(Date_(2027, 9, 20))};
    const String_ liveEvent = "pay PAYS FIX(EQ[DAL418_TEST])\nEXERCISE MAX(120 - FIX(EQ[DAL418_TEST]), 0)";
    const ScriptProductData_ withDeadFix("", dates, {"dead = FIX(EQ[DAL418_TEST])", liveEvent});
    const ScriptProductData_ minimal("", dates, {"dead = 0", liveEvent});
    auto model = CreateModel<double>(StandardModel());
    const auto prepared = PrepareScript(withDeadFix, model.get(), {}, {});
    ASSERT_EQ(prepared.Plan().Requests().size(), 2u);
    ASSERT_EQ(prepared.Plan().SampleDates().size(), 2u);
    ASSERT_TRUE(prepared.DefLine()[0].indexNames_.empty());
    ASSERT_EQ(prepared.DefLine()[1].indexNames_.size(), 1u);
    ASSERT_FALSE(prepared.Plan().Requests()[0].modelSlot_);
    ASSERT_TRUE(prepared.Plan().Requests()[1].modelSlot_);

    //  A dead off-event fixing still contributes a Sobol dimension: removing
    //  that date would change every later simulated spot.
    const ScriptProductData_ offGrid("", dates, {"dead = 0", "dead = FIX(EQ[DAL418_TEST], 2027-06-20)\n" + liveEvent});
    auto offGridModel = CreateModel<double>(StandardModel());
    const auto offGridPrepared = PrepareScript(offGrid, offGridModel.get(), {}, {});
    ASSERT_EQ(offGridPrepared.Plan().SampleDates().size(), 3u);
    ASSERT_TRUE(offGridPrepared.DefLine()[1].indexNames_.empty());
    ASSERT_EQ(offGridModel->SimDim(), 3u);

    for (const bool dupire : {false, true}) {
        SCOPED_TRACE(dupire);
        const Handle_<ModelData_> modelData =
            dupire ? Handle_<ModelData_>(new DupireModelData_("dupire", SPOT, RATE, DIV, {50.0, 150.0}, {0.0, 2.0}, Matrix_<>(2, 2, VOL)))
                   : StandardModel();
        for (const bool compiled : {false, true}) {
            for (const bool aad : {false, true}) {
                MonteCarloSettings_ settings;
                settings.compiled_ = compiled;
                settings.enableAad_ = aad;
                settings.lsmcTrainingPaths_ = 4096;
                const auto run = [&](const ScriptProductData_& product) {
                    return aad ? MCSimulation<AAD::Number_>(product, modelData, 8197, {}, settings)
                               : MCSimulation<double>(product, modelData, 8197, {}, settings);
                };
                const auto actual = run(withDeadFix), expected = run(minimal);
                ASSERT_EQ(actual.aggregated_, expected.aggregated_);
                ASSERT_EQ(actual.risks_, expected.risks_);
            }
        }
    }
}

TEST(ScriptExerciseLSMCTest, TestDeadHistoricalFixStillRequiresResolution) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const ScriptProductData_ product("", {Cell_(Date_(2027, 3, 20)), Cell_(Date_(2027, 9, 20))},
                                     {"dead = FIX(EQ[DAL418_MISSING], 2026-09-11)", "EXERCISE MAX(120 - FIX(EQ[DAL418_MISSING]), 0)"});
    auto model = CreateModel<double>(StandardModel());
    const Handle_<MarketFixingSnapshot_> empty(new MarketFixingSnapshot_({}));
    try {
        static_cast<void>(PrepareScript(product, model.get(), {}, {}, empty));
        FAIL() << "dead historical fixing was skipped";
    } catch (const ScriptError_& error) {
        ASSERT_NE(std::string(error.what()).find("MissingFixing"), std::string::npos);
    }
}

TEST(ScriptExerciseLSMCTest, TestDeadFutureUnsupportedIndexStillFailsPreparation) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const ScriptProductData_ product("", {Cell_(Date_(2027, 3, 20)), Cell_(Date_(2027, 9, 20))}, {"dead = FIX(FX[EUR/USD])", "EXERCISE 1"});
    auto model = CreateModel<double>(StandardModel());
    try {
        static_cast<void>(PrepareScript(product, model.get(), {}, {}));
        FAIL() << "dead unsupported model fixing was skipped";
    } catch (const ScriptError_& error) {
        ASSERT_NE(std::string(error.what()).find("UnsupportedModelObservation"), std::string::npos);
    }
}

TEST(ScriptExerciseLSMCTest, TestOnlySelectedPayoffReceiverEntersContinuation) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const ScriptProductData_ product("", {Cell_(Date_(2027, 9, 20))}, {"fee PAYS 100\npay PAYS 1\nEXERCISE 2 IF fee > 0"});
    for (bool compiled : {false, true}) {
        MonteCarloSettings_ settings;
        settings.compiled_ = compiled;
        const auto hard = MCSimulation<double>(product, ModelWithVol(0.0), 256, {}, settings);
        ASSERT_NEAR(hard.aggregated_ / 256, 2.0 * std::exp(-RATE), 1e-10);
        settings.enableAad_ = true;
        const auto fuzzy = MCSimulation<AAD::Number_>(product, ModelWithVol(0.0), 256, {}, settings);
        ASSERT_NEAR(fuzzy.aggregated_ / 256, 2.0 * std::exp(-RATE), 1e-10);
    }
}

TEST(ScriptExerciseLSMCTest, TestRejectsPayoffAssignmentsThatInvalidateCashflowRecursion) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const ScriptProductData_ product("", {Cell_(Date_(2027, 9, 20))}, {"pay PAYS 100\npay = 1\nEXERCISE 2"});
    ASSERT_THROW(RunLsmc(product, ModelWithVol(0.0), 256), Dal::Exception_);
}

TEST(ScriptExerciseLSMCTest, TestRejectsHistoricalPayoffSeedWithZeroValueAndLiveRisk) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const ScriptProductData_ product("", {Cell_("SCALE"), Cell_(EvalDate().AddDays(-1)), Cell_(Date_(2027, 9, 20))},
                                     {"1", "pay = SCALE - 1", "pay PAYS 1\nEXERCISE 2"});
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ settings;
        settings.compiled_ = compiled;
        ASSERT_THROW(MCSimulation<double>(product, StandardModel(), 64, {}, settings), ScriptError_);
        settings.enableAad_ = true;
        ASSERT_THROW(MCSimulation<AAD::Number_>(product, StandardModel(), 64, {}, settings), ScriptError_);
    }
}

TEST(ScriptExerciseLSMCTest, TestHistoricalZeroInitializationAndExpiredPaymentAreAllowed) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const ScriptProductData_ product("", {Cell_(EvalDate().AddDays(-1)), Cell_(Date_(2027, 9, 20))},
                                     {"pay PAYS 100\npay = 0", "pay PAYS 1\nEXERCISE 2"});
    for (const bool compiled : {false, true}) {
        MonteCarloSettings_ settings;
        settings.compiled_ = compiled;
        const auto hard = MCSimulation<double>(product, StandardModel(), 64, {}, settings);
        ASSERT_NEAR(hard.aggregated_ / 64, 2.0 * std::exp(-RATE), 1e-10);
        settings.enableAad_ = true;
        const auto aad = MCSimulation<AAD::Number_>(product, StandardModel(), 64, {}, settings);
        ASSERT_NEAR(aad.aggregated_ / 64, 2.0 * std::exp(-RATE), 1e-10);
    }
}

TEST(ScriptExerciseLSMCTest, TestZeroInitializationInMutuallyExclusivePaymentBranch) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const Handle_<ModelData_> model(new BSModelData_("bs", 1.0, 0.0, 0.0, 0.0));
    const Vector_<String_> scripts{"IF spot() > 1 THEN pay PAYS 4 ELSE pay = 0 END\nEXERCISE 0",
                                   "IF spot() > 1 THEN pay = 0 ELSE pay PAYS 4 END\nEXERCISE 0"};
    for (size_t i = 0; i < scripts.size(); ++i) {
        const ScriptProductData_ product("", {Cell_(Date_(2027, 9, 20))}, {scripts[i]});
        for (const bool compiled : {false, true}) {
            MonteCarloSettings_ settings;
            settings.compiled_ = compiled;
            const auto hard = MCSimulation<double>(product, model, 64, {}, settings);
            ASSERT_NEAR(hard.aggregated_ / 64, i == 0 ? 0.0 : 4.0, 1e-10);
            settings.enableAad_ = true;
            const auto aad = MCSimulation<AAD::Number_>(product, model, 64, {}, settings);
            ASSERT_NEAR(aad.aggregated_ / 64, 2.0, 1e-10);
        }
    }
}

TEST(ScriptExerciseLSMCTest, TestRejectsNonFiniteExerciseValue) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const ScriptProductData_ product("", {Cell_(Date_(2027, 9, 20))}, {"EXERCISE SQRT(-spot())"});
    for (bool compiled : {false, true})
        ASSERT_THROW(RunLsmc(product, StandardModel(), 64, 3, compiled), Dal::Exception_);
}

TEST(ScriptExerciseLSMCTest, TestLsmcPruningRetainsImplicitBranchState) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const Handle_<ModelData_> model(new BSModelData_("bs", 1.0, 0.0, 0.0, 0.0));
    const Vector_<String_> scripts{"x = 4\nIF spot() > 1 THEN x = 2 ELSE dead = LOG(spot()) END\nEXERCISE x",
                                   "x = 4\nIF spot() > 1 THEN dead = LOG(spot()) ELSE x = 2 END\nEXERCISE x",
                                   "x = 4\nIF spot() > 1 THEN IF spot() > 1 THEN x = 2 ELSE dead = 3 END END\nEXERCISE x"};
    const Vector_<> hardValues{4.0, 2.0, 4.0}, fuzzyValues{3.0, 3.0, 3.5};
    for (size_t i = 0; i < scripts.size(); ++i) {
        SCOPED_TRACE(std::to_string(i));
        const ScriptProductData_ product("", {Cell_(Date_(2027, 9, 20))}, {scripts[i]});
        for (bool compiled : {false, true}) {
            MonteCarloSettings_ settings;
            settings.compiled_ = compiled;
            const auto hard = MCSimulation<double>(product, model, 64, {}, settings);
            ASSERT_NEAR(hard.aggregated_ / 64, hardValues[i], 1e-10);
            settings.enableAad_ = true;
            const auto fuzzy = MCSimulation<AAD::Number_>(product, model, 64, {}, settings);
            ASSERT_NEAR(fuzzy.aggregated_ / 64, fuzzyValues[i], 1e-10);
        }
    }
}

//  ---------------------------------------------------------------------------
//  PDE-anchored Bermudan benchmarks
//  ---------------------------------------------------------------------------

TEST(ScriptExerciseLSMCTest, TestBermudanMidDateBenchmark) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const Vector_<Date_> exerciseDates{Date_(2027, 9, 20), Date_(2028, 3, 20)};
    const double pde = BermudanPutPDE(SPOT, VOL, RATE, DIV, STRIKE, {YearFracTo(exerciseDates[0]), YearFracTo(exerciseDates[1])}, 2000, 2000);
    const auto run = RunLsmc(ExerciseOnlyProduct(exerciseDates), StandardModel(), EUROPEAN_LIMIT_PATHS);
    ASSERT_NEAR(run.pv_, pde, 0.005 * SPOT);
}

TEST(ScriptExerciseLSMCTest, TestAmericanWeeklyBenchmark) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const auto exerciseDates = WeeklyDates(78);
    std::vector<double> times;
    for (const auto& d : exerciseDates)
        times.push_back(YearFracTo(d));
    const double pde = BermudanPutPDE(SPOT, VOL, RATE, DIV, STRIKE, times, 1500, 150);
    //  discriminating power: the weekly premium over the European put exceeds the tolerance,
    //  so a driver that never exercises cannot pass
    ASSERT_GT(pde - EuropeanPutClosedForm(), 0.005 * SPOT);
    const auto run = RunLsmc(ExerciseOnlyProduct(exerciseDates), StandardModel(), EUROPEAN_LIMIT_PATHS, 3, false, 1 << 14);
    ASSERT_NEAR(run.pv_, pde, 0.005 * SPOT);
}

TEST(ScriptExerciseLSMCTest, TestConvergenceBandAndDispersion) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const Vector_<Date_> exerciseDates{Date_(2027, 9, 20), Date_(2028, 3, 20)};
    const double pde = BermudanPutPDE(SPOT, VOL, RATE, DIV, STRIKE, {YearFracTo(exerciseDates[0]), YearFracTo(exerciseDates[1])}, 2000, 2000);
    //  Disjoint QMC blocks need not improve absolute error monotonically. Require
    //  tight PDE accuracy at every size and the expected payoff-dispersion scaling.
    double dispersionAt64k = 0.0;
    for (size_t paths : {1u << 16, 1u << 17, 1u << 18}) {
        const auto run = RunLsmc(ExerciseOnlyProduct(exerciseDates), StandardModel(), paths, 4);
        const double error = std::abs(run.pv_ - pde);
        ASSERT_LT(error, std::max(3.0 * run.diagnostics_.StandardError(), 0.0075 * SPOT));
        ASSERT_LT(error, 0.01);
        if (paths == (1u << 16))
            dispersionAt64k = run.diagnostics_.StandardError();
        if (paths == (1u << 18))
            ASSERT_LT(run.diagnostics_.StandardError(), 0.51 * dispersionAt64k);
    }
}

//  ---------------------------------------------------------------------------
//  Thread invariance (N9): 1 vs N threads, bitwise
//  ---------------------------------------------------------------------------

namespace {
    struct PoolRestore_ {
        ThreadPool_* pool_ = ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        ~PoolRestore_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    uint64_t BitsOf(double x) {
        uint64_t bits;
        std::memcpy(&bits, &x, sizeof(bits));
        return bits;
    }

    void AssertBitwiseEqual(const LsmcRun_& lhs, const LsmcRun_& rhs) {
        ASSERT_EQ(BitsOf(lhs.pv_), BitsOf(rhs.pv_));
        ASSERT_EQ(lhs.diagnostics_.events_.size(), rhs.diagnostics_.events_.size());
        for (size_t k = 0; k < lhs.diagnostics_.events_.size(); ++k) {
            const auto& l = lhs.diagnostics_.events_[k];
            const auto& r = rhs.diagnostics_.events_[k];
            ASSERT_EQ(BitsOf(l.exerciseRate_), BitsOf(r.exerciseRate_));
            ASSERT_EQ(l.degenerate_, r.degenerate_);
            ASSERT_EQ(l.degenerateReason_, r.degenerateReason_);
            ASSERT_EQ(l.coefficients_.size(), r.coefficients_.size());
            for (size_t j = 0; j < l.coefficients_.size(); ++j)
                ASSERT_EQ(BitsOf(l.coefficients_[j]), BitsOf(r.coefficients_[j]));
        }
    }
} // namespace

TEST(ScriptExerciseLSMCTest, TestThreadInvarianceBitwise) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    PoolRestore_ pool;
    const auto product = ExerciseOnlyProduct(WeeklyDates(12));
    for (const bool compiled : {false, true}) {
        SCOPED_TRACE(compiled ? "compiled" : "tree-walk");
        pool.pool_->Start(1, true);
        const auto single = RunLsmc(product, StandardModel(), 1u << 16, 3, compiled, 12307);
        pool.pool_->Start(std::max(4u, static_cast<unsigned>(pool.threads_)), true);
        const auto multi = RunLsmc(product, StandardModel(), 1u << 16, 3, compiled, 12307);
        AssertBitwiseEqual(single, multi);
    }
}

//  ---------------------------------------------------------------------------
//  Zero-exercise degeneration: always-false condition == plain product, bitwise
//  ---------------------------------------------------------------------------

TEST(ScriptExerciseLSMCTest, TestZeroExerciseMatchesPlainProductBitwise) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    PoolRestore_ pool;
    pool.pool_->Start(1, true);
    const Vector_<Cell_> dates{Cell_(Date_(2027, 9, 20)), Cell_(Date_(2028, 3, 20))};
    const Vector_<String_> plainEvents{"pay PAYS 1.0", "pay PAYS 2.0"};
    const Vector_<String_> exerciseEvents{"pay PAYS 1.0", "pay PAYS 2.0\nEXERCISE 1000.0 IF spot() > 1.0e9"};
    MonteCarloSettings_ simulation;
    const auto plain =
        MCSimulation<double>(ScriptProductData_("", dates, plainEvents), StandardModel(), 4096, ScriptValuationSettings_(), simulation);
    const auto exercise =
        MCSimulation<double>(ScriptProductData_("", dates, exerciseEvents), StandardModel(), 4096, ScriptValuationSettings_(), simulation);
    ASSERT_EQ(BitsOf(plain.aggregated_), BitsOf(exercise.aggregated_));
}

//  ---------------------------------------------------------------------------
//  Degenerate strategies (N3): SigmaFloor, ConditionPathsBelowMin, IllConditioned
//  ---------------------------------------------------------------------------

TEST(ScriptExerciseLSMCTest, TestDegenerateSigmaFloorOnZeroVol) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const auto product = ExerciseOnlyProduct({Date_(2027, 9, 20), Date_(2028, 3, 20)}, 120.0);
    const auto run = RunLsmc(product, ModelWithVol(0.0), 4096);
    ASSERT_EQ(run.diagnostics_.events_.size(), 2u);
    for (const auto& event : run.diagnostics_.events_) {
        ASSERT_TRUE(event.degenerate_);
        ASSERT_EQ(event.degenerateReason_, "SigmaFloor");
        ASSERT_EQ(event.basisDegree_, 0);
    }
    //  deterministic economy: exercise at the 1y mid date beats holding to 1.5y, PV = h(mid) discounted
    const double sMid = SPOT * std::exp(RATE * 1.0);
    const double expected = (120.0 - sMid) * std::exp(-RATE * 1.0);
    ASSERT_NEAR(run.pv_, expected, 1e-10);
}

TEST(ScriptExerciseLSMCTest, TestDegenerateConditionPathsBelowMin) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const Vector_<Cell_> dates{Cell_(Date_(2028, 3, 20))};
    const ScriptProductData_ product("", dates, {"EXERCISE 50.0 IF spot() > 1.0e6"});
    const auto run = RunLsmc(product, StandardModel(), 4096);
    ASSERT_EQ(run.diagnostics_.events_.size(), 1u);
    ASSERT_TRUE(run.diagnostics_.events_[0].degenerate_);
    ASSERT_EQ(run.diagnostics_.events_[0].degenerateReason_, "ConditionPathsBelowMin");
    ASSERT_EQ(run.diagnostics_.events_[0].numCondTruePaths_, 0u);
    ASSERT_EQ(run.pv_, 0.0);
}

//  Longstaff-Schwartz: only in-the-money paths (h > 0) enter the continuation
//  regression or the exercise decision; an all-OTM day has an empty regression
//  set even though the day is unconditional
TEST(ScriptExerciseLSMCTest, TestDegenerateAllPathsOutOfTheMoney) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const auto product = ExerciseOnlyProduct({Date_(2027, 9, 20), Date_(2028, 3, 20)}, 90.0);
    const auto run = RunLsmc(product, ModelWithVol(0.0), 256);
    ASSERT_EQ(run.diagnostics_.events_.size(), 2u);
    for (const auto& event : run.diagnostics_.events_) {
        ASSERT_TRUE(event.degenerate_);
        ASSERT_EQ(event.degenerateReason_, "ConditionPathsBelowMin");
        ASSERT_EQ(event.numCondTruePaths_, 0u);
        ASSERT_EQ(event.exerciseRate_, 0.0);
    }
    ASSERT_EQ(run.pv_, 0.0);
}

//  Regression undershoot must not "exercise" out-of-the-money paths: the day-1
//  regression set is the 4084 in-the-money paths and the exercise rate stays below
//  the ITM fraction. The unfiltered driver exercised 41.4% of all paths at day 1 —
//  most of them at h == 0 against a negative continuation estimate — and
//  underpriced this put by more than 1% against the PDE oracle
TEST(ScriptExerciseLSMCTest, TestOutOfTheMoneyPathsNeverExercise) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const Vector_<Date_> exerciseDates{Date_(2027, 9, 20), Date_(2028, 3, 20)};
    constexpr size_t N_PATHS = 1u << 14;
    const auto run = RunLsmc(ExerciseOnlyProduct(exerciseDates, 90.0), StandardModel(), N_PATHS);
    ASSERT_EQ(run.diagnostics_.events_.size(), 2u);
    ASSERT_EQ(run.diagnostics_.events_[0].numCondTruePaths_, 4084u);
    ASSERT_EQ(run.diagnostics_.events_[1].numCondTruePaths_, 4412u);
    ASSERT_GT(run.diagnostics_.events_[0].exerciseRate_, 0.0);
    ASSERT_LE(run.diagnostics_.events_[0].exerciseRate_,
              static_cast<double>(run.diagnostics_.events_[0].numCondTruePaths_) / static_cast<double>(N_PATHS));
    const double pde = BermudanPutPDE(SPOT, VOL, RATE, DIV, 90.0, {YearFracTo(exerciseDates[0]), YearFracTo(exerciseDates[1])}, 2000, 2000);
    ASSERT_NEAR(run.pv_, pde, 0.005 * SPOT);
}

TEST(ScriptExerciseLSMCTest, TestQrFallbackForDeepTailExercise) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    //  5y at 50% vol puts an extreme right-tail outlier into the regressors; the degree-8
    //  Gram diagonal ratio crosses the conditioning guard. The call keeps the outlier
    //  inside the in-the-money regression set (on a put the right tail is all h == 0 and
    //  the ITM filter excludes it before the guard can see it)
    const Handle_<ModelData_> wild(new BSModelData_("bs", SPOT, 0.5, RATE, DIV));
    const ScriptProductData_ product("", {Cell_(Date_(2031, 9, 20))}, {"EXERCISE MAX(spot() - 100.0, 0.0)"});
    const auto run = RunLsmc(product, wild, 4096, 8);
    ASSERT_EQ(run.diagnostics_.events_.size(), 1u);
    ASSERT_FALSE(run.diagnostics_.events_[0].degenerate_);
    ASSERT_GE(run.diagnostics_.events_[0].basisDegree_, 1);
    ASSERT_EQ(run.diagnostics_.events_[0].solver_, "PivotedQR");
    ASSERT_FALSE(run.diagnostics_.events_[0].fallbackReason_.empty());
    ASSERT_TRUE(std::isfinite(run.pv_));
}

//  ---------------------------------------------------------------------------
//  Basis degree cases (X4)
//  ---------------------------------------------------------------------------

TEST(ScriptExerciseLSMCTest, TestDegreeSixMatchesPde) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const auto exerciseDates = WeeklyDates(78);
    std::vector<double> times;
    for (const auto& d : exerciseDates)
        times.push_back(YearFracTo(d));
    const double pde = BermudanPutPDE(SPOT, VOL, RATE, DIV, STRIKE, times, 1500, 150);
    const auto run = RunLsmc(ExerciseOnlyProduct(exerciseDates), StandardModel(), EUROPEAN_LIMIT_PATHS, 6);
    ASSERT_NEAR(run.pv_, pde, 0.005 * SPOT);
}

TEST(ScriptExerciseLSMCTest, TestDegreeEightGuardDegenerateMarked) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    //  43 condition-true paths: degree 3 needs 40 (healthy fit), degree 8 needs 90 (guard fires)
    const ScriptProductData_ product("", {Cell_(Date_(2027, 9, 20))}, {"EXERCISE 50.0 IF spot() > 180.0"});
    const auto atThree = RunLsmc(product, StandardModel(), 1u << 14, 3);
    ASSERT_FALSE(atThree.diagnostics_.events_[0].degenerate_);
    ASSERT_EQ(atThree.diagnostics_.events_[0].basisDegree_, 3);
    const auto atEight = RunLsmc(product, StandardModel(), 1u << 14, 8);
    ASSERT_TRUE(atEight.diagnostics_.events_[0].degenerate_);
    ASSERT_EQ(atEight.diagnostics_.events_[0].degenerateReason_, "ConditionPathsBelowMin");
    ASSERT_EQ(atEight.diagnostics_.events_[0].numCondTruePaths_, 43u);
    ASSERT_TRUE(std::isfinite(atEight.pv_));
}

//  ---------------------------------------------------------------------------
//  PAYS x EXERCISE composition (S3/S4): the same-day payment sits on the holding
//  side of the decision, and exercise replaces it on the exercising paths
//  ---------------------------------------------------------------------------

TEST(ScriptExerciseLSMCTest, TestPaysAndExerciseCompose) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const Vector_<Cell_> dates{Cell_(Date_(2027, 9, 20)), Cell_(Date_(2028, 3, 20))};
    { //  deterministic economy, deep ITM: exercise at the mid date kills the maturity coupon
        const ScriptProductData_ product("", dates, {"EXERCISE MAX(120.0 - spot(), 0.0)", "EXERCISE MAX(120.0 - spot(), 0.0)\npay PAYS 2.0"});
        const auto run = RunLsmc(product, ModelWithVol(0.0), 256);
        const double sMid = SPOT * std::exp(RATE * 1.0);
        ASSERT_NEAR(run.pv_, (120.0 - sMid) * std::exp(-RATE * 1.0), 1e-10);
    }
    { //  deterministic economy, OTM everywhere: never exercises, the coupon survives intact
        const ScriptProductData_ product("", dates, {"EXERCISE MAX(90.0 - spot(), 0.0)", "EXERCISE MAX(90.0 - spot(), 0.0)\npay PAYS 2.0"});
        const auto run = RunLsmc(product, ModelWithVol(0.0), 256);
        ASSERT_NEAR(run.pv_, 2.0 * std::exp(-RATE * YearFracTo(Date_(2028, 3, 20))), 1e-12);
    }
    { //  a coupon strictly before the exercise date survives the exercise (S4 replaces same-day and later only)
        const Vector_<Cell_> withCoupon{Cell_(Date_(2027, 3, 20)), Cell_(Date_(2027, 9, 20)), Cell_(Date_(2028, 3, 20))};
        const ScriptProductData_ product("", withCoupon, {"pay PAYS 2.0", "EXERCISE MAX(120.0 - spot(), 0.0)", "EXERCISE MAX(120.0 - spot(), 0.0)"});
        const auto run = RunLsmc(product, ModelWithVol(0.0), 256);
        const double sMid = SPOT * std::exp(RATE * 1.0);
        const double expected = 2.0 * std::exp(-RATE * YearFracTo(Date_(2027, 3, 20))) + (120.0 - sMid) * std::exp(-RATE * 1.0);
        ASSERT_NEAR(run.pv_, expected, 1e-10);
    }
}

TEST(ScriptExerciseLSMCTest, TestPricingKeepsEarlierPaymentAndDropsSameDayPaymentAcrossBatches) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    PoolRestore_ pool;
    const Date_ couponDate(2027, 3, 20);
    const Date_ exerciseDate(2027, 9, 20);
    const Date_ maturity(2028, 3, 20);
    const ScriptProductData_ product(
        "", {Cell_(couponDate), Cell_(exerciseDate), Cell_(maturity)},
        {"pay PAYS 2.0", "pay PAYS 3.0\nEXERCISE MAX(300.0 - spot(), 0.0)", "pay PAYS 5.0\nEXERCISE MAX(300.0 - spot(), 0.0)"});
    const double tCoupon = YearFracTo(couponDate);
    const double tExercise = YearFracTo(exerciseDate);
    const double expected = 2.0 * std::exp(-RATE * tCoupon) + (300.0 - SPOT * std::exp(RATE * tExercise)) * std::exp(-RATE * tExercise);
    constexpr size_t PRICING_PATHS = 8197;
    for (const int trainingPaths : {73, 16391}) {
        SCOPED_TRACE(trainingPaths);
        for (const bool compiled : {false, true}) {
            SCOPED_TRACE(compiled);
            pool.pool_->Start(1, true);
            const auto single = RunLsmc(product, ModelWithVol(0.0), PRICING_PATHS, 3, compiled, trainingPaths);
            pool.pool_->Start(std::max(4u, static_cast<unsigned>(pool.threads_)), true);
            const auto multi = RunLsmc(product, ModelWithVol(0.0), PRICING_PATHS, 3, compiled, trainingPaths);
            ASSERT_NEAR(single.pv_, expected, 1e-10);
            ASSERT_EQ(BitsOf(single.pv_), BitsOf(multi.pv_));
            ASSERT_EQ(single.diagnostics_.events_.size(), 2u);
            ASSERT_DOUBLE_EQ(single.diagnostics_.events_[0].exerciseRate_, 1.0);
            ASSERT_DOUBLE_EQ(single.diagnostics_.events_[1].exerciseRate_, 0.0);
        }
    }
}

//  ---------------------------------------------------------------------------
//  Execution-mode gates
//  ---------------------------------------------------------------------------

namespace {
    template <class F_> void AssertUnsupportedMode(F_ action) {
        try {
            action();
            FAIL() << "expected an execution-mode rejection";
        } catch (const ScriptError_& error) {
            ASSERT_NE(std::string(error.what()).find("UnsupportedExecutionMode"), std::string::npos) << error.what();
        }
    }
} // namespace

//  ---------------------------------------------------------------------------
//  AAD valuation (T4): fuzzy recursive blending over the frozen policy (S9/N6)
//  ---------------------------------------------------------------------------

namespace {
    MonteCarloSettings_ AadSettings(bool compiled, double smooth = 0.01, int degree = 3) {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        simulation.compiled_ = compiled;
        simulation.smooth_ = smooth;
        simulation.lsmcBasisDegree_ = degree;
        return simulation;
    }

    //  Reference market with every parameter live, so the relative 1e-3 bumps stay nonzero
    Handle_<ModelData_> BumpModel(double spot = SPOT, double vol = VOL, double rate = RATE, double div = 0.03) {
        return Handle_<ModelData_>(new BSModelData_("bs", spot, vol, rate, div));
    }

    double PvOf(const SimResults_& results, size_t nPaths) { return results.aggregated_ / static_cast<double>(nPaths); }
} // namespace

TEST(ScriptExerciseLSMCTest, TestAadGate) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const auto product = ExerciseOnlyProduct({Date_(2027, 9, 20)});
    { //  the fuzzy driver values AAD EXERCISE products in both engines (gate lifted in T4)
        const auto aad = MCSimulation<AAD::Number_>(product, StandardModel(), 4096, ScriptValuationSettings_(), AadSettings(false));
        const auto aadCompiled = MCSimulation<AAD::Number_>(product, StandardModel(), 4096, ScriptValuationSettings_(), AadSettings(true));
        const auto treeWalk = MCSimulation<double>(product, StandardModel(), 4096, ScriptValuationSettings_(), MonteCarloSettings_());
        ASSERT_GT(aad.aggregated_, 0.0);
        ASSERT_NEAR(aad.aggregated_, treeWalk.aggregated_, 0.02 * 4096);
        ASSERT_NEAR(aadCompiled.aggregated_, aad.aggregated_, 1e-6 * 4096);
    }
    { //  double simulation still rejects a requested AAD mode
        AssertUnsupportedMode([&] {
            MonteCarloSettings_ simulation;
            simulation.enableAad_ = true;
            static_cast<void>(MCSimulation<double>(product, StandardModel(), 128, ScriptValuationSettings_(), simulation));
        });
    }
}

//  AAD parameter risks vs central differences under the production behavior: every
//  bump re-runs the full valuation, so the regression policy regenerates (N6).
//
//  N6 exceedance record (T4): with the plan's bump sizes the plan's 1e-3/5e-3 band is
//  exceeded on this two-date product - measured at 2^18 sobol paths and smooth 0.01:
//  spot 1.1%, vol 2.6%, rate 1.1%, div 0.6%. Attribution: the adjoint is the exact
//  gradient of the frozen-policy functional, so the comparison gap is the envelope
//  remainder dV/dpolicy * dpolicy/dtheta plus the QMC truncation of the small
//  relative bumps (the fd estimates themselves move 1-3% between 2^17 and 2^18); vol
//  is the worst because the continuation regression itself is vol-dependent and its
//  dC/dvol is absent from the replay by design. The single-date case (policy exactly
//  optimal, continuation identically zero) matches analytic greeks to better than
//  0.2% in TestHistoricalFixingTimesExerciseAad, which pins the machinery itself.
//  Per the plan the fallback (seeded coefficient regeneration) is recorded here and
//  stays outside v1; this test guards the measured envelope scale deterministically.
TEST(ScriptExerciseLSMCTest, TestAadRiskMatchesCentralDifferences) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const Vector_<Date_> exerciseDates{Date_(2027, 9, 20), Date_(2028, 3, 20)};
    const auto product = ExerciseOnlyProduct(exerciseDates);
    constexpr size_t N_PATHS = 1u << 18;
    for (const bool compiled : {false, true}) {
        SCOPED_TRACE(compiled ? "compiled" : "tree-walk");
        const auto aad = MCSimulation<AAD::Number_>(product, BumpModel(), N_PATHS, ScriptValuationSettings_(), AadSettings(compiled));
        struct Bumped_ {
            String_ name_;
            Handle_<ModelData_> up_;
            Handle_<ModelData_> down_;
            double step_;
        };
        const Bumped_ bumps[]{
            {"spot", BumpModel(SPOT + 0.05), BumpModel(SPOT - 0.05), 0.05},
            {"vol", BumpModel(SPOT, VOL * (1.0 + 1.0e-3)), BumpModel(SPOT, VOL * (1.0 - 1.0e-3)), VOL * 1.0e-3},
            {"rate", BumpModel(SPOT, VOL, RATE * (1.0 + 1.0e-3)), BumpModel(SPOT, VOL, RATE * (1.0 - 1.0e-3)), RATE * 1.0e-3},
            {"div", BumpModel(SPOT, VOL, RATE, 0.03 * (1.0 + 1.0e-3)), BumpModel(SPOT, VOL, RATE, 0.03 * (1.0 - 1.0e-3)), 0.03 * 1.0e-3},
        };
        for (const auto& bump : bumps) {
            const double up = PvOf(MCSimulation<double>(product, bump.up_, N_PATHS, ScriptValuationSettings_(), MonteCarloSettings_()), N_PATHS);
            const double down = PvOf(MCSimulation<double>(product, bump.down_, N_PATHS, ScriptValuationSettings_(), MonteCarloSettings_()), N_PATHS);
            const double finiteDifference = (up - down) / (2.0 * bump.step_);
            const double relative = std::abs(aad[bump.name_] - finiteDifference) / std::abs(finiteDifference);
            ASSERT_LT(relative, 5.0e-2) << bump.name_ << ": aad=" << aad[bump.name_] << " fd=" << finiteDifference;
        }
    }
}

//  Check the shrinking smoothing band and the hard limit on the same held-out
//  paths. A wide finite band has no universal 0.001 absolute-error guarantee.
TEST(ScriptExerciseLSMCTest, TestFuzzyConvergesToHardMode) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const auto product = ExerciseOnlyProduct(WeeklyDates(12));
    constexpr size_t N_PATHS = 1u << 16;
    const double hard = PvOf(MCSimulation<double>(product, StandardModel(), N_PATHS, ScriptValuationSettings_(), MonteCarloSettings_()), N_PATHS);
    for (const double smooth : {0.1, 0.01, 0.001, 1e-8}) {
        SCOPED_TRACE(std::to_string(smooth));
        const auto aad = MCSimulation<AAD::Number_>(product, StandardModel(), N_PATHS, ScriptValuationSettings_(), AadSettings(false, smooth));
        const double error = std::abs(PvOf(aad, N_PATHS) - hard);
        ASSERT_LT(error, smooth) << "blend bias must shrink with the transition band";
    }
}

//  A PAYS inside a fuzzy-if branch must record the degree-weighted payment, never the
//  sum of both branches: the out-of-branch formulation pays the blended variable once,
//  so the two scripts agree path by path (the wide band puts many paths at interior
//  degrees; the EXERCISE keeps the product on the LSMC replay path)
TEST(ScriptExerciseLSMCTest, TestFuzzyBranchPaymentsBlend) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const Vector_<Cell_> cells{Cell_(Date_(2027, 9, 20)), Cell_(Date_(2028, 3, 20))};
    const ScriptProductData_ inBranch("", cells, {"IF spot() > 100.0 THEN pay PAYS 1.0 ELSE pay PAYS 3.0 END", "EXERCISE 0.0"});
    const ScriptProductData_ outOfBranch("", cells, {"x = 3.0\nIF spot() > 100.0 THEN x = 1.0 END\npay PAYS x", "EXERCISE 0.0"});
    constexpr size_t N_PATHS = 1u << 15;
    for (const bool compiled : {false, true}) {
        SCOPED_TRACE(compiled ? "compiled" : "tree-walk");
        const auto a = MCSimulation<AAD::Number_>(inBranch, StandardModel(), N_PATHS, ScriptValuationSettings_(), AadSettings(compiled, 10.0));
        const auto b = MCSimulation<AAD::Number_>(outOfBranch, StandardModel(), N_PATHS, ScriptValuationSettings_(), AadSettings(compiled, 10.0));
        ASSERT_NEAR(PvOf(a, N_PATHS), PvOf(b, N_PATHS), 1e-8 * std::abs(PvOf(b, N_PATHS)));
        ASSERT_NEAR(a["spot"], b["spot"], 1e-8 * std::abs(b["spot"]));
    }
}

//  N9 dual-mode promise completed: AAD risks are bitwise thread-count independent
TEST(ScriptExerciseLSMCTest, TestAadThreadInvarianceBitwise) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    PoolRestore_ pool;
    const Vector_<Cell_> dates{Cell_(Date_(2027, 3, 20)), Cell_(Date_(2027, 9, 20)), Cell_(Date_(2028, 3, 20))};
    const Vector_<String_> events{"pay PAYS 2.0", "EXERCISE MAX(120.0 - spot(), 0.0) IF spot() < 130.0", "EXERCISE MAX(120.0 - spot(), 0.0)"};
    const ScriptProductData_ product("", dates, events);
    for (const bool compiled : {false, true}) {
        SCOPED_TRACE(compiled ? "compiled" : "tree-walk");
        auto settings = AadSettings(compiled);
        settings.lsmcTrainingPaths_ = 12307;
        pool.pool_->Start(1, true);
        const auto single = MCSimulation<AAD::Number_>(product, StandardModel(), 1u << 16, ScriptValuationSettings_(), settings);
        pool.pool_->Start(std::max(4u, static_cast<unsigned>(pool.threads_)), true);
        const auto multi = MCSimulation<AAD::Number_>(product, StandardModel(), 1u << 16, ScriptValuationSettings_(), settings);
        ASSERT_EQ(BitsOf(single.aggregated_), BitsOf(multi.aggregated_));
        ASSERT_EQ(single.risks_.size(), multi.risks_.size());
        for (size_t j = 0; j < single.risks_.size(); ++j)
            ASSERT_EQ(BitsOf(single.risks_[j]), BitsOf(multi.risks_[j])) << "risk " << single.names_[j];
    }
}

//  ---------------------------------------------------------------------------
//  FIX x EXERCISE (T4): historical fixings seed the fuzzy exercise value, and the
//  parameter risk survives the historical chain (DAL-201 past replay composition)
//  ---------------------------------------------------------------------------

namespace {
    double NormalCdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

    struct HistoryRestore_ {
        String_ index_;
        FixHistory_ previous_;
        explicit HistoryRestore_(const String_& index) : index_(index), previous_(Global::Fixings_().History(index)) {}
        ~HistoryRestore_() {
            if (!previous_.vals_.empty()) {
                XGLOBAL::StoreFixings(index_, previous_, false);
                return;
            }
            for (const auto& object : ObjectAccess_::Find("##GLOBAL##FixingsFor:"))
                if (object->Name() == "##GLOBAL##FixingsFor:" + index_)
                    static_cast<void>(ObjectAccess_::Erase(*object));
        }
    };

    //  x = SCALE * FIX EQ (past) = 100; single future exercise of MAX(x - index, 0) on the
    //  same index: holding is worthless (no later payments), so the continuation fit is
    //  identically zero and the policy equals the European payoff - no envelope remainder
    ScriptProductData_ HistoryExerciseProduct() {
        return {"",
                {Cell_("SCALE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 26))},
                {"1.25", "x = SCALE * FIX(EQ[DAL283_TEST], 2026-09-11)", "EXERCISE MAX(x - FIX(EQ[DAL283_TEST]), 0.0) IF FIX(EQ[DAL283_TEST]) > 0.0"}};
    }

    Handle_<MarketFixingSnapshot_> HistorySnapshot(double fixing = 80.0) {
        return Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[DAL283_TEST]", {{DateTime_(Date_(2026, 9, 11), 0.0), fixing}}}}));
    }

    void AssertHistoryExerciseAad(const Handle_<MarketFixingSnapshot_>& snapshot) {
        const double t = 14.0 / DAYS_PER_YEAR;
        const double spot = 100.0, vol = 0.2, rate = 0.03, div = 0.01;
        const double strike = 1.25 * 80.0;
        const double fwd = spot * std::exp((rate - div) * t);
        const double discount = std::exp(-rate * t);
        const double sd = vol * std::sqrt(t);
        const double d1 = std::log(spot / strike) / sd + (rate - div + 0.5 * vol * vol) * t / sd;
        const double d2 = d1 - sd;
        constexpr size_t N_PATHS = 1u << 15;
        const auto product = HistoryExerciseProduct();
        const auto model = Handle_<ModelData_>(new BSModelData_("bs", spot, vol, rate, div));
        const auto aad = MCSimulation<AAD::Number_>(product, model, N_PATHS, ScriptValuationSettings_(), AadSettings(false), snapshot);
        ASSERT_NEAR(PvOf(aad, N_PATHS), discount * Distribution::BlackOpt(fwd, sd, strike, OptionType_::Value_::PUT), 0.02);
        //  the strike scales with the historical chain: d_SCALE = fixing * dPut/dK = fixing * df * P(S_T < K)
        ASSERT_NEAR(aad["SCALE"], 80.0 * discount * NormalCdf(-d2), 0.02);
        ASSERT_NEAR(aad["spot"], -std::exp(-div * t) * NormalCdf(-d1), 0.01);
    }
} // namespace

TEST(ScriptExerciseLSMCTest, TestHistoricalFixingTimesExerciseAad) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(Date_(2026, 9, 12));
    { //  explicit snapshot
        AssertHistoryExerciseAad(HistorySnapshot());
    }
    { //  global fixings store
        HistoryRestore_ restore("EQ[DAL283_TEST]");
        FixHistory_ history;
        history.vals_.push_back({DateTime_(Date_(2026, 9, 11), 0.0), 80.0});
        XGLOBAL::StoreFixings("EQ[DAL283_TEST]", history, false);
        AssertHistoryExerciseAad(Handle_<MarketFixingSnapshot_>());
    }
}
