

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeResult_New
    (const OPER_* xl_name, const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeResult_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "EarlyExercisePdeResult_New; name");
        argName = "request (input #2)";
        const Handle_<StorableEarlyExercisePdeRequest_> request = Excel::ToHandle<StorableEarlyExercisePdeRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableEarlyExercisePdeResult_> result;
        EarlyExercisePdeResult_New(name, request, &result);
        Excel::Retval_ retval;
        retval.Load(_env, result);
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

struct XlRegister_EarlyExercisePdeResult_New_
{
    XlRegister_EarlyExercisePdeResult_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Request handle");
        Excel::Register("Base", "xl_EarlyExercisePdeResult_New", "EARLYEXERCISEPDERESULT.NEW", "Price an early exercise option without recording AAD", "QQQ", "name,request", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeResult_New_ The_EarlyExercisePdeResult_New_XlRegisterer;
