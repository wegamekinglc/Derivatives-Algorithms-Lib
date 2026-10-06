

extern "C" __declspec(dllexport) OPER_* xl_WeightedRiskRequest_New
    (const OPER_* xl_name, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("WeightedRiskRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "WeightedRiskRequest_New; name"); const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get(); argName = "settings (input #2)"; Excel::ValidateRiskRequestSettings(xl_settings, "WeightedRiskRequest_New");
        argName = "settings (input #2)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true, true);
        argName = 0;
		Handle_<StorableWeightedRiskRequest_> request;
        WeightedRiskRequest_New(name, settings, &request);
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

struct XlRegister_WeightedRiskRequest_New_
{
    XlRegister_WeightedRiskRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Request name");
        argHelp.push_back("Two columns: inputs/outputs (semicolon IDs), weights/report_factors (semicolon numbers), numeric_payload_budget_bytes. Missing key uses defaults; blank lists explicitly select none; weights may be signed or zero.");
        Excel::Register("Base", "xl_WeightedRiskRequest_New", "WEIGHTEDRISKREQUEST.NEW", "Create an immutable fixed-weight script risk request", "QQQ", "name,[settings]", argHelp, false);
    }
};
static XlRegister_WeightedRiskRequest_New_ The_WeightedRiskRequest_New_XlRegisterer;
