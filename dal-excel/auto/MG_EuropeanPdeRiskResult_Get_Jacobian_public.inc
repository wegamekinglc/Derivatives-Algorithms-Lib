

extern "C" __declspec(dllexport) OPER_* xl_EuropeanPdeRiskResult_Get_Jacobian
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EuropeanPdeRiskResult_Get_Jacobian");        
        argName = "result (input #1)";
        const Handle_<StorableEuropeanPdeRiskResult_> result = Excel::ToHandle<StorableEuropeanPdeRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> risks;
        EuropeanPdeRiskResult_Get_Jacobian(result, &risks);
        Excel::Retval_ retval;
        retval.Load(risks);
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

struct XlRegister_EuropeanPdeRiskResult_Get_Jacobian_
{
    XlRegister_EuropeanPdeRiskResult_Get_Jacobian_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning result");
        Excel::Register("Base", "xl_EuropeanPdeRiskResult_Get_Jacobian", "EUROPEANPDERISKRESULT.GET.JACOBIAN", "Copy raw European first derivatives and their units", "QQ", "result", argHelp, false);
    }
};
static XlRegister_EuropeanPdeRiskResult_Get_Jacobian_ The_EuropeanPdeRiskResult_Get_Jacobian_XlRegisterer;
