//
// Created by Codex on 2026/10/11.
//

#include "__europeanpderisk.hpp"

#include "__europeanpdeinput.hpp"
#include "__platform.hpp"
#include "__riskrequestrows.hpp"

// clang-format off
/*IF--------------------------------------------------------------------------
public EuropeanPdeRiskSettings_New
    Create checked immutable European PDE settings
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "EuropeanPdeRiskSettings_New; name");
+const Excel::ScriptSettingsInput_ normalized(xl_settings); xl_settings = normalized.Get();
+argName = "settings"; Excel::ValidateRiskRequestSettings(xl_settings, "EuropeanPdeRiskSettings_New");
&optional
settings is cell[][]+
    Two key/value columns; blank defaults; budgets exclude worksheet cells and labels
&outputs
configuration is handle StorableEuropeanPdeRiskSettings
    Passive physical settings and optional native budgets
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EuropeanPdeRiskSettings_Get_Configuration
    Copy European PDE settings and optional native budgets
&inputs
settings is handle StorableEuropeanPdeRiskSettings
    Immutable settings
&outputs
configuration is cell[][]
    Ten canonical key/value rows; unset optional values are blank
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EuropeanPdeRiskRequest_New
    Create a passive fixed-grid European call and put risk request
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "EuropeanPdeRiskRequest_New; name");
+argName = "rate"; Excel::ValidateEuropeanPdeNumber(xl_rate, "rate");
+xl_rate = Excel::ScriptScalarInput(xl_rate);
+const Excel::ScriptSettingsInput_ rateInput(xl_rate); xl_rate = rateInput.Get();
+argName = "volatility"; Excel::ValidateEuropeanPdeNumber(xl_volatility, "volatility");
+xl_volatility = Excel::ScriptScalarInput(xl_volatility);
+const Excel::ScriptSettingsInput_ volatilityInput(xl_volatility); xl_volatility = volatilityInput.Get();
+argName = "strike"; Excel::ValidateEuropeanPdeNumber(xl_strike, "strike");
+xl_strike = Excel::ScriptScalarInput(xl_strike);
+const Excel::ScriptSettingsInput_ strikeInput(xl_strike); xl_strike = strikeInput.Get();
+xl_settings = Excel::ScriptScalarInput(xl_settings);
rate is number
    Finite continuously compounded decimal rate
volatility is number
    Finite decimal volatility; positive domain checked at execution
strike is number
    Finite strike price; domain and grid kink checked at execution
&optional
settings is handle StorableEuropeanPdeRiskSettings
    Blank uses native defaults
&outputs
request is handle StorableEuropeanPdeRiskRequest
    Copied passive settings, point and budgets
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EuropeanPdeRiskRequest_Get_Settings
    Copy settings retained by a European PDE request
&inputs
request is handle StorableEuropeanPdeRiskRequest
    Passive request
&outputs
settings is handle StorableEuropeanPdeRiskSettings
    Detached physical settings and budgets
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EuropeanPdeRiskRequest_Get_Point
    Copy the European PDE rate, volatility and strike
&inputs
request is handle StorableEuropeanPdeRiskRequest
    Passive request
&outputs
point is cell[][]
    Rate, volatility and strike key/value rows
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EuropeanPdeRiskResult_New
    Execute native AAD European call and put risk on an empty caller tape
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "EuropeanPdeRiskResult_New; name");
request is handle StorableEuropeanPdeRiskRequest
    Checked passive request
&outputs
result is handle StorableEuropeanPdeRiskResult
    Owning prices, first derivatives and solve diagnostics
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EuropeanPdeRiskResult_Get_Request
    Copy the actually resolved European PDE request
&inputs
result is handle StorableEuropeanPdeRiskResult
    Completed result
&outputs
request is handle StorableEuropeanPdeRiskRequest
    Detached request with resolved zero-based spot index
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EuropeanPdeRiskResult_Get_Prices
    Copy European call and put prices
&inputs
result is handle StorableEuropeanPdeRiskResult
    Completed owning result
&outputs
prices is cell[][]
    Payoff/Price header and Call/Put rows
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EuropeanPdeRiskResult_Get_Jacobian
    Copy raw European first derivatives and their units
&inputs
result is handle StorableEuropeanPdeRiskResult
    Completed owning result
&outputs
risks is cell[][]
    Six Payoff/Parameter/Unit/Derivative rows; raw decimal rate and volatility units
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EuropeanPdeRiskResult_Get_Grid
    Copy the fixed physical European spot grid
&inputs
result is handle StorableEuropeanPdeRiskResult
    Completed owning result
&outputs
grid is cell[][]
    NodeIndex/Spot header; zero-based node indices
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EuropeanPdeRiskResult_Get_ForwardErrors
    Copy chronological European forward solve errors
&inputs
result is handle StorableEuropeanPdeRiskResult
    Completed owning result
&outputs
errors is cell[][]
    Step/Call/Put header; one-based actual step ordinals
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EuropeanPdeRiskResult_Get_TransposeErrors
    Copy chronological European transpose solve errors
&inputs
result is handle StorableEuropeanPdeRiskResult
    Completed owning result
&outputs
errors is cell[][]
    Step plus four layer/seed columns; one-based actual step ordinals
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EuropeanPdeRiskResult_Get_Execution
    Copy European PDE method and native resource counters
&inputs
result is handle StorableEuropeanPdeRiskResult
    Completed owning result
&outputs
execution is cell[][]
    Method and five counters; native budgets exclude worksheet cells and labels
-IF-------------------------------------------------------------------------*/
// clang-format on

