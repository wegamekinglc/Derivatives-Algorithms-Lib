

extern "C" __declspec(dllexport) OPER_* xl_RiskRequest_New
    (const OPER_* xl_name, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RiskRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get(); argName = "settings (input #2)"; Excel::ValidateScriptSettingsRange(xl_settings, "RiskRequest_New", "settings");
        argName = "settings (input #2)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true, true);
        argName = 0;
		Handle_<StorableRiskRequest_> request;
        RiskRequest_New(name, settings, &request);
        Excel::Retval_ retval;
        retval.Load(_env, request);
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

struct XlRegister_RiskRequest_New_
{
    XlRegister_RiskRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Request name");
        argHelp.push_back("Two columns: inputs/outputs (semicolon IDs), report_factors (semicolon numbers), numeric_payload_budget_bytes. Missing key uses defaults; blank inputs selects none.");
        Excel::Register("Base", "xl_RiskRequest_New", "RISKREQUEST.NEW", "Create an immutable scalar risk request", "QQQ", "name,[settings]", argHelp, false);
    }
};
static XlRegister_RiskRequest_New_ The_RiskRequest_New_XlRegisterer;
