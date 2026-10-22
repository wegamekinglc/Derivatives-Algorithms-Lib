

extern "C" __declspec(dllexport) OPER_* xl_DepositScriptInstrument_New
    (const OPER_* xl_deposit, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DepositScriptInstrument_New");        
        argName = "deposit (input #1)";
        const Handle_<StorableYCInstrument_> deposit = Excel::ToHandle<StorableYCInstrument_>(_env, xl_deposit);
        argName = "settings (input #2)";
        const Handle_<StorableScriptInstrumentImportSettings_> settings = Excel::ToHandle<StorableScriptInstrumentImportSettings_>(_env, xl_settings);
        argName = 0;
		Handle_<CalibrationInstrument_> instrument;
        DepositScriptInstrument_New(deposit, settings, &instrument);
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

struct XlRegister_DepositScriptInstrument_New_
{
    XlRegister_DepositScriptInstrument_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed deposit");
        argHelp.push_back("Typed settings");
        Excel::Register("Base", "xl_DepositScriptInstrument_New", "DEPOSITSCRIPTINSTRUMENT.NEW", "Owning typed script calibration interface", "QQQ", "deposit,settings", argHelp, false);
    }
};
static XlRegister_DepositScriptInstrument_New_ The_DepositScriptInstrument_New_XlRegisterer;
