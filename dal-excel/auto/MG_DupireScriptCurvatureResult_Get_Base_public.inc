

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptCurvatureResult_Get_Base
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptCurvatureResult_Get_Base");        
        argName = "result (input #1)";
        const Handle_<StorableDupireScriptCurvatureResult_> result = Excel::ToHandle<StorableDupireScriptCurvatureResult_>(_env, xl_result);
        argName = 0;
		Handle_<StorableDupireScriptRiskResult_> base;
        DupireScriptCurvatureResult_Get_Base(result, &base);
        Excel::Retval_ retval;
        retval.Load(_env, base);
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

struct XlRegister_DupireScriptCurvatureResult_Get_Base_
{
    XlRegister_DupireScriptCurvatureResult_Get_Base_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning result");
        Excel::Register("Base", "xl_DupireScriptCurvatureResult_Get_Base", "DUPIRESCRIPTCURVATURERESULT.GET.BASE", "Copy the selected first-order valuation retained by Dupire curvature", "QQ", "result", argHelp, false);
    }
};
static XlRegister_DupireScriptCurvatureResult_Get_Base_ The_DupireScriptCurvatureResult_Get_Base_XlRegisterer;
