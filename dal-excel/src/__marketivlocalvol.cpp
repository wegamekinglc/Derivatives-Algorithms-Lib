#include "__marketivlocalvol.hpp"

#include <set>

#include "__marketivinput.hpp"
#include "__platform.hpp"
#include "__riskrequestrows.hpp"

// clang-format off
/*IF--------------------------------------------------------------------------
public MarketIvLocalVolModelSettings_New
    Same-source market IV local-volatility model
&inputs
name is string
    name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "InvalidMarketIvLocalVolRequest: /name");
+Excel::ValidateMarketIvLocalVolSettingsInput(xl_settings);
+const Excel::ScriptSettingsInput_ normalized(xl_settings); xl_settings = normalized.Get();
settings is cell[][]+
    settings
&outputs
settingsHandle is handle StorableMarketIvLocalVolModelSettings
    settingsHandle
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvLocalVolModelSettings_Get_Configuration
    Same-source market IV local-volatility model
&inputs
settings is handle StorableMarketIvLocalVolModelSettings
    settings
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvLocalVolModelData_New
    Same-source market IV local-volatility model
&inputs
name is string
    name
+argName = "name"; Excel::ValidateRiskRequestText(xl_name, "InvalidMarketIvLocalVolRequest: /name");
grid is handle StorableMarketIvDupireResult
    grid
settings is handle StorableMarketIvLocalVolModelSettings
    settings
&outputs
model is handle MarketIvLocalVolModelData
    model
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvLocalVolModelData_Get_Configuration
    Same-source market IV local-volatility model
&inputs
model is handle MarketIvLocalVolModelData
    model
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvLocalVolModelData_Get_Summary
    Same-source market IV local-volatility model
&inputs
model is handle MarketIvLocalVolModelData
    model
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvLocalVolModelData_Get_Carry
    Same-source market IV local-volatility model
&inputs
model is handle MarketIvLocalVolModelData
    model
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvLocalVolModelData_Get_Capabilities
    Same-source market IV local-volatility model
&inputs
model is handle MarketIvLocalVolModelData
    model
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvLocalVolModelData_Get_Warnings
    Same-source market IV local-volatility model
&inputs
model is handle MarketIvLocalVolModelData
    model
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvLocalVolModelData_Get_ConstructionExecution
    Same-source market IV local-volatility model
&inputs
model is handle MarketIvLocalVolModelData
    model
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MarketIvLocalVolModelData_Get_Execution
    Same-source market IV local-volatility model
&inputs
product is handle ScriptProductData
    product
model is handle MarketIvLocalVolModelData
    model
&optional
valuation is handle StorableScriptValuationSettings
    valuation
simulation is handle StorableMonteCarloSettings
    simulation
&outputs
table is cell[][]
    table
-IF-------------------------------------------------------------------------*/
// clang-format on

