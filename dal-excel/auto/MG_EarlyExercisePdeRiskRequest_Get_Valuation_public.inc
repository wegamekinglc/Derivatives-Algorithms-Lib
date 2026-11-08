

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRiskRequest_Get_Valuation
    (const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRiskRequest_Get_Valuation");        
        argName = "request (input #1)";
        const Handle_<StorableEarlyExercisePdeRiskRequest_> request = Excel::ToHandle<StorableEarlyExercisePdeRiskRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableEarlyExercisePdeRequest_> valuation;
        EarlyExercisePdeRiskRequest_Get_Valuation(request, &valuation);
        Excel::Retval_ retval;
        retval.Load(_env, valuation);
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

struct XlRegister_EarlyExercisePdeRiskRequest_Get_Valuation_
{
    XlRegister_EarlyExercisePdeRiskRequest_Get_Valuation_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning handle");
        Excel::Register("Base", "xl_EarlyExercisePdeRiskRequest_Get_Valuation", "EARLYEXERCISEPDERISKREQUEST.GET.VALUATION", "Copy early exercise risk Valuation without native solves", "QQ", "request", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRiskRequest_Get_Valuation_ The_EarlyExercisePdeRiskRequest_Get_Valuation_XlRegisterer;
