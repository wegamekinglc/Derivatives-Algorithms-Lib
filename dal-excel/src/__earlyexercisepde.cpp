#include "__earlyexercisepde.hpp"

#include <array>
#include <set>

#include "__earlyexercisepdeinput.hpp"
#include "__platform.hpp"
#include "__riskrequestinput.hpp"
#include "__riskrequestrows.hpp"

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeSettings_New
    Create immutable early exercise settings with native budgets
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "EarlyExercisePdeSettings_New; name");
+const Excel::ScriptSettingsInput_ normalized(xl_settings); xl_settings = normalized.Get();
+argName = "settings"; Excel::ValidateEarlyExercisePdeSettingsInput(xl_settings, "EarlyExercisePdeSettings_New");
&optional
settings is cell[][]+
    Two key/value columns; blank uses defaults
&outputs
settingsHandle is handle StorableEarlyExercisePdeSettings
    Owning settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeSettings_Get_Configuration
    Copy all early exercise settings
&inputs
settings is handle StorableEarlyExercisePdeSettings
    Settings handle
&outputs
configuration is cell[][]
    Header and canonical key/value rows
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRequest_New
    Create an owning BS early exercise request
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "EarlyExercisePdeRequest_New; name");
+const Excel::ScriptSettingsInput_ normalized_contract(xl_contract); xl_contract = normalized_contract.Get();
+argName = "contract"; Excel::ValidateEarlyExercisePdeRange(xl_contract, "contract");
+const Excel::ScriptSettingsInput_ normalized_market(xl_market); xl_market = normalized_market.Get();
+argName = "market"; Excel::ValidateEarlyExercisePdeRange(xl_market, "market");
+const Excel::ScriptSettingsInput_ normalized_carry(xl_carry); xl_carry = normalized_carry.Get();
+argName = "carry"; Excel::ValidateEarlyExercisePdeRange(xl_carry, "carry");
+const Excel::ScriptSettingsInput_ normalized_exercise_dates(xl_exercise_dates); xl_exercise_dates = normalized_exercise_dates.Get();
+argName = "exercise_dates"; Excel::ValidateEarlyExercisePdeRange(xl_exercise_dates, "exercise_dates");
+xl_settings = Excel::ScriptScalarInput(xl_settings);
contract is cell[][]+
    EvaluationDate and MaturityDate integer Excel serials; Strike; OptionType; ExerciseStyle using snake_case keys
market is cell[][]+
    Spot and decimal Volatility using snake_case keys
carry is cell[][]+
    EndDate integer Excel serial / Rate / DividendYield; no header
&optional
exercise_dates is cell[][]+
    Single row or column of integer Excel date serials; required Bermudan; empty American
settings is handle StorableEarlyExercisePdeSettings
    Optional settings handle
&outputs
request is handle StorableEarlyExercisePdeRequest
    Copied contract and market
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRequest_Get_Settings
    Copy request settings
&inputs
request is handle StorableEarlyExercisePdeRequest
    Request handle
&outputs
settings is handle StorableEarlyExercisePdeSettings
    Detached settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeResult_New
    Price an early exercise option without recording AAD
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "EarlyExercisePdeResult_New; name");
request is handle StorableEarlyExercisePdeRequest
    Request handle
&outputs
result is handle StorableEarlyExercisePdeResult
    Owning result; inspect status before reading price
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeResult_Get_Request
    Copy the resolved request
&inputs
result is handle StorableEarlyExercisePdeResult
    Result handle
&outputs
request is handle StorableEarlyExercisePdeRequest
    Detached resolved request
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeResult_Get_Price
    Read price per unit underlying; fails if status is not SUCCESS
&inputs
result is handle StorableEarlyExercisePdeResult
    Result handle
&outputs
price is number
    Price in underlying quote units; numerical failure raises Error text
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRequest_Get_Contract
    Copy early exercise Contract diagnostics
&inputs
request is handle StorableEarlyExercisePdeRequest
    Owning request handle
&outputs
table is cell[][]
    Header plus detached rows; unavailable values are blank; zero-based indices
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRequest_Get_Market
    Copy early exercise Market diagnostics
&inputs
request is handle StorableEarlyExercisePdeRequest
    Owning request handle
