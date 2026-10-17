//
// Created by Codex on 2026/10/11.
//

#include "__lsmccurvature.hpp"

#include "__curvatureinput.hpp"
#include "__mcriskrows.hpp"

// clang-format off
/*IF--------------------------------------------------------------------------
public BlackScholesLsmcPlan_New
    Seal native AAD LSMC preparation with copied observations and settings
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "BlackScholesLsmcPlan_New; name");
+argName = "product"; Excel::ValidateRiskRequestText(xl_product, "BlackScholesLsmcPlan_New; product");
+xl_product = Excel::ScriptScalarInput(xl_product);
+argName = "valuation"; Excel::ValidateRiskRequestText(xl_valuation, "BlackScholesLsmcPlan_New; valuation");
+xl_valuation = Excel::ScriptScalarInput(xl_valuation);
+argName = "simulation"; Excel::ValidateRiskRequestText(xl_simulation, "BlackScholesLsmcPlan_New; simulation");
+xl_simulation = Excel::ScriptScalarInput(xl_simulation);
product is handle ScriptProductData
    Script contract with strictly future EXERCISE dates
&optional
valuation is handle StorableScriptValuationSettings
    Blank captures native valuation defaults
simulation is handle StorableMonteCarloSettings
    Blank selects native risk defaults with AAD enabled
&outputs
plan is handle StorableBlackScholesLsmcPlan
    Immutable copied contract, full simulation settings and captured observations
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcCurvatureResult_New
    Evaluate finite-step LSMC curvature under Frozen or RetrainedBump policies
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "BlackScholesLsmcCurvatureResult_New; name");
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesLsmcCurvatureResult_New; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
+argName = "bumps"; Excel::ValidateRiskRequestText(xl_bumps, "BlackScholesLsmcCurvatureResult_New; bumps");
+xl_bumps = Excel::ScriptScalarInput(xl_bumps);
+argName = "point"; Excel::ValidateCurvatureNumbers(xl_point, "BlackScholesLsmcCurvatureResult_New", "point");
+const Excel::ScriptSettingsInput_ normalizedPoint(xl_point); xl_point = normalizedPoint.Get();
+argName = "paths"; Excel::ValidateCurvatureNumber(*Excel::ScriptScalarInput(xl_paths), "BlackScholesLsmcCurvatureResult_New; paths; ");
+xl_paths = Excel::ScriptScalarInput(xl_paths);
+const Excel::ScriptSettingsInput_ normalizedpaths(xl_paths); xl_paths = normalizedpaths.Get();
plan is handle StorableBlackScholesLsmcPlan
    Sealed native LSMC plan with AAD enabled
point is cell[][]
    Complete finite numeric row or column; blank gaps reject
paths is number
    Positive exact size_t integer; combined Sobol blocks must fit native limits
bumps is handle StorableBumpOverAADRequest
    Full signed directions, positive actual steps and separate numeric and recording caps
&outputs
result is handle StorableBlackScholesLsmcCurvatureResult
    Completed owning value, gradient, products, base policies and execution metadata
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MonteCarloSettings_Get_Configuration
    Copy MonteCarloSettings Get Configuration
&inputs
simulation is handle StorableMonteCarloSettings
    Stored immutable simulation
+argName = "simulation"; Excel::ValidateRiskRequestText(xl_simulation, "MonteCarloSettings_Get_Configuration; simulation");
+xl_simulation = Excel::ScriptScalarInput(xl_simulation);
&outputs
configuration is cell[][]
    All sixteen simulation fields; absent optional values blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcPlan_Get_Product
    Copy BlackScholesLsmcPlan Get Product
&inputs
plan is handle StorableBlackScholesLsmcPlan
    Stored immutable plan
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesLsmcPlan_Get_Product; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
&outputs
product is handle ScriptProductData
    Copied sealed script contract
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcPlan_Get_Valuation
    Copy BlackScholesLsmcPlan Get Valuation
&inputs
plan is handle StorableBlackScholesLsmcPlan
    Stored immutable plan
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesLsmcPlan_Get_Valuation; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
&outputs
valuation is handle StorableScriptValuationSettings
    Copied valuation settings and captured history
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcPlan_Get_Simulation
    Copy BlackScholesLsmcPlan Get Simulation
&inputs
plan is handle StorableBlackScholesLsmcPlan
    Stored immutable plan
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesLsmcPlan_Get_Simulation; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
&outputs
simulation is handle StorableMonteCarloSettings
    Copied full native simulation settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcPlan_Get_ParameterLabels
    Copy BlackScholesLsmcPlan Get ParameterLabels
&inputs
plan is handle StorableBlackScholesLsmcPlan
    Stored immutable plan
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesLsmcPlan_Get_ParameterLabels; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
&outputs
parameterlabels is string[]
    Complete point order: spot, vol, rate, div, then script constants
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcPlan_Get_ScriptConstants
    Copy BlackScholesLsmcPlan Get ScriptConstants
&inputs
plan is handle StorableBlackScholesLsmcPlan
    Stored immutable plan
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesLsmcPlan_Get_ScriptConstants; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
&outputs
scriptconstants is cell[][]
    Copied script constants; no constants spill one empty cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcPlan_Get_Observations
    Copy BlackScholesLsmcPlan Get Observations
&inputs
plan is handle StorableBlackScholesLsmcPlan
    Stored immutable plan
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesLsmcPlan_Get_Observations; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
&outputs
observations is cell[][]
    Header then index, fixing time, historical flag and optional captured value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcPlan_Get_EventDates
    Copy BlackScholesLsmcPlan Get EventDates
&inputs
plan is handle StorableBlackScholesLsmcPlan
    Stored immutable plan
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesLsmcPlan_Get_EventDates; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
&outputs
eventdates is cell[][]
    Complete prepared event dates; policy ordinals are not event ordinals
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcPlan_Get_TimeLine
    Copy BlackScholesLsmcPlan Get TimeLine
&inputs
plan is handle StorableBlackScholesLsmcPlan
    Stored immutable plan
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesLsmcPlan_Get_TimeLine; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
&outputs
timeline is cell[][]
    Complete prepared event times in years
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcCurvatureResult_Get_Value
    Copy BlackScholesLsmcCurvatureResult Get Value
&inputs
result is handle StorableBlackScholesLsmcCurvatureResult
    Stored immutable result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_Value; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
value is number
    Completed sample value under the declared LSMC policy mode
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcCurvatureResult_Get_Gradient
    Copy BlackScholesLsmcCurvatureResult Get Gradient
&inputs
result is handle StorableBlackScholesLsmcCurvatureResult
    Stored immutable result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_Gradient; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
gradient is cell[][]
    Complete raw gradient in plan parameter order
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcCurvatureResult_Get_ParameterLabels
    Copy BlackScholesLsmcCurvatureResult Get ParameterLabels
&inputs
result is handle StorableBlackScholesLsmcCurvatureResult
    Stored immutable result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_ParameterLabels; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
parameterlabels is string[]
    Complete point order: spot, vol, rate, div, then script constants
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcCurvatureResult_Get_Point
    Copy BlackScholesLsmcCurvatureResult Get Point
&inputs
result is handle StorableBlackScholesLsmcCurvatureResult
    Stored immutable result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_Point; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
point is cell[][]
    Copied complete parameter point
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcCurvatureResult_Get_Directions
    Copy BlackScholesLsmcCurvatureResult Get Directions
&inputs
result is handle StorableBlackScholesLsmcCurvatureResult
    Stored immutable result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_Directions; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
directions is cell[][]
    Copied signed direction rows; zero rows spill one empty cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcCurvatureResult_Get_Steps
    Copy BlackScholesLsmcCurvatureResult Get Steps
&inputs
result is handle StorableBlackScholesLsmcCurvatureResult
    Stored immutable result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_Steps; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
steps is cell[][]
    Copied positive actual outer steps
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcCurvatureResult_Get_HessianProducts
    Copy BlackScholesLsmcCurvatureResult Get HessianProducts
&inputs
result is handle StorableBlackScholesLsmcCurvatureResult
    Stored immutable result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_HessianProducts; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
hessianproducts is cell[][]
    Finite outer-step differences of the declared native gradient estimator
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcCurvatureResult_Get_Shape
    Copy BlackScholesLsmcCurvatureResult Get Shape
&inputs
result is handle StorableBlackScholesLsmcCurvatureResult
    Stored immutable result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_Shape; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
shape is cell[][]
    Logical product dimensions M by N, including zero directions
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcCurvatureResult_Get_Execution
    Copy BlackScholesLsmcCurvatureResult Get Execution
&inputs
result is handle StorableBlackScholesLsmcCurvatureResult
    Stored immutable result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_Execution; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
execution is cell[][]
    Ten fields: method, work counts, numeric payload and separate tape capacity
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcCurvatureResult_Get_Plan
    Copy BlackScholesLsmcCurvatureResult Get Plan
&inputs
result is handle StorableBlackScholesLsmcCurvatureResult
    Stored immutable result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_Plan; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
plan is handle StorableBlackScholesLsmcPlan
    Copied sealed plan, including contract, valuation and simulation
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcCurvatureResult_Get_Simulation
    Copy BlackScholesLsmcCurvatureResult Get Simulation
&inputs
result is handle StorableBlackScholesLsmcCurvatureResult
    Stored immutable result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_Simulation; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
simulation is handle StorableMonteCarloSettings
    Copied original requested simulation settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcCurvatureResult_Get_BasePolicyCount
    Copy BlackScholesLsmcCurvatureResult Get BasePolicyCount
&inputs
result is handle StorableBlackScholesLsmcCurvatureResult
    Stored immutable result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_BasePolicyCount; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
count is number
    Number of stored baseline policies in native exercise-day order
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesLsmcCurvatureResult_Get_BasePolicy
    Copy BlackScholesLsmcCurvatureResult Get BasePolicy
&inputs
result is handle StorableBlackScholesLsmcCurvatureResult
    Stored immutable result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_BasePolicy; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
+argName = "index"; Excel::ValidateCurvatureNumber(*Excel::ScriptScalarInput(xl_index), "BlackScholesLsmcCurvatureResult_Get_BasePolicy; index; ");
+xl_index = Excel::ScriptScalarInput(xl_index);
+const Excel::ScriptSettingsInput_ normalizedindex(xl_index); xl_index = normalizedindex.Get();
index is number
    Exact zero-based policy ordinal below BasePolicyCount
&outputs
policy is handle StorableLsmcExercisePolicy
    Copied baseline policy selected by zero-based policy ordinal
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public LsmcExercisePolicy_Get_Coefficients
    Copy LsmcExercisePolicy Get Coefficients
&inputs
policy is handle StorableLsmcExercisePolicy
    Stored immutable policy
+argName = "policy"; Excel::ValidateRiskRequestText(xl_policy, "LsmcExercisePolicy_Get_Coefficients; policy");
+xl_policy = Excel::ScriptScalarInput(xl_policy);
&outputs
coefficients is cell[][]
    Copied regression coefficients; empty vector spills one empty cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public LsmcExercisePolicy_Get_NormalizationMeans
    Copy LsmcExercisePolicy Get NormalizationMeans
&inputs
policy is handle StorableLsmcExercisePolicy
    Stored immutable policy
+argName = "policy"; Excel::ValidateRiskRequestText(xl_policy, "LsmcExercisePolicy_Get_NormalizationMeans; policy");
+xl_policy = Excel::ScriptScalarInput(xl_policy);
&outputs
normalizationmeans is cell[][]
    Copied basis normalization means
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public LsmcExercisePolicy_Get_NormalizationSigmas
    Copy LsmcExercisePolicy Get NormalizationSigmas
&inputs
policy is handle StorableLsmcExercisePolicy
    Stored immutable policy
+argName = "policy"; Excel::ValidateRiskRequestText(xl_policy, "LsmcExercisePolicy_Get_NormalizationSigmas; policy");
+xl_policy = Excel::ScriptScalarInput(xl_policy);
&outputs
normalizationsigmas is cell[][]
    Copied basis normalization sigmas
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public LsmcExercisePolicy_Get_BasisPowers
    Copy LsmcExercisePolicy Get BasisPowers
&inputs
policy is handle StorableLsmcExercisePolicy
    Stored immutable policy
+argName = "policy"; Excel::ValidateRiskRequestText(xl_policy, "LsmcExercisePolicy_Get_BasisPowers; policy");
+xl_policy = Excel::ScriptScalarInput(xl_policy);
&outputs
basispowers is cell[][]
    Three basis-power columns; empty basis spills one empty cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public LsmcExercisePolicy_Get_Configuration
    Copy LsmcExercisePolicy Get Configuration
&inputs
policy is handle StorableLsmcExercisePolicy
    Stored immutable policy
+argName = "policy"; Excel::ValidateRiskRequestText(xl_policy, "LsmcExercisePolicy_Get_Configuration; policy");
+xl_policy = Excel::ScriptScalarInput(xl_policy);
&outputs
configuration is cell[][]
    Ten native scalar fields and five logical size fields; absent MSE blank
-IF-------------------------------------------------------------------------*/
// clang-format on

