

extern "C" __declspec(dllexport) OPER_* xl_JacobianRiskRequest_New
    (const OPER_* xl_name, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JacobianRiskRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "JacobianRiskRequest_New; name"); const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get(); argName = "settings (input #2)"; Excel::ValidateRiskRequestSettings(xl_settings, "JacobianRiskRequest_New");
        argName = "settings (input #2)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true, true);
        argName = 0;
		Handle_<StorableJacobianRiskRequest_> request;
        JacobianRiskRequest_New(name, settings, &request);
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

struct XlRegister_JacobianRiskRequest_New_
{
    XlRegister_JacobianRiskRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Request name");
        argHelp.push_back("Two columns: inputs/outputs (semicolon IDs), report_factors (semicolon numbers), max_block_width, numeric_payload_budget_bytes, recording_capacity_budget_bytes, scratch_capacity_budget_bytes. Missing keys use defaults; blank ID lists explicitly select none. Width defaults to one.");
        Excel::Register("Base", "xl_JacobianRiskRequest_New", "JACOBIANRISKREQUEST.NEW", "Create an immutable budgeted script Jacobian request", "QQQ", "name,[settings]", argHelp, false);
    }
};
static XlRegister_JacobianRiskRequest_New_ The_JacobianRiskRequest_New_XlRegisterer;
