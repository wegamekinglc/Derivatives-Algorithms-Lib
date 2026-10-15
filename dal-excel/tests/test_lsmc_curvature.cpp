//
// Created by Codex on 2026/10/11.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

#include <dal-excel/src/__lsmccurvature.hpp>
#include <dal-excel/src/__script_test_api.hpp>
#include <dal-public/src/script.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/platform/initall.hpp>
#include <dal/storage/json.hpp>

#include <script_test_observers.hpp>

using namespace Dal;

namespace {
    void InitializeRuntime() {
        RegisterAll_::Init();
#ifdef _WIN32
        Excel::ScriptTestInitialize(1);
#endif
    }

    auto Plan(Script::MonteCarloSettings_ simulation = DefaultRiskMonteCarloSettings()) {
        InitializeRuntime();
        const auto product =
            NewScriptProduct("sealed", {Cell_("SCALE"), Cell_(Date_(2027, 1, 2))}, {"2", "EXERCISE SCALE * FIX(EQ[EXCEL_LSMC]) ^ 2"});
        Script::ScriptValuationSettings_ value;
        value.evaluationDate_ = Date_(2026, 1, 2);
        const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("valuation", value));
        const Handle_<StorableMonteCarloSettings_> settings(new StorableMonteCarloSettings_("simulation", simulation));
        Handle_<StorableBlackScholesLsmcPlan_> plan;
        BlackScholesLsmcPlan_New("plan", product, valuation, settings, &plan);
        return plan;
    }

    Matrix_<Cell_> Point(const Vector_<>& values = {10, 0.2, 0.03, 0.01, 2}, bool column = false) {
        Matrix_<Cell_> cells(column ? static_cast<int>(values.size()) : 1, column ? 1 : static_cast<int>(values.size()));
        for (int index = 0; index < static_cast<int>(values.size()); ++index)
            cells(column ? index : 0, column ? 0 : index) = values[index];
        return cells;
    }

    auto Bumps(int rows = 2) {
        AAD::BumpOverAADRequest_ value;
        value.directions_ = Matrix_<>(rows, 5, 0.0);
        if (rows > 0) {
            value.directions_(0, 0) = 1.0;
            value.directions_(0, 4) = 0.5;
        }
        if (rows > 1) {
            value.directions_(1, 0) = -2.0;
            value.directions_(1, 4) = -1.0;
        }
        value.steps_ = Vector_<>(rows, 0.01);
        if (rows > 1)
            value.steps_[1] = 0.005;
        return Handle_<StorableBumpOverAADRequest_>(new StorableBumpOverAADRequest_("bumps", value));
    }

    auto Curvature(const Handle_<StorableBlackScholesLsmcPlan_>& plan = Plan(), const Handle_<StorableBumpOverAADRequest_>& bumps = Bumps()) {
        Handle_<StorableBlackScholesLsmcCurvatureResult_> result;
        BlackScholesLsmcCurvatureResult_New("result", plan, Point(), 33.0, bumps, &result);
        return result;
    }

    const Cell_& Field(const Matrix_<Cell_>& cells, const char* name) {
        for (int row = 0; row < cells.Rows(); ++row)
            if (Cell::ToString(cells(row, 0)) == name)
                return cells(row, 1);
        THROW(String_("Missing field ") + name);
    }

    void CheckVectorCells(const Matrix_<Cell_>& cells, const Vector_<>& values) {
        ASSERT_EQ(cells.Cols(), 1);
        if (values.empty()) {
            ASSERT_EQ(cells.Rows(), 1);
            ASSERT_TRUE(Cell::IsEmpty(cells(0, 0)));
            return;
        }
        ASSERT_EQ(cells.Rows(), values.size());
        for (int row = 0; row < cells.Rows(); ++row)
            ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(row, 0)), values[row]);
    }

    template <class F_> void CheckError(F_&& function, std::initializer_list<const char*> fields) {
        try {
            function();
            FAIL() << "Expected input rejection";
        } catch (const Exception_& error) {
            for (const auto* field : fields)
                ASSERT_NE(std::string(error.what()).find(field), std::string::npos) << error.what();
        }
    }

    Vector_<> PricingNormals(size_t offset, size_t count) {
        auto random = Script::CreateRNG("sobol", 1, false);
        random->SkipTo(offset);
        Vector_<> normals(count), next(1);
        for (size_t path = 0; path < count; ++path) {
            random->FillNormal(&next);
            normals[path] = next[0];
        }
        return normals;
    }

    struct ObserveWork_ {
#ifdef _WIN32
        Detail::FixingReadObserver_*& history_ = Excel::ScriptTestFixingObserver();
        Script::Detail::SimulationObserver_*& workers_ = Excel::ScriptTestSimulationObserver();
#else
        Detail::FixingReadObserver_*& history_ = Detail::FixingReadObserver();
        Script::Detail::SimulationObserver_*& workers_ = Script::Detail::SimulationObserver();
#endif
        Detail::FixingReadObserver_* previousHistory_ = history_;
        Script::Detail::SimulationObserver_* previousWorkers_ = workers_;
        ObserveWork_(Detail::FixingReadObserver_* history, Script::Detail::SimulationObserver_* workers) {
            history_ = history;
            workers_ = workers;
        }
        ~ObserveWork_() {
            history_ = previousHistory_;
            workers_ = previousWorkers_;
        }
    };

    Vector_<> PolynomialGradient(const Vector_<>& point, double time, double normal) {
        const double value = point[4] * point[0] * point[0] *
                             std::exp((point[2] - 2.0 * point[3] - point[1] * point[1]) * time + 2.0 * point[1] * std::sqrt(time) * normal);
        return {2.0 * value / point[0], 2.0 * (std::sqrt(time) * normal - point[1] * time) * value, time * value, -2.0 * time * value,
                value / point[4]};
    }

    Vector_<> MeanGradient(const Vector_<>& point, const Vector_<>& normals) {
        Vector_<> gradient(5, 0.0);
        for (double normal : normals) {
            const auto path = PolynomialGradient(point, 1.0, normal);
            for (int coordinate = 0; coordinate < 5; ++coordinate)
                gradient[coordinate] += path[coordinate] / static_cast<double>(normals.size());
        }
        return gradient;
    }

    void CheckPolynomialCurvature(const Script::BlackScholesLsmcCurvatureResult_& result) {
        const auto normals = PricingNormals(result.Execution().trainingPaths_, result.Execution().pathsPerReplicate_);
        const auto gradient = MeanGradient(result.Point(), normals);
        ASSERT_NEAR(result.Value(), result.Point()[4] * gradient[4], 1e-10);
        for (int coordinate = 0; coordinate < 5; ++coordinate)
            ASSERT_NEAR(result.Gradient()[coordinate], gradient[coordinate], 1e-10);
        for (int row = 0; row < result.Directions().Rows(); ++row) {
            auto plus = result.Point(), minus = result.Point();
            for (int coordinate = 0; coordinate < 5; ++coordinate) {
                plus[coordinate] += result.Steps()[row] * result.Directions()(row, coordinate);
                minus[coordinate] -= result.Steps()[row] * result.Directions()(row, coordinate);
            }
            const auto plusGradient = MeanGradient(plus, normals), minusGradient = MeanGradient(minus, normals);
            for (int coordinate = 0; coordinate < 5; ++coordinate)
                ASSERT_NEAR(result.HessianProducts()(row, coordinate),
                            (plusGradient[coordinate] - minusGradient[coordinate]) / (2.0 * result.Steps()[row]), 1e-8);
        }
    }
} // namespace