namespace Dal {
    namespace {
        constexpr int MAX_OUTPUT_ROWS = 1048576;

        void CheckName(const String_& name, const char* function) {
            REQUIRE(name.find('\0') == String_::npos, String_(function) + "; name; embedded NUL is unsupported");
        }

        double Number(const Cell_& cell, const String_& context) {
            const auto* value = std::get_if<double>(&cell.val_);
            REQUIRE(value && std::isfinite(*value), context + "expected finite numeric cell, excluding bool, text and dates");
            return *value;
        }

        int Integer(const Cell_& cell, const String_& context) {
            const size_t value = Excel::PayloadBudget(cell, context, "value");
            REQUIRE(value <= static_cast<size_t>((std::numeric_limits<int>::max)()), context + "integer exceeds int range");
            return static_cast<int>(value);
        }

        bool ReadPhysicalNumber(const String_& key, const Cell_& cell, const String_& context, EuropeanPdeSettings_* settings) {
            struct Field_ {
                const char* key_;
                double EuropeanPdeSettings_::* member_;
            };
            const std::array<Field_, 3> fields = {{{"upper", &EuropeanPdeSettings_::upper_},
                                                   {"expiry", &EuropeanPdeSettings_::expiry_},
                                                   {"dividend_yield", &EuropeanPdeSettings_::dividendYield_}}};
            const auto found = std::find_if(fields.begin(), fields.end(), [&](const auto& field) { return key == field.key_; });
            if (found == fields.end())
                return false;
            const double value = Number(cell, context);
            REQUIRE(key == "dividend_yield" || value > 0.0, context + "expected positive number");
            settings->*(found->member_) = value;
            return true;
        }

        bool ReadErrorLimit(const String_& key, const Cell_& cell, const String_& context, EuropeanPdeSettings_* settings) {
            const std::array<const char*, 2> keys = {"forward_backward_error_limit", "transpose_backward_error_limit"};
            const auto found = std::find(keys.begin(), keys.end(), key);
            if (found == keys.end())
                return false;
            const double value = Number(cell, context);
            REQUIRE(value >= 0.0 && value <= 1.0, context + "expected solve-error limit in [0,1]");
            const std::array<double*, 2> limits = {&settings->accuracy_.forwardBackwardErrorLimit_,
                                                   &settings->accuracy_.transposeBackwardErrorLimit_};
            *limits[static_cast<size_t>(found - keys.begin())] = value;
            return true;
        }

        void ReadExtent(const String_& key, const Cell_& cell, const String_& context, EuropeanPdeSettings_* settings) {
            if (key == "spot_index")
                settings->spotIndex_ = Cell::IsEmpty(cell) ? std::nullopt : std::optional<int>(Integer(cell, context));
            else if (key == "grid_points")
                settings->gridPoints_ = Integer(cell, context);
            else
                settings->ordinarySteps_ = Integer(cell, context);
        }

