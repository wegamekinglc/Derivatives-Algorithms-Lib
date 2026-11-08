

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRiskResult_Get_Plan
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRiskResult_Get_Plan");        
        argName = "result (input #1)";
        const Handle_<StorableEarlyExercisePdeRiskResult_> result = Excel::ToHandle<StorableEarlyExercisePdeRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> table;
        EarlyExercisePdeRiskResult_Get_Plan(result, &table);
        Excel::Retval_ retval;
        retval.Load(table);
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

struct XlRegister_EarlyExercisePdeRiskResult_Get_Plan_
{
    XlRegister_EarlyExercisePdeRiskResult_Get_Plan_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning handle");
        Excel::Register("Base", "xl_EarlyExercisePdeRiskResult_Get_Plan", "EARLYEXERCISEPDERISKRESULT.GET.PLAN", "Copy early exercise risk Plan without native solves", "QQ", "result", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRiskResult_Get_Plan_ The_EarlyExercisePdeRiskResult_Get_Plan_XlRegisterer;