TEST(ExcelLsmcCurvatureTest, TestIndependentFuturePolynomialValueGradientAndMixedProduct) {
    RegisterAll_::Init();
#ifdef _WIN32
    Excel::ScriptTestInitialize(1);
#endif
    const auto product = NewScriptProduct("exercise", {Cell_("SCALE"), Cell_(Date_(2026, 1, 3))}, {"2", "EXERCISE SCALE * FIX(EQ[EXCEL_LSMC]) ^ 2"});
    Script::ScriptValuationSettings_ valuationValue;
    valuationValue.evaluationDate_ = Date_(2026, 1, 2);
    const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("valuation", valuationValue));
    Handle_<StorableBlackScholesLsmcPlan_> plan;
    BlackScholesLsmcPlan_New("plan", product, valuation, {}, &plan);
    Matrix_<Cell_> point(1, 5);
    const Vector_<> numbers{10.0, 0.0, 0.0, 0.0, 2.0};
    for (int column = 0; column < 5; ++column)
        point(0, column) = numbers[column];
    AAD::BumpOverAADRequest_ request;
    request.directions_ = Matrix_<>(1, 5, 0.0);
    request.directions_(0, 0) = 1.0;
    request.directions_(0, 4) = 0.5;
    request.steps_ = {0.01};
    const Handle_<StorableBumpOverAADRequest_> bumps(new StorableBumpOverAADRequest_("bumps", request));
    Handle_<StorableBlackScholesLsmcCurvatureResult_> result;
    BlackScholesLsmcCurvatureResult_New("result", plan, point, 1.0, bumps, &result);
    ASSERT_NEAR(result->val_.Curvature().Value(), 200.0, 1e-10);
    const double time = 1.0 / 365.0;
    auto random = Script::CreateRNG("sobol", 1, false);
    random->SkipTo(1);
    Vector_<> normals(1);
    random->FillNormal(&normals);
    const auto expected = PolynomialGradient(numbers, time, normals[0]);
    for (int coordinate = 0; coordinate < 5; ++coordinate)
        ASSERT_NEAR(result->val_.Curvature().Gradient()[coordinate], expected[coordinate], 1e-10);
    ASSERT_NEAR(result->val_.Curvature().HessianProducts()(0, 0), 14.0, 1e-9);
    ASSERT_NEAR(result->val_.Curvature().HessianProducts()(0, 4), 20.0, 1e-9);
    auto plus = numbers, minus = numbers;
    for (int coordinate = 0; coordinate < 5; ++coordinate) {
        plus[coordinate] += request.steps_[0] * request.directions_(0, coordinate);
        minus[coordinate] -= request.steps_[0] * request.directions_(0, coordinate);
    }
    const auto plusGradient = PolynomialGradient(plus, time, normals[0]);
    const auto minusGradient = PolynomialGradient(minus, time, normals[0]);
    for (int coordinate = 0; coordinate < 5; ++coordinate)
        ASSERT_NEAR(result->val_.Curvature().HessianProducts()(0, coordinate),
                    (plusGradient[coordinate] - minusGradient[coordinate]) / (2.0 * request.steps_[0]), 1e-9);
    ASSERT_EQ(result->val_.Curvature().Execution().gradientEvaluations_, 3U);
}