namespace Dal {
    void BlackScholesLsmcPlan_New(const String_& name,
                                  const Handle_<ScriptProductData_>& product,
                                  const Handle_<StorableScriptValuationSettings_>& valuation,
                                  const Handle_<StorableMonteCarloSettings_>& simulation,
                                  Handle_<StorableBlackScholesLsmcPlan_>* plan) {
        Excel::CheckCurvatureText(name, "BlackScholesLsmcPlan_New; name");
        auto value = PlanBlackScholesLsmc(product, simulation ? simulation->val_ : DefaultRiskMonteCarloSettings(),
                                          valuation ? valuation->val_ : Script::ScriptValuationSettings_());
        plan->reset(new StorableBlackScholesLsmcPlan_(name, std::move(value)));
    }

    void BlackScholesLsmcCurvatureResult_New(const String_& name,
                                             const Handle_<StorableBlackScholesLsmcPlan_>& plan,
                                             const Matrix_<Cell_>& point,
                                             double paths,
                                             const Handle_<StorableBumpOverAADRequest_>& bumps,
                                             Handle_<StorableBlackScholesLsmcCurvatureResult_>* result) {
        constexpr const char* FUNCTION = "BlackScholesLsmcCurvatureResult_New";
        Excel::CheckCurvatureText(name, String_(FUNCTION) + "; name");
        const auto& fixedPlan = Excel::CheckedCurvatureValue(plan, "BlackScholesLsmcCurvatureResult_New; plan");
        const auto& request = Excel::CheckedCurvatureValue(bumps, "BlackScholesLsmcCurvatureResult_New; bumps");
        auto value = ValueByBlackScholesLsmcWithCurvature(fixedPlan, Excel::ReadMonteCarloPoint(point, FUNCTION),
                                                          Excel::ReadMonteCarloPaths(paths, FUNCTION), request);
        result->reset(new StorableBlackScholesLsmcCurvatureResult_(name, std::move(value)));
    }

