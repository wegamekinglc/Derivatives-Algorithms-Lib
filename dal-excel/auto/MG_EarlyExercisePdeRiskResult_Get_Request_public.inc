

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRiskResult_Get_Request
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRiskResult_Get_Request");        
        argName = "result (input #1)";
        const Handle_<StorableEarlyExercisePdeRiskResult_> result = Excel::ToHandle_orEarlyExerciseRisk<StorableEarlyExercisePdeRiskResult_>(_env, xl_result, "result");
        argName = 0;
		Handle_<StorableEarlyExercisePdeRiskRequest_> request;
        EarlyExercisePdeRiskResult_Get_Request(result, &request);
        Excel::Retval_ retval;
        retval.Load(_env, request);
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

struct XlRegister_EarlyExercisePdeRiskResult_Get_Request_
{
    XlRegister_EarlyExercisePdeRiskResult_Get_Request_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning handle");
        Excel::Register("Base", "xl_EarlyExercisePdeRiskResult_Get_Request", "EARLYEXERCISEPDERISKRESULT.GET.REQUEST", "Copy early exercise risk Request without native solves", "QQ", "result", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRiskResult_Get_Request_ The_EarlyExercisePdeRiskResult_Get_Request_XlRegisterer;
