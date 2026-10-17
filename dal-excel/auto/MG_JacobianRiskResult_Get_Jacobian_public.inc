

extern "C" __declspec(dllexport) OPER_* xl_JacobianRiskResult_Get_Jacobian
    (const OPER_* xl_result, const OPER_* xl_reported)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JacobianRiskResult_Get_Jacobian");        
        argName = "result (input #1)";
        const Handle_<StorableJacobianRiskResult_> result = Excel::ToHandle<StorableJacobianRiskResult_>(_env, xl_result);
        argName = "reported"; xl_reported = Excel::ScriptScalarInput(xl_reported); Excel::ValidateRiskRequestBoolean(xl_reported, "JacobianRiskResult_Get_Jacobian; reported");
        argName = "reported (input #2)";
        const bool reported = Excel::ToBool(xl_reported, true).value_or(false);
        argName = 0;
		Matrix_<Cell_> jacobian;
        JacobianRiskResult_Get_Jacobian(result, reported, &jacobian);
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

struct XlRegister_JacobianRiskResult_Get_Jacobian_
{
    XlRegister_JacobianRiskResult_Get_Jacobian_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        argHelp.push_back("True applies each selected input report factor once");
        Excel::Register("Base", "xl_JacobianRiskResult_Get_Jacobian", "JACOBIANRISKRESULT.GET.JACOBIAN", "Extract a copy of the complete selected-output Jacobian", "QQQ", "result,[reported]", argHelp, false);
    }
};
static XlRegister_JacobianRiskResult_Get_Jacobian_ The_JacobianRiskResult_Get_Jacobian_XlRegisterer;
