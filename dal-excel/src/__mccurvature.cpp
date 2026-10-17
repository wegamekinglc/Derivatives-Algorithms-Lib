#include "__mccurvature.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <system_error>

#include "__curvatureinput.hpp"
#include "__curvaturerows.hpp"
#include "__mcriskrows.hpp"

// clang-format off
/*IF--------------------------------------------------------------------------
public SegmentedMonteCarloSettings_New
    Copy checked segmented MC settings and exact uint64 scramble keys
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "SegmentedMonteCarloSettings_New; name");
+const Excel::ScriptSettingsInput_ normalized(xl_settings); xl_settings = normalized.Get();
+argName = "settings"; Excel::ValidateRiskRequestSettings(xl_settings, "SegmentedMonteCarloSettings_New");
&optional
settings is cell[][]+
    Eight native key/value fields; blank caps unset, zero caps actual
&outputs
configuration is handle StorableSegmentedMonteCarloSettings
    Immutable segmented settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public SegmentedMonteCarloSettings_Get_Configuration
    Copy all eight requested segmented MC settings
&inputs
settings is handle StorableSegmentedMonteCarloSettings
    Copied segmented settings; blank uses native defaults
+argName = "settings"; Excel::ValidateRiskRequestText(xl_settings, "SegmentedMonteCarloSettings_Get_Configuration; settings");
+xl_settings = Excel::ScriptScalarInput(xl_settings);
&outputs
configuration is cell[][]
    Eight key/value rows; present scramble key is exact decimal text
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloPlan_New
    Seal a contract and observations for compiled segmented native AAD
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "BlackScholesMonteCarloPlan_New; name");
+argName = "product"; Excel::ValidateRiskRequestText(xl_product, "BlackScholesMonteCarloPlan_New; product");
+xl_product = Excel::ScriptScalarInput(xl_product);
+argName = "valuation"; Excel::ValidateRiskRequestText(xl_valuation, "BlackScholesMonteCarloPlan_New; valuation");
+xl_valuation = Excel::ScriptScalarInput(xl_valuation);
+argName = "smoothing"; Excel::ValidateCurvatureNumbers(xl_smoothing, "BlackScholesMonteCarloPlan_New", "smoothing");
+xl_smoothing = Excel::ScriptScalarInput(xl_smoothing);
+const Excel::ScriptSettingsInput_ normalizedSmoothing(xl_smoothing); xl_smoothing = normalizedSmoothing.Get();
product is handle ScriptProductData
    Script contract; EXERCISE uses the separate LSMC interface
&optional
valuation is handle StorableScriptValuationSettings
    Blank captures native valuation defaults
smoothing is number (0.01)
    Finite smoothing width; native default 0.01
&outputs
plan is handle StorableBlackScholesMonteCarloPlan
    Immutable copied contract, full parameter order and observations
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloPlan_Get_Product
    Copy sealed Monte Carlo plan product
&inputs
plan is handle StorableBlackScholesMonteCarloPlan
    Sealed native Black-Scholes script plan
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesMonteCarloPlan_Get_Product; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
&outputs
product is handle ScriptProductData
    Copied contract; use PRODUCT.DESCRIBE for dates, events and product settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloPlan_Get_Valuation
    Copy sealed Monte Carlo plan valuation
&inputs
plan is handle StorableBlackScholesMonteCarloPlan
    Sealed native Black-Scholes script plan
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesMonteCarloPlan_Get_Valuation; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
&outputs
valuation is handle StorableScriptValuationSettings
    Copied valuation settings and captured history
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloPlan_Get_Smoothing
    Copy sealed Monte Carlo plan smoothing
&inputs
plan is handle StorableBlackScholesMonteCarloPlan
    Sealed native Black-Scholes script plan
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesMonteCarloPlan_Get_Smoothing; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
&outputs
smoothing is number
    Native preparation smoothing width
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloPlan_Get_ParameterLabels
    Copy sealed Monte Carlo plan parameterlabels
&inputs
plan is handle StorableBlackScholesMonteCarloPlan
    Sealed native Black-Scholes script plan
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesMonteCarloPlan_Get_ParameterLabels; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
&outputs
parameterlabels is string[]
    Complete point order: spot, vol, rate, div, then script constants
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloPlan_Get_ScriptConstants
    Copy sealed Monte Carlo plan scriptconstants
&inputs
plan is handle StorableBlackScholesMonteCarloPlan
    Sealed native Black-Scholes script plan
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesMonteCarloPlan_Get_ScriptConstants; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
&outputs
scriptconstants is cell[][]
    Copied original script constants; no constants spill one empty cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloPlan_Get_Observations
    Copy sealed Monte Carlo plan observations
&inputs
plan is handle StorableBlackScholesMonteCarloPlan
    Sealed native Black-Scholes script plan
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesMonteCarloPlan_Get_Observations; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
&outputs
observations is cell[][]
    Header then index, fixing time, historical flag and optional captured value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloResult_New
    Evaluate an owning segmented native MC mean and full raw gradient
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "BlackScholesMonteCarloResult_New; name");
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesMonteCarloResult_New; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
+argName = "settings"; Excel::ValidateRiskRequestText(xl_settings, "BlackScholesMonteCarloResult_New; settings");
+xl_settings = Excel::ScriptScalarInput(xl_settings);
+argName = "point"; Excel::ValidateCurvatureNumbers(xl_point, "BlackScholesMonteCarloResult_New", "point");
+const Excel::ScriptSettingsInput_ normalizedPoint(xl_point); xl_point = normalizedPoint.Get();
+argName = "paths"; Excel::ValidateCurvatureNumber(*Excel::ScriptScalarInput(xl_paths), "BlackScholesMonteCarloResult_New; paths; ");
+xl_paths = Excel::ScriptScalarInput(xl_paths);
+const Excel::ScriptSettingsInput_ normalizedPaths(xl_paths); xl_paths = normalizedPaths.Get();
plan is handle StorableBlackScholesMonteCarloPlan
    Sealed native Black-Scholes script plan
point is cell[][]
    Complete finite numeric row or column; blank gaps reject
paths is number
    Positive exact size_t integer at most 2^53-1
&optional
settings is handle StorableSegmentedMonteCarloSettings
    Copied segmented settings; blank uses native defaults
&outputs
result is handle StorableBlackScholesMonteCarloResult
    Completed owning mean, gradient, plan, point and requested settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloResult_Get_Value
    Copy completed segmented MC value
&inputs
result is handle StorableBlackScholesMonteCarloResult
    Completed owning mean and gradient
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloResult_Get_Value; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
value is number
    Native sample mean PV
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloResult_Get_Gradient
    Copy completed segmented MC gradient
&inputs
result is handle StorableBlackScholesMonteCarloResult
    Completed owning mean and gradient
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloResult_Get_Gradient; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
gradient is cell[][]
    Full raw mean gradient in parameter-label order
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloResult_Get_ParameterLabels
    Copy completed segmented MC parameterlabels
&inputs
result is handle StorableBlackScholesMonteCarloResult
    Completed owning mean and gradient
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloResult_Get_ParameterLabels; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
parameterlabels is string[]
    Complete parameter order including script constants
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloResult_Get_Execution
    Copy completed segmented MC execution
&inputs
result is handle StorableBlackScholesMonteCarloResult
    Completed owning mean and gradient
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloResult_Get_Execution; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
execution is cell[][]
    Thirteen native fields; tape, checkpoint and cleanup maxima are per path
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloResult_Get_Plan
    Copy completed segmented MC plan
&inputs
result is handle StorableBlackScholesMonteCarloResult
    Completed owning mean and gradient
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloResult_Get_Plan; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
plan is handle StorableBlackScholesMonteCarloPlan
    Copied sealed plan; no preparation or observation reads
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloResult_Get_Point
    Copy completed segmented MC point
&inputs
result is handle StorableBlackScholesMonteCarloResult
    Completed owning mean and gradient
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloResult_Get_Point; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
point is cell[][]
    Copied complete raw parameter column
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloResult_Get_Settings
    Copy completed segmented MC settings
&inputs
result is handle StorableBlackScholesMonteCarloResult
    Completed owning mean and gradient
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloResult_Get_Settings; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
settings is handle StorableSegmentedMonteCarloSettings
    Copied requested path settings and optional caps
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloCurvatureResult_New
    Evaluate finite-step Hessian products using common absolute native MC paths
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "BlackScholesMonteCarloCurvatureResult_New; name");
+argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesMonteCarloCurvatureResult_New; plan");
+xl_plan = Excel::ScriptScalarInput(xl_plan);
+argName = "bumps"; Excel::ValidateRiskRequestText(xl_bumps, "BlackScholesMonteCarloCurvatureResult_New; bumps");
+xl_bumps = Excel::ScriptScalarInput(xl_bumps);
+argName = "settings"; Excel::ValidateRiskRequestText(xl_settings, "BlackScholesMonteCarloCurvatureResult_New; settings");
+xl_settings = Excel::ScriptScalarInput(xl_settings);
+argName = "point"; Excel::ValidateCurvatureNumbers(xl_point, "BlackScholesMonteCarloCurvatureResult_New", "point");
+const Excel::ScriptSettingsInput_ normalizedPoint(xl_point); xl_point = normalizedPoint.Get();
+argName = "paths"; Excel::ValidateCurvatureNumber(*Excel::ScriptScalarInput(xl_paths), "BlackScholesMonteCarloCurvatureResult_New; paths; ");
+xl_paths = Excel::ScriptScalarInput(xl_paths);
+const Excel::ScriptSettingsInput_ normalizedPaths(xl_paths); xl_paths = normalizedPaths.Get();
plan is handle StorableBlackScholesMonteCarloPlan
    Sealed native Black-Scholes script plan
point is cell[][]
    Complete finite numeric row or column; no blank gaps
paths is number
    Positive exact size_t integer at most 2^53-1
bumps is handle StorableBumpOverAADRequest
    Full directions and positive actual steps; numeric and recording caps separate
&optional
settings is handle StorableSegmentedMonteCarloSettings
    Copied segmented settings; blank uses native defaults
&outputs
result is handle StorableBlackScholesMonteCarloCurvatureResult
    Owning BumpOverSegmentedNativeAAD result; 1+2M gradient evaluations
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloCurvatureResult_Get_Point
    Copy stored native MC curvature point
&inputs
result is handle StorableBlackScholesMonteCarloCurvatureResult
    Completed owning finite-step result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloCurvatureResult_Get_Point; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
point is cell[][]
    Complete raw parameter column
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloCurvatureResult_Get_Directions
    Copy stored native MC curvature directions
&inputs
result is handle StorableBlackScholesMonteCarloCurvatureResult
    Completed owning finite-step result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloCurvatureResult_Get_Directions; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
directions is cell[][]
    Copied signed direction rows; zero rows spill empty text
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloCurvatureResult_Get_Steps
    Copy stored native MC curvature steps
&inputs
result is handle StorableBlackScholesMonteCarloCurvatureResult
    Completed owning finite-step result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloCurvatureResult_Get_Steps; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
steps is cell[][]
    Copied positive finite steps; zero directions spill empty text
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloCurvatureResult_Get_HessianProducts
    Copy stored native MC curvature hessianproducts
&inputs
result is handle StorableBlackScholesMonteCarloCurvatureResult
    Completed owning finite-step result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloCurvatureResult_Get_HessianProducts; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
hessianproducts is cell[][]
    Actual-step raw gradient secants; empty result spills one empty cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloCurvatureResult_Get_Shape
    Copy stored native MC curvature shape
&inputs
result is handle StorableBlackScholesMonteCarloCurvatureResult
    Completed owning finite-step result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloCurvatureResult_Get_Shape; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
shape is cell[][]
    One row: direction count, then complete parameter count; preserves 0-by-N
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloCurvatureResult_Get_Execution
    Copy stored native MC curvature execution
&inputs
result is handle StorableBlackScholesMonteCarloCurvatureResult
    Completed owning finite-step result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloCurvatureResult_Get_Execution; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
execution is cell[][]
    Seven native fields; effective recording cap and per-path resource maxima
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloCurvatureResult_Get_Base
    Copy stored native MC curvature base
&inputs
result is handle StorableBlackScholesMonteCarloCurvatureResult
    Completed owning finite-step result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloCurvatureResult_Get_Base; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
base is handle StorableBlackScholesMonteCarloResult
    Copied completed base mean, gradient, plan, point and settings; no reevaluation
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloCurvatureResult_Get_Plan
    Copy stored native MC curvature plan
&inputs
result is handle StorableBlackScholesMonteCarloCurvatureResult
    Completed owning finite-step result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloCurvatureResult_Get_Plan; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
plan is handle StorableBlackScholesMonteCarloPlan
    Copied sealed plan; no preparation or history reads
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BlackScholesMonteCarloCurvatureResult_Get_Settings
    Copy stored native MC curvature settings
&inputs
result is handle StorableBlackScholesMonteCarloCurvatureResult
    Completed owning finite-step result
+argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloCurvatureResult_Get_Settings; result");
+xl_result = Excel::ScriptScalarInput(xl_result);
&outputs
settings is handle StorableSegmentedMonteCarloSettings
    Copied requested path cap; execution reports the effective bump/path minimum
-IF-------------------------------------------------------------------------*/
// clang-format on

