

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloReference_New
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloReference_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings);
        const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get(); argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "XccyMonteCarloReference_New", "settings");
        argName = 0;
		Handle_<StorableXccyMonteCarloReference_> value;
        XccyMonteCarloReference_New(settings, &value);
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

struct XlRegister_XccyMonteCarloReference_New_
{
    XlRegister_XccyMonteCarloReference_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed settings");
        Excel::Register("Base", "xl_XccyMonteCarloReference_New", "XCCYMONTECARLOREFERENCE.NEW", "XccyMonteCarloReference New", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloReference_New_ The_XccyMonteCarloReference_New_XlRegisterer;
