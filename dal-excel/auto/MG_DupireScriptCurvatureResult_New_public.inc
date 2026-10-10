

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptCurvatureResult_New
    (const OPER_* xl_name, const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptCurvatureResult_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "DupireScriptCurvatureResult_New; name");
        argName = "plan (input #2)";
        const Handle_<StorableDupireScriptCurvaturePlan_> plan = Excel::ToHandle<StorableDupireScriptCurvaturePlan_>(_env, xl_plan);
        argName = 0;
		Handle_<StorableDupireScriptCurvatureResult_> result;
        DupireScriptCurvatureResult_New(name, plan, &result);
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

struct XlRegister_DupireScriptCurvatureResult_New_
{
    XlRegister_DupireScriptCurvatureResult_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Frozen common-path plan; active outer recordings reject");
        Excel::Register("Base", "xl_DupireScriptCurvatureResult_New", "DUPIRESCRIPTCURVATURERESULT.NEW", "Evaluate finite-step Dupire quote curvature through full recalibration", "QQQ", "name,plan", argHelp, false);
    }
};
static XlRegister_DupireScriptCurvatureResult_New_ The_DupireScriptCurvatureResult_New_XlRegisterer;
