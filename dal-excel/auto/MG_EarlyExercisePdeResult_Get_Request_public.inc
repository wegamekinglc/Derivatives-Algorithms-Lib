

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeResult_Get_Request
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeResult_Get_Request");        
        argName = "result (input #1)";
        const Handle_<StorableEarlyExercisePdeResult_> result = Excel::ToHandle<StorableEarlyExercisePdeResult_>(_env, xl_result);
        argName = 0;
		Handle_<StorableEarlyExercisePdeRequest_> request;
        EarlyExercisePdeResult_Get_Request(result, &request);
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

struct XlRegister_EarlyExercisePdeResult_Get_Request_
{
    XlRegister_EarlyExercisePdeResult_Get_Request_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Result handle");
        Excel::Register("Base", "xl_EarlyExercisePdeResult_Get_Request", "EARLYEXERCISEPDERESULT.GET.REQUEST", "Copy the resolved request", "QQ", "result", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeResult_Get_Request_ The_EarlyExercisePdeResult_Get_Request_XlRegisterer;
