

extern "C" __declspec(dllexport) OPER_* xl_JacobianRiskResult_Get_Outputs
    (const OPER_* xl_result, const OPER_* xl_complete)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JacobianRiskResult_Get_Outputs");        
        argName = "result (input #1)";
        const Handle_<StorableJacobianRiskResult_> result = Excel::ToHandle<StorableJacobianRiskResult_>(_env, xl_result);
        argName = "complete"; xl_complete = Excel::ScriptScalarInput(xl_complete); Excel::ValidateRiskRequestBoolean(xl_complete, "JacobianRiskResult_Get_Outputs; complete");
        argName = "complete (input #2)";
        const bool complete = Excel::ToBool(xl_complete, true).value_or(false);
        argName = 0;
		Matrix_<Cell_> outputs;
        JacobianRiskResult_Get_Outputs(result, complete, &outputs);
        Excel::Retval_ retval;
        retval.Load(outputs);
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

struct XlRegister_JacobianRiskResult_Get_Outputs_
{
    XlRegister_JacobianRiskResult_Get_Outputs_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        argHelp.push_back("True returns the original complete scalar output axis");
        Excel::Register("Base", "xl_JacobianRiskResult_Get_Outputs", "JACOBIANRISKRESULT.GET.OUTPUTS", "Extract ordered selected or complete output coordinates", "QQQ", "result,[complete]", argHelp, false);
    }
};
static XlRegister_JacobianRiskResult_Get_Outputs_ The_JacobianRiskResult_Get_Outputs_XlRegisterer;
