

extern "C" __declspec(dllexport) OPER_* xl_GSRInstrumentCalibrationSettings_New
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRInstrumentCalibrationSettings_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings);
        const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get(); argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "GSRInstrumentCalibrationSettings_New", "settings");
        argName = 0;
		Handle_<StorableGSRInstrumentCalibrationSettings_> value;
        GSRInstrumentCalibrationSettings_New(settings, &value);
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

struct XlRegister_GSRInstrumentCalibrationSettings_New_
{
    XlRegister_GSRInstrumentCalibrationSettings_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Exact field/value rows; valuation required; nested values are typed session handles");
        Excel::Register("Base", "xl_GSRInstrumentCalibrationSettings_New", "GSRINSTRUMENTCALIBRATIONSETTINGS.NEW", "Create owning GSR script calibration settings with an explicit valuation", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_GSRInstrumentCalibrationSettings_New_ The_GSRInstrumentCalibrationSettings_New_XlRegisterer;
