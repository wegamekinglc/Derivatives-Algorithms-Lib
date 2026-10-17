

extern "C" __declspec(dllexport) OPER_* xl_RateTradeQuoteCurvatureResult_Get_Execution
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateTradeQuoteCurvatureResult_Get_Execution");        
        argName = "result (input #1)";
        const Handle_<StorableRateTradeQuoteCurvatureResult_> result = Excel::ToHandle<StorableRateTradeQuoteCurvatureResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> execution;
        RateTradeQuoteCurvatureResult_Get_Execution(result, &execution);
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

struct XlRegister_RateTradeQuoteCurvatureResult_Get_Execution_
{
    XlRegister_RateTradeQuoteCurvatureResult_Get_Execution_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning native rate result");
        Excel::Register("Base", "xl_RateTradeQuoteCurvatureResult_Get_Execution", "RATETRADEQUOTECURVATURERESULT.GET.EXECUTION", "Copy method, evaluations, calibrations, reverses, numeric bytes, peak tape bytes and cleanup reserve", "QQ", "result", argHelp, false);
    }
};
static XlRegister_RateTradeQuoteCurvatureResult_Get_Execution_ The_RateTradeQuoteCurvatureResult_Get_Execution_XlRegisterer;