&outputs
table is cell[][]
    Header plus detached rows; unavailable values are blank; zero-based indices
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRequest_Get_Carry
    Copy early exercise Carry diagnostics
&inputs
request is handle StorableEarlyExercisePdeRequest
    Owning request handle
&outputs
table is cell[][]
    Header plus detached rows; unavailable values are blank; zero-based indices
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRequest_Get_ExerciseDates
    Copy early exercise ExerciseDates diagnostics
&inputs
request is handle StorableEarlyExercisePdeRequest
    Owning request handle
&outputs
table is cell[][]
    Header plus detached rows; unavailable values are blank; zero-based indices
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeResult_Get_Summary
    Copy early exercise Summary diagnostics
&inputs
result is handle StorableEarlyExercisePdeResult
    Owning result handle
&outputs
table is cell[][]
    Header plus detached rows; unavailable values are blank; zero-based indices
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeResult_Get_Grid
    Copy early exercise Grid diagnostics
&inputs
result is handle StorableEarlyExercisePdeResult
    Owning result handle
&outputs
table is cell[][]
    Header plus detached rows; unavailable values are blank; zero-based indices
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeResult_Get_TimeGrid
    Copy early exercise TimeGrid diagnostics
&inputs
result is handle StorableEarlyExercisePdeResult
    Owning result handle
&outputs
table is cell[][]
    Header plus detached rows; unavailable values are blank; zero-based indices
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeResult_Get_Steps
    Copy early exercise Steps diagnostics
&inputs
result is handle StorableEarlyExercisePdeResult
    Owning result handle
&outputs
table is cell[][]
    Header plus detached rows; unavailable values are blank; zero-based indices
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeResult_Get_ExerciseLevels
    Copy early exercise ExerciseLevels diagnostics
&inputs
result is handle StorableEarlyExercisePdeResult
    Owning result handle
&outputs
table is cell[][]
    Header plus detached rows; unavailable values are blank; zero-based indices
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeResult_Get_ExerciseRegions
    Copy early exercise ExerciseRegions diagnostics
&inputs
result is handle StorableEarlyExercisePdeResult
    Owning result handle
&outputs
table is cell[][]
    Header plus detached rows; unavailable values are blank; zero-based indices
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeResult_Get_Execution
    Copy early exercise Execution diagnostics
&inputs
result is handle StorableEarlyExercisePdeResult
    Owning result handle
&outputs
table is cell[][]
    Header plus detached rows; unavailable values are blank; zero-based indices
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public EarlyExercisePdeResult_Get_Failure
    Copy early exercise Failure diagnostics
&inputs
result is handle StorableEarlyExercisePdeResult
    Owning result handle
&outputs
table is cell[][]
    Header plus detached rows; unavailable values are blank; zero-based indices
-IF-------------------------------------------------------------------------*/

// clang-format on

