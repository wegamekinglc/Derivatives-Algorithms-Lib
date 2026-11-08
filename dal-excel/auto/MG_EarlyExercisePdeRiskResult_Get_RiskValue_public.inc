

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRiskResult_Get_RiskValue
    (const OPER_* xl_result, const OPER_* xl_risk_id)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRiskResult_Get_RiskValue");        
        argName = "result (input #1)";
        const Handle_<StorableEarlyExercisePdeRiskResult_> result = Excel::ToHandle_orEarlyExerciseRisk<StorableEarlyExercisePdeRiskResult_>(_env, xl_result, "result");
        argName = "risk_id (input #2)";
        const String_ risk_id = Excel::ToString_orEarlyExerciseRisk(xl_risk_id, "risk_id");
        argName = 0;
		double value;
        EarlyExercisePdeRiskResult_Get_RiskValue(result, risk_id, &value);
        Excel::Retval_ retval;
        retval.Load(value);
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

struct XlRegister_EarlyExercisePdeRiskResult_Get_RiskValue_
{
    XlRegister_EarlyExercisePdeRiskResult_Get_RiskValue_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning handle");
        argHelp.push_back("Exact canonical case-sensitive risk ID");
        Excel::Register("Base", "xl_EarlyExercisePdeRiskResult_Get_RiskValue", "EARLYEXERCISEPDERISKRESULT.GET.RISKVALUE", "Copy early exercise risk RiskValue without native solves", "QQQ", "result,risk_id", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRiskResult_Get_RiskValue_ The_EarlyExercisePdeRiskResult_Get_RiskValue_XlRegisterer;
