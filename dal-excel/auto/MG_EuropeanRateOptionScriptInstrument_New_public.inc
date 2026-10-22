

extern "C" __declspec(dllexport) OPER_* xl_EuropeanRateOptionScriptInstrument_New
    (const OPER_* xl_option, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EuropeanRateOptionScriptInstrument_New");        
        argName = "option (input #1)";
        const Handle_<StorableEuropeanRateOption_> option = Excel::ToHandle<StorableEuropeanRateOption_>(_env, xl_option);
        argName = "settings (input #2)";
        const Handle_<StorableScriptInstrumentImportSettings_> settings = Excel::ToHandle<StorableScriptInstrumentImportSettings_>(_env, xl_settings);
        argName = 0;
		Handle_<CalibrationInstrument_> instrument;
        EuropeanRateOptionScriptInstrument_New(option, settings, &instrument);
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

struct XlRegister_EuropeanRateOptionScriptInstrument_New_
{
    XlRegister_EuropeanRateOptionScriptInstrument_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed option");
        argHelp.push_back("Typed settings");
        Excel::Register("Base", "xl_EuropeanRateOptionScriptInstrument_New", "EUROPEANRATEOPTIONSCRIPTINSTRUMENT.NEW", "Owning typed script calibration interface", "QQQ", "option,settings", argHelp, false);
    }
};
static XlRegister_EuropeanRateOptionScriptInstrument_New_ The_EuropeanRateOptionScriptInstrument_New_XlRegisterer;