namespace Dal {
    namespace {
        template <class T_> const T_& Checked(const Handle_<Excel::StorableRiskValue_<T_>>& value) {
            REQUIRE(value, "InvalidEarlyExercisePdeRequest; handle must be nonnull");
            return value->val_;
        }
        void Name(const String_& name) { REQUIRE(name.find('\0') == String_::npos, "InvalidEarlyExercisePdeRequest; name; embedded NUL"); }
        double Number(const Cell_& cell, const String_& context) {
            const auto* value = std::get_if<double>(&cell.val_);
            REQUIRE(value && std::isfinite(*value),
                    "InvalidEarlyExercisePdeRequest; " + context + "; expected finite number excluding bool, dates and text");
            return *value;
        }
        Date_ DateCell(const Cell_& cell, const String_& context) {
            const double value = Number(cell, context);
            REQUIRE(std::trunc(value) == value && value >= Date::ToExcel(Date::Minimum()) && value <= Date::ToExcel(Date::Maximum()),
                    "InvalidEarlyExercisePdeRequest; " + context + "; expected valid integer Excel date serial");
            return Date::FromExcel(static_cast<int>(value));
        }
        bool EmptyRange(const Matrix_<Cell_>& rows) {
            return std::all_of(rows.begin(), rows.end(), [](const auto& cell) { return Cell::IsEmpty(cell); });
        }
        template <class F_> std::set<String_> Rows(const Matrix_<Cell_>& rows, const char* field, bool optional, F_ read) {
            std::set<String_> seen;
            if (EmptyRange(rows)) {
                REQUIRE(optional, String_("InvalidEarlyExercisePdeRequest; ") + field + "; required table is empty");
                return seen;
            }
            REQUIRE(rows.Cols() == 2, String_("InvalidEarlyExercisePdeRequest; ") + field + "; expected two columns");
            for (int row = 0; row < rows.Rows(); ++row) {
                const auto context = Excel::ScriptSettingLocation("EarlyExercisePde", field, row + 1, 2);
                const auto key = Excel::TextValue(rows(row, 0), context + "; key; ");
                REQUIRE(seen.insert(key).second, context + "; duplicate key " + key);
                read(key, rows(row, 1), context + "; " + key);
            }
            return seen;
        }
        template <class T_> struct SettingField_ {
            const char* key_;
            T_ EarlyExercisePdeSettings_::* member_;
        };
        const std::array<SettingField_<double>, 5> REAL_FIELDS = {
            {{"theta", &EarlyExercisePdeSettings_::theta_},
             {"penalty_factor", &EarlyExercisePdeSettings_::penaltyFactor_},
             {"absolute_tolerance", &EarlyExercisePdeSettings_::absoluteTolerance_},
             {"relative_tolerance", &EarlyExercisePdeSettings_::relativeTolerance_},
             {"linear_backward_error_limit", &EarlyExercisePdeSettings_::linearBackwardErrorLimit_}}};
        const std::array<SettingField_<int>, 5> INT_FIELDS = {{{"grid_points", &EarlyExercisePdeSettings_::gridPoints_},
                                                               {"ordinary_steps", &EarlyExercisePdeSettings_::ordinarySteps_},
                                                               {"rannacher_intervals", &EarlyExercisePdeSettings_::rannacherIntervals_},
                                                               {"max_iterations_per_step", &EarlyExercisePdeSettings_::maxIterationsPerStep_},
                                                               {"max_actual_steps", &EarlyExercisePdeSettings_::maxActualSteps_}}};
        const std::array<SettingField_<size_t>, 3> SIZE_FIELDS = {
            {{"max_work_units", &EarlyExercisePdeSettings_::maxWorkUnits_},
             {"numeric_payload_budget_bytes", &EarlyExercisePdeSettings_::numericPayloadBudgetBytes_},
             {"workspace_budget_bytes", &EarlyExercisePdeSettings_::workspaceBudgetBytes_}}};
        template <class T_>
        bool SetField(const String_& key, const Cell_& cell, const String_& context, const T_& fields, EarlyExercisePdeSettings_* settings) {
            for (const auto& field : fields) {
                if (key != field.key_)
                    continue;
                using Value_ = std::decay_t<decltype(settings->*(field.member_))>;
                if constexpr (std::is_same_v<Value_, double>)
                    settings->*(field.member_) = Number(cell, context);
                else {
                    const size_t value = Excel::PayloadBudget(cell, context, key.c_str());
                    REQUIRE(value <= static_cast<size_t>((std::numeric_limits<Value_>::max)()), context + "; integer out of range");
                    settings->*(field.member_) = static_cast<Value_>(value);
                }
                return true;
            }
            return false;
        }
        Cell_ ValueCell(double x) { return Cell_(x); }
        Cell_ ValueCell(int x) { return Cell_(double(x)); }
        Cell_ ValueCell(size_t x) {
            REQUIRE(x <= size_t(9007199254740991ULL), "EarlyExercisePde getter; integer cannot be represented exactly by Excel");
            return Cell_(double(x));
        }
        Cell_ ValueCell(bool x) { return Cell_(x); }
        Cell_ ValueCell(Date_ x) { return Cell_(double(Date::ToExcel(x))); }
        Cell_ ValueCell(const char* x) { return Cell_(x); }
        Cell_ ValueCell(const String_& x) { return Cell_(x); }
        template <class T_> Cell_ ValueCell(const std::optional<T_>& x) { return x ? ValueCell(*x) : Cell_(); }
        Matrix_<Cell_> Table(size_t rows, std::initializer_list<const char*> headers) {
            REQUIRE(rows < 1048576 && headers.size() <= 16384, "EarlyExercisePde getter; table plus header exceeds worksheet limits");
            Matrix_<Cell_> result(static_cast<int>(rows) + 1, static_cast<int>(headers.size()));
            int col = 0;
            for (const auto* name : headers)
                result(0, col++) = name;
            return result;
        }
        Matrix_<Cell_> Fields(const Vector_<std::pair<String_, Cell_>>& fields) {
            auto result = Table(fields.size(), {"Key", "Value"});
            for (size_t i = 0; i < fields.size(); ++i) {
                result(static_cast<int>(i) + 1, 0) = fields[i].first;
                result(static_cast<int>(i) + 1, 1) = fields[i].second;
            }
            return result;
        }
        void Required(const std::set<String_>& seen, std::initializer_list<const char*> keys, const char* field) {
            for (const auto* key : keys)
                REQUIRE(seen.count(key), String_("InvalidEarlyExercisePdeRequest; ") + field + "." + key + "; missing required key");
        }
    } // namespace