    void MonteCarloSettings_Get_Configuration(const Handle_<StorableMonteCarloSettings_>& simulation, Matrix_<Cell_>* configuration) {
        using namespace Excel;
        REQUIRE(simulation, "MonteCarloSettings_Get_Configuration; simulation handle is null");
        const auto& value = simulation->val_;
        *configuration = FieldCells(
            {Field("method", value.rsg_), Field("use_bb", value.useBb_), Field("enable_aad", value.enableAad_), Field("smooth", value.smooth_),
             Field("compiled", OptionalCell(value.compiled_)), Field("lsmc_basis_degree", value.lsmcBasisDegree_),
             Field("lsmc_training_paths", OptionalCell(value.lsmcTrainingPaths_)),
             Field("lsmc_validation_paths", OptionalCell(value.lsmcValidationPaths_)),
             Field("lsmc_rqmc_replicates", OptionalCell(value.lsmcRqmcReplicates_)),
             Field("lsmc_training_seed", OptionalCell(value.lsmcTrainingSeed_)), Field("lsmc_pricing_seed", OptionalCell(value.lsmcPricingSeed_)),
             Field("lsmc_policy_risk_mode", value.lsmcPolicyRiskMode_), Field("lsmc_policy_bump_relative", value.lsmcPolicyBumpRelative_),
             Field("normal_precision", value.normalPrecision_), Field("lsmc_checkpoint_block_events", OptionalCell(value.lsmcCheckpointBlockEvents_)),
             Field("lsmc_training_memory_mb", OptionalCell(value.lsmcTrainingMemoryMb_))});
    }

