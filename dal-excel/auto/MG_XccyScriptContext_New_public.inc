

extern "C" __declspec(dllexport) OPER_* xl_XccyScriptContext_New
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyScriptContext_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings);
        const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get(); argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "XccyScriptContext_New", "settings");
        argName = 0;
		Handle_<StorableXccyScriptContext_> value;
        XccyScriptContext_New(settings, &value);
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

struct XlRegister_XccyScriptContext_New_
{
    XlRegister_XccyScriptContext_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Exact field/value rows with explicit currency pair, collateral, route and timestamp");
        Excel::Register("Base", "xl_XccyScriptContext_New", "XCCYSCRIPTCONTEXT.NEW", "Create an immutable NativeXccyTimestamp context", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_XccyScriptContext_New_ The_XccyScriptContext_New_XlRegisterer;
