

extern "C" __declspec(dllexport) OPER_* xl_WeightedRiskResult_Get_Components
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("WeightedRiskResult_Get_Components");        
        argName = "result (input #1)";
        const Handle_<StorableWeightedRiskResult_> result = Excel::ToHandle<StorableWeightedRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> components;
        WeightedRiskResult_Get_Components(result, &components);
        Excel::Retval_ retval;
        retval.Load(components);
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

struct XlRegister_WeightedRiskResult_Get_Components_
{
    XlRegister_WeightedRiskResult_Get_Components_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        Excel::Register("Base", "xl_WeightedRiskResult_Get_Components", "WEIGHTEDRISKRESULT.GET.COMPONENTS", "Extract ordered output identities, passive weights and component means", "QQ", "result", argHelp, false);
    }
};
static XlRegister_WeightedRiskResult_Get_Components_ The_WeightedRiskResult_Get_Components_XlRegisterer;