namespace Dal {
    namespace {
        Cell_ ScrambleCell(const std::optional<std::uint64_t>& key) { return key ? Cell_(String_(std::to_string(*key))) : Cell_(); }

        bool CanonicalScramble(const String_& text) {
            return !text.empty() && (text.size() == 1 || text.front() != '0') &&
                   std::all_of(text.begin(), text.end(), [](char value) { return value >= '0' && value <= '9'; });
        }

        std::uint64_t NumericScramble(double value, const String_& context) {
            REQUIRE(std::isfinite(value) && value >= 0 && std::trunc(value) == value && value <= 9007199254740991.0,
                    context + "scramble_key must be an exact nonnegative integer at most 2^53-1; use decimal text for larger uint64 keys");
            return static_cast<std::uint64_t>(value);
        }

        std::optional<std::uint64_t> ReadScramble(const Cell_& cell, const String_& context) {
            if (Cell::IsEmpty(cell))
                return std::nullopt;
            if (const auto* number = std::get_if<double>(&cell.val_))
                return NumericScramble(*number, context);
            const auto text = Excel::TextValue(cell, context);
            Excel::CheckCurvatureText(text, context);
            REQUIRE(CanonicalScramble(text), context + "scramble_key requires canonical unsigned decimal text or an exact numeric integer");
            std::uint64_t key = 0;
            const auto result = std::from_chars(text.data(), text.data() + text.size(), key);
            REQUIRE(result.ec == std::errc() && result.ptr == text.data() + text.size(), context + "scramble_key exceeds uint64 range");
            return key;
        }

