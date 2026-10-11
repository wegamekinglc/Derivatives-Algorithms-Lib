

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptCurvatureRequest_Get_Risk
    (const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptCurvatureRequest_Get_Risk");        
        argName = "request (input #1)";
        const Handle_<StorableDupireScriptCurvatureRequest_> request = Excel::ToHandle<StorableDupireScriptCurvatureRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableDupireScriptRiskRequest_> risk;
        DupireScriptCurvatureRequest_Get_Risk(request, &risk);
        Excel::Retval_ retval;
        retval.Load(_env, risk);
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

struct XlRegister_DupireScriptCurvatureRequest_Get_Risk_
{
    XlRegister_DupireScriptCurvatureRequest_Get_Risk_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Passive curvature request");
        Excel::Register("Base", "xl_DupireScriptCurvatureRequest_Get_Risk", "DUPIRESCRIPTCURVATUREREQUEST.GET.RISK", "Copy the first-order request retained by Dupire curvature", "QQ", "request", argHelp, false);
    }
};
static XlRegister_DupireScriptCurvatureRequest_Get_Risk_ The_DupireScriptCurvatureRequest_Get_Risk_XlRegisterer;
