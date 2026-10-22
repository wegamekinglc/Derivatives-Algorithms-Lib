

extern "C" __declspec(dllexport) OPER_* xl_CalibrationInstrument_Get_Json
    (const OPER_* xl_instrument)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationInstrument_Get_Json");        
        argName = "instrument (input #1)";
        const Handle_<CalibrationInstrument_> instrument = Excel::ToHandle<CalibrationInstrument_>(_env, xl_instrument);
        argName = 0;
		Vector_<String_> json;
        CalibrationInstrument_Get_Json(instrument, &json);
        Excel::Retval_ retval;
        retval.Load(json);
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

struct XlRegister_CalibrationInstrument_Get_Json_
{
    XlRegister_CalibrationInstrument_Get_Json_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed instrument");
        Excel::Register("Base", "xl_CalibrationInstrument_Get_Json", "CALIBRATIONINSTRUMENT.GET.JSON", "Owning typed script calibration interface", "QQ", "instrument", argHelp, false);
    }
};
static XlRegister_CalibrationInstrument_Get_Json_ The_CalibrationInstrument_Get_Json_XlRegisterer;
