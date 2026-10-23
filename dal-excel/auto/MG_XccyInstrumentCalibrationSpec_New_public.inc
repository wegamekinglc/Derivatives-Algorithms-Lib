

extern "C" __declspec(dllexport) OPER_* xl_XccyInstrumentCalibrationSpec_New
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyInstrumentCalibrationSpec_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings);
        const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get(); argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "XccyInstrumentCalibrationSpec_New", "settings");
        argName = 0;
		Handle_<StorableXccyInstrumentCalibrationSpec_> value;
        XccyInstrumentCalibrationSpec_New(settings, &value);
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

struct XlRegister_XccyInstrumentCalibrationSpec_New_
{
    XlRegister_XccyInstrumentCalibrationSpec_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Exact field/value rows; nested context, blocks, observations, dates and solver are handles");
        Excel::Register("Base", "xl_XccyInstrumentCalibrationSpec_New", "XCCYINSTRUMENTCALIBRATIONSPEC.NEW", "Create immutable staged XCCY instrument calibration inputs", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_XccyInstrumentCalibrationSpec_New_ The_XccyInstrumentCalibrationSpec_New_XlRegisterer;
