

extern "C" __declspec(dllexport) OPER_* xl_MarketIvRefitRiskRequest_New
    (const OPER_* xl_name, const OPER_* xl_quote_ids, const OPER_* xl_outputs, const OPER_* xl_settings, const OPER_* xl_steps)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvRefitRiskRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "quote_ids (input #2)";
        const Matrix_<Cell_> quote_ids = Excel::ToCellMatrix(xl_quote_ids, false, true);
        argName = "outputs (input #3)";
        const Matrix_<Cell_> outputs = Excel::ToCellMatrix(xl_outputs, false, true);
        Excel::ValidateMarketIvRange(xl_quote_ids, "quote_ids"); const Excel::ScriptSettingsInput_ normalized_quote_ids(xl_quote_ids); xl_quote_ids = normalized_quote_ids.Get(); Excel::ValidateMarketIvRange(xl_outputs, "outputs"); const Excel::ScriptSettingsInput_ normalized_outputs(xl_outputs); xl_outputs = normalized_outputs.Get(); Excel::ValidateMarketIvRange(xl_steps, "steps"); const Excel::ScriptSettingsInput_ normalized_steps(xl_steps); xl_steps = normalized_steps.Get();
        argName = "settings (input #4)";
        const Handle_<StorableMarketIvRefitRiskSettings_> settings = Excel::ToHandle<StorableMarketIvRefitRiskSettings_>(_env, xl_settings, true);
        argName = "steps (input #5)";
        const Matrix_<Cell_> steps = Excel::ToCellMatrix(xl_steps, true, true);
        argName = 0;
		Handle_<StorableMarketIvRefitRiskRequest_> request;
        MarketIvRefitRiskRequest_New(name, quote_ids, outputs, settings, steps, &request);
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

struct XlRegister_MarketIvRefitRiskRequest_New_
{
    XlRegister_MarketIvRefitRiskRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("name");
        argHelp.push_back("quote ids");
        argHelp.push_back("outputs");
        argHelp.push_back("settings");
        argHelp.push_back("steps");
        Excel::Register("Base", "xl_MarketIvRefitRiskRequest_New", "MARKETIVREFITRISKREQUEST.NEW", "Market IV surface owning value", "QQQQQQ", "name,quote_ids,outputs,[settings],[steps]", argHelp, false);
    }
};
static XlRegister_MarketIvRefitRiskRequest_New_ The_MarketIvRefitRiskRequest_New_XlRegisterer;
