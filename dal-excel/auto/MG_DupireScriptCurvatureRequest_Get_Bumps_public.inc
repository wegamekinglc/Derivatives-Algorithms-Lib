

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptCurvatureRequest_Get_Bumps
    (const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptCurvatureRequest_Get_Bumps");        
        argName = "request (input #1)";
        const Handle_<StorableDupireScriptCurvatureRequest_> request = Excel::ToHandle<StorableDupireScriptCurvatureRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableBumpOverAADRequest_> bumps;
        DupireScriptCurvatureRequest_Get_Bumps(request, &bumps);
        Excel::Retval_ retval;
        retval.Load(_env, bumps);
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

struct XlRegister_DupireScriptCurvatureRequest_Get_Bumps_
{
    XlRegister_DupireScriptCurvatureRequest_Get_Bumps_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Passive curvature request");
        Excel::Register("Base", "xl_DupireScriptCurvatureRequest_Get_Bumps", "DUPIRESCRIPTCURVATUREREQUEST.GET.BUMPS", "Copy directions, steps and native budgets retained by Dupire curvature", "QQ", "request", argHelp, false);
    }
};
static XlRegister_DupireScriptCurvatureRequest_Get_Bumps_ The_DupireScriptCurvatureRequest_Get_Bumps_XlRegisterer;
