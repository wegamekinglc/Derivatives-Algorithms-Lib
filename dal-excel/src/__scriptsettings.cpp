//
// Created by Codex on 2026/9/15.
//

#include <cmath>
#include <limits>

#include <dal/script/settings.hpp>

#include "__curve_storable.hpp"
#include "__platform.hpp"
#include "__script_test_api.hpp"
#include "__scriptinput.hpp"

/*IF--------------------------------------------------------------------------
public ScriptProductSettings_New
    Create immutable script contract settings; no market reads
&inputs
name is string
    Object name
+const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get();
+argName = "settings (input #2)"; Excel::ValidateScriptSettingsRange(xl_settings, "ScriptProductSettings_New", "settings");
&optional
settings is cell[][]+
    Two columns key/value: default_index. Blank selects no default index.
&outputs
productSettings is handle StorableScriptProductSettings
    Immutable script product settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public ScriptValuationSettings_New
    Create immutable valuation settings; no date capture or preparation
&inputs
name is string
    Object name
+const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get();
+argName = "settings (input #2)"; Excel::ValidateScriptSettingsRange(xl_settings, "ScriptValuationSettings_New", "settings");
+xl_fixings = Excel::ScriptScalarInput(xl_fixings);
&optional
settings is cell[][]+
    Two columns: evaluation_date (integer serial), today_fixing (Model or RequireHistorical). Blank uses defaults.
fixings is handle StorableMarketFixingSnapshot
    Snapshot handle; blank uses global history per call. Explicit empty snapshot never falls back.
&outputs
valuation is handle StorableScriptValuationSettings
    Immutable valuation settings
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
public MonteCarloSettings_New
    Create immutable Monte Carlo execution settings
&inputs
name is string
    Object name
+const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get();
+argName = "settings (input #2)"; Excel::ValidateScriptSettingsRange(xl_settings, "MonteCarloSettings_New", "settings");
&optional
settings is cell[][]+
    Two columns: method, use_bb, enable_aad, smooth, compiled, lsmc_basis_degree. Booleans 0/1; smooth positive; lsmc_basis_degree integer 1..8.
&outputs
simulation is handle StorableMonteCarloSettings
    Immutable Monte Carlo settings
-IF-------------------------------------------------------------------------*/

namespace Dal {
    namespace {
        using Excel::ScriptSettingLocation;

        String_ TextValue(const Cell_& cell, const String_& context) {
            REQUIRE(Cell::IsString(cell), context + "expected non-empty string; received cell type=" + String_(std::to_string(cell.val_.index())));
            return std::get<String_>(cell.val_);
        }

        bool BooleanValue(const Cell_& cell, const String_& context) {
            if (Cell::IsBool(cell))
                return std::get<bool>(cell.val_);
            const auto* number = std::get_if<double>(&cell.val_);
            REQUIRE(number && (*number == 0.0 || *number == 1.0), context + "expected Excel boolean or numeric 0/1");
            return *number == 1.0;
        }

        Date_ EvaluationDate(const Cell_& cell, const String_& context) {
            const auto constraint = context + "expected valid Date or valid integral Excel date serial";
            if (Cell::IsDate(cell)) {
                const auto date = Cell::ToDate(cell);
                REQUIRE(date.IsValid(), constraint);
                return date;
            }
            const auto* number = std::get_if<double>(&cell.val_);
            REQUIRE(number && std::isfinite(*number) && std::trunc(*number) == *number && *number >= std::numeric_limits<int>::min() &&
                        *number <= std::numeric_limits<int>::max(),
                    constraint);
            const auto date = Date::FromExcel(static_cast<int>(*number));
            REQUIRE(date.IsValid(), constraint);
            return date;
        }

        String_ MethodValue(const Cell_& cell, const String_& context) {
            const auto method = TextValue(cell, context);
            try {
                Script::ValidateRNG(method);
            } catch (const Exception_& error) {
                THROW(context + String_(error.what()));
            }
            return method;
        }

        double SmoothingValue(const Cell_& cell, const String_& context) {
            const auto* number = std::get_if<double>(&cell.val_);
            REQUIRE(number && std::isfinite(*number) && *number > 0.0, context + "InvalidSmoothing: expected finite positive number");
            return *number;
        }

        int BasisDegreeValue(const Cell_& cell, const String_& context) {
            const auto* number = std::get_if<double>(&cell.val_);
            REQUIRE(number && std::isfinite(*number) && std::trunc(*number) == *number && *number >= 1.0 && *number <= 8.0,
                    context + "InvalidLsmcBasisDegree: expected integral number between 1 and 8");
            return static_cast<int>(*number);
        }

        bool IsDefaultSettingsInput(const Matrix_<Cell_>& input) {
            return (input.Rows() == 0 && input.Cols() == 0) || (input.Rows() == 1 && input.Cols() == 1 && Cell::IsEmpty(input(0, 0)));
        }

