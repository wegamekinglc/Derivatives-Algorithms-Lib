

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRiskRequest_New
    (const OPER_* xl_name, const OPER_* xl_valuation, const OPER_* xl_risk)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRiskRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateEarlyExercisePdeRiskName(xl_name); xl_risk = Excel::ScriptScalarInput(xl_risk);
        argName = "valuation (input #2)";
        const Handle_<StorableEarlyExercisePdeRequest_> valuation = Excel::ToHandle<StorableEarlyExercisePdeRequest_>(_env, xl_valuation);
        argName = "risk (input #3)";
        const Handle_<StorableEarlyExercisePdeRiskSettings_> risk = Excel::ToHandle<StorableEarlyExercisePdeRiskSettings_>(_env, xl_risk, true);
        argName = 0;
		Handle_<StorableEarlyExercisePdeRiskRequest_> request;
        EarlyExercisePdeRiskRequest_New(name, valuation, risk, &request);
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

struct XlRegister_EarlyExercisePdeRiskRequest_New_
{
    XlRegister_EarlyExercisePdeRiskRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Existing price request");
        argHelp.push_back("Blank uses default risk settings");
        Excel::Register("Base", "xl_EarlyExercisePdeRiskRequest_New", "EARLYEXERCISEPDERISKREQUEST.NEW", "Copy a price request and optional risk settings", "QQQQ", "name,valuation,[risk]", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRiskRequest_New_ The_EarlyExercisePdeRiskRequest_New_XlRegisterer;