        bool ReadScalarSetting(const String_& key, const Cell_& cell, const String_& context, Script::SegmentedMonteCarloSettings_* settings) {
            const std::array<std::pair<const char*, String_*>, 2> strings = {
                {{"rsg", &settings->rsg_}, {"normal_precision", &settings->normalPrecision_}}};
            const auto text = std::find_if(strings.begin(), strings.end(), [&](const auto& field) { return key == field.first; });
            if (text != strings.end()) {
                *text->second = Excel::TextValue(cell, context);
                Excel::CheckCurvatureText(*text->second, context);
                return true;
            }
            if (key != "use_bb")
                return false;
            const auto* value = std::get_if<bool>(&cell.val_);
            REQUIRE(value, context + "use_bb requires a Boolean cell");
            settings->useBb_ = *value;
            return true;
        }

        void ReadSetting(const String_& key, const Cell_& cell, const String_& context, Script::SegmentedMonteCarloSettings_* settings) {
            if (ReadScalarSetting(key, cell, context, settings))
                return;
            if (key == "scramble_key") {
                settings->scrambleKey_ = ReadScramble(cell, context);
                return;
            }
            const std::array<std::pair<const char*, size_t*>, 2> counts = {
                {{"first_path", &settings->firstPath_}, {"segment_steps", &settings->path_.segmentSteps_}}};
            const auto count = std::find_if(counts.begin(), counts.end(), [&](const auto& field) { return key == field.first; });
            if (count != counts.end()) {
                *count->second = Excel::PayloadBudget(cell, context, count->first);
                return;
            }
            const std::array<std::pair<const char*, std::optional<size_t>*>, 2> budgets = {
                {{"checkpoint_capacity_budget_bytes", &settings->path_.checkpointCapacityBudgetBytes_},
                 {"recording_capacity_budget_bytes", &settings->path_.recordingCapacityBudgetBytes_}}};
            const auto budget = std::find_if(budgets.begin(), budgets.end(), [&](const auto& field) { return key == field.first; });
            REQUIRE(budget != budgets.end(), context + "unknown segmented Monte Carlo setting");
            *budget->second = Cell::IsEmpty(cell) ? std::nullopt : std::optional<size_t>(Excel::PayloadBudget(cell, context, budget->first));
        }

