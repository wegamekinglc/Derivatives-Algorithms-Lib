

extern "C" __declspec(dllexport) OPER_* xl_CalibrationInstrument_ValueWithQuoteRisk
    (const OPER_* xl_instrument, const OPER_* xl_snapshot, const OPER_* xl_context, const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationInstrument_ValueWithQuoteRisk");        
        argName = "instrument (input #1)";
        const Handle_<CalibrationInstrument_> instrument = Excel::ToHandle<CalibrationInstrument_>(_env, xl_instrument);
        argName = "snapshot (input #2)";
        const Handle_<StorableScriptCurveQuoteRiskSnapshot_> snapshot = Excel::ToHandle<StorableScriptCurveQuoteRiskSnapshot_>(_env, xl_snapshot);
        argName = "context (input #3)";
        const Handle_<StorableCurveInstrumentCalibrationSpec_> context = Excel::ToHandle<StorableCurveInstrumentCalibrationSpec_>(_env, xl_context);
        argName = "request (input #4)";
        const Handle_<StorableScriptCurveQuoteRiskRequest_> request = Excel::ToHandle<StorableScriptCurveQuoteRiskRequest_>(_env, xl_request, true);
        argName = 0;
		Handle_<StorableScriptCurveQuoteRiskResult_> value;
        CalibrationInstrument_ValueWithQuoteRisk(instrument, snapshot, context, request, &value);
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

struct XlRegister_CalibrationInstrument_ValueWithQuoteRisk_
{
    XlRegister_CalibrationInstrument_ValueWithQuoteRisk_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Fixed bound contract");
        argHelp.push_back("Owning fitted snapshot");
        argHelp.push_back("Explicit current context");
        argHelp.push_back("Optional quote selection and Price measure");
        Excel::Register("Base", "xl_CalibrationInstrument_ValueWithQuoteRisk", "CALIBRATIONINSTRUMENT.VALUEWITHQUOTERISK", "Value fixed script payments with implicit quote risk", "QQQQQ", "instrument,snapshot,context,[request]", argHelp, false);
    }
};
static XlRegister_CalibrationInstrument_ValueWithQuoteRisk_ The_CalibrationInstrument_ValueWithQuoteRisk_XlRegisterer;
