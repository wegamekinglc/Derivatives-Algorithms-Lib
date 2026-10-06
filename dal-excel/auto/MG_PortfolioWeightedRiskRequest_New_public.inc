

extern "C" __declspec(dllexport) OPER_* xl_PortfolioWeightedRiskRequest_New
    (const OPER_* xl_name, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PortfolioWeightedRiskRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "PortfolioWeightedRiskRequest_New; name"); const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get(); argName = "settings"; Excel::ValidateRiskRequestSettings(xl_settings, "PortfolioWeightedRiskRequest_New");
        argName = "settings (input #2)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true);
        argName = 0;
		Handle_<StorablePortfolioWeightedRiskRequest_> request;
        PortfolioWeightedRiskRequest_New(name, settings, &request);
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

struct XlRegister_PortfolioWeightedRiskRequest_New_
{
    XlRegister_PortfolioWeightedRiskRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Two columns: inputs, outputs, weights, report_factors and three byte budgets");
        Excel::Register("Base", "xl_PortfolioWeightedRiskRequest_New", "PORTFOLIOWEIGHTEDRISKREQUEST.NEW", "Construct an immutable global portfolio weighted-risk request", "QQQ", "name,[settings]", argHelp, false);
    }
};
static XlRegister_PortfolioWeightedRiskRequest_New_ The_PortfolioWeightedRiskRequest_New_XlRegisterer;
