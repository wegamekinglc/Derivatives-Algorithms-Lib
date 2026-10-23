

extern "C" __declspec(dllexport) OPER_* xl_CalibrationInstrument_Get_XccyCapabilities
    (const OPER_* xl_instrument, const OPER_* xl_measureId, const OPER_* xl_context)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationInstrument_Get_XccyCapabilities");        
        argName = "instrument (input #1)";
        const Handle_<CalibrationInstrument_> instrument = Excel::ToHandle<CalibrationInstrument_>(_env, xl_instrument);
        argName = "measureId (input #2)";
        const String_ measureId = Excel::ToString(xl_measureId);
        argName = "context (input #3)";
        const Handle_<StorableXccyScriptContext_> context = Excel::ToHandle<StorableXccyScriptContext_>(_env, xl_context);
        argName = 0;
		Handle_<CalibrationReport_> report;
        CalibrationInstrument_Get_XccyCapabilities(instrument, measureId, context, &report);
        Excel::Retval_ retval;
        retval.Load(_env, report);
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

struct XlRegister_CalibrationInstrument_Get_XccyCapabilities_
{
    XlRegister_CalibrationInstrument_Get_XccyCapabilities_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Shared immutable instrument");
        argHelp.push_back("Price or BasisSpread");
        argHelp.push_back("Explicit NativeXccyTimestamp context");
        Excel::Register("Base", "xl_CalibrationInstrument_Get_XccyCapabilities", "CALIBRATIONINSTRUMENT.GET.XCCYCAPABILITIES", "Query admitted and rejected XCCY execution capabilities", "QQQQ", "instrument,measureId,context", argHelp, false);
    }
};
static XlRegister_CalibrationInstrument_Get_XccyCapabilities_ The_CalibrationInstrument_Get_XccyCapabilities_XlRegisterer;
