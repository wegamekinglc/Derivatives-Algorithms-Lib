

extern "C" __declspec(dllexport) OPER_* xl_RiskResult_Get_Jacobian
    (const OPER_* xl_result, const OPER_* xl_reported)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RiskResult_Get_Jacobian");        
        argName = "result (input #1)";
        const Handle_<StorableRiskResult_> result = Excel::ToHandle<StorableRiskResult_>(_env, xl_result);
        argName = "reported (input #2)";
        const bool reported = Excel::ToBool(xl_reported, true).value_or(false);
        argName = 0;
		Matrix_<Cell_> jacobian;
        RiskResult_Get_Jacobian(result, reported, &jacobian);
        Excel::Retval_ retval;
        retval.Load(jacobian);
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

struct XlRegister_RiskResult_Get_Jacobian_
{
    XlRegister_RiskResult_Get_Jacobian_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        argHelp.push_back("False returns raw derivatives; true applies each report factor once");
        Excel::Register("Base", "xl_RiskResult_Get_Jacobian", "RISKRESULT.GET.JACOBIAN", "Extract a copy of the scalar Jacobian", "QQQ", "result,[reported]", argHelp, false);
    }
};
static XlRegister_RiskResult_Get_Jacobian_ The_RiskResult_Get_Jacobian_XlRegisterer;
