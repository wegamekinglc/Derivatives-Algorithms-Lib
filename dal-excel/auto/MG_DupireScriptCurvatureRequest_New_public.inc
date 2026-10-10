

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptCurvatureRequest_New
    (const OPER_* xl_name, const OPER_* xl_risk, const OPER_* xl_bumps)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptCurvatureRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "DupireScriptCurvatureRequest_New; name");
        argName = "risk (input #2)";
        const Handle_<StorableDupireScriptRiskRequest_> risk = Excel::ToHandle<StorableDupireScriptRiskRequest_>(_env, xl_risk);
        argName = "bumps (input #3)";
        const Handle_<StorableBumpOverAADRequest_> bumps = Excel::ToHandle<StorableBumpOverAADRequest_>(_env, xl_bumps);
        argName = 0;
		Handle_<StorableDupireScriptCurvatureRequest_> request;
        DupireScriptCurvatureRequest_New(name, risk, bumps, &request);
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

struct XlRegister_DupireScriptCurvatureRequest_New_
{
    XlRegister_DupireScriptCurvatureRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Existing first-order settings and optional direct scalar quote bindings");
        argHelp.push_back("Full raw decimal-vol quote directions and positive steps; worker recording caps are unsupported");
        Excel::Register("Base", "xl_DupireScriptCurvatureRequest_New", "DUPIRESCRIPTCURVATUREREQUEST.NEW", "Create a passive recalibrated Dupire quote curvature request", "QQQQ", "name,risk,bumps", argHelp, false);
    }
};
static XlRegister_DupireScriptCurvatureRequest_New_ The_DupireScriptCurvatureRequest_New_XlRegisterer;