        Matrix_<Cell_> MeanExecution(const Script::SegmentedMonteCarloExecution_& execution) {
            using namespace Excel;
            return FieldCells({Field("path_count", double(execution.pathCount_)), Field("first_path", double(execution.firstPath_)),
                               Field("batch_size", double(execution.batchSize_)), Field("batches", double(execution.batches_)),
                               Field("lanes", double(execution.lanes_)), Field("segment_steps", double(execution.segmentSteps_)),
                               Field("rsg", execution.rsg_), Field("use_bb", execution.useBb_),
                               Field("scramble_key", ScrambleCell(execution.scrambleKey_)),
                               Field("max_path_tape_bytes", double(execution.maxPathTapeBytes_)),
                               Field("max_path_checkpoint_bytes", double(execution.maxPathCheckpointBytes_)),
                               Field("max_path_cleanup_reserve_bytes", double(execution.maxPathCleanupReserveBytes_)),
                               Field("normal_precision", execution.normalPrecision_)});
        }
    } // namespace

    void SegmentedMonteCarloSettings_New(const String_& name, const Matrix_<Cell_>& cells, Handle_<StorableSegmentedMonteCarloSettings_>* settings) {
        constexpr const char* FUNCTION = "SegmentedMonteCarloSettings_New";
        Excel::CheckCurvatureText(name, "SegmentedMonteCarloSettings_New; name");
        Script::SegmentedMonteCarloSettings_ value;
        Excel::ReadRows(
            cells, FUNCTION, "settings",
            [&](const String_& key, const Cell_& cell, const String_& keyContext, const String_& context) {
                Excel::CheckCurvatureText(key, keyContext);
                ReadSetting(key, cell, context, &value);
            },
            true);
        REQUIRE(value.path_.segmentSteps_ > 0, "SegmentedMonteCarloSettings_New; segment_steps must be positive");
        settings->reset(new StorableSegmentedMonteCarloSettings_(name, std::move(value)));
    }

