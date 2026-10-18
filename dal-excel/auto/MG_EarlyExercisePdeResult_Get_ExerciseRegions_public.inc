

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeResult_Get_ExerciseRegions
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeResult_Get_ExerciseRegions");        
        argName = "result (input #1)";
        const Handle_<StorableEarlyExercisePdeResult_> result = Excel::ToHandle<StorableEarlyExercisePdeResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> table;
        EarlyExercisePdeResult_Get_ExerciseRegions(result, &table);
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

struct XlRegister_EarlyExercisePdeResult_Get_ExerciseRegions_
{
    XlRegister_EarlyExercisePdeResult_Get_ExerciseRegions_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning result handle");
        Excel::Register("Base", "xl_EarlyExercisePdeResult_Get_ExerciseRegions", "EARLYEXERCISEPDERESULT.GET.EXERCISEREGIONS", "Copy early exercise ExerciseRegions diagnostics", "QQ", "result", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeResult_Get_ExerciseRegions_ The_EarlyExercisePdeResult_Get_ExerciseRegions_XlRegisterer;