    void BlackScholesLsmcPlan_Get_Product(const Handle_<StorableBlackScholesLsmcPlan_>& plan, Handle_<ScriptProductData_>* product) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "BlackScholesLsmcPlan_Get_Product; plan").Contract();
        product->reset(new ScriptProductData_(value.Name(), value.Dates(), value.EventTexts(), value.Settings()));
    }

    void BlackScholesLsmcPlan_Get_Valuation(const Handle_<StorableBlackScholesLsmcPlan_>& plan,
                                            Handle_<StorableScriptValuationSettings_>* valuation) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "BlackScholesLsmcPlan_Get_Valuation; plan");
        valuation->reset(new StorableScriptValuationSettings_(plan->Name(), value.Valuation()));
    }

    void BlackScholesLsmcPlan_Get_Simulation(const Handle_<StorableBlackScholesLsmcPlan_>& plan, Handle_<StorableMonteCarloSettings_>* simulation) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "BlackScholesLsmcPlan_Get_Simulation; plan");
        simulation->reset(new StorableMonteCarloSettings_(plan->Name(), value.Simulation()));
    }

    void BlackScholesLsmcPlan_Get_Observations(const Handle_<StorableBlackScholesLsmcPlan_>& plan, Matrix_<Cell_>* observations) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "BlackScholesLsmcPlan_Get_Observations; plan");
        *observations = Excel::MonteCarloObservationCells(value.Observations(), "BlackScholesLsmcPlan_Get_Observations");
    }

    void BlackScholesLsmcPlan_Get_EventDates(const Handle_<StorableBlackScholesLsmcPlan_>& plan, Matrix_<Cell_>* dates) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "BlackScholesLsmcPlan_Get_EventDates; plan").Prepared().EventDates();
        REQUIRE(value.size() <= static_cast<size_t>(Excel::CURVATURE_MAX_ROWS), "BlackScholesLsmcPlan_Get_EventDates; rows exceed worksheet limit");
        Matrix_<Cell_> cells(value.empty() ? 1 : static_cast<int>(value.size()), 1);
        for (int row = 0; row < static_cast<int>(value.size()); ++row)
            cells(row, 0) = value[row];
        *dates = std::move(cells);
    }

    void BlackScholesLsmcCurvatureResult_Get_Execution(const Handle_<StorableBlackScholesLsmcCurvatureResult_>& result, Matrix_<Cell_>* execution) {
        using namespace Excel;
        const auto& value = CheckedCurvatureValue(result, "BlackScholesLsmcCurvatureResult_Get_Execution; result").Curvature().Execution();
        *execution = FieldCells(
            {Field("method", value.method_), Field("gradient_evaluations", double(value.gradientEvaluations_)),
             Field("paths_per_replicate", double(value.pathsPerReplicate_)), Field("training_paths", double(value.trainingPaths_)),
             Field("validation_paths", double(value.validationPaths_)), Field("pricing_replicates", double(value.pricingReplicates_)),
             Field("numeric_payload_bytes", double(value.numericPayloadBytes_)), Field("max_batch_tape_bytes", double(value.maxBatchTapeBytes_)),
             Field("max_batch_cleanup_reserve_bytes", double(value.maxBatchCleanupReserveBytes_)),
             Field("recording_capacity_budget_bytes", CurvatureBudgetCell(value.recordingCapacityBudgetBytes_))});
    }

    void BlackScholesLsmcCurvatureResult_Get_Plan(const Handle_<StorableBlackScholesLsmcCurvatureResult_>& result,
                                                  Handle_<StorableBlackScholesLsmcPlan_>* plan) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesLsmcCurvatureResult_Get_Plan; result");
        plan->reset(new StorableBlackScholesLsmcPlan_(result->Name(), value.Plan()));
    }

    void BlackScholesLsmcCurvatureResult_Get_Simulation(const Handle_<StorableBlackScholesLsmcCurvatureResult_>& result,
                                                        Handle_<StorableMonteCarloSettings_>* simulation) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesLsmcCurvatureResult_Get_Simulation; result");
        simulation->reset(new StorableMonteCarloSettings_(result->Name(), value.Plan().Simulation()));
    }

    void BlackScholesLsmcCurvatureResult_Get_BasePolicy(const Handle_<StorableBlackScholesLsmcCurvatureResult_>& result,
                                                        double index,
                                                        Handle_<StorableLsmcExercisePolicy_>* policy) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesLsmcCurvatureResult_Get_BasePolicy; result").Curvature().BasePolicy();
        const auto ordinal = Excel::PayloadBudget(Cell_(index), "BlackScholesLsmcCurvatureResult_Get_BasePolicy; ", "index");
        REQUIRE(ordinal < value.size(),
                "BlackScholesLsmcCurvatureResult_Get_BasePolicy; index must be a zero-based policy ordinal below policy count");
        policy->reset(new StorableLsmcExercisePolicy_(result->Name(), value[ordinal]));
    }

    void LsmcExercisePolicy_Get_BasisPowers(const Handle_<StorableLsmcExercisePolicy_>& policy, Matrix_<Cell_>* powers) {
        const auto& value = Excel::CheckedCurvatureValue(policy, "LsmcExercisePolicy_Get_BasisPowers; policy").powers_;
        REQUIRE(value.size() <= static_cast<size_t>(Excel::CURVATURE_MAX_ROWS), "LsmcExercisePolicy_Get_BasisPowers; rows exceed worksheet limit");
        if (value.empty()) {
            *powers = Matrix_<Cell_>(1, 1);
            return;
        }
        Matrix_<Cell_> cells(static_cast<int>(value.size()), 3);
        for (int row = 0; row < cells.Rows(); ++row)
            for (int column = 0; column < 3; ++column)
                cells(row, column) = double(value[row][column]);
        *powers = std::move(cells);
    }

    void LsmcExercisePolicy_Get_Configuration(const Handle_<StorableLsmcExercisePolicy_>& policy, Matrix_<Cell_>* configuration) {
        using namespace Excel;
        const auto& value = CheckedCurvatureValue(policy, "LsmcExercisePolicy_Get_Configuration; policy");
        *configuration = FieldCells(
            {Field("basis_degree", value.basisDegree_), Field("degenerate", value.degenerate_), Field("degenerate_reason", value.degenerateReason_),
             Field("mean", value.mean_), Field("sigma", value.sigma_), Field("condition_path_count", double(value.numCondTrue_)),
             Field("effective_rank", double(value.effectiveRank_)), Field("solver", value.solver_), Field("fallback_reason", value.fallbackReason_),
             Field("validation_mse", OptionalCell(value.validationMse_)), Field("coefficients_length", double(value.coefficients_.size())),
             Field("normalization_means_length", double(value.means_.size())), Field("normalization_sigmas_length", double(value.sigmas_.size())),
             Field("basis_power_rows", double(value.powers_.size())), Field("basis_power_columns", 3.0)});
    }

    void BlackScholesLsmcPlan_Get_ParameterLabels(const Handle_<StorableBlackScholesLsmcPlan_>& plan, Vector_<String_>* output) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "BlackScholesLsmcPlan_Get_ParameterLabels; plan");
        *output = value.ParameterLabels();
    }

    void BlackScholesLsmcPlan_Get_ScriptConstants(const Handle_<StorableBlackScholesLsmcPlan_>& plan, Matrix_<Cell_>* output) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "BlackScholesLsmcPlan_Get_ScriptConstants; plan");
        *output = Excel::CurvatureVectorCells(value.ScriptConstants());
    }

    void BlackScholesLsmcPlan_Get_TimeLine(const Handle_<StorableBlackScholesLsmcPlan_>& plan, Matrix_<Cell_>* output) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "BlackScholesLsmcPlan_Get_TimeLine; plan");
        *output = Excel::CurvatureVectorCells(value.Prepared().TimeLine());
    }

    void BlackScholesLsmcCurvatureResult_Get_Value(const Handle_<StorableBlackScholesLsmcCurvatureResult_>& result, double* output) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesLsmcCurvatureResult_Get_Value; result");
        *output = value.Curvature().Value();
    }

    void BlackScholesLsmcCurvatureResult_Get_Gradient(const Handle_<StorableBlackScholesLsmcCurvatureResult_>& result, Matrix_<Cell_>* output) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesLsmcCurvatureResult_Get_Gradient; result");
        *output = Excel::CurvatureVectorCells(value.Curvature().Gradient());
    }

    void BlackScholesLsmcCurvatureResult_Get_ParameterLabels(const Handle_<StorableBlackScholesLsmcCurvatureResult_>& result,
                                                             Vector_<String_>* output) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesLsmcCurvatureResult_Get_ParameterLabels; result");
        *output = value.Plan().ParameterLabels();
    }

    void BlackScholesLsmcCurvatureResult_Get_Point(const Handle_<StorableBlackScholesLsmcCurvatureResult_>& result, Matrix_<Cell_>* output) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesLsmcCurvatureResult_Get_Point; result");
        *output = Excel::CurvatureVectorCells(value.Curvature().Point());
    }

    void BlackScholesLsmcCurvatureResult_Get_Directions(const Handle_<StorableBlackScholesLsmcCurvatureResult_>& result, Matrix_<Cell_>* output) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesLsmcCurvatureResult_Get_Directions; result");
        *output = Excel::CurvatureMatrixCells(value.Curvature().Directions());
    }

    void BlackScholesLsmcCurvatureResult_Get_Steps(const Handle_<StorableBlackScholesLsmcCurvatureResult_>& result, Matrix_<Cell_>* output) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesLsmcCurvatureResult_Get_Steps; result");
        *output = Excel::CurvatureVectorCells(value.Curvature().Steps());
    }

    void BlackScholesLsmcCurvatureResult_Get_HessianProducts(const Handle_<StorableBlackScholesLsmcCurvatureResult_>& result,
                                                             Matrix_<Cell_>* output) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesLsmcCurvatureResult_Get_HessianProducts; result");
        *output = Excel::CurvatureMatrixCells(value.Curvature().HessianProducts());
    }

    void BlackScholesLsmcCurvatureResult_Get_Shape(const Handle_<StorableBlackScholesLsmcCurvatureResult_>& result, Matrix_<Cell_>* output) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesLsmcCurvatureResult_Get_Shape; result");
        *output = Excel::RiskShapeCells(value.Curvature().HessianProducts());
    }

    void BlackScholesLsmcCurvatureResult_Get_BasePolicyCount(const Handle_<StorableBlackScholesLsmcCurvatureResult_>& result, double* output) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesLsmcCurvatureResult_Get_BasePolicyCount; result");
        *output = double(value.Curvature().BasePolicy().size());
    }

    void LsmcExercisePolicy_Get_Coefficients(const Handle_<StorableLsmcExercisePolicy_>& policy, Matrix_<Cell_>* output) {
        const auto& value = Excel::CheckedCurvatureValue(policy, "LsmcExercisePolicy_Get_Coefficients; policy");
        *output = Excel::CurvatureVectorCells(value.coefficients_);
    }

    void LsmcExercisePolicy_Get_NormalizationMeans(const Handle_<StorableLsmcExercisePolicy_>& policy, Matrix_<Cell_>* output) {
        const auto& value = Excel::CheckedCurvatureValue(policy, "LsmcExercisePolicy_Get_NormalizationMeans; policy");
        *output = Excel::CurvatureVectorCells(value.means_);
    }

    void LsmcExercisePolicy_Get_NormalizationSigmas(const Handle_<StorableLsmcExercisePolicy_>& policy, Matrix_<Cell_>* output) {
        const auto& value = Excel::CheckedCurvatureValue(policy, "LsmcExercisePolicy_Get_NormalizationSigmas; policy");
        *output = Excel::CurvatureVectorCells(value.sigmas_);
    }
    // clang-format off
