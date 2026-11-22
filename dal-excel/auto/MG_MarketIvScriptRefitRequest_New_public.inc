

extern "C" __declspec(dllexport) OPER_* xl_MarketIvScriptRefitRequest_New
    (const OPER_* xl_name, const OPER_* xl_snapshot, const OPER_* xl_grid, const OPER_* xl_model, const OPER_* xl_product, const OPER_* xl_quote_ids, const OPER_* xl_steps, const OPER_* xl_valuation, const OPER_* xl_simulation, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvScriptRefitRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "InvalidMarketIvLocalVolRequest: /name"); Excel::ValidateMarketIvRange(xl_quote_ids, "quote_ids"); const Excel::ScriptSettingsInput_ normalized_ids(xl_quote_ids); xl_quote_ids = normalized_ids.Get(); Excel::ValidateMarketIvRange(xl_steps, "steps"); const Excel::ScriptSettingsInput_ normalized_steps(xl_steps); xl_steps = normalized_steps.Get(); Excel::ValidateMarketIvRange(xl_settings, "settings", true); const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get();
        argName = "snapshot (input #2)";
        const Handle_<StorableMarketIvCalibrationSnapshot_> snapshot = Excel::ToHandle<StorableMarketIvCalibrationSnapshot_>(_env, xl_snapshot);
        argName = "grid (input #3)";
        const Handle_<StorableMarketIvDupireRequest_> grid = Excel::ToHandle<StorableMarketIvDupireRequest_>(_env, xl_grid);
        argName = "model (input #4)";
        const Handle_<StorableMarketIvLocalVolModelSettings_> model = Excel::ToHandle<StorableMarketIvLocalVolModelSettings_>(_env, xl_model);
        argName = "product (input #5)";
        const Handle_<ScriptProductData_> product = Excel::ToHandle<ScriptProductData_>(_env, xl_product);
        argName = "quote_ids (input #6)";
        const Matrix_<Cell_> quote_ids = Excel::ToCellMatrix(xl_quote_ids, false, true);
        argName = "steps (input #7)";
        const Matrix_<Cell_> steps = Excel::ToCellMatrix(xl_steps, true, true);
        argName = "valuation (input #8)";
        const Handle_<StorableScriptValuationSettings_> valuation = Excel::ToHandle<StorableScriptValuationSettings_>(_env, xl_valuation, true);
        argName = "simulation (input #9)";
        const Handle_<StorableMonteCarloSettings_> simulation = Excel::ToHandle<StorableMonteCarloSettings_>(_env, xl_simulation, true);
        argName = "settings (input #10)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true, true);
        argName = 0;
		Handle_<StorableMarketIvScriptRefitRequest_> request;
        MarketIvScriptRefitRequest_New(name, snapshot, grid, model, product, quote_ids, steps, valuation, simulation, settings, &request);
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

struct XlRegister_MarketIvScriptRefitRequest_New_
{
    XlRegister_MarketIvScriptRefitRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Request name");
        argHelp.push_back("Certified base fit");
        argHelp.push_back("Dupire grid axes and settings");
        argHelp.push_back("Local-volatility model settings");
        argHelp.push_back("Script product");
        argHelp.push_back("Row or column of original quote IDs");
        argHelp.push_back("Positive bump sizes in requested ID order");
        argHelp.push_back("Valuation date and fixings");
        argHelp.push_back("MRG32 Precise simulation settings");
        argHelp.push_back("Two columns of sampling counts and budget limits");
        Excel::Register("Base", "xl_MarketIvScriptRefitRequest_New", "MARKETIVSCRIPTREFITREQUEST.NEW", "Script PV risk from complete local-volatility quote refits", "QQQQQQQQQQQ", "name,snapshot,grid,model,product,quote_ids,[steps],[valuation],[simulation],[settings]", argHelp, false);
    }
};
static XlRegister_MarketIvScriptRefitRequest_New_ The_MarketIvScriptRefitRequest_New_XlRegisterer;
