

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptRiskResult_Get_Valuation
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptRiskResult_Get_Valuation");        
        argName = "result (input #1)";
        const Handle_<StorableDupireScriptRiskResult_> result = Excel::ToHandle<StorableDupireScriptRiskResult_>(_env, xl_result);
        argName = 0;
		Handle_<StorableRiskResult_> valuation;
        DupireScriptRiskResult_Get_Valuation(result, &valuation);
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

struct XlRegister_DupireScriptRiskResult_Get_Valuation_
{
    XlRegister_DupireScriptRiskResult_Get_Valuation_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Automatic result");
        Excel::Register("Base", "xl_DupireScriptRiskResult_Get_Valuation", "DUPIRESCRIPTRISKRESULT.GET.VALUATION", "Copy the retained fixed-calibration model valuation", "QQ", "result", argHelp, false);
    }
};
static XlRegister_DupireScriptRiskResult_Get_Valuation_ The_DupireScriptRiskResult_Get_Valuation_XlRegisterer;
