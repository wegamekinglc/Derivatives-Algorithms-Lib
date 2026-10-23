

extern "C" __declspec(dllexport) OPER_* xl_XccyScriptImportSettings_New
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyScriptImportSettings_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings);
        const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get(); argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "XccyScriptImportSettings_New", "settings");
        argName = 0;
		Handle_<StorableXccyScriptImportSettings_> value;
        XccyScriptImportSettings_New(settings, &value);
        Excel::Retval_ retval;
        retval.Load(_env, value);
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

struct XlRegister_XccyScriptImportSettings_New_
{
    XlRegister_XccyScriptImportSettings_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Exact instrument_id, discount_route and optional orientation/annuity fields");
        Excel::Register("Base", "xl_XccyScriptImportSettings_New", "XCCYSCRIPTIMPORTSETTINGS.NEW", "Create immutable XCCY import settings", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_XccyScriptImportSettings_New_ The_XccyScriptImportSettings_New_XlRegisterer;
