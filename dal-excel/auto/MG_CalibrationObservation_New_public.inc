

extern "C" __declspec(dllexport) OPER_* xl_CalibrationObservation_New
    (const OPER_* xl_id, const OPER_* xl_instrument, const OPER_* xl_measureId, const OPER_* xl_quote, const OPER_* xl_errorScale)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationObservation_New");        
        argName = "id (input #1)";
        const String_ id = Excel::ToString(xl_id);
        argName = "id"; Excel::ValidateNativeTextInput(xl_id, "/id", "InvalidCalibrationInput");
        argName = "instrument (input #2)";
        const Handle_<CalibrationInstrument_> instrument = Excel::ToHandle<CalibrationInstrument_>(_env, xl_instrument);
        argName = "measureId (input #3)";
        const String_ measureId = Excel::ToString(xl_measureId);
        argName = "measureId"; Excel::ValidateNativeTextInput(xl_measureId, "/measureId", "InvalidCalibrationInput");
        argName = "quote (input #4)";
        const Handle_<StorableCalibrationMarketQuote_> quote = Excel::ToHandle<StorableCalibrationMarketQuote_>(_env, xl_quote);
        argName = "errorScale (input #5)";
        const Cell_ errorScale = Excel::ToCell(xl_errorScale);
        const Excel::ScriptSettingsInput_ normalized_errorScale(xl_errorScale); xl_errorScale = normalized_errorScale.Get();
        argName = 0;
		Handle_<StorableCalibrationObservation_> observation;
        CalibrationObservation_New(id, instrument, measureId, quote, errorScale, &observation);
        Excel::Retval_ retval;
        retval.Load(_env, observation);
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

struct XlRegister_CalibrationObservation_New_
{
    XlRegister_CalibrationObservation_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed id");
        argHelp.push_back("Typed instrument");
        argHelp.push_back("Typed measureId");
        argHelp.push_back("Typed quote");
        argHelp.push_back("Typed errorScale");
        Excel::Register("Base", "xl_CalibrationObservation_New", "CALIBRATIONOBSERVATION.NEW", "Owning typed script calibration interface", "QQQQQQ", "id,instrument,measureId,quote,errorScale", argHelp, false);
    }
};
static XlRegister_CalibrationObservation_New_ The_CalibrationObservation_New_XlRegisterer;
