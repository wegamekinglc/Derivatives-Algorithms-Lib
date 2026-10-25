

extern "C" __declspec(dllexport) OPER_* xl_Calibrate_CurveInstruments
    (const OPER_* xl_spec, const OPER_* xl_options)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Calibrate_CurveInstruments");        
        argName = "spec (input #1)";
        const Handle_<StorableCurveInstrumentCalibrationSpec_> spec = Excel::ToHandle<StorableCurveInstrumentCalibrationSpec_>(_env, xl_spec);
        argName = "options (input #2)";
        const Handle_<StorableCurveInstrumentCalibrationOptions_> options = Excel::ToHandle<StorableCurveInstrumentCalibrationOptions_>(_env, xl_options, true);
        argName = 0;
		Handle_<StorableCalibrationOutcome_> outcome;
        Calibrate_CurveInstruments(spec, options, &outcome);
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

struct XlRegister_Calibrate_CurveInstruments_
{
    XlRegister_Calibrate_CurveInstruments_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed spec");
        argHelp.push_back("Typed options");
        Excel::Register("Base", "xl_Calibrate_CurveInstruments", "CALIBRATE.CURVEINSTRUMENTS", "Owning typed script calibration interface", "QQQ", "spec,[options]", argHelp, false);
    }
};
static XlRegister_Calibrate_CurveInstruments_ The_Calibrate_CurveInstruments_XlRegisterer;
