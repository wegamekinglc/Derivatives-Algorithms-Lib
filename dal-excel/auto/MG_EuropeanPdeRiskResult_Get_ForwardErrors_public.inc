

extern "C" __declspec(dllexport) OPER_* xl_EuropeanPdeRiskResult_Get_ForwardErrors
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EuropeanPdeRiskResult_Get_ForwardErrors");        
        argName = "result (input #1)";
        const Handle_<StorableEuropeanPdeRiskResult_> result = Excel::ToHandle<StorableEuropeanPdeRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> errors;
        EuropeanPdeRiskResult_Get_ForwardErrors(result, &errors);
        Excel::Retval_ retval;
        retval.Load(errors);
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

struct XlRegister_EuropeanPdeRiskResult_Get_ForwardErrors_
{
    XlRegister_EuropeanPdeRiskResult_Get_ForwardErrors_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning result");
        Excel::Register("Base", "xl_EuropeanPdeRiskResult_Get_ForwardErrors", "EUROPEANPDERISKRESULT.GET.FORWARDERRORS", "Copy chronological European forward solve errors", "QQ", "result", argHelp, false);
    }
};
static XlRegister_EuropeanPdeRiskResult_Get_ForwardErrors_ The_EuropeanPdeRiskResult_Get_ForwardErrors_XlRegisterer;
