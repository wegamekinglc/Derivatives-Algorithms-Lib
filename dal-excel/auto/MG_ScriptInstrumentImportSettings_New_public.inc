

extern "C" __declspec(dllexport) OPER_* xl_ScriptInstrumentImportSettings_New
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptInstrumentImportSettings_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings);
        const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get(); argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "ScriptInstrumentImportSettings_New", "settings");
        argName = 0;
		Handle_<StorableScriptInstrumentImportSettings_> value;
        ScriptInstrumentImportSettings_New(settings, &value);
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

struct XlRegister_ScriptInstrumentImportSettings_New_
{
    XlRegister_ScriptInstrumentImportSettings_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Exact field/value rows; nested values are typed handles");
        Excel::Register("Base", "xl_ScriptInstrumentImportSettings_New", "SCRIPTINSTRUMENTIMPORTSETTINGS.NEW", "Create immutable typed calibration settings", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_ScriptInstrumentImportSettings_New_ The_ScriptInstrumentImportSettings_New_XlRegisterer;
