#include "__earlyexercisepderisk.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>

#include "__earlyexercisepderiskinput.hpp"
#include "__platform.hpp"

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskSettings_New
    Create owning early exercise risk settings and cumulative budgets
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateEarlyExercisePdeRiskName(xl_name);
+const Excel::ScriptSettingsInput_ normalized(xl_settings); xl_settings = normalized.Get();
+argName = "settings"; Excel::ValidateEarlyExercisePdeRiskRange(xl_settings, "settings");
+const Excel::ScriptSettingsInput_ normalized_bumps(xl_carry_bumps); xl_carry_bumps = normalized_bumps.Get();
+argName = "carry_bumps"; Excel::ValidateEarlyExercisePdeRiskRange(xl_carry_bumps, "carry_bumps");
&optional
settings is cell[][]+
    Headerless canonical scalar key/value rows; blank uses defaults
carry_bumps is cell[][]+
    Headerless rate/yield bump columns; each blank column defaults independently
&outputs
settingsHandle is handle StorableEarlyExercisePdeRiskSettings
    Owning settings
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskSettings_Get_Configuration
    Copy early exercise risk Configuration without native solves
&inputs
settings is handle StorableEarlyExercisePdeRiskSettings
    Owning handle
&outputs
configuration is cell[][]
    Key/Value header and scalar settings in declaration order
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskSettings_Get_CarryBumps
    Copy early exercise risk CarryBumps without native solves
&inputs
settings is handle StorableEarlyExercisePdeRiskSettings
    Owning handle
&outputs
bumps is cell[][]
    SegmentIndex/RateBump/DividendYieldBump; default family blank
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskRequest_New
    Copy a price request and optional risk settings
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateEarlyExercisePdeRiskName(xl_name);
+xl_risk = Excel::ScriptScalarInput(xl_risk);
valuation is handle StorableEarlyExercisePdeRequest
    Existing price request
&optional
risk is handle StorableEarlyExercisePdeRiskSettings
    Blank uses default risk settings
&outputs
request is handle StorableEarlyExercisePdeRiskRequest
    Owning risk request
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskRequest_Get_Valuation
    Copy early exercise risk Valuation without native solves
&inputs
request is handle StorableEarlyExercisePdeRiskRequest
    Owning handle
&outputs
valuation is handle StorableEarlyExercisePdeRequest
    Detached original valuation
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskRequest_Get_Settings
    Copy early exercise risk Settings without native solves
&inputs
request is handle StorableEarlyExercisePdeRiskRequest
    Owning handle
&outputs
settings is handle StorableEarlyExercisePdeRiskSettings
    Detached risk settings
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_New
    Evaluate fixed-plan passive early exercise risks; inspect availability
&inputs
name is string
    Object name
+argName = "name"; Excel::ValidateEarlyExercisePdeRiskName(xl_name);
request is handle StorableEarlyExercisePdeRiskRequest
    Owning risk request
&outputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning result including numerical failures
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_Get_Request
    Copy early exercise risk Request without native solves
&inputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning handle
&outputs
request is handle StorableEarlyExercisePdeRiskRequest
    Detached original risk request
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_Get_BaseResult
    Copy early exercise risk BaseResult without native solves
&inputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning handle
&outputs
base is handle StorableEarlyExercisePdeResult
    Detached base result with resolved valuation
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_Get_ResolvedSettings
    Copy early exercise risk ResolvedSettings without native solves
&inputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning handle
&outputs
settings is handle StorableEarlyExercisePdeRiskSettings
    Detached resolved bumps and settings
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_Get_Price
    Copy early exercise risk Price without native solves
&inputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning handle
&outputs
price is number
    Price per unit underlying; unavailable raises Error text
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_Get_RiskValue
    Copy early exercise risk RiskValue without native solves
&inputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning handle
risk_id is string
    Exact canonical case-sensitive risk ID
+argName = "risk_id"; Excel::ValidateEarlyExercisePdeRiskName(xl_risk_id, "risk_id");
&outputs
value is number
    Finite qualified partial; unavailable or unknown ID raises Error text
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_Get_Summary
    Copy early exercise risk Summary without native solves
&inputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning handle
&outputs
table is cell[][]
    Header plus detached schema-v1 rows; unavailable values blank
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_Get_Plan
    Copy early exercise risk Plan without native solves
&inputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning handle
&outputs
table is cell[][]
    Header plus detached schema-v1 rows; unavailable values blank
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_Get_PlanGrid
    Copy early exercise risk PlanGrid without native solves
&inputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning handle
&outputs
table is cell[][]
    Header plus detached schema-v1 rows; unavailable values blank
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_Get_PlanTimeGrid
    Copy early exercise risk PlanTimeGrid without native solves
&inputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning handle
&outputs
table is cell[][]
    Header plus detached schema-v1 rows; unavailable values blank
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_Get_PlanSteps
    Copy early exercise risk PlanSteps without native solves
&inputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning handle
&outputs
table is cell[][]
    Header plus detached schema-v1 rows; unavailable values blank
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_Get_BumpOrigins
    Copy early exercise risk BumpOrigins without native solves
&inputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning handle
&outputs
table is cell[][]
    Header plus detached schema-v1 rows; unavailable values blank
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_Get_Risks
    Copy early exercise risk Risks without native solves
&inputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning handle
&outputs
table is cell[][]
    Header plus detached schema-v1 rows; unavailable values blank
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_Get_Stencils
    Copy early exercise risk Stencils without native solves
&inputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning handle
&outputs
table is cell[][]
    Header plus detached schema-v1 rows; unavailable values blank
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_Get_Scenarios
    Copy early exercise risk Scenarios without native solves
&inputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning handle
&outputs
table is cell[][]
    Header plus detached schema-v1 rows; unavailable values blank
-IF-------------------------------------------------------------------------*/
// clang-format on

// clang-format off
/*IF--------------------------------------------------------------------------
public EarlyExercisePdeRiskResult_Get_Execution
    Copy early exercise risk Execution without native solves
&inputs
result is handle StorableEarlyExercisePdeRiskResult
    Owning handle
&outputs
table is cell[][]
    Header plus detached schema-v1 rows; unavailable values blank
-IF-------------------------------------------------------------------------*/
// clang-format on