    void SegmentedMonteCarloSettings_Get_Configuration(const Handle_<StorableSegmentedMonteCarloSettings_>& settings, Matrix_<Cell_>* configuration) {
        using namespace Excel;
        const auto& value = CheckedCurvatureValue(settings, "SegmentedMonteCarloSettings_Get_Configuration; settings");
        *configuration = FieldCells({Field("rsg", value.rsg_), Field("use_bb", value.useBb_), Field("first_path", double(value.firstPath_)),
                                     Field("scramble_key", ScrambleCell(value.scrambleKey_)), Field("normal_precision", value.normalPrecision_),
                                     Field("segment_steps", double(value.path_.segmentSteps_)),
                                     Field("checkpoint_capacity_budget_bytes", CurvatureBudgetCell(value.path_.checkpointCapacityBudgetBytes_)),
                                     Field("recording_capacity_budget_bytes", CurvatureBudgetCell(value.path_.recordingCapacityBudgetBytes_))});
    }

    void BlackScholesMonteCarloPlan_New(const String_& name,
                                        const Handle_<ScriptProductData_>& product,
                                        const Handle_<StorableScriptValuationSettings_>& valuation,
                                        double smoothing,
                                        Handle_<StorableBlackScholesMonteCarloPlan_>* plan) {
        Excel::CheckCurvatureText(name, "BlackScholesMonteCarloPlan_New; name");
        REQUIRE(std::isfinite(smoothing), "BlackScholesMonteCarloPlan_New; smoothing must be finite");
        auto value = PlanBlackScholesMonteCarlo(product, valuation ? valuation->val_ : Script::ScriptValuationSettings_(), smoothing);
        plan->reset(new StorableBlackScholesMonteCarloPlan_(name, std::move(value)));
    }

