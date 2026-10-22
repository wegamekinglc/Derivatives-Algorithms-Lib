

extern "C" __declspec(dllexport) OPER_* xl_CalibrationInstrument_ValueByMonteCarlo
    (const OPER_* xl_instrument, const OPER_* xl_model, const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationInstrument_ValueByMonteCarlo");        
        argName = "instrument (input #1)";
        const Handle_<CalibrationInstrument_> instrument = Excel::ToHandle<CalibrationInstrument_>(_env, xl_instrument);
        argName = "model (input #2)";
        const Handle_<ModelData_> model = Excel::ToHandle<ModelData_>(_env, xl_model);
        argName = "request (input #3)";
        const Handle_<StorableCalibrationMonteCarloRequest_> request = Excel::ToHandle<StorableCalibrationMonteCarloRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableCalibrationOutcome_> outcome;
        CalibrationInstrument_ValueByMonteCarlo(instrument, model, request, &outcome);
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

struct XlRegister_CalibrationInstrument_ValueByMonteCarlo_
{
    XlRegister_CalibrationInstrument_ValueByMonteCarlo_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed instrument");
        argHelp.push_back("Typed model");
        argHelp.push_back("Typed request");
        Excel::Register("Base", "xl_CalibrationInstrument_ValueByMonteCarlo", "CALIBRATIONINSTRUMENT.VALUEBYMONTECARLO", "Owning typed script calibration interface", "QQQQ", "instrument,model,request", argHelp, false);
    }
};
static XlRegister_CalibrationInstrument_ValueByMonteCarlo_ The_CalibrationInstrument_ValueByMonteCarlo_XlRegisterer;
