

extern "C" __declspec(dllexport) OPER_* xl_CalibrationRiskRequest_New
    (const OPER_* xl_name, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationRiskRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "CalibrationRiskRequest_New; name"); argName = "settings"; Excel::ValidateRiskRequestSettings(xl_settings, "CalibrationRiskRequest_New"); const Excel::ScriptSettingsInput_ normalized(xl_settings); xl_settings = normalized.Get();
        argName = "settings (input #2)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true, true);
        argName = 0;
		Handle_<StorableCalibrationRiskRequest_> request;
        CalibrationRiskRequest_New(name, settings, &request);
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

struct XlRegister_CalibrationRiskRequest_New_
{
    XlRegister_CalibrationRiskRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Two columns: inputs, report_factors, numeric_payload_budget_bytes; semicolon lists");
        Excel::Register("Base", "xl_CalibrationRiskRequest_New", "CALIBRATIONRISKREQUEST.NEW", "Create an owning quote request", "QQQ", "name,[settings]", argHelp, false);
    }
};
static XlRegister_CalibrationRiskRequest_New_ The_CalibrationRiskRequest_New_XlRegisterer;
