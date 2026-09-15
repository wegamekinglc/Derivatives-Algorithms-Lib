

extern "C" __declspec(dllexport) OPER_* xl_ScriptProductSettings_New
    (const OPER_* xl_name, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptProductSettings_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        const Excel::ScriptSettingsInput_ settingsInput(xl_settings); xl_settings = settingsInput.Get(); argName = "settings (input #2)"; Excel::ValidateScriptSettingsRange(xl_settings, "ScriptProductSettings_New", "settings");
        argName = "settings (input #2)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true, true);
        argName = 0;
		Handle_<StorableScriptProductSettings_> productSettings;
        ScriptProductSettings_New(name, settings, &productSettings);
        Excel::Retval_ retval;
        retval.Load(_env, productSettings);
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

struct XlRegister_ScriptProductSettings_New_
{
    XlRegister_ScriptProductSettings_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Two columns key/value: default_index. Blank selects no default index.");
        Excel::Register("Base", "xl_ScriptProductSettings_New", "SCRIPTPRODUCTSETTINGS.NEW", "Create immutable script contract settings; no market reads", "QQQ", "name,[settings]", argHelp, false);
    }
};
static XlRegister_ScriptProductSettings_New_ The_ScriptProductSettings_New_XlRegisterer;
