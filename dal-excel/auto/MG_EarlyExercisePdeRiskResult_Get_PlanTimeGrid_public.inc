

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRiskResult_Get_PlanTimeGrid
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRiskResult_Get_PlanTimeGrid");        
        argName = "result (input #1)";
        const Handle_<StorableEarlyExercisePdeRiskResult_> result = Excel::ToHandle<StorableEarlyExercisePdeRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> table;
        EarlyExercisePdeRiskResult_Get_PlanTimeGrid(result, &table);
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

struct XlRegister_EarlyExercisePdeRiskResult_Get_PlanTimeGrid_
{
    XlRegister_EarlyExercisePdeRiskResult_Get_PlanTimeGrid_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning handle");
        Excel::Register("Base", "xl_EarlyExercisePdeRiskResult_Get_PlanTimeGrid", "EARLYEXERCISEPDERISKRESULT.GET.PLANTIMEGRID", "Copy early exercise risk PlanTimeGrid without native solves", "QQ", "result", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRiskResult_Get_PlanTimeGrid_ The_EarlyExercisePdeRiskResult_Get_PlanTimeGrid_XlRegisterer;