TEST(ExcelLsmcCurvatureTest, TestCompletePassivePlanResultSettingsAndPolicyProjectionsDoNoWork) {
    const auto plan = Plan();
    const auto result = Curvature(plan);
    const auto& native = result->val_.Curvature();
    ASSERT_EQ(native.BasePolicy().size(), 1U);
    const auto originalPolicy = native.BasePolicy()[0];
    Script::TestSupport::RejectFixingReads_ history;
    Script::TestSupport::RejectSubmissions_ submissions;
    const ObserveWork_ observe(&history, &submissions);

    Handle_<ScriptProductData_> product;
    BlackScholesLsmcPlan_Get_Product(plan, &product);
    ASSERT_EQ(product->EventTexts(), plan->val_.Contract().EventTexts());
    ASSERT_EQ(product->Dates().size(), 2U);
    ASSERT_EQ(Cell::ToString(product->Dates()[0]), "SCALE");
    ASSERT_EQ(Cell::ToDate(product->Dates()[1]), Date_(2027, 1, 2));
    Handle_<StorableScriptValuationSettings_> valuation;
    BlackScholesLsmcPlan_Get_Valuation(plan, &valuation);
    ASSERT_EQ(valuation->val_.evaluationDate_, Date_(2026, 1, 2));
    Handle_<StorableMonteCarloSettings_> simulation;
    BlackScholesLsmcPlan_Get_Simulation(plan, &simulation);
    ASSERT_TRUE(simulation->val_.enableAad_);
    Matrix_<Cell_> cells;
    MonteCarloSettings_Get_Configuration(simulation, &cells);
    ASSERT_EQ(cells.Rows(), 16);
    ASSERT_EQ(Cell::ToString(Field(cells, "method")), "sobol");
    ASSERT_TRUE(Cell::IsBool(Field(cells, "enable_aad")));
    for (const auto* key :
         {"compiled", "lsmc_training_paths", "lsmc_validation_paths", "lsmc_rqmc_replicates", "lsmc_training_seed", "lsmc_pricing_seed"})
        ASSERT_TRUE(Cell::IsEmpty(Field(cells, key))) << key;
    Vector_<String_> labels;
    BlackScholesLsmcPlan_Get_ParameterLabels(plan, &labels);
    ASSERT_EQ(labels, (Vector_<String_>{"spot", "vol", "rate", "div", "SCALE"}));
    BlackScholesLsmcPlan_Get_ScriptConstants(plan, &cells);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 2.0);
    BlackScholesLsmcPlan_Get_Observations(plan, &cells);
    ASSERT_EQ(cells.Cols(), 4);
    ASSERT_EQ(Cell::ToString(cells(0, 0)), "index");
    ASSERT_EQ(cells.Rows(), 2);
    ASSERT_FALSE(std::get<bool>(cells(1, 2).val_));
    ASSERT_TRUE(Cell::IsEmpty(cells(1, 3)));
    BlackScholesLsmcPlan_Get_EventDates(plan, &cells);
    ASSERT_EQ(Cell::ToDate(cells(0, 0)), Date_(2027, 1, 2));
    BlackScholesLsmcPlan_Get_TimeLine(plan, &cells);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 1.0);

    double value = 0;
    BlackScholesLsmcCurvatureResult_Get_Value(result, &value);
    ASSERT_DOUBLE_EQ(value, native.Value());
    BlackScholesLsmcCurvatureResult_Get_Gradient(result, &cells);
    ASSERT_EQ(cells.Rows(), 5);
    for (int coordinate = 0; coordinate < 5; ++coordinate)
        ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(coordinate, 0)), native.Gradient()[coordinate]);
    cells(0, 0) = 999.0;
    ASSERT_NE(native.Gradient()[0], 999.0);
    BlackScholesLsmcCurvatureResult_Get_ParameterLabels(result, &labels);
    ASSERT_EQ(labels, plan->val_.ParameterLabels());
    BlackScholesLsmcCurvatureResult_Get_Point(result, &cells);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 10.0);
    BlackScholesLsmcCurvatureResult_Get_Directions(result, &cells);
    ASSERT_EQ(cells.Rows(), 2);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(1, 4)), -1.0);
    BlackScholesLsmcCurvatureResult_Get_Steps(result, &cells);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(1, 0)), 0.005);
    BlackScholesLsmcCurvatureResult_Get_HessianProducts(result, &cells);
    ASSERT_EQ(cells.Rows(), 2);
    ASSERT_EQ(cells.Cols(), 5);
    for (int coordinate = 0; coordinate < 5; ++coordinate) {
        ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, coordinate)), native.HessianProducts()(0, coordinate));
        ASSERT_NEAR(Cell::ToDouble(cells(1, coordinate)), -2.0 * Cell::ToDouble(cells(0, coordinate)), 1e-8);
    }
    BlackScholesLsmcCurvatureResult_Get_Shape(result, &cells);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 0)), 2.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(0, 1)), 5.0);
    BlackScholesLsmcCurvatureResult_Get_Execution(result, &cells);
    ASSERT_EQ(cells.Rows(), 10);
    ASSERT_EQ(Cell::ToString(Field(cells, "method")), "BumpOverFrozenNativeLsmcAAD");
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "gradient_evaluations")), 5.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "numeric_payload_bytes")), 264.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "training_paths")), 33.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "paths_per_replicate")), 33.0);
    ASSERT_TRUE(Cell::IsEmpty(Field(cells, "recording_capacity_budget_bytes")));
    Handle_<StorableBlackScholesLsmcPlan_> copiedPlan;
    BlackScholesLsmcCurvatureResult_Get_Plan(result, &copiedPlan);
    ASSERT_NE(copiedPlan.get(), plan.get());
    ASSERT_EQ(copiedPlan->val_.Contract().EventTexts(), product->EventTexts());
    BlackScholesLsmcCurvatureResult_Get_Simulation(result, &simulation);
    ASSERT_TRUE(simulation->val_.enableAad_);
    BlackScholesLsmcCurvatureResult_Get_BasePolicyCount(result, &value);
    ASSERT_DOUBLE_EQ(value, 1.0);
    Handle_<StorableLsmcExercisePolicy_> policy;
    BlackScholesLsmcCurvatureResult_Get_BasePolicy(result, 0.0, &policy);
    LsmcExercisePolicy_Get_Coefficients(policy, &cells);
    CheckVectorCells(cells, originalPolicy.coefficients_);
    LsmcExercisePolicy_Get_NormalizationMeans(policy, &cells);
    CheckVectorCells(cells, originalPolicy.means_);
    LsmcExercisePolicy_Get_NormalizationSigmas(policy, &cells);
    CheckVectorCells(cells, originalPolicy.sigmas_);
    LsmcExercisePolicy_Get_BasisPowers(policy, &cells);
    if (originalPolicy.powers_.empty()) {
        ASSERT_EQ(cells.Rows(), 1);
        ASSERT_EQ(cells.Cols(), 1);
        ASSERT_TRUE(Cell::IsEmpty(cells(0, 0)));
    } else {
        ASSERT_EQ(cells.Cols(), 3);
        ASSERT_EQ(cells.Rows(), originalPolicy.powers_.size());
        for (int row = 0; row < cells.Rows(); ++row)
            for (int column = 0; column < 3; ++column)
                ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(row, column)), originalPolicy.powers_[row][column]);
    }
    LsmcExercisePolicy_Get_Configuration(policy, &cells);
    ASSERT_EQ(cells.Rows(), 15);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "basis_degree")), originalPolicy.basisDegree_);
    ASSERT_EQ(std::get<bool>(Field(cells, "degenerate").val_), originalPolicy.degenerate_);
    ASSERT_EQ(Cell::ToString(Field(cells, "degenerate_reason")), originalPolicy.degenerateReason_);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "mean")), originalPolicy.mean_);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "sigma")), originalPolicy.sigma_);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "condition_path_count")), originalPolicy.numCondTrue_);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "effective_rank")), originalPolicy.effectiveRank_);
    ASSERT_EQ(Cell::ToString(Field(cells, "solver")), originalPolicy.solver_);
    ASSERT_EQ(Cell::ToString(Field(cells, "fallback_reason")), originalPolicy.fallbackReason_);
    ASSERT_TRUE(Cell::IsEmpty(Field(cells, "validation_mse")));
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "coefficients_length")), originalPolicy.coefficients_.size());
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "normalization_means_length")), originalPolicy.means_.size());
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "normalization_sigmas_length")), originalPolicy.sigmas_.size());
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "basis_power_rows")), originalPolicy.powers_.size());
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "basis_power_columns")), 3.0);
    ASSERT_EQ(submissions.calls_, 0U);
    ASSERT_EQ(history.historyCalls_, 0U);
    ASSERT_EQ(history.fixingCalls_, 0U);
}