    void EarlyExercisePdeSettings_New(const String_& name, const Matrix_<Cell_>& rows, Handle_<StorableEarlyExercisePdeSettings_>* output) {
        Name(name);
        EarlyExercisePdeSettings_ value;
        Rows(rows, "settings", true, [&](const String_& key, const Cell_& cell, const String_& context) {
            if (key == "upper") {
                value.upper_ = Cell::IsEmpty(cell) ? std::nullopt : std::optional<double>(Number(cell, context));
                return;
            }
            REQUIRE(SetField(key, cell, context, REAL_FIELDS, &value) || SetField(key, cell, context, INT_FIELDS, &value) ||
                        SetField(key, cell, context, SIZE_FIELDS, &value),
                    context + "; unknown settings key");
        });
        ValidateEarlyExercisePdeSettings(value);
        output->reset(new StorableEarlyExercisePdeSettings_(name, std::move(value)));
    }
    void EarlyExercisePdeSettings_Get_Configuration(const Handle_<StorableEarlyExercisePdeSettings_>& settings, Matrix_<Cell_>* output) {
        const auto& value = Checked(settings);
        *output = Fields({{"grid_points", ValueCell(value.gridPoints_)},
                          {"upper", ValueCell(value.upper_)},
                          {"ordinary_steps", ValueCell(value.ordinarySteps_)},
                          {"theta", ValueCell(value.theta_)},
                          {"rannacher_intervals", ValueCell(value.rannacherIntervals_)},
                          {"penalty_factor", ValueCell(value.penaltyFactor_)},
                          {"absolute_tolerance", ValueCell(value.absoluteTolerance_)},
                          {"relative_tolerance", ValueCell(value.relativeTolerance_)},
                          {"linear_backward_error_limit", ValueCell(value.linearBackwardErrorLimit_)},
                          {"max_iterations_per_step", ValueCell(value.maxIterationsPerStep_)},
                          {"max_actual_steps", ValueCell(value.maxActualSteps_)},
                          {"max_work_units", ValueCell(value.maxWorkUnits_)},
                          {"numeric_payload_budget_bytes", ValueCell(value.numericPayloadBudgetBytes_)},
                          {"workspace_budget_bytes", ValueCell(value.workspaceBudgetBytes_)}});
    }
    void EarlyExercisePdeRequest_New(const String_& name,
                                     const Matrix_<Cell_>& contract,
                                     const Matrix_<Cell_>& market,
                                     const Matrix_<Cell_>& carry,
                                     const Matrix_<Cell_>& dates,
                                     const Handle_<StorableEarlyExercisePdeSettings_>& settings,
                                     Handle_<StorableEarlyExercisePdeRequest_>* output) {
        Name(name);
        EarlyExercisePdeRequest_ value;
        value.settings_ = settings ? settings->val_ : EarlyExercisePdeSettings_();
        const auto seenContract = Rows(contract, "contract", false, [&](const String_& key, const Cell_& cell, const String_& context) {
            auto& c = value.contract_;
            if (key == "evaluation_date")
                c.evaluationDate_ = DateCell(cell, context);
            else if (key == "maturity_date")
                c.maturityDate_ = DateCell(cell, context);
            else if (key == "strike")
                c.strike_ = Number(cell, context);
            else if (key == "option_type") {
                const auto text = Excel::TextValue(cell, context);
                REQUIRE(text == "CALL" || text == "PUT", context + "; expected CALL or PUT");
                c.optionType_ = OptionType_(text);
            } else if (key == "exercise_style") {
                const auto text = Excel::TextValue(cell, context);
                REQUIRE(text == "BERMUDAN" || text == "AMERICAN", context + "; expected BERMUDAN or AMERICAN");
                c.exerciseStyle_ = EarlyExerciseStyle_(text);
            } else
                THROW(context + "; unknown contract key");
        });
        Required(seenContract, {"evaluation_date", "maturity_date", "strike", "option_type", "exercise_style"}, "contract");
        const auto seenMarket = Rows(market, "market", false, [&](const String_& key, const Cell_& cell, const String_& context) {
            if (key == "spot")
                value.market_.spot_ = Number(cell, context);
            else if (key == "volatility")
                value.market_.volatility_ = Number(cell, context);
            else
                THROW(context + "; unknown market key");
        });
        Required(seenMarket, {"spot", "volatility"}, "market");
        REQUIRE(carry.Cols() == 3 && carry.Rows() > 0, "InvalidEarlyExercisePdeRequest; carry; expected nonempty three-column rectangle");
        value.market_.carry_.reserve(carry.Rows());
        for (int row = 0; row < carry.Rows(); ++row) {
            const auto context = Excel::ScriptSettingLocation("EarlyExercisePdeRequest_New", "carry", row + 1, 1);
            value.market_.carry_.push_back({DateCell(carry(row, 0), context), Number(carry(row, 1), context + "; column=2 rate"),
                                            Number(carry(row, 2), context + "; column=3 dividend_yield")});
        }
        if (!EmptyRange(dates)) {
            REQUIRE(dates.Rows() == 1 || dates.Cols() == 1, "InvalidEarlyExercisePdeRequest; exercise_dates; expected single row or column");
            const int count = std::max(dates.Rows(), dates.Cols());
            value.contract_.exerciseDates_.reserve(count);
            for (int i = 0; i < count; ++i)
                value.contract_.exerciseDates_.push_back(
                    DateCell(dates(dates.Rows() == 1 ? 0 : i, dates.Rows() == 1 ? i : 0), "exercise_dates[" + String_(std::to_string(i)) + "]"));
        }
        ValidateEarlyExercisePdeRequest(value);
        output->reset(new StorableEarlyExercisePdeRequest_(name, std::move(value)));
    }
    void EarlyExercisePdeRequest_Get_Settings(const Handle_<StorableEarlyExercisePdeRequest_>& request,
                                              Handle_<StorableEarlyExercisePdeSettings_>* output) {
        output->reset(new StorableEarlyExercisePdeSettings_("", Checked(request).settings_));
    }
    void EarlyExercisePdeRequest_Get_Contract(const Handle_<StorableEarlyExercisePdeRequest_>& request, Matrix_<Cell_>* output) {
        const auto& c = Checked(request).contract_;
        *output = Fields({{"evaluation_date", ValueCell(c.evaluationDate_)},
                          {"maturity_date", ValueCell(c.maturityDate_)},
                          {"strike", ValueCell(c.strike_)},
                          {"option_type", ValueCell(c.optionType_.String())},
                          {"exercise_style", ValueCell(c.exerciseStyle_.String())}});
    }
    void EarlyExercisePdeRequest_Get_Market(const Handle_<StorableEarlyExercisePdeRequest_>& request, Matrix_<Cell_>* output) {
        const auto& m = Checked(request).market_;
        *output = Fields({{"spot", ValueCell(m.spot_)}, {"volatility", ValueCell(m.volatility_)}});
    }
    void EarlyExercisePdeRequest_Get_Carry(const Handle_<StorableEarlyExercisePdeRequest_>& request, Matrix_<Cell_>* output) {
        const auto& carry = Checked(request).market_.carry_;
        auto result = Table(carry.size(), {"EndDate", "Rate", "DividendYield"});
        for (size_t i = 0; i < carry.size(); ++i) {
            result(static_cast<int>(i) + 1, 0) = ValueCell(carry[i].endDate_);
            result(static_cast<int>(i) + 1, 1) = carry[i].rate_;
            result(static_cast<int>(i) + 1, 2) = carry[i].dividendYield_;
        }
        *output = std::move(result);
    }
    void EarlyExercisePdeRequest_Get_ExerciseDates(const Handle_<StorableEarlyExercisePdeRequest_>& request, Matrix_<Cell_>* output) {
        const auto& dates = Checked(request).contract_.exerciseDates_;
        auto result = Table(dates.size(), {"ExerciseDate"});
        for (size_t i = 0; i < dates.size(); ++i)
            result(static_cast<int>(i) + 1, 0) = ValueCell(dates[i]);
        *output = std::move(result);
    }
    void EarlyExercisePdeResult_New(const String_& name,
                                    const Handle_<StorableEarlyExercisePdeRequest_>& request,
                                    Handle_<StorableEarlyExercisePdeResult_>* output) {
        Name(name);
        output->reset(new StorableEarlyExercisePdeResult_(name, EvaluateEarlyExercisePde(Checked(request))));
    }
    void EarlyExercisePdeResult_Get_Request(const Handle_<StorableEarlyExercisePdeResult_>& result,
                                            Handle_<StorableEarlyExercisePdeRequest_>* output) {
        output->reset(new StorableEarlyExercisePdeRequest_("", Checked(result).Request()));
    }
    void EarlyExercisePdeResult_Get_Price(const Handle_<StorableEarlyExercisePdeResult_>& result, double* output) {
        *output = Checked(result).Price();
    }
    void EarlyExercisePdeResult_Get_Summary(const Handle_<StorableEarlyExercisePdeResult_>& result, Matrix_<Cell_>* output) {
        const auto& r = Checked(result);
        const auto& e = r.Execution();
        *output = Fields({{"Status", ValueCell(r.Status().String())},
                          {"IsSuccess", ValueCell(r.IsSuccess())},
                          {"Method", ValueCell(r.Method())},
                          {"Price", r.IsSuccess() ? ValueCell(r.Price()) : Cell_()},
                          {"PriceUnit", ValueCell("price per unit underlying")},
                          {"AbsoluteTolerance", ValueCell(r.Request().settings_.absoluteTolerance_)},
                          {"RelativeTolerance", ValueCell(r.Request().settings_.relativeTolerance_)},
                          {"MaxLinearBackwardError", ValueCell(e.maxLinearBackwardError_)},
                          {"MaxComplementarityResidual", ValueCell(e.maxComplementarityResidual_)}});
    }
    void EarlyExercisePdeResult_Get_Grid(const Handle_<StorableEarlyExercisePdeResult_>& result, Matrix_<Cell_>* output) {
        const auto& r = Checked(result);
        auto cells = Table(r.Grid().size(), {"NodeIndex", "Spot", "Value"});
        for (size_t i = 0; i < r.Grid().size(); ++i) {
            cells(static_cast<int>(i) + 1, 0) = ValueCell(i);
            cells(static_cast<int>(i) + 1, 1) = r.Grid()[i];
            if (r.IsSuccess())
                cells(static_cast<int>(i) + 1, 2) = r.Values()[i];
        }
        *output = std::move(cells);
    }
    void EarlyExercisePdeResult_Get_TimeGrid(const Handle_<StorableEarlyExercisePdeResult_>& result, Matrix_<Cell_>* output) {
        const auto& rows = Checked(result).TimeGrid();
        auto cells = Table(rows.size(), {"TimeIndex", "TimeYears", "CalendarDate", "IsExerciseDate", "IsCarryKnot", "IsMaturity"});
        for (size_t i = 0; i < rows.size(); ++i) {
            cells(static_cast<int>(i) + 1, 0) = ValueCell(i);
            cells(static_cast<int>(i) + 1, 1) = ValueCell(rows[i].timeYears_);
            cells(static_cast<int>(i) + 1, 2) = ValueCell(rows[i].calendarDate_);
            cells(static_cast<int>(i) + 1, 3) = ValueCell(rows[i].isExerciseDate_);
            cells(static_cast<int>(i) + 1, 4) = ValueCell(rows[i].isCarryKnot_);
            cells(static_cast<int>(i) + 1, 5) = ValueCell(rows[i].isMaturity_);
        }
        *output = std::move(cells);
    }
    void EarlyExercisePdeResult_Get_Steps(const Handle_<StorableEarlyExercisePdeResult_>& result, Matrix_<Cell_>* output) {
        const auto& rows = Checked(result).StepDiagnostics();
        auto cells = Table(rows.size(), {"rollback_step", "source_time_index", "time_index", "solve_count", "dt", "theta", "iteration_change",
                                         "damped", "projection", "completed", "linear_backward_error", "projection_change", "projection_residual",
                                         "obstacle_violation", "dual_violation", "complementarity_residual", "normalized_obstacle_violation",
                                         "normalized_dual_violation", "normalized_complementarity_residual"});
        for (size_t i = 0; i < rows.size(); ++i) {
            cells(static_cast<int>(i) + 1, 0) = ValueCell(rows[i].rollbackStep_);
            cells(static_cast<int>(i) + 1, 1) = ValueCell(rows[i].sourceTimeIndex_);
            cells(static_cast<int>(i) + 1, 2) = ValueCell(rows[i].timeIndex_);
            cells(static_cast<int>(i) + 1, 3) = ValueCell(rows[i].solveCount_);
            cells(static_cast<int>(i) + 1, 4) = ValueCell(rows[i].dt_);
            cells(static_cast<int>(i) + 1, 5) = ValueCell(rows[i].theta_);
            cells(static_cast<int>(i) + 1, 6) = ValueCell(rows[i].iterationChange_);
            cells(static_cast<int>(i) + 1, 7) = ValueCell(rows[i].damped_);
            cells(static_cast<int>(i) + 1, 8) = ValueCell(rows[i].projection_);
            cells(static_cast<int>(i) + 1, 9) = ValueCell(rows[i].completed_);
            cells(static_cast<int>(i) + 1, 10) = ValueCell(rows[i].linearBackwardError_);
            cells(static_cast<int>(i) + 1, 11) = ValueCell(rows[i].projectionChange_);
            cells(static_cast<int>(i) + 1, 12) = ValueCell(rows[i].projectionResidual_);
            cells(static_cast<int>(i) + 1, 13) = ValueCell(rows[i].obstacleViolation_);
            cells(static_cast<int>(i) + 1, 14) = ValueCell(rows[i].dualViolation_);
            cells(static_cast<int>(i) + 1, 15) = ValueCell(rows[i].complementarityResidual_);
            cells(static_cast<int>(i) + 1, 16) = ValueCell(rows[i].normalizedObstacleViolation_);
            cells(static_cast<int>(i) + 1, 17) = ValueCell(rows[i].normalizedDualViolation_);
            cells(static_cast<int>(i) + 1, 18) = ValueCell(rows[i].normalizedComplementarityResidual_);
        }
        *output = std::move(cells);
    }
    void EarlyExercisePdeResult_Get_ExerciseLevels(const Handle_<StorableEarlyExercisePdeResult_>& result, Matrix_<Cell_>* output) {
        const auto& rows = Checked(result).ExerciseLevels();
        auto cells = Table(rows.size(), {"time_index", "calendar_date", "region_offset", "region_count", "epsilon", "kind"});
        for (size_t i = 0; i < rows.size(); ++i) {
            cells(static_cast<int>(i) + 1, 0) = ValueCell(rows[i].timeIndex_);
            cells(static_cast<int>(i) + 1, 1) = ValueCell(rows[i].calendarDate_);
            cells(static_cast<int>(i) + 1, 2) = ValueCell(rows[i].regionOffset_);
            cells(static_cast<int>(i) + 1, 3) = ValueCell(rows[i].regionCount_);
            cells(static_cast<int>(i) + 1, 4) = ValueCell(rows[i].epsilon_);
            cells(static_cast<int>(i) + 1, 5) = ValueCell(rows[i].kind_.String());
        }
        *output = std::move(cells);
    }
    void EarlyExercisePdeResult_Get_ExerciseRegions(const Handle_<StorableEarlyExercisePdeResult_>& result, Matrix_<Cell_>* output) {
        const auto& rows = Checked(result).ExerciseRegions();
        auto cells = Table(rows.size(), {"time_index", "region_index", "first_node", "last_node", "left_clipped", "right_clipped"});
        for (size_t i = 0; i < rows.size(); ++i) {
            cells(static_cast<int>(i) + 1, 0) = ValueCell(rows[i].timeIndex_);
            cells(static_cast<int>(i) + 1, 1) = ValueCell(rows[i].regionIndex_);
            cells(static_cast<int>(i) + 1, 2) = ValueCell(rows[i].firstNode_);
            cells(static_cast<int>(i) + 1, 3) = ValueCell(rows[i].lastNode_);
            cells(static_cast<int>(i) + 1, 4) = ValueCell(rows[i].leftClipped_);
            cells(static_cast<int>(i) + 1, 5) = ValueCell(rows[i].rightClipped_);
        }
        *output = std::move(cells);
    }
    void EarlyExercisePdeResult_Get_Execution(const Handle_<StorableEarlyExercisePdeResult_>& result, Matrix_<Cell_>* output) {
        const auto& e = Checked(result).Execution();
        *output = Fields({{"planned_steps", ValueCell(e.plannedSteps_)},
                          {"actual_steps", ValueCell(e.actualSteps_)},
                          {"completed_steps", ValueCell(e.completedSteps_)},
                          {"base_steps", ValueCell(e.baseSteps_)},
                          {"damping_added_steps", ValueCell(e.dampingAddedSteps_)},
                          {"stability_added_steps", ValueCell(e.stabilityAddedSteps_)},
                          {"effective_rannacher_intervals", ValueCell(e.effectiveRannacherIntervals_)},
                          {"max_iterations", ValueCell(e.maxIterations_)},
                          {"upwind_rows", ValueCell(e.upwindRows_)},
                          {"total_solves", ValueCell(e.totalSolves_)},
                          {"work_units", ValueCell(e.workUnits_)},
                          {"planned_region_capacity", ValueCell(e.plannedRegionCapacity_)},
                          {"region_capacity", ValueCell(e.regionCapacity_)},
                          {"planned_numeric_payload_bytes", ValueCell(e.plannedNumericPayloadBytes_)},
                          {"planned_workspace_bytes", ValueCell(e.plannedWorkspaceBytes_)},
                          {"numeric_payload_bytes", ValueCell(e.numericPayloadBytes_)},
                          {"peak_workspace_bytes", ValueCell(e.peakWorkspaceBytes_)},
                          {"epsilon", ValueCell(e.epsilon_)},
                          {"max_linear_backward_error", ValueCell(e.maxLinearBackwardError_)},
                          {"max_obstacle_violation", ValueCell(e.maxObstacleViolation_)},
                          {"max_dual_violation", ValueCell(e.maxDualViolation_)},
                          {"max_complementarity_residual", ValueCell(e.maxComplementarityResidual_)}});
    }
    void EarlyExercisePdeResult_Get_Failure(const Handle_<StorableEarlyExercisePdeResult_>& result, Matrix_<Cell_>* output) {
        const auto& failure = Checked(result).Failure();
        if (!failure) {
            *output = Table(0, {"Key", "Value"});
            return;
        }
        const auto& f = *failure;
        *output = Fields({{"status", ValueCell(f.status_.String())},
                          {"reason", ValueCell(f.reason_)},
                          {"time_index", ValueCell(f.timeIndex_)},
                          {"iteration", ValueCell(f.iteration_)},
                          {"calendar_date", ValueCell(f.calendarDate_)},
                          {"node", ValueCell(f.node_)},
                          {"residual", ValueCell(f.residual_)},
                          {"threshold", ValueCell(f.threshold_)},
                          {"work_units", ValueCell(f.workUnits_)}});
    }
    // clang-format off
#ifdef _WIN32
#include <dal-excel/auto/MG_EarlyExercisePdeSettings_New_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeSettings_Get_Configuration_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRequest_New_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRequest_Get_Settings_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeResult_New_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeResult_Get_Request_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeResult_Get_Price_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRequest_Get_Contract_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRequest_Get_Market_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRequest_Get_Carry_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRequest_Get_ExerciseDates_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeResult_Get_Summary_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeResult_Get_Grid_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeResult_Get_TimeGrid_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeResult_Get_Steps_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeResult_Get_ExerciseLevels_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeResult_Get_ExerciseRegions_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeResult_Get_Execution_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeResult_Get_Failure_public.inc>
#endif
    // clang-format on
} // namespace Dal
