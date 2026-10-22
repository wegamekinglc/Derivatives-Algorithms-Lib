

extern "C" __declspec(dllexport) OPER_* xl_FraScriptInstrument_New
    (const OPER_* xl_native, const OPER_* xl_payment, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("FraScriptInstrument_New");        
        argName = "native (input #1)";
        const Handle_<StorableYCInstrument_> native = Excel::ToHandle<StorableYCInstrument_>(_env, xl_native);
        argName = "payment (input #2)";
        const Cell_ payment = Excel::ToCell(xl_payment);
        argName = "settings (input #3)";
        const Handle_<StorableScriptInstrumentImportSettings_> settings = Excel::ToHandle<StorableScriptInstrumentImportSettings_>(_env, xl_settings);
        argName = 0;
		Handle_<CalibrationInstrument_> instrument;
        FraScriptInstrument_New(native, payment, settings, &instrument);
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

struct XlRegister_FraScriptInstrument_New_
{
    XlRegister_FraScriptInstrument_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Matching native product");
        argHelp.push_back("Explicit FRA payment date or integral serial");
        argHelp.push_back("Explicit units, route, notional and annuity floor");
        Excel::Register("Base", "xl_FraScriptInstrument_New", "FRASCRIPTINSTRUMENT.NEW", "Import resolved deterministic native curve cashflows once", "QQQQ", "native,payment,settings", argHelp, false);
    }
};
static XlRegister_FraScriptInstrument_New_ The_FraScriptInstrument_New_XlRegisterer;
