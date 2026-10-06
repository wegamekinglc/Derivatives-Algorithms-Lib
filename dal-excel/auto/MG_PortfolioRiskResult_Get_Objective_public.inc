

extern "C" __declspec(dllexport) OPER_* xl_PortfolioRiskResult_Get_Objective
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PortfolioRiskResult_Get_Objective");        
        argName = "result (input #1)";
        const Handle_<Storable_> result = Excel::ToHandle<Storable_>(_env, xl_result);
        argName = 0;
		double objective;
        PortfolioRiskResult_Get_Objective(result, &objective);
        Excel::Retval_ retval;
        retval.Load(objective);
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

struct XlRegister_PortfolioRiskResult_Get_Objective_
{
    XlRegister_PortfolioRiskResult_Get_Objective_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed weighted portfolio result; attribution results reject");
        Excel::Register("Base", "xl_PortfolioRiskResult_Get_Objective", "PORTFOLIORISKRESULT.GET.OBJECTIVE", "Extract a completed weighted portfolio objective", "QQ", "result", argHelp, false);
    }
};
static XlRegister_PortfolioRiskResult_Get_Objective_ The_PortfolioRiskResult_Get_Objective_XlRegisterer;
