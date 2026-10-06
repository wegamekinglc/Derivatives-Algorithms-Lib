

extern "C" __declspec(dllexport) OPER_* xl_WeightedRiskResult_Get_Provenance
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("WeightedRiskResult_Get_Provenance");        
        argName = "result (input #1)";
        const Handle_<StorableWeightedRiskResult_> result = Excel::ToHandle<StorableWeightedRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> provenance;
        WeightedRiskResult_Get_Provenance(result, &provenance);
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

struct XlRegister_WeightedRiskResult_Get_Provenance_
{
    XlRegister_WeightedRiskResult_Get_Provenance_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        Excel::Register("Base", "xl_WeightedRiskResult_Get_Provenance", "WEIGHTEDRISKRESULT.GET.PROVENANCE", "Extract the retained method and execution settings without valuation", "QQ", "result", argHelp, false);
    }
};
static XlRegister_WeightedRiskResult_Get_Provenance_ The_WeightedRiskResult_Get_Provenance_XlRegisterer;
