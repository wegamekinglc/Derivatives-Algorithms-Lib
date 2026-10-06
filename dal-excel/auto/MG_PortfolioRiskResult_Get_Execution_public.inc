

extern "C" __declspec(dllexport) OPER_* xl_PortfolioRiskResult_Get_Execution
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PortfolioRiskResult_Get_Execution");        
        argName = "result (input #1)";
        const Handle_<Storable_> result = Excel::ToHandle<Storable_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> execution;
        PortfolioRiskResult_Get_Execution(result, &execution);
        Excel::Retval_ retval;
        retval.Load(execution);
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

struct XlRegister_PortfolioRiskResult_Get_Execution_
{
    XlRegister_PortfolioRiskResult_Get_Execution_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed weighted or attribution portfolio result");
        Excel::Register("Base", "xl_PortfolioRiskResult_Get_Execution", "PORTFOLIORISKRESULT.GET.EXECUTION", "Extract actual frozen portfolio group work and whole-request peaks", "QQ", "result", argHelp, false);
    }
};
static XlRegister_PortfolioRiskResult_Get_Execution_ The_PortfolioRiskResult_Get_Execution_XlRegisterer;