#ifdef _WIN32
#include <dal-excel/auto/MG_BlackScholesLsmcPlan_New_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcCurvatureResult_New_public.inc>
#include <dal-excel/auto/MG_MonteCarloSettings_Get_Configuration_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcPlan_Get_Product_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcPlan_Get_Valuation_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcPlan_Get_Simulation_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcPlan_Get_ParameterLabels_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcPlan_Get_ScriptConstants_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcPlan_Get_Observations_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcPlan_Get_EventDates_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcPlan_Get_TimeLine_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcCurvatureResult_Get_Value_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcCurvatureResult_Get_Gradient_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcCurvatureResult_Get_ParameterLabels_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcCurvatureResult_Get_Point_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcCurvatureResult_Get_Directions_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcCurvatureResult_Get_Steps_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcCurvatureResult_Get_HessianProducts_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcCurvatureResult_Get_Shape_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcCurvatureResult_Get_Execution_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcCurvatureResult_Get_Plan_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcCurvatureResult_Get_Simulation_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcCurvatureResult_Get_BasePolicyCount_public.inc>
#include <dal-excel/auto/MG_BlackScholesLsmcCurvatureResult_Get_BasePolicy_public.inc>
#include <dal-excel/auto/MG_LsmcExercisePolicy_Get_Coefficients_public.inc>
#include <dal-excel/auto/MG_LsmcExercisePolicy_Get_NormalizationMeans_public.inc>
#include <dal-excel/auto/MG_LsmcExercisePolicy_Get_NormalizationSigmas_public.inc>
#include <dal-excel/auto/MG_LsmcExercisePolicy_Get_BasisPowers_public.inc>
#include <dal-excel/auto/MG_LsmcExercisePolicy_Get_Configuration_public.inc>
#endif
    // clang-format on
} // namespace Dal
