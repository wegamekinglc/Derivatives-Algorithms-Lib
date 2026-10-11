//
// Created by Codex on 2026/10/11.
//

#include "__curvaturerequest.hpp"

#include "__curvatureinput.hpp"
#include "__curvaturerows.hpp"

// clang-format off
/*IF--------------------------------------------------------------------------
public BumpOverAADRequest_New
    Create an owning finite-step curvature direction request
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "BumpOverAADRequest_New; name");
+argName = "directions"; Excel::ValidateCurvatureNumbers(xl_directions, "BumpOverAADRequest_New", "directions");
+const Excel::ScriptSettingsInput_ normalizedDirections(xl_directions); xl_directions = normalizedDirections.Get();
+argName = "steps"; Excel::ValidateCurvatureNumbers(xl_steps, "BumpOverAADRequest_New", "steps");
+const Excel::ScriptSettingsInput_ normalizedSteps(xl_steps); xl_steps = normalizedSteps.Get();
+const Excel::ScriptSettingsInput_ normalizedSettings(xl_settings); xl_settings = normalizedSettings.Get();
+argName = "settings"; Excel::ValidateRiskRequestSettings(xl_settings, "BumpOverAADRequest_New");
&optional
directions is cell[][]+
    Finite direction rows; blank means zero directions with input_count from settings
steps is cell[][]+
    One finite positive step per direction; row or column vector; blank for zero directions
settings is cell[][]+
    input_count and optional native budgets; blank caps unset, zero caps actual; worksheet copies excluded
&outputs
request is handle StorableBumpOverAADRequest
    Copied passive directions, steps and optional budgets
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BumpOverAADRequest_Get_Directions
    Copy finite-step direction rows in full raw input order
&inputs
request is handle StorableBumpOverAADRequest
    Owning passive request
&outputs
directions is cell[][]
    Direction rows; empty shape spills one blank cell; query Get_Shape for its logical dimensions
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BumpOverAADRequest_Get_Steps
    Copy positive finite-step sizes as a column
&inputs
request is handle StorableBumpOverAADRequest
    Owning passive request
&outputs
steps is cell[][]
    One step per direction row; zero directions spill one blank cell
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BumpOverAADRequest_Get_Settings
    Copy the logical input count and optional native budgets
&inputs
request is handle StorableBumpOverAADRequest
    Owning passive request
&outputs
settings is cell[][]
    Three key/value rows; unset budgets are blank; worksheet ownership and spill copies are excluded
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public BumpOverAADRequest_Get_Shape
    Copy finite-step direction and input counts
&inputs
request is handle StorableBumpOverAADRequest
    Owning passive request
&outputs
shape is cell[][]
    One row containing direction count then input count; retains zero-by-N shapes
-IF-------------------------------------------------------------------------*/
// clang-format on

namespace Dal {
    namespace {
        struct Settings_ {
            std::optional<int> inputCount_;
            std::optional<size_t> numericBudget_;
            std::optional<size_t> recordingBudget_;
        };

        void ReadSetting(const String_& key, const Cell_& cell, const String_& context, Settings_* value) {
            if (key == "input_count") {
                if (!Cell::IsEmpty(cell)) {
                    const size_t count = Excel::PayloadBudget(cell, context, "input_count");
                    REQUIRE(count < static_cast<size_t>(Excel::CURVATURE_MAX_ROWS), context + "input_count plus axis header exceeds worksheet rows");
                    value->inputCount_ = static_cast<int>(count);
                }
                return;
            }
            const std::array<std::pair<const char*, std::optional<size_t>*>, 2> fields = {
                {{"numeric_payload_budget_bytes", &value->numericBudget_}, {"recording_capacity_budget_bytes", &value->recordingBudget_}}};
            const auto found = std::find_if(fields.begin(), fields.end(), [&](const auto& field) { return key == field.first; });
            REQUIRE(found != fields.end(),
                    context + "unknown key; expected input_count, numeric_payload_budget_bytes or recording_capacity_budget_bytes");
            *found->second = Cell::IsEmpty(cell) ? std::nullopt : std::optional<size_t>(Excel::PayloadBudget(cell, context, found->first));
        }

        Matrix_<> EmptyDirections(const Matrix_<Cell_>& cells, const std::optional<int>& inputCount) {
            const int columns = cells.Rows() == 0 ? cells.Cols() : 0;
            REQUIRE(columns == 0 || !inputCount || columns == *inputCount,
                    "BumpOverAADRequest_New: input_count does not match the typed empty direction matrix");
            const int count = inputCount.value_or(columns);
            REQUIRE(count < Excel::CURVATURE_MAX_ROWS, "BumpOverAADRequest_New: input_count plus axis header exceeds worksheet rows");
            return Matrix_<>(0, count);
        }

