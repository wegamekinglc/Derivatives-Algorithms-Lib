

extern "C" __declspec(dllexport) OPER_* xl_DupireRiskInputs_New
    (const OPER_* xl_name, const OPER_* xl_quoteStrikes, const OPER_* xl_quoteMaturities, const OPER_* xl_quoteSpreads, const OPER_* xl_grid)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireRiskInputs_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireRiskInputs_New; name"); const Excel::ScriptSettingsInput_ quoteStrikesInput(xl_quoteStrikes); xl_quoteStrikes = quoteStrikesInput.Get();
        argName = "quoteStrikes (input #2)";
        const Vector_<double> quoteStrikes = Excel::ToDoubleVector(xl_quoteStrikes);
        const Excel::ScriptSettingsInput_ quoteMaturitiesInput(xl_quoteMaturities); xl_quoteMaturities = quoteMaturitiesInput.Get();
        argName = "quoteMaturities (input #3)";
        const Vector_<double> quoteMaturities = Excel::ToDoubleVector(xl_quoteMaturities);
        const Excel::ScriptSettingsInput_ quoteSpreadsInput(xl_quoteSpreads); xl_quoteSpreads = quoteSpreadsInput.Get();
        argName = "quoteSpreads (input #4)";
        const Matrix_<double> quoteSpreads = Excel::ToDoubleMatrix(xl_quoteSpreads);
        argName = "grid (input #5)";
        const Handle_<StorableDupireGrid_> grid = Excel::ToHandle<StorableDupireGrid_>(_env, xl_grid);
        argName = 0;
		Handle_<StorableDupireRiskInputs_> inputs;
        DupireRiskInputs_New(name, quoteStrikes, quoteMaturities, quoteSpreads, grid, &inputs);
        Excel::Retval_ retval;
        retval.Load(_env, inputs);
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

struct XlRegister_DupireRiskInputs_New_
{
    XlRegister_DupireRiskInputs_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Positive increasing strike axis");
        argHelp.push_back("Positive increasing maturity axis");
        argHelp.push_back("Strike rows, maturity columns; absolute decimal-vol spreads");
        argHelp.push_back("Immutable grid settings");
        Excel::Register("Base", "xl_DupireRiskInputs_New", "DUPIRERISKINPUTS.NEW", "Create immutable spread quotes and grid inputs", "QQQQQQ", "name,quoteStrikes,quoteMaturities,quoteSpreads,grid", argHelp, false);
    }
};
static XlRegister_DupireRiskInputs_New_ The_DupireRiskInputs_New_XlRegisterer;