TEST(ExcelLsmcCurvatureTest, TestBothPolicyModesAndProgramsMatchIndependentEveryGradientAndActualStepProducts) {
    auto request = Bumps()->val_;
    request.directions_(0, 1) = 0.01;
    request.directions_(0, 2) = 0.002;
    request.directions_(0, 3) = -0.003;
    for (int coordinate = 0; coordinate < 5; ++coordinate)
        request.directions_(1, coordinate) = -2.0 * request.directions_(0, coordinate);
    const Handle_<StorableBumpOverAADRequest_> bumps(new StorableBumpOverAADRequest_("mixed", request));
    for (const auto* mode : {"Frozen", "RetrainedBump"}) {
        for (bool compiled : {false, true}) {
            auto simulation = DefaultRiskMonteCarloSettings();
            simulation.compiled_ = compiled;
            simulation.lsmcTrainingPaths_ = 64;
            simulation.lsmcPolicyRiskMode_ = mode;
            simulation.lsmcPolicyBumpRelative_ = 0.002;
            const auto result = Curvature(Plan(simulation), bumps);
            CheckPolynomialCurvature(result->val_.Curvature());
            const String_ method = String_(mode) == "Frozen" ? "BumpOverFrozenNativeLsmcAAD" : "BumpOverRetrainedNativeLsmcPolicySecant";
            ASSERT_EQ(result->val_.Curvature().Execution().method_, method);
            ASSERT_EQ(result->val_.Curvature().Execution().gradientEvaluations_, 5U);
            ASSERT_EQ(result->val_.Plan().Simulation().lsmcPolicyBumpRelative_, 0.002);
            ASSERT_EQ(result->val_.Plan().Simulation().compiled_, compiled);
        }
    }
}

