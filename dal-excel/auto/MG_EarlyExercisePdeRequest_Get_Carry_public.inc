

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRequest_Get_Carry
    (const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRequest_Get_Carry");        
        argName = "request (input #1)";
        const Handle_<StorableEarlyExercisePdeRequest_> request = Excel::ToHandle<StorableEarlyExercisePdeRequest_>(_env, xl_request);
        argName = 0;
		Matrix_<Cell_> table;
        EarlyExercisePdeRequest_Get_Carry(request, &table);
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

struct XlRegister_EarlyExercisePdeRequest_Get_Carry_
{
    XlRegister_EarlyExercisePdeRequest_Get_Carry_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning request handle");
        Excel::Register("Base", "xl_EarlyExercisePdeRequest_Get_Carry", "EARLYEXERCISEPDEREQUEST.GET.CARRY", "Copy early exercise Carry diagnostics", "QQ", "request", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRequest_Get_Carry_ The_EarlyExercisePdeRequest_Get_Carry_XlRegisterer;
