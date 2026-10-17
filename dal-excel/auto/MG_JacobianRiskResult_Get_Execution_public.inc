

extern "C" __declspec(dllexport) OPER_* xl_JacobianRiskResult_Get_Execution
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JacobianRiskResult_Get_Execution");        
        argName = "result (input #1)";
        const Handle_<StorableJacobianRiskResult_> result = Excel::ToHandle<StorableJacobianRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> execution;
        JacobianRiskResult_Get_Execution(result, &execution);
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

struct XlRegister_JacobianRiskResult_Get_Execution_
{
    XlRegister_JacobianRiskResult_Get_Execution_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        Excel::Register("Base", "xl_JacobianRiskResult_Get_Execution", "JACOBIANRISKRESULT.GET.EXECUTION", "Extract actual widths, replayed work and numeric capacity peaks", "QQ", "result", argHelp, false);
    }
};
static XlRegister_JacobianRiskResult_Get_Execution_ The_JacobianRiskResult_Get_Execution_XlRegisterer;