TEST(ExcelLsmcCurvatureTest, TestCompleteSimulationOptionalZeroAndValidationPolicyMetadataRoundTrip) {
    auto original = DefaultRiskMonteCarloSettings();
    original.useBb_ = true;
    original.smooth_ = 0.25;
    original.compiled_ = false;
    original.lsmcBasisDegree_ = 2;
    original.lsmcTrainingPaths_ = 64;
    original.lsmcValidationPaths_ = 64;
    original.lsmcRqmcReplicates_ = 2;
    original.lsmcTrainingSeed_ = 0;
    original.lsmcPricingSeed_ = 7;
    original.lsmcPolicyRiskMode_ = "RetrainedBump";
    original.lsmcPolicyBumpRelative_ = 0.002;
    original.normalPrecision_ = "Precise";
    const auto plan = Plan(original);
    original.enableAad_ = false;
    original.lsmcTrainingSeed_ = 99;
    Handle_<StorableMonteCarloSettings_> simulation;
    BlackScholesLsmcPlan_Get_Simulation(plan, &simulation);
    Matrix_<Cell_> cells;
    MonteCarloSettings_Get_Configuration(simulation, &cells);
    ASSERT_EQ(cells.Rows(), 16);
    ASSERT_EQ(Cell::ToString(Field(cells, "method")), "sobol");
    ASSERT_TRUE(std::get<bool>(Field(cells, "use_bb").val_));
    ASSERT_TRUE(std::get<bool>(Field(cells, "enable_aad").val_));
    ASSERT_FALSE(std::get<bool>(Field(cells, "compiled").val_));
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "smooth")), 0.25);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "lsmc_basis_degree")), 2.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "lsmc_training_paths")), 64.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "lsmc_validation_paths")), 64.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "lsmc_rqmc_replicates")), 2.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "lsmc_training_seed")), 0.0);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "lsmc_pricing_seed")), 7.0);
    ASSERT_EQ(Cell::ToString(Field(cells, "lsmc_policy_risk_mode")), "RetrainedBump");
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "lsmc_policy_bump_relative")), 0.002);
    ASSERT_EQ(Cell::ToString(Field(cells, "normal_precision")), "Precise");
    const auto result = Curvature(plan, Bumps(0));
    const auto& execution = result->val_.Curvature().Execution();
    ASSERT_EQ(execution.trainingPaths_, 64U);
    ASSERT_EQ(execution.validationPaths_, 64U);
    ASSERT_EQ(execution.pricingReplicates_, 2U);
    ASSERT_EQ(execution.pathsPerReplicate_, 33U);
    Handle_<StorableLsmcExercisePolicy_> policy;
    BlackScholesLsmcCurvatureResult_Get_BasePolicy(result, 0, &policy);
    ASSERT_TRUE(policy->val_.validationMse_.has_value());
    LsmcExercisePolicy_Get_Configuration(policy, &cells);
    ASSERT_DOUBLE_EQ(Cell::ToDouble(Field(cells, "validation_mse")), 0.0);
}

