

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptRiskRequest_Get_Configuration
    (const OPER_* xl_request, const OPER_* format)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptRiskRequest_Get_Configuration");        
        argName = "request (input #1)";
        const Handle_<StorableDupireScriptRiskRequest_> request = Excel::ToHandle<StorableDupireScriptRiskRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableDupireScriptRiskSettings_> settings;
		Handle_<StorableCalibrationRiskRequest_> quotes;
		Matrix_<Cell_> bindings;
		bool has_direct;
        DupireScriptRiskRequest_Get_Configuration(request, &settings, &quotes, &bindings, &has_direct);
        Excel::Retval_ retval;
        retval.Load(_env, settings);
        retval.Load(_env, quotes);
        retval.Load(bindings);
        retval.Load(has_direct);
        return retval.ToXloper(Excel::ToString(format, true));
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

struct XlRegister_DupireScriptRiskRequest_Get_Configuration_
{
    XlRegister_DupireScriptRiskRequest_Get_Configuration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Automatic request");
        Excel::Register("Base", "xl_DupireScriptRiskRequest_Get_Configuration", "DUPIRESCRIPTRISKREQUEST.GET.CONFIGURATION", "Copy owning automatic request controls", "QQQ", "request,format", argHelp, false);
    }
};
static XlRegister_DupireScriptRiskRequest_Get_Configuration_ The_DupireScriptRiskRequest_Get_Configuration_XlRegisterer;