        Matrix_<> Directions(const Matrix_<Cell_>& cells, const std::optional<int>& inputCount) {
            using namespace Excel;
            constexpr const char* FUNCTION = "BumpOverAADRequest_New";
            if (BlankCurvatureRange(cells))
                return EmptyDirections(cells, inputCount);
            REQUIRE(cells.Rows() <= CURVATURE_MAX_ROWS && cells.Cols() <= CURVATURE_MAX_COLUMNS,
                    "BumpOverAADRequest_New: directions exceed worksheet bounds");
            REQUIRE(!inputCount || *inputCount == cells.Cols(), "BumpOverAADRequest_New: input_count must match direction columns");
            Matrix_<> matrix(cells.Rows(), cells.Cols());
            for (int row = 0; row < cells.Rows(); ++row)
                for (int column = 0; column < cells.Cols(); ++column)
                    matrix(row, column) = CurvatureNumber(cells(row, column), ScriptSettingLocation(FUNCTION, "directions", row + 1, column + 1));
            return matrix;
        }

        Vector_<> Steps(const Matrix_<Cell_>& cells) {
            if (Excel::BlankCurvatureRange(cells))
                return {};
            REQUIRE(cells.Rows() == 1 || cells.Cols() == 1, "BumpOverAADRequest_New: steps must be a row or column vector");
            Vector_<> values;
            for (int row = 0; row < cells.Rows(); ++row)
                for (int column = 0; column < cells.Cols(); ++column) {
                    const auto context = Excel::ScriptSettingLocation("BumpOverAADRequest_New", "steps", row + 1, column + 1);
                    const double step = Excel::CurvatureNumber(cells(row, column), context);
                    REQUIRE(step > 0.0, context + "step must be positive");
                    values.push_back(step);
                }
            return values;
        }
    } // namespace

    void BumpOverAADRequest_New(const String_& name,
                                const Matrix_<Cell_>& directions,
                                const Matrix_<Cell_>& steps,
                                const Matrix_<Cell_>& settings,
                                Handle_<StorableBumpOverAADRequest_>* request) {
        Excel::CheckCurvatureText(name, "BumpOverAADRequest_New; name");
        Settings_ config;
        Excel::ReadRows(
            settings, "BumpOverAADRequest_New", "settings",
            [&](const String_& key, const Cell_& cell, const String_& keyContext, const String_& context) {
                Excel::CheckCurvatureText(key, keyContext + "key");
                ReadSetting(key, cell, context, &config);
            },
            true);
        auto matrix = Directions(directions, config.inputCount_);
        auto sizes = Steps(steps);
        REQUIRE(sizes.size() == static_cast<size_t>(matrix.Rows()), "BumpOverAADRequest_New: steps requires exactly one value per direction row");
        request->reset(new StorableBumpOverAADRequest_(name, {std::move(matrix), std::move(sizes), config.numericBudget_, config.recordingBudget_}));
    }

    void BumpOverAADRequest_Get_Directions(const Handle_<StorableBumpOverAADRequest_>& request, Matrix_<Cell_>* directions) {
        const auto& value = Excel::CheckedCurvatureValue(request, "BumpOverAADRequest_Get_Directions; request");
        *directions = Excel::CurvatureMatrixCells(value.directions_);
    }

    void BumpOverAADRequest_Get_Steps(const Handle_<StorableBumpOverAADRequest_>& request, Matrix_<Cell_>* steps) {
        const auto& value = Excel::CheckedCurvatureValue(request, "BumpOverAADRequest_Get_Steps; request");
        *steps = Excel::CurvatureVectorCells(value.steps_);
    }

    void BumpOverAADRequest_Get_Settings(const Handle_<StorableBumpOverAADRequest_>& request, Matrix_<Cell_>* settings) {
        using namespace Excel;
        const auto& value = CheckedCurvatureValue(request, "BumpOverAADRequest_Get_Settings; request");
        *settings = FieldCells({Field("input_count", value.directions_.Cols()),
                                Field("numeric_payload_budget_bytes", CurvatureBudgetCell(value.numericPayloadBudgetBytes_)),
                                Field("recording_capacity_budget_bytes", CurvatureBudgetCell(value.recordingCapacityBudgetBytes_))});
    }

    void BumpOverAADRequest_Get_Shape(const Handle_<StorableBumpOverAADRequest_>& request, Matrix_<Cell_>* shape) {
        const auto& value = Excel::CheckedCurvatureValue(request, "BumpOverAADRequest_Get_Shape; request");
        *shape = Excel::RiskShapeCells(value.directions_);
    }

    // clang-format off
#ifdef _WIN32
#include <dal-excel/auto/MG_BumpOverAADRequest_New_public.inc>
#include <dal-excel/auto/MG_BumpOverAADRequest_Get_Directions_public.inc>
#include <dal-excel/auto/MG_BumpOverAADRequest_Get_Steps_public.inc>
#include <dal-excel/auto/MG_BumpOverAADRequest_Get_Settings_public.inc>
#include <dal-excel/auto/MG_BumpOverAADRequest_Get_Shape_public.inc>
#endif
    // clang-format on
} // namespace Dal