TEST(ExcelLsmcCurvatureTest, TestZeroDirectionsAndSeparateExactBudgetsRecoverAtomically) {
    const auto plan = Plan();
    auto result = Curvature(plan);
    const auto original = result;
    auto request = Bumps()->val_;
    request.numericPayloadBudgetBytes_ = 263;
    auto bumps = Handle_<StorableBumpOverAADRequest_>(new StorableBumpOverAADRequest_("budget", request));
    {
        Script::TestSupport::RejectSubmissions_ work;
        const ObserveWork_ observer(nullptr, &work);
        CheckError([&] { BlackScholesLsmcCurvatureResult_New("bad", plan, Point(), 33, bumps, &result); }, {"numeric payload budget"});
        ASSERT_EQ(work.calls_, 0);
    }
    ASSERT_EQ(result, original);
    request.numericPayloadBudgetBytes_ = 264;
    request.recordingCapacityBudgetBytes_ = 0;
    bumps.reset(new StorableBumpOverAADRequest_("capacity", request));
    CheckError([&] { BlackScholesLsmcCurvatureResult_New("bad", plan, Point(), 33, bumps, &result); }, {"Tape capacity budget exceeded", "limit=0"});
    ASSERT_EQ(result, original);
    request.recordingCapacityBudgetBytes_ = size_t(1) << 26;
    bumps.reset(new StorableBumpOverAADRequest_("exact", request));
    BlackScholesLsmcCurvatureResult_New("recovered", plan, Point(), 33, bumps, &result);
    ASSERT_EQ(result->val_.Curvature().Execution().numericPayloadBytes_, 264U);
    ASSERT_EQ(result->val_.Curvature().Execution().recordingCapacityBudgetBytes_, size_t(1) << 26);
    CheckPolynomialCurvature(result->val_.Curvature());
    result = Curvature(plan, Bumps(0));
    Matrix_<Cell_> cells;
    BlackScholesLsmcCurvatureResult_Get_HessianProducts(result, &cells);
    ASSERT_EQ(cells.Rows(), 1);
    ASSERT_EQ(cells.Cols(), 1);
    ASSERT_TRUE(Cell::IsEmpty(cells(0, 0)));
    BlackScholesLsmcCurvatureResult_Get_Shape(result, &cells);
    ASSERT_EQ(Cell::ToDouble(cells(0, 0)), 0.0);
    ASSERT_EQ(Cell::ToDouble(cells(0, 1)), 5.0);
    ASSERT_EQ(result->val_.Curvature().Execution().gradientEvaluations_, 1U);
    ASSERT_EQ(result->val_.Curvature().Execution().numericPayloadBytes_, 88U);
}

TEST(ExcelLsmcCurvatureTest, TestCompletePointPathsAndPolicyIndexRejectWithoutReplacingOutputs) {
    const auto plan = Plan();
    const auto bumps = Bumps();
    auto result = Curvature(plan);
    const auto original = result;
    for (const bool column : {false, true})
        for (const int index : {0, 2, 4}) {
            auto point = Point({10, 0.2, 0.03, 0.01, 2}, column);
            point(column ? index : 0, column ? 0 : index) = Cell_();
            ASSERT_THROW(BlackScholesLsmcCurvatureResult_New("bad", plan, point, 1, bumps, &result), Exception_);
        }
    for (const auto& invalid : {Cell_(true), Cell_("10"), Cell_(Date_(2026, 1, 2)), Cell_(std::numeric_limits<double>::infinity())}) {
        auto point = Point();
        point(0, 0) = invalid;
        ASSERT_THROW(BlackScholesLsmcCurvatureResult_New("bad", plan, point, 1, bumps, &result), Exception_);
    }
    for (const double paths : {0.0, -1.0, 1.5, 9007199254740992.0, std::numeric_limits<double>::infinity()})
        ASSERT_THROW(BlackScholesLsmcCurvatureResult_New("bad", plan, Point(), paths, bumps, &result), Exception_);
    ASSERT_THROW(BlackScholesLsmcCurvatureResult_New("bad", {}, Point(), 1, bumps, &result), Exception_);
    ASSERT_THROW(BlackScholesLsmcCurvatureResult_New("bad", plan, Point(), 1, {}, &result), Exception_);
    ASSERT_THROW(BlackScholesLsmcCurvatureResult_New("bad", plan, Matrix_<Cell_>(2, 3, Cell_(1.0)), 1, bumps, &result), Exception_);
    auto request = Bumps(0)->val_;
    request.numericPayloadBudgetBytes_ = 0;
    const Handle_<StorableBumpOverAADRequest_> zero(new StorableBumpOverAADRequest_("zero", request));
    CheckError([&] { BlackScholesLsmcCurvatureResult_New("large", plan, Point(), 2147483648.0, zero, &result); }, {"numeric payload budget"});
    CheckError([&] { BlackScholesLsmcCurvatureResult_New("limit", plan, Point(), 4294967295.0, bumps, &result); }, {"32-bit Sobol"});
    ASSERT_EQ(result, original);
    Handle_<StorableLsmcExercisePolicy_> policy;
    BlackScholesLsmcCurvatureResult_Get_BasePolicy(result, 0, &policy);
    const auto originalPolicy = policy;
    for (const double index : {-1.0, 0.5, 1.0, 9007199254740992.0, std::numeric_limits<double>::infinity()})
        ASSERT_THROW(BlackScholesLsmcCurvatureResult_Get_BasePolicy(result, index, &policy), Exception_);
    ASSERT_EQ(policy, originalPolicy);
}

