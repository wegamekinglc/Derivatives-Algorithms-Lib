

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRiskResult_Get_BaseResult
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRiskResult_Get_BaseResult");        
        argName = "result (input #1)";
        const Handle_<StorableEarlyExercisePdeRiskResult_> result = Excel::ToHandle_orEarlyExerciseRisk<StorableEarlyExercisePdeRiskResult_>(_env, xl_result, "result");
        argName = 0;
		Handle_<StorableEarlyExercisePdeResult_> base;
        EarlyExercisePdeRiskResult_Get_BaseResult(result, &base);
        Excel::Retval_ retval;
        retval.Load(_env, base);
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

struct XlRegister_EarlyExercisePdeRiskResult_Get_BaseResult_
{
    XlRegister_EarlyExercisePdeRiskResult_Get_BaseResult_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning handle");
        Excel::Register("Base", "xl_EarlyExercisePdeRiskResult_Get_BaseResult", "EARLYEXERCISEPDERISKRESULT.GET.BASERESULT", "Copy early exercise risk BaseResult without native solves", "QQ", "result", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRiskResult_Get_BaseResult_ The_EarlyExercisePdeRiskResult_Get_BaseResult_XlRegisterer;