namespace Dal {
    namespace {
        constexpr const char* IDENTIFIER = "InvalidEarlyExercisePdeRiskRequest";
        template <class T_> const T_& Checked(const Handle_<Excel::StorableRiskValue_<T_>>& value) {
            REQUIRE(value, String_(IDENTIFIER) + "; handle; expected nonnull typed handle");
            return value->val_;
        }
        void Name(const String_& name, const char* field = "name") {
            REQUIRE(name.find('\0') == String_::npos, String_(IDENTIFIER) + "; " + field + "; embedded NUL");
        }
        double Number(const Cell_& cell, const String_& context) {
            const auto* value = std::get_if<double>(&cell.val_);
            REQUIRE(value && std::isfinite(*value),
                    String_(IDENTIFIER) + "; " + context + "; expected finite numeric cell excluding bool, dates and text");
            return *value;
        }
        size_t Count(const Cell_& cell, const String_& context) {
            const double value = Number(cell, context);
            REQUIRE(value >= 0 && value == std::trunc(value) && value <= 9007199254740991.0 &&
                        value < std::ldexp(1.0, std::numeric_limits<size_t>::digits),
                    String_(IDENTIFIER) + "; " + context + "; expected nonnegative size_t integer <=2^53-1");
            return static_cast<size_t>(value);
        }
        bool Boolean(const Cell_& cell, const String_& context) {
            const auto* value = std::get_if<bool>(&cell.val_);
            REQUIRE(value, String_(IDENTIFIER) + "; " + context + "; expected native Boolean cell");
            return *value;
        }
        bool Empty(const Matrix_<Cell_>& cells) {
            return std::all_of(cells.begin(), cells.end(), [](const auto& cell) { return Cell::IsEmpty(cell); });
        }
        String_ Location(const char* field, int row, int col) {
            return String_(IDENTIFIER) + "; " + field + "; row=" + String_(std::to_string(row + 1)) + "; column=" + String_(std::to_string(col + 1));
        }
        template <class T_> struct SettingField_ {
            const char* key_;
            T_ EarlyExercisePdeRiskSettings_::* member_;
        };
        const std::array<SettingField_<bool>, 4> BOOL_FIELDS = {{{"delta", &EarlyExercisePdeRiskSettings_::delta_},
                                                                 {"gamma", &EarlyExercisePdeRiskSettings_::gamma_},
                                                                 {"vega", &EarlyExercisePdeRiskSettings_::vega_},
                                                                 {"carry_risk", &EarlyExercisePdeRiskSettings_::carryRisk_}}};
        const std::array<SettingField_<double>, 6> REAL_FIELDS = {
            {{"volatility_bump", &EarlyExercisePdeRiskSettings_::volatilityBump_},
             {"delta_stability_tolerance", &EarlyExercisePdeRiskSettings_::deltaStabilityTolerance_},
             {"gamma_stability_tolerance", &EarlyExercisePdeRiskSettings_::gammaStabilityTolerance_},
             {"vega_stability_tolerance", &EarlyExercisePdeRiskSettings_::vegaStabilityTolerance_},
             {"rate_stability_tolerance", &EarlyExercisePdeRiskSettings_::rateStabilityTolerance_},
             {"dividend_yield_stability_tolerance", &EarlyExercisePdeRiskSettings_::dividendYieldStabilityTolerance_}}};
        const std::array<SettingField_<size_t>, 4> SIZE_FIELDS = {
            {{"max_scenarios", &EarlyExercisePdeRiskSettings_::maxScenarios_},
             {"max_work_units", &EarlyExercisePdeRiskSettings_::maxWorkUnits_},
             {"numeric_payload_budget_bytes", &EarlyExercisePdeRiskSettings_::numericPayloadBudgetBytes_},
             {"workspace_budget_bytes", &EarlyExercisePdeRiskSettings_::workspaceBudgetBytes_}}};
        template <class T_, size_t N_>
        bool Set(const std::string& key,
                 const Cell_& cell,
                 const String_& context,
                 const std::array<SettingField_<T_>, N_>& fields,
                 EarlyExercisePdeRiskSettings_* value) {
            for (const auto& field : fields) {
                if (key != field.key_)
                    continue;
                if constexpr (std::is_same_v<T_, bool>)
                    value->*(field.member_) = Boolean(cell, context);
                else if constexpr (std::is_same_v<T_, size_t>)
                    value->*(field.member_) = Count(cell, context);
                else {
                    const double number = Number(cell, context);
                    REQUIRE(number > 0.0 && (key != "volatility_bump" || number / 2.0 > 0.0),
                            context + "; value=" + String_(std::to_string(number)) + "; expected positive value and representable bump half");
                    value->*(field.member_) = number;
                }
                return true;
            }
            return false;
        }
        void Scalars(const Matrix_<Cell_>& rows, EarlyExercisePdeRiskSettings_* value) {
            if (Empty(rows))
                return;
            REQUIRE(rows.Cols() == 2, String_(IDENTIFIER) + "; settings; expected two columns");
            std::set<std::string> seen;
            for (int i = 0; i < rows.Rows(); ++i) {
                const auto context = Location("settings", i, 0);
                const auto* text = std::get_if<String_>(&rows(i, 0).val_);
                REQUIRE(text && text->find('\0') == String_::npos, context + "; expected canonical ASCII key");
                const std::string key(text->data(), text->size());
                REQUIRE(seen.insert(key).second, context + "; duplicate key " + *text);
                const auto field = Location("settings", i, 1) + "; risk." + *text;
                if (key == "spot_bump") {
                    if (!Cell::IsEmpty(rows(i, 1))) {
                        const double bump = Number(rows(i, 1), field);
                        REQUIRE(bump > 0.0 && std::isfinite(2.0 * bump),
                                field + "; value=" + String_(std::to_string(bump)) + "; expected positive finite fine/coarse bump");
                        value->spotBump_ = bump;
                    }
                } else
                    REQUIRE(Set(key, rows(i, 1), field, BOOL_FIELDS, value) || Set(key, rows(i, 1), field, REAL_FIELDS, value) ||
                                Set(key, rows(i, 1), field, SIZE_FIELDS, value),
                            field + "; unknown canonical key");
            }
        }
        void Carry(const Matrix_<Cell_>& rows, EarlyExercisePdeRiskSettings_* value) {
            if (Empty(rows))
                return;
            REQUIRE(rows.Cols() == 2, String_(IDENTIFIER) + "; carry_bumps; expected two columns");
            const std::array<Vector_<double>*, 2> vectors = {&value->rateBumps_, &value->dividendYieldBumps_};
            for (int col = 0; col < 2; ++col) {
                bool allBlank = true;
                for (int row = 0; row < rows.Rows(); ++row)
                    allBlank = allBlank && Cell::IsEmpty(rows(row, col));
                if (allBlank)
                    continue;
                for (int row = 0; row < rows.Rows(); ++row) {
                    const auto context = Location("carry_bumps", row, col);
                    const double bump = Number(rows(row, col), context);
                    REQUIRE(bump > 0.0 && bump / 2.0 > 0.0, context + "; expected positive representable bump and half bump");
                    vectors[col]->push_back(bump);
                }
            }
        }
        template <class T_, class = void> struct IsDalEnum_ : std::false_type {};
        template <class T_> struct IsDalEnum_<T_, std::void_t<decltype(std::declval<T_>().String())>> : std::true_type {};
        template <class T_> Cell_ ValueCell(const T_& value) {
            if constexpr (IsDalEnum_<T_>::value)
                return Cell_(value.String());
            else if constexpr (std::is_same_v<T_, Date_>)
                return Cell_(double(Date::ToExcel(value)));
            else if constexpr (std::is_integral_v<T_> && !std::is_same_v<T_, bool>) {
                REQUIRE(double(value) <= 9007199254740991.0, "EarlyExercisePdeRisk getter; count exceeds Excel exact integer range");
                return Cell_(double(value));
            } else
                return Cell_(value);
        }
        template <class T_> Cell_ ValueCell(const std::optional<T_>& value) { return value ? ValueCell(*value) : Cell_(); }
        Matrix_<Cell_> Table(size_t rows, std::initializer_list<const char*> headers) {
            REQUIRE(rows < 1048576, "EarlyExercisePdeRisk getter; table plus header exceeds worksheet rows");
            Matrix_<Cell_> result(static_cast<int>(rows) + 1, static_cast<int>(headers.size()));
            int i = 0;
            for (const auto* header : headers)
                result(0, i++) = header;
            return result;
        }
        void Row(Matrix_<Cell_>* table, int row, std::initializer_list<Cell_> values) {
            int col = 0;
            for (const auto& value : values)
                (*table)(row, col++) = value;
        }
        struct Field_ {
            const char* key_;
            Cell_ value_;
        };
        Matrix_<Cell_> Fields(std::initializer_list<Field_> fields, const char* first = "Field") {
            auto table = Table(fields.size(), {first, "Value"});
            int row = 1;
            for (const auto& field : fields)
                Row(&table, row++, {Cell_(field.key_), field.value_});
            return table;
        }
    } // namespace