    void BlackScholesMonteCarloPlan_Get_Product(const Handle_<StorableBlackScholesMonteCarloPlan_>& plan, Handle_<ScriptProductData_>* product) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "BlackScholesMonteCarloPlan_Get_Product; plan").Contract();
        product->reset(new ScriptProductData_(value.Name(), value.Dates(), value.EventTexts(), value.Settings()));
    }

    void BlackScholesMonteCarloPlan_Get_Valuation(const Handle_<StorableBlackScholesMonteCarloPlan_>& plan,
                                                  Handle_<StorableScriptValuationSettings_>* valuation) {
        const auto& value = Excel::CheckedCurvatureValue(plan, "BlackScholesMonteCarloPlan_Get_Valuation; plan");
        valuation->reset(new StorableScriptValuationSettings_(plan->Name(), value.Valuation()));
    }

    void BlackScholesMonteCarloPlan_Get_Smoothing(const Handle_<StorableBlackScholesMonteCarloPlan_>& plan, double* smoothing) {
        *smoothing = Excel::CheckedCurvatureValue(plan, "BlackScholesMonteCarloPlan_Get_Smoothing; plan").Prepared().Simulation().smooth_;
    }

    void BlackScholesMonteCarloPlan_Get_ParameterLabels(const Handle_<StorableBlackScholesMonteCarloPlan_>& plan, Vector_<String_>* labels) {
        *labels = Excel::CheckedCurvatureValue(plan, "BlackScholesMonteCarloPlan_Get_ParameterLabels; plan").ParameterLabels();
    }

    void BlackScholesMonteCarloPlan_Get_ScriptConstants(const Handle_<StorableBlackScholesMonteCarloPlan_>& plan, Matrix_<Cell_>* constants) {
        *constants =
            Excel::CurvatureVectorCells(Excel::CheckedCurvatureValue(plan, "BlackScholesMonteCarloPlan_Get_ScriptConstants; plan").ScriptConstants());
    }

    void BlackScholesMonteCarloPlan_Get_Observations(const Handle_<StorableBlackScholesMonteCarloPlan_>& plan, Matrix_<Cell_>* observations) {
        const auto& values = Excel::CheckedCurvatureValue(plan, "BlackScholesMonteCarloPlan_Get_Observations; plan").Observations();
        *observations = Excel::MonteCarloObservationCells(values, "BlackScholesMonteCarloPlan_Get_Observations");
    }

    void BlackScholesMonteCarloResult_New(const String_& name,
                                          const Handle_<StorableBlackScholesMonteCarloPlan_>& plan,
                                          const Matrix_<Cell_>& point,
                                          double paths,
                                          const Handle_<StorableSegmentedMonteCarloSettings_>& settings,
                                          Handle_<StorableBlackScholesMonteCarloResult_>* result) {
        constexpr const char* FUNCTION = "BlackScholesMonteCarloResult_New";
        Excel::CheckCurvatureText(name, String_(FUNCTION) + "; name");
        const auto& fixedPlan = Excel::CheckedCurvatureValue(plan, "BlackScholesMonteCarloResult_New; plan");
        auto value = ValueByBlackScholesSegmentedMonteCarlo(fixedPlan, Excel::ReadMonteCarloPoint(point, FUNCTION),
                                                            Excel::ReadMonteCarloPaths(paths, FUNCTION),
                                                            settings ? settings->val_ : Script::SegmentedMonteCarloSettings_());
        result->reset(new StorableBlackScholesMonteCarloResult_(
            name, Excel::MonteCarloMeanSnapshot_{value.Plan(), value.Point(), value.Settings(), value.Mean()}));
    }

    void BlackScholesMonteCarloCurvatureResult_New(const String_& name,
                                                   const Handle_<StorableBlackScholesMonteCarloPlan_>& plan,
                                                   const Matrix_<Cell_>& point,
                                                   double paths,
                                                   const Handle_<StorableBumpOverAADRequest_>& bumps,
                                                   const Handle_<StorableSegmentedMonteCarloSettings_>& settings,
                                                   Handle_<StorableBlackScholesMonteCarloCurvatureResult_>* result) {
        constexpr const char* FUNCTION = "BlackScholesMonteCarloCurvatureResult_New";
        Excel::CheckCurvatureText(name, String_(FUNCTION) + "; name");
        const auto& fixedPlan = Excel::CheckedCurvatureValue(plan, "BlackScholesMonteCarloCurvatureResult_New; plan");
        const auto& request = Excel::CheckedCurvatureValue(bumps, "BlackScholesMonteCarloCurvatureResult_New; bumps");
        auto value = ValueByBlackScholesMonteCarloWithCurvature(fixedPlan, Excel::ReadMonteCarloPoint(point, FUNCTION),
                                                                Excel::ReadMonteCarloPaths(paths, FUNCTION), request,
                                                                settings ? settings->val_ : Script::SegmentedMonteCarloSettings_());
        result->reset(new StorableBlackScholesMonteCarloCurvatureResult_(name, std::move(value)));
    }

    void BlackScholesMonteCarloCurvatureResult_Get_Base(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>& result,
                                                        Handle_<StorableBlackScholesMonteCarloResult_>* base) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesMonteCarloCurvatureResult_Get_Base; result");
        const auto& curvature = value.Curvature();
        base->reset(new StorableBlackScholesMonteCarloResult_(
            result->Name(), Excel::MonteCarloMeanSnapshot_{value.Plan(), curvature.Point(), curvature.Settings(), curvature.Base()}));
    }

    void BlackScholesMonteCarloResult_Get_Value(const Handle_<StorableBlackScholesMonteCarloResult_>& result, double* value) {
        *value = Excel::CheckedCurvatureValue(result, "BlackScholesMonteCarloResult_Get_Value; result").mean_.MeanValue();
    }

    void BlackScholesMonteCarloResult_Get_Gradient(const Handle_<StorableBlackScholesMonteCarloResult_>& result, Matrix_<Cell_>* gradient) {
        *gradient = Excel::CurvatureVectorCells(
            Excel::CheckedCurvatureValue(result, "BlackScholesMonteCarloResult_Get_Gradient; result").mean_.MeanGradient());
    }

    void BlackScholesMonteCarloResult_Get_ParameterLabels(const Handle_<StorableBlackScholesMonteCarloResult_>& result, Vector_<String_>* labels) {
        *labels = Excel::CheckedCurvatureValue(result, "BlackScholesMonteCarloResult_Get_ParameterLabels; result").mean_.ParameterLabels();
    }

    void BlackScholesMonteCarloResult_Get_Execution(const Handle_<StorableBlackScholesMonteCarloResult_>& result, Matrix_<Cell_>* execution) {
        *execution = MeanExecution(Excel::CheckedCurvatureValue(result, "BlackScholesMonteCarloResult_Get_Execution; result").mean_.Execution());
    }

    void BlackScholesMonteCarloResult_Get_Plan(const Handle_<StorableBlackScholesMonteCarloResult_>& result,
                                               Handle_<StorableBlackScholesMonteCarloPlan_>* plan) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesMonteCarloResult_Get_Plan; result");
        plan->reset(new StorableBlackScholesMonteCarloPlan_(result->Name(), value.plan_));
    }

    void BlackScholesMonteCarloResult_Get_Point(const Handle_<StorableBlackScholesMonteCarloResult_>& result, Matrix_<Cell_>* point) {
        *point = Excel::CurvatureVectorCells(Excel::CheckedCurvatureValue(result, "BlackScholesMonteCarloResult_Get_Point; result").point_);
    }

    void BlackScholesMonteCarloResult_Get_Settings(const Handle_<StorableBlackScholesMonteCarloResult_>& result,
                                                   Handle_<StorableSegmentedMonteCarloSettings_>* settings) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesMonteCarloResult_Get_Settings; result");
        settings->reset(new StorableSegmentedMonteCarloSettings_(result->Name(), value.settings_));
    }

    void BlackScholesMonteCarloCurvatureResult_Get_Point(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>& result,
                                                         Matrix_<Cell_>* point) {
        *point = Excel::CurvatureVectorCells(
            Excel::CheckedCurvatureValue(result, "BlackScholesMonteCarloCurvatureResult_Get_Point; result").Curvature().Point());
    }

    void BlackScholesMonteCarloCurvatureResult_Get_Directions(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>& result,
                                                              Matrix_<Cell_>* directions) {
        *directions = Excel::CurvatureMatrixCells(
            Excel::CheckedCurvatureValue(result, "BlackScholesMonteCarloCurvatureResult_Get_Directions; result").Curvature().Directions());
    }

    void BlackScholesMonteCarloCurvatureResult_Get_Steps(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>& result,
                                                         Matrix_<Cell_>* steps) {
        *steps = Excel::CurvatureVectorCells(
            Excel::CheckedCurvatureValue(result, "BlackScholesMonteCarloCurvatureResult_Get_Steps; result").Curvature().Steps());
    }

    void BlackScholesMonteCarloCurvatureResult_Get_HessianProducts(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>& result,
                                                                   Matrix_<Cell_>* products) {
        *products = Excel::CurvatureMatrixCells(
            Excel::CheckedCurvatureValue(result, "BlackScholesMonteCarloCurvatureResult_Get_HessianProducts; result").Curvature().HessianProducts());
    }

    void BlackScholesMonteCarloCurvatureResult_Get_Shape(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>& result,
                                                         Matrix_<Cell_>* shape) {
        *shape = Excel::RiskShapeCells(
            Excel::CheckedCurvatureValue(result, "BlackScholesMonteCarloCurvatureResult_Get_Shape; result").Curvature().HessianProducts());
    }

    void BlackScholesMonteCarloCurvatureResult_Get_Execution(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>& result,
                                                             Matrix_<Cell_>* execution) {
        using namespace Excel;
        const auto& value = CheckedCurvatureValue(result, "BlackScholesMonteCarloCurvatureResult_Get_Execution; result").Curvature().Execution();
        *execution = FieldCells({Field("method", value.method_), Field("gradient_evaluations", double(value.gradientEvaluations_)),
                                 Field("numeric_payload_bytes", double(value.numericPayloadBytes_)),
                                 Field("max_path_tape_bytes", double(value.maxPathTapeBytes_)),
                                 Field("max_path_checkpoint_bytes", double(value.maxPathCheckpointBytes_)),
                                 Field("max_path_cleanup_reserve_bytes", double(value.maxPathCleanupReserveBytes_)),
                                 Field("recording_capacity_budget_bytes", CurvatureBudgetCell(value.recordingCapacityBudgetBytes_))});
    }

    void BlackScholesMonteCarloCurvatureResult_Get_Plan(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>& result,
                                                        Handle_<StorableBlackScholesMonteCarloPlan_>* plan) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesMonteCarloCurvatureResult_Get_Plan; result");
        plan->reset(new StorableBlackScholesMonteCarloPlan_(result->Name(), value.Plan()));
    }

    void BlackScholesMonteCarloCurvatureResult_Get_Settings(const Handle_<StorableBlackScholesMonteCarloCurvatureResult_>& result,
                                                            Handle_<StorableSegmentedMonteCarloSettings_>* settings) {
        const auto& value = Excel::CheckedCurvatureValue(result, "BlackScholesMonteCarloCurvatureResult_Get_Settings; result");
        settings->reset(new StorableSegmentedMonteCarloSettings_(result->Name(), value.Curvature().Settings()));
    }
    // clang-format off
#ifdef _WIN32
#include <dal-excel/auto/MG_SegmentedMonteCarloSettings_New_public.inc>
#include <dal-excel/auto/MG_SegmentedMonteCarloSettings_Get_Configuration_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloPlan_New_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloPlan_Get_Product_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloPlan_Get_Valuation_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloPlan_Get_Smoothing_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloPlan_Get_ParameterLabels_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloPlan_Get_ScriptConstants_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloPlan_Get_Observations_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloResult_New_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloResult_Get_Value_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloResult_Get_Gradient_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloResult_Get_ParameterLabels_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloResult_Get_Execution_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloResult_Get_Plan_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloResult_Get_Point_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloResult_Get_Settings_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloCurvatureResult_New_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloCurvatureResult_Get_Point_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloCurvatureResult_Get_Directions_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloCurvatureResult_Get_Steps_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloCurvatureResult_Get_HessianProducts_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloCurvatureResult_Get_Shape_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloCurvatureResult_Get_Execution_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloCurvatureResult_Get_Base_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloCurvatureResult_Get_Plan_public.inc>
#include <dal-excel/auto/MG_BlackScholesMonteCarloCurvatureResult_Get_Settings_public.inc>
#endif
    // clang-format on

} // namespace Dal
