

extern "C" __declspec(dllexport) OPER_* xl_XccyScriptInstrument_New
    (const OPER_* xl_native, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyScriptInstrument_New");        
        argName = "native (input #1)";
        const Handle_<StorableCrossCurrencySwap_> native = Excel::ToHandle<StorableCrossCurrencySwap_>(_env, xl_native);
        argName = "settings (input #2)";
        const Handle_<StorableXccyScriptImportSettings_> settings = Excel::ToHandle<StorableXccyScriptImportSettings_>(_env, xl_settings);
        argName = 0;
		Handle_<CalibrationInstrument_> value;
        XccyScriptInstrument_New(native, settings, &value);
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

struct XlRegister_XccyScriptInstrument_New_
{
    XlRegister_XccyScriptInstrument_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Native import input");
        argHelp.push_back("Immutable import settings");
        Excel::Register("Base", "xl_XccyScriptInstrument_New", "XCCYSCRIPTINSTRUMENT.NEW", "Import native XCCY terms once into shared script cashflows", "QQQ", "native,settings", argHelp, false);
    }
};
static XlRegister_XccyScriptInstrument_New_ The_XccyScriptInstrument_New_XlRegisterer;
