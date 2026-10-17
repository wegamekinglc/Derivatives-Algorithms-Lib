

extern "C" __declspec(dllexport) OPER_* xl_WeightedRiskResult_Get_WeightedValue
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("WeightedRiskResult_Get_WeightedValue");        
        argName = "result (input #1)";
        const Handle_<StorableWeightedRiskResult_> result = Excel::ToHandle<StorableWeightedRiskResult_>(_env, xl_result);
        argName = 0;
		double value;
        WeightedRiskResult_Get_WeightedValue(result, &value);
        Excel::Retval_ retval;
        retval.Load(value);
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

struct XlRegister_WeightedRiskResult_Get_WeightedValue_
{
    XlRegister_WeightedRiskResult_Get_WeightedValue_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        Excel::Register("Base", "xl_WeightedRiskResult_Get_WeightedValue", "WEIGHTEDRISKRESULT.GET.WEIGHTEDVALUE", "Extract the mean weighted objective", "QQ", "result", argHelp, false);
    }
};
static XlRegister_WeightedRiskResult_Get_WeightedValue_ The_WeightedRiskResult_Get_WeightedValue_XlRegisterer;
