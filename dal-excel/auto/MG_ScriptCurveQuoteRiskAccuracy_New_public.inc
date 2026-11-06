

extern "C" __declspec(dllexport) OPER_* xl_ScriptCurveQuoteRiskAccuracy_New
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptCurveQuoteRiskAccuracy_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings);
        const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get(); argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "ScriptCurveQuoteRiskAccuracy_New", "settings");
        argName = 0;
		Handle_<StorableScriptCurveQuoteRiskAccuracy_> value;
        ScriptCurveQuoteRiskAccuracy_New(settings, &value);
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

struct XlRegister_ScriptCurveQuoteRiskAccuracy_New_
{
    XlRegister_ScriptCurveQuoteRiskAccuracy_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Exact field/value table; residual_absolute_limits is a CalibrationNumbers handle");
        Excel::Register("Base", "xl_ScriptCurveQuoteRiskAccuracy_New", "SCRIPTCURVEQUOTERISKACCURACY.NEW", "Create explicit script curve risk accuracy", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_ScriptCurveQuoteRiskAccuracy_New_ The_ScriptCurveQuoteRiskAccuracy_New_XlRegisterer;