TEST(ExcelLsmcCurvatureTest, TestRequiredPlanInputsNamesArchivesAndBumpedDomainsRejectAtomically) {
    auto plan = Plan();
    const auto originalPlan = plan;
    const auto& contract = plan->val_.Contract();
    const Handle_<ScriptProductData_> product(new ScriptProductData_(contract.Name(), contract.Dates(), contract.EventTexts(), contract.Settings()));
    Script::ScriptValuationSettings_ valuationValue;
    valuationValue.evaluationDate_ = Date_(2026, 1, 2);
    const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("valuation", valuationValue));
    ASSERT_THROW(BlackScholesLsmcPlan_New("bad", {}, valuation, {}, &plan), Exception_);
    ASSERT_THROW(BlackScholesLsmcPlan_New(String_(std::string("bad\0name", 8)), product, valuation, {}, &plan), Exception_);
    auto disabled = DefaultRiskMonteCarloSettings();
    disabled.enableAad_ = false;
    const Handle_<StorableMonteCarloSettings_> simulation(new StorableMonteCarloSettings_("disabled", disabled));
    CheckError([&] { BlackScholesLsmcPlan_New("disabled", product, valuation, simulation, &plan); }, {"native AAD preparation required"});
    const auto today = NewScriptProduct("today", {Cell_(Date_(2026, 1, 2))}, {"EXERCISE FIX(EQ[EXCEL_LSMC])"});
    ASSERT_THROW(BlackScholesLsmcPlan_New("today", today, valuation, {}, &plan), Exception_);
    ASSERT_EQ(plan, originalPlan);
    auto result = Curvature(plan);
    const auto original = result;
    auto request = Bumps(1)->val_;
    request.directions_(0, 1) = 1.0;
    request.steps_[0] = 0.3;
    const Handle_<StorableBumpOverAADRequest_> invalid(new StorableBumpOverAADRequest_("domain", request));
    {
        Script::TestSupport::RejectSubmissions_ work;
        const ObserveWork_ observer(nullptr, &work);
        CheckError([&] { BlackScholesLsmcCurvatureResult_New("bad", plan, Point(), 33, invalid, &result); }, {"direction=0", "minus"});
        ASSERT_EQ(work.calls_, 0);
    }
    ASSERT_THROW(BlackScholesLsmcCurvatureResult_New(String_(std::string("bad\0name", 8)), plan, Point(), 33, Bumps(), &result), Exception_);
    ASSERT_EQ(result, original);
    Handle_<StorableLsmcExercisePolicy_> policy;
    BlackScholesLsmcCurvatureResult_Get_BasePolicy(result, 0, &policy);
    ASSERT_THROW(JSON::WriteString(*plan), Exception_);
    ASSERT_THROW(JSON::WriteString(*result), Exception_);
    ASSERT_THROW(JSON::WriteString(*policy), Exception_);
    Matrix_<Cell_> cells(1, 1, Cell_(9.0));
    ASSERT_THROW(MonteCarloSettings_Get_Configuration({}, &cells), Exception_);
    ASSERT_THROW(BlackScholesLsmcPlan_Get_Observations({}, &cells), Exception_);
    ASSERT_THROW(BlackScholesLsmcCurvatureResult_Get_Execution({}, &cells), Exception_);
    ASSERT_THROW(LsmcExercisePolicy_Get_Configuration({}, &cells), Exception_);
    ASSERT_EQ(Cell::ToDouble(cells(0, 0)), 9.0);
    BlackScholesLsmcCurvatureResult_New("recovered", plan, Point(), 33, Bumps(), &result);
    CheckPolynomialCurvature(result->val_.Curvature());
}

