

extern "C" __declspec(dllexport) OPER_* xl_Calibrate_XccyInstruments
    (const OPER_* xl_spec, const OPER_* xl_options)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Calibrate_XccyInstruments");        
        argName = "spec (input #1)";
        const Handle_<StorableXccyInstrumentCalibrationSpec_> spec = Excel::ToHandle<StorableXccyInstrumentCalibrationSpec_>(_env, xl_spec);
        argName = "options (input #2)";
        const Handle_<StorableCurveInstrumentCalibrationOptions_> options = Excel::ToHandle<StorableCurveInstrumentCalibrationOptions_>(_env, xl_options, true);
        argName = 0;
		Handle_<StorableCalibrationOutcome_> outcome;
        Calibrate_XccyInstruments(spec, options, &outcome);
        Excel::Retval_ retval;
        retval.Load(_env, outcome);
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

struct XlRegister_Calibrate_XccyInstruments_
{
    XlRegister_Calibrate_XccyInstruments_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Frozen calibration inputs");
        argHelp.push_back("Deterministic evaluation and raw derivative settings");
        Excel::Register("Base", "xl_Calibrate_XccyInstruments", "CALIBRATE.XCCYINSTRUMENTS", "Fit staged XCCY basis from shared observations", "QQQ", "spec,[options]", argHelp, false);
    }
};
static XlRegister_Calibrate_XccyInstruments_ The_Calibrate_XccyInstruments_XlRegisterer;
