

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRequest_Get_Market
    (const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRequest_Get_Market");        
        argName = "request (input #1)";
        const Handle_<StorableEarlyExercisePdeRequest_> request = Excel::ToHandle<StorableEarlyExercisePdeRequest_>(_env, xl_request);
        argName = 0;
		Matrix_<Cell_> table;
        EarlyExercisePdeRequest_Get_Market(request, &table);
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

struct XlRegister_EarlyExercisePdeRequest_Get_Market_
{
    XlRegister_EarlyExercisePdeRequest_Get_Market_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning request handle");
        Excel::Register("Base", "xl_EarlyExercisePdeRequest_Get_Market", "EARLYEXERCISEPDEREQUEST.GET.MARKET", "Copy early exercise Market diagnostics", "QQ", "request", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRequest_Get_Market_ The_EarlyExercisePdeRequest_Get_Market_XlRegisterer;
