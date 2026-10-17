

extern "C" __declspec(dllexport) OPER_* xl_WeightedRiskResult_Get_ModelSnapshot
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("WeightedRiskResult_Get_ModelSnapshot");        
        argName = "result (input #1)";
        const Handle_<StorableWeightedRiskResult_> result = Excel::ToHandle<StorableWeightedRiskResult_>(_env, xl_result);
        argName = 0;
		Vector_<String_> json;
        WeightedRiskResult_Get_ModelSnapshot(result, &json);
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

struct XlRegister_WeightedRiskResult_Get_ModelSnapshot_
{
    XlRegister_WeightedRiskResult_Get_ModelSnapshot_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        Excel::Register("Base", "xl_WeightedRiskResult_Get_ModelSnapshot", "WEIGHTEDRISKRESULT.GET.MODELSNAPSHOT", "Extract retained passive model JSON chunks", "QQ", "result", argHelp, false);
    }
};
static XlRegister_WeightedRiskResult_Get_ModelSnapshot_ The_WeightedRiskResult_Get_ModelSnapshot_XlRegisterer;
