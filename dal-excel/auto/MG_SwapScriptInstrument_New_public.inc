

extern "C" __declspec(dllexport) OPER_* xl_SwapScriptInstrument_New
    (const OPER_* xl_native, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("SwapScriptInstrument_New");        
        argName = "native (input #1)";
        const Handle_<StorableYCInstrument_> native = Excel::ToHandle<StorableYCInstrument_>(_env, xl_native);
        argName = "settings (input #2)";
        const Handle_<StorableScriptInstrumentImportSettings_> settings = Excel::ToHandle<StorableScriptInstrumentImportSettings_>(_env, xl_settings);
        argName = 0;
		Handle_<CalibrationInstrument_> instrument;
        SwapScriptInstrument_New(native, settings, &instrument);
        Excel::Retval_ retval;
        retval.Load(_env, instrument);
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

struct XlRegister_SwapScriptInstrument_New_
{
    XlRegister_SwapScriptInstrument_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Matching native product");
        argHelp.push_back("Explicit units, route, notional and annuity floor");
        Excel::Register("Base", "xl_SwapScriptInstrument_New", "SWAPSCRIPTINSTRUMENT.NEW", "Import resolved deterministic native curve cashflows once", "QQQ", "native,settings", argHelp, false);
    }
};
static XlRegister_SwapScriptInstrument_New_ The_SwapScriptInstrument_New_XlRegisterer;
