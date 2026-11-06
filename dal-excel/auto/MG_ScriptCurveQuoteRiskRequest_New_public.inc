

extern "C" __declspec(dllexport) OPER_* xl_ScriptCurveQuoteRiskRequest_New
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptCurveQuoteRiskRequest_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings);
        const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get(); argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "ScriptCurveQuoteRiskRequest_New", "settings");
        argName = 0;
		Handle_<StorableScriptCurveQuoteRiskRequest_> value;
        ScriptCurveQuoteRiskRequest_New(settings, &value);
        Excel::Retval_ retval;
        retval.Load(_env, value);
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

struct XlRegister_ScriptCurveQuoteRiskRequest_New_
{
    XlRegister_ScriptCurveQuoteRiskRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Exact optional fields; quote_ids is a ScriptCurveQuoteIds handle");
        Excel::Register("Base", "xl_ScriptCurveQuoteRiskRequest_New", "SCRIPTCURVEQUOTERISKREQUEST.NEW", "Create a script curve risk request", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_ScriptCurveQuoteRiskRequest_New_ The_ScriptCurveQuoteRiskRequest_New_XlRegisterer;