    void EarlyExercisePdeRiskSettings_New(const String_& name,
                                          const Matrix_<Cell_>& settings,
                                          const Matrix_<Cell_>& bumps,
                                          Handle_<StorableEarlyExercisePdeRiskSettings_>* output) {
        Name(name);
        EarlyExercisePdeRiskSettings_ value;
        Scalars(settings, &value);
        Carry(bumps, &value);
        ValidateEarlyExercisePdeRiskSettings(value);
        output->reset(new StorableEarlyExercisePdeRiskSettings_(name, std::move(value)));
    }
    void EarlyExercisePdeRiskSettings_Get_Configuration(const Handle_<StorableEarlyExercisePdeRiskSettings_>& settings, Matrix_<Cell_>* output) {
        const auto& value = Checked(settings);
        *output = Fields({{"delta", ValueCell(value.delta_)},
                          {"gamma", ValueCell(value.gamma_)},
                          {"vega", ValueCell(value.vega_)},
                          {"carry_risk", ValueCell(value.carryRisk_)},
                          {"spot_bump", ValueCell(value.spotBump_)},
                          {"volatility_bump", ValueCell(value.volatilityBump_)},
                          {"delta_stability_tolerance", ValueCell(value.deltaStabilityTolerance_)},
                          {"gamma_stability_tolerance", ValueCell(value.gammaStabilityTolerance_)},
                          {"vega_stability_tolerance", ValueCell(value.vegaStabilityTolerance_)},
                          {"rate_stability_tolerance", ValueCell(value.rateStabilityTolerance_)},
                          {"dividend_yield_stability_tolerance", ValueCell(value.dividendYieldStabilityTolerance_)},
                          {"max_scenarios", ValueCell(value.maxScenarios_)},
                          {"max_work_units", ValueCell(value.maxWorkUnits_)},
                          {"numeric_payload_budget_bytes", ValueCell(value.numericPayloadBudgetBytes_)},
                          {"workspace_budget_bytes", ValueCell(value.workspaceBudgetBytes_)}},
                         "Key");
    }
    void EarlyExercisePdeRiskSettings_Get_CarryBumps(const Handle_<StorableEarlyExercisePdeRiskSettings_>& settings, Matrix_<Cell_>* output) {
        const auto& value = Checked(settings);
        auto table = Table(std::max(value.rateBumps_.size(), value.dividendYieldBumps_.size()), {"SegmentIndex", "RateBump", "DividendYieldBump"});
        for (int i = 1; i < table.Rows(); ++i) {
            table(i, 0) = double(i - 1);
            if (i <= value.rateBumps_.size())
                table(i, 1) = value.rateBumps_[i - 1];
            if (i <= value.dividendYieldBumps_.size())
                table(i, 2) = value.dividendYieldBumps_[i - 1];
        }
        *output = std::move(table);
    }
    void EarlyExercisePdeRiskRequest_New(const String_& name,
                                         const Handle_<StorableEarlyExercisePdeRequest_>& valuation,
                                         const Handle_<StorableEarlyExercisePdeRiskSettings_>& settings,
                                         Handle_<StorableEarlyExercisePdeRiskRequest_>* output) {
        Name(name);
        EarlyExercisePdeRiskRequest_ value{Checked(valuation), settings ? settings->val_ : EarlyExercisePdeRiskSettings_()};
        ValidateEarlyExercisePdeRiskRequest(value);
        output->reset(new StorableEarlyExercisePdeRiskRequest_(name, std::move(value)));
    }
    void EarlyExercisePdeRiskResult_New(const String_& name,
                                        const Handle_<StorableEarlyExercisePdeRiskRequest_>& request,
                                        Handle_<StorableEarlyExercisePdeRiskResult_>* output) {
        Name(name);
        output->reset(new StorableEarlyExercisePdeRiskResult_(name, EvaluateEarlyExercisePdeRisk(Checked(request))));
    }
    void EarlyExercisePdeRiskRequest_Get_Valuation(const Handle_<StorableEarlyExercisePdeRiskRequest_>& input,
                                                   Handle_<StorableEarlyExercisePdeRequest_>* output) {
        output->reset(new StorableEarlyExercisePdeRequest_("", Checked(input).valuation_));
    }
    void EarlyExercisePdeRiskRequest_Get_Settings(const Handle_<StorableEarlyExercisePdeRiskRequest_>& input,
                                                  Handle_<StorableEarlyExercisePdeRiskSettings_>* output) {
        output->reset(new StorableEarlyExercisePdeRiskSettings_("", Checked(input).risk_));
    }
    void EarlyExercisePdeRiskResult_Get_Request(const Handle_<StorableEarlyExercisePdeRiskResult_>& input,
                                                Handle_<StorableEarlyExercisePdeRiskRequest_>* output) {
        output->reset(new StorableEarlyExercisePdeRiskRequest_("", Checked(input).Request()));
    }
    void EarlyExercisePdeRiskResult_Get_BaseResult(const Handle_<StorableEarlyExercisePdeRiskResult_>& input,
                                                   Handle_<StorableEarlyExercisePdeResult_>* output) {
        output->reset(new StorableEarlyExercisePdeResult_("", Checked(input).BaseResult()));
    }
    void EarlyExercisePdeRiskResult_Get_ResolvedSettings(const Handle_<StorableEarlyExercisePdeRiskResult_>& input,
                                                         Handle_<StorableEarlyExercisePdeRiskSettings_>* output) {
        output->reset(new StorableEarlyExercisePdeRiskSettings_("", Checked(input).Plan().resolvedRisk_));
    }
    void EarlyExercisePdeRiskResult_Get_Price(const Handle_<StorableEarlyExercisePdeRiskResult_>& input, double* output) {
        *output = Checked(input).Price();
    }
    void EarlyExercisePdeRiskResult_Get_RiskValue(const Handle_<StorableEarlyExercisePdeRiskResult_>& input, const String_& id, double* output) {
        Name(id, "risk_id");
        *output = Checked(input).RiskValue(id);
    }
    void EarlyExercisePdeRiskResult_Get_Summary(const Handle_<StorableEarlyExercisePdeRiskResult_>& input, Matrix_<Cell_>* output) {
        const auto& r = Checked(input);
        *output = Fields({{"SchemaVersion", ValueCell(r.SchemaVersion())},
                          {"Status", ValueCell(r.Status())},
                          {"PriceAvailable", ValueCell(r.PriceAvailable())},
                          {"RiskComplete", ValueCell(r.IsComplete())},
                          {"Price", r.PriceAvailable() ? ValueCell(r.Price()) : Cell_()},
                          {"PriceUnit", Cell_("price per unit underlying")},
                          {"Method", ValueCell(r.Method())},
                          {"RiskBasis", ValueCell(r.RiskBasis())},
                          {"AadUsed", ValueCell(r.AadUsed())},
                          {"PlanId", ValueCell(r.Plan().planId_)}});
    }
    void EarlyExercisePdeRiskResult_Get_Plan(const Handle_<StorableEarlyExercisePdeRiskResult_>& input, Matrix_<Cell_>* output) {
        const auto& value = Checked(input).Plan();
        *output = Fields({{"SchemaVersion", ValueCell(value.schemaVersion_)},
                          {"PlanId", ValueCell(value.planId_)},
                          {"EvaluationDate", ValueCell(value.evaluationDate_)},
                          {"MaturityDate", ValueCell(value.maturityDate_)},
                          {"Upper", ValueCell(value.upper_)},
                          {"GridPoints", ValueCell(value.gridPoints_)},
                          {"GridSpacing", ValueCell(value.gridSpacing_)},
                          {"Epsilon", ValueCell(value.epsilon_)},
                          {"Theta", ValueCell(value.theta_)},
                          {"EffectiveRannacherIntervals", ValueCell(value.effectiveRannacherIntervals_)},
                          {"PenaltyFactor", ValueCell(value.penaltyFactor_)},
                          {"MaxIterationsPerStep", ValueCell(value.maxIterationsPerStep_)},
                          {"LinearBackwardErrorLimit", ValueCell(value.linearBackwardErrorLimit_)},
                          {"InterpolationPolicy", ValueCell(value.interpolationPolicy_)},
                          {"SpotBumpOrigin", ValueCell(value.spotBumpOrigin_)},
                          {"VolatilityBumpOrigin", ValueCell(value.volatilityBumpOrigin_)},
                          {"PlannedScenarios", ValueCell(value.plannedScenarios_)},
                          {"BasePlannedSteps", ValueCell(value.basePlannedSteps_)},
                          {"PlannedSteps", ValueCell(value.plannedSteps_)},
                          {"PlannedNumericPayloadBytes", ValueCell(value.plannedNumericPayloadBytes_)},
                          {"PlannedWorkspaceBytes", ValueCell(value.plannedWorkspaceBytes_)},
                          {"PlannedPeakNativeBytes", ValueCell(value.plannedPeakNativeBytes_)}});
    }
    void EarlyExercisePdeRiskResult_Get_Execution(const Handle_<StorableEarlyExercisePdeRiskResult_>& input, Matrix_<Cell_>* output) {
        const auto& value = Checked(input).Execution();
        *output = Fields({{"PlannedScenarios", ValueCell(value.plannedScenarios_)},
                          {"AttemptedScenarios", ValueCell(value.attemptedScenarios_)},
                          {"SuccessfulScenarios", ValueCell(value.successfulScenarios_)},
                          {"FailedScenarios", ValueCell(value.failedScenarios_)},
                          {"NotRunScenarios", ValueCell(value.notRunScenarios_)},
                          {"TotalSolves", ValueCell(value.totalSolves_)},
                          {"WorkUnits", ValueCell(value.workUnits_)},
                          {"PlannedNumericPayloadBytes", ValueCell(value.plannedNumericPayloadBytes_)},
                          {"PlannedWorkspaceBytes", ValueCell(value.plannedWorkspaceBytes_)},
                          {"PlannedPeakNativeBytes", ValueCell(value.plannedPeakNativeBytes_)},
                          {"NumericPayloadBytes", ValueCell(value.numericPayloadBytes_)},
                          {"PeakWorkspaceBytes", ValueCell(value.peakWorkspaceBytes_)},
                          {"PeakNativeBytes", ValueCell(value.peakNativeBytes_)},
                          {"WorkLimitScenarioIndex", ValueCell(value.workLimitScenarioIndex_)},
                          {"WorkLimitSource", ValueCell(value.workLimitSource_)}});
    }
    void EarlyExercisePdeRiskResult_Get_PlanGrid(const Handle_<StorableEarlyExercisePdeRiskResult_>& input, Matrix_<Cell_>* output) {
        const auto& values = Checked(input).Plan().grid_;
        auto table = Table(values.size(), {"NodeIndex", "Spot"});
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& value = values[i];
            Row(&table, static_cast<int>(i) + 1, {ValueCell(i), ValueCell(value)});
        }
        *output = std::move(table);
    }
    void EarlyExercisePdeRiskResult_Get_PlanTimeGrid(const Handle_<StorableEarlyExercisePdeRiskResult_>& input, Matrix_<Cell_>* output) {
        const auto& values = Checked(input).Plan().timeGrid_;
        auto table = Table(values.size(), {"TimeIndex", "TimeYears", "CalendarDate", "IsExerciseDate", "IsCarryKnot", "IsMaturity"});
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& value = values[i];
            Row(&table, static_cast<int>(i) + 1,
                {ValueCell(i), ValueCell(value.timeYears_), ValueCell(value.calendarDate_), ValueCell(value.isExerciseDate_),
                 ValueCell(value.isCarryKnot_), ValueCell(value.isMaturity_)});
        }
        *output = std::move(table);
    }
    void EarlyExercisePdeRiskResult_Get_PlanSteps(const Handle_<StorableEarlyExercisePdeRiskResult_>& input, Matrix_<Cell_>* output) {
        const auto& values = Checked(input).Plan().steps_;
        auto table = Table(values.size(), {"SourceTimeIndex", "TimeIndex", "Dt", "Theta", "CarryIndex", "Damped", "Projection"});
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& value = values[i];
            Row(&table, static_cast<int>(i) + 1,
                {ValueCell(value.sourceTimeIndex_), ValueCell(value.timeIndex_), ValueCell(value.dt_), ValueCell(value.theta_),
                 ValueCell(value.carryIndex_), ValueCell(value.damped_), ValueCell(value.projection_)});
        }
        *output = std::move(table);
    }
    void EarlyExercisePdeRiskResult_Get_Risks(const Handle_<StorableEarlyExercisePdeRiskResult_>& input, Matrix_<Cell_>* output) {
        const auto& values = Checked(input).Risks();
        auto table = Table(
            values.size(),
            {"RiskIndex",         "RiskId",     "ParameterId", "DerivativeOrder", "SegmentIndex",   "StartDate",      "EndDate",
             "ParameterValue",    "Unit",       "Value",       "Status",          "Reason",         "Method",         "Interpretation",
             "FineBump",          "CoarseBump", "BumpOrigin",  "FineEstimate",    "CoarseEstimate", "StepDifference", "NormalizedStepDifference",
             "StabilityTolerance"});
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& value = values[i];
            Row(&table, static_cast<int>(i) + 1,
                {ValueCell(value.riskIndex_),
                 ValueCell(value.riskId_),
                 ValueCell(value.parameterId_),
                 ValueCell(value.derivativeOrder_),
                 ValueCell(value.segmentIndex_),
                 ValueCell(value.startDate_),
                 ValueCell(value.endDate_),
                 ValueCell(value.parameterValue_),
                 ValueCell(value.unit_),
                 ValueCell(value.value_),
                 ValueCell(value.status_),
                 ValueCell(value.reason_),
                 ValueCell(value.method_),
                 ValueCell(value.interpretation_),
                 ValueCell(value.fineBump_),
                 ValueCell(value.coarseBump_),
                 ValueCell(value.bumpOrigin_),
                 ValueCell(value.fineEstimate_),
                 ValueCell(value.coarseEstimate_),
                 ValueCell(value.stepDifference_),
                 ValueCell(value.normalizedStepDifference_),
                 ValueCell(value.stabilityTolerance_)});
        }
        *output = std::move(table);
    }
    void EarlyExercisePdeRiskResult_Get_Scenarios(const Handle_<StorableEarlyExercisePdeRiskResult_>& input, Matrix_<Cell_>* output) {
        const auto& values = Checked(input).Scenarios();
        auto table = Table(values.size(), {"ScenarioIndex",
                                           "ScenarioId",
                                           "PlanId",
                                           "ParameterId",
                                           "SegmentIndex",
                                           "ParameterValue",
                                           "Offset",
                                           "Status",
                                           "SolverStatus",
                                           "WorkLimitSource",
                                           "PriceAvailable",
                                           "Price",
                                           "Epsilon",
                                           "PlannedSteps",
                                           "ActualSteps",
                                           "CompletedSteps",
                                           "TotalSolves",
                                           "WorkUnits",
                                           "NumericPayloadBytes",
                                           "PeakWorkspaceBytes",
                                           "MaxIterationChange",
                                           "MaxLinearBackwardError",
                                           "MaxObstacleViolation",
                                           "MaxDualViolation",
                                           "MaxComplementarityResidual",
                                           "FailureStatus",
                                           "FailureTimeIndex",
                                           "FailureIteration",
                                           "FailureCalendarDate",
                                           "FailureNode",
                                           "FailureResidual",
                                           "FailureThreshold",
                                           "FailureWorkUnits",
                                           "FailureReason",
                                           "EvaluationMembership",
                                           "EvaluationBoundaryIntersectsSpotStencil",
                                           "FutureContactChanged",
                                           "UpwindChanged"});
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& value = values[i];
            Row(&table, static_cast<int>(i) + 1,
                {ValueCell(value.scenarioIndex_),
                 ValueCell(value.scenarioId_),
                 ValueCell(value.planId_),
                 ValueCell(value.parameterId_),
                 ValueCell(value.segmentIndex_),
                 ValueCell(value.parameterValue_),
                 ValueCell(value.offset_),
                 ValueCell(value.status_),
                 ValueCell(value.solverStatus_),
                 ValueCell(value.workLimitSource_),
                 ValueCell(value.priceAvailable_),
                 ValueCell(value.price_),
                 ValueCell(value.epsilon_),
                 ValueCell(value.plannedSteps_),
                 ValueCell(value.actualSteps_),
                 ValueCell(value.completedSteps_),
                 ValueCell(value.totalSolves_),
                 ValueCell(value.workUnits_),
                 ValueCell(value.numericPayloadBytes_),
                 ValueCell(value.peakWorkspaceBytes_),
                 ValueCell(value.maxIterationChange_),
                 ValueCell(value.maxLinearBackwardError_),
                 ValueCell(value.maxObstacleViolation_),
                 ValueCell(value.maxDualViolation_),
                 ValueCell(value.maxComplementarityResidual_),
                 value.failure_ ? ValueCell(value.failure_->status_) : Cell_(),
                 value.failure_ ? ValueCell(value.failure_->timeIndex_) : Cell_(),
                 value.failure_ ? ValueCell(value.failure_->iteration_) : Cell_(),
                 value.failure_ ? ValueCell(value.failure_->calendarDate_) : Cell_(),
                 value.failure_ ? ValueCell(value.failure_->node_) : Cell_(),
                 value.failure_ ? ValueCell(value.failure_->residual_) : Cell_(),
                 value.failure_ ? ValueCell(value.failure_->threshold_) : Cell_(),
                 value.failure_ ? ValueCell(value.failure_->workUnits_) : Cell_(),
                 value.failure_ ? ValueCell(value.failure_->reason_) : Cell_(),
                 ValueCell(value.evaluationMembership_),
                 ValueCell(value.evaluationBoundaryIntersectsSpotStencil_),
                 ValueCell(value.futureContactChanged_),
                 ValueCell(value.upwindChanged_)});
        }
        *output = std::move(table);
    }
    void EarlyExercisePdeRiskResult_Get_BumpOrigins(const Handle_<StorableEarlyExercisePdeRiskResult_>& input, Matrix_<Cell_>* output) {
        const auto& plan = Checked(input).Plan();
        const auto count = plan.rateBumpOrigins_.size();
        auto table = Table(2 + 2 * count, {"ParameterId", "Origin"});
        Row(&table, 1, {Cell_("Spot"), ValueCell(plan.spotBumpOrigin_)});
        Row(&table, 2, {Cell_("Volatility"), ValueCell(plan.volatilityBumpOrigin_)});
        for (size_t i = 0; i < count; ++i) {
            const auto index = String_(std::to_string(i));
            Row(&table, static_cast<int>(3 + 2 * i), {Cell_("Rate[" + index + "]"), ValueCell(plan.rateBumpOrigins_[i])});
            Row(&table, static_cast<int>(4 + 2 * i), {Cell_("DividendYield[" + index + "]"), ValueCell(plan.dividendYieldBumpOrigins_[i])});
        }
        *output = std::move(table);
    }
    void EarlyExercisePdeRiskResult_Get_Stencils(const Handle_<StorableEarlyExercisePdeRiskResult_>& input, Matrix_<Cell_>* output) {
        const auto& risks = Checked(input).Risks();
        size_t count = 0;
        for (const auto& risk : risks)
            count += risk.coarseStencil_.size() + risk.fineStencil_.size();
        auto table = Table(count, {"RiskIndex", "RiskId", "Scale", "EntryIndex", "ScenarioIndex", "Weight"});
        int row = 1;
        for (const auto& risk : risks) {
            const std::array<std::pair<const char*, const Vector_<EarlyExercisePdeRiskStencilEntry_>*>, 2> scales = {
                {{"COARSE", &risk.coarseStencil_}, {"FINE", &risk.fineStencil_}}};
            for (const auto& scale : scales)
                for (size_t i = 0; i < scale.second->size(); ++i) {
                    const auto& entry = (*scale.second)[i];
                    Row(&table, row++,
                        {ValueCell(risk.riskIndex_), ValueCell(risk.riskId_), Cell_(scale.first), ValueCell(i), ValueCell(entry.scenarioIndex_),
                         ValueCell(entry.weight_)});
                }
        }
        *output = std::move(table);
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_EarlyExercisePdeRiskRequest_Get_Settings_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskRequest_Get_Valuation_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskRequest_New_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_Get_BaseResult_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_Get_BumpOrigins_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_Get_Execution_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_Get_PlanGrid_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_Get_PlanSteps_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_Get_PlanTimeGrid_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_Get_Plan_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_Get_Price_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_Get_Request_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_Get_ResolvedSettings_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_Get_RiskValue_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_Get_Risks_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_Get_Scenarios_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_Get_Stencils_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_Get_Summary_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskResult_New_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskSettings_Get_CarryBumps_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskSettings_Get_Configuration_public.inc>
#include <dal-excel/auto/MG_EarlyExercisePdeRiskSettings_New_public.inc>
#endif
} // namespace Dal