TEST(ExcelLsmcCurvatureTest, TestHistoricalPreparationSealsFixingsWhileKeepingConstantsActive) {
    InitializeRuntime();
    Script::ScriptValuationSettings_ valuationValue;
    valuationValue.evaluationDate_ = Date_(2026, 1, 2);
    valuationValue.fixings_ =
        Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[EXCEL_LSMC_HISTORY]", {{DateTime_(Date_(2025, 1, 2), 0.0), 80.0}}}}));
    const Handle_<StorableScriptValuationSettings_> valuation(new StorableScriptValuationSettings_("history", valuationValue));
    const auto product = NewScriptProduct("history", {Cell_("SCALE"), Cell_(Date_(2025, 1, 2)), Cell_(Date_(2027, 1, 2))},
                                          {"2", "past = SCALE * FIX(EQ[EXCEL_LSMC_HISTORY])", "EXERCISE past"});
    Handle_<StorableBlackScholesLsmcPlan_> plan;
    BlackScholesLsmcPlan_New("sealed", product, valuation, {}, &plan);
    valuationValue.fixings_ =
        Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_({{"EQ[EXCEL_LSMC_HISTORY]", {{DateTime_(Date_(2025, 1, 2), 0.0), 999.0}}}}));
    Script::TestSupport::RejectFixingReads_ history;
    const ObserveWork_ observer(&history, nullptr);
    Handle_<StorableBlackScholesLsmcCurvatureResult_> result;
    BlackScholesLsmcCurvatureResult_New("history", plan, Point({100, 0.2, 0, 0, 3}), 33, Bumps(), &result);
    ASSERT_DOUBLE_EQ(result->val_.Curvature().Value(), 240.0);
    ASSERT_EQ(result->val_.Curvature().Gradient(), (Vector_<>{0, 0, -240, 0, 80}));
    for (int row = 0; row < 2; ++row) {
        ASSERT_NEAR(result->val_.Curvature().HessianProducts()(row, 2), row == 0 ? -40.0 : 80.0, 1e-8);
        for (const int coordinate : {0, 1, 3, 4})
            ASSERT_NEAR(result->val_.Curvature().HessianProducts()(row, coordinate), 0.0, 1e-8);
    }
    Matrix_<Cell_> cells;
    BlackScholesLsmcPlan_Get_Observations(plan, &cells);
    ASSERT_TRUE(Cell::ToBool(cells(1, 2)));
    ASSERT_DOUBLE_EQ(Cell::ToDouble(cells(1, 3)), 80.0);
    ASSERT_EQ(history.historyCalls_, 0);
    ASSERT_EQ(history.fixingCalls_, 0);
}

TEST(ExcelLsmcCurvatureTest, TestPassiveQueriesAndNestedExecutionPreserveCallerGraphAndSeed) {
    const auto plan = Plan();
    auto result = Curvature(plan);
    const auto original = result;
#ifdef _WIN32
    double adjoint = 0;
    Excel::ScriptTestWithRecording(
        [&](size_t nodes) {
#else
    AAD::Clear(*AAD::Tape());
    const auto mode = AAD::SetNumResultsForAAD(true, 3);
    AAD::RecordingScope_ recording;
    AAD::Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    AAD::Number_ square = input * input;
    AAD::NativeOperations_::SetSeed(square, 7.0, 2);
    const auto nodes = AAD::Tape()->nodes_.Size();
#endif
            Matrix_<Cell_> cells;
            BlackScholesLsmcPlan_Get_Observations(plan, &cells);
            BlackScholesLsmcCurvatureResult_Get_Execution(result, &cells);
            Handle_<StorableLsmcExercisePolicy_> policy;
            BlackScholesLsmcCurvatureResult_Get_BasePolicy(result, 0, &policy);
            LsmcExercisePolicy_Get_Configuration(policy, &cells);
            ASSERT_THROW(BlackScholesLsmcCurvatureResult_New("nested", plan, Point(), 33, Bumps(), &result), Exception_);
            ASSERT_EQ(result, original);
#ifdef _WIN32
            ASSERT_EQ(Excel::ScriptTestTapeNodeCount(), nodes);
        },
        false, &adjoint);
    ASSERT_EQ(adjoint, 4.0);
#else
    ASSERT_EQ(AAD::Tape()->nodes_.Size(), nodes);
    ASSERT_EQ(AAD::Tape()->numAdj_, 3);
    ASSERT_EQ(AAD::NativeOperations_::ReadAdjoint(square, 2), 7.0);
    recording.FinishRecording();
    recording.Reverse();
    ASSERT_EQ(AAD::NativeOperations_::ReadAdjoint(input, 2), 28.0);
    recording.Close();
#endif
}
