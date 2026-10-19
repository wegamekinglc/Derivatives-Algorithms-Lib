#include "__marketivsurface.hpp"

#include <set>

#include "__marketivinput.hpp"
#include "__platform.hpp"
#include "__riskrequestrows.hpp"

// clang-format off
/*IF--------------------------------------------------------------------------
public MarketIvFitSettings_New
    Market IV surface owning value
&inputs
name is string
    name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "MarketIvSettings_New");
+Excel::ValidateMarketIvRange(xl_settings, "settings", true);
+const Excel::ScriptSettingsInput_ normalized(xl_settings); xl_settings = normalized.Get();
&optional
settings is cell[][]+
    settings
&outputs
settingsHandle is handle StorableMarketIvFitSettings
    settingsHandle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvFitSettings_Get_Configuration
    Market IV surface copied diagnostics
&inputs
settings is handle StorableMarketIvFitSettings
    settings
&outputs
configuration is cell[][]
    configuration
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvEvaluationSettings_New
    Market IV surface owning value
&inputs
name is string
    name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "MarketIvSettings_New");
+Excel::ValidateMarketIvRange(xl_settings, "settings", true);
+const Excel::ScriptSettingsInput_ normalized(xl_settings); xl_settings = normalized.Get();
&optional
settings is cell[][]+
    settings
&outputs
settingsHandle is handle StorableMarketIvEvaluationSettings
    settingsHandle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvEvaluationSettings_Get_Configuration
    Market IV surface copied diagnostics
&inputs
settings is handle StorableMarketIvEvaluationSettings
    settings
&outputs
configuration is cell[][]
    configuration
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvDupireSettings_New
    Market IV surface owning value
&inputs
name is string
    name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "MarketIvSettings_New");
+Excel::ValidateMarketIvRange(xl_settings, "settings", true);
+const Excel::ScriptSettingsInput_ normalized(xl_settings); xl_settings = normalized.Get();
&optional
settings is cell[][]+
    settings
&outputs
settingsHandle is handle StorableMarketIvDupireSettings
    settingsHandle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvDupireSettings_Get_Configuration
    Market IV surface copied diagnostics
&inputs
settings is handle StorableMarketIvDupireSettings
    settings
&outputs
configuration is cell[][]
    configuration
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvRefitRiskSettings_New
    Market IV surface owning value
&inputs
name is string
    name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "MarketIvSettings_New");
+Excel::ValidateMarketIvRange(xl_settings, "settings", true);
+const Excel::ScriptSettingsInput_ normalized(xl_settings); xl_settings = normalized.Get();
&optional
settings is cell[][]+
    settings
&outputs
settingsHandle is handle StorableMarketIvRefitRiskSettings
    settingsHandle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvRefitRiskSettings_Get_Configuration
    Market IV surface copied diagnostics
&inputs
settings is handle StorableMarketIvRefitRiskSettings
    settings
&outputs
configuration is cell[][]
    configuration
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvMarket_New
    Market IV surface owning value
&inputs
name is string
    name
+Excel::ValidateMarketIvNumber(xl_evaluation_date, "evaluation_date");
+const Excel::ScriptSettingsInput_ normalized_evaluation_date(xl_evaluation_date); xl_evaluation_date = normalized_evaluation_date.Get();
+Excel::ValidateMarketIvNumber(xl_spot, "spot");
+const Excel::ScriptSettingsInput_ normalized_spot(xl_spot); xl_spot = normalized_spot.Get();
+Excel::ValidateMarketIvRange(xl_terms, "terms");
+const Excel::ScriptSettingsInput_ normalized_terms(xl_terms); xl_terms = normalized_terms.Get();
evaluation_date is number
    evaluation date
spot is number
    spot
terms is cell[][]+
    terms
&outputs
market is handle StorableMarketIvMarket
    market
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvFlatMarket_New
    Market IV surface owning value
&inputs
name is string
    name
+Excel::ValidateMarketIvNumber(xl_evaluation_date, "evaluation_date");
+const Excel::ScriptSettingsInput_ normalized_evaluation_date(xl_evaluation_date); xl_evaluation_date = normalized_evaluation_date.Get();
+Excel::ValidateMarketIvNumber(xl_spot, "spot");
+const Excel::ScriptSettingsInput_ normalized_spot(xl_spot); xl_spot = normalized_spot.Get();
+Excel::ValidateMarketIvNumber(xl_rate, "rate");
+const Excel::ScriptSettingsInput_ normalized_rate(xl_rate); xl_rate = normalized_rate.Get();
+Excel::ValidateMarketIvNumber(xl_dividend_yield, "dividend_yield");
+const Excel::ScriptSettingsInput_ normalized_dividend_yield(xl_dividend_yield); xl_dividend_yield = normalized_dividend_yield.Get();
+Excel::ValidateMarketIvRange(xl_expiries, "expiries");
+const Excel::ScriptSettingsInput_ normalized_expiries(xl_expiries); xl_expiries = normalized_expiries.Get();
evaluation_date is number
    evaluation date
spot is number
    spot
rate is number
    rate
dividend_yield is number
    dividend yield
expiries is cell[][]+
    expiries
&outputs
market is handle StorableMarketIvMarket
    market
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvFitRequest_New
    Market IV surface owning value
&inputs
name is string
    name
+Excel::ValidateMarketIvRange(xl_quotes, "quotes");
+const Excel::ScriptSettingsInput_ normalized_quotes(xl_quotes); xl_quotes = normalized_quotes.Get();
market is handle StorableMarketIvMarket
    market
quotes is cell[][]+
    quotes
&optional
settings is handle StorableMarketIvFitSettings
    settings
&outputs
request is handle StorableMarketIvFitRequest
    request
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvFitResult_New
    Market IV surface owning value
&inputs
name is string
    name
request is handle StorableMarketIvFitRequest
    request
&outputs
result is handle StorableMarketIvFitResult
    result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvQuery_New
    Market IV surface owning value
&inputs
name is string
    name
+Excel::ValidateMarketIvNumber(xl_strike, "strike");
+const Excel::ScriptSettingsInput_ normalized_strike(xl_strike); xl_strike = normalized_strike.Get();
+Excel::ValidateMarketIvNumber(xl_time, "time");
+const Excel::ScriptSettingsInput_ normalized_time(xl_time); xl_time = normalized_time.Get();
strike is number
    strike
time is number
    time
&outputs
query is handle StorableMarketIvQuery
    query
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvPointResult_New
    Market IV surface owning value
&inputs
name is string
    name
snapshot is handle StorableMarketIvCalibrationSnapshot
    snapshot
query is handle StorableMarketIvQuery
    query
&optional
settings is handle StorableMarketIvEvaluationSettings
    settings
&outputs
result is handle StorableMarketIvPointResult
    result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvDupireRequest_New
    Market IV surface owning value
&inputs
name is string
    name
+Excel::ValidateMarketIvRange(xl_spots, "spots");
+const Excel::ScriptSettingsInput_ normalized_spots(xl_spots); xl_spots = normalized_spots.Get();
+Excel::ValidateMarketIvRange(xl_times, "times");
+const Excel::ScriptSettingsInput_ normalized_times(xl_times); xl_times = normalized_times.Get();
spots is cell[][]+
    spots
times is cell[][]+
    times
&optional
settings is handle StorableMarketIvDupireSettings
    settings
&outputs
request is handle StorableMarketIvDupireRequest
    request
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvDupireResult_New
    Market IV surface owning value
&inputs
name is string
    name
snapshot is handle StorableMarketIvCalibrationSnapshot
    snapshot
request is handle StorableMarketIvDupireRequest
    request
&outputs
result is handle StorableMarketIvDupireResult
    result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvRefitRiskRequest_New
    Market IV surface owning value
&inputs
name is string
    name
+Excel::ValidateMarketIvRange(xl_quote_ids, "quote_ids");
+const Excel::ScriptSettingsInput_ normalized_quote_ids(xl_quote_ids); xl_quote_ids = normalized_quote_ids.Get();
+Excel::ValidateMarketIvRange(xl_outputs, "outputs");
+const Excel::ScriptSettingsInput_ normalized_outputs(xl_outputs); xl_outputs = normalized_outputs.Get();
+Excel::ValidateMarketIvRange(xl_steps, "steps");
+const Excel::ScriptSettingsInput_ normalized_steps(xl_steps); xl_steps = normalized_steps.Get();
quote_ids is cell[][]+
    quote ids
outputs is cell[][]+
    outputs
&optional
settings is handle StorableMarketIvRefitRiskSettings
    settings
steps is cell[][]+
    steps
&outputs
request is handle StorableMarketIvRefitRiskRequest
    request
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvRefitRiskResult_New
    Market IV surface owning value
&inputs
name is string
    name
snapshot is handle StorableMarketIvCalibrationSnapshot
    snapshot
request is handle StorableMarketIvRefitRiskRequest
    request
&outputs
result is handle StorableMarketIvRefitRiskResult
    result
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvFitResult_Get_Request
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvFitResult
    source
&outputs
value is handle StorableMarketIvFitRequest
    value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvFitResult_Get_Snapshot
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvFitResult
    source
&outputs
value is handle StorableMarketIvCalibrationSnapshot
    value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvCalibrationSnapshot_Get_Market
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvCalibrationSnapshot
    source
&outputs
value is handle StorableMarketIvMarket
    value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvDupireResult_Get_Surface
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvDupireResult
    source
&outputs
value is handle LocalVolSurfaceData
    value
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvMarket_Get_Terms
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvMarket
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvMarket_Get_Summary
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvMarket
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvFitResult_Get_Summary
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvFitResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvFitResult_Get_Quotes
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvFitResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvFitResult_Get_Parameters
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvFitResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvFitResult_Get_Constraints
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvFitResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvFitResult_Get_Iterations
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvFitResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvFitResult_Get_Execution
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvFitResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvFitResult_Get_Failure
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvFitResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvCalibrationSnapshot_Get_Terms
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvCalibrationSnapshot
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvCalibrationSnapshot_Get_Theta
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvCalibrationSnapshot
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvCalibrationSnapshot_Get_Certificate
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvCalibrationSnapshot
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvPointResult_Get_Values
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvPointResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvPointResult_Get_Derivatives
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvPointResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvPointResult_Get_Summary
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvPointResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvDupireResult_Get_Nodes
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvDupireResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvDupireResult_Get_Execution
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvDupireResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvDupireResult_Get_Failure
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvDupireResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvRefitRiskResult_Get_Axis
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvRefitRiskResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvRefitRiskResult_Get_Responses
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvRefitRiskResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvRefitRiskResult_Get_Execution
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvRefitRiskResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvRefitRiskResult_Get_Failures
    Market IV surface copied diagnostics
&inputs
source is handle StorableMarketIvRefitRiskResult
    source
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

// clang-format on

namespace Dal {
    namespace {
        using namespace Excel;
        bool AllBlank(const Matrix_<Cell_>& cells) {
            return std::all_of(cells.begin(), cells.end(), [](const auto& c) { return Cell::IsEmpty(c); });
        }
        double Number(const Cell_& cell, const String_& field) {
            const auto* value = std::get_if<double>(&cell.val_);
            REQUIRE(value && std::isfinite(*value), "InvalidMarketIvRequest: " + field + "; finite number required excluding date/bool/text");
            return *value;
        }
        Date_ DateNumber(double value, const String_& field) {
            REQUIRE(std::isfinite(value) && value == std::trunc(value) && value >= Date::ToExcel(Date::Minimum()) &&
                        value <= Date::ToExcel(Date::Maximum()),
                    "InvalidMarketIvRequest: " + field + "; integer valid Excel date serial required");
            return Date::FromExcel(static_cast<int>(value));
        }
        String_ TextInput(const Cell_& cell, const String_& field) {
            const auto value = TextValue(cell, field);
            REQUIRE(!value.empty() && value.find('\0') == String_::npos, "InvalidMarketIvRequest: " + field + "; nonempty text without NUL required");
            return value;
        }
        String_ Location(const char* field, int row, int col) {
            return String_(field) + " row=" + String_(std::to_string(row + 1)) + " column=" + String_(std::to_string(col + 1));
        }
        template <class S_> bool Setting(const String_& key, const Cell_& cell, const String_& context, S_* s) {
            for (const auto& f : MarketIvSettings::RealFields(*s))
                if (key == f.name_) {
                    s->*(f.member_) = Number(cell, context);
                    return true;
                }
            for (const auto& f : MarketIvSettings::CountFields(*s))
                if (key == f.name_) {
                    s->*(f.member_) = PayloadBudget(cell, context, f.name_);
                    return true;
                }
            for (const auto& f : MarketIvSettings::IntegerFields(*s))
                if (key == f.name_) {
                    const size_t value = PayloadBudget(cell, context, f.name_);
                    REQUIRE(value > 0 && value <= static_cast<size_t>(std::numeric_limits<int>::max()),
                            "InvalidMarketIvRequest: " + context + "; positive int required");
                    s->*(f.member_) = static_cast<int>(value);
                    return true;
                }
            return false;
        }
        void Validate(const MarketIvFitSettings_& s) { ValidateMarketIvFitSettings(s); }
        void Validate(const MarketIvEvaluationSettings_& s) { ValidateMarketIvEvaluationSettings(s); }
        void Validate(const MarketIvDupireSettings_& s) {
            Validate(s.evaluation_);
            REQUIRE(s.maxGridPoints_ > 0, "InvalidMarketIvRequest: max_grid_points must be positive");
        }
        void Validate(const MarketIvRefitRiskSettings_& s) {
            Validate(s.evaluation_);
            REQUIRE(s.maxSelectedQuotes_ > 0 && s.maxOutputs_ > 0, "InvalidMarketIvRequest: refit size limits must be positive");
        }
        template <class S_> S_ Settings(const Matrix_<Cell_>& cells) {
            S_ s;
            if (AllBlank(cells))
                return s;
            for (int r = 0; r < cells.Rows(); ++r)
                REQUIRE(cells.Cols() == 2 && !Cell::IsEmpty(cells(r, 0)),
                        "InvalidMarketIvRequest: settings row=" + String_(std::to_string(r + 1)) + "; key required in nonempty range");
            ReadRows(
                cells, "MarketIvSettings_New", "settings",
                [&](const auto& key, const auto& value, const auto&, const auto& context) {
                    if (Setting(key, value, context, &s))
                        return;
                    if constexpr (std::is_same_v<S_, MarketIvDupireSettings_> || std::is_same_v<S_, MarketIvRefitRiskSettings_>) {
                        if (Setting(key, value, context, &s.evaluation_))
                            return;
                    }
                    if constexpr (std::is_same_v<S_, MarketIvRefitRiskSettings_>) {
                        if (key == "max_total_model_row_evaluations") {
                            s.maxTotalModelRowEvaluations_ =
                                Cell::IsEmpty(value) ? std::nullopt
                                                     : std::optional<size_t>(PayloadBudget(value, context, "max_total_model_row_evaluations"));
                            return;
                        }
                    }
                    THROW("InvalidMarketIvRequest: " + context + "; unknown settings key=" + key);
                },
                true);
            Validate(s);
            return s;
        }
        template <class S_> Vector_<std::pair<String_, Cell_>> Configuration(const S_& s) {
            Vector_<std::pair<String_, Cell_>> result{Field("key", "value")};
            for (const auto& f : MarketIvSettings::RealFields(s))
                result.push_back(Field(f.name_, s.*(f.member_)));
            for (const auto& f : MarketIvSettings::CountFields(s))
                result.push_back(Field(f.name_, double(s.*(f.member_))));
            for (const auto& f : MarketIvSettings::IntegerFields(s))
                result.push_back(Field(f.name_, double(s.*(f.member_))));
            if constexpr (std::is_same_v<S_, MarketIvDupireSettings_> || std::is_same_v<S_, MarketIvRefitRiskSettings_>) {
                for (const auto& f : MarketIvSettings::RealFields(s.evaluation_))
                    result.push_back(Field(f.name_, s.evaluation_.*(f.member_)));
            }
            if constexpr (std::is_same_v<S_, MarketIvRefitRiskSettings_>)
                result.push_back(Field("max_total_model_row_evaluations",
                                       s.maxTotalModelRowEvaluations_ ? Cell_(double(*s.maxTotalModelRowEvaluations_)) : Cell_()));
            return result;
        }
        template <class T_> void Store(const String_& name, T_ value, Handle_<Excel::StorableRiskValue_<T_>>* output) {
            *output = Handle_<Excel::StorableRiskValue_<T_>>(new Excel::StorableRiskValue_<T_>(name, std::move(value)));
        }
        template <class T_> const T_& Value(const Handle_<Excel::StorableRiskValue_<T_>>& h) {
            REQUIRE(h, "InvalidMarketIvRequest: required handle is empty");
            return h->val_;
        }
        template <class T_> T_ Default(const Handle_<Excel::StorableRiskValue_<T_>>& h) { return h ? h->val_ : T_{}; }
        Vector_<> AxisNumbers(const Matrix_<Cell_>& cells, const char* field, bool empty = false) {
            if (empty && AllBlank(cells))
                return {};
            REQUIRE(cells.Rows() == 1 || cells.Cols() == 1, String_("InvalidMarketIvRequest: ") + field + "; row or column required");
            Vector_<> values;
            values.reserve(static_cast<size_t>(cells.Rows()) * cells.Cols());
            for (int r = 0; r < cells.Rows(); ++r)
                for (int c = 0; c < cells.Cols(); ++c)
                    values.push_back(Number(cells(r, c), Location(field, r, c)));
            return values;
        }
        Matrix_<Cell_> Table(std::initializer_list<const char*> headers, size_t count) {
            REQUIRE(count < 1048576 && headers.size() <= 16384, "MarketIvBudgetExceeded: Excel spill exceeds worksheet dimensions");
            Matrix_<Cell_> result(static_cast<int>(count + 1), static_cast<int>(headers.size()));
            int c = 0;
            for (const auto* header : headers)
                result(0, c++) = header;
            return result;
        }
        Matrix_<Cell_> Terms(const MarketIvMarket_& market) {
            auto table = Table({"expiry_serial", "time_years", "forward", "discount"}, market.Terms().size());
            for (size_t i = 0; i < market.Terms().size(); ++i) {
                const auto& t = market.Terms()[i];
                const int r = static_cast<int>(i + 1);
                table(r, 0) = double(Date::ToExcel(t.expiry_));
                table(r, 1) = (t.expiry_ - market.EvaluationDate()) / 365.;
                table(r, 2) = t.forward_;
                table(r, 3) = t.discount_;
            }
            return table;
        }
        Matrix_<Cell_> Execution(const MarketIvExecution_& e) {
            return FieldCells(
                {Field("key", "value"), Field("planned_numeric_payload_bytes", double(e.plannedNumericPayloadBytes_)),
                 Field("planned_workspace_bytes", double(e.plannedWorkspaceBytes_)), Field("numeric_payload_bytes", double(e.numericPayloadBytes_)),
                 Field("peak_workspace_bytes", double(e.peakWorkspaceBytes_)), Field("model_row_evaluations", double(e.modelRowEvaluations_)),
                 Field("quote_conversions", double(e.quoteConversions_)), Field("output_evaluations", double(e.outputEvaluations_)),
                 Field("quote_capacity", double(e.quoteCapacity_)), Field("iteration_capacity", double(e.iterationCapacity_)),
                 Field("jacobian_capacity", double(e.jacobianCapacity_))});
        }
        Matrix_<Cell_> Certificate(const MarketIvCertificate_& c) {
            return FieldCells({Field("key", "value"), Field("method", c.method_), Field("domain", c.domain_), Field("certified", c.certified_),
                               Field("rho_margin", c.rhoMargin_), Field("eta_margin", c.etaMargin_), Field("butterfly_bound", c.butterflyBound_),
                               Field("wing_slope_bound", c.wingSlopeBound_)});
        }
        Matrix_<Cell_> Point(const MarketIvPointResult_& p) {
            return FieldCells({Field("quantity_and_unit", "value"), Field("total_variance", OptionalCell(p.TotalVariance())),
                               Field("implied_vol_decimal", OptionalCell(p.ImpliedVol())), Field("call_price_premium", OptionalCell(p.CallPrice())),
                               Field("put_price_premium", OptionalCell(p.PutPrice())), Field("w_k", OptionalCell(p.VarianceK())),
                               Field("w_kk", OptionalCell(p.VarianceKK())), Field("w_t_fixed_k_per_year", OptionalCell(p.VarianceT())),
                               Field("w_t_fixed_K_per_year", OptionalCell(p.FixedKVarianceT())),
                               Field("call_k_premium_per_strike", OptionalCell(p.CallK())),
                               Field("call_kk_premium_per_strike_squared", OptionalCell(p.CallKK())),
                               Field("call_t_premium_per_year", OptionalCell(p.CallT())), Field("density_factor", OptionalCell(p.DensityFactor())),
                               Field("local_variance", OptionalCell(p.LocalVariance())), Field("local_vol_decimal", OptionalCell(p.LocalVol())),
                               Field("value_status", p.ValueStatus()), Field("derivative_status", p.DerivativeStatus()),
                               Field("dupire_status", p.DupireStatus()), Field("carry_derivative", p.CarryDerivative())});
        }
    } // namespace

    void MarketIvMarket_New(const String_& name, double date, double spot, const Matrix_<Cell_>& terms, Handle_<StorableMarketIvMarket_>* output) {
        REQUIRE(terms.Cols() == 3, "InvalidMarketIvRequest: terms; expected Expiry/Forward/Discount");
        Vector_<MarketIvTerm_> values;
        values.reserve(terms.Rows());
        for (int r = 0; r < terms.Rows(); ++r)
            values.push_back({DateNumber(Number(terms(r, 0), Location("terms", r, 0)), Location("terms", r, 0)),
                              Number(terms(r, 1), Location("terms", r, 1)), Number(terms(r, 2), Location("terms", r, 2))});
        Store(name, MarketIvMarket_(DateNumber(date, "evaluation_date"), spot, values), output);
    }
    void MarketIvFlatMarket_New(const String_& name,
                                double date,
                                double spot,
                                double rate,
                                double yield,
                                const Matrix_<Cell_>& expiries,
                                Handle_<StorableMarketIvMarket_>* output) {
        Vector_<Date_> dates;
        for (const double x : AxisNumbers(expiries, "expiries"))
            dates.push_back(DateNumber(x, "expiries"));
        Store(name, NewFlatMarketIvMarket(DateNumber(date, "evaluation_date"), spot, rate, yield, dates), output);
    }
    void MarketIvFitRequest_New(const String_& name,
                                const Handle_<StorableMarketIvMarket_>& market,
                                const Matrix_<Cell_>& quotes,
                                const Handle_<StorableMarketIvFitSettings_>& settings,
                                Handle_<StorableMarketIvFitRequest_>* output) {
        REQUIRE(quotes.Cols() == 9, "InvalidMarketIvRequest: quotes; expected nine columns");
        Vector_<MarketIvQuote_> values;
        values.reserve(quotes.Rows());
        for (int r = 0; r < quotes.Rows(); ++r) {
            auto number = [&](int c) { return Number(quotes(r, c), Location("quotes", r, c)); };
            auto text = [&](int c) { return TextInput(quotes(r, c), Location("quotes", r, c)); };
            const auto option = text(3), type = text(4);
            REQUIRE((option == "CALL" || option == "PUT") && (type == "PRICE" || type == "DECIMAL_IV"),
                    "InvalidMarketIvRequest: quotes; canonical CALL/PUT and PRICE/DECIMAL_IV required");
            values.push_back({text(0), DateNumber(number(1), Location("quotes", r, 1)), number(2), OptionType_(option), MarketIvQuoteType_(type),
                              number(5), Cell::IsEmpty(quotes(r, 6)) ? std::nullopt : std::optional<double>(number(6)),
                              Cell::IsEmpty(quotes(r, 7)) ? std::nullopt : std::optional<double>(number(7)),
                              Cell::IsEmpty(quotes(r, 8)) ? 1. : number(8)});
        }
        MarketIvFitRequest_ request{Value(market), values, Default(settings)};
        ValidateMarketIvFitRequest(request);
        Store(name, std::move(request), output);
    }
    void
    MarketIvFitResult_New(const String_& name, const Handle_<StorableMarketIvFitRequest_>& request, Handle_<StorableMarketIvFitResult_>* output) {
        Store(name, FitMarketIvSurface(Value(request)), output);
    }
    void MarketIvQuery_New(const String_& name, double strike, double time, Handle_<StorableMarketIvQuery_>* output) {
        REQUIRE(std::isfinite(strike) && strike > 0. && std::isfinite(time) && time > 0.,
                "InvalidMarketIvRequest: query requires positive finite strike/time");
        Store(name, MarketIvQuery_{strike, time}, output);
    }
    void MarketIvPointResult_New(const String_& name,
                                 const Handle_<StorableMarketIvCalibrationSnapshot_>& snapshot,
                                 const Handle_<StorableMarketIvQuery_>& query,
                                 const Handle_<StorableMarketIvEvaluationSettings_>& settings,
                                 Handle_<StorableMarketIvPointResult_>* output) {
        Store(name, EvaluateMarketIvSurface(Value(snapshot), Value(query), Default(settings)), output);
    }
    void MarketIvDupireRequest_New(const String_& name,
                                   const Matrix_<Cell_>& spots,
                                   const Matrix_<Cell_>& times,
                                   const Handle_<StorableMarketIvDupireSettings_>& settings,
                                   Handle_<StorableMarketIvDupireRequest_>* output) {
        MarketIvDupireRequest_ request{AxisNumbers(spots, "spots"), AxisNumbers(times, "times"), Default(settings)};
        ValidateMarketIvDupireRequest(request);
        Store(name, std::move(request), output);
    }
    void MarketIvDupireResult_New(const String_& name,
                                  const Handle_<StorableMarketIvCalibrationSnapshot_>& snapshot,
                                  const Handle_<StorableMarketIvDupireRequest_>& request,
                                  Handle_<StorableMarketIvDupireResult_>* output) {
        Store(name, CalibrateMarketIvDupire(Value(snapshot), Value(request)), output);
    }
    void MarketIvRefitRiskRequest_New(const String_& name,
                                      const Matrix_<Cell_>& ids,
                                      const Matrix_<Cell_>& outputs,
                                      const Handle_<StorableMarketIvRefitRiskSettings_>& settings,
                                      const Matrix_<Cell_>& steps,
                                      Handle_<StorableMarketIvRefitRiskRequest_>* output) {
        REQUIRE(ids.Rows() == 1 || ids.Cols() == 1, "InvalidMarketIvRequest: quote_ids requires row or column");
        REQUIRE(outputs.Cols() == 3, "InvalidMarketIvRequest: outputs requires Strike/Time/Quantity");
        MarketIvRefitRiskRequest_ request;
        for (int r = 0; r < ids.Rows(); ++r)
            for (int c = 0; c < ids.Cols(); ++c)
                request.quoteIds_.push_back(TextInput(ids(r, c), Location("quote_ids", r, c)));
        for (int r = 0; r < outputs.Rows(); ++r) {
            const auto quantity = TextInput(outputs(r, 2), Location("outputs", r, 2));
            REQUIRE(quantity == "TOTAL_VARIANCE" || quantity == "IMPLIED_VOL" || quantity == "CALL_PRICE" || quantity == "PUT_PRICE" ||
                        quantity == "LOCAL_VOL",
                    "InvalidMarketIvRequest: outputs; unknown quantity");
            request.outputs_.push_back(
                {{Number(outputs(r, 0), Location("outputs", r, 0)), Number(outputs(r, 1), Location("outputs", r, 1))}, MarketIvQuantity_(quantity)});
        }
        request.settings_ = Default(settings);
        request.steps_ = AxisNumbers(steps, "steps", true);
        ValidateMarketIvRefitRiskRequest(request);
        Store(name, std::move(request), output);
    }
    void MarketIvRefitRiskResult_New(const String_& name,
                                     const Handle_<StorableMarketIvCalibrationSnapshot_>& snapshot,
                                     const Handle_<StorableMarketIvRefitRiskRequest_>& request,
                                     Handle_<StorableMarketIvRefitRiskResult_>* output) {
        Store(name, EvaluateMarketIvRefitRisk(Value(snapshot), Value(request)), output);
    }
    void MarketIvFitSettings_New(const String_& name, const Matrix_<Cell_>& cells, Handle_<StorableMarketIvFitSettings_>* output) {
        Store(name, Settings<MarketIvFitSettings_>(cells), output);
    }
    void MarketIvFitSettings_Get_Configuration(const Handle_<StorableMarketIvFitSettings_>& source, Matrix_<Cell_>* table) {
        *table = Excel::FieldCells(Configuration(Value(source)));
    }
    void MarketIvEvaluationSettings_New(const String_& name, const Matrix_<Cell_>& cells, Handle_<StorableMarketIvEvaluationSettings_>* output) {
        Store(name, Settings<MarketIvEvaluationSettings_>(cells), output);
    }
    void MarketIvEvaluationSettings_Get_Configuration(const Handle_<StorableMarketIvEvaluationSettings_>& source, Matrix_<Cell_>* table) {
        *table = Excel::FieldCells(Configuration(Value(source)));
    }
    void MarketIvDupireSettings_New(const String_& name, const Matrix_<Cell_>& cells, Handle_<StorableMarketIvDupireSettings_>* output) {
        Store(name, Settings<MarketIvDupireSettings_>(cells), output);
    }
    void MarketIvDupireSettings_Get_Configuration(const Handle_<StorableMarketIvDupireSettings_>& source, Matrix_<Cell_>* table) {
        *table = Excel::FieldCells(Configuration(Value(source)));
    }
    void MarketIvRefitRiskSettings_New(const String_& name, const Matrix_<Cell_>& cells, Handle_<StorableMarketIvRefitRiskSettings_>* output) {
        Store(name, Settings<MarketIvRefitRiskSettings_>(cells), output);
    }
    void MarketIvRefitRiskSettings_Get_Configuration(const Handle_<StorableMarketIvRefitRiskSettings_>& source, Matrix_<Cell_>* table) {
        *table = Excel::FieldCells(Configuration(Value(source)));
    }

    void MarketIvFitResult_Get_Request(const Handle_<StorableMarketIvFitResult_>& source, Handle_<StorableMarketIvFitRequest_>* output) {
        Store("", Value(source).Request(), output);
    }
    void MarketIvFitResult_Get_Snapshot(const Handle_<StorableMarketIvFitResult_>& source, Handle_<StorableMarketIvCalibrationSnapshot_>* output) {
        Store("", Value(source).Snapshot(), output);
    }
    void MarketIvCalibrationSnapshot_Get_Market(const Handle_<StorableMarketIvCalibrationSnapshot_>& source,
                                                Handle_<StorableMarketIvMarket_>* output) {
        Store("", Value(source).Request().market_, output);
    }
    void MarketIvDupireResult_Get_Surface(const Handle_<StorableMarketIvDupireResult_>& source, Handle_<LocalVolSurfaceData_>* output) {
        const auto& s = Value(source).SurfaceData();
        *output = Handle_<LocalVolSurfaceData_>(new LocalVolSurfaceData_(s.Name(), s.spots_, s.times_, s.vols_));
    }
    void MarketIvMarket_Get_Terms(const Handle_<StorableMarketIvMarket_>& source, Matrix_<Cell_>* table) { *table = Terms(Value(source)); }
    void MarketIvMarket_Get_Summary(const Handle_<StorableMarketIvMarket_>& source, Matrix_<Cell_>* table) {
        const auto& m = Value(source);
        *table = Excel::FieldCells({Field("key", "value"), Excel::Field("evaluation_date_serial", double(Date::ToExcel(m.EvaluationDate()))),
                                    Excel::Field("spot", m.Spot()), Excel::Field("carry_mode", m.CarryMode())});
    }
    void MarketIvCalibrationSnapshot_Get_Terms(const Handle_<StorableMarketIvCalibrationSnapshot_>& source, Matrix_<Cell_>* table) {
        *table = Terms(Value(source).Request().market_);
    }
    void MarketIvCalibrationSnapshot_Get_Theta(const Handle_<StorableMarketIvCalibrationSnapshot_>& source, Matrix_<Cell_>* table) {
        const auto& s = Value(source);
        *table = Table({"time_years", "total_variance", "slope_per_year", "segment_derivative_lower_bound", "limiter_branch"}, s.Times().size());
        for (size_t i = 0; i < s.Times().size(); ++i) {
            const int r = static_cast<int>(i + 1);
            (*table)(r, 0) = s.Times()[i];
            (*table)(r, 1) = s.Theta()[i];
            (*table)(r, 2) = s.Slopes()[i];
            if (i < s.Certificate().derivativeLowerBounds_.size())
                (*table)(r, 3) = s.Certificate().derivativeLowerBounds_[i];
            (*table)(r, 4) = double(s.Certificate().limiterBranches_[i]);
        }
    }
    void MarketIvCalibrationSnapshot_Get_Certificate(const Handle_<StorableMarketIvCalibrationSnapshot_>& source, Matrix_<Cell_>* table) {
        *table = Certificate(Value(source).Certificate());
    }
    void MarketIvFitResult_Get_Summary(const Handle_<StorableMarketIvFitResult_>& source, Matrix_<Cell_>* table) {
        const auto& f = Value(source);
        *table = Excel::FieldCells(
            {Field("key", "value"), Excel::Field("status", f.Status()), Excel::Field("method", f.Method()),
             Excel::Field("selected_start", Excel::OptionalCell(f.SelectedStart() >= 0 ? std::optional<int>(f.SelectedStart()) : std::nullopt)),
             Excel::Field("underidentified", f.Underidentified()), Excel::Field("near_tie", f.NearTie())});
    }
    void MarketIvFitResult_Get_Failure(const Handle_<StorableMarketIvFitResult_>& source, Matrix_<Cell_>* table) {
        const auto& f = Value(source);
        *table = Excel::FieldCells({Excel::Field("status", "failure"), Excel::Field(f.Status(), f.Failure())});
    }
    void MarketIvFitResult_Get_Execution(const Handle_<StorableMarketIvFitResult_>& source, Matrix_<Cell_>* table) {
        *table = Execution(Value(source).Execution());
    }
    void MarketIvFitResult_Get_Constraints(const Handle_<StorableMarketIvFitResult_>& source, Matrix_<Cell_>* table) {
        const auto& fit = Value(source);
        *table = fit.IsSuccess() ? Certificate(fit.Snapshot().Certificate())
                                 : Excel::FieldCells({Field("key", "value"), Excel::Field("status", fit.Status()),
                                                      Excel::Field("certified_surface_available", false)});
    }
    void MarketIvFitResult_Get_Parameters(const Handle_<StorableMarketIvFitResult_>& source, Matrix_<Cell_>* table) {
        const auto& f = Value(source);
        *table = Table({"start", "termination", "converged", "seed_projected", "certified", "quality_accepted", "rank", "condition_estimate",
                        "objective", "projected_gradient", "parameter_index", "parameter_value", "failure"},
                       [&]() {
                           size_t n = 0;
                           for (const auto& s : f.Starts())
                               n += std::max(size_t(1), s.parameters_.size());
                           return n;
                       }());
        int r = 1;
        for (size_t j = 0; j < f.Starts().size(); ++j) {
            const auto& s = f.Starts()[j];
            for (size_t k = 0; k < std::max(size_t(1), s.parameters_.size()); ++k, ++r) {
                (*table)(r, 0) = double(j);
                (*table)(r, 1) = s.termination_;
                (*table)(r, 2) = s.converged_;
                (*table)(r, 3) = s.projectedSeed_;
                (*table)(r, 4) = s.certified_;
                (*table)(r, 5) = s.qualityAccepted_;
                (*table)(r, 6) = Excel::OptionalCell(s.converged_ ? std::optional<int>(s.rank_) : std::nullopt);
                (*table)(r, 7) = Excel::OptionalCell(s.conditionEstimate_);
                (*table)(r, 8) = Excel::OptionalCell(s.objective_);
                (*table)(r, 9) = Excel::OptionalCell(s.projectedGradient_);
                (*table)(r, 12) = s.failure_;
                if (!s.parameters_.empty()) {
                    (*table)(r, 10) = double(k);
                    (*table)(r, 11) = s.parameters_[k];
                }
            }
        }
    }
    void MarketIvFitResult_Get_Quotes(const Handle_<StorableMarketIvFitResult_>& source, Matrix_<Cell_>* table) {
        const auto& f = Value(source);
        *table = Table({"id",
                        "expiry_serial",
                        "strike",
                        "option_type",
                        "quote_type",
                        "mid_native",
                        "bid_native",
                        "ask_native",
                        "weight",
                        "forward",
                        "discount",
                        "k",
                        "normalized_mid",
                        "normalized_bid",
                        "normalized_ask",
                        "scale",
                        "fitted_call_normalized",
                        "residual_normalized",
                        "fitted_native",
                        "residual_native",
                        "band_violation_normalized",
                        "within_band",
                        "parity_gap"},
                       f.Quotes().size());
        for (size_t i = 0; i < f.Quotes().size(); ++i) {
            const auto& q = f.Request().quotes_[i];
            const auto& d = f.Quotes()[i];
            const int r = static_cast<int>(i + 1);
            (*table)(r, 0) = q.id_;
            (*table)(r, 1) = double(Date::ToExcel(q.expiry_));
            (*table)(r, 2) = q.strike_;
            (*table)(r, 3) = q.optionType_.String();
            (*table)(r, 4) = q.quoteType_.String();
            (*table)(r, 5) = q.mid_;
            (*table)(r, 6) = Excel::OptionalCell(q.bid_);
            (*table)(r, 7) = Excel::OptionalCell(q.ask_);
            (*table)(r, 8) = q.weight_;
            (*table)(r, 9) = d.forward_;
            (*table)(r, 10) = d.discount_;
            (*table)(r, 11) = d.logMoneyness_;
            (*table)(r, 12) = d.normalizedMid_;
            (*table)(r, 13) = d.normalizedBid_;
            (*table)(r, 14) = d.normalizedAsk_;
            (*table)(r, 15) = d.scale_;
            (*table)(r, 16) = Excel::OptionalCell(d.fittedNormalizedCall_);
            (*table)(r, 17) = Excel::OptionalCell(d.normalizedResidual_);
            (*table)(r, 18) = Excel::OptionalCell(d.nativeFitted_);
            (*table)(r, 19) = Excel::OptionalCell(d.nativeResidual_);
            (*table)(r, 20) = Excel::OptionalCell(d.bandViolation_);
            (*table)(r, 21) = d.withinBand_;
            (*table)(r, 22) = Excel::OptionalCell(d.parityGap_);
        }
    }
    void MarketIvFitResult_Get_Iterations(const Handle_<StorableMarketIvFitResult_>& source, Matrix_<Cell_>* table) {
        const auto& values = Value(source).Iterations();
        *table = Table({"start", "iteration", "trials", "objective", "projected_gradient", "gradient_threshold", "damping"}, values.size());
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& v = values[i];
            const int r = static_cast<int>(i + 1);
            (*table)(r, 0) = double(v.start_);
            (*table)(r, 1) = double(v.iteration_);
            (*table)(r, 2) = double(v.trials_);
            (*table)(r, 3) = v.objective_;
            (*table)(r, 4) = v.projectedGradient_;
            (*table)(r, 5) = v.gradientThreshold_;
            (*table)(r, 6) = v.damping_;
        }
    }
    void MarketIvPointResult_Get_Values(const Handle_<StorableMarketIvPointResult_>& source, Matrix_<Cell_>* table) { *table = Point(Value(source)); }
    void MarketIvPointResult_Get_Derivatives(const Handle_<StorableMarketIvPointResult_>& source, Matrix_<Cell_>* table) {
        *table = Point(Value(source));
    }
    void MarketIvPointResult_Get_Summary(const Handle_<StorableMarketIvPointResult_>& source, Matrix_<Cell_>* table) {
        *table = Point(Value(source));
    }
    void MarketIvDupireResult_Get_Nodes(const Handle_<StorableMarketIvDupireResult_>& source, Matrix_<Cell_>* table) {
        const auto& f = Value(source);
        *table = Table({"spot", "time_years", "w", "w_t_fixed_k", "g", "local_variance", "local_vol_decimal", "value_status", "derivative_status",
                        "dupire_status"},
                       f.Nodes().size());
        for (size_t i = 0; i < f.Nodes().size(); ++i) {
            const auto& p = f.Nodes()[i];
            const int r = static_cast<int>(i + 1);
            (*table)(r, 0) = f.Request().spots_[i / f.Request().times_.size()];
            (*table)(r, 1) = f.Request().times_[i % f.Request().times_.size()];
            (*table)(r, 2) = Excel::OptionalCell(p.TotalVariance());
            (*table)(r, 3) = Excel::OptionalCell(p.VarianceT());
            (*table)(r, 4) = Excel::OptionalCell(p.DensityFactor());
            (*table)(r, 5) = Excel::OptionalCell(p.LocalVariance());
            (*table)(r, 6) = Excel::OptionalCell(p.LocalVol());
            (*table)(r, 7) = p.ValueStatus();
            (*table)(r, 8) = p.DerivativeStatus();
            (*table)(r, 9) = p.DupireStatus();
        }
    }
    void MarketIvDupireResult_Get_Execution(const Handle_<StorableMarketIvDupireResult_>& source, Matrix_<Cell_>* table) {
        *table = Execution(Value(source).Execution());
    }
    void MarketIvDupireResult_Get_Failure(const Handle_<StorableMarketIvDupireResult_>& source, Matrix_<Cell_>* table) {
        *table = Excel::FieldCells({Field("key", "value"), Excel::Field("failure", Value(source).Failure())});
    }
    void MarketIvRefitRiskResult_Get_Execution(const Handle_<StorableMarketIvRefitRiskResult_>& source, Matrix_<Cell_>* table) {
        *table = Execution(Value(source).Execution());
    }
    void MarketIvRefitRiskResult_Get_Axis(const Handle_<StorableMarketIvRefitRiskResult_>& source, Matrix_<Cell_>* table) {
        const auto& f = Value(source);
        *table = Table({"quote_id", "quote_index", "step_native", "input_unit", "plus_status", "minus_status", "regime_changed", "plus_start",
                        "minus_start", "plus_rank", "minus_rank"},
                       f.Axis().size());
        for (size_t i = 0; i < f.Axis().size(); ++i) {
            const auto& a = f.Axis()[i];
            const int r = static_cast<int>(i + 1);
            (*table)(r, 0) = f.Snapshot().Request().quotes_[a.quoteIndex_].id_;
            (*table)(r, 1) = double(a.quoteIndex_);
            (*table)(r, 2) = a.step_;
            (*table)(r, 3) = a.inputUnit_;
            (*table)(r, 4) = a.plusStatus_;
            (*table)(r, 5) = a.minusStatus_;
            (*table)(r, 6) = a.regimeChanged_;
            (*table)(r, 7) = Excel::OptionalCell(a.plusStart_ >= 0 ? std::optional<int>(a.plusStart_) : std::nullopt);
            (*table)(r, 8) = Excel::OptionalCell(a.minusStart_ >= 0 ? std::optional<int>(a.minusStart_) : std::nullopt);
            (*table)(r, 9) = Excel::OptionalCell(a.plusStart_ >= 0 ? std::optional<int>(a.plusRank_) : std::nullopt);
            (*table)(r, 10) = Excel::OptionalCell(a.minusStart_ >= 0 ? std::optional<int>(a.minusRank_) : std::nullopt);
        }
    }
    void MarketIvRefitRiskResult_Get_Responses(const Handle_<StorableMarketIvRefitRiskResult_>& source, Matrix_<Cell_>* table) {
        const auto& f = Value(source);
        *table = Table({"quote_index", "output_index", "strike", "time_years", "quantity", "output_unit", "base", "plus", "minus",
                        "symmetric_bump_slope", "plus_slope", "minus_slope", "status", "plus_output_status", "minus_output_status"},
                       f.Responses().size());
        for (size_t i = 0; i < f.Responses().size(); ++i) {
            const auto& v = f.Responses()[i];
            const auto& o = f.Request().outputs_[v.outputIndex_];
            const int r = static_cast<int>(i + 1);
            (*table)(r, 0) = double(v.quoteIndex_);
            (*table)(r, 1) = double(v.outputIndex_);
            (*table)(r, 2) = o.query_.strike_;
            (*table)(r, 3) = o.query_.time_;
            (*table)(r, 4) = o.quantity_.String();
            (*table)(r, 5) = o.quantity_ == MarketIvQuantity_::Value_::CALL_PRICE || o.quantity_ == MarketIvQuantity_::Value_::PUT_PRICE ? "premium"
                             : o.quantity_ == MarketIvQuantity_::Value_::TOTAL_VARIANCE ? "total_variance"
                                                                                        : "decimal_vol";
            (*table)(r, 6) = Excel::OptionalCell(v.base_);
            (*table)(r, 7) = Excel::OptionalCell(v.plus_);
            (*table)(r, 8) = Excel::OptionalCell(v.minus_);
            (*table)(r, 9) = Excel::OptionalCell(v.symmetricBumpSlope_);
            (*table)(r, 10) = Excel::OptionalCell(v.plusSlope_);
            (*table)(r, 11) = Excel::OptionalCell(v.minusSlope_);
            (*table)(r, 12) = v.Status();
            (*table)(r, 13) = v.PlusOutputStatus();
            (*table)(r, 14) = v.MinusOutputStatus();
        }
    }
    void MarketIvRefitRiskResult_Get_Failures(const Handle_<StorableMarketIvRefitRiskResult_>& source, Matrix_<Cell_>* table) {
        const auto& values = Value(source).Axis();
        *table = Table({"quote_index", "plus_status", "plus_failure", "minus_status", "minus_failure"}, values.size());
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& v = values[i];
            const int r = static_cast<int>(i + 1);
            (*table)(r, 0) = double(v.quoteIndex_);
            (*table)(r, 1) = v.plusStatus_;
            (*table)(r, 2) = v.plusFailure_;
            (*table)(r, 3) = v.minusStatus_;
            (*table)(r, 4) = v.minusFailure_;
        }
    }

} // namespace Dal

#ifdef _WIN32
namespace Dal {
#include <dal-excel/auto/MG_MarketIvCalibrationSnapshot_Get_Certificate_public.inc>
#include <dal-excel/auto/MG_MarketIvCalibrationSnapshot_Get_Market_public.inc>
#include <dal-excel/auto/MG_MarketIvCalibrationSnapshot_Get_Terms_public.inc>
#include <dal-excel/auto/MG_MarketIvCalibrationSnapshot_Get_Theta_public.inc>
#include <dal-excel/auto/MG_MarketIvDupireRequest_New_public.inc>
#include <dal-excel/auto/MG_MarketIvDupireResult_Get_Execution_public.inc>
#include <dal-excel/auto/MG_MarketIvDupireResult_Get_Failure_public.inc>
#include <dal-excel/auto/MG_MarketIvDupireResult_Get_Nodes_public.inc>
#include <dal-excel/auto/MG_MarketIvDupireResult_Get_Surface_public.inc>
#include <dal-excel/auto/MG_MarketIvDupireResult_New_public.inc>
#include <dal-excel/auto/MG_MarketIvDupireSettings_Get_Configuration_public.inc>
#include <dal-excel/auto/MG_MarketIvDupireSettings_New_public.inc>
#include <dal-excel/auto/MG_MarketIvEvaluationSettings_Get_Configuration_public.inc>
#include <dal-excel/auto/MG_MarketIvEvaluationSettings_New_public.inc>
#include <dal-excel/auto/MG_MarketIvFitRequest_New_public.inc>
#include <dal-excel/auto/MG_MarketIvFitResult_Get_Constraints_public.inc>
#include <dal-excel/auto/MG_MarketIvFitResult_Get_Execution_public.inc>
#include <dal-excel/auto/MG_MarketIvFitResult_Get_Failure_public.inc>
#include <dal-excel/auto/MG_MarketIvFitResult_Get_Iterations_public.inc>
#include <dal-excel/auto/MG_MarketIvFitResult_Get_Parameters_public.inc>
#include <dal-excel/auto/MG_MarketIvFitResult_Get_Quotes_public.inc>
#include <dal-excel/auto/MG_MarketIvFitResult_Get_Request_public.inc>
#include <dal-excel/auto/MG_MarketIvFitResult_Get_Snapshot_public.inc>
#include <dal-excel/auto/MG_MarketIvFitResult_Get_Summary_public.inc>
#include <dal-excel/auto/MG_MarketIvFitResult_New_public.inc>
#include <dal-excel/auto/MG_MarketIvFitSettings_Get_Configuration_public.inc>
#include <dal-excel/auto/MG_MarketIvFitSettings_New_public.inc>
#include <dal-excel/auto/MG_MarketIvFlatMarket_New_public.inc>
#include <dal-excel/auto/MG_MarketIvMarket_Get_Summary_public.inc>
#include <dal-excel/auto/MG_MarketIvMarket_Get_Terms_public.inc>
#include <dal-excel/auto/MG_MarketIvMarket_New_public.inc>
#include <dal-excel/auto/MG_MarketIvPointResult_Get_Derivatives_public.inc>
#include <dal-excel/auto/MG_MarketIvPointResult_Get_Summary_public.inc>
#include <dal-excel/auto/MG_MarketIvPointResult_Get_Values_public.inc>
#include <dal-excel/auto/MG_MarketIvPointResult_New_public.inc>
#include <dal-excel/auto/MG_MarketIvQuery_New_public.inc>
#include <dal-excel/auto/MG_MarketIvRefitRiskRequest_New_public.inc>
#include <dal-excel/auto/MG_MarketIvRefitRiskResult_Get_Axis_public.inc>
#include <dal-excel/auto/MG_MarketIvRefitRiskResult_Get_Execution_public.inc>
#include <dal-excel/auto/MG_MarketIvRefitRiskResult_Get_Failures_public.inc>
#include <dal-excel/auto/MG_MarketIvRefitRiskResult_Get_Responses_public.inc>
#include <dal-excel/auto/MG_MarketIvRefitRiskResult_New_public.inc>
#include <dal-excel/auto/MG_MarketIvRefitRiskSettings_Get_Configuration_public.inc>
#include <dal-excel/auto/MG_MarketIvRefitRiskSettings_New_public.inc>
} // namespace Dal
#endif
