

extern "C" __declspec(dllexport) OPER_* xl_JacobianRiskResult_Get_Provenance
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JacobianRiskResult_Get_Provenance");        
        argName = "result (input #1)";
        const Handle_<StorableJacobianRiskResult_> result = Excel::ToHandle<StorableJacobianRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> provenance;
        JacobianRiskResult_Get_Provenance(result, &provenance);
        Excel::Retval_ retval;
        retval.Load(provenance);
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

struct XlRegister_JacobianRiskResult_Get_Provenance_
{
    XlRegister_JacobianRiskResult_Get_Provenance_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        Excel::Register("Base", "xl_JacobianRiskResult_Get_Provenance", "JACOBIANRISKRESULT.GET.PROVENANCE", "Extract frozen method and valuation settings", "QQ", "result", argHelp, false);
    }
};
static XlRegister_JacobianRiskResult_Get_Provenance_ The_JacobianRiskResult_Get_Provenance_XlRegisterer;
