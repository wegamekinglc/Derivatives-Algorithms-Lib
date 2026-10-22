

extern "C" __declspec(dllexport) OPER_* xl_CalibrationInstrument_LoadJson
    (const OPER_* xl_json)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationInstrument_LoadJson");        
        argName = "json (input #1)";
        const String_ json = Excel::ToString(xl_json);
        argName = "json"; Excel::ValidateNativeTextInput(xl_json, "/json", "InvalidCalibrationInput");
        argName = 0;
		Handle_<CalibrationInstrument_> instrument;
        CalibrationInstrument_LoadJson(json, &instrument);
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

struct XlRegister_CalibrationInstrument_LoadJson_
{
    XlRegister_CalibrationInstrument_LoadJson_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed json");
        Excel::Register("Base", "xl_CalibrationInstrument_LoadJson", "CALIBRATIONINSTRUMENT.LOADJSON", "Owning typed script calibration interface", "QQ", "json", argHelp, false);
    }
};
static XlRegister_CalibrationInstrument_LoadJson_ The_CalibrationInstrument_LoadJson_XlRegisterer;
