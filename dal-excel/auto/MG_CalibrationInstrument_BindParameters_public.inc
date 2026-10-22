

extern "C" __declspec(dllexport) OPER_* xl_CalibrationInstrument_BindParameters
    (const OPER_* xl_instrument, const OPER_* xl_bindings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationInstrument_BindParameters");        
        argName = "instrument (input #1)";
        const Handle_<CalibrationInstrument_> instrument = Excel::ToHandle<CalibrationInstrument_>(_env, xl_instrument);
        argName = "bindings (input #2)";
        const Matrix_<Cell_> bindings = Excel::ToCellMatrix(xl_bindings, false, true);
        const Excel::ScriptSettingsInput_ normalized_bindings(xl_bindings); xl_bindings = normalized_bindings.Get();
        argName = 0;
		Handle_<CalibrationInstrument_> result;
        CalibrationInstrument_BindParameters(instrument, bindings, &result);
        Excel::Retval_ retval;
        retval.Load(_env, result);
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

struct XlRegister_CalibrationInstrument_BindParameters_
{
    XlRegister_CalibrationInstrument_BindParameters_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed instrument");
        argHelp.push_back("Typed bindings");
        Excel::Register("Base", "xl_CalibrationInstrument_BindParameters", "CALIBRATIONINSTRUMENT.BINDPARAMETERS", "Owning typed script calibration interface", "QQQ", "instrument,bindings", argHelp, false);
    }
};
static XlRegister_CalibrationInstrument_BindParameters_ The_CalibrationInstrument_BindParameters_XlRegisterer;