        template <class F_> void ReadRows(const Matrix_<Cell_>& input, const String_& function, const String_& argument, F_ applyRow) {
            if (IsDefaultSettingsInput(input))
                return;
            REQUIRE(input.Cols() == 2, ScriptSettingLocation(function, argument, 1, input.Cols() < 2 ? input.Cols() + 1 : 3) +
                                           "expected 2 columns; rows=" + String_(std::to_string(input.Rows())) +
                                           " cols=" + String_(std::to_string(input.Cols())));
            std::map<String_, int> seen;
            for (int row = 0; row < input.Rows(); ++row) {
                const auto& keyCell = input(row, 0);
                const auto& value = input(row, 1);
                if (Cell::IsEmpty(keyCell) && Cell::IsEmpty(value))
                    continue;
                const auto keyContext = ScriptSettingLocation(function, argument, row + 1, 1);
                REQUIRE(!Cell::IsEmpty(keyCell), keyContext + "expected non-empty key");
                const auto key = TextValue(keyCell, keyContext);
                const auto valueContext = ScriptSettingLocation(function, argument, row + 1, 2) + key + "; ";
                REQUIRE(!Cell::IsEmpty(value), valueContext + "expected non-empty value");
                const auto inserted = seen.emplace(key, row + 1);
                REQUIRE(inserted.second, keyContext + "duplicate key " + key + "; first row=" + String_(std::to_string(inserted.first->second)) +
                                             "; expected each key once");
                applyRow(key, value, keyContext, valueContext);
            }
        }
    } // namespace

    void ScriptProductSettings_New(const String_& name, const Matrix_<Cell_>& settings, Handle_<StorableScriptProductSettings_>* productSettings) {
        Script::ScriptProductSettings_ value;
        ReadRows(settings, "ScriptProductSettings_New", "settings",
                 [&](const String_& key, const Cell_& cell, const String_& keyContext, const String_& valueContext) {
                     REQUIRE(key == "default_index", keyContext + "unknown key " + key + "; expected default_index");
                     value.defaultIndex_ = TextValue(cell, valueContext);
                 });
        productSettings->reset(new StorableScriptProductSettings_(name, value));
    }

    void ScriptValuationSettings_New(const String_& name,
                                     const Matrix_<Cell_>& settings,
                                     const Handle_<StorableMarketFixingSnapshot_>& fixings,
                                     Handle_<StorableScriptValuationSettings_>* valuation) {
        Script::ScriptValuationSettings_ value;
        ReadRows(settings, "ScriptValuationSettings_New", "settings",
                 [&](const String_& key, const Cell_& cell, const String_& keyContext, const String_& valueContext) {
                     if (key == "evaluation_date") {
                         value.evaluationDate_ = EvaluationDate(cell, valueContext);
                     } else if (key == "today_fixing") {
                         const auto text = TextValue(cell, valueContext);
                         //  Deliberately case-sensitive: values must read Model/RequireHistorical exactly, unlike the case-insensitive keys
                         const std::string policy(text.data(), text.size());
                         REQUIRE(policy == "Model" || policy == "RequireHistorical",
                                 valueContext + "InvalidTodayFixingPolicy: expected Model or RequireHistorical; received " + text);
                         value.todayFixingPolicy_ =
                             policy == "Model" ? TodayFixingPolicy_::Value_::MODEL : TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
                     } else {
                         THROW(keyContext + "unknown key " + key + "; expected evaluation_date or today_fixing");
                     }
                 });
        if (fixings) {
            REQUIRE(fixings->val_, "InvalidSetting: ScriptValuationSettings_New; fixings; expected non-null native snapshot");
            value.fixings_ = fixings->val_;
        }
        valuation->reset(new StorableScriptValuationSettings_(name, value));
    }

    void MonteCarloSettings_New(const String_& name, const Matrix_<Cell_>& settings, Handle_<StorableMonteCarloSettings_>* simulation) {
        Script::MonteCarloSettings_ value;
        ReadRows(settings, "MonteCarloSettings_New", "settings",
                 [&](const String_& key, const Cell_& cell, const String_& keyContext, const String_& valueContext) {
                     if (key == "method") {
                         value.rsg_ = MethodValue(cell, valueContext);
                     } else if (key == "smooth") {
                         value.smooth_ = SmoothingValue(cell, valueContext);
                     } else if (key == "lsmc_basis_degree") {
                         value.lsmcBasisDegree_ = BasisDegreeValue(cell, valueContext);
                     } else {
                         REQUIRE(key == "use_bb" || key == "enable_aad" || key == "compiled",
                                 keyContext + "unknown key " + key + "; expected method, use_bb, enable_aad, smooth, compiled or lsmc_basis_degree");
                         const auto flag = BooleanValue(cell, valueContext);
                         if (key == "use_bb")
                             value.useBb_ = flag;
                         else if (key == "enable_aad")
                             value.enableAad_ = flag;
                         else
                             value.compiled_ = flag;
                     }
                 });
        simulation->reset(new StorableMonteCarloSettings_(name, value));
    }
#ifdef _WIN32
#include <dal-excel/auto/MG_MonteCarloSettings_New_public.inc>
#include <dal-excel/auto/MG_ScriptProductSettings_New_public.inc>
#include <dal-excel/auto/MG_ScriptValuationSettings_New_public.inc>
#endif
} // namespace Dal
