

extern "C" __declspec(dllexport) OPER_* xl_PortfolioRiskResult_Get_Values
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PortfolioRiskResult_Get_Values");        
        argName = "result (input #1)";
        const Handle_<Storable_> result = Excel::ToHandle<Storable_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> values;
        PortfolioRiskResult_Get_Values(result, &values);
        Excel::Retval_ retval;
        retval.Load(values);
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

struct XlRegister_PortfolioRiskResult_Get_Values_
{
    XlRegister_PortfolioRiskResult_Get_Values_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed weighted or attribution portfolio result");
        Excel::Register("Base", "xl_PortfolioRiskResult_Get_Values", "PORTFOLIORISKRESULT.GET.VALUES", "Extract independent selected portfolio row values and weights", "QQ", "result", argHelp, false);
    }
};
static XlRegister_PortfolioRiskResult_Get_Values_ The_PortfolioRiskResult_Get_Values_XlRegisterer;
