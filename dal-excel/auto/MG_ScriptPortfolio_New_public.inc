

extern "C" __declspec(dllexport) OPER_* xl_ScriptPortfolio_New
    (const OPER_* xl_name, const OPER_* xl_trades)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptPortfolio_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "ScriptPortfolio_New; name"); argName = "trades"; Excel::ValidatePortfolioTrades(xl_trades);
        argName = "trades (input #2)";
        const Matrix_<Cell_> trades = Excel::ToCellMatrix(xl_trades);
        argName = 0;
		Handle_<ScriptPortfolioData_> portfolio;
        ScriptPortfolio_New(name, trades, &portfolio);
        Excel::Retval_ retval;
        retval.Load(_env, portfolio);
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

struct XlRegister_ScriptPortfolio_New_
{
    XlRegister_ScriptPortfolio_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Physical three-column table: unique trade ID, product handle, model handle");
        Excel::Register("Base", "xl_ScriptPortfolio_New", "SCRIPTPORTFOLIO.NEW", "Seal ordered script trades with explicit original model ownership", "QQQ", "name,trades", argHelp, false);
    }
};
static XlRegister_ScriptPortfolio_New_ The_ScriptPortfolio_New_XlRegisterer;