        void ReadBudget(const String_& key, const Cell_& cell, const String_& context, Excel::EuropeanPdeRiskSettings_* settings) {
            const auto value = Cell::IsEmpty(cell) ? std::nullopt : std::optional<size_t>(Excel::PayloadBudget(cell, context, key.c_str()));
            if (key == "numeric_payload_budget_bytes")
                settings->numericPayloadBudgetBytes_ = value;
            else
                settings->recordingCapacityBudgetBytes_ = value;
        }

        void ReadSetting(const String_& key, const Cell_& cell, const String_& context, Excel::EuropeanPdeRiskSettings_* settings) {
            const Vector_<String_> keys{"grid_points",
                                        "ordinary_steps",
                                        "upper",
                                        "spot_index",
                                        "expiry",
                                        "dividend_yield",
                                        "forward_backward_error_limit",
                                        "transpose_backward_error_limit",
                                        "numeric_payload_budget_bytes",
                                        "recording_capacity_budget_bytes"};
            REQUIRE(std::find(keys.begin(), keys.end(), key) != keys.end(),
                    context + "unknown settings key; expected " + String::Accumulate(keys, ", "));
            if (ReadPhysicalNumber(key, cell, context, &settings->physical_) || ReadErrorLimit(key, cell, context, &settings->physical_))
                return;
            if (key == "grid_points" || key == "ordinary_steps" || key == "spot_index")
                ReadExtent(key, cell, context, &settings->physical_);
            else
                ReadBudget(key, cell, context, settings);
        }

        template <class T_> const T_& Checked(const Handle_<Excel::StorableRiskValue_<T_>>& handle, const char* field) {
            REQUIRE(handle, String_("InvalidEuropeanPdeRiskRequest: ") + field + "; handle is null");
            return handle->val_;
        }

        Cell_ BudgetCell(const std::optional<size_t>& value) { return value ? Cell_(double(*value)) : Cell_(); }

        Matrix_<Cell_> DiagnosticCells(const Matrix_<>& errors, const Vector_<String_>& labels) {
            Matrix_<Cell_> cells(errors.Rows() + 1, errors.Cols() + 1);
            cells(0, 0) = "Step";
            for (int column = 0; column < errors.Cols(); ++column)
                cells(0, column + 1) = labels[column];
            for (int row = 0; row < errors.Rows(); ++row) {
                cells(row + 1, 0) = double(row + 1);
                for (int column = 0; column < errors.Cols(); ++column)
                    cells(row + 1, column + 1) = errors(row, column);
            }
            return cells;
        }
    } // namespace

    void EuropeanPdeRiskSettings_New(const String_& name, const Matrix_<Cell_>& rows, Handle_<StorableEuropeanPdeRiskSettings_>* settings) {
        CheckName(name, "EuropeanPdeRiskSettings_New");
        Excel::EuropeanPdeRiskSettings_ value;
        Excel::ReadRows(
            rows, "EuropeanPdeRiskSettings_New", "settings",
            [&](const String_& key, const Cell_& cell, const String_&, const String_& context) { ReadSetting(key, cell, context, &value); }, true);
        static_cast<void>(ResolveEuropeanPdeSettings(value.physical_));
        REQUIRE(value.physical_.gridPoints_ < MAX_OUTPUT_ROWS, "EuropeanPdeRiskSettings_New: grid_points plus header exceeds worksheet row limit");
        REQUIRE(value.physical_.ordinarySteps_ <= MAX_OUTPUT_ROWS - 3,
                "EuropeanPdeRiskSettings_New: ordinary_steps plus two steps and header exceeds worksheet row limit");
        settings->reset(new StorableEuropeanPdeRiskSettings_(name, std::move(value)));
    }

    void EuropeanPdeRiskSettings_Get_Configuration(const Handle_<StorableEuropeanPdeRiskSettings_>& settings, Matrix_<Cell_>* configuration) {
        using namespace Excel;
        const auto& value = Checked(settings, "EuropeanPdeRiskSettings_Get_Configuration; settings");
        const auto& physical = value.physical_;
        *configuration = FieldCells({Field("grid_points", physical.gridPoints_), Field("ordinary_steps", physical.ordinarySteps_),
                                     Field("upper", physical.upper_), Field("spot_index", OptionalCell(physical.spotIndex_)),
                                     Field("expiry", physical.expiry_), Field("dividend_yield", physical.dividendYield_),
                                     Field("forward_backward_error_limit", physical.accuracy_.forwardBackwardErrorLimit_),
                                     Field("transpose_backward_error_limit", physical.accuracy_.transposeBackwardErrorLimit_),
                                     Field("numeric_payload_budget_bytes", BudgetCell(value.numericPayloadBudgetBytes_)),
                                     Field("recording_capacity_budget_bytes", BudgetCell(value.recordingCapacityBudgetBytes_))});
    }

