

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptCurvaturePlan_Get_Payload
    (const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptCurvaturePlan_Get_Payload");        
        argName = "plan (input #1)";
        const Handle_<StorableDupireScriptCurvaturePlan_> plan = Excel::ToHandle<StorableDupireScriptCurvaturePlan_>(_env, xl_plan);
        argName = 0;
		double payload;
        DupireScriptCurvaturePlan_Get_Payload(plan, &payload);
        Excel::Retval_ retval;
        retval.Load(payload);
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

struct XlRegister_DupireScriptCurvaturePlan_Get_Payload_
{
    XlRegister_DupireScriptCurvaturePlan_Get_Payload_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Admitted curvature plan");
        Excel::Register("Base", "xl_DupireScriptCurvaturePlan_Get_Payload", "DUPIRESCRIPTCURVATUREPLAN.GET.PAYLOAD", "Copy admitted native Dupire curvature numeric bytes", "QQ", "plan", argHelp, false);
    }
};
static XlRegister_DupireScriptCurvaturePlan_Get_Payload_ The_DupireScriptCurvaturePlan_Get_Payload_XlRegisterer;
