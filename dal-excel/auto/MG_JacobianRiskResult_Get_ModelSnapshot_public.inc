

extern "C" __declspec(dllexport) OPER_* xl_JacobianRiskResult_Get_ModelSnapshot
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JacobianRiskResult_Get_ModelSnapshot");        
        argName = "result (input #1)";
        const Handle_<StorableJacobianRiskResult_> result = Excel::ToHandle<StorableJacobianRiskResult_>(_env, xl_result);
        argName = 0;
		Vector_<String_> json;
        JacobianRiskResult_Get_ModelSnapshot(result, &json);
        Excel::Retval_ retval;
        retval.Load(json);
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

struct XlRegister_JacobianRiskResult_Get_ModelSnapshot_
{
    XlRegister_JacobianRiskResult_Get_ModelSnapshot_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        Excel::Register("Base", "xl_JacobianRiskResult_Get_ModelSnapshot", "JACOBIANRISKRESULT.GET.MODELSNAPSHOT", "Extract frozen passive model JSON chunks", "QQ", "result", argHelp, false);
    }
};
static XlRegister_JacobianRiskResult_Get_ModelSnapshot_ The_JacobianRiskResult_Get_ModelSnapshot_XlRegisterer;
