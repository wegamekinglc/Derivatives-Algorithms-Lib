

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRiskResult_New
    (const OPER_* xl_name, const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRiskResult_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateEarlyExercisePdeRiskName(xl_name);
        argName = "request (input #2)";
        const Handle_<StorableEarlyExercisePdeRiskRequest_> request = Excel::ToHandle<StorableEarlyExercisePdeRiskRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableEarlyExercisePdeRiskResult_> result;
        EarlyExercisePdeRiskResult_New(name, request, &result);
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

struct XlRegister_EarlyExercisePdeRiskResult_New_
{
    XlRegister_EarlyExercisePdeRiskResult_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Owning risk request");
        Excel::Register("Base", "xl_EarlyExercisePdeRiskResult_New", "EARLYEXERCISEPDERISKRESULT.NEW", "Evaluate fixed-plan passive early exercise risks; inspect availability", "QQQ", "name,request", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRiskResult_New_ The_EarlyExercisePdeRiskResult_New_XlRegisterer;
