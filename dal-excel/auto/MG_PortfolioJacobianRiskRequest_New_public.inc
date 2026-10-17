

extern "C" __declspec(dllexport) OPER_* xl_PortfolioJacobianRiskRequest_New
    (const OPER_* xl_name, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PortfolioJacobianRiskRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "PortfolioJacobianRiskRequest_New; name"); const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get(); argName = "settings"; Excel::ValidateRiskRequestSettings(xl_settings, "PortfolioJacobianRiskRequest_New");
        argName = "settings (input #2)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true);
        argName = 0;
		Handle_<StorablePortfolioJacobianRiskRequest_> request;
        PortfolioJacobianRiskRequest_New(name, settings, &request);
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

struct XlRegister_PortfolioJacobianRiskRequest_New_
{
    XlRegister_PortfolioJacobianRiskRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Two columns: inputs, outputs, report_factors, max_block_width and three byte budgets");
        Excel::Register("Base", "xl_PortfolioJacobianRiskRequest_New", "PORTFOLIOJACOBIANRISKREQUEST.NEW", "Construct an immutable global portfolio attribution request", "QQQ", "name,[settings]", argHelp, false);
    }
};
static XlRegister_PortfolioJacobianRiskRequest_New_ The_PortfolioJacobianRiskRequest_New_XlRegisterer;
