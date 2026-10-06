

extern "C" __declspec(dllexport) OPER_* xl_WeightedRiskResult_Get_History
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("WeightedRiskResult_Get_History");        
        argName = "result (input #1)";
        const Handle_<StorableWeightedRiskResult_> result = Excel::ToHandle<StorableWeightedRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> history;
        WeightedRiskResult_Get_History(result, &history);
        Excel::Retval_ retval;
        retval.Load(history);
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

struct XlRegister_WeightedRiskResult_Get_History_
{
    XlRegister_WeightedRiskResult_Get_History_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        Excel::Register("Base", "xl_WeightedRiskResult_Get_History", "WEIGHTEDRISKRESULT.GET.HISTORY", "Extract frozen observations without history access", "QQ", "result", argHelp, false);
    }
};
static XlRegister_WeightedRiskResult_Get_History_ The_WeightedRiskResult_Get_History_XlRegisterer;