    void EuropeanPdeRiskRequest_New(const String_& name,
                                    double rate,
                                    double volatility,
                                    double strike,
                                    const Handle_<StorableEuropeanPdeRiskSettings_>& settings,
                                    Handle_<StorableEuropeanPdeRiskRequest_>* request) {
        CheckName(name, "EuropeanPdeRiskRequest_New");
        const auto config = settings ? settings->val_ : Excel::EuropeanPdeRiskSettings_();
        EuropeanPdeRiskRequest_ value{
            {rate, volatility, strike}, config.physical_, config.numericPayloadBudgetBytes_, config.recordingCapacityBudgetBytes_};
        const std::array<const char*, 3> fields = {"rate", "volatility", "strike"};
        for (size_t index = 0; index < fields.size(); ++index)
            REQUIRE(std::isfinite(value.point_[index]), String_("EuropeanPdeRiskRequest_New: ") + fields[index] + " must be finite");
        request->reset(new StorableEuropeanPdeRiskRequest_(name, std::move(value)));
    }

    void EuropeanPdeRiskRequest_Get_Settings(const Handle_<StorableEuropeanPdeRiskRequest_>& request,
                                             Handle_<StorableEuropeanPdeRiskSettings_>* settings) {
        const auto& value = Checked(request, "EuropeanPdeRiskRequest_Get_Settings; request");
        settings->reset(
            new StorableEuropeanPdeRiskSettings_("", {value.settings_, value.numericPayloadBudgetBytes_, value.recordingCapacityBudgetBytes_}));
    }

    void EuropeanPdeRiskRequest_Get_Point(const Handle_<StorableEuropeanPdeRiskRequest_>& request, Matrix_<Cell_>* point) {
        using namespace Excel;
        const auto& value = Checked(request, "EuropeanPdeRiskRequest_Get_Point; request");
        *point = FieldCells({Field("rate", value.point_[0]), Field("volatility", value.point_[1]), Field("strike", value.point_[2])});
    }

    void EuropeanPdeRiskResult_New(const String_& name,
                                   const Handle_<StorableEuropeanPdeRiskRequest_>& request,
                                   Handle_<StorableEuropeanPdeRiskResult_>* result) {
        CheckName(name, "EuropeanPdeRiskResult_New");
        result->reset(new StorableEuropeanPdeRiskResult_(name, EvaluateEuropeanPdeRisk(Checked(request, "EuropeanPdeRiskResult_New; request"))));
    }

    void EuropeanPdeRiskResult_Get_Request(const Handle_<StorableEuropeanPdeRiskResult_>& result, Handle_<StorableEuropeanPdeRiskRequest_>* request) {
        const auto& value = Checked(result, "EuropeanPdeRiskResult_Get_Request; result");
        request->reset(new StorableEuropeanPdeRiskRequest_("", value.request_));
    }

    void EuropeanPdeRiskResult_Get_Prices(const Handle_<StorableEuropeanPdeRiskResult_>& result, Matrix_<Cell_>* prices) {
        const auto& value = Checked(result, "EuropeanPdeRiskResult_Get_Prices; result");
        Matrix_<Cell_> cells(3, 2);
        cells(0, 0) = "Payoff";
        cells(0, 1) = "Price";
        for (int layer = 0; layer < 2; ++layer) {
            cells(layer + 1, 0) = value.payoffLabels_[layer];
            cells(layer + 1, 1) = value.prices_[layer];
        }
        *prices = std::move(cells);
    }

