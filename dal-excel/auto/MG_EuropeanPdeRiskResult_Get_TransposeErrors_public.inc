

extern "C" __declspec(dllexport) OPER_* xl_EuropeanPdeRiskResult_Get_TransposeErrors
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EuropeanPdeRiskResult_Get_TransposeErrors");        
        argName = "result (input #1)";
        const Handle_<StorableEuropeanPdeRiskResult_> result = Excel::ToHandle<StorableEuropeanPdeRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> errors;
        EuropeanPdeRiskResult_Get_TransposeErrors(result, &errors);
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

struct XlRegister_EuropeanPdeRiskResult_Get_TransposeErrors_
{
    XlRegister_EuropeanPdeRiskResult_Get_TransposeErrors_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning result");
        Excel::Register("Base", "xl_EuropeanPdeRiskResult_Get_TransposeErrors", "EUROPEANPDERISKRESULT.GET.TRANSPOSEERRORS", "Copy chronological European transpose solve errors", "QQ", "result", argHelp, false);
    }
};
static XlRegister_EuropeanPdeRiskResult_Get_TransposeErrors_ The_EuropeanPdeRiskResult_Get_TransposeErrors_XlRegisterer;
