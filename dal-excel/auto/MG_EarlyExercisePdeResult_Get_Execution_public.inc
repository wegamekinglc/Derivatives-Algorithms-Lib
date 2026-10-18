

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeResult_Get_Execution
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeResult_Get_Execution");        
        argName = "result (input #1)";
        const Handle_<StorableEarlyExercisePdeResult_> result = Excel::ToHandle<StorableEarlyExercisePdeResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> table;
        EarlyExercisePdeResult_Get_Execution(result, &table);
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

struct XlRegister_EarlyExercisePdeResult_Get_Execution_
{
    XlRegister_EarlyExercisePdeResult_Get_Execution_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning result handle");
        Excel::Register("Base", "xl_EarlyExercisePdeResult_Get_Execution", "EARLYEXERCISEPDERESULT.GET.EXECUTION", "Copy early exercise Execution diagnostics", "QQ", "result", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeResult_Get_Execution_ The_EarlyExercisePdeResult_Get_Execution_XlRegisterer;