    void EuropeanPdeRiskResult_Get_Jacobian(const Handle_<StorableEuropeanPdeRiskResult_>& result, Matrix_<Cell_>* risks) {
        const auto& value = Checked(result, "EuropeanPdeRiskResult_Get_Jacobian; result");
        Matrix_<Cell_> cells(7, 4);
        const std::array<const char*, 4> headers = {"Payoff", "Parameter", "Unit", "Derivative"};
        for (int column = 0; column < 4; ++column)
            cells(0, column) = headers[column];
        for (int layer = 0; layer < 2; ++layer)
            for (int coordinate = 0; coordinate < 3; ++coordinate) {
                const int row = 1 + 3 * layer + coordinate;
                cells(row, 0) = value.payoffLabels_[layer];
                cells(row, 1) = value.parameterLabels_[coordinate];
                cells(row, 2) = value.parameterUnits_[coordinate];
                cells(row, 3) = value.jacobian_(layer, coordinate);
            }
        *risks = std::move(cells);
    }

    void EuropeanPdeRiskResult_Get_Grid(const Handle_<StorableEuropeanPdeRiskResult_>& result, Matrix_<Cell_>* grid) {
        const auto& value = Checked(result, "EuropeanPdeRiskResult_Get_Grid; result");
        Matrix_<Cell_> cells(static_cast<int>(value.grid_.size()) + 1, 2);
        cells(0, 0) = "NodeIndex";
        cells(0, 1) = "Spot";
        for (int node = 0; node < static_cast<int>(value.grid_.size()); ++node) {
            cells(node + 1, 0) = double(node);
            cells(node + 1, 1) = value.grid_[node];
        }
        *grid = std::move(cells);
    }

    void EuropeanPdeRiskResult_Get_ForwardErrors(const Handle_<StorableEuropeanPdeRiskResult_>& result, Matrix_<Cell_>* errors) {
        const auto& value = Checked(result, "EuropeanPdeRiskResult_Get_ForwardErrors; result");
        *errors = DiagnosticCells(value.forwardBackwardErrors_, value.payoffLabels_);
    }

    void EuropeanPdeRiskResult_Get_TransposeErrors(const Handle_<StorableEuropeanPdeRiskResult_>& result, Matrix_<Cell_>* errors) {
        const auto& value = Checked(result, "EuropeanPdeRiskResult_Get_TransposeErrors; result");
        *errors = DiagnosticCells(value.transposeBackwardErrors_, value.transposeErrorLabels_);
    }

    void EuropeanPdeRiskResult_Get_Execution(const Handle_<StorableEuropeanPdeRiskResult_>& result, Matrix_<Cell_>* execution) {
        using namespace Excel;
        const auto& value = Checked(result, "EuropeanPdeRiskResult_Get_Execution; result");
        const auto& counts = value.execution_;
        *execution =
            FieldCells({Field("method", value.method_), Field("actual_steps", counts.actualSteps_),
                        Field("numeric_payload_bytes", double(counts.numericPayloadBytes_)), Field("peak_tape_bytes", double(counts.peakTapeBytes_)),
                        Field("cleanup_reserve_bytes", double(counts.cleanupReserveBytes_)),
                        Field("reverse_scratch_peak_bytes", double(counts.reverseScratchPeakBytes_))});
    }
    // clang-format off
#ifdef _WIN32
#include <dal-excel/auto/MG_EuropeanPdeRiskSettings_New_public.inc>
#include <dal-excel/auto/MG_EuropeanPdeRiskSettings_Get_Configuration_public.inc>
#include <dal-excel/auto/MG_EuropeanPdeRiskRequest_New_public.inc>
#include <dal-excel/auto/MG_EuropeanPdeRiskRequest_Get_Settings_public.inc>
#include <dal-excel/auto/MG_EuropeanPdeRiskRequest_Get_Point_public.inc>
#include <dal-excel/auto/MG_EuropeanPdeRiskResult_New_public.inc>
#include <dal-excel/auto/MG_EuropeanPdeRiskResult_Get_Request_public.inc>
#include <dal-excel/auto/MG_EuropeanPdeRiskResult_Get_Prices_public.inc>
#include <dal-excel/auto/MG_EuropeanPdeRiskResult_Get_Jacobian_public.inc>
#include <dal-excel/auto/MG_EuropeanPdeRiskResult_Get_Grid_public.inc>
#include <dal-excel/auto/MG_EuropeanPdeRiskResult_Get_ForwardErrors_public.inc>
#include <dal-excel/auto/MG_EuropeanPdeRiskResult_Get_TransposeErrors_public.inc>
#include <dal-excel/auto/MG_EuropeanPdeRiskResult_Get_Execution_public.inc>
#endif
    // clang-format on
} // namespace Dal
