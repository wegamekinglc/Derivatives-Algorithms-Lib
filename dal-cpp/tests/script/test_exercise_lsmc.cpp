//
// Created by dal-implementer on 2026/9/20.
//

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <dal/platform/platform.hpp>

#include <dal/math/distribution/black.hpp>
#include <dal/math/operators.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/script/diagnostics.hpp>
#include <dal/script/lsmc.hpp>
#include <dal/script/preparation.hpp>
#include <dal/script/simulation.hpp>
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
    for (size_t i = 0; i < n; ++i)
        ASSERT_NEAR(RegressionPredict(regression, x[i]), targets[i], 1e-8);
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
    ASSERT_TRUE(regression.degenerate_);
    ASSERT_EQ(regression.degenerateReason_, "IllConditioned");
    ASSERT_EQ(regression.basisDegree_, 0);
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

//  ---------------------------------------------------------------------------
//  LSMC driver: European limit (exercise only at maturity == European put)
//  ---------------------------------------------------------------------------

namespace {
    struct LsmcRun_ {
        double pv_;
        LsmcDiagnostics_ diagnostics_;
    };

    LsmcRun_ RunLsmc(const ScriptProductData_& product, const Handle_<ModelData_>& modelData, size_t nPaths, int degree = 3, bool compiled = false) {
        MonteCarloSettings_ simulation;
        simulation.lsmcBasisDegree_ = degree;
        simulation.compiled_ = compiled;
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
    const auto run = RunLsmc(ExerciseOnlyProduct(exerciseDates), StandardModel(), EUROPEAN_LIMIT_PATHS);
    ASSERT_NEAR(run.pv_, pde, 0.005 * SPOT);
}

TEST(ScriptExerciseLSMCTest, TestConvergenceBandAndTrend) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const Vector_<Date_> exerciseDates{Date_(2027, 9, 20), Date_(2028, 3, 20)};
    const double pde = BermudanPutPDE(SPOT, VOL, RATE, DIV, STRIKE, {YearFracTo(exerciseDates[0]), YearFracTo(exerciseDates[1])}, 2000, 2000);
    //  Degree 4: the degree-3 fit bias of this two-date product is size independent (the
    //  error is flat at ~0.075 across path counts), which turns the single-point trend
    //  comparison into noise vs noise; degree 4 shows the sampling-noise reduction the
    //  trend clause targets while staying well inside the plan's acceptance band.
    double errorAt64k = std::numeric_limits<double>::max();
    for (size_t paths : {1u << 16, 1u << 17, 1u << 18}) {
        const auto run = RunLsmc(ExerciseOnlyProduct(exerciseDates), StandardModel(), paths, 4);
        const double error = std::abs(run.pv_ - pde);
        ASSERT_LT(error, std::max(3.0 * run.diagnostics_.StandardError(), 0.0075 * SPOT));
        if (paths == (1u << 16))
            errorAt64k = error;
        if (paths == (1u << 18))
            ASSERT_LE(error, errorAt64k);
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
        const auto single = RunLsmc(product, StandardModel(), 1u << 16, 3, compiled);
        pool.pool_->Start(std::max(4u, static_cast<unsigned>(pool.threads_)), true);
        const auto multi = RunLsmc(product, StandardModel(), 1u << 16, 3, compiled);
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

TEST(ScriptExerciseLSMCTest, TestDegenerateIllConditioned) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    //  5y at 50% vol puts an extreme right-tail outlier into the regressors; the degree-8
    //  Gram diagonal ratio crosses the conditioning guard
    const Handle_<ModelData_> wild(new BSModelData_("bs", SPOT, 0.5, RATE, DIV));
    const auto run = RunLsmc(ExerciseOnlyProduct({Date_(2031, 9, 20)}), wild, 4096, 8);
    ASSERT_EQ(run.diagnostics_.events_.size(), 1u);
    ASSERT_TRUE(run.diagnostics_.events_[0].degenerate_);
    ASSERT_EQ(run.diagnostics_.events_[0].degenerateReason_, "IllConditioned");
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

//  ---------------------------------------------------------------------------
//  Execution-mode gates: AAD valuation of EXERCISE arrives in T4 (compiled joined in T3)
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

TEST(ScriptExerciseLSMCTest, TestAadGate) {
    const auto date = XGLOBAL::SetEvaluationDateInScope(EvalDate());
    const auto product = ExerciseOnlyProduct({Date_(2027, 9, 20)});
    AssertUnsupportedMode([&] {
        MonteCarloSettings_ simulation;
        simulation.enableAad_ = true;
        static_cast<void>(MCSimulation<AAD::Number_>(product, StandardModel(), 128, ScriptValuationSettings_(), simulation));
    });
    { //  the compiled engine joined the LSMC driver in T3 and values the same product
        MonteCarloSettings_ simulation;
        simulation.compiled_ = true;
        const auto compiled = MCSimulation<double>(product, StandardModel(), 128, ScriptValuationSettings_(), simulation);
        const auto treeWalk = MCSimulation<double>(product, StandardModel(), 128, ScriptValuationSettings_(), MonteCarloSettings_());
        ASSERT_NEAR(compiled.aggregated_, treeWalk.aggregated_, 1e-8 * 128);
    }
}
