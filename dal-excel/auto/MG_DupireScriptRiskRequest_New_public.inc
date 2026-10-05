

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptRiskRequest_New
    (const OPER_* xl_name, const OPER_* xl_settings, const OPER_* xl_quotes, const OPER_* xl_bindings, const OPER_* xl_direct)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptRiskRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "DupireScriptRiskRequest_New; name");
        argName = "settings (input #2)";
        const Handle_<StorableDupireScriptRiskSettings_> settings = Excel::ToHandle<StorableDupireScriptRiskSettings_>(_env, xl_settings);
        xl_quotes = Excel::ScriptScalarInput(xl_quotes); argName = "bindings"; Excel::ValidateRiskRequestBindings(xl_bindings); const Excel::ScriptSettingsInput_ normalized(xl_bindings); xl_bindings = normalized.Get(); xl_direct = Excel::ScriptScalarInput(xl_direct);
        argName = "quotes (input #3)";
        const Handle_<StorableCalibrationRiskRequest_> quotes = Excel::ToHandle<StorableCalibrationRiskRequest_>(_env, xl_quotes, true);
        argName = "bindings (input #4)";
        const Matrix_<Cell_> bindings = Excel::ToCellMatrix(xl_bindings, true, true);
        argName = "direct (input #5)";
        const Handle_<StorableCalibrationDirectQuoteAdjoints_> direct = Excel::ToHandle<StorableCalibrationDirectQuoteAdjoints_>(_env, xl_direct, true);
        argName = 0;
		Handle_<StorableDupireScriptRiskRequest_> request;
        DupireScriptRiskRequest_New(name, settings, quotes, bindings, direct, &request);
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

struct XlRegister_DupireScriptRiskRequest_New_
{
    XlRegister_DupireScriptRiskRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Required execution settings");
        argHelp.push_back("Blank selects all quotes; budget applies to combined retained results");
        argHelp.push_back("Two data columns: exact constant ordinal, quote ID; no header");
        argHelp.push_back("Optional external direct seeds; mutually exclusive with bindings");
        Excel::Register("Base", "xl_DupireScriptRiskRequest_New", "DUPIRESCRIPTRISKREQUEST.NEW", "Create an automatic quote request with explicit direct dependencies", "QQQQQQ", "name,settings,[quotes],[bindings],[direct]", argHelp, false);
    }
};
static XlRegister_DupireScriptRiskRequest_New_ The_DupireScriptRiskRequest_New_XlRegisterer;