namespace Dal {
    namespace {
        [[noreturn]] void Invalid(const String_& field, const String_& expected, const String_& actual) {
            throw MarketIvLocalVolError_("InvalidMarketIvLocalVolRequest", "binding", field, expected, actual);
        }
        template <class T_> const T_& Required(const Handle_<T_>& h, const char* field) {
            if (!h)
                Invalid(field, "nonempty typed handle", "empty");
            return *h;
        }
        double Number(const Cell_& cell, const String_& field) {
            const auto* n = std::get_if<double>(&cell.val_);
            if (!n || !std::isfinite(*n))
                Invalid(field, "finite number excluding bool/text", "invalid cell");
            return *n;
        }
        String_ Text(const Cell_& cell, const String_& field) {
            if (!Cell::IsString(cell))
                Invalid(field, "nonempty text", "invalid cell");
            const auto text = std::get<String_>(cell.val_);
            if (text.empty() || text.find('\0') != String_::npos)
                Invalid(field, "nonempty text without NUL", "invalid text");
            return text;
        }
        template <class T_, class F_>
        bool SetField(const Vector_<MarketIvSettings::Field_<MarketIvLocalVolModelSettings_, T_>>& fields,
                      const String_& key,
                      const Cell_& cell,
                      const String_& location,
                      F_ convert,
                      MarketIvLocalVolModelSettings_* settings) {
            for (const auto& field : fields)
                if (key == field.name_) {
                    if (!Cell::IsEmpty(cell))
                        settings->*field.member_ = convert(cell, location);
                    return true;
                }
            return false;
        }
        bool RequiredKey(const String_& key) {
            static const std::set<String_> keys{"index", "currency", "factor", "horizon"};
            return keys.find(key) != keys.end();
        }
        void ApplySetting(const String_& key, const Cell_& value, const String_& field, MarketIvLocalVolModelSettings_* settings) {
            const auto count = [](const Cell_& c, const String_& f) { return Excel::PayloadBudget(c, "InvalidMarketIvLocalVolRequest: " + f, ""); };
            if (SetField(MarketIvLocalVolFields::TextFields(), key, value, field, Text, settings) ||
                SetField(MarketIvLocalVolFields::RealFields(), key, value, field, Number, settings) ||
                SetField(MarketIvLocalVolFields::CountFields(), key, value, field, count, settings))
                return;
            Invalid(field, "known settings key", "unknown key");
        }
        struct SettingsRows_ {
            MarketIvLocalVolModelSettings_ settings_;
            std::set<String_> seen_;
            bool trailing_ = false;
        };
        void ReadRow(const Matrix_<Cell_>& rows, int r, SettingsRows_* state) {
            const auto& keyCell = rows(r, 0);
            const auto& value = rows(r, 1);
            if (Cell::IsEmpty(keyCell) && Cell::IsEmpty(value)) {
                state->trailing_ = true;
                return;
            }
            const auto location = String_("/settings row=") + String_(std::to_string(r + 1));
            if (state->trailing_)
                Invalid(location, "empty rows only at end", "interior empty row");
            const auto key = Text(keyCell, location + " column=1");
            const auto field = String_("/settings/") + key + " row=" + String_(std::to_string(r + 1)) + " column=2";
            if (!state->seen_.insert(key).second)
                Invalid(field, "unique key", "duplicate");
            if (RequiredKey(key) && Cell::IsEmpty(value))
                Invalid(field, "required value", "empty");
            ApplySetting(key, value, field, &state->settings_);
        }
        MarketIvLocalVolModelSettings_ ReadSettings(const Matrix_<Cell_>& rows) {
            if (rows.Cols() != 2 || rows.Rows() == 0)
                Invalid("/settings", "nonempty two-column table", "empty or wrong columns");
            SettingsRows_ state;
            for (int r = 0; r < rows.Rows(); ++r) {
                ReadRow(rows, r, &state);
            }
            ValidateMarketIvLocalVolModelSettings(state.settings_);
            return state.settings_;
        }
        template <class T_> Cell_ ReportCell(const T_& x) { return Cell_(x); }
        Cell_ ReportCell(size_t x) {
            if (x > 9007199254740991ULL)
                Invalid("/report", "integer at most 2^53-1", String_(std::to_string(x)));
            return Cell_(static_cast<double>(x));
        }
        Cell_ ReportCell(const std::optional<Date_>& x) { return x ? Cell_(double(Date::ToExcel(*x))) : Cell_(); }
        template <class T_> Matrix_<Cell_> Fields(const T_& x) {
            Vector_<std::pair<String_, Cell_>> fields;
            MarketIvLocalVolFields::Visit(x, [&](const char* key, const auto& value) { fields.emplace_back(key, ReportCell(value)); });
            return Excel::FieldCells(fields);
        }
        template <class T_> Matrix_<Cell_> Rows(const Vector_<T_>& values) {
            Vector_<String_> headers;
            MarketIvLocalVolFields::Visit(T_{}, [&](const char* key, const auto&) { headers.push_back(key); });
            if (values.size() >= 1048576 || headers.size() > 16384)
                Invalid("/report", "Excel spill dimensions", "too many rows/columns");
            Matrix_<Cell_> table(static_cast<int>(values.size()) + 1, static_cast<int>(headers.size()));
            for (int c = 0; c < table.Cols(); ++c)
                table(0, c) = headers[c];
            for (size_t r = 0; r < values.size(); ++r) {
                int c = 0;
                MarketIvLocalVolFields::Visit(values[r],
                                              [&](const char*, const auto& value) { table(static_cast<int>(r) + 1, c++) = ReportCell(value); });
            }
            return table;
        }
    } // namespace
    void MarketIvLocalVolModelSettings_New(const String_& name, const Matrix_<Cell_>& rows, Handle_<StorableMarketIvLocalVolModelSettings_>* out) {
        const auto label = Text(Cell_(name), "/name");
        auto settings = ReadSettings(rows);
        *out = Handle_<StorableMarketIvLocalVolModelSettings_>(new StorableMarketIvLocalVolModelSettings_(label, std::move(settings)));
    }
    void MarketIvLocalVolModelSettings_Get_Configuration(const Handle_<StorableMarketIvLocalVolModelSettings_>& settings, Matrix_<Cell_>* out) {
        *out = Fields(Required(settings, "/settings").val_);
    }
    void MarketIvLocalVolModelData_New(const String_& name,
                                       const Handle_<StorableMarketIvDupireResult_>& grid,
                                       const Handle_<StorableMarketIvLocalVolModelSettings_>& settings,
                                       Handle_<MarketIvLocalVolModelData_>* out) {
        *out = NewMarketIvLocalVolModelData(name, Required(grid, "/grid").val_, Required(settings, "/settings").val_);
    }
    void MarketIvLocalVolModelData_Get_Configuration(const Handle_<MarketIvLocalVolModelData_>& model, Matrix_<Cell_>* out) {
        *out = Fields(Required(model, "/model").Configuration());
    }
    void MarketIvLocalVolModelData_Get_Summary(const Handle_<MarketIvLocalVolModelData_>& model, Matrix_<Cell_>* out) {
        *out = Fields(Required(model, "/model").Summary());
    }
    void MarketIvLocalVolModelData_Get_Carry(const Handle_<MarketIvLocalVolModelData_>& model, Matrix_<Cell_>* out) {
        *out = Rows(Required(model, "/model").CarryTerms());
    }
    void MarketIvLocalVolModelData_Get_Capabilities(const Handle_<MarketIvLocalVolModelData_>& model, Matrix_<Cell_>* out) {
        *out = Fields(Required(model, "/model").Capabilities());
    }
    void MarketIvLocalVolModelData_Get_Warnings(const Handle_<MarketIvLocalVolModelData_>& model, Matrix_<Cell_>* out) {
        *out = Rows(Required(model, "/model").Warnings());
    }
    void MarketIvLocalVolModelData_Get_ConstructionExecution(const Handle_<MarketIvLocalVolModelData_>& model, Matrix_<Cell_>* out) {
        *out = Fields(Required(model, "/model").ConstructionExecution());
    }
    void MarketIvLocalVolModelData_Get_Execution(const Handle_<ScriptProductData_>& product,
                                                 const Handle_<MarketIvLocalVolModelData_>& model,
                                                 const Handle_<StorableScriptValuationSettings_>& valuation,
                                                 const Handle_<StorableMonteCarloSettings_>& simulation,
                                                 Matrix_<Cell_>* out) {
        *out = Fields(InspectMarketIvLocalVolExecution(Required(product, "/product"), Required(model, "/model"),
                                                       valuation ? valuation->val_ : ScriptValuationSettings_{},
                                                       simulation ? simulation->val_ : MonteCarloSettings_{}));
    }
#ifdef _WIN32
    namespace Excel {
        void ValidateMarketIvLocalVolSettingsInput(const OPER_* input) {
            try {
                ValidateMarketIvRange(input, "settings", true);
            } catch (const Exception_& error) {
                throw MarketIvLocalVolError_("InvalidMarketIvLocalVolRequest", "binding", "/settings", "typed two-column settings range",
                                             error.what());
            }
        }
    } // namespace Excel
#include <dal-excel/auto/MG_MarketIvLocalVolModelData_Get_Capabilities_public.inc>
#include <dal-excel/auto/MG_MarketIvLocalVolModelData_Get_Carry_public.inc>
#include <dal-excel/auto/MG_MarketIvLocalVolModelData_Get_Configuration_public.inc>
#include <dal-excel/auto/MG_MarketIvLocalVolModelData_Get_ConstructionExecution_public.inc>
#include <dal-excel/auto/MG_MarketIvLocalVolModelData_Get_Execution_public.inc>
#include <dal-excel/auto/MG_MarketIvLocalVolModelData_Get_Summary_public.inc>
#include <dal-excel/auto/MG_MarketIvLocalVolModelData_Get_Warnings_public.inc>
#include <dal-excel/auto/MG_MarketIvLocalVolModelData_New_public.inc>
#include <dal-excel/auto/MG_MarketIvLocalVolModelSettings_Get_Configuration_public.inc>
#include <dal-excel/auto/MG_MarketIvLocalVolModelSettings_New_public.inc>
#endif
} // namespace Dal
