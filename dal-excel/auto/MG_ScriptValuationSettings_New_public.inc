

extern "C" __declspec(dllexport) OPER_* xl_ScriptValuationSettings_New
    (const OPER_* xl_name, const OPER_* xl_settings, const OPER_* xl_fixings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptValuationSettings_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get(); argName = "settings (input #2)"; Excel::ValidateScriptSettingsRange(xl_settings, "ScriptValuationSettings_New", "settings"); xl_fixings = Excel::ScriptScalarInput(xl_fixings);
        argName = "settings (input #2)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true, true);
        argName = "fixings (input #3)";
        const Handle_<StorableMarketFixingSnapshot_> fixings = Excel::ToHandle<StorableMarketFixingSnapshot_>(_env, xl_fixings, true);
        argName = 0;
		Handle_<StorableScriptValuationSettings_> valuation;
        ScriptValuationSettings_New(name, settings, fixings, &valuation);
        Excel::Retval_ retval;
        retval.Load(_env, valuation);
        return retval.ToXloper();
    }
    catch (std::exception& e)
    {
        return Excel::Error(e.what(), argName);
    }
    catch (...)
    {
        return Excel::Error("Unknown error", argName);
    }
}

struct XlRegister_ScriptValuationSettings_New_
{
    XlRegister_ScriptValuationSettings_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Two columns: evaluation_date (integer serial), today_fixing (Model or RequireHistorical). Blank uses defaults.");
        argHelp.push_back("Snapshot handle; blank uses global history per call. Explicit empty snapshot never falls back.");
        Excel::Register("Base", "xl_ScriptValuationSettings_New", "SCRIPTVALUATIONSETTINGS.NEW", "Create immutable valuation settings; no date capture or preparation", "QQQQ", "name,[settings],[fixings]", argHelp, false);
    }
};
static XlRegister_ScriptValuationSettings_New_ The_ScriptValuationSettings_New_XlRegisterer;
